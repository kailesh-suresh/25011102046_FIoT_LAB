
/* USER CODE BEGIN Header */
/*
 * Lab 10: HTTP Web Server using STM32 + Wemos ESP8266
 *
 * USART1: STM32 <-> ESP8266 AT firmware
 * USART2: STM32 <-> PC/PuTTY
 */
/* USER CODE END Header */

#include "main.h"

/* Configure locally; do not commit real Wi-Fi credentials. */
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#include <stdio.h>
#include <string.h>

/* Private variables */
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* Private function prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
void Error_Handler(void);

/* HTTP request parsing buffer */
static char rx_stream[1024];
static uint16_t rx_length = 0;

/* =========================================================
   SEND AT COMMAND AND DISPLAY ESP8266 RESPONSE ON PUTTY
   ========================================================= */

void Send_AT_Command(const char *command, uint32_t wait_ms)
{
    uint8_t ch;
    char response[512];
    uint16_t len = 0;

    uint32_t start = HAL_GetTick();
    uint32_t last_rx = start;
    uint8_t received = 0;

    HAL_UART_Transmit(&huart1,
                      (uint8_t *)command,
                      strlen(command),
                      HAL_MAX_DELAY);

    while ((HAL_GetTick() - start) < wait_ms)
    {
        if (HAL_UART_Receive(&huart1, &ch, 1, 100) == HAL_OK)
        {
            received = 1;
            last_rx = HAL_GetTick();

            if (len < sizeof(response) - 1)
            {
                response[len++] = ch;
            }
        }
        else if (received &&
                 (HAL_GetTick() - last_rx) >= 1500)
        {
            break;
        }
    }

    if (len > 0)
    {
        HAL_UART_Transmit(&huart2,
                          (uint8_t *)response,
                          len,
                          HAL_MAX_DELAY);
    }

    HAL_Delay(300);
}

/* =========================================================
   WAIT FOR ESP8266 CIPSEND PROMPT
   ========================================================= */

int Wait_For_Prompt(void)
{
    uint8_t ch;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < 4000)
    {
        if (HAL_UART_Receive(&huart1, &ch, 1, 100) == HAL_OK)
        {
            HAL_UART_Transmit(&huart2,
                              &ch,
                              1,
                              HAL_MAX_DELAY);

            if (ch == '>')
            {
                return 1;
            }
        }
    }

    return 0;
}

/* =========================================================
   WAIT FOR SEND OK
   ========================================================= */

void Wait_For_Send_Result(void)
{
    uint8_t ch;
    char result[100];
    uint16_t len = 0;
    uint32_t start = HAL_GetTick();

    memset(result, 0, sizeof(result));

    while ((HAL_GetTick() - start) < 2000)
    {
        if (HAL_UART_Receive(&huart1, &ch, 1, 100) == HAL_OK)
        {
            HAL_UART_Transmit(&huart2,
                              &ch,
                              1,
                              HAL_MAX_DELAY);

            if (len < sizeof(result) - 1)
            {
                result[len++] = ch;
                result[len] = '\0';
            }

            if (strstr(result, "SEND OK") != NULL ||
                strstr(result, "SEND FAIL") != NULL ||
                strstr(result, "ERROR") != NULL)
            {
                break;
            }
        }
    }
}

/* =========================================================
   CONSTRUCT AND SEND HTTP RESPONSE
   ========================================================= */

