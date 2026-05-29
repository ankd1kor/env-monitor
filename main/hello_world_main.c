#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

//--------------------------------------------------
// Event Group Bits
//--------------------------------------------------
#define WIFI_READY_BIT      BIT0
#define SENSOR_READY_BIT    BIT1
#define DISPLAY_READY_BIT   BIT2

//--------------------------------------------------
// Sensor Data Structure
//--------------------------------------------------
typedef struct
{
    float temperature;
    float humidity;
    float pressure;
} SensorData_t;

//--------------------------------------------------
// Global Handles
//--------------------------------------------------
QueueHandle_t sensor_queue;
SemaphoreHandle_t i2c_mutex;
EventGroupHandle_t system_event_group;

//--------------------------------------------------
// WiFi Task
//--------------------------------------------------
void wifi_task(void *pvParameters)
{
    printf("[WiFi] Connecting...\n");

    // Simulate WiFi connection delay
    vTaskDelay(pdMS_TO_TICKS(5000));

    printf("[WiFi] Connected.\n");

    xEventGroupSetBits(
        system_event_group,
        WIFI_READY_BIT
    );

    printf("[WiFi] WIFI_READY_BIT set.\n");

    vTaskDelete(NULL);
}

//--------------------------------------------------
// Sensor Task
//--------------------------------------------------
void sensor_task(void *pvParameters)
{
    SensorData_t data;
    int reading_count = 0;
    bool sensor_ready_signalled = false;

    printf("[Sensor] Task started.\n");

    while(1)
    {
        //--------------------------------------------------
        // Take I2C mutex
        //--------------------------------------------------
        xSemaphoreTake(i2c_mutex, portMAX_DELAY);

        data.temperature = 25.0f + (reading_count % 10) * 0.1f;
        data.humidity    = 60.0f + (reading_count % 5) * 0.2f;
        data.pressure    = 1013.0f;

        xSemaphoreGive(i2c_mutex);

        reading_count++;

        //--------------------------------------------------
        // Signal sensor ready only once
        //--------------------------------------------------
        if(!sensor_ready_signalled)
        {
            xEventGroupSetBits(
                system_event_group,
                SENSOR_READY_BIT
            );

            sensor_ready_signalled = true;

            printf("[Sensor] SENSOR_READY_BIT set.\n");
        }

        //--------------------------------------------------
        // Send data to queue
        //--------------------------------------------------
        if(xQueueSend(sensor_queue,
                      &data,
                      portMAX_DELAY) == pdPASS)
        {
            printf("[Sensor] Reading #%d sent.\n",
                   reading_count);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//--------------------------------------------------
// MQTT Task
//--------------------------------------------------
void mqtt_task(void *pvParameters)
{
    SensorData_t data;

    printf("[MQTT] Waiting for WiFi AND Sensor...\n");

    //--------------------------------------------------
    // Wait for BOTH bits
    //--------------------------------------------------
    xEventGroupWaitBits(
        system_event_group,
        WIFI_READY_BIT | SENSOR_READY_BIT,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY
    );

    printf("[MQTT] System Ready.\n");
    printf("[MQTT] Publishing started.\n");

    while(1)
    {
        if(xQueueReceive(sensor_queue,
                         &data,
                         portMAX_DELAY) == pdPASS)
        {
            printf("[MQTT] Publish => "
                   "Temp: %.1f C | "
                   "Humidity: %.1f%% | "
                   "Pressure: %.1f hPa\n",
                   data.temperature,
                   data.humidity,
                   data.pressure);
        }
    }
}

//--------------------------------------------------
// Main Application
//--------------------------------------------------
void app_main(void)
{
    printf("System Starting...\n");

    //--------------------------------------------------
    // Create Queue
    //--------------------------------------------------
    sensor_queue = xQueueCreate(
        5,
        sizeof(SensorData_t)
    );

    if(sensor_queue == NULL)
    {
        printf("Queue creation failed!\n");
        return;
    }

    //--------------------------------------------------
    // Create Mutex
    //--------------------------------------------------
    i2c_mutex = xSemaphoreCreateMutex();

    if(i2c_mutex == NULL)
    {
        printf("Mutex creation failed!\n");
        return;
    }

    //--------------------------------------------------
    // Create Event Group
    //--------------------------------------------------
    system_event_group = xEventGroupCreate();

    if(system_event_group == NULL)
    {
        printf("Event Group creation failed!\n");
        return;
    }

    printf("Queue Created.\n");
    printf("Mutex Created.\n");
    printf("Event Group Created.\n");

    //--------------------------------------------------
    // Create Tasks
    //--------------------------------------------------
    xTaskCreate(
        wifi_task,
        "wifi_task",
        4096,
        NULL,
        5,
        NULL
    );

    xTaskCreate(
        sensor_task,
        "sensor_task",
        4096,
        NULL,
        5,
        NULL
    );

    xTaskCreate(
        mqtt_task,
        "mqtt_task",
        4096,
        NULL,
        4,
        NULL
    );

    printf("All tasks started.\n");
}