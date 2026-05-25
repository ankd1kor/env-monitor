#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// This struct holds one sensor reading
// Later this will carry real BME280 data
typedef struct {
    float temperature;
    float humidity;
    float pressure;
} SensorData_t;

// Your first FreeRTOS task
// This is the heart of your environmental monitor
void sensor_task(void *pvParameters)
{
    SensorData_t data;
    int reading_count = 0;

    printf("Sensor task started!\n");

    while(1) {
        // RIGHT NOW: fake values
        // LATER: replace these 3 lines with real BME280 driver calls
        data.temperature = 25.0f + (reading_count % 10) * 0.1f;
        data.humidity    = 60.0f + (reading_count % 5)  * 0.2f;
        data.pressure    = 1013.0f;

        reading_count++;

        // Print the reading
        printf("[Sensor] Reading #%d -> Temp: %.1f C  Humidity: %.1f%%  Pressure: %.1f hPa\n",
               reading_count,
               data.temperature,
               data.humidity,
               data.pressure);

        // Sleep for 500ms — releases CPU to other tasks
        // pdMS_TO_TICKS converts milliseconds to FreeRTOS ticks
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    // We never reach here
    // A FreeRTOS task never returns
}

void app_main(void)
{
    printf("Environmental Monitor starting...\n");

    // Create the sensor task
    // This registers it with the FreeRTOS scheduler
    xTaskCreate(
        sensor_task,        // function to run
        "sensor_task",      // name (for debugging)
        4096,               // stack size in bytes
        NULL,               // parameters to pass (none yet)
        5,                  // priority (higher = more important)
        NULL                // task handle (we don't need it yet)
    );

    printf("Sensor task created. Scheduler is running.\n");
}