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

osMessageQId RFID_QueueHandle;
osMessageQId Encoder_QueueHandle;
osMessageQId Display_QueueHandle;

SemaphoreHandle_t motor_start_semHandle;
SemaphoreHandle_t motor_stop_semHandle;
/* USER CODE END Variables */
osThreadId defaultTaskHandle;
osThreadId TaskMotorHandle;
osThreadId TaskRFIDHandle;
osThreadId TaskEncoderHandle;
osThreadId TaskPlayerHandle;
osThreadId TaskDisplayHandle;
osMutexId i2c_mutexHandle;
osSemaphoreId pause_semHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void StartTaskMotor(void const * argument);
void StartTaskRFID(void const * argument);
void StartTaskEncoder(void const * argument);
void StartTaskPlayer(void const * argument);
void StartTaskDisplay(void const * argument);

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
		osDelay(1);
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
	/* Infinite loop */
	for(;;)
	{
		osDelay(1);
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
	/* Infinite loop */
	for(;;)
	{
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
	/* Infinite loop */
	for(;;)
	{
		osDelay(1);
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
	/* Infinite loop */
	for(;;)
	{
		osDelay(1);
	}
	/* USER CODE END StartTaskDisplay */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

