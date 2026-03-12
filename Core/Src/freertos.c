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
#include "dht11.h"
#include "light_sensor.h"
#include "mq2.h"
#include "beep.h"
#include "ws2812b.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  UI_PAGE_SCREENSAVER = 0,
  UI_PAGE_MENU,
  UI_PAGE_ENV,
  UI_PAGE_SWITCH,
  UI_PAGE_LIGHT_MENU,
  UI_PAGE_SYSTEM_MENU,
  UI_PAGE_TIME_MENU,
  UI_PAGE_LED_CTRL,
  UI_PAGE_RGB_CTRL,
  UI_PAGE_TIME,
  UI_PAGE_ALARM_TH,
  UI_PAGE_ALARM
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

typedef enum
{
  ALARM_FIELD_HOUR = 0,
  ALARM_FIELD_MIN
} AlarmField_t;
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
TaskHandle_t ENVTaskHandle;
TaskHandle_t WS2812TaskHandle;

volatile int8_t  g_MenuIndex = 0;      /* 0~3 */
volatile uint8_t g_MenuEnterFlag = 0;  /* ???????? */
volatile int16_t g_LastEncStep = 0;    /* ?????????? */

volatile uint8_t g_LedModeAuto = 0;    /* 0=Manual, 1=Auto */
volatile uint8_t g_LedBrightness[3] = {30, 30, 30};

volatile uint8_t g_EnvTemp = 0;
volatile uint8_t g_EnvHumi = 0;
volatile uint8_t g_EnvTempDec = 0;
volatile uint8_t g_EnvHumiDec = 0;
volatile uint8_t g_EnvValid = 0;
volatile uint8_t g_EnvLastStatus = 0;
volatile uint16_t g_LightRaw = 0;
volatile uint8_t g_LightPercent = 0;
volatile uint8_t g_Mq2Percent = 0;
volatile uint8_t g_SwitchState[2] = {0, 0};
volatile uint8_t g_AlarmHour = 12;
volatile uint8_t g_AlarmMin = 0;
volatile uint8_t g_Mq2Threshold = 50;
volatile WS2812B_Mode_t g_WsMode = WS2812B_MODE_OFF;
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
void ENV_Task_Entry(void *argument);
void LightTest_Task_Entry(void *argument);
void WS2812_Task_Entry(void *argument);
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
  xTaskCreate(ENV_Task_Entry, "ENVTask", 256, NULL, osPriorityLow, &ENVTaskHandle);
  xTaskCreate(WS2812_Task_Entry, "WS2812", 256, NULL, osPriorityLow, &WS2812TaskHandle);
  xTaskCreate(OLED_Task_Entry, "OLEDTask", 512, NULL, osPriorityNormal, &OLEDTaskHandle);
  //xTaskCreate(LightTest_Task_Entry, "LightTest", 512, NULL, osPriorityLow, &OLEDTaskHandle);
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
  uint8_t autoBrightness = 0;

  if (LED_PWM_Init(&htim3) == HAL_OK)
  {
    LED_PWM_Start();
  }

  for (;;)
  {
    if (g_LedModeAuto)
    {
      /* ????????? */
      autoBrightness = g_LightPercent;
      LED_SetAllPercent(autoBrightness, autoBrightness, autoBrightness);
    }
    else
    {
      LED_SetAllPercent(g_LedBrightness[0], g_LedBrightness[1], g_LedBrightness[2]);
    }

    osDelay(30);
  }
}

