/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "clcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#ifdef __GNUC__
/* With GCC, small printf (option LD Linker->Libraries->Small printf
   set to 'Yes') calls __io_putchar() */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */
#define ARR_CNT 15
#define CMD_SIZE 128
#define BUZZER_VOLUME 50
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */
uint8_t rx2char;
volatile unsigned char rx2Flag = 0;
volatile char rx2Data[CMD_SIZE];
volatile unsigned char btFlag = 0;
uint8_t btchar;
char btData[CMD_SIZE];
volatile int tim3Flag1Sec=1;
volatile unsigned int openSec;
volatile bool open_flag = false;
volatile bool add_flag = false;
char g_Danger_Menus[5][20]; // 최대 5개 메뉴, 각 20글자 저장
int g_Danger_Cnt = 0;       // 위험 메뉴 개수
int g_Current_Idx = 0;      // 현재 보여주는 메뉴 번호
uint32_t g_Last_Show_Time = 0; // 마지막으로 화면 바꾼 시간
bool g_Warning_Mode = false;   // 경고 모드인지 여부
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
void bluetooth_Event();
void Buzzer_Tone(uint32_t frequency, uint32_t duration_ms);
void Sound_DingDong(void);
void Sound_BeepWarning(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void LCD_clear(void);
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
  MX_USART2_UART_Init();
  MX_USART6_UART_Init();
  MX_I2C1_Init();
  MX_TIM4_Init();
  MX_TIM3_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart2, &rx2char,1);
  HAL_UART_Receive_IT(&huart6, &btchar,1);
  LCD_init(&hi2c1);
  LCD_writeStringXY(0, 0, "LUNCH TIME");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  if(rx2Flag)
	  {
			printf("recv2 : %s\r\n",rx2Data);
			rx2Flag =0;
	//	    HAL_UART_Transmit(&huart6, (uint8_t *)buf, strlen(buf), 0xFFFF);
	  }
	  if(btFlag)
	  {
			btFlag =0;
			bluetooth_Event();
	  }
	  if(tim3Flag1Sec)
	  {
		    tim3Flag1Sec = 0;
		    if(!(openSec%5) && open_flag)
		    {
				int value = 500;
				__HAL_TIM_SetCompare(&htim4,TIM_CHANNEL_1,(value-1)<0?0:value-1);
				open_flag = false;
				openSec = 0;
		    }
	  }
	        // [추가] 3초마다 메뉴 변경 로직
	        if(g_Warning_Mode && g_Danger_Cnt > 1) // 경보 상태이고, 메뉴가 2개 이상일 때만
	        {
	            // 현재 시간 - 마지막 바꾼 시간 >= 3000ms (3초)
	            if(HAL_GetTick() - g_Last_Show_Time >= 3000)
	            {
	                g_Last_Show_Time = HAL_GetTick(); // 시간 갱신

	                // 다음 메뉴로 인덱스 변경 (0 -> 1 -> 2 -> 0 ...)
	                g_Current_Idx++;
	                if(g_Current_Idx >= g_Danger_Cnt) g_Current_Idx = 0;

	                // LCD 아랫줄만 갱신 (공백으로 지우고 씀)
	                LCD_writeStringXY(1, 0, "                ");
	                LCD_writeStringXY(1, 0, g_Danger_Menus[g_Current_Idx]);
	            }
	        }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  hi2c1.Init.ClockSpeed = 10000;
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
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 84-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 1000-1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 84-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1000-1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 84-1;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 20000-1;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
  HAL_TIM_MspPostInit(&htim4);

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
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 9600;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED_GREEN_Pin|LED_RED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_GREEN_Pin LED_RED_Pin */
  GPIO_InitStruct.Pin = LED_GREEN_Pin|LED_RED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  HAL_GPIO_WritePin(GPIOB, LED_G_Pin|LED_A_Pin|LED_B_Pin|LED_C_Pin
                            |LED_D_Pin|LED_E_Pin|LED_F_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = LED_G_Pin|LED_A_Pin|LED_B_Pin|LED_C_Pin
                            |LED_D_Pin|LED_E_Pin|LED_F_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void Buzzer_Tone(uint32_t frequency, uint32_t duration_ms)
{
    if (frequency == 0) return;

    // 1. 주파수 설정 (ARR 계산)
    uint32_t period = 1000000 / frequency;

    // 2. 볼륨 설정 (Pulse/CCR 계산)
    uint32_t pulse = (period * BUZZER_VOLUME) / 1000;

    // [수정] 초기화 코드에 맞춰 TIM_CHANNEL_3 사용 (1 -> 3)
    __HAL_TIM_SET_AUTORELOAD(&htim2, period - 1);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pulse);
    __HAL_TIM_SET_COUNTER(&htim2, 0);

    // 3. 소리 시작
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);

    // 4. 지속 시간 대기
    HAL_Delay(duration_ms);

    // 5. 소리 끄기
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
}

// [안전] 딩~동~ 소리
void Sound_DingDong(void)
{
    Buzzer_Tone(2093, 200); // 딩 (높은 도, C7)
    HAL_Delay(50);          // 약간의 공백
    Buzzer_Tone(1568, 400); // 동 (솔, G6) - 길게
}

