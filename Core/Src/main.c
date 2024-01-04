/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "dma.h"
#include "dma2d.h"
#include "fatfs.h"
#include "jpeg.h"
#include "ltdc.h"
#include "sdmmc.h"
#include "tim.h"
#include "usb_device.h"
#include "gpio.h"
#include "fmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "usbd_cdc_if.h"
#include "pic.h"

extern unsigned char gImage_pic[];
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define one_clk_time	0.00416666f
#define SDRAM_Size 		32*1024*1024
#define SDRAM_BASEADDRESS	0xC0000000
#define DTCM_BASEADDRESS	0x20000000


#define LCD_ON	HAL_GPIO_WritePin(GPIOH, GPIO_PIN_6, GPIO_PIN_SET)
#define LCD_OFF	HAL_GPIO_WritePin(GPIOH, GPIO_PIN_6, GPIO_PIN_RESET)


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define	p_buf

#ifdef	p_buf
	uint16_t *buf = (uint16_t *)0xC0000000;
#else
	SDRAM uint16_t buf[SDRAM_Size/2] = {0};
#endif
uint32_t	tim_start,tim_end;
uint32_t	tim_long;
float		time_us;
uint32_t Get_TIM_CNT(TIM_HandleTypeDef htim)
{
	return	htim.Instance->CNT;
}


uint8_t SDRAM_Test(void)
{
	volatile uint32_t  i = 0;			// ¼ÆÊý±äÁ¿
	uint32_t k = 0;
	uint16_t ReadData = 0; 	// ¶ÁÈ¡µ½µÄÊý¾Ý
	uint32_t Err_Num = 0;

	float    ExecutionSpeed;			// Ö´ÐÐËÙ¶È
	htim2.Instance->CNT = 0;
	tim_start = Get_TIM_CNT(htim2);
	HAL_TIM_Base_Start(&htim2);
	HAL_Delay(100);
	tim_end = Get_TIM_CNT(htim2);
	HAL_TIM_Base_Stop(&htim2);
	tim_long = tim_end - tim_start;
	time_us = tim_long * one_clk_time;
	usb_printf("\r\n delay %.2f us!\r\n", time_us);
	HAL_Delay(10);

	usb_printf("\r\n SDRAM Write an Read test start!\r\n");
	HAL_Delay(10);

	htim2.Instance->CNT = 0;
	tim_start = Get_TIM_CNT(htim2);
	HAL_TIM_Base_Start(&htim2);
	for (k = 0; k < SDRAM_Size/2; k++)
	{
		buf[k] = (uint16_t)k;
	}
	tim_end = Get_TIM_CNT(htim2);
	HAL_TIM_Base_Stop(&htim2);
	tim_long = tim_end - tim_start;
	time_us = tim_long * one_clk_time;
	ExecutionSpeed = (float)SDRAM_Size /1024/1024 /time_us*1000*1000 ;

	usb_printf("\r\nWrite Speed is %.2f MB/s\r\n",ExecutionSpeed);
	HAL_Delay(10);


	htim2.Instance->CNT = 0;
	tim_start = Get_TIM_CNT(htim2);
	HAL_TIM_Base_Start(&htim2);
	for(i = 0; i < SDRAM_Size/2;i++ )
	{
		ReadData = buf[i];  // ´ÓSDRAM¶Á³öÊý¾Ý
	}
	tim_end = Get_TIM_CNT(htim2);
	HAL_TIM_Base_Stop(&htim2);
	tim_long = tim_end - tim_start;
	time_us = tim_long * one_clk_time;


	ExecutionSpeed = (float)SDRAM_Size /1024/1024 /time_us*1000*1000 ;

	usb_printf("\r\nRead Speed is %.2f MB/s\r\n",ExecutionSpeed);
	HAL_Delay(10);


	usb_printf("\r\nstart 16bits test\r\n");
	HAL_Delay(10);
	for(i = 0; i < SDRAM_Size/2;i++ )
	{
		ReadData = buf[i];
		if( ReadData != (uint16_t)i )
		{
			Err_Num++;
		}
	}
	if(Err_Num != 0){
		usb_printf("\r\nSDRAM test failed , %d\r\n", Err_Num);
		HAL_Delay(10);
		return ERROR;
	}

	usb_printf("SDRAM OK\r\n");
	return SUCCESS;
}

uint8_t *frame_buf = (uint8_t *)(SDRAM_BASEADDRESS);
uint8_t *frame = (uint8_t *)(SDRAM_BASEADDRESS + 800*480*3);

