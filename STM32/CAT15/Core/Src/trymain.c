
/* ================================================================
   STM32F401RB + BME280 + ESP8266 (WEMOS) AT COMMAND + HTTP JSON

   STAGE 1:
       - Enter AT commands through PuTTY.
       - Commands are sent to ESP8266.
       - ESP8266 responses are displayed on PuTTY.
       - Enter 123 and press ENTER to continue.

   STAGE 2:
       - Initialize BME280.
       - Read and display temperature, pressure and humidity.

   STAGE 3:
       - Receive HTTP requests from ESP8266 using +IPD.
       - Read fresh BME280 values.
       - Generate JSON.
       - Send HTTP response.

   UART1 : ESP8266, 115200 baud
   UART2 : PuTTY,   115200 baud
   I2C1  : BME280

   IMPORTANT:
       USART1 RX uses interrupt-driven circular buffering.
       ESP_RX_BUFFER_SIZE must remain a power of two.
   ================================================================ */

#include "main.h"
#include "bme280.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* ======================== EXTERNAL HANDLES ===================== */

extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/* ======================== BUFFER SIZES ========================= */

#define ESP_RX_BUFFER_SIZE       4096U
#define AT_COMMAND_SIZE           256U
#define AT_RESPONSE_SIZE         1024U
#define HTTP_REQUEST_SIZE        1024U
#define HTTP_RESPONSE_SIZE        512U
#define JSON_BUFFER_SIZE          256U

#define ESP_UART_TIMEOUT          1000U
#define PC_UART_TIMEOUT           1000U

#define BME280_I2C_ADDRESS        (0x76U << 1)

/* ======================== GLOBAL BUFFERS ======================= */

static uint8_t esp_rx_it_byte;

static uint8_t esp_rx_buffer[ESP_RX_BUFFER_SIZE];

static volatile uint16_t esp_rx_head = 0U;
static volatile uint16_t esp_rx_tail = 0U;

static volatile uint32_t esp_rx_overflow = 0U;

static char command[AT_COMMAND_SIZE];
static char at_response[AT_RESPONSE_SIZE];

static char http_request[HTTP_REQUEST_SIZE];
static char json[JSON_BUFFER_SIZE];
static char http_response_buffer[HTTP_RESPONSE_SIZE];

static uint16_t http_request_length = 0U;

static volatile uint8_t http_pending = 0U;

static uint8_t http_link_id = 0U;

/* ======================== BME280 OBJECTS ======================= */

static struct bme280_dev bme280;
static struct bme280_data sensor_data;

static uint16_t bme280_address = BME280_I2C_ADDRESS;

/* ======================== HTTP PARSER ========================== */

static const char ipd_prefix[] = "+IPD,";

static uint8_t ipd_match_index = 0U;

static uint8_t ipd_state = 0U;

static uint32_t ipd_payload_length = 0U;
static uint32_t ipd_payload_received = 0U;

static uint32_t ipd_number = 0U;

static uint8_t ipd_link = 0U;

static uint8_t ipd_number_digits = 0U;

static uint8_t ipd_payload_overflow = 0U;

/* ======================== FUNCTION PROTOTYPES ================= */

void SystemClock_Config(void);
void Error_Handler(void);

static void pc_print(const char *text);
static void esp_send(const char *text);

static uint8_t esp_rx_pop(uint8_t *data);
static void poll_esp(void);

static void inspect_esp_byte(uint8_t c);
static void reset_ipd_parser(void);

static int wait_token(
    const char *success,
    const char *failure,
    uint32_t timeout
);

static int wait_wifi_join(uint32_t timeout);

static int at_command(
    const char *cmd,
    uint32_t timeout
);

static void wemos_at_stage(void);

static int sensor_init(void);
static int read_sensor(void);

static void display_sensor_data(void);

static void http_response(uint8_t link);

static int send_http_data(
    const char *data,
    uint16_t length
);

static int wait_for_prompt(uint32_t timeout);

static void user_delay_us(uint32_t period, void *intf_ptr);

static int8_t user_i2c_read(
    uint8_t reg_addr,
    uint8_t *reg_data,
    uint32_t length,
    void *intf_ptr
);