void ENV_Task_Entry(void *argument)
{
  DHT11_Data_t dht;
  HAL_StatusTypeDef st;
  uint32_t lightAvg = 0;
  uint32_t lastDhtTick = 0;
  uint32_t lastBeepTick = 0;

  DHT11_Init();
  BEEP_Init(); /* ???????????????? */
  osDelay(1200);

  for (;;)
  {
    RTC_TimeTypeDef nowTime;
    RTC_DateTypeDef nowDate;
    uint8_t alarmActive = 0;
    uint8_t mq2Active = 0;

    /* ???? + ???????? */
    g_LightRaw = LightSensor_ReadRaw();
    lightAvg = (lightAvg * 3U + g_LightRaw) / 4U;
    /* ???????????? */
    g_LightPercent = (uint8_t)(100U - (lightAvg * 100U) / 4095U);

    /* MQ2 ???? */
    g_Mq2Percent = MQ2_ReadPercent();

    /* ???? */
    HAL_RTC_GetTime(&hrtc, &nowTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &nowDate, RTC_FORMAT_BIN);
    if ((nowTime.Hours == g_AlarmHour) && (nowTime.Minutes == g_AlarmMin))
    {
      alarmActive = 1U;
    }

    /* MQ2 ??????? */
    if (g_Mq2Percent > g_Mq2Threshold)
    {
      mq2Active = 1U;
    }

    /* MQ2/????????0.5s ???? */
    if (alarmActive || mq2Active)
    {
      if ((HAL_GetTick() - lastBeepTick) >= 500U)
      {
        lastBeepTick = HAL_GetTick();
        BEEP_Toggle();
      }
    }
    else
    {
      BEEP_Off();
    }

    /* DHT11 ?? 1s ???????? BUSY/ERROR */
    if ((HAL_GetTick() - lastDhtTick) >= 1000U)
    {
      lastDhtTick = HAL_GetTick();

      st = DHT11_Read(&dht);
      g_EnvLastStatus = (uint8_t)st;

      if (st == HAL_OK)
      {
        g_EnvTemp = dht.temp_int;
        g_EnvHumi = dht.hum_int;
        g_EnvTempDec = (uint8_t)(dht.temp_dec % 10U);
        g_EnvHumiDec = (uint8_t)(dht.hum_dec % 10U);
        g_EnvValid = 1;
      }
      else
      {
        g_EnvValid = 0;
      }
    }

    osDelay(100);
  }
}

void WS2812_Task_Entry(void *argument)
{
  WS2812B_Init(&htim1);
  WS2812B_UpdateMode(WS2812B_MODE_OFF);

  for (;;)
  {
    if (!WS2812B_IsBusy())
    {
      WS2812B_RenderFrame();
    }
    osDelay(30);
  }
}

