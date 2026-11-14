/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32F401RE + BME280 + Wemos ESP8266 HTTP Web Server (debug build)
  *
  * USART1: STM32 <-> Wemos ESP8266 (AT commands)
  * USART2: STM32 <-> PuTTY (ST-LINK virtual COM port)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "bme280.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
void Error_Handler(void);
int8_t user_i2c_read(uint8_t reg, uint8_t *data, uint32_t len, void *ptr);
int8_t user_i2c_write(uint8_t reg, const uint8_t *data, uint32_t len, void *ptr);
void user_delay_us(uint32_t delay, void *ptr);

/* USER CODE BEGIN PD */
#define HTTP_REQUEST_PREVIEW  32
/* USER CODE END PD */

/* USER CODE BEGIN PV */
/* Each ESP8266 +IPD frame looks like: +IPD,<id>,<length>:<payload> */
static uint8_t ipd_state = 0;
static uint8_t ipd_prefix_pos = 0;
static int ipd_link = 0;
static uint32_t ipd_length = 0;
static uint32_t ipd_received = 0;
static char request_start[HTTP_REQUEST_PREVIEW];
static uint8_t request_start_len = 0;
/* Bosch BME280: same driver callbacks and settings as sensor-only test. */
static struct bme280_dev dev;
static struct bme280_data comp_data;
static struct bme280_settings settings;
static uint8_t dev_addr = BME280_I2C_ADDR_PRIM;
static uint8_t sensor_ready = 0;
static char sensor_log[160];
/* USER CODE END PV */

/* USER CODE BEGIN 0 */
void Debug_Print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg,
                      (uint16_t)strlen(msg), HAL_MAX_DELAY);
}

/* These are the real Bosch BME280 sensor callbacks using HAL I2C1. */
int8_t user_i2c_read(uint8_t reg, uint8_t *data, uint32_t len, void *ptr)
{
    uint8_t addr = *(uint8_t *)ptr;
    return HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(addr << 1), reg,
                            I2C_MEMADD_SIZE_8BIT, data, (uint16_t)len, 1000)
           == HAL_OK ? BME280_OK : BME280_E_COMM_FAIL;
}

int8_t user_i2c_write(uint8_t reg, const uint8_t *data, uint32_t len, void *ptr)
{
    uint8_t addr = *(uint8_t *)ptr;
    return HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(addr << 1), reg,
                             I2C_MEMADD_SIZE_8BIT, (uint8_t *)data,
                             (uint16_t)len, 1000)
           == HAL_OK ? BME280_OK : BME280_E_COMM_FAIL;
}

void user_delay_us(uint32_t delay, void *ptr)
{
    (void)ptr;
    HAL_Delay((delay + 999U) / 1000U);
}

/* Sensor failure is reported, but does not prevent WiFi server startup. */
static void BME280_Init(void)
{
    int8_t result;
    memset(&dev, 0, sizeof(dev));
    dev.intf_ptr = &dev_addr;
    dev.intf = BME280_I2C_INTF;
    dev.read = user_i2c_read;
    dev.write = user_i2c_write;
    dev.delay_us = user_delay_us;

    Debug_Print("\r\n[BME280] Trying address 0x76...\r\n");
    result = bme280_init(&dev);
    if (result != BME280_OK)
    {
        dev_addr = BME280_I2C_ADDR_SEC;
        Debug_Print("[BME280] Trying address 0x77...\r\n");
        result = bme280_init(&dev);
    }
    if (result == BME280_OK)
    {
        memset(&settings, 0, sizeof(settings));
        settings.osr_t = BME280_OVERSAMPLING_1X;
        settings.osr_p = BME280_OVERSAMPLING_1X;
        settings.osr_h = BME280_OVERSAMPLING_1X;
        settings.filter = BME280_FILTER_COEFF_OFF;
        result = bme280_set_sensor_settings(
            BME280_SEL_OSR_TEMP | BME280_SEL_OSR_PRESS |
            BME280_SEL_OSR_HUM | BME280_SEL_FILTER, &settings, &dev);
        if (result == BME280_OK)
            result = bme280_set_sensor_mode(BME280_POWERMODE_SLEEP, &dev);
    }
    if (result != BME280_OK)
    {
        snprintf(sensor_log, sizeof(sensor_log),
                 "[BME280] Error=%d, HAL I2C=0x%08lX; WiFi continues.\r\n",
                 result, (unsigned long)HAL_I2C_GetError(&hi2c1));
        Debug_Print(sensor_log);
        return;
    }
    sensor_ready = 1;
    snprintf(sensor_log, sizeof(sensor_log),
             "[BME280] Ready: address 0x%02X, chip ID 0x%02X\r\n",
             (unsigned)dev_addr, (unsigned)dev.chip_id);
    Debug_Print(sensor_log);
}