void DMA2D_FillColor(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
{

	uint32_t addr = (uint32_t)frame_buf + 3*(800*sy + sx);
	uint32_t offline = 800 - (ex - sx + 1);

	__HAL_RCC_DMA2D_CLK_ENABLE();
	DMA2D->CR &= ~DMA2D_CR_START;
	DMA2D->CR = DMA2D_R2M;
	DMA2D->OPFCCR = LTDC_PIXEL_FORMAT_RGB888;
	DMA2D->OMAR = addr;

	DMA2D->OOR = offline;
	DMA2D->NLR = ((ex - sx + 1) << 16) | ((ey - sy + 1) << 0);
	DMA2D->OCOLR = color;

	DMA2D->CR |= DMA2D_CR_START;

	while((DMA2D->ISR & DMA2D_FLAG_TC) == 0)
	{

	}
	DMA2D->IFCR |= DMA2D_FLAG_TC;
}

void DMA2D_FillFrame(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t *color)
{

	uint32_t addr = (uint32_t)frame_buf + 3*(800*sy + sx);
	uint32_t offline = 800 - (ex - sx + 1);
	uint32_t size = (ex - sx + 1)*(ey - sy + 1)*3;

	__HAL_RCC_DMA2D_CLK_ENABLE();
	DMA2D->CR &= ~DMA2D_CR_START;
	DMA2D->CR = DMA2D_M2M;
	DMA2D->FGPFCCR = LTDC_PIXEL_FORMAT_RGB888;

	DMA2D->FGMAR = (uint32_t)color;
	DMA2D->OMAR = addr;

	DMA2D->FGOR = 0;
	DMA2D->OOR = offline;
	DMA2D->NLR = ((ex - sx + 1) << 16) | ((ey - sy + 1) << 0);

	SCB_CleanDCache_by_Addr((uint32_t *)color, size);
//	SCB_CleanDCache();

	DMA2D->CR |= DMA2D_CR_START;

	while((DMA2D->ISR & DMA2D_FLAG_TC) == 0)
	{

	}
	DMA2D->IFCR |= DMA2D_FLAG_TC;
}




uint32_t color = 0xff;
uint32_t cnt = 0;


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();//刷屏后记得FlushDCache，不然会有画面问题

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

/* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA2D_Init();
  MX_FMC_Init();
  MX_JPEG_Init();
  MX_LTDC_Init();
  MX_SDMMC1_SD_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM5_Init();
  MX_FATFS_Init();
  MX_DMA_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  LCD_ON;

//  for(int i=0; i<480*800*3; i++)
//  {
//	  if(i%3==0)
//		  frame_buf[i] = 0xff;
//	  else if(i%3==1)
//		  frame_buf[i] = 0xff;
//	  else if(i%3==2)
//		  frame_buf[i] = 0xff;
//  }
  DMA2D_FillColor(0, 0, 799, 479, 0x00);

  uint32_t *p = (uint32_t *)0x20000000;
  *p = 0xffffffff;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  //SDRAM_Test();
//	  for(int i=0; i<480*800*3; i++)
//	  {
//		  if(i%3==0)
//			  frame[i] = color;
//		  else if(i%3==1)
//			  frame[i] = color>>8;
//		  else if(i%3==2)
//			  frame[i] = color>>16;
//	  }
	  //SCB_CleanDCache_by_Addr((uint32_t *)frame, 3*800*480);
//	  color++;
//	  DMA2D_FillFrame(0, 0, 799, 479, (uint32_t *)frame);
//	  DMA2D_FillFrame(0, 0, 773, 479, (uint32_t *)gImage_pic);

	  DMA2D_FillColor(0, 0, 199, 479, 0xff);
	  DMA2D_FillColor(199, 0, 399, 479, 0xff00);
	  DMA2D_FillColor(399, 0, 599, 479, 0xff0000);
	  DMA2D_FillColor(599, 0, 799, 479, 0xffffff);

	  if(color == 0xff0000)
		  color = 0xff;
	  else
		  color = color << 8;
	  usb_printf("%d", cnt++);
	  //HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_IMMEDIATE);
	  //HAL_Delay(1000);


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

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
  /** Macro to configure the PLL clock source
  */
  __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 32;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB|RCC_PERIPHCLK_LTDC;
  PeriphClkInitStruct.PLL3.PLL3M = 25;
  PeriphClkInitStruct.PLL3.PLL3N = 192;
  PeriphClkInitStruct.PLL3.PLL3P = 2;
  PeriphClkInitStruct.PLL3.PLL3Q = 4;
  PeriphClkInitStruct.PLL3.PLL3R = 6;
  PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_0;
  PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
  PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_PLL3;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();
  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x24000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x30000000;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER2;
  MPU_InitStruct.BaseAddress = 0x38000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_64KB;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER3;
  MPU_InitStruct.BaseAddress = 0xC0000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_32MB;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER4;
  MPU_InitStruct.BaseAddress = 0x90000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_8MB;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

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

#ifdef  USE_FULL_ASSERT
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