void OLED_Task_Entry(void *argument)
{
  const char *menuItems[4] = {"Environment", "Light", "Switch", "System"};
  const char *lightItems[2] = {"LED Control", "RGB Control"};
  const char *systemItems[2] = {"Time", "Alarm"};
  const char *timeItems[2] = {"Time Set", "Alarm Clock"};
  UI_Page_t uiPage = UI_PAGE_SCREENSAVER;
  DateTimeField_t dtField = FIELD_HOUR;
  uint8_t timeEditing = 0;

  uint8_t lightMenuIndex = 0;
  uint8_t systemMenuIndex = 0;
  uint8_t timeMenuIndex = 0;
  uint8_t ledCursor = 0;   /* 0:Mode, 1:LED1, 2:LED2, 3:LED3 */
  uint8_t ledEditing = 0;
  uint8_t switchCursor = 0; /* 0:SW1, 1:SW2 */
  uint8_t rgbModeIndex = 0; /* 0=OFF,1=Breath,2=Flow,3=Gradient,4=Colorful */
  uint8_t rgbEditing = 0;

  RTC_TimeTypeDef rtcTime;
  RTC_DateTypeDef rtcDate;
  uint8_t editHour = 0, editMin = 0, editSec = 0;
  uint8_t editYear = 0, editMonth = 1, editDay = 1;

  AlarmField_t alarmField = ALARM_FIELD_HOUR;
  uint8_t alarmEditing = 0;
  uint8_t alarmThEditing = 0;

  uint8_t enteredIndex = 0xFF;
  int16_t highlightY = 14;          /* ????????Y */
  int16_t lightHighlightY = 13;     /* Light Menu ?????Y */
  int16_t ledHighlightY = 14;       /* LED Control ??????Y */
  int16_t switchHighlightY = 16;    /* Switch ??????Y */
  int16_t systemHighlightY = 15;    /* System Menu ?????Y */
  int16_t timeHighlightY = 15;      /* Time Menu ?????Y */
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

    /* KEY1: ?????????????????? */
    if ((keyEvt == 1) && (uiPage != UI_PAGE_MENU) && (uiPage != UI_PAGE_SCREENSAVER)
        && (uiPage != UI_PAGE_TIME_MENU) && (uiPage != UI_PAGE_TIME)
        && (uiPage != UI_PAGE_ALARM) && (uiPage != UI_PAGE_ALARM_TH))
    {
      g_MenuEnterFlag = 0;
      uiPage = UI_PAGE_MENU;
      continue;
    }

    if (uiPage == UI_PAGE_SCREENSAVER)
    {
      RTC_TimeTypeDef nowTime;
      RTC_DateTypeDef nowDate;

      HAL_RTC_GetTime(&hrtc, &nowTime, RTC_FORMAT_BIN);
      HAL_RTC_GetDate(&hrtc, &nowDate, RTC_FORMAT_BIN);

      /* ???????????? */
      if (g_MenuEnterFlag)
      {
        g_MenuEnterFlag = 0;
        uiPage = UI_PAGE_MENU;
        continue;
      }

      OLED_Clear();
      OLED_ShowString(36, 0, "Smart Home", OLED_6X8);
      OLED_DrawLine(0, 10, 127, 10);

      snprintf(buf, sizeof(buf), "20%02d-%02d-%02d", nowDate.Year, nowDate.Month, nowDate.Date);
      OLED_ShowString(28, 24, buf, OLED_6X8);

      snprintf(buf, sizeof(buf), "%02d:%02d:%02d", nowTime.Hours, nowTime.Minutes, nowTime.Seconds);
      OLED_ShowString(28, 38, buf, OLED_8X16);

      OLED_Update();
      continue;
    }

    if (uiPage == UI_PAGE_MENU)
    {
      /* ?????? KEY1 ???? */
      if (keyEvt == 1)
      {
        g_MenuEnterFlag = 0;
        uiPage = UI_PAGE_SCREENSAVER;
        continue;
      }

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
        else if (g_MenuIndex == 0) /* Environment */
        {
          uiPage = UI_PAGE_ENV;
        }
        else if (g_MenuIndex == 2) /* Switch */
        {
          uiPage = UI_PAGE_SWITCH;
        }
        else if (g_MenuIndex == 3) /* System */
        {
          systemMenuIndex = 0;
          uiPage = UI_PAGE_SYSTEM_MENU;
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

    if (uiPage == UI_PAGE_SWITCH)
    {
      int16_t targetSwitchY;
      int16_t switchDy;

      if (step > 0)
      {
        switchCursor = (uint8_t)((switchCursor + 1U) % 2U);
      }
      else if (step < 0)
      {
        switchCursor = (uint8_t)((switchCursor == 0U) ? 1U : 0U);
      }

      if (g_MenuEnterFlag)
      {
        g_MenuEnterFlag = 0;
        g_SwitchState[switchCursor] = (uint8_t)!g_SwitchState[switchCursor];
        HAL_GPIO_WritePin(SWITCH_1_GPIO_Port, SWITCH_1_Pin,
                          g_SwitchState[0] ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(SWITCH_2_GPIO_Port, SWITCH_2_Pin,
                          g_SwitchState[1] ? GPIO_PIN_SET : GPIO_PIN_RESET);
      }

      targetSwitchY = (int16_t)((switchCursor == 0U) ? 15 : 27);
      switchDy = targetSwitchY - switchHighlightY;
      if (switchDy > 2) switchDy = 2;
      if (switchDy < -2) switchDy = -2;
      switchHighlightY += switchDy;

      OLED_Clear();
      OLED_ShowString(0, 0, "Switch", OLED_6X8);
      OLED_DrawLine(0, 10, 127, 10);
      OLED_ShowString(0, 16, g_SwitchState[0] ? "SWITCH1: ON " : "SWITCH1: OFF", OLED_6X8);
      OLED_ShowString(0, 28, g_SwitchState[1] ? "SWITCH2: ON " : "SWITCH2: OFF", OLED_6X8);
      OLED_ReverseArea(0, switchHighlightY, 128, 10);
      OLED_Update();
      continue;
    }

    if (uiPage == UI_PAGE_SYSTEM_MENU)
    {
      int16_t targetSystemY;
      int16_t systemDy;

      if (step > 0)
      {
        systemMenuIndex = (uint8_t)((systemMenuIndex + 1U) % 2U);
      }
      else if (step < 0)
      {
        systemMenuIndex = (uint8_t)((systemMenuIndex == 0U) ? 1U : 0U);
      }

      if (g_MenuEnterFlag)
      {
        g_MenuEnterFlag = 0;
        if (systemMenuIndex == 0)
        {
          timeMenuIndex = 0;
          uiPage = UI_PAGE_TIME_MENU;
        }
        else
        {
          blinkOn = 1;
          lastBlinkTick = HAL_GetTick();
          alarmThEditing = 0;
          uiPage = UI_PAGE_ALARM_TH;
        }
      }

      targetSystemY = (int16_t)(15 + ((int16_t)systemMenuIndex * 12));
      systemDy = targetSystemY - systemHighlightY;
      if (systemDy > 2) systemDy = 2;
      if (systemDy < -2) systemDy = -2;
      systemHighlightY += systemDy;

      OLED_Clear();
      OLED_ShowString(0, 0, "System Menu", OLED_6X8);
      OLED_DrawLine(0, 10, 127, 10);
      OLED_ShowString(0, 16, (char *)systemItems[0], OLED_6X8);
      OLED_ShowString(0, 28, (char *)systemItems[1], OLED_6X8);
      OLED_ReverseArea(0, systemHighlightY, 128, 10);
      OLED_ShowString(0, 56, "KEY1:Menu", OLED_6X8);

      OLED_Update();
      continue;
    }

    if (uiPage == UI_PAGE_TIME_MENU)
    {
      if (keyEvt == 1)
      {
        uiPage = UI_PAGE_SYSTEM_MENU;
        continue;
      }

      int16_t targetTimeY;
      int16_t timeDy;

      if (step > 0)
      {
        timeMenuIndex = (uint8_t)((timeMenuIndex + 1U) % 2U);
      }
      else if (step < 0)
      {
        timeMenuIndex = (uint8_t)((timeMenuIndex == 0U) ? 1U : 0U);
      }

      if (g_MenuEnterFlag)
      {
        g_MenuEnterFlag = 0;
        if (timeMenuIndex == 0) /* Time Set */
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
        else
        {
          alarmField = ALARM_FIELD_HOUR;
          alarmEditing = 0;
          blinkOn = 1;
          lastBlinkTick = HAL_GetTick();
          uiPage = UI_PAGE_ALARM;
        }
      }

      targetTimeY = (int16_t)(15 + ((int16_t)timeMenuIndex * 12));
      timeDy = targetTimeY - timeHighlightY;
      if (timeDy > 2) timeDy = 2;
      if (timeDy < -2) timeDy = -2;
      timeHighlightY += timeDy;

      OLED_Clear();
      OLED_ShowString(0, 0, "Time Menu", OLED_6X8);
      OLED_DrawLine(0, 10, 127, 10);
      OLED_ShowString(0, 16, (char *)timeItems[0], OLED_6X8);
      OLED_ShowString(0, 28, (char *)timeItems[1], OLED_6X8);
      OLED_ReverseArea(0, timeHighlightY, 128, 10);
      OLED_ShowString(0, 56, "KEY1:Menu", OLED_6X8);
      OLED_Update();
      continue;
    }

    if (uiPage == UI_PAGE_ALARM)
    {
      if (keyEvt == 1)
      {
        uiPage = UI_PAGE_TIME_MENU;
        alarmEditing = 0;
        continue;
      }

      if (g_MenuEnterFlag)
      {
        g_MenuEnterFlag = 0;
        alarmEditing = (uint8_t)!alarmEditing;
        blinkOn = 1;
        lastBlinkTick = HAL_GetTick();
      }

      if (step != 0)
      {
        if (!alarmEditing)
        {
          alarmField = (alarmField == ALARM_FIELD_HOUR) ? ALARM_FIELD_MIN : ALARM_FIELD_HOUR;
        }
        else
        {
          if (alarmField == ALARM_FIELD_HOUR)
          {
            int16_t v = (int16_t)g_AlarmHour + step;
            while (v < 0) v += 24;
            while (v >= 24) v -= 24;
            g_AlarmHour = (uint8_t)v;
          }
          else
          {
            int16_t v = (int16_t)g_AlarmMin + step;
            while (v < 0) v += 60;
            while (v >= 60) v -= 60;
            g_AlarmMin = (uint8_t)v;
          }
        }
      }

      OLED_Clear();
      OLED_ShowString(28, 0, "ALARM", OLED_8X16);
      OLED_DrawLine(0, 18, 127, 18);

      OLED_DrawRectangle(16, 24, 32, 24, OLED_UNFILLED);
      OLED_DrawRectangle(80, 24, 32, 24, OLED_UNFILLED);

      if (!(alarmEditing && alarmField == ALARM_FIELD_HOUR && !blinkOn))
      {
        snprintf(buf, sizeof(buf), "%02d", g_AlarmHour);
        OLED_ShowString(24, 32, buf, OLED_6X8);
      }
      if (!(alarmEditing && alarmField == ALARM_FIELD_MIN && !blinkOn))
      {
        snprintf(buf, sizeof(buf), "%02d", g_AlarmMin);
        OLED_ShowString(88, 32, buf, OLED_6X8);
      }

      OLED_ShowString(52, 32, ":", OLED_8X16);
      OLED_ShowString(20, 52, "hour", OLED_6X8);
      OLED_ShowString(86, 52, "min", OLED_6X8);

      if (alarmField == ALARM_FIELD_HOUR) OLED_ReverseArea(17, 25, 30, 22);
      if (alarmField == ALARM_FIELD_MIN)  OLED_ReverseArea(81, 25, 30, 22);

      OLED_Update();
      continue;
    }

    if (uiPage == UI_PAGE_ALARM_TH)
    {
      if (keyEvt == 1)
      {
        alarmThEditing = 0;
        uiPage = UI_PAGE_SYSTEM_MENU;
        continue;
      }

      if (g_MenuEnterFlag)
      {
        g_MenuEnterFlag = 0;
        alarmThEditing = (uint8_t)!alarmThEditing;
        blinkOn = 1;
        lastBlinkTick = HAL_GetTick();
      }

      if (step != 0 && alarmThEditing)
      {
        int16_t v = (int16_t)g_Mq2Threshold + step * 2;
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        g_Mq2Threshold = (uint8_t)v;
      }

      OLED_Clear();
      OLED_ShowString(0, 0, "Alarm Setting", OLED_6X8);
      OLED_DrawLine(0, 10, 127, 10);
      OLED_ShowString(0, 16, "MQ2 THRESH", OLED_6X8);

      OLED_DrawRectangle(10, 32, 100, 10, OLED_UNFILLED);
      if (!alarmThEditing || blinkOn)
      {
        uint8_t fill = (uint8_t)((g_Mq2Threshold * 98U) / 100U);
        if (fill > 0)
        {
          OLED_DrawRectangle(11, 33, fill, 8, OLED_FILLED);
        }
      }

      snprintf(buf, sizeof(buf), "%3d%%", g_Mq2Threshold);
      OLED_ShowString(44, 48, buf, OLED_6X8);
      if (alarmThEditing && blinkOn)
      {
        OLED_ReverseArea(0, 30, 128, 14);
      }
      OLED_ShowString(0, 56, "KEY1:Back", OLED_6X8);

      OLED_Update();
      continue;
    }

    if (uiPage == UI_PAGE_ENV)
    {
      OLED_Clear();
      OLED_ShowString(0, 0, "Environment", OLED_6X8);
      OLED_DrawLine(0, 10, 127, 10);

      if (g_EnvValid)
      {
        snprintf(buf, sizeof(buf), "Temp:%2d.%1dC", g_EnvTemp, g_EnvTempDec);
        OLED_ShowString(0, 16, buf, OLED_6X8);

        snprintf(buf, sizeof(buf), "Humi:%2d.%1d%%", g_EnvHumi, g_EnvHumiDec);
        OLED_ShowString(0, 28, buf, OLED_6X8);

        snprintf(buf, sizeof(buf), "Light:%3d%%", g_LightPercent);
        OLED_ShowString(0, 40, buf, OLED_6X8);

        snprintf(buf, sizeof(buf), "AIR:%3d%%", g_Mq2Percent);
        OLED_ShowString(0, 52, buf, OLED_6X8);
      }
      else
      {
        OLED_ShowString(8, 24, "Sensor reading...", OLED_6X8);
        OLED_ShowString(8, 36, "CHK DHT11 WIRE", OLED_6X8);

        if (g_EnvLastStatus == (uint8_t)HAL_TIMEOUT)
        {
          OLED_ShowString(8, 48, "ERR: TIMEOUT", OLED_6X8);
        }
        else if (g_EnvLastStatus == (uint8_t)HAL_ERROR)
        {
          OLED_ShowString(8, 48, "ERR: CHECKSUM", OLED_6X8);
        }
        else if (g_EnvLastStatus == (uint8_t)HAL_BUSY)
        {
          OLED_ShowString(8, 48, "ERR: BUSY", OLED_6X8);
        }
        else
        {
          OLED_ShowString(8, 48, "ERR: UNKNOWN", OLED_6X8);
        }
      }

      OLED_Update();
      continue;
    }

    if (uiPage == UI_PAGE_LIGHT_MENU)
    {
      int16_t targetLightY;
      int16_t lightDy;

      targetLightY = (int16_t)(13 + ((int16_t)lightMenuIndex * 12));
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
          rgbModeIndex = (uint8_t)g_WsMode;
          rgbEditing = 0;
          blinkOn = 1;
          lastBlinkTick = HAL_GetTick();
          uiPage = UI_PAGE_RGB_CTRL;
        }
      }

      OLED_Clear();
      OLED_ShowString(0, 0, "Light Menu", OLED_6X8);
      OLED_DrawLine(0, 10, 127, 10);
      OLED_ShowString(0, 14, (char *)lightItems[0], OLED_6X8);
      OLED_ShowString(0, 26, (char *)lightItems[1], OLED_6X8);
      OLED_ReverseArea(0, lightHighlightY, 128, 9);
      OLED_Update();
      continue;
    }

    if (uiPage == UI_PAGE_RGB_CTRL)
    {
      const char *rgbItems[5] = {"OFF", "Breath", "Flow", "Gradient", "Colorful"};

      if (keyEvt == 1)
      {
        rgbEditing = 0;
        uiPage = UI_PAGE_LIGHT_MENU;
        continue;
      }

      if (g_MenuEnterFlag)
      {
        g_MenuEnterFlag = 0;
        rgbEditing = (uint8_t)!rgbEditing;
        blinkOn = 1;
        lastBlinkTick = HAL_GetTick();
      }

      if (step != 0 && rgbEditing)
      {
        if (step > 0)
        {
          rgbModeIndex = (uint8_t)((rgbModeIndex + 1U) % 5U);
        }
        else
        {
          rgbModeIndex = (uint8_t)((rgbModeIndex == 0U) ? 4U : (rgbModeIndex - 1U));
        }

        g_WsMode = (WS2812B_Mode_t)rgbModeIndex;
        WS2812B_UpdateMode(g_WsMode);
      }

      OLED_Clear();
      OLED_ShowString(0, 0, "RGB Control", OLED_6X8);
      OLED_DrawLine(0, 10, 127, 10);
      OLED_ShowString(0, 14, "Mode:", OLED_6X8);
      OLED_ShowString(42, 14, (char *)rgbItems[rgbModeIndex], OLED_6X8);
      OLED_ShowString(0, 56, "KEY1 Back", OLED_6X8);

      if (!rgbEditing || blinkOn)
      {
        OLED_ReverseArea(0, 13, 128, 9);
      }
      OLED_Update();
      continue;
    }

    if (uiPage == UI_PAGE_LED_CTRL)
    {
      uint8_t d1, d2, d3;
      uint8_t i;
      uint8_t disp[3];
      uint8_t autoBrightness = 0;
      int16_t targetLedY;
      int16_t ledDy;

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
        /* Auto ?????????? PWM ?? */
        autoBrightness = g_LightPercent;
        d1 = d2 = d3 = autoBrightness;
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

      /* ???????????????????LED??0.5s?? */
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

      uiPage = UI_PAGE_TIME_MENU;
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

void LightTest_Task_Entry(void *argument)
{
  uint16_t raw = 0;
  uint32_t mv = 0;
  uint8_t percent = 0;
  char buf[16];

  for (;;)
  {
    raw = LightSensor_ReadRaw();
    percent = (uint8_t)((raw * 100U) / 4095U);
    mv = (raw * 3300U) / 4095U;

    OLED_Clear();
    OLED_ShowString(0, 0, "Light ADC Test", OLED_6X8);
    OLED_DrawLine(0, 10, 127, 10);

    snprintf(buf, sizeof(buf), "RAW:%4u", raw);
    OLED_ShowString(0, 18, buf, OLED_6X8);

    snprintf(buf, sizeof(buf), "PCT:%3u%%", percent);
    OLED_ShowString(0, 30, buf, OLED_6X8);

    snprintf(buf, sizeof(buf), "MV:%4lu", (unsigned long)mv);
    OLED_ShowString(0, 42, buf, OLED_6X8);

    OLED_Update();
    osDelay(200);
  }
}
/* USER CODE END Application */


