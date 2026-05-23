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
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stdio.h"
#include <stdlib.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BUFFER_SIZE 20
#define LUT_SIZE 28
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;

/* USER CODE BEGIN PV */

uint8_t brilho_led = 50;
uint8_t resistencia_input = 0;

char rx_buffer[10];
uint8_t rx_index = 0;
uint8_t rx_data;

typedef struct {
    uint8_t pwm;
    float resistance;
} LutPoint;

LutPoint lut[LUT_SIZE] = {
};

#define RX_BUFFER_SIZE 50
char rx_dma_buffer[RX_BUFFER_SIZE];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int file, char *ptr, int len) {
  // Receber os dados do printf e enviar pela UART
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}

float read_adc(void){
	// realiza a leitura do valor no ADC e retorna um float com a resistencia medida
	uint32_t adc_value = 0;
	float ldr_resistance = 0.0;
	const float R_FIXO = 10000.0;

	HAL_ADC_Start(&hadc1);

	if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
	{
	  // Obtém o valor (0 a 4095)
	  adc_value = HAL_ADC_GetValue(&hadc1);

	  if (adc_value > 0 && adc_value < 4095)
	  {
		  ldr_resistance = R_FIXO * ((4095.0 / (float)adc_value) - 1.0);
	  }
	}
	HAL_ADC_Stop(&hadc1);

	return ldr_resistance;
}

float resistencia_media() {
	// Calcula a média móvel do valor da resistencia obtido pelo ldr
    static float buffer[BUFFER_SIZE] = {0};
    static int index = 0;
    static float soma = 0.0;
    static int count = 0;

    //Soma os valores do buffer
    soma -= buffer[index];
    buffer[index] = read_adc();
    soma += buffer[index];

    // Avança o índice
    index = (index + 1) % BUFFER_SIZE;

    // Se o buffer ainda não encheu
    if (count < BUFFER_SIZE) {
        count++;
        return soma / count;
    }

    return soma / BUFFER_SIZE;
}


float medir_resistencia_media() {
    float soma = 0;

    for(int i = 0; i < 20; i++) {
        soma += read_adc();
        HAL_Delay(5);
    }

    return soma / 20;
}

uint8_t resistencia_pwm(float res){
	// Retorna um valor para o pwm proporcional a resistencia desejada, usando a lut como referencia

	if (res <= lut[0].resistance){ return lut[0].pwm; }
	if (res >= lut[LUT_SIZE-1].resistance){ return lut[LUT_SIZE-1].pwm; }

	for(int i = 0; i < LUT_SIZE-1; i++) {
		// Procura se a resistencia desejada está entre os valores da lut (ordem crescente)
		float r1 = lut[i].resistance;
		float r2 = lut[i + 1].resistance;

		if(res >= r1 && res <= r2) {
			uint8_t pwm1 = lut[i].pwm;
			uint8_t pwm2 = lut[i + 1].pwm;

			if (r1 == r2)
				return pwm1;

			// Interpolação linear (a matemática ajusta automaticamente o sinal negativo do PWM)
			float pwm = pwm1 + ((res - r1) * (pwm2 - pwm1)) / (r2 - r1);

			return (uint8_t)pwm;
		}
	}

	return 0;
}


void criar_lut(){
	// Cria a Lut para a relação pwm -> resistencia do ldr
	uint8_t index_lut = 0;

	for (int i = 100; i >= 15; i-=5){
		__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, i);
		HAL_Delay(200);

		lut[index_lut].pwm = i;
		lut[index_lut].resistance = medir_resistencia_media();

		printf("PWM: %d | RES: %d\r\n",
		               lut[index_lut].pwm,
		               (int)lut[index_lut].resistance);

		index_lut ++;
	}

	for (int i = 10; i >= 1; i--){
			__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, i);
			HAL_Delay(200);

			lut[index_lut].pwm = i;
			lut[index_lut].resistance = medir_resistencia_media();

			printf("PWM: %d | RES: %d\r\n",
					               lut[index_lut].pwm,
					               (int)lut[index_lut].resistance);

			index_lut ++;
		}
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
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_USB_DEVICE_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);

  criar_lut();

  //  HAL_UART_Receive_IT(&huart2, &rx_data, 1);
  HAL_UARTEx_ReceiveToIdle_DMA(&huart2, (uint8_t *)rx_dma_buffer, RX_BUFFER_SIZE);
  __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, brilho_led);

	  float resistance = resistencia_media();
	  // ALTERADO AQUI: Removido %f e adicionado cast (int) para converter float em int no print
	  printf("Resistencia media: %d | Brilho LED: %d%%\n", (int)resistance, brilho_led);

	  HAL_Delay(100);
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_USB;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 100;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size){
	if (huart->Instance == USART2) {
	// Garante que a string recebida termine com o caractere nulo
	rx_dma_buffer[Size] = '\0';

	int nova_res_int;
	int index, novo_pwm;

	if (rx_dma_buffer[0] == 'R' || rx_dma_buffer[0] == 'r') {
		// Lê como %d em vez de %f
		if (sscanf(rx_dma_buffer, "%*c %d", &nova_res_int) == 1) {
			// Converte de volta para float para a função de resistência processar
			brilho_led = resistencia_pwm((float)nova_res_int);
			// Imprime como inteiro
			printf("Resistencia alvo atualizada para: %d\r\n", nova_res_int);
		}
	}

	else if (rx_dma_buffer[0] == 'L' || rx_dma_buffer[0] == 'l') {
		// Lê o ultimo parâmetro como %d
		if (sscanf(rx_dma_buffer, "%*c %d %d %d", &index, &novo_pwm, &nova_res_int) == 3) {
			if (index >= 0 && index < LUT_SIZE) {
				lut[index].pwm = (uint8_t)novo_pwm;
				lut[index].resistance = (float)nova_res_int; // Salva como float no struct para não quebrar a interpolação

				// Imprime os valores todos como inteiros %d
				printf("LUT[%d] atualizada: PWM=%d, RES=%d\r\n", index, lut[index].pwm, (int)lut[index].resistance);
			} else {
				printf("Erro: Indice da LUT fora do limite!\r\n");
			}
		}
	}

	// Reinicia o DMA para aguardar o próximo comando do terminal
	HAL_UARTEx_ReceiveToIdle_DMA(&huart2, (uint8_t *)rx_dma_buffer, RX_BUFFER_SIZE);
	__HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);
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
  * where the assert_param error has occurred.
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
