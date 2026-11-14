/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bme280.h" 
#include "ESPLib.h" 
#include <stdio.h> 
#include <string.h> 
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
// BME280 Variables
struct bme280_dev bme280;
struct bme280_settings settings; 
uint8_t bme_i2c_addr = BME280_I2C_ADDR_PRIM << 1; // 0x76 << 1 = 0xEC for HAL

// Wi-Fi & MQTT Credentials
const char WIFI_SSID[] = "SNUC";
const char WIFI_PASS[] = "snu12345";
const char MQTT_BROKER[] = "broker.emqx.io";
const char MQTT_PORT[]   = "1883";

char rxTopic[32];
char rxMsg[64];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void Debug_Print(const char *msg);
int8_t user_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);
int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);
void user_delay_us(uint32_t period, void *intf_ptr);
void Setup_Hardware_Sensors(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Print debug messages to PC Terminal over USART2
void Debug_Print(const char *msg) {
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 100);
}

// Bridge Bosch BME280 Driver I2C Read to STM32 HAL
int8_t user_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    uint8_t dev_addr = *(uint8_t*)intf_ptr;
    if (HAL_I2C_Mem_Read(&hi2c1, dev_addr, reg_addr, I2C_MEMADD_SIZE_8BIT, reg_data, len, 100) == HAL_OK) {
        return BME280_OK;
    }
    return BME280_E_COMM_FAIL;
}

int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    uint8_t dev_addr = *(uint8_t*)intf_ptr;
    if (HAL_I2C_Mem_Write(&hi2c1, dev_addr, reg_addr, I2C_MEMADD_SIZE_8BIT, (uint8_t*)reg_data, len, 100) == HAL_OK) {
        return BME280_OK;
    }
    return BME280_E_COMM_FAIL;
}

// Bridge Bosch Delay to STM32 HAL Delay
void user_delay_us(uint32_t period, void *intf_ptr) {
    HAL_Delay((period + 999) / 1000);
}

void Setup_Hardware_Sensors(void) {
    bme280.intf_ptr = &bme_i2c_addr;
    bme280.intf     = BME280_I2C_INTF;
    bme280.read     = user_i2c_read;
    bme280.write    = user_i2c_write;
    bme280.delay_us = user_delay_us;

    if (bme280_init(&bme280) == BME280_OK) {
        Debug_Print("[BME280]: Driver Initialized Successfully!\r\n");
    } else {
        Debug_Print("[BME280 ERROR]: Communication Failed!\r\n");
    }

    settings.osr_h        = BME280_OVERSAMPLING_1X;
    settings.osr_p        = BME280_OVERSAMPLING_16X;
    settings.osr_t        = BME280_OVERSAMPLING_2X;
    settings.filter       = BME280_FILTER_COEFF_16;
    settings.standby_time = BME280_STANDBY_TIME_62_5_MS;

    uint8_t settings_sel = BME280_SEL_OSR_PRESS | BME280_SEL_OSR_TEMP | BME280_SEL_OSR_HUM | BME280_SEL_FILTER | BME280_SEL_STANDBY;
    bme280_set_sensor_settings(settings_sel, &settings, &bme280);
    bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &bme280);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(2000);
  Debug_Print("--- Starting STM32 IoT Environmental Monitoring Pipeline ---\r\n");

  // 1. Initialize BME280 Sensor
  Setup_Hardware_Sensors();

  // 2. Initialize ESP8266 and Connect to Wi-Fi
  Debug_Print("Initializing ESP8266...\r\n");
  ESP8266_Init(1, huart1); // Station Mode = 1

  Debug_Print("Connecting to Wi-Fi...\r\n");
  ESP8266_wifiConnect((char*)WIFI_SSID, (char*)WIFI_PASS);
  Debug_Print("Wi-Fi Connected!\r\n");

  // 3. Open TCP Connection to MQTT Broker
  Debug_Print("Connecting to Broker...\r\n");
  ESP8266_portConnect("TCP", (char*)MQTT_BROKER, (char*)MQTT_PORT);

  // 4. Send MQTT CONNECT Control Packet
  MQTT_InitTypeDef_t mqttInitConfig;
  mqttInitConfig.clientID    = "STM32_BME280_Node_SNUC_01";
  mqttInitConfig.keepAlive   = 500;
  mqttInitConfig.connectFlag = CleanSession; // Clean Session

  MQTT_connectBroker(&mqttInitConfig);
  Debug_Print("MQTT Broker Connected!\r\n");

  // 5. Subscribe to Incoming Control Topic
  MQTT_subscribeTopic("stm32/control", 0);
  Debug_Print("Subscribed to stm32/control!\r\n");

  uint32_t lastPubTime = 0;
  uint32_t count = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
 // while (1)
  //{
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
 // }
	while(1) {
		// Read sensor and publish telemetry every 5 seconds
      if ((HAL_GetTick() - lastPubTime) > 5000) {
          lastPubTime = HAL_GetTick();

          struct bme280_data comp_data;
          bme280_get_sensor_data(BME280_ALL, &comp_data, &bme280);

          char payload[128];
 				
					snprintf(payload, sizeof(payload), 
         "{\"status\":\"success\",\"temperature\":%.2f,\"humidity\":%.2f,\"pressure\":%.2f}", 
         comp_data.temperature, comp_data.humidity, comp_data.pressure / 100.0);
          
          MQTT_publishTopic("stm32/environment", payload, 0);
          
          Debug_Print("[BME280 TELEMETRY PUBLISHED]: ");
          Debug_Print(payload);
          Debug_Print("\r\n");
      }

      // Check for incoming MQTT control messages
      MQTTDataHandler(rxTopic, rxMsg);

      if (strlen(rxMsg) > 0) {
          Debug_Print("----------------------------\r\n");
          Debug_Print("[RECEIVED TOPIC]: ");
          Debug_Print(rxTopic);
          Debug_Print("\r\n[RECEIVED MSG]: ");
          Debug_Print(rxMsg);
          Debug_Print("\r\n----------------------------\r\n");

          if (strstr(rxMsg, "ON") != NULL) {
              HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
          } else if (strstr(rxMsg, "OFF") != NULL) {
              HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
          }

          memset(rxMsg, 0, sizeof(rxMsg));
          memset(rxTopic, 0, sizeof(rxTopic));
      }
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
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
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
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
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void MQTT_MessageReceivedCallback(char *topic, char *payload) 
{
    if (strcmp(topic, "stm32/control") == 0) 
    {
        if (strcmp(payload, "ON") == 0) 
        {
            // Turn ON onboard LED (Active-LOW: RESET pulls PC13 LOW)
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        } 
        else if (strcmp(payload, "OFF") == 0) 
        {
            // Turn OFF onboard LED (SET pulls PC13 HIGH)
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        }
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
