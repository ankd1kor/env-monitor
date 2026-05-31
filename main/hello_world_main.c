#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

//--------------------------------------------------
// Event Group Bits
//--------------------------------------------------
#define WIFI_READY_BIT      BIT0
#define SENSOR_READY_BIT    BIT1
#define DISPLAY_READY_BIT   BIT2

//--------------------------------------------------
// Sensor Data Structure
//--------------------------------------------------
typedef struct {
    float temperature;
    float humidity;
    float pressure;
} SensorData_t;

//--------------------------------------------------
// Watchdog Flags
//--------------------------------------------------
typedef struct {
    bool sensor_task_alive;
    bool display_task_alive;
    bool wifi_task_alive;
    bool mqtt_task_alive;
} WatchdogFlags_t;

WatchdogFlags_t wd_flags = {false, false, false, false};

//--------------------------------------------------
// Global Handles
//--------------------------------------------------
QueueHandle_t      sensor_queue_display;   // sensor → display
QueueHandle_t      sensor_queue_mqtt;      // sensor → mqtt
SemaphoreHandle_t  i2c_mutex;
SemaphoreHandle_t  wd_mutex;
SemaphoreHandle_t  publish_semaphore;
EventGroupHandle_t system_event_group;
TimerHandle_t      mqtt_publish_timer;

//--------------------------------------------------
// Timer Callback
// Called every 10 seconds — signals mqtt_task to publish
//--------------------------------------------------
void mqtt_timer_callback(TimerHandle_t xTimer)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(publish_semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

//--------------------------------------------------
// WiFi Task
//--------------------------------------------------
void wifi_task(void *pvParameters)
{
    printf("[WiFi] Connecting...\n");

    // Simulate WiFi connection delay
    vTaskDelay(pdMS_TO_TICKS(3000));

    printf("[WiFi] Connected.\n");

    // Signal system WiFi is ready
    xEventGroupSetBits(system_event_group, WIFI_READY_BIT);
    printf("[WiFi] WIFI_READY_BIT set.\n");

    // Keep running — monitor WiFi health
    while(1)
    {
        // Check in with watchdog
        xSemaphoreTake(wd_mutex, portMAX_DELAY);
            wd_flags.wifi_task_alive = true;
        xSemaphoreGive(wd_mutex);

        // Later: check WiFi, reconnect if dropped
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
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
        // Take I2C mutex before reading sensor
        xSemaphoreTake(i2c_mutex, portMAX_DELAY);
            data.temperature = 25.0f + (reading_count % 10) * 0.1f;
            data.humidity    = 60.0f + (reading_count % 5)  * 0.2f;
            data.pressure    = 1013.0f;
        xSemaphoreGive(i2c_mutex);

        reading_count++;

        // Signal sensor ready after first read
        if(!sensor_ready_signalled) {
            xEventGroupSetBits(system_event_group, SENSOR_READY_BIT);
            sensor_ready_signalled = true;
            printf("[Sensor] SENSOR_READY_BIT set.\n");
        }

        // Send to BOTH queues
        xQueueSend(sensor_queue_display, &data, portMAX_DELAY);
        xQueueSend(sensor_queue_mqtt,    &data, portMAX_DELAY);

        printf("[Sensor] Reading #%d sent.\n", reading_count);

        // Check in with watchdog
        xSemaphoreTake(wd_mutex, portMAX_DELAY);
            wd_flags.sensor_task_alive = true;
        xSemaphoreGive(wd_mutex);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//--------------------------------------------------
// Display Task
//--------------------------------------------------
void display_task(void *pvParameters)
{
    SensorData_t data;

    printf("[Display] Task started.\n");

    while(1)
    {
        // Wait for sensor data
        if(xQueueReceive(sensor_queue_display,
                         &data,
                         portMAX_DELAY) == pdPASS)
        {
            // Take I2C mutex before writing to OLED
            xSemaphoreTake(i2c_mute