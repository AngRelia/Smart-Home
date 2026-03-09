/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "queue.h"
#include "OLED.h"
#include "key.h"
#include "my_encoder.h"
#include "tim.h"
#include "rtc.h"
#include "led.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  UI_PAGE_MENU = 0,
  UI_PAGE_LIGHT_MENU,
  UI_PAGE_LED_CTRL,
  UI_PAGE_RGB_CTRL,
  UI_PAGE_TIME
} UI_Page_t;

typedef enum
{
  FIELD_HOUR = 0,
  FIELD_MIN,
  FIELD_SEC,
  FIELD_YEAR,
  FIELD_MONTH,
  FIELD_DAY
} DateTimeField_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
volatile uint8_t g_GlobalKeyNum = 0;
QueueHandle_t xKeyQueue;
TaskHandle_t KeyTaskHandle;
TaskHandle_t OLEDTaskHandle;
TaskHandle_t LEDTaskHandle;

volatile int8_t  g_MenuIndex = 0;      /* 0~3 */
volatile uint8_t g_MenuEnterFlag = 0;  /* ???????? */
volatile int16_t g_LastEncStep = 0;    /* ?????????? */

volatile uint8_t g_LedModeAuto = 0;    /* 0=Manual, 1=Auto */
volatile uint8_t g_LedBrightness[3] = {30, 30, 30};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void Key_Task_Entry(void *argument);
void OLED_Task_Entry(void *argument);
void LED_Control_Task_Entry(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  OLED_Init();
  OLED_ShowString(1, 1, "OS Loading...", OLED_6X8);
  OLED_Update();

  xKeyQueue = xQueueCreate(10, sizeof(uint8_t));
  MY_ENCODER_Init(&htim2);
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  xTaskCreate(Key_Task_Entry, "KeyTask", 256, NULL, osPriorityAboveNormal, &KeyTaskHandle);
  xTaskCreate(LED_Control_Task_Entry, "LEDTask", 256, NULL, osPriorityLow, &LEDTaskHandle);
  xTaskCreate(OLED_Task_Entry, "OLEDTask", 512, NULL, osPriorityLow, &OLEDTaskHandle);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  for (;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void Key_Task_Entry(void *argument)
{
  uint8_t CurrState = 0;
  uint8_t PrevState = 0;
  uint32_t lastKeyScanTick = 0;

  for (;;)
  {
    MY_ENCODER_Process();

    {
      int16_t step = MY_ENCODER_GetMoveSteps();
      if (step != 0)
      {
        if (step > 0) step = 1;
        if (step < 0) step = -1;
        g_LastEncStep = step;
      }
    }

    if (MY_ENCODER_GetClick())
    {
      g_MenuEnterFlag = 1;
    }

    if ((HAL_GetTick() - lastKeyScanTick) >= 20U)
    {
      lastKeyScanTick = HAL_GetTick();
      PrevState = CurrState;
      CurrState = Key_GetState();
      if ((CurrState == 0U) && (PrevState != 0U))
      {
        xQueueSend(xKeyQueue, &PrevState, 0);
      }
    }

    osDelay(2);
  }
}

void LED_Control_Task_Entry(void *argument)
{
  if (LED_PWM_Init(&htim3) == HAL_OK)
  {
    LED_PWM_Start();
  }

  for (;;)
  {
    if (g_LedModeAuto)
    {
      /* ???????100%??????????????? */
      LED_SetAllPercent(100, 100, 100);
    }
    else
    {
      LED_SetAllPercent(g_LedBrightness[0], g_LedBrightness[1], g_LedBrightness[2]);
    }

    osDelay(30);
  }
}

void OLED_Task_Entry(void *argument)
{
  const char *menuItems[4] = {"Temp", "Light", "Time", "System"};
  const char *lightItems[2] = {"LED Control", "RGB Control"};
  UI_Page_t uiPage = UI_PAGE_MENU;
  DateTimeField_t dtField = FIELD_HOUR;
  uint8_t timeEditing = 0;

  uint8_t lightMenuIndex = 0;
  uint8_t ledCursor = 0;   /* 0:Mode, 1:LED1, 2:LED2, 3:LED3 */
  uint8_t ledEditing = 0;

  RTC_TimeTypeDef rtcTime;
  RTC_DateTypeDef rtcDate;
  uint8_t editHour = 0, editMin = 0, editSec = 0;
  uint8_t editYear = 0, editMonth = 1, editDay = 1;

  uint8_t enteredIndex = 0xFF;
  int16_t highlightY = 14;          /* ????????Y */
  int16_t lightHighlightY = 22;     /* Light Menu 高亮条动画Y */
  int16_t ledHighlightY = 14;       /* LED Control 标签高亮动画Y */
  uint8_t clickShowCnt = 0;         /* OPEN?????? */

  uint8_t blinkOn = 1;
  uint32_t lastBlinkTick = 0;

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(20);   /* 50FPS */

  for (;;)
  {
    int16_t targetY;
    int16_t dy;
    int16_t step;
    uint8_t keyEvt = 0;
    char buf[16];

    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    step = g_LastEncStep;
    g_LastEncStep = 0;

    if (xQueueReceive(xKeyQueue, &keyEvt, 0) == pdPASS)
    {
      g_GlobalKeyNum = keyEvt;
    }

    if ((HAL_GetTick() - lastBlinkTick) >= 500U)
    {
      lastBlinkTick = HAL_GetTick();
      blinkOn = (uint8_t)!blinkOn;
    }

    if (uiPage == UI_PAGE_MENU)
    {
      if (step > 0)
      {
        g_MenuIndex++;
        if (g_MenuIndex >= 4) g_MenuIndex = 0;
      }
      else if (step < 0)
      {
        if (g_MenuIndex == 0) g_MenuIndex = 3;
        else g_MenuIndex--;
      }

      if (g_MenuEnterFlag)
      {
        enteredIndex = (uint8_t)g_MenuIndex;
        g_MenuEnterFlag = 0;
        clickShowCnt = 20;

        if (g_MenuIndex == 1) /* ?? Light ?? */
        {
          lightMenuIndex = 0;
          uiPage = UI_PAGE_LIGHT_MENU;
        }
        else if (g_MenuIndex == 2) /* ?? Time */
        {
          HAL_RTC_GetTime(&hrtc, &rtcTime, RTC_FORMAT_BIN);
          HAL_RTC_GetDate(&hrtc, &rtcDate, RTC_FORMAT_BIN);
          editHour = rtcTime.Hours;
          editMin  = rtcTime.Minutes;
          editSec  = rtcTime.Seconds;
          editYear = rtcDate.Year;
          editMonth = rtcDate.Month;
          editDay = rtcDate.Date;
          dtField = FIELD_HOUR;
          timeEditing = 0;
          blinkOn = 1;
          lastBlinkTick = HAL_GetTick();
          uiPage = UI_PAGE_TIME;
        }
      }

      targetY = (int16_t)(14 + ((int16_t)g_MenuIndex * 12));
      dy = targetY - highlightY;
      if (dy > 2) dy = 2;
      if (dy < -2) dy = -2;
      highlightY += dy;

      OLED_Clear();
      OLED_ShowString(0, 0, "Smart Home Menu", OLED_6X8);
      OLED_DrawLine(0, 10, 127, 10);

      OLED_ShowString(6, 14, (char *)menuItems[0], OLED_6X8);
      OLED_ShowString(6, 26, (char *)menuItems[1], OLED_6X8);
      OLED_ShowString(6, 38, (char *)menuItems[2], OLED_6X8);
      OLED_ShowString(6, 50, (char *)menuItems[3], OLED_6X8);

      OLED_ReverseArea(0, highlightY - 1, 128, 10);

      if ((enteredIndex != 0xFF) && (clickShowCnt > 0))
      {
        OLED_ShowString(76, 0, "OPEN:", OLED_6X8);
        OLED_ShowNum(106, 0, (uint32_t)(enteredIndex + 1), 1, OLED_6X8);
        clickShowCnt--;
      }

      OLED_Update();
      continue;
    }

    if (uiPage == UI_PAGE_LIGHT_MENU)
    {
      int16_t targetLightY;
      int16_t lightDy;

      if (keyEvt == 1)
      {
        uiPage = UI_PAGE_MENU;
        continue;
      }

      targetLightY = (int16_t)(22 + ((int16_t)lightMenuIndex * 12));
      lightDy = targetLightY - lightHighlightY;
      if (lightDy > 2) lightDy = 2;
      if (lightDy < -2) lightDy = -2;
      lightHighlightY += lightDy;

      if (step > 0)
      {
        lightMenuIndex++;
        if (lightMenuIndex >= 2) lightMenuIndex = 0;
      }
      else if (step < 0)
      {
        if (lightMenuIndex == 0) lightMenuIndex = 1;
        else lightMenuIndex--;
      }

      if (g_MenuEnterFlag)
      {
        g_MenuEnterFlag = 0;
        if (lightMenuIndex == 0)
        {
          ledCursor = 0;
          ledEditing = 0;
          uiPage = UI_PAGE_LED_CTRL;
        }
        else
        {
          uiPage = UI_PAGE_RGB_CTRL;
        }
      }

      OLED_Clear();
      OLED_ShowString(0, 0, "Light Menu", OLED_6X8);
      OLED_DrawLine(0, 10, 127, 10);
      OLED_ShowString(8, 22, (char *)lightItems[0], OLED_6X8);
      OLED_ShowString(8, 34, (char *)lightItems[1], OLED_6X8);
      OLED_ReverseArea(0, lightHighlightY - 1, 128, 9);
      OLED_Update();
      continue;
    }

    if (uiPage == UI_PAGE_RGB_CTRL)
    {
      if (keyEvt == 1)
      {
        uiPage = UI_PAGE_LIGHT_MENU;
      }

      OLED_Clear();
      OLED_ShowString(0, 0, "RGB Control", OLED_6X8);
      OLED_DrawLine(0, 10, 127, 10);
      OLED_ShowString(8, 28, "TODO", OLED_6X8);
      OLED_ShowString(0, 56, "KEY1 Back", OLED_6X8);
      OLED_Update();
      continue;
    }

    if (uiPage == UI_PAGE_LED_CTRL)
    {
      uint8_t d1, d2, d3;
      uint8_t i;
      uint8_t disp[3];
      int16_t targetLedY;
      int16_t ledDy;

      if (keyEvt == 1)
      {
        ledEditing = 0;
        uiPage = UI_PAGE_LIGHT_MENU;
        continue;
      }

      targetLedY = (int16_t)(14 + ((int16_t)ledCursor * 12));
      ledDy = targetLedY - ledHighlightY;
      if (ledDy > 2) ledDy = 2;
      if (ledDy < -2) ledDy = -2;
      ledHighlightY += ledDy;

      if (g_MenuEnterFlag)
      {
        g_MenuEnterFlag = 0;

        if (ledCursor == 0)
        {
          g_LedModeAuto = (uint8_t)!g_LedModeAuto;   /* ?? Mode ?? */
          ledEditing = 0;
        }
        else if (!g_LedModeAuto)
        {
          ledEditing = (uint8_t)!ledEditing;          /* Manual???/?????? */
        }
      }

      if (!g_LedModeAuto)
      {
        if (!ledEditing)
        {
          if (step > 0)
          {
            ledCursor++;
            if (ledCursor > 3) ledCursor = 0;
          }
          else if (step < 0)
          {
            if (ledCursor == 0) ledCursor = 3;
            else ledCursor--;
          }
        }
        else
        {
          if (step != 0)
          {
            int16_t v = (int16_t)g_LedBrightness[ledCursor - 1] + step * 5;
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            g_LedBrightness[ledCursor - 1] = (uint8_t)v;
          }
        }
      }

      d1 = g_LedBrightness[0];
      d2 = g_LedBrightness[1];
      d3 = g_LedBrightness[2];
      if (g_LedModeAuto)
      {
        d1 = d2 = d3 = 100;
      }
      disp[0] = d1; disp[1] = d2; disp[2] = d3;

      OLED_Clear();
      OLED_ShowString(0, 0, "LED Control", OLED_6X8);
      OLED_DrawLine(0, 10, 127, 10);

      OLED_ShowString(0, 14, "Mode:", OLED_6X8);
      OLED_ShowString(36, 14, g_LedModeAuto ? "Auto" : "Manual", OLED_6X8);

      if (ledEditing && (ledCursor == 1) && !blinkOn) OLED_ShowString(0, 26, "     ", OLED_6X8);
      else                                           OLED_ShowString(0, 26, "LED1:", OLED_6X8);

      if (ledEditing && (ledCursor == 2) && !blinkOn) OLED_ShowString(0, 38, "     ", OLED_6X8);
      else                                           OLED_ShowString(0, 38, "LED2:", OLED_6X8);

      if (ledEditing && (ledCursor == 3) && !blinkOn) OLED_ShowString(0, 50, "     ", OLED_6X8);
      else                                           OLED_ShowString(0, 50, "LED3:", OLED_6X8);

      for (i = 0; i < 3; i++)
      {
        uint8_t y = (uint8_t)(26 + i * 12);
        uint8_t fill = (uint8_t)((disp[i] * 58) / 100);

        OLED_DrawRectangle(34, y, 60, 8, OLED_UNFILLED);
        if (fill > 0)
        {
          OLED_DrawRectangle(35, y + 1, fill, 6, OLED_FILLED);
        }

        OLED_ShowNum(98, y, disp[i], 3, OLED_6X8);
      }

      /* 仅反转左侧标签区，不反转进度条区；编辑LED时按0.5s闪烁 */
      if (ledEditing && (ledCursor != 0))
      {
        if (blinkOn)
        {
          OLED_ReverseArea(0, ledHighlightY-1, 30, 9);
        }
      }
      else
      {
        OLED_ReverseArea(0, ledHighlightY-1, 30, 9);
      }

      if (ledEditing)
      {
        OLED_ShowString(72, 14, "Adj", OLED_6X8);
      }

      OLED_Update();
      continue;
    }

    /* Time?? */
    if (keyEvt == 1)
    {
      RTC_TimeTypeDef sTime = {0};
      RTC_DateTypeDef sDate = {0};
      sTime.Hours = editHour;
      sTime.Minutes = editMin;
      sTime.Seconds = editSec;
      sDate.Year = editYear;
      sDate.Month = editMonth;
      sDate.Date = editDay;
      sDate.WeekDay = rtcDate.WeekDay;
      HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
      HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

      uiPage = UI_PAGE_MENU;
      timeEditing = 0;
      continue;
    }

    if (g_MenuEnterFlag)
    {
      g_MenuEnterFlag = 0;
      timeEditing = (uint8_t)!timeEditing;
      blinkOn = 1;
      lastBlinkTick = HAL_GetTick();
    }

    if (step != 0)
    {
      if (!timeEditing)
      {
        if (step > 0)
        {
          if (dtField == FIELD_DAY) dtField = FIELD_HOUR;
          else dtField = (DateTimeField_t)(dtField + 1);
        }
        else
        {
          if (dtField == FIELD_HOUR) dtField = FIELD_DAY;
          else dtField = (DateTimeField_t)(dtField - 1);
        }
      }
      else
      {
        if (dtField == FIELD_HOUR)
        {
          int16_t v = (int16_t)editHour + step;
          while (v < 0) v += 24;
          while (v >= 24) v -= 24;
          editHour = (uint8_t)v;
        }
        else if (dtField == FIELD_MIN)
        {
          int16_t v = (int16_t)editMin + step;
          while (v < 0) v += 60;
          while (v >= 60) v -= 60;
          editMin = (uint8_t)v;
        }
        else if (dtField == FIELD_SEC)
        {
          int16_t v = (int16_t)editSec + step;
          while (v < 0) v += 60;
          while (v >= 60) v -= 60;
          editSec = (uint8_t)v;
        }
        else if (dtField == FIELD_YEAR)
        {
          int16_t v = (int16_t)editYear + step;
          while (v < 0) v += 100;
          while (v >= 100) v -= 100;
          editYear = (uint8_t)v;
        }
        else if (dtField == FIELD_MONTH)
        {
          int16_t v = (int16_t)editMonth + step;
          while (v < 1) v += 12;
          while (v > 12) v -= 12;
          editMonth = (uint8_t)v;
        }
        else
        {
          int16_t v = (int16_t)editDay + step;
          while (v < 1) v += 31;
          while (v > 31) v -= 31;
          editDay = (uint8_t)v;
        }
      }
    }

    OLED_Clear();
    OLED_ShowString(28, 0, "TIME", OLED_8X16);
    OLED_DrawLine(0, 18, 127, 18);

    OLED_DrawRectangle(8, 24, 32, 24, OLED_UNFILLED);
    OLED_DrawRectangle(48, 24, 32, 24, OLED_UNFILLED);
    OLED_DrawRectangle(88, 24, 32, 24, OLED_UNFILLED);

    /* ????????20YY-MM-DD */
    snprintf(buf, sizeof(buf), "20%02d-%02d-%02d", editYear, editMonth, editDay);
    OLED_ShowString(62, 0, buf, OLED_6X8);

    if (dtField <= FIELD_SEC)
    {
      if (!(timeEditing && dtField == FIELD_HOUR && !blinkOn))
      {
        snprintf(buf, sizeof(buf), "%02d", editHour);
        OLED_ShowString(16, 32, buf, OLED_6X8);
      }
      if (!(timeEditing && dtField == FIELD_MIN && !blinkOn))
      {
        snprintf(buf, sizeof(buf), "%02d", editMin);
        OLED_ShowString(56, 32, buf, OLED_6X8);
      }
      if (!(timeEditing && dtField == FIELD_SEC && !blinkOn))
      {
        snprintf(buf, sizeof(buf), "%02d", editSec);
        OLED_ShowString(96, 32, buf, OLED_6X8);
      }
    }
    else
    {
      if (!(timeEditing && dtField == FIELD_YEAR && !blinkOn))
      {
        snprintf(buf, sizeof(buf), "%02d", editYear);
        OLED_ShowString(16, 32, buf, OLED_6X8);
      }
      if (!(timeEditing && dtField == FIELD_MONTH && !blinkOn))
      {
        snprintf(buf, sizeof(buf), "%02d", editMonth);
        OLED_ShowString(56, 32, buf, OLED_6X8);
      }
      if (!(timeEditing && dtField == FIELD_DAY && !blinkOn))
      {
        snprintf(buf, sizeof(buf), "%02d", editDay);
        OLED_ShowString(96, 32, buf, OLED_6X8);
      }
    }

    if (dtField <= FIELD_SEC)
    {
      OLED_ShowString(12, 52, "hour", OLED_6X8);
      OLED_ShowString(56, 52, "min", OLED_6X8);
      OLED_ShowString(96, 52, "sec", OLED_6X8);
    }
    else
    {
      OLED_ShowString(12, 52, "year", OLED_6X8);
      OLED_ShowString(56, 52, "mon", OLED_6X8);
      OLED_ShowString(96, 52, "day", OLED_6X8);
    }

    /* ???????????????????????? */
    if ((dtField == FIELD_HOUR) || (dtField == FIELD_YEAR)) OLED_ReverseArea(9, 25, 30, 22);
    if ((dtField == FIELD_MIN)  || (dtField == FIELD_MONTH)) OLED_ReverseArea(49, 25, 30, 22);
    if ((dtField == FIELD_SEC)  || (dtField == FIELD_DAY)) OLED_ReverseArea(89, 25, 30, 22);

    OLED_Update();
  }
}
/* USER CODE END Application */

