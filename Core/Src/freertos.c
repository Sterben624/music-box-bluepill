/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "pn532_stm32f1.h"
#include "df_player.h"
#include "ssd1306.h"
#include "stepper.h"
#include <string.h>
#include <stdio.h>
#include "ws2812b.h"
#include "event_groups.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
	uint64_t uid;
	int8_t   track_number;
} rfid_msg_t;

typedef struct {
	char    track_name[32];
	uint8_t volume;
	uint8_t is_paused;
	uint8_t motor_running;
} display_msg_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TAG_1 0x53c49bf6630001
#define TAG_2 0x53539af6630001

#define HEALTH_NFC_BIT      (1 << 0)
#define HEALTH_DFPLAYER_BIT (1 << 1)
#define HEALTH_WS_BIT       (1 << 2)
#define HEALTH_MOTOR_BIT    (1 << 3)
#define HEALTH_ALL_BITS     (HEALTH_NFC_BIT | HEALTH_DFPLAYER_BIT | HEALTH_WS_BIT | HEALTH_MOTOR_BIT)

#define TIMEOUT_MS 1000  /* health-check period, well under min. IWDG timeout of ~2.18s */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osMessageQId RFID_QueueHandle;
osMessageQId Encoder_QueueHandle;
osMessageQId Display_QueueHandle;

SemaphoreHandle_t motor_start_semHandle;
SemaphoreHandle_t motor_stop_semHandle;

static EventGroupHandle_t healthEventGroup;

extern PN532 pn532;
extern TIM_HandleTypeDef htim1;
extern IWDG_HandleTypeDef hiwdg;

