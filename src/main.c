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

	xTaskCreate(TaskDisplayControl, "Display ctrl", 64, NULL, 4, NULL);
	xTaskCreate(TaskSystemControl, "System", 128, NULL, 1, NULL);
	xTaskCreate(TaskUIControl, "UI handle", 64, NULL, 3, &UIControlTaskHandle);
	//xTaskCreate(TaskADCReadout, "ADC readout", 64, NULL, 4, NULL);
	vTaskStartScheduler();

	while (1) {
	}
}
