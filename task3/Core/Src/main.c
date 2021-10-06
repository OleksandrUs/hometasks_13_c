/*
 * task3, main.c
 *
 * Purpose: create program "The Dining Philosophers".
 * 
 * The program simulates the dining philosophers. The philosopher (task) eats incredibly delicious italian food
 * for some time (does some useful work) only if both forks on his sides are available (binary semaphores in this case). 
 * When the philosopher finishes eating, he puts the forks on the table (releases the semaphores) and 
 * thinking for a while. Blinking one of five LEDs, each of which impersonates the philosophers, indicates,
 * that the philosopher is eating. The serial number of the philosoper that is eating is sent via UART.
 * The next solution was used to avoid deadlock: a philosopher is allowed to pick their forks only if both
 * are available at the same time.
 *
 * @author Oleksandr Ushkarenko
 * @version 1.0 06/10/2021
 */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "stdio.h"
#include "string.h"

/*
 * These identifiers are used to determine the microcontroller pins
 * the eight color LEDs connected to.
 */
#define BLUE_LED_1		GPIO_PIN_8
#define RED_LED_1 		GPIO_PIN_9
#define ORANGE_LED_1 	GPIO_PIN_10
#define GREEN_LED_1		GPIO_PIN_11
#define BLUE_LED_2 		GPIO_PIN_12
#define RED_LED_2 		GPIO_PIN_13
#define ORANGE_LED_2	GPIO_PIN_14
#define GREEN_LED_2 	GPIO_PIN_15

#define STACK_SIZE 128		// in 4-byte words
#define TASKS_NUM 5				// The number of the tasks in the program (5 philosophers)
#define MAX_STR_LENGTH 32 // The maximum length of the string that is used as a task name
#define QUEUE_LENGTH 10		// The length of the queue
#define QUEUE_TIMEOUT 1		
#define DELAY 250					// Time delay (in ms) that is used in the tasks
#define UART_TIMEOUT 100

// UART handle (this variable created by STM32CubeMX)
UART_HandleTypeDef huart2;

/* In the array the pin numbers the LEDs connected to are stores. 
 * Each LED impersonates a dining philosopher (a task in this program).
 */
uint16_t leds[TASKS_NUM] = {BLUE_LED_1, RED_LED_1, ORANGE_LED_1, GREEN_LED_1, BLUE_LED_2};

// Task function prototypes.
void task_function(void *param);
void uart_transmitter(void *param);

// Function prototypes created by STM32CubeMX
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

TaskHandle_t task_handles[TASKS_NUM];
SemaphoreHandle_t semaphore_handles[TASKS_NUM];
SemaphoreHandle_t uart_mutex;

/* 
 * The queue is used to pass text the number of philosopher that is eating from one task 
 * (task_function) to the other task (uart_transmitter).
 */
QueueHandle_t queue;

/*
 * The main function of the program (the entry point).
 * Demonstration of the dining philosophers problem solution.
 */
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART2_UART_Init();

	char task_name[MAX_STR_LENGTH];
	for(int i = 0; i < TASKS_NUM; i++){
		sprintf(task_name, "Task %d", i);
		if(xTaskCreate(task_function, task_name, STACK_SIZE, (void*)i, 1, &task_handles[i]) != pdPASS){
			Error_Handler();
		}
	}
	
	if(xTaskCreate(uart_transmitter, "UART transmitter", STACK_SIZE, NULL, 2, NULL) != pdPASS){
		Error_Handler();
	}
	
	for(int i = 0; i < TASKS_NUM; i++){
		if((semaphore_handles[i] = xSemaphoreCreateBinary()) == NULL){
			Error_Handler();
		}
	}
	
	for(int i = 0; i < TASKS_NUM; i++){
		xSemaphoreGive(semaphore_handles[i]);
	}
	
	uart_mutex = xSemaphoreCreateMutex();
	if(uart_mutex == NULL){
		Error_Handler();
	}
	
	queue = xQueueCreate(QUEUE_LENGTH, sizeof(int));
	if(queue == NULL){
		Error_Handler();
	}
	
	vTaskStartScheduler();
	
	while(1){
  }
}