static int8_t user_i2c_write(
    uint8_t reg_addr,
    const uint8_t *reg_data,
    uint32_t length,
    void *intf_ptr
);

/* ================================================================
   BASIC UART PRINT FUNCTIONS
   ================================================================ */

static void pc_print(const char *text)
{
    if (text == NULL)
        return;

    HAL_UART_Transmit(
        &huart2,
        (uint8_t *)text,
        (uint16_t)strlen(text),
        PC_UART_TIMEOUT
    );
}

static void esp_send(const char *text)
{
    if (text == NULL)
        return;

    HAL_UART_Transmit(
        &huart1,
        (uint8_t *)text,
        (uint16_t)strlen(text),
        ESP_UART_TIMEOUT
    );
}

/* ================================================================
   ESP8266 INTERRUPT-DRIVEN RECEIVE

   USART1 receives one byte at a time.

   The interrupt stores bytes in the circular buffer.
   Main code consumes them independently.

   Effective capacity = 4095 bytes.
   ================================================================ */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uint16_t next_head =
            (uint16_t)((esp_rx_head + 1U) &
            (ESP_RX_BUFFER_SIZE - 1U));

        if (next_head != esp_rx_tail)
        {
            esp_rx_buffer[esp_rx_head] = esp_rx_it_byte;
            esp_rx_head = next_head;
        }
        else
        {
            esp_rx_overflow++;
        }

        HAL_UART_Receive_IT(
            &huart1,
            &esp_rx_it_byte,
            1U
        );
    }
}

/* ================================================================
   POP ONE BYTE FROM ESP8266 RECEIVE BUFFER
   ================================================================ */

static uint8_t esp_rx_pop(uint8_t *data)
{
    if (esp_rx_tail == esp_rx_head)
        return 0U;

    *data = esp_rx_buffer[esp_rx_tail];

    esp_rx_tail =
        (uint16_t)((esp_rx_tail + 1U) &
        (ESP_RX_BUFFER_SIZE - 1U));

    return 1U;
}

/* ================================================================
   HTTP +IPD PARSER

   Supports:
       +IPD,<link>,<length>:<payload>

   Captures exactly the announced payload length.

   If the request is larger than HTTP_REQUEST_SIZE - 1,
   excess bytes are discarded safely.

   A new request is not accepted while one is pending.
   ================================================================ */

static void reset_ipd_parser(void)
{
    ipd_state = 0U;
    ipd_match_index = 0U;

    ipd_payload_length = 0U;
    ipd_payload_received = 0U;

    ipd_number = 0U;
    ipd_number_digits = 0U;

    ipd_link = 0U;
    ipd_payload_overflow = 0U;
}

static void inspect_esp_byte(uint8_t c)
{
    switch (ipd_state)
    {
        /* Search for +IPD, */
        case 0:

            if (c == (uint8_t)ipd_prefix[ipd_match_index])
            {
                ipd_match_index++;

                if (ipd_match_index == sizeof(ipd_prefix) - 1U)
                {
                    ipd_state = 1U;
                    ipd_match_index = 0U;

                    ipd_number = 0U;
                    ipd_number_digits = 0U;
                }
            }
            else
            {
                ipd_match_index =
                    (c == '+') ? 1U : 0U;
            }

            break;

        /* Read connection ID */
        case 1:

            if (c >= '0' && c <= '9')
            {
                ipd_number =
                    ipd_number * 10U + (uint32_t)(c - '0');

                ipd_number_digits++;

                if (ipd_number > 255U)
                    reset_ipd_parser();
            }
            else if (c == ',' && ipd_number_digits > 0U)
            {
                ipd_link = (uint8_t)ipd_number;

                ipd_number = 0U;
                ipd_number_digits = 0U;

                ipd_state = 2U;
            }
            else
            {
                reset_ipd_parser();
            }

            break;

        /* Read payload length */
        case 2:

            if (c >= '0' && c <= '9')
            {
                if (ipd_number > 100000U)
                {
                    reset_ipd_parser();
                    break;
                }

                ipd_number =
                    ipd_number * 10U + (uint32_t)(c - '0');

                ipd_number_digits++;
            }
            else if (c == ':' && ipd_number_digits > 0U)
            {
                ipd_payload_length = ipd_number;
                ipd_payload_received = 0U;

                ipd_payload_overflow = 0U;

                http_request_length = 0U;
                http_link_id = ipd_link;

                if (ipd_payload_length >= HTTP_REQUEST_SIZE)
                    ipd_payload_overflow = 1U;

                ipd_state = 3U;

                if (ipd_payload_length == 0U)
                    reset_ipd_parser();
            }
            else
            {
                reset_ipd_parser();
            }

            break;

        /* Capture exactly the announced payload */
        case 3:

            if (!ipd_payload_overflow &&
                http_request_length < HTTP_REQUEST_SIZE - 1U)
            {
                http_request[http_request_length++] = (char)c;
            }
            else
            {
                ipd_payload_overflow = 1U;
            }

            ipd_payload_received++;

            if (ipd_payload_received >= ipd_payload_length)
            {
                if (!ipd_payload_overflow)
                {
                    http_request[http_request_length] = '\0';

                    http_pending = 1U;
                }

                reset_ipd_parser();
            }

            break;

        default:

            reset_ipd_parser();
            break;
    }
}

