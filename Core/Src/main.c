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
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// === LCD PIN NAMES ===
#define LCD_CS_PORT   GPIOB       // CHIP SELECTf
#define LCD_CS_PIN    GPIO_PIN_6  // IF LOW THEN LISTEN

#define LCD_DC_PORT   GPIOC       // DATA
#define LCD_DC_PIN    GPIO_PIN_7  // IF LOW; COMMAND | IF HIGH; SEND BYTES

#define LCD_RST_PORT  GPIOA       // REBOOT; LOW TO HIGH
#define LCD_RST_PIN   GPIO_PIN_9

// === OUR HAL FOR THE DISPLAY ===

void LCD_WriteCmd(uint8_t cmd)
{
  HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET); // we're about to send a COMMAND (1byte) to screen
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET); // make screen listen
  HAL_SPI_Transmit(&hspi1, &cmd,1, 10);
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET); // done talking; stop listening
}

void LCD_WriteData(uint8_t data)
{
  HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);   // we're about to send DATA to screen
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET); // make screen listen
  HAL_SPI_Transmit(&hspi1, &data,1, 10);
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET); // done talking; stop listening
}

void LCD_Init(void)
{
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET); // not sending anything
  HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET);
  HAL_Delay(50);
  HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET); // reset done
  HAL_Delay(120);

  // command 0x01 is to RESET
  LCD_WriteCmd(0x01);
  HAL_Delay(100);

  // command 0x11 is sleep out (WAKE UP) to starts the voltage supply
  LCD_WriteCmd(0x11);
  HAL_Delay(120);

  // command 0x3A is for PIXEL FORMAT, then 0x55 means 16bit input (screen manufacturer decided)
  // , we just want 2bytes for the color of a pixel
  LCD_WriteCmd(0x3A);
  LCD_WriteData(0x55);

  // command 0x36 is for MEMORY CONTROL, then data 0x48 for portrait orientation and BGR color order
  LCD_WriteCmd(0x36);
  LCD_WriteData(0x48);

  // command 0x29 is to TURN ON DISPLAY
  LCD_WriteCmd(0x29);
  HAL_Delay(20);
}


//16 BIT COLOR
//16 BIT
void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  uint16_t x2 = x + w - 1;   // LAST COLUMN (last x at 239)
  uint16_t y2 = y + h - 1;   // LAST ROW (last y at 319)

  // === COMMAND TO SET 1ST BOUNDARY PIXEL ===

  LCD_WriteCmd(0x2A);            // next 4 data COMMAND bytes for column

  // START COLUMN X
  LCD_WriteData(x >> 8);         // send high byte
  LCD_WriteData(x & 0xFF);       // send low byte

  // END COLUMN X
  LCD_WriteData(x2 >> 8);        // send high byte
  LCD_WriteData(x2 & 0xFF);      // send low byte

  // === COMMAND TO SET 2ND BOUNDARY PIXEL ===

  LCD_WriteCmd(0x2B);            // next 4 data COMMAND bytes for row

  // START ROW Y
  LCD_WriteData(y >> 8);         // send high byte
  LCD_WriteData(y & 0xFF);       // send low byte

  // END ROW Y
  LCD_WriteData(y2 >> 8);        // send high byte
  LCD_WriteData(y2 & 0xFF);      // send low byte

  // === WRITE TO SCREENS MEMORY TO DRAW BACKGROUND ===

  LCD_WriteCmd(0x2C);

  uint8_t pix[2] = { color >> 8, color & 0xFF};
  HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
  for (int i = 0 ; i < w*h ; i++)
  {
    HAL_SPI_Transmit(&hspi1, pix, 2, 10);
  }
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

#define RGB565(r, g, b)  ( (((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3) )

static uint32_t rng_state = 12345;
static uint32_t Rand(void)
{
  rng_state = rng_state * 1103515245u + 12345u;   // random generator
  return (rng_state >> 16) & 0x7FFF;
}

static void Frame(uint16_t t, uint16_t color)       // make a border t pixels thick
{
  LCD_DrawRect(0, 0, 240, t, color);
  LCD_DrawRect(0, 320 - t, 240, t, color);
  LCD_DrawRect(0, 0, t, 320, color);
  LCD_DrawRect(240 - t, 0, t, 320, color);
}