uint8_t volume = 2;
/* USER CODE END Variables */
osThreadId defaultTaskHandle;
osThreadId TaskMotorHandle;
osThreadId TaskRFIDHandle;
osThreadId TaskEncoderHandle;
osThreadId TaskPlayerHandle;
osThreadId TaskDisplayHandle;
osThreadId TaskHealthHandle;
osMutexId i2c_mutexHandle;
osSemaphoreId pause_semHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void SendDisplayUpdate(int8_t track, uint8_t vol, bool paused, bool motor_run)
{
    display_msg_t disp;
    disp.volume = vol;
    disp.is_paused = paused;
    disp.motor_running = motor_run;

    if (track == 1)
        strncpy(disp.track_name, "BTS - Spine Breaker", 32);
    else if (track == 2)
        strncpy(disp.track_name, "J-Hope - Arson", 32);
    else
        strncpy(disp.track_name, "Waiting...", 32);

    xQueueSend(Display_QueueHandle, &disp, 0);
}
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void StartTaskMotor(void const * argument);
void StartTaskRFID(void const * argument);
void StartTaskEncoder(void const * argument);
void StartTaskPlayer(void const * argument);
void StartTaskDisplay(void const * argument);
void StartTaskHealth(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
	*ppxIdleTaskStackBuffer = &xIdleStack[0];
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
	/* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
	healthEventGroup = xEventGroupCreate();
  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* definition and creation of i2c_mutex */
  osMutexDef(i2c_mutex);
  i2c_mutexHandle = osMutexCreate(osMutex(i2c_mutex));

  /* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* definition and creation of pause_sem */
  osSemaphoreDef(pause_sem);
  pause_semHandle = osSemaphoreCreate(osSemaphore(pause_sem), 1);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
	/* Using xSemaphoreCreateBinary instead of osSemaphoreCreate:
	 * CMSIS V1 counting semaphore hangs on Release when already Available.
	 * True binary semaphore returns pdFAIL gracefully instead. */
	motor_start_semHandle = xSemaphoreCreateBinary();
	motor_stop_semHandle = xSemaphoreCreateBinary();
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
	/* Custom item sizes require direct FreeRTOS API instead of osMessageQDef */
	RFID_QueueHandle = xQueueCreate(1, sizeof(rfid_msg_t));
	Encoder_QueueHandle = xQueueCreate(4, sizeof(int8_t));
	Display_QueueHandle = xQueueCreate(2, sizeof(display_msg_t));
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of TaskMotor */
  osThreadDef(TaskMotor, StartTaskMotor, osPriorityAboveNormal, 0, 128);
  TaskMotorHandle = osThreadCreate(osThread(TaskMotor), NULL);

  /* definition and creation of TaskRFID */
  osThreadDef(TaskRFID, StartTaskRFID, osPriorityNormal, 0, 256);
  TaskRFIDHandle = osThreadCreate(osThread(TaskRFID), NULL);

  /* definition and creation of TaskEncoder */
  osThreadDef(TaskEncoder, StartTaskEncoder, osPriorityNormal, 0, 128);
  TaskEncoderHandle = osThreadCreate(osThread(TaskEncoder), NULL);

  /* definition and creation of TaskPlayer */
  osThreadDef(TaskPlayer, StartTaskPlayer, osPriorityNormal, 0, 256);
  TaskPlayerHandle = osThreadCreate(osThread(TaskPlayer), NULL);

  /* definition and creation of TaskDisplay */
  osThreadDef(TaskDisplay, StartTaskDisplay, osPriorityLow, 0, 256);
  TaskDisplayHandle = osThreadCreate(osThread(TaskDisplay), NULL);

  /* definition and creation of TaskHealth */
  osThreadDef(TaskHealth, StartTaskHealth, osPriorityHigh, 0, 128);
  TaskHealthHandle = osThreadCreate(osThread(TaskHealth), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
	/* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
	/* Infinite loop */
	for(;;)
	{
		osDelay(1);
	}
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTaskMotor */
/**
 * @brief Function implementing the TaskMotor thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskMotor */
void StartTaskMotor(void const * argument)
{
  /* USER CODE BEGIN StartTaskMotor */
	/* Infinite loop */
	for(;;)
	{
		/* Block until Player signals motor to start */
		if(osSemaphoreWait(motor_start_semHandle, pdMS_TO_TICKS(1000)) != osOK){
			xEventGroupSetBits(healthEventGroup, HEALTH_MOTOR_BIT);
			continue;
		}

		/* Clear any pending stop signal */
		xSemaphoreTake(motor_stop_semHandle, 0);

		for (;;)
		{
			stepper_Step(true);
			osDelay(1);
			xEventGroupSetBits(healthEventGroup, HEALTH_MOTOR_BIT);

			/* Non-blocking check for stop signal from Player */
			if (osSemaphoreWait(motor_stop_semHandle, 0) == osOK)
				break;
		}

		/* De-energize all coils to save power */
		stepper_Stop();

		/* Clear any pending start signal that arrived during stop */
		xSemaphoreTake(motor_start_semHandle, 0);
	}
  /* USER CODE END StartTaskMotor */
}

/* USER CODE BEGIN Header_StartTaskRFID */
/**
 * @brief Function implementing the TaskRFID thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskRFID */
void StartTaskRFID(void const * argument)
{
  /* USER CODE BEGIN StartTaskRFID */
	uint8_t uid[MIFARE_UID_MAX_LENGTH];
	int32_t uid_len = 0;
	static uint64_t last_tag_id = 0;

	/* Infinite loop */
	for(;;)
	{
		osMutexWait(i2c_mutexHandle, osWaitForever);
		uid_len = PN532_ReadPassiveTarget(&pn532, uid, PN532_MIFARE_ISO14443A, 1000);
		osMutexRelease(i2c_mutexHandle);

		if (uid_len > 0)
		{
			uint64_t current_id = uid_to_u64(uid, uid_len);

			if (current_id != last_tag_id)
			{
				/* New tag placed (or a previously-removed tag came back) */
				last_tag_id = current_id;

				rfid_msg_t msg;
				msg.uid = current_id;

				if (current_id == TAG_1)      msg.track_number = 1;
				else if (current_id == TAG_2) msg.track_number = 2;
				else                          msg.track_number = -1;

				xQueueOverwrite(RFID_QueueHandle, &msg);
			}
			/* else: same tag still present, nothing to do */
		}
		else
		{
			/* No tag currently visible */
			if (last_tag_id != 0)
			{
				/* We previously had a tag and now we don't - treat as removed */
				last_tag_id = 0;

				rfid_msg_t msg;
				msg.uid = 0;
				msg.track_number = -1;
				xQueueOverwrite(RFID_QueueHandle, &msg);
			}
			/* else: no tag before, no tag now, nothing to do */
		}

		xEventGroupSetBits(healthEventGroup, HEALTH_NFC_BIT);

		osDelay(50);
	}
  /* USER CODE END StartTaskRFID */
}

/* USER CODE BEGIN Header_StartTaskEncoder */
/**
 * @brief Function implementing the TaskEncoder thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskEncoder */
void StartTaskEncoder(void const * argument)
{
  /* USER CODE BEGIN StartTaskEncoder */
	static int32_t last_count = 0;
	int32_t count = 0;
	int32_t diff = 0;
	int8_t delta = 0;
	/* Infinite loop */
	for(;;)
	{
		count = (int32_t)(uint16_t)__HAL_TIM_GET_COUNTER(&htim1);
		diff = count - last_count;

		/* Handle counter overflow/underflow */
		if (diff > 32767)  diff -= 65536;
		if (diff < -32767) diff += 65536;

		if (diff >= 2)
		{
			delta = 1;
			xQueueSend(Encoder_QueueHandle, &delta, 0);
			last_count = count;
		}
		else if (diff <= -2)
		{
			delta = -1;
			xQueueSend(Encoder_QueueHandle, &delta, 0);
			last_count = count;
		}

		osDelay(1);
	}
  /* USER CODE END StartTaskEncoder */
}

/* USER CODE BEGIN Header_StartTaskPlayer */
/**
 * @brief Function implementing the TaskPlayer thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskPlayer */
void StartTaskPlayer(void const * argument)
{
  /* USER CODE BEGIN StartTaskPlayer */
	rfid_msg_t rfid_msg;
	int8_t encoder_delta = 0;
	bool is_paused = false;
	int8_t current_track = -1;
	/* Infinite loop */
	for(;;)
	{
		/* Check for new RFID event */
		if (xQueueReceive(RFID_QueueHandle, &rfid_msg, 0) == pdTRUE)
		{
			if (rfid_msg.track_number >= 0)
			{
				if (rfid_msg.track_number == current_track)
				{
					/* Same tag came back - resume, don't restart the track */
					is_paused = false;

					dfplayer_Play();

					xSemaphoreTake(motor_stop_semHandle, 0);
					osSemaphoreRelease(motor_start_semHandle);

					SendDisplayUpdate(current_track, volume, is_paused, current_track >= 0);
				}
				else
				{
					/* New/different tag - start playback from the beginning */
					current_track = rfid_msg.track_number;
					is_paused = false;

					dfplayer_SetTrakNumber(current_track);

					/* Clear pending stop signal before starting motor */
					xSemaphoreTake(motor_stop_semHandle, 0);
					osSemaphoreRelease(motor_start_semHandle);

					SendDisplayUpdate(current_track, volume, is_paused, current_track >= 0);
				}
			}
			else
			{
				/* Tag removed - pause playback and stop the motor
				 * (kept as pause rather than full stop, so DFPlayer resumes
				 * from the same position when the tag is placed back) */
				is_paused = true;

				dfplayer_Pause();
				osSemaphoreRelease(motor_stop_semHandle);

				SendDisplayUpdate(current_track, volume, is_paused, current_track >= 0);
			}
		}

		/* Drain all encoder messages and update volume once */
		bool volume_changed = false;
		while (xQueueReceive(Encoder_QueueHandle, &encoder_delta, 0) == pdTRUE)
		{
			if (encoder_delta > 0 && volume < 30) volume++;
			if (encoder_delta < 0 && volume > 0)  volume--;
			volume_changed = true;
		}

		if (volume_changed)
		{
			dfplayer_SetVolume(volume);

			SendDisplayUpdate(current_track, volume, is_paused, current_track >= 0);
		}

		/* Check for pause semaphore from EXTI */
		if (osSemaphoreWait(pause_semHandle, 0) == osOK)
		{
			if (current_track >= 0)
			{
				is_paused = !is_paused;

				if (is_paused)
				{
					dfplayer_Pause();
					osSemaphoreRelease(motor_stop_semHandle);
				}
				else
				{
					dfplayer_Play();
					xSemaphoreTake(motor_stop_semHandle, 0);
					osSemaphoreRelease(motor_start_semHandle);
				}

				SendDisplayUpdate(current_track, volume, is_paused, current_track >= 0);
			}
		}

		xEventGroupSetBits(healthEventGroup, HEALTH_DFPLAYER_BIT);

		osDelay(10);
	}
  /* USER CODE END StartTaskPlayer */
}
/* USER CODE BEGIN Header_StartTaskDisplay */
/**
 * @brief Function implementing the TaskDisplay thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskDisplay */
void StartTaskDisplay(void const * argument)
{
  /* USER CODE BEGIN StartTaskDisplay */
	display_msg_t msg;
	/* Infinite loop */
	for(;;)
	{
		/* Block until Player sends a display update */
		if (xQueueReceive(Display_QueueHandle, &msg, pdMS_TO_TICKS(1000)) == pdTRUE)
		{
			osMutexWait(i2c_mutexHandle, osWaitForever);

			SSD1306_Clear();

			SSD1306_SetCursor(0, 0);
			SSD1306_WriteString(msg.track_name);

			char status[22];
			sprintf(status, "Vol:%02d %s", msg.volume,
					msg.is_paused ? "Paused " : "Playing");
			SSD1306_SetCursor(0, 16);
			SSD1306_WriteString(status);

			SSD1306_UpdateScreen();

			osMutexRelease(i2c_mutexHandle);

			if (msg.motor_running && !msg.is_paused)
			{
				/* Playing - full brightness */
				WS2812B_Fill(255, 0, 128);
			}
			else if (!msg.motor_running && msg.is_paused)
			{
				/* Paused - dim */
				WS2812B_Fill(60, 0, 30);
			}
			else
			{
				/* Stopped - off */
				WS2812B_Clear();
			}
			WS2812B_Show();
		}
		xEventGroupSetBits(healthEventGroup, HEALTH_WS_BIT);
	}
  /* USER CODE END StartTaskDisplay */
}

/* USER CODE BEGIN Header_StartTaskHealth */
/**
 * @brief Function implementing the TaskHealth thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskHealth */
void StartTaskHealth(void const * argument)
{
  /* USER CODE BEGIN StartTaskHealth */
	/* Infinite loop */
	for(;;)
	{
		EventBits_t bits = xEventGroupWaitBits(
				healthEventGroup,
				HEALTH_ALL_BITS,
				pdTRUE,
				pdTRUE,
				pdMS_TO_TICKS(TIMEOUT_MS)
		);

		if ((bits & HEALTH_ALL_BITS) == HEALTH_ALL_BITS)
		{
			HAL_IWDG_Refresh(&hiwdg);
		}

		osDelay(200);
	}
  /* USER CODE END StartTaskHealth */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == GPIO_PIN_2) {
		osSemaphoreRelease(pause_semHandle);
	}
}
/* USER CODE END Application */