/*
 * This task function simulates a dining philosopher. The philosopher eats incredibly delicious italian food
 * for some time only if both forks on his sides are available (binary semaphores in this case). 
 * When the philosopher finishes eating, he puts the forks on the table (releases the semaphores) and 
 * thinks for a while. Blinking one of the LEDs indicates that the philosopher is eating.
 * The next solution was used to avoid deadlock: a philosopher is allowed to pick
 * their forks only if both are available at the same time.
 *
 * @param a serial number of the task (i.e. the philosopher this task impersonates).
 */
void task_function(void *param)
{	
	int task_num = (int)param;	// the task's (philosopher's) serial number
	while(1){
		// Check if both semaphores (forks on the philosopher's sides) are available.
		if((uxSemaphoreGetCount(semaphore_handles[task_num]) == 1) && (uxSemaphoreGetCount(semaphore_handles[(task_num + 1) % TASKS_NUM]) == 1)){
			
			// Take semaphores (take forks from the table).
			xSemaphoreTake(semaphore_handles[task_num], portMAX_DELAY);
			xSemaphoreTake(semaphore_handles[(task_num + 1) % TASKS_NUM], portMAX_DELAY);
			
			// Doing useful work (eating).
			HAL_GPIO_WritePin(GPIOE, leds[task_num], GPIO_PIN_SET);
			
			if(uxQueueSpacesAvailable(queue) > 0){
				xQueueSend(queue, &task_num, QUEUE_TIMEOUT);
			}
			
			vTaskDelay(pdMS_TO_TICKS(DELAY)); 
			HAL_GPIO_WritePin(GPIOE, leds[task_num], GPIO_PIN_RESET);

			// Take semaphores (put forks on the table).
			xSemaphoreGive(semaphore_handles[task_num]);
			xSemaphoreGive(semaphore_handles[(task_num + 1) % TASKS_NUM]);
			
			// Doing another useful work (thinking).
			vTaskDelay(pdMS_TO_TICKS(DELAY));
		}
	}
}

/*
 * This task function is used to transmit via UART the text message with the serial number of the
 * philosopher that is eating now. The numbers are being received from the other tasks by means of queue.
 *
 * @param a value that is passed as the parameter to the created task.
 */
void uart_transmitter(void *param)
{
	int task_num;
	char str[MAX_STR_LENGTH];
	while(1){
		if(xQueueReceive(queue, &task_num, portMAX_DELAY)){
		
			// It isn't necessary here to take mutex because only this task uses UART
			xSemaphoreTake(uart_mutex, portMAX_DELAY); 
		
			sprintf(str, "Philosopher %d is eating.\n\r", task_num + 1);
			HAL_UART_Transmit(&huart2, (uint8_t*)str, strlen(str), UART_TIMEOUT);
		
			xSemaphoreGive(uart_mutex);
		}
	}
}

/*=================================================================================*/
/*========= All the functions below were created by means of STM32CubeMX ==========*/
/*=================================================================================*/

/*
 * System Clock Configuration
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/*
 * USART2 Initialization Function
 */
static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

/*
 * GPIO Initialization Function
 */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pins : PE8 PE9 PE10 PE11
                           PE12 PE13 PE14 PE15 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}


/*
 * This function is executed in case of error occurrence.
 * Two RED LEDs are switched on, and the other LEDs are switched off.
 */
void Error_Handler(void)
{
  __disable_irq();
	HAL_GPIO_WritePin(GPIOE, BLUE_LED_1 | BLUE_LED_2 | ORANGE_LED_1 | ORANGE_LED_2 | GREEN_LED_1 | GREEN_LED_2, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOE, RED_LED_1 | RED_LED_2, GPIO_PIN_SET);
  while (1)
  {
  }
}