// [위험] 삐----- 소리
void Sound_BeepWarning(void)
{
    // 경고음: 1000Hz로 1초간 길게
    Buzzer_Tone(1000, 1000);
}
void LCD_clear(void)
{
    // 화면의 첫 번째 줄, 두 번째 줄을 공백으로 덮어씀
    LCD_writeStringXY(0, 0, "                "); // 공백 16칸
    LCD_writeStringXY(1, 0, "                "); // 공백 16칸
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)		//1ms 마다 호출
{
	static int tim3Cnt = 0;

	if (open_flag)
	{
		tim3Cnt++;
		if(tim3Cnt >= 1000) //1ms * 1000 = 1Sec
		{
			tim3Flag1Sec = 1;
			openSec++;
			tim3Cnt = 0;
		}
	}
}

void bluetooth_Event()
{
	int i = 0;
	char * pToken;
	char * pArray[ARR_CNT] = {0};
	char recvBuf[CMD_SIZE] = {0};

	strcpy(recvBuf, btData);
	printf("btData : %s\r\n", btData);

	pToken = strtok(recvBuf, "[@]");
	while (pToken != NULL)
	{
		pArray[i] = pToken;
		if (++i >= ARR_CNT) break;
		pToken = strtok(NULL, "[@]");
	}

	if(pArray[1] == NULL) return;

	// 1. WARNING 처리 (위험!)
	if (!strcmp(pArray[1], "WARNING"))
	{
		g_Warning_Mode = true;

		g_Danger_Cnt = 0;
		for(int k=3; k<ARR_CNT; k++) {
			if(pArray[k] != NULL) {
				strcpy(g_Danger_Menus[g_Danger_Cnt], pArray[k]);
				g_Danger_Cnt++;
			}
		}

		LCD_clear();
		LCD_writeStringXY(0, 0, pArray[2]);
		LCD_writeStringXY(1, 0, g_Danger_Menus[0]);

		g_Current_Idx = 0;
		g_Last_Show_Time = HAL_GetTick();

		// [수정] 빨간불 켜기, 초록불 끄기
		HAL_GPIO_WritePin(GPIOB, LED_RED_Pin, GPIO_PIN_SET);     // Red ON
		HAL_GPIO_WritePin(GPIOB, LED_GREEN_Pin, GPIO_PIN_RESET); // Green OFF

		Sound_BeepWarning();
	}

	// 2. SAFE 처리 (안전)
	else if (!strcmp(pArray[1], "SAFE"))
	{
		g_Warning_Mode = false;
		LCD_clear();
		LCD_writeStringXY(0, 0, pArray[2]);
		LCD_writeStringXY(1, 0, pArray[3]);

		// [정상] 초록불 켜기, 빨간불 끄기
		HAL_GPIO_WritePin(GPIOB, LED_GREEN_Pin, GPIO_PIN_SET);   // Green ON
		HAL_GPIO_WritePin(GPIOB, LED_RED_Pin, GPIO_PIN_RESET);   // Red OFF

		Sound_DingDong();

		// 문 열기
		int value = 1500;
		__HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_1, value - 1);
		open_flag = true;
		openSec = 0;
	}

	// 3. UNKNOWN 처리 (미등록)
	else if (!strcmp(pArray[1], "UNKNOWN"))
	{
		g_Warning_Mode = false;
		LCD_clear();
		LCD_writeStringXY(0, 0, "Unknown User");

		// [정상] 빨간불 켜기 (경고)
		HAL_GPIO_WritePin(GPIOB, LED_RED_Pin, GPIO_PIN_SET);     // Red ON
		HAL_GPIO_WritePin(GPIOB, LED_GREEN_Pin, GPIO_PIN_RESET); // Green OFF

		Buzzer_Tone(500, 300);
	}
}
/**
  * @brief  Retargets the C library printf function to the USART.
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the USART6 and Loop until the end of transmission */
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);

  return ch;
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    // 1. PC 시리얼
    if(huart->Instance == USART2)
    {
        static int i = 0;
        rx2Data[i] = rx2char;
        if((rx2Data[i] == '\r') || (rx2Data[i] == '\n'))
        {
            rx2Data[i] = '\0';
            rx2Flag = 1;
            i = 0;
        }
        else
        {
            // [안전장치 추가]
            if(i < CMD_SIZE - 1) i++;
        }
        HAL_UART_Receive_IT(&huart2, &rx2char, 1);
    }

    // 2. 블루투스 (이 부분이 중요합니다!)
    if(huart->Instance == USART6)
    {
        static int i = 0;
        btData[i] = btchar;
        if((btData[i] == '\n') || (btData[i] == '\r'))
        {
            btData[i] = '\0';
            btFlag = 1;
            i = 0;
        }
        else
        {
            // [안전장치 추가] 이 줄이 없으면 소리가 한 번 나고 멈춥니다.
            if(i < CMD_SIZE - 1) i++;
        }
        HAL_UART_Receive_IT(&huart6, &btchar, 1);
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
