
/* USER CODE BEGIN Header */
/*
 * CAT11 TASK 11A
 * Configure ESP8266 as a TCP Server using AT Commands
 *
 * MCU: STM32F103C8T6 (Blue Pill)
 * Wi-Fi Module: Wemos D1 mini (ESP8266)
 *
 * USART1: STM32 <-> ESP8266
 * USART2: STM32 <-> PC/PuTTY through USB-TTL
 */
/* USER CODE END Header */

#include "main.h"
#include <stdio.h>
#include <string.h>

/* UART handles */
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* Function prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
void Error_Handler(void);

void Send_AT_Command(const char *command, uint32_t wait_ms);

/*
 * Replace these placeholders with your Wi-Fi credentials.
 * Do not publish your real Wi-Fi password in the repository.
 */
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

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

            if (len < sizeof(response))
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
                          response,
                          len,
                          HAL_MAX_DELAY);
    }

    HAL_Delay(300);
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

    /* 1. Check ESP8266 communication */
    Send_AT_Command("AT\r\n", 3000);

    /* 2. Set ESP8266 to Station Mode */
    Send_AT_Command("AT+CWMODE=1\r\n", 3000);

    /* 3. Connect ESP8266 to Wi-Fi */
    Send_AT_Command(
        "AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PASSWORD "\"\r\n",
        25000
    );

    /* 4. Display ESP8266 IP address */
    Send_AT_Command("AT+CIFSR\r\n", 4000);

    /* 5. Enable multiple TCP connections */
    Send_AT_Command("AT+CIPMUX=1\r\n", 3000);

    /* 6. Start TCP Server on port 80 */
    Send_AT_Command("AT+CIPSERVER=1,80\r\n", 3000);

    {
        const char ready[] =
            "\r\n================================\r\n"
            "CAT11 TASK 11A COMPLETE\r\n"
            "ESP8266 TCP SERVER CONFIGURED\r\n"
            "Server Port: 80\r\n"
            "================================\r\n"
            "Waiting for incoming TCP data...\r\n";

        HAL_UART_Transmit(&huart2,
                          (uint8_t *)ready,
                          sizeof(ready) - 1,
                          HAL_MAX_DELAY);
    }

    /* 7. Continuously monitor ESP8266 responses */
    while (1)
    {
        uint8_t ch;

        if (HAL_UART_Receive(&huart1, &ch, 1, 10) == HAL_OK)
        {
            HAL_UART_Transmit(&huart2,
                              &ch,
                              1,
                              HAL_MAX_DELAY);
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