/* ================================================================
   POLL ESP8266 BUFFER

   Displays received bytes on PuTTY and feeds the HTTP parser.
   ================================================================ */

static void poll_esp(void)
{
    uint8_t c;

    while (esp_rx_pop(&c))
    {
        HAL_UART_Transmit(
            &huart2,
            &c,
            1U,
            PC_UART_TIMEOUT
        );

        inspect_esp_byte(c);
    }
}

/* ================================================================
   WAIT FOR A RESPONSE TOKEN

   This function consumes bytes from the receive ring buffer.
   It also feeds the HTTP parser.

   The response buffer is maintained as a rolling window.
   ================================================================ */

static int wait_token(
    const char *success,
    const char *failure,
    uint32_t timeout
)
{
    uint32_t start = HAL_GetTick();

    uint16_t length = 0U;

    uint8_t c;

    memset(at_response, 0, sizeof(at_response));

    while ((HAL_GetTick() - start) < timeout)
    {
        while (esp_rx_pop(&c))
        {
            HAL_UART_Transmit(
                &huart2,
                &c,
                1U,
                PC_UART_TIMEOUT
            );

            inspect_esp_byte(c);

            if (length >= AT_RESPONSE_SIZE - 1U)
            {
                memmove(
                    at_response,
                    at_response + 1,
                    AT_RESPONSE_SIZE - 2U
                );

                length = AT_RESPONSE_SIZE - 2U;
            }

            at_response[length++] = (char)c;
            at_response[length] = '\0';

            if (failure != NULL &&
                strstr(at_response, failure) != NULL)
            {
                return -1;
            }

            if (success != NULL &&
                strstr(at_response, success) != NULL)
            {
                return 1;
            }
        }

        HAL_Delay(1U);
    }

    return 0;
}

/* ================================================================
   WIFI JOIN RESPONSE

   A successful join requires both:
       WIFI GOT IP
       OK

   The order of these messages does not matter.
   ================================================================ */

static int wait_wifi_join(uint32_t timeout)
{
    uint32_t start = HAL_GetTick();

    uint8_t got_ip = 0U;
    uint8_t got_ok = 0U;

    uint16_t length = 0U;

    uint8_t c;

    memset(at_response, 0, sizeof(at_response));

    while ((HAL_GetTick() - start) < timeout)
    {
        while (esp_rx_pop(&c))
        {
            HAL_UART_Transmit(
                &huart2,
                &c,
                1U,
                PC_UART_TIMEOUT
            );

            inspect_esp_byte(c);

            if (length >= AT_RESPONSE_SIZE - 1U)
            {
                memmove(
                    at_response,
                    at_response + 1,
                    AT_RESPONSE_SIZE - 2U
                );

                length = AT_RESPONSE_SIZE - 2U;
            }

            at_response[length++] = (char)c;
            at_response[length] = '\0';

            if (strstr(at_response, "WIFI GOT IP") != NULL)
                got_ip = 1U;

            if (strstr(at_response, "OK") != NULL)
                got_ok = 1U;

            if (strstr(at_response, "FAIL") != NULL ||
                strstr(at_response, "ERROR") != NULL)
            {
                return -1;
            }

            if (got_ip && got_ok)
                return 1;
        }

        HAL_Delay(1U);
    }

    return 0;
}