void Send_HTTP_Response(uint8_t link_id)
{
    char html[] =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<title>STM32 Lab 10</title>"
        "</head>"
        "<body>"
        "<h1>Hello Naga,Kailesh & Gowtham TEAM!</h1>"
        "<p>ESP8266 HTTP Server is working. Congrats</p>"
        "</body>"
        "</html>";

    char http_response[512];
    char command[40];

    int html_length = strlen(html);

    int response_length = snprintf(
        http_response,
        sizeof(http_response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        html_length,
        html
    );

    if (response_length <= 0 ||
        response_length >= sizeof(http_response))
    {
        return;
    }

    /* Request permission to send the response */
    snprintf(command,
             sizeof(command),
             "AT+CIPSEND=%u,%d\r\n",
             (unsigned int)link_id,
             response_length);

    HAL_UART_Transmit(&huart1,
                      (uint8_t *)command,
                      strlen(command),
                      HAL_MAX_DELAY);

    /* Wait until ESP8266 displays '>' */
    if (Wait_For_Prompt())
    {
        /* Send HTTP headers followed by HTML */
        HAL_UART_Transmit(&huart1,
                          (uint8_t *)http_response,
                          response_length,
                          HAL_MAX_DELAY);

        Wait_For_Send_Result();
    }

    HAL_Delay(200);

    /* Close the browser connection */
    snprintf(command,
             sizeof(command),
             "AT+CIPCLOSE=%u\r\n",
             (unsigned int)link_id);

    HAL_UART_Transmit(&huart1,
                      (uint8_t *)command,
                      strlen(command),
                      HAL_MAX_DELAY);

    HAL_Delay(200);
}

/* =========================================================
   PROCESS INCOMING ESP8266 DATA
   Detect +IPD, extract connection ID, and identify GET
   ========================================================= */

void Process_ESP_Byte(uint8_t ch)
{
    char *ipd;
    char *colon;
    char *request;
    char *header_end;

    int link_id = -1;

    /* Add the received character to the stream buffer */
    if (rx_length >= sizeof(rx_stream) - 2)
    {
        rx_length = 0;
        memset(rx_stream, 0, sizeof(rx_stream));
    }

    rx_stream[rx_length++] = (char)ch;
    rx_stream[rx_length] = '\0';

    /* Find the start of an incoming network packet */
    ipd = strstr(rx_stream, "+IPD,");

    if (ipd == NULL)
    {
        /*
         * Preserve the last four bytes because "+IPD,"
         * might be split across UART receive operations.
         */
        if (rx_length > 5)
        {
            memmove(rx_stream,
                    rx_stream + rx_length - 4,
                    4);

            rx_length = 4;
            rx_stream[rx_length] = '\0';
        }

        return;
    }

    /* Find the end of the +IPD header */
    colon = strchr(ipd, ':');

    if (colon == NULL)
    {
        return;
    }

    /*
     * Multi-connection format:
     * +IPD,<link_id>,<data_length>:<received_data>
     */
    if (sscanf(ipd, "+IPD,%d,", &link_id) != 1)
    {
        rx_length = 0;
        memset(rx_stream, 0, sizeof(rx_stream));
        return;
    }

    if (link_id < 0 || link_id > 4)
    {
        rx_length = 0;
        memset(rx_stream, 0, sizeof(rx_stream));
        return;
    }

    request = colon + 1;

    /* Wait until the HTTP request headers are complete */
    header_end = strstr(request, "\r\n\r\n");

    if (header_end == NULL)
    {
        return;
    }

    /* Display the incoming browser request on PuTTY */
    {
        const char message[] = "\r\n--- HTTP REQUEST ---\r\n";

        HAL_UART_Transmit(&huart2,
                          (uint8_t *)message,
                          sizeof(message) - 1,
                          HAL_MAX_DELAY);

        HAL_UART_Transmit(&huart2,
                          (uint8_t *)request,
                          (uint16_t)(header_end - request + 4),
                          HAL_MAX_DELAY);

        const char end_message[] =
            "\r\n--- SENDING HTTP RESPONSE ---\r\n";

        HAL_UART_Transmit(&huart2,
                          (uint8_t *)end_message,
                          sizeof(end_message) - 1,
                          HAL_MAX_DELAY);
    }

    /* Respond to the browser's GET request */
    if (strncmp(request, "GET ", 4) == 0)
    {
        Send_HTTP_Response((uint8_t)link_id);
    }
    else
    {
        /* Respond to other HTTP methods with a basic page too */
        Send_HTTP_Response((uint8_t)link_id);
    }

    /* Reset the buffer for the next browser request */
    rx_length = 0;
    memset(rx_stream, 0, sizeof(rx_stream));
}

/* =========================================================
   MAIN
   ========================================================= */

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    /* Allow ESP8266 to boot */
    HAL_Delay(3000);

    /* 1. Test ESP8266 */
    Send_AT_Command("AT\r\n", 3000);

    /* 2. Set station mode */
    Send_AT_Command("AT+CWMODE=1\r\n", 3000);

    /* 3. Connect to Wi-Fi */
    Send_AT_Command(
        "AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PASSWORD "\"\r\n",
        25000
    );

    /* 4. Display ESP8266 IP address */
    Send_AT_Command("AT+CIFSR\r\n", 4000);

    /* 5. Enable multiple TCP connections */
    Send_AT_Command("AT+CIPMUX=1\r\n", 3000);

    /* 6. Start HTTP server on port 80 */
    Send_AT_Command("AT+CIPSERVER=1,80\r\n", 3000);

    {
        const char ready[] =
            "\r\nHTTP SERVER READY\r\n"
            "Open the ESP8266 IP address in your browser.\r\n";

        HAL_UART_Transmit(&huart2,
                          (uint8_t *)ready,
                          sizeof(ready) - 1,
                          HAL_MAX_DELAY);
    }

    /* 7. Continuously listen for browser requests */
    while (1)
    {
        uint8_t ch;

        if (HAL_UART_Receive(&huart1, &ch, 1, 10) == HAL_OK)
        {
            Process_ESP_Byte(ch);
        }
    }
}

/* =========================================================
   SYSTEM CLOCK CONFIGURATION
   ========================================================= */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSE;

    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;

    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct,
                            FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/* =========================================================
   USART1 INITIALIZATION
   ========================================================= */

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
    {
        Error_Handler();
    }
}

/* =========================================================
   USART2 INITIALIZATION
   ========================================================= */

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
    {
        Error_Handler();
    }
}

/* =========================================================
   GPIO INITIALIZATION
   ========================================================= */

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
}

/* =========================================================
   ERROR HANDLER
   ========================================================= */

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
    /* User may add error reporting here */
}

#endif