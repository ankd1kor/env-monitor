#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

//--------------------------------------------------
// Sensor data structure
//--------------------------------------------------

typedef struct {
    float temperature;
    float humidity;
    float pressure;
} SensorData_t;

//--------------------------------------------------
// Global Queue Handle
//--------------------------------------------------

QueueHandle_t sensor_queue;

//--------------------------------------------------
// Sensor Task (Producer)
//--------------------------------------------------

void sensor_task(void *pvParameters)
{
    SensorData_t data;
    int reading_count = 0;

    printf("Sensor task started!\n");

    while(1)
    {
        //--------------------------------------------------
        // Simulate sensor readings
        //--------------------------------------------------

        data.temperature = 25.0f + (reading_count % 10) * 0.1f;
        data.humidity    = 60.0f + (reading_count % 5)  * 0.2f;
        data.pressure    = 1013.0f;

        reading_count++;

        //--------------------------------------------------
        // Send data to queue
        //--------------------------------------------------

        if(xQueueSend(sensor_queue, &data, portMAX_DELAY) == pdPASS)
        {
            printf("[Sensor] Sent Reading #%d to queue\n", reading_count);
        }

        //--------------------------------------------------
        // Wait 500ms
        //--------------------------------------------------

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

//--------------------------------------------------
// Display Task (Consumer)
//--------------------------------------------------

void display_task(void *pvParameters)
{
    SensorData_t received_data;

    printf("Display task started!\n");

    while(1)
    {
        //--------------------------------------------------
        // Wait forever until data arrives
        //--------------------------------------------------

        if(xQueueReceive(sensor_queue,
                         &received_data,
                         portMAX_DELAY) == pdPASS)
        {
            printf("[Display] Temp: %.1f C | Humidity: %.1f%% | Pressure: %.1f hPa\n",
                   received_data.temperature,
                   received_data.humidity,
                   received_data.pressure);
        }
    }
}

//--------------------------------------------------
// Main Application
//--------------------------------------------------

void app_main(void)
{
    printf("Environmental Monitor starting...\n");

    //--------------------------------------------------
    // Create Queue
    //--------------------------------------------------

    sensor_queue = xQueueCreate(
                        5,                      // queue length
                        sizeof(SensorData_t)); // item size

    if(sensor_queue == NULL)
    {
        printf("Failed to create queue!\n");
        return;
    }

    printf("Queue created successfully.\n");

    //--------------------------------------------------
    // Create Sensor Task
    //--------------------------------------------------

    xTaskCreate(
        sensor_task,
        "sensor_task",
        4096,
        NULL,
        5,
        NULL
    );

    //--------------------------------------------------
    // Create Display Task
    //--------------------------------------------------

    xTaskCreate(
        display_task,
        "display_task",
        4096,
        NULL,
        4,
        NULL
    );

    printf("Tasks created. Scheduler running.\n");
}