/* ================================================================
   SEND AT COMMAND TO ESP8266
   ================================================================ */

static int at_command(
    const char *cmd,
    uint32_t timeout
)
{
    char tx[AT_COMMAND_SIZE + 4U];

    int result;

    int ret;

    if (cmd == NULL)
        return -1;

    ret = snprintf(
        tx,
        sizeof(tx),
        "%s\r\n",
        cmd
    );

    if (ret < 0 || (size_t)ret >= sizeof(tx))
    {
        pc_print("\r\nCommand too long.\r\n");
        return -1;
    }

    pc_print("\r\n\r\n[STM32 -> ESP8266] ");
    pc_print(cmd);
    pc_print("\r\n[ESP8266 RESPONSE]\r\n");

    HAL_UART_Transmit(
        &huart1,
        (uint8_t *)tx,
        (uint16_t)ret,
        ESP_UART_TIMEOUT
    );

    if (strncmp(cmd, "AT+CWJAP", 8U) == 0)
    {
        result = wait_wifi_join(timeout);
    }
    else
    {
        result = wait_token("OK", "ERROR", timeout);
    }

    if (result == 1)
        pc_print("\r\n[COMMAND SUCCESS]\r\n");
    else if (result == -1)
        pc_print("\r\n[COMMAND FAILED]\r\n");
    else
        pc_print("\r\n[COMMAND TIMEOUT]\r\n");

    return result;
}

/* ================================================================
   STAGE 1 - MANUAL WEMOS AT COMMAND CONFIGURATION

   Type commands in PuTTY.

   Example sequence:

       AT
       AT+GMR
       AT+CWMODE=1
       AT+CWJAP="SSID","PASSWORD"
       AT+CIFSR
       AT+CIPMUX=1
       AT+CIPSERVER=1,80

   Then enter:

       123

   Press ENTER to proceed.
   ================================================================ */

static void wemos_at_stage(void)
{
    uint16_t index = 0U;

    uint8_t c;

    pc_print("\r\n====================================");
    pc_print("\r\n       STAGE 1: WEMOS CONFIGURATION");
    pc_print("\r\n====================================");

    pc_print("\r\nEnter ESP8266 AT commands below.");
    pc_print("\r\nEnter 123 and press ENTER to continue.");
    pc_print("\r\n------------------------------------\r\n");

    memset(command, 0, sizeof(command));

    while (1)
    {
        poll_esp();

        if (HAL_UART_Receive(
                &huart2,
                &c,
                1U,
                10U
            ) == HAL_OK)
        {
            if (c == '\r' || c == '\n')
            {
                if (index == 0U)
                    continue;

                command[index] = '\0';

                pc_print("\r\n");

                if (strcmp(command, "123") == 0)
                {
                    pc_print("\r\nLeaving Stage 1...\r\n");
                    return;
                }

                /*
                   Allow extra time for router connection.
                   ESP8266 may take several seconds to join Wi-Fi.
                */

                if (strncmp(command, "AT+CWJAP", 8U) == 0)
                {
                    at_command(command, 90000U);
                }
                else
                {
                    at_command(command, 15000U);
                }

                index = 0U;
                memset(command, 0, sizeof(command));

                pc_print("\r\nEnter next AT command:\r\n");
            }
            else if (c == 8U || c == 127U)
            {
                if (index > 0U)
                {
                    index--;

                    command[index] = '\0';

                    pc_print("\b \b");
                }
            }
            else if (c >= 32U && c <= 126U)
            {
                if (index < AT_COMMAND_SIZE - 1U)
                {
                    command[index++] = (char)c;

                    HAL_UART_Transmit(
                        &huart2,
                        &c,
                        1U,
                        PC_UART_TIMEOUT
                    );
                }
                else
                {
                    pc_print("\r\nCommand buffer full.\r\n");
                    index = 0U;
                    memset(command, 0, sizeof(command));
                }
            }
        }
    }
}

