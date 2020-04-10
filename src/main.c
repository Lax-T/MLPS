/**
  ******************************************************************************
  * @file    main.c
  * @author  Ac6
  * @version V1.0
  * @date    01-December-2013
  * @brief   Default main function.
  ******************************************************************************
*/
#include "main.h"
#include "init.h"
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "tasks/display_control.h"
#include "tasks/system.h"
#include "tasks/ui_control.h"
#include "globals.h"

QueueHandle_t xQueueUIEvent;
QueueHandle_t xQueueSysEvent;
QueueHandle_t xQueueButtonEvent;
TaskHandle_t UIControlTaskHandle;

int main(void)
{
	init();
	UIControlTaskHandle = NULL;
	xQueueButtonEvent = xQueueCreate(5, sizeof( uint8_t ));
	xQueueUIEvent = xQueueCreate(5, sizeof( struct UIEventMessage ));
	xQueueSysEvent = xQueueCreate(4, sizeof( struct SystemEventMessage ));

	xTaskCreate(dc_taskDisplayControl, "Display ctrl", 64, NULL, 4, NULL);
	xTaskCreate(sys_taskSystemControl, "System", 128, NULL, 1, NULL);
	xTaskCreate(uic_taskUIControl, "UI handle", 64, NULL, 3, &UIControlTaskHandle);
	vTaskStartScheduler();

	while(1);
}
