/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

const uint PIN_TRIGGER = 15;
const uint PIN_ECHO = 16;

volatile uint32_t before = 0;
volatile bool alarm_fired = false;

QueueHandle_t xQueueDistance;
QueueHandle_t xQueueTime;
SemaphoreHandle_t xSemaphoreTrigger;

void oled1_btn_led_init(void) {
    gpio_init(LED_1_OLED);
    gpio_set_dir(LED_1_OLED, GPIO_OUT);

    gpio_init(LED_2_OLED);
    gpio_set_dir(LED_2_OLED, GPIO_OUT);

    gpio_init(LED_3_OLED);
    gpio_set_dir(LED_3_OLED, GPIO_OUT);

    gpio_init(BTN_1_OLED);
    gpio_set_dir(BTN_1_OLED, GPIO_IN);
    gpio_pull_up(BTN_1_OLED);

    gpio_init(BTN_2_OLED);
    gpio_set_dir(BTN_2_OLED, GPIO_IN);
    gpio_pull_up(BTN_2_OLED);

    gpio_init(BTN_3_OLED);
    gpio_set_dir(BTN_3_OLED, GPIO_IN);
    gpio_pull_up(BTN_3_OLED);
}

void pin_callback(uint gpio, uint32_t events) {
    uint32_t time;

    if (events == 0x4) { // fall edge
        time = to_us_since_boot(get_absolute_time());
    } else if (events == 0x8){ // rise edge
        time = to_us_since_boot(get_absolute_time());
    }

    xQueueSendFromISR(xQueueTime, &time, 0);
}

void trigger_task(void *p) {
    gpio_init(PIN_TRIGGER);
    gpio_set_dir(PIN_TRIGGER, GPIO_OUT);

    while(true){
        gpio_put(PIN_TRIGGER, 1);
        vTaskDelay(10);
        gpio_put(PIN_TRIGGER, 0);
        vTaskDelay(2);

        xSemaphoreGive(xSemaphoreTrigger);
    }
}

void echo_task(void *p) {
    gpio_init(PIN_ECHO);
    gpio_set_dir(PIN_ECHO, GPIO_IN);
    gpio_set_irq_enabled_with_callback(PIN_ECHO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &pin_callback);
    int delay_echo = 200;

    while (true){
        uint32_t stop_time, start_time;

        if (xQueueReceive(xQueueTime, &start_time, pdMS_TO_TICKS(delay_echo)) && xQueueReceive(xQueueTime, &stop_time, pdMS_TO_TICKS(delay_echo))){
            double dist = ((stop_time - start_time)*0.0343)/2;
            xQueueSend(xQueueDistance, &dist, 0);
        } else{
            double dist = 300;
            xQueueSend(xQueueDistance, &dist, 0);
        }

    }
}

void oled_task(void *p) {
    ssd1306_init();
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);
    oled1_btn_led_init();
    int delay_oled = 10;

    while (true){
        double dist;

        if (xSemaphoreTake(xSemaphoreTrigger, pdMS_TO_TICKS(delay_oled))){
            if (xQueueReceive(xQueueDistance, &dist, pdMS_TO_TICKS(delay_oled))){   
                if (dist > 225){
                    gfx_clear_buffer(&disp);
                    gfx_draw_string(&disp, 0, 0, 1, "Dist: FALHA");
                    gfx_show(&disp);
                    vTaskDelay(pdMS_TO_TICKS(150));
                } else {
                    char msg[20];
                    int progresso = (int)(dist*128/225);

                    xQueueReceive(xQueueDistance, &dist, 0);
                    sprintf(msg, "Dist: %.2f cm", dist);
    
                    gfx_clear_buffer(&disp);
                    gfx_draw_string(&disp, 0, 0, 1, msg);
                    gfx_draw_line(&disp, 0, 27, progresso, 27);

                    gfx_show(&disp);
    
                    vTaskDelay(pdMS_TO_TICKS(150));
                }
            }
        }
    }
}

int main() {
    stdio_init_all();

    xQueueDistance = xQueueCreate(32, sizeof(double));
    xQueueTime = xQueueCreate(32, sizeof(uint32_t));
    xSemaphoreTrigger = xSemaphoreCreateBinary();

    xTaskCreate(trigger_task, "trigger", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "echo", 256, NULL, 1, NULL);
    xTaskCreate(oled_task, "oled", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