/* ================================================================
   DWT MICROSECOND DELAY

   Cortex-M4 DWT cycle counter.
   Used by Bosch BME280 driver delay callback.
   ================================================================ */

static void user_delay_us(uint32_t period, void *intf_ptr)
{
    uint32_t start;
    uint32_t cycles;

    (void)intf_ptr;

    cycles = (HAL_RCC_GetHCLKFreq() / 1000000U) * period;

    start = DWT->CYCCNT;

    while ((DWT->CYCCNT - start) < cycles)
    {
        /* Wait */
    }
}

/* ================================================================
   BME280 I2C READ CALLBACK
   ================================================================ */

static int8_t user_i2c_read(
    uint8_t reg_addr,
    uint8_t *reg_data,
    uint32_t length,
    void *intf_ptr
)
{
    uint16_t address = *((uint16_t *)intf_ptr);

    if (HAL_I2C_Mem_Read(
            &hi2c1,
            address,
            reg_addr,
            I2C_MEMADD_SIZE_8BIT,
            reg_data,
            (uint16_t)length,
            1000U
        ) == HAL_OK)
    {
        return BME280_OK;
    }

    return BME280_E_COMM_FAIL;
}

/* ================================================================
   BME280 I2C WRITE CALLBACK
   ================================================================ */

static int8_t user_i2c_write(
    uint8_t reg_addr,
    const uint8_t *reg_data,
    uint32_t length,
    void *intf_ptr
)
{
    uint16_t address = *((uint16_t *)intf_ptr);

    if (HAL_I2C_Mem_Write(
            &hi2c1,
            address,
            reg_addr,
            I2C_MEMADD_SIZE_8BIT,
            (uint8_t *)reg_data,
            (uint16_t)length,
            1000U
        ) == HAL_OK)
    {
        return BME280_OK;
    }

    return BME280_E_COMM_FAIL;
}

/* ================================================================
   STAGE 2 - BME280 INITIALIZATION
   ================================================================ */

static int sensor_init(void)
{
    int8_t result;

    memset(&bme280, 0, sizeof(bme280));

    bme280.intf = BME280_I2C_INTF;

    bme280.intf_ptr = &bme280_address;

    bme280.read = user_i2c_read;
    bme280.write = user_i2c_write;
    bme280.delay_us = user_delay_us;

    pc_print("\r\nInitializing BME280...\r\n");

    result = bme280_init(&bme280);

    if (result != BME280_OK)
    {
        pc_print("\r\nBME280 initialization failed.\r\n");
        return -1;
    }

    struct bme280_settings settings;

    memset(&settings, 0, sizeof(settings));

    settings.filter = BME280_FILTER_COEFF_2;

    settings.osr_h = BME280_OVERSAMPLING_1X;
    settings.osr_p = BME280_OVERSAMPLING_1X;
    settings.osr_t = BME280_OVERSAMPLING_2X;

    settings.standby_time = BME280_STANDBY_TIME_1000_MS;

    result = bme280_set_sensor_settings(
        BME280_SEL_ALL,
        &settings,
        &bme280
    );

    if (result != BME280_OK)
    {
        pc_print("\r\nBME280 settings failed.\r\n");
        return -1;
    }

    result = bme280_set_sensor_mode(
        BME280_NORMAL_MODE,
        &bme280
    );

    if (result != BME280_OK)
    {
        pc_print("\r\nBME280 mode setting failed.\r\n");
        return -1;
    }

    HAL_Delay(100U);

    pc_print("\r\nBME280 initialized successfully.\r\n");

    return 0;
}

/* ================================================================
   READ BME280 SENSOR DATA
   ================================================================ */

static int read_sensor(void)
{
    int8_t result;

    result = bme280_get_sensor_data(
        BME280_ALL,
        &sensor_data,
        &bme280
    );

    if (result != BME280_OK)
    {
        pc_print("\r\nBME280 read failed.\r\n");
        return -1;
    }

    return 0;
}