void LCD_DrawPaper(void)
{
  rng_state = 12345;

  // BASE PAPER
  LCD_DrawRect(0, 0, 240, 320, RGB565(210, 180, 140));

  // BLOTCHES
  for (int i = 0; i < 25; i++)
  {
    // Random width and heigght (between 0 and 30)
    uint16_t w = 10 + Rand() % 30;
    uint16_t h = 10 + Rand() % 30;

    // Random x and y position for top left of blotch rectangle so it wont come off the screen
    uint16_t x = Rand() % (240 - w);
    uint16_t y = Rand() % (320 - h);

    uint16_t blotchColor;
    if (i % 2) { // basically an alternating boolean
      blotchColor = RGB565(218, 190, 150);
    } else {
      blotchColor = RGB565(200, 168, 128);
    }
    LCD_DrawRect(x, y, w, h, blotchColor);
  }

  // GUNK SPECKS
  for (int i = 0 ; i < 60 ; i++)
  {
    uint16_t x = Rand() % 238;       // random column
    uint16_t y = Rand() % 318;       // random row
    uint16_t w = 1 + Rand() % 2;     // 1 or 2 pixels wide
    uint16_t h = 1 + Rand() % 2;     // 1 or 2 pixels tall
    LCD_DrawRect(x, y, w, h, RGB565(150, 110, 70));
  }
  // DARKER BORDERS
  Frame(12, RGB565(205, 173, 131));
  Frame(8,  RGB565(197, 162, 118));
  Frame(4,  RGB565(185, 148, 104));
}

#define T_IRQ_PORT GPIOA
#define T_IRQ_PIN  GPIO_PIN_8 // ACTIVE LOW FOR TRUE PRESSED

uint8_t Touch_IsPressed(void)
{
  if (HAL_GPIO_ReadPin(T_IRQ_PORT, T_IRQ_PIN) == GPIO_PIN_RESET)
  {
  return 1;
  } else {
    return 0;
  }
}

#define T_DO_PORT GPIOB
#define T_DO_PIN  GPIO_PIN_10 // SEND A TOUCH INPUT FROM TOUCH CHIP TO THE STM32

#define T_DIN_PORT GPIOB
#define T_DIN_PIN  GPIO_PIN_4 // SEND AN STM32 INPUT TO THE TOUCH CHIP

#define T_CS_PORT GPIOB
#define T_CS_PIN  GPIO_PIN_5 // ACTIVE LOW TO TALK TO CHIP

#define T_CLK_PORT GPIOB
#define T_CLK_PIN GPIO_PIN_3 // CLOCK; RISE AND FALL EDGE TO SEND 1BIT

static void Touch_Delay(void)
{
  for (volatile int d = 0 ; d < 30 ; d++);
}

// This screen uses the XPT2046 touch chip which we communicate with more crudely since the STM32s SPI wires are already
// being used by the ILI9341 display chip (he's more complex to mess with)
uint16_t Touch_ReadUncalibrated(uint8_t cmd)
{
  uint16_t data = 0;

  HAL_GPIO_WritePin(T_CS_PORT, T_CS_PIN, GPIO_PIN_RESET); // select xpt2046

  // === SENDING X/Y LISTENING COMMAND TO XPT2046 ===
  // OxD0 for X
  // 0x90 for Y

  for (int i = 7 ; i >= 0 ; i--) // translating command to binary XPT2046 understands
  {
    if (cmd & (1 << i) )
    {
     HAL_GPIO_WritePin(T_DIN_PORT,T_DIN_PIN,GPIO_PIN_SET);
    } else {
      HAL_GPIO_WritePin(T_DIN_PORT,T_DIN_PIN,GPIO_PIN_RESET);
    }

    // Set up clock; want it to alternate at a reasonable rate so that the XPT2046 can keep up to read incoming bits
    HAL_GPIO_WritePin(T_CLK_PORT, T_CLK_PIN, GPIO_PIN_SET);
    Touch_Delay();
    HAL_GPIO_WritePin(T_CLK_PORT, T_CLK_PIN, GPIO_PIN_RESET);
    Touch_Delay();
  }

  // === READING WHAT XPT2046 SAYS THE X/Y POSITION IS ==

  for (int i = 15 ; i >= 0 ; i--)
  {
    HAL_GPIO_WritePin(T_CLK_PORT, T_CLK_PIN, GPIO_PIN_SET); // rising edge; TDO stays same
    Touch_Delay();

    if (HAL_GPIO_ReadPin(T_DO_PORT, T_DO_PIN) == GPIO_PIN_SET) // force a 1 in all the spots that have a 1
    {
      data |= (1 << i);
    }

    HAL_GPIO_WritePin(T_CLK_PORT, T_CLK_PIN, GPIO_PIN_RESET); // falling edge; TDO sends a bit
    Touch_Delay();
  }

  HAL_GPIO_WritePin(T_CS_PORT, T_CS_PIN, GPIO_PIN_SET);

  //Positions are only 12 bits long, so move over 3
  return (data >> 3) & 0x0FFF;
}

