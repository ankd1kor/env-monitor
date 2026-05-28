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
// Global Handles
//--------------------------------------------------

// Separate queues for each consumer
QueueHandle_t display_queue;
QueueHandle_t mqtt_queue;

// Mutex for shared I2C bus
SemaphoreHandle_t i2c_mutex;

// Binary semaphore for WiFi ready event
SemaphoreHandle_t wifi_semaphore;

//--------------------------------------------------
// Sensor Task (Producer)
//--------------------------------------------------

void sensor_task(void *pvParameters)
{
    SensorData_t data;
    int reading_count = 0;

    printf("[Sensor] Task started\n");

    while(1)
    {
        //--------------------------------------------------
        // Protect I2C access
        //--------------------------------------------------

        xSemaphoreTake(i2c_mutex, portMAX_DELAY);

        // Fake sensor readings
        data.temperature = 25.0f + (reading_count % 10) * 0.1f;
        data.humidity    = 60.0f + (reading_count % 5)  * 0.2f;
        data.pressure    = 1013.0f;

        xSemaphoreGive(i2c_mutex);

        reading_count++;

        //--------------------------------------------------
        // Send data to BOTH queues
        //--------------------------------------------------

        xQueueSend(display_queue, &data, portMAX_DELAY);
        xQueueSend(mqtt_queue, &data, portMAX_DELAY);

        printf("[Sensor] Reading #%d sent to queues\n",
               reading_count);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

//--------------------------------------------------
// Display Task
//--------------------------------------------------

void display_task(void *pvParameters)
{
    SensorData_t received_data;

    printf("[Display] Task started\n");

    while(1)
    {
        if(xQueueReceive(display_queue,
                         &received_data,
                         portMAX_DELAY) == pdPASS)
        {
            //--------------------------------------------------
            // Protect OLED I2C access
            //--------------------------------------------------

            xSemaphoreTake(i2c_mutex, portMAX_DELAY);

            printf("[Display] Temp: %.1f C | "
                   "Humidity: %.1f%% | "
                   "Pressure: %.1f hPa\n",
                   received_data.temperature,
                   received_data.humidity,
                   received_data.pressure);

            xSemaphoreGive(i2c_mutex);
        }
    }
}

//--------------------------------------------------
// WiFi Task
//--------------------------------------------------

void wifi_task(void *pvParameters)
{
    printf("[WiFi] Connecting to WiFi...\n");

    //--------------------------------------------------
    // Simulate WiFi connection delay
    //--------------------------------------------------

    vTaskDelay(pdMS_TO_TICKS(3000));

    printf("[WiFi] Connected!\n");

    //--------------------------------------------------
    // Signal MQTT task
    //--------------------------------------------------

    xSemaphoreGive(wifi_semaphore);

    while(1)
    {
        //--------------------------------------------------
        // Future:
        // monitor WiFi health
        // reconnect if disconnected
        //--------------------------------------------------

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

//--------------------------------------------------
// MQTT Task
//--------------------------------------------------

void mqtt_task(void *pvParameters)
{
    SensorData_t received_data;

    printf("[MQTT] Waiting for WiFi...\n");

    //--------------------------------------------------
    // Wait until WiFi task signals connection
    //--------------------------------------------------

    xSemaphoreTake(wifi_semaphore, portMAX_DELAY);

    printf("[MQTT] WiFi ready! Starting publishing...\n");

    while(1)
    {
        if(xQueueReceive(mqtt_queue,
                         &received_data,
                         portMAX_DELAY) == pdPASS)
        {
            printf("[MQTT] Publishing -> "
                   "Temp: %.1f C | "
                   "Humidity: %.1f%% | "
                   "Pressure: %.1f hPa\n",
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
    printf("Environmental Monitor Starting...\n");

    //--------------------------------------------------
    // Create Queues
    //--------------------------------------------------

    display_queue = xQueueCreate(5, sizeof(SensorData_t));
    mqtt_queue    = xQueueCreate(5, sizeof(SensorData_t));

    if(display_queue == NULL || mqtt_queue == NULL)
    {
        printf("Failed to create queues!\n");
        return;
    }

    printf("Queues created successfully\n");

    //--------------------------------------------------
    // Create Mutex
    //--------------------------------------------------

    i2c_mutex = xSemaphoreCreateMutex();

    if(i2c_mutex == NULL)
    {
        printf("Failed to create mutex!\n");
        return;
    }

    printf("Mutex created successfully\n");

    //--------------------------------------------------
    // Create Binary Semaphore
    //--------------------------------------------------

    wifi_semaphore = xSemaphoreCreateBinary();

    if(wifi_semaphore == NULL)
    {
        printf("Failed to create WiFi semaphore!\n");
        return;
    }

    printf("WiFi semaphore created successfully\n");

    //--------------------------------------------------
    // Create Tasks
    //--------------------------------------------------

    xTaskCreate(sensor_task,
                "sensor_task",
                4096,
                NULL,
                5,
                NULL);

    xTaskCreate(display_task,
                "display_task",
                4096,
                NULL,
                4,
                NULL);

    xTaskCreate(wifi_task,
                "wifi_task",
                4096,
                NULL,
                3,
                NULL);

    xTaskCreate(mqtt_task,
                "mqtt_task",
                4096,
                NULL,
                3,
                NULL);

    printf("All tasks created. Scheduler running.\n");
}