/* ================================================================
   DISPLAY SENSOR DATA ON PUTTY
   ================================================================ */

static void display_sensor_data(void)
{
    char output[256];

    int ret;

    ret = snprintf(
        output,
        sizeof(output),
        "\r\nTemperature : %.2f C"
        "\r\nPressure    : %.2f hPa"
        "\r\nHumidity    : %.2f %%RH\r\n",
        sensor_data.temperature / 100.0,
        sensor_data.pressure / 100.0,
        sensor_data.humidity / 1024.0
    );

    if (ret > 0 && (size_t)ret < sizeof(output))
        pc_print(output);
    else
        pc_print("\r\nSensor display formatting error.\r\n");
}

/* ================================================================
   WAIT FOR ESP8266 '>' PROMPT
   ================================================================ */

static int wait_for_prompt(uint32_t timeout)
{
    uint32_t start = HAL_GetTick();

    uint8_t c;

    while ((HAL_GetTick() - start) < timeout)
    {
        while (esp_rx_pop(&c))
        {
            HAL_UART_Transmit(
                &huart2,
                &c,
                1U,
                PC_UART_TIMEOUT
            );

            inspect_esp_byte(c);

            if (c == '>')
                return 1;

            if (c == 'E')
            {
                /* The normal response checker handles ERROR. */
            }
        }

        HAL_Delay(1U);
    }

    return 0;
}

/* ================================================================
   SEND HTTP RESPONSE DATA

   Sends the exact response length, not strlen of a truncated
   or unrelated buffer.

   Requires ESP8266 CIPMUX=1 and an active server connection.
   ================================================================ */

static int send_http_data(
    const char *data,
    uint16_t length
)
{
    char send_command[64];

    int ret;

    ret = snprintf(
        send_command,
        sizeof(send_command),
        "AT+CIPSEND=%u,%u\r\n",
        http_link_id,
        length
    );

    if (ret < 0 || (size_t)ret >= sizeof(send_command))
        return -1;

    HAL_UART_Transmit(
        &huart1,
        (uint8_t *)send_command,
        (uint16_t)ret,
        ESP_UART_TIMEOUT
    );

    if (!wait_for_prompt(5000U))
    {
        pc_print("\r\nCIPSEND prompt timeout.\r\n");
        return -1;
    }

    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)data,
            length,
            5000U
        ) != HAL_OK)
    {
        pc_print("\r\nHTTP transmission failed.\r\n");
        return -1;
    }

    if (wait_token("SEND OK", "ERROR", 10000U) != 1)
    {
        pc_print("\r\nSEND OK not received.\r\n");
        return -1;
    }

    return 0;
}

/* ================================================================
   STAGE 3 - HTTP REQUEST HANDLING

   1. Request arrives through +IPD.
   2. Parser captures the announced payload length.
   3. Fresh sensor values are read.
   4. JSON is generated.
   5. HTTP response is transmitted.
   ================================================================ */

