/*
 * task2, main.c
 * Purpose: create program with using queue.
 * 
 * There are two tasks in the program. The first task generates two random numbers. The first number is used
 * to choose LED which state will be toggled. The second random number is used to make a delay before next
 * chande of the LEDs state. A queue is used to pass the data betwen these tasks.
 *
 * @author Oleksandr Ushkarenko
 * @version 1.0 04/10/2021
 */

#include "stm32f3xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "stdlib.h"
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
 * The size of the stack (in 4-byte words) for created static tasks.
 */
#define STACK_SIZE 32U

/*
 * These identifiers are used as the lower and highest values of time delay.  
 */
#define MIN_DELAY 200U
#define MAX_DELAY 1000U

/*
 * The number of LEDs on the board.
 */
#define LEDS_NUM 8

#define QUEUE_LENGTH 8

/*
 * This data structure is used to transfer data between two tasks.
 */
struct led_controller_data{
	uint16_t led_pin;
	uint32_t delay;
};

/*
 * Declaration of the function prototypes.
 */
void GPIO_Init(void);
void led_controller_task(void *param);
void rand_num_generator_task(void *param);
void error_handler(void);

/*
 * These variables are used to store task's handles.
 */
TaskHandle_t led_controller_task_handle;
TaskHandle_t rand_num_generator_task_handle;

/*
 * Queue handle.
 */
QueueHandle_t queue_handle;

/*
 * The array contais pin numbers the LEDs connected to.
 */
uint16_t led_pins[LEDS_NUM] = {BLUE_LED_1, RED_LED_1, ORANGE_LED_1, GREEN_LED_1,
															 BLUE_LED_2, RED_LED_2, ORANGE_LED_2, GREEN_LED_2};

/*
 * The main function of the program (the entry point).
 * Demonstration of using queues for inter-task communications.
 */
int main()
{
	GPIO_Init();
	
	BaseType_t result;

	result = xTaskCreate(led_controller_task, "LED controller Task", STACK_SIZE, NULL, 1, &led_controller_task_handle);
	if(result != pdPASS){
		error_handler();
	}
	
	result = xTaskCreate(rand_num_generator_task, "Random number generator Task", STACK_SIZE, NULL, 1, &rand_num_generator_task_handle);
	if(result != pdPASS){
		error_handler();
	}
	
	queue_handle = xQueueCreate(QUEUE_LENGTH, sizeof(struct led_controller_data));
	if(queue_handle == NULL){
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
 * This is a task function in which the data received from the queue is used to toggle LEDs state
 * and make a time delay.
 *
 * @param a value that is passed as the parameter to the created task.
 */
void led_controller_task(void *param)
{
	struct led_controller_data data;
	BaseType_t result;
	while(1) {
		result = xQueueReceive(queue_handle, &data, portMAX_DELAY);
		if(result == pdTRUE){
			HAL_GPIO_TogglePin(GPIOE, data.led_pin);
			vTaskDelay(pdMS_TO_TICKS(data.delay));
		}
	}
}

/*
 * This is a task function in which two random numbers are generated, packed into a struct
 * instance and put to the queue. These numbers are used by the other task (led_controller_task)
 * to toggle LEDs state.
 *
 * @param a value that is passed as the parameter to the created task.
 */
void rand_num_generator_task(void *param)
{
	struct led_controller_data data;
	while(1) {
		data.led_pin = led_pins[rand() % LEDS_NUM]; // The range of randomly generated is [0, LEDS_NUM - 1]; 
		data.delay = MIN_DELAY + rand() % (MAX_DELAY - MIN_DELAY + 1); // The range of randomly generated delay [MIN_DELAY, MAX_DELAY];
		xQueueSend(queue_handle, &data, portMAX_DELAY);
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