/* Take a new measurement on each browser GET.
   Requires BME280_DOUBLE_ENABLE: degrees C, Pa, percent RH. */
static uint8_t BME280_Read(int32_t *t100, int32_t *p100, int32_t *h100)
{
    if (!sensor_ready) return 0;
    if (bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev) != BME280_OK)
        return 0;
    HAL_Delay(100);
    if (bme280_get_sensor_data(BME280_ALL, &comp_data, &dev) != BME280_OK)
        return 0;
    double t = comp_data.temperature;
    double p = comp_data.pressure / 100.0; /* Pa to hPa */
    double h = comp_data.humidity;
    *t100 = (int32_t)(t * 100.0 + (t >= 0.0 ? 0.5 : -0.5));
    *p100 = (int32_t)(p * 100.0 + 0.5);
    *h100 = (int32_t)(h * 100.0 + 0.5);
    return 1;
}

static uint32_t abs100(int32_t x)
{
    return (uint32_t)(x < 0 ? -(int64_t)x : x);
}

/* Keep the same working AT-command approach as the WiFi/IP experiment. */

void Send_AT_Command(const char *cmd, uint32_t wait_ms)
{
    uint8_t rx[512];
    uint16_t count = 0;
    uint32_t start;

    HAL_UART_Transmit(&huart1,
                      (uint8_t *)cmd,
                      strlen(cmd),
                      HAL_MAX_DELAY);

    start = HAL_GetTick();

    while (HAL_GetTick() - start < wait_ms)
    {
        uint8_t ch;

        if (HAL_UART_Receive(&huart1, &ch, 1, 10) == HAL_OK)
        {
            if (count < sizeof(rx) - 1)
                rx[count++] = ch;
        }
    }

    rx[count] = '\0';

    Debug_Print("\r\n[ESP RESPONSE]\r\n");
    Debug_Print((char *)rx);
    Debug_Print("\r\n[END RESPONSE]\r\n");
}


/* Wait for ESP8266 to answer with '>' or 'SEND OK'.
   Also forward the response to PuTTY for debugging. */

int ESP_Wait_For(const char *expected, uint32_t timeout_ms)
{
    char response[512] = {0};
    uint16_t count = 0;
    uint8_t ch;
    uint8_t result = 0;
    uint32_t start = HAL_GetTick();

    while (HAL_GetTick() - start < timeout_ms)
    {
        if (HAL_UART_Receive(&huart1, &ch, 1, 10) == HAL_OK)
        {
            if (count < sizeof(response) - 1)
            {
                response[count++] = (char)ch;
                response[count] = '\0';
            }

            if (strstr(response, expected) != NULL)
            {
                result = 1;
                break;
            }

            if (strstr(response, "ERROR") ||
                strstr(response, "SEND FAIL") ||
                strstr(response, "link is not valid"))
            {
                break;
            }
        }
    }

    Debug_Print("\r\n[ESP RESPONSE]\r\n");
    Debug_Print(response);
    Debug_Print("\r\n[END RESPONSE]\r\n");

    return result;
}



