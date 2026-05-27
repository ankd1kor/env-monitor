#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

//--------------------------------------------------
// Sensor data structure
//--------------------------------------------------
typedef struct {
    float temperature;
    float humidity;
    float pressure;
} SensorData_t;

//--------------------------------------------------
// Global handles
//--------------------------------------------------
QueueHandle_t     sensor_queue;
SemaphoreHandle_t i2c_mutex;

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
        // Take mutex before touching I2C bus
        // When real BME280 arrives — driver call goes here
        //--------------------------------------------------
        xSemaphoreTake(i2c_mutex, portMAX_DELAY);

            // Fake sensor values — replace with bme280_read() later
            data.temperature = 25.0f + (reading_count % 10) * 0.1f;
            data.humidity    = 60.0f + (reading_count % 5)  * 0.2f;
            data.pressure    = 1013.0f;

        // Always give mutex back
        xSemaphoreGive(i2c_mutex);

        reading_count++;

        //--------------------------------------------------
        // Send data to queue
        //--------------------------------------------------
        if(xQueueSend(sensor_queue, &data, portMAX_DELAY) == pdPASS)
        {
            printf("[Sensor] Sent Reading #%d → Temp: %.1f C | "
                   "Humidity: %.1f%% | Pressure: %.1f hPa\n",
                   reading_count,
                   data.temperature,
                   data.humidity,
                   data.pressure);
        }

        // Wait 500ms — releases CPU to other tasks
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

//--------------------------------------------------
// Display Task (Consumer)
// Also uses I2C bus — must take same mutex
//--------------------------------------------------
void display_task(void *pvParameters)
{
    SensorData_t received_data;

    printf("Display task started!\n");

    while(1)
    {
        //--------------------------------------------------
        // Wait forever until sensor data arrives
        //--------------------------------------------------
        if(xQueueReceive(sensor_queue,
                         &received_data,
                         portMAX_DELAY) == pdPASS)
        {
            //--------------------------------------------------
            // Take mutex before writing to OLED over I2C
            // When real OLED arrives — driver call goes here
            //--------------------------------------------------
            xSemaphoreTake(i2c_mutex, portMAX_DELAY);

                // Fake display output — replace with oled_write() later
                printf("[Display] Temp: %.1f C | "
                       "Humidity: %.1f%% | "
                       "Pressure: %.1f hPa\n",
                       received_data.temperature,
                       received_data.humidity,
                       received_data.pressure);

            // Always give mutex back
            xSemaphoreGive(i2c_mutex);
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
    // Create Queue — before creating any tasks
    //--------------------------------------------------
    sensor_queue = xQueueCreate(5, sizeof(SensorData_t));
    if(sensor_queue == NULL)
    {
        printf("Failed to create queue!\n");
        return;
    }
    printf("Queue created successfully.\n");

    //--------------------------------------------------
    // Create Mutex — before creating any tasks
    //--------------------------------------------------
    i2c_mutex = xSemaphoreCreateMutex();
    if(i2c_mutex == NULL)
    {
        printf("Failed to create mutex!\n");
        return;
    }
    printf("Mutex created successfully.\n");

    //--------------------------------------------------
    // Create Sensor Task — priority 5 (higher)
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
    // Create Display Task — priority 4 (lower)
    //--------------------------------------------------
    xTaskCreate(
        display_task,
        "display_task",
        4096,
        NULL,
        4,
        NULL
    );

    printf("All tasks created. Scheduler running.\n");
}