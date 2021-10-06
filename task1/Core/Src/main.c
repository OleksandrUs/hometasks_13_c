/*
 * task1, main.c
 * Purpose: create program with livelock.
 * 
 * The program is an artificial example of livelock. Two tasks (processes) are being considered.
 * Each task uses a common resource (the LEDs of green colors). A mutex is used to synchronize
 * these tasks. The first task takes the resource (mutex) and checks if the second task is blocked.
 * If it is so, the first task gives the resource (mutex) and waits for it. The second task takes 
 * the mutex and checks if the first task is in blocked states. If it is, the second task gives the
 * resource and  waits for it. And this series of actions repeats indefinitely - the tasks change
 * its states with no progress. The useful work performed by tasks is in toggling LEDs of green color. 
 * When the task gives the mutex and frees the common resource, the LED of blue color toggles its state.
 * So, when the livelock occurs, the LEDs of blue color are toggling continuously.
 *
 * @author Oleksandr Ushkarenko
 * @version 1.0 06/10/2021
 */

#include "stm32f3xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

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

/*
 * The size of the stack (in 4-byte words) for created tasks.
 */
#define STACK_SIZE 32U

/*
 * This identifier determines the time delay used in the tasks.
 */
#define DELAY 250U

/*
 * Declaration of the function prototypes.
 */
void GPIO_Init(void);
void task1_function(void *param);
void task2_function(void *param);
void error_handler(void);

/*
 * The variables are used to store task's handles.
 */
TaskHandle_t task1_handle;
TaskHandle_t task2_handle;

/*
 * The variable is used to store mutex handle.
 */
SemaphoreHandle_t mutex;

/*
 * The main function of the program (the entry point).
 * Demonstration of livelock occurrence. There are two tasks in the program and ona common resource
 * (green LEDs). Each task gets this resource and checks if the oher task is in the blocked state and
 * needs this resource. If it is so, the task frees the resource and waits for it. The conditions
 * when the task is able to perform some useful work are when it has taken the resource (mutex) and
 * the other task is not in the blocked state. When the states of the tasks change, the blue LEDs are blinking.
 * When the tasks perform useful work, the green LEDs are blinking.
 */
int main()
{
	GPIO_Init();

	BaseType_t result;
	result = xTaskCreate(task1_function, "Task 1", STACK_SIZE, NULL, 1, &task1_handle);
	if(result != pdPASS){
		error_handler();
	}
	
	result = xTaskCreate(task2_function, "Task 2", STACK_SIZE, NULL, 1, &task2_handle);
	if(result != pdPASS){
		error_handler();
	}
	
	mutex = xSemaphoreCreateMutex();
	if(mutex == NULL){
		error_handler();
	}

	vTaskStartScheduler();
	while(1) {}
}

/*
 * The function is used to initialize I/O pins of port E (GPIOE). 
 * All microcontroller pins the LEDs connected to configured to output.
 * The push-pull mode is used, no pull-ups.
 */
void GPIO_Init()
{
	__HAL_RCC_GPIOE_CLK_ENABLE();

	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 
	| GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15, GPIO_PIN_RESET);

	GPIO_InitTypeDef gpio_init_struct;
	gpio_init_struct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |
												 GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
	gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
	gpio_init_struct.Pull = GPIO_NOPULL;
	gpio_init_struct.Speed = GPIO_SPEED_LOW;
	
	HAL_GPIO_Init(GPIOE, &gpio_init_struct);
}

/*
 * This is the first task function in which the state of the first green LED should be toggled.
 *
 * @param a value that is passed as the parameter to the created task.
 */
void task1_function(void *param)
{
	while(1){
		xSemaphoreTake(mutex, portMAX_DELAY);
		taskYIELD();
		if(eTaskGetState(task2_handle) == eBlocked){
			HAL_GPIO_TogglePin(GPIOE, BLUE_LED_1); // useless work
			xSemaphoreGive(mutex);
		}else{
			HAL_GPIO_TogglePin(GPIOE, GREEN_LED_1);	// useful work (here is an unreachable statement due to the livelock)
			xSemaphoreGive(mutex);
		}
		vTaskDelay(DELAY);
	}
}

/*
 * This is the second task function in which the state of the second green LED should be toggled.
 *
 * @param a value that is passed as the parameter to the created task.
 */
void task2_function(void *param)
{
	while(1){
		xSemaphoreTake(mutex, portMAX_DELAY);
		taskYIELD();
		if(eTaskGetState(task1_handle) == eBlocked){
			HAL_GPIO_TogglePin(GPIOE, BLUE_LED_2); // useless work
			xSemaphoreGive(mutex);
		}else{
			HAL_GPIO_TogglePin(GPIOE, GREEN_LED_2); // useful work (here is an unreachable statement due to the livelock)
			xSemaphoreGive(mutex);
		}
		vTaskDelay(DELAY);
	}
}

/*
 * The function is used as an error handler: if an error occures, this function
 * is invoked and two red LEDs on board will be switched on.
 */
void error_handler(void)
{
	HAL_GPIO_WritePin(GPIOE, RED_LED_1 | RED_LED_2, GPIO_PIN_SET);
	while(1){	}
}