// === RECALIBRATE THE READING OF THE WAND ===
//force an OK value?

uint8_t Touch_ReadFiltered(uint16_t *outX, uint16_t *outY)
{
  // Total of multiple tries to measure
  uint32_t sumX = 0;
  uint32_t sumY = 0;

  //Highest and lowest readings; basically wanting to bring these extremes close
  uint16_t minX = 4095;
  uint16_t maxX = 0;
  uint16_t minY = 4095;
  uint16_t maxY = 0;

  for (int i = 0 ; i < 5 ; i++) // SAMPLE 5 TIMES !!!!!!MIGHT NEED TO TRY MORE!!!!!!!!!!
  {
    if (!Touch_IsPressed())
    {
      return 0;
    }

    uint16_t rx = Touch_ReadUncalibrated(0xD0);
    uint16_t ry = Touch_ReadUncalibrated(0x90);

    if (rx < 150 || rx > 3950 || ry < 150 || ry > 3950)
    {
      return 0;
    }

    if (rx < minX)
    {
      minX = rx;
    }

    if (rx > maxX)
    {
      maxX = rx;
    }

    if (ry < minY)
    {
      minY = ry;
    }

    if (ry > maxY)
    {
      maxY = ry;
    }

    sumX += rx;
    sumY += ry;
  }

  if ((maxX - minX) > 60 || (maxY - minY) > 60)
  {
    return 0;
  }

  *outX = sumX / 5;
  *outY = sumY / 5;
  return 1;
}

// === SPELL CASTING AND SENDING ===

void Send_Text(const char *text) //ufart what spell casted
{
  HAL_UART_Transmit(&huart2, (uint8_t *)text, strlen(text), 100);
}

#define SPELL_NONE      0
#define SPELL_WATER     1
#define SPELL_LIGHTNING 2
#define SPELL_FIRE      3
#define SPELL_WIND      4