void Send_HTTP_Response(uint8_t link_id)
{
    static char html[900];
    static char http_response[1150];
    static char command[50];
    static char values[230];
    int32_t t, p, h;
    int html_length, response_length;
		Debug_Print("\r\n[HTTP] Response function entered\r\n");
		Debug_Print("[HTTP] Reading BME280...\r\n");
    if (BME280_Read(&t, &p, &h))
    {
        uint32_t ta = abs100(t), pa = abs100(p), ha = abs100(h);
        snprintf(values, sizeof(values),
                 "<p>Temperature: %s%lu.%02lu &deg;C</p>"
                 "<p>Pressure: %lu.%02lu hPa</p>"
                 "<p>Humidity: %lu.%02lu %%</p>",
                 t < 0 ? "-" : "", (unsigned long)(ta / 100U),
                 (unsigned long)(ta % 100U), (unsigned long)(pa / 100U),
                 (unsigned long)(pa % 100U), (unsigned long)(ha / 100U),
                 (unsigned long)(ha % 100U));
        snprintf(sensor_log, sizeof(sensor_log),
                 "[BME280] %s%lu.%02lu C, %lu.%02lu hPa, %lu.%02lu %%\r\n",
                 t < 0 ? "-" : "", (unsigned long)(ta / 100U),
                 (unsigned long)(ta % 100U), (unsigned long)(pa / 100U),
                 (unsigned long)(pa % 100U), (unsigned long)(ha / 100U),
                 (unsigned long)(ha % 100U));
        Debug_Print(sensor_log);
    }
    else
    {
        strcpy(values, "<p>Sensor unavailable or read failed.</p>"
                       "<p>Check BME280 I2C wiring and initialization.</p>");
        Debug_Print("[BME280] No valid reading for this request\r\n");
    }

    html_length = snprintf(html, sizeof(html),
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta http-equiv='refresh' content='5'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>STM32 BME280 Live</title>"
        "<style>body{font-family:Arial;text-align:center;background:#eef3f9;"
        "padding:35px}main{background:white;margin:auto;max-width:540px;"
        "padding:24px;border-radius:14px}p{font-size:21px}</style>"
        "</head><body><main><h1>BME280 Live Sensor Data</h1>"
        "<h3>STM32F401RE + ESP8266</h3>%s"
        "<small>Refresh every 5 seconds | Chip ID: 0x%02X</small>"
        "</main></body></html>",
        values, (unsigned)(sensor_ready ? dev.chip_id : 0));
    if (html_length <= 0 || html_length >= (int)sizeof(html))
    {
        Debug_Print("[HTTP] HTML buffer too small\r\n");
        return;
    }

    response_length = snprintf(http_response, sizeof(http_response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n\r\n%s", html_length, html);
    if (response_length <= 0 || response_length >= (int)sizeof(http_response))
    {
        Debug_Print("[HTTP] Response buffer too small\r\n");
        return;
    }
		
		/* Prepare CIPSEND command first */
		snprintf(command, sizeof(command),
						 "AT+CIPSEND=%u,%d\r\n",
						 (unsigned)link_id,
						 response_length);

		/* Print debug information */
		Debug_Print("[HTTP] HTML generated successfully\r\n");
		Debug_Print("[HTTP] Sending CIPSEND command...\r\n");
		Debug_Print(command);

		/* Send command to Wemos */
		HAL_UART_Transmit(&huart1,
											(uint8_t *)command,
											(uint16_t)strlen(command),
											HAL_MAX_DELAY);


    if (ESP_Wait_For(">", 5000))
    {
        HAL_UART_Transmit(&huart1, (uint8_t *)http_response,
                          (uint16_t)response_length, HAL_MAX_DELAY);
        if (ESP_Wait_For("SEND OK", 10000))
            Debug_Print("\r\n[HTTP] Sensor webpage sent\r\n");
        else
            Debug_Print("\r\n[HTTP] SEND OK not received\r\n");
    }
    else
        Debug_Print("\r\n[HTTP] Missing > prompt\r\n");

    /* The HTTP response has Connection: close. Ask ESP8266 to close this
       client socket; the server may also report <id>,CLOSED asynchronously. */
    snprintf(command, sizeof(command), "AT+CIPCLOSE=%u\r\n",
             (unsigned)link_id);
    HAL_UART_Transmit(&huart1, (uint8_t *)command,
                      (uint16_t)strlen(command), HAL_MAX_DELAY);
    if (!ESP_Wait_For("OK", 1500))
        Debug_Print("[HTTP] Socket close acknowledgement not received (may already be closed).\r\n");
}

/* Parse +IPD,<link id>,<payload length>: and consume the complete payload.
   The browser's GET method is at the start of the payload. */
void Process_ESP_Byte(uint8_t ch)
{
    const char prefix[] = "+IPD,";

    switch (ipd_state)
    {
    case 0: /* Search for +IPD, */
        if (ch == (uint8_t)prefix[ipd_prefix_pos])
        {
            ipd_prefix_pos++;
            if (ipd_prefix_pos == (sizeof(prefix) - 1U))
            {
                ipd_state = 1;
                ipd_prefix_pos = 0;
                ipd_link = 0;
                ipd_length = 0;
                ipd_received = 0;
                request_start_len = 0;
                request_start[0] = '\0';
            }
        }
        else
        {
            /* Keep partial prefix matching if another '+' is encountered. */
            ipd_prefix_pos = (ch == '+') ? 1U : 0U;
        }
        break;

    case 1: /* Connection ID */
        if (ch >= '0' && ch <= '9')
        {
            ipd_link = (ipd_link * 10) + (int)(ch - '0');
            if (ipd_link > 4)
            {
                ipd_state = 0;
                ipd_prefix_pos = 0;
            }
        }
        else if (ch == ',' && ipd_link >= 0 && ipd_link <= 4)
        {
            ipd_state = 2;
        }
        else
        {
            ipd_state = 0;
            ipd_prefix_pos = (ch == '+') ? 1U : 0U;
        }
        break;

    case 2: /* Payload length */
        if (ch >= '0' && ch <= '9')
        {
            if (ipd_length > 819U)
            {
                ipd_state = 0;
                ipd_prefix_pos = 0;
                break;
            }
            ipd_length = (ipd_length * 10U) + (uint32_t)(ch - '0');
        }
        else if (ch == ':' && ipd_length > 0U && ipd_length <= 8192U)
        {
            ipd_state = 3;
        }
        else
        {
            ipd_state = 0;
            ipd_prefix_pos = (ch == '+') ? 1U : 0U;
        }
        break;

    case 3: /* Consume exactly the payload length reported by ESP8266. */
        if (request_start_len < (HTTP_REQUEST_PREVIEW - 1U))
        {
            request_start[request_start_len++] = (char)ch;
            request_start[request_start_len] = '\0';
        }

        ipd_received++;
        if (ipd_received >= ipd_length)
        {
            uint8_t client = (uint8_t)ipd_link;
            uint8_t is_get = (strncmp(request_start, "GET ", 4) == 0);

            /* Reset parser before sending the response, as response handling
               temporarily reads the UART itself. */
            ipd_state = 0;
            ipd_prefix_pos = 0;
            ipd_link = 0;
            ipd_length = 0;
            ipd_received = 0;

            if (is_get)
            {
                Debug_Print("[HTTP] Browser GET request parsed successfully.\r\n");
                Send_HTTP_Response(client);
            }
            else
            {
                Debug_Print("[HTTP] Received a non-GET request; ignoring it.\r\n");
            }
        }
        break;

    default:
        ipd_state = 0;
        ipd_prefix_pos = 0;
        break;
    }
}
/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_I2C1_Init();

    /* USER CODE BEGIN 2 */
    uint8_t msg[] = "\r\nStarting ESP8266 WiFi Web Server...\r\n";
    HAL_UART_Transmit(&huart2, msg, sizeof(msg) - 1, HAL_MAX_DELAY);
    BME280_Init();
    HAL_Delay(2000);

    /* Existing commands that already worked on your setup. */
    Debug_Print("\r\nSTEP 1: Testing ESP8266\r\n");
    Send_AT_Command("AT\r\n", 2000);

    Debug_Print("\r\nSTEP 2: Setting Station mode\r\n");
    Send_AT_Command("AT+CWMODE=1\r\n", 2000);

    Debug_Print("\r\nSTEP 3: Connecting to WiFi (wait 20 seconds)\r\n");
    Send_AT_Command("AT+CWJAP=\"Dharsan\",\"dharsan778\"\r\n", 20000);

    Debug_Print("\r\nSTEP 4: Getting IP address\r\n");
    Send_AT_Command("AT+CIFSR\r\n", 3000);

    /* New commands for hosting the webpage. */
    Debug_Print("\r\n[ESP] Stopping previous server...\r\n");
		Send_AT_Command("AT+CIPSERVER=0\r\n", 2000);

		HAL_Delay(500);

		Debug_Print("\r\nSTEP 5: Enabling multiple connections\r\n");
		Send_AT_Command("AT+CIPMUX=1\r\n", 2000);

    Debug_Print("\r\nSTEP 5.5: Selecting the simple +IPD format\r\n");
    Send_AT_Command("AT+CIPDINFO=0\r\n", 2000);

    Debug_Print("\r\nSTEP 6: Starting HTTP server on port 80\r\n");
    Send_AT_Command("AT+CIPSERVER=1,80\r\n", 2000);

    Debug_Print("\r\nHTTP server setup commands sent.\r\n");
    Debug_Print("Open the ESP8266 IP using http://<ESP_IP>/ (not https).\r\n");
    Debug_Print("Waiting for +IPD browser requests...\r\n");
    /* USER CODE END 2 */

    /* USER CODE BEGIN WHILE */
    while (1)
    {
        uint8_t rx;
        if (HAL_UART_Receive(&huart1, &rx, 1, 10) == HAL_OK)
        {
            Process_ESP_Byte(rx);
        }
    }
    /* USER CODE END WHILE */
}

/* Identical I2C1 settings to the working BME280-only program. */
static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

/* Preserve your previously working HSE-bypass 84 MHz configuration. */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 84;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        Error_Handler();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
        Error_Handler();
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK)
        Error_Handler();
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK)
        Error_Handler();
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
