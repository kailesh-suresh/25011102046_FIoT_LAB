/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Task 9C - Connect Wemos ESP8266 to Wi-Fi
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN 0 */

/*
 * Send AT command to Wemos through USART1
 * Receive complete response into buffer
 * Forward response to PC through USART2
 */
void Send_AT_Command(uint8_t *command, uint16_t command_length)
{
    uint8_t response[256];
    uint16_t index = 0;

    /* Send command to Wemos */
    HAL_UART_Transmit(&huart1,
                      command,
                      command_length,
                      HAL_MAX_DELAY);

    /*
     * Receive response byte by byte into buffer.
     * Stop when no more data is received for 1 second.
     */
    while (index < sizeof(response) - 1)
    {
        if (HAL_UART_Receive(&huart1,
                             &response[index],
                             1,
                             1000) == HAL_OK)
        {
            index++;
        }
        else
        {
            break;
        }
    }

    /* Send complete response to PC */
    if (index > 0)
    {
        HAL_UART_Transmit(&huart2,
                          response,
                          index,
                          HAL_MAX_DELAY);
    }

    /* Small delay before next command */
    HAL_Delay(1000);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    /* USER CODE BEGIN 2 */

    uint8_t response[500];
    uint16_t len;

    uint8_t AT[] =
        "AT\r\n";

    uint8_t MODE[] =
        "AT+CWMODE=1\r\n";

    uint8_t WIFI[] =
        "AT+CWJAP=\"SNUC\",\"snu12345\"\r\n";

    uint8_t IP[] =
        "AT+CIFSR\r\n";

    /* USER CODE END 2 */

    while (1)
    {
        /* ========================= */
        /* 1. TEST ESP8266 */
        /* ========================= */

        HAL_UART_Transmit(&huart1,
                          AT,
                          sizeof(AT) - 1,
                          HAL_MAX_DELAY);

        len = 0;

        while (len < sizeof(response) - 1)
        {
            if (HAL_UART_Receive(&huart1,
                                 &response[len],
                                 1,
                                 2000) == HAL_OK)
            {
                len++;
            }
            else
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

        HAL_Delay(1000);


        /* ========================= */
        /* 2. SET STATION MODE */
        /* ========================= */

        HAL_UART_Transmit(&huart1,
                          MODE,
                          sizeof(MODE) - 1,
                          HAL_MAX_DELAY);

        len = 0;

        while (len < sizeof(response) - 1)
        {
            if (HAL_UART_Receive(&huart1,
                                 &response[len],
                                 1,
                                 2000) == HAL_OK)
            {
                len++;
            }
            else
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

        HAL_Delay(1000);


        /* ========================= */
        /* 3. CONNECT TO WIFI */
        /* ========================= */

        HAL_UART_Transmit(&huart1,
                          WIFI,
                          sizeof(WIFI) - 1,
                          HAL_MAX_DELAY);

        len = 0;

        /*
         * Wi-Fi connection can take much longer.
         * Wait up to 15 seconds for response.
         */

        while (len < sizeof(response) - 1)
        {
            if (HAL_UART_Receive(&huart1,
                                 &response[len],
                                 1,
                                 15000) == HAL_OK)
            {
                len++;
            }
            else
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

        HAL_Delay(2000);


        /* ========================= */
        /* 4. GET IP ADDRESS */
        /* ========================= */

        HAL_UART_Transmit(&huart1,
                          IP,
                          sizeof(IP) - 1,
                          HAL_MAX_DELAY);

        len = 0;

        while (len < sizeof(response) - 1)
        {
            if (HAL_UART_Receive(&huart1,
                                 &response[len],
                                 1,
                                 3000) == HAL_OK)
            {
                len++;
            }
            else
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


        /* ========================= */
        /* STOP */
        /* ========================= */

        while (1)
        {
            HAL_Delay(1000);
        }
    }
}
/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
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

/**
  * @brief USART1 Initialization Function
  * @retval None
  */
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

/**
  * @brief USART2 Initialization Function
  * @retval None
  */
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

/**
  * @brief GPIO Initialization Function
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
}

/**
  * @brief Error Handler
  * @retval None
  */
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
}

#endif /* USE_FULL_ASSERT */