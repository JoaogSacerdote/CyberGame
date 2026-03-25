#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

void app_main() {
  printf("Hello, Wokwi!\n");
  while (true) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