uint8_t Recognize_Spell(int width, int height, int gap)
{
  if (width > 40 && height > 40 && gap < (width + height) / 3)
  {
    return SPELL_FIRE; // fire is when squareish and a pointy side
  }

  if (width > 40 && height < (width/5))
  {
    return SPELL_WATER; // water is when just wide and kinda flat
  }

  if (height > 40 && height > width * 2)
  {
    return SPELL_LIGHTNING; // lightning is when evil water
  }

  if (width > 40 && height > width/4) {
    return SPELL_WIND;
  }

  return SPELL_NONE;
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
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  HAL_GPIO_WritePin(T_CS_PORT, T_CS_PIN, GPIO_PIN_SET);

  LCD_Init();
  LCD_DrawPaper();

  uint16_t cal_x1=0;
  uint16_t cal_y1=0; // calibration point 1

  uint16_t cal_x2=0;
  uint16_t cal_y2=0; // calibration point 2

  LCD_DrawRect(15,15,12,12, RGB565(255,0,0));

  while (!Touch_ReadFiltered(&cal_x1, &cal_y1)); // GUARANTEE STABLE READING

  LCD_DrawPaper();
  while (Touch_IsPressed());
  HAL_Delay(200);

  LCD_DrawRect(214, 294, 12, 12, RGB565(0, 255, 0));

  while (!Touch_ReadFiltered(&cal_x2, &cal_y2)); // SAME ^^^

  LCD_DrawPaper();
  while (Touch_IsPressed());
  HAL_Delay(200);

  int red_x = 20;
  int red_y = 20;

  int green_x = 220;
  int green_y = 300;

  int point_count = 0;

  uint32_t last_touch_time = 0;

  int start_x = 0;
  int start_y = 0;

  int end_x = 0;
  int end_y = 0;

  int min_x = 0;
  int max_x = 0;

  int min_y = 0;
  int max_y = 0;

  int stroke_count = 0;

  int first_min_x = 0;
  int first_max_x = 0;

  int first_min_y = 0;
  int first_max_y = 0;

  int first_gap = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    uint16_t rx;
    uint16_t ry;

    if (Touch_ReadFiltered(&rx, &ry)) // good reading will grant rx and ry pointers that point to them that will fill them
    {
      int x_from_red_tap = rx - cal_x1;     // current x position sample with offset from calibrated x
      int x_between_taps = cal_x2 - cal_x1; // corner to corner to know width
      int pixels_between_boxes_x = green_x - red_x;
      int px = red_x + x_from_red_tap * pixels_between_boxes_x / x_between_taps; // starting with red_x for baseline
      // ^ BASELINE + (FRACTION OF THE WAY ACROSS  * PIXEL DISTANCE BETWEEN THE BOXES) is current pixel's x

      int y_from_red_tap = ry - cal_y1;
      int y_between_taps = cal_y2 - cal_y1;
      int pixels_between_boxes_y = green_y - red_y;
      int py = red_y + y_from_red_tap * pixels_between_boxes_y / y_between_taps; // SAME FOR Y ^^^

      if (px >= 0 && px <= 235 && py >= 0 && py <= 315) // on screen with space for the ink dot
      {
        LCD_DrawRect(px, py, 4, 4, RGB565(70, 40, 20)); //ink
        last_touch_time = HAL_GetTick();

        if (point_count == 0) // STARTING POINT
        {
          start_x = px;
          start_y = py;
          min_x = px;
          max_x = px;
          min_y = py;
          max_y = py;
        }

        end_x = px;
        end_y = py;

        if (px < min_x) { min_x = px; } // if going left, make new dot corners
        if (px > max_x) { max_x = px; } // if going right, ^

        if (py < min_y) { min_y = py; } // if going UP, ^
        if (py > max_y) { max_y = py; } // if going DOWN, ^

        point_count++;
      }
    }



    else if (point_count > 0 && HAL_GetTick() - last_touch_time > 200)
    {
      int width = max_x - min_x;
      int height = max_y - min_y; // extreme dimensions of stroke just finished

      int gap_x = end_x - start_x;
      int gap_y = end_y - start_y; // how far the extreme dimensions are from where they started

      if (gap_x < 0) { gap_x = -gap_x; }
      if (gap_y < 0) { gap_y = -gap_y; }

      int gap = gap_x + gap_y; // total start TO end distance; so the smaller the more closed

        // === FIRST STROKE ===

      if (stroke_count == 0)
      {
        first_min_x = min_x;
        first_max_x = max_x;
        first_min_y = min_y;
        first_max_y = max_y;
        first_gap = gap;    // copy the first stroke's data before 2nd stroke overwrites

        stroke_count = 1; // got 1, need 2nd
        point_count = 0;  // give 2nd stroke a fresh start
      }
        // === SECOND STROKE ===
      else
      {
        int first_width = first_max_x - first_min_x;
        int first_height = first_max_y - first_min_y; // extremes

        int circle_surrounds_symbol = 0;
        // x num grow left to right
        // y num grow top to bottom
        if (min_x < first_min_x && max_x > first_max_x && min_y < first_min_y && max_y > first_max_y)
        {
          circle_surrounds_symbol = 1;
        }

        int circle_is_closed = 0;
        if (gap < (width + height) / 5) // eyeball circle being closed
        {
          circle_is_closed = 1;
        }

        uint8_t spell = SPELL_NONE;

        if (circle_surrounds_symbol && circle_is_closed) // activate spell once sealed
        {
          spell = Recognize_Spell(first_width, first_height, first_gap);
        }

        if (spell == SPELL_WATER)
        {
          LCD_DrawRect(0, 0, 240, 320, RGB565(0, 0, 255));
          Send_Text("WATER\r\n");
          HAL_Delay(600);
        }
        else if (spell == SPELL_LIGHTNING)
        {
          LCD_DrawRect(0, 0, 240, 320, RGB565(255, 255, 0));
          Send_Text("LIGHTNING\r\n");
          HAL_Delay(600);
        }
        else if (spell == SPELL_FIRE)
        {
          LCD_DrawRect(0, 0, 240, 320, RGB565(255, 0, 0));
          Send_Text("FIRE\r\n");
          HAL_Delay(600);
        }

        else if (spell == SPELL_WIND) {
          LCD_DrawRect(0, 0, 240, 320, RGB565(0,0,0));
          Send_Text("WIND\r\n");
          HAL_Delay(600);
        }

        else
        {
          HAL_Delay(300);
        }

        LCD_DrawPaper();
        point_count = 0;
        stroke_count = 0;
      }
    }
    else if (stroke_count == 1 && point_count == 0 && HAL_GetTick() - last_touch_time > 4000)
    {
      LCD_DrawPaper();
      stroke_count = 0;
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

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
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PB10 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB3 PB4 PB5 PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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