static void http_response(uint8_t link)
{
    int ret;

    uint16_t response_length;

    char json_local[JSON_BUFFER_SIZE];

    char http_local[HTTP_RESPONSE_SIZE];

    (void)link;

    pc_print("\r\n\r\n[HTTP REQUEST RECEIVED]\r\n");

    if (http_request_length == 0U)
    {
        pc_print("Empty HTTP request.\r\n");
        return;
    }

    /*
       Display the request safely.

       The request is NUL-terminated by the parser.
    */

    pc_print(http_request);
    pc_print("\r\n");

    if (read_sensor() != 0)
    {
        pc_print("Could not read BME280.\r\n");
        return;
    }

    ret = snprintf(
        json_local,
        sizeof(json_local),
        "{"
        "\"temperature\":%.2f,"
        "\"pressure\":%.2f,"
        "\"humidity\":%.2f"
        "}",
        sensor_data.temperature / 100.0,
        sensor_data.pressure / 100.0,
        sensor_data.humidity / 1024.0
    );

    if (ret < 0 || (size_t)ret >= sizeof(json_local))
    {
        pc_print("JSON buffer too small.\r\n");
        return;
    }

    ret = snprintf(
        http_local,
        sizeof(http_local),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "Content-Length: %u\r\n"
        "\r\n"
        "%s",
        (unsigned int)strlen(json_local),
        json_local
    );

    if (ret < 0 || (size_t)ret >= sizeof(http_local))
    {
        pc_print("HTTP response buffer too small.\r\n");
        return;
    }

    response_length = (uint16_t)ret;

    pc_print("\r\n[JSON RESPONSE]\r\n");
    pc_print(json_local);

    pc_print("\r\n\r\n[HTTP RESPONSE]\r\n");
    pc_print(http_local);

    if (send_http_data(http_local, response_length) == 0)
    {
        pc_print("\r\nHTTP response sent successfully.\r\n");
    }
    else
    {
        pc_print("\r\nHTTP response transmission failed.\r\n");
    }

    /*
       Close the TCP connection after responding.
       The server remains active for future connections.
    */

    {
        char close_command[32];

        ret = snprintf(
            close_command,
            sizeof(close_command),
            "AT+CIPCLOSE=%u\r\n",
            http_link_id
        );

        if (ret > 0 && (size_t)ret < sizeof(close_command))
        {
            HAL_UART_Transmit(
                &huart1,
                (uint8_t *)close_command,
                (uint16_t)ret,
                ESP_UART_TIMEOUT
            );

            wait_token("OK", "ERROR", 3000U);
        }
    }
}

/* ================================================================
   MAIN APPLICATION
   ================================================================ */

int main(void)
{
    uint32_t previous_overflow = 0U;

    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    /*
       Enable DWT cycle counter for microsecond delays.
    */

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CYCCNT = 0U;

    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /*
       Start continuous interrupt-driven reception from ESP8266.
    */

    if (HAL_UART_Receive_IT(
            &huart1,
            &esp_rx_it_byte,
            1U
        ) != HAL_OK)
    {
        Error_Handler();
    }

    pc_print("\r\n\r\n");
    pc_print("====================================");
    pc_print("\r\n STM32F401RB - IOT APPLICATION");
    pc_print("\r\n====================================");

    /* ============================================================
       STAGE 1 - WEMOS AT COMMAND CONFIGURATION
       ============================================================ */

    wemos_at_stage();

    /* ============================================================
       STAGE 2 - BME280 SENSOR
       ============================================================ */

    pc_print("\r\n====================================");
    pc_print("\r\n       STAGE 2: BME280 SENSOR");
    pc_print("\r\n====================================");

    if (sensor_init() != 0)
    {
        pc_print("\r\nCheck BME280 wiring and I2C address.\r\n");
        Error_Handler();
    }

    if (read_sensor() == 0)
    {
        display_sensor_data();
    }

    /* ============================================================
       STAGE 3 - HTTP SERVER
       ============================================================ */

    pc_print("\r\n====================================");
    pc_print("\r\n       STAGE 3: HTTP SERVER");
    pc_print("\r\n====================================");

    pc_print("\r\nSTM32 is now listening for HTTP requests.");
    pc_print("\r\nOpen the Wemos IP address in a browser.");
    pc_print("\r\nExample: http://192.168.x.x/\r\n");

    while (1)
    {
        /*
           Drain received ESP8266 bytes.
        */

        poll_esp();

        /*
           Process a complete HTTP request.
        */

        if (http_pending)
        {
            /*
               Temporarily clear pending before processing.
               A new request is not captured until parsing resumes.
            */

            http_pending = 0U;

            http_response(http_link_id);

            http_request_length = 0U;

            memset(
                http_request,
                0,
                sizeof(http_request)
            );
        }

        /*
           Report UART receive overflow.

           An overflow means the STM32 could not consume incoming
           data quickly enough. It must not be silently ignored.
        */

        if (esp_rx_overflow != previous_overflow)
        {
            previous_overflow = esp_rx_overflow;

            pc_print(
                "\r\n[WARNING] ESP8266 RX BUFFER OVERFLOW!"
                "\r\nSome received bytes may have been lost.\r\n"
            );
        }

        HAL_Delay(1U);
    }
}

/* ================================================================
   ERROR HANDLER
   ================================================================ */

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
        /* Fatal error */
    }
}

#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}

#endif
