/* USER CODE BEGIN Header */
#include "OLED.h"
#include "key.h"
#include "rtc.h"
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
/* USER CODE BEGIN Variables */
// ??????????????????????????
volatile uint8_t g_GlobalKeyNum = 0;
QueueHandle_t xKeyQueue; // ??????о??
TaskHandle_t KeyTaskHandle; // ??????????
TaskHandle_t OLEDTaskHandle;  // ???? OLED ??????

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
  OLED_ShowString(1, 1, "OS Loading...",OLED_6X8);
  OLED_Update(); 
  xKeyQueue = xQueueCreate(10, sizeof(uint8_t));
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
  
  xTaskCreate(Key_Task_Entry, 
              "KeyTask", 
              256,                    // ????????? 256?????? 256????(Word) = 1024???
              NULL, 
              osPriorityAboveNormal,  // ?????? cmsis ???????д????
              &KeyTaskHandle);

  // ???? OLEDTask
  xTaskCreate(OLED_Task_Entry, 
              "OLEDTask", 
              512,                    // ????????? 512?????? 512????(Word) = 2048???
              NULL, 
              osPriorityLow, 
              &OLEDTaskHandle);

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
  while(1)
  {
    osDelay(1000); // ??????????????????????????
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/* USER CODE BEGIN Header_Key_Task_Entry */
/**
* @brief Function implementing the KeyTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Key_Task_Entry */
void Key_Task_Entry(void *argument)
{
    uint8_t CurrState = 0;
    uint8_t PrevState = 0;
    while(1)
    {
        PrevState = CurrState;
        CurrState = Key_GetState();
        if (CurrState == 0 && PrevState != 0)
        {
            // ???????????????У?????????0??
            xQueueSend(xKeyQueue, &PrevState, 0); 
        }
        osDelay(20); 
    }
}
/* USER CODE BEGIN Header_OLED_Task_Entry */
/**
* @brief Function implementing the OLEDTask thread.
* @param argument: Not used
* @retval None
*/
void OLED_Task_Entry(void *argument)
{
    /* USER CODE BEGIN OLED_Task_Entry */
    // 1. ???????????????
    OLED_Clear();
    OLED_ShowString(20, 0, "FreeRTOS OS", OLED_8X16);
    OLED_DrawLine(0, 18, 127, 18);
    OLED_ShowString(8, 24, "Sys Tick :", OLED_6X8);
    OLED_ShowString(8, 40, "Key Press:", OLED_6X8);
    OLED_ShowString(8, 52, "Real Time:", OLED_6X8); 
    OLED_Update();

    uint8_t receivedKey = 0;
    
    // 2. ????????????????
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // ????????100ms

    // ????? xLastWakeTime ?????????            
    xLastWakeTime = xTaskGetTickCount();

    // 3. 定义显示与RTC变量
    TickType_t MeasureNow = 0;
    RTC_TimeTypeDef rtcTime;
    RTC_DateTypeDef rtcDate;
    char rtcTimeStr[9]; // "HH:MM:SS"

    /* Infinite loop */
    while(1)
    {
        // --- ??????? 1????????? 100ms ??????? ---
        // ??? vTaskDelayUntil ???????? 100ms ??????Σ???????????к???????
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // --- 核心逻辑 2：计算系统时间 ---
        MeasureNow = xTaskGetTickCount();    // 获取当前系统滴答数

        // --- ??????? 3????????????? ---
        // ????????????? 0????? vTaskDelayUntil ????????????
        if (xQueueReceive(xKeyQueue, &receivedKey, 0) == pdPASS) 
        {
            OLED_ShowNum(78, 40, receivedKey, 2, OLED_6X8);
        }
        else 
        {
            // ???????????????????? None
            OLED_ShowString(78, 40, "None ", OLED_6X8);
        }

        // --- ??????? 4?????? OLED ??????? ---
        // ?????????? Tick
        OLED_ShowNum(78, 24, MeasureNow, 7, OLED_6X8);
        
        // 显示 RTC 实时时间 HH:MM:SS
        // 注意：F1系列读取RTC时，先读Time再读Date用于同步影子寄存器
        HAL_RTC_GetTime(&hrtc, &rtcTime, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&hrtc, &rtcDate, RTC_FORMAT_BIN);

        rtcTimeStr[0] = '0' + (rtcTime.Hours / 10);
        rtcTimeStr[1] = '0' + (rtcTime.Hours % 10);
        rtcTimeStr[2] = ':';
        rtcTimeStr[3] = '0' + (rtcTime.Minutes / 10);
        rtcTimeStr[4] = '0' + (rtcTime.Minutes % 10);
        rtcTimeStr[5] = ':';
        rtcTimeStr[6] = '0' + (rtcTime.Seconds / 10);
        rtcTimeStr[7] = '0' + (rtcTime.Seconds % 10);
        rtcTimeStr[8] = '\0';

        OLED_ShowString(78, 52, rtcTimeStr, OLED_6X8);

        OLED_Update();
    }
    /* USER CODE END OLED_Task_Entry */
}
/* USER CODE END Application */


