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

alarm_id_t alarm;
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

void oled1_demo_1(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    char cnt = 15;
    while (1) {

        if (gpio_get(BTN_1_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_1_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 1 - ON");
            gfx_show(&disp);
        } else if (gpio_get(BTN_2_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_2_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 2 - ON");
            gfx_show(&disp);
        } else if (gpio_get(BTN_3_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_3_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 3 - ON");
            gfx_show(&disp);
        } else {

            gpio_put(LED_1_OLED, 1);
            gpio_put(LED_2_OLED, 1);
            gpio_put(LED_3_OLED, 1);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "PRESSIONE ALGUM");
            gfx_draw_string(&disp, 0, 10, 1, "BOTAO");
            gfx_draw_line(&disp, 15, 27, cnt,
                          27);
            vTaskDelay(pdMS_TO_TICKS(50));
            if (++cnt == 112)
                cnt = 15;

            gfx_show(&disp);
        }
    }
}

void oled1_demo_2(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    char cnt = 15;
    while (1) {

        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 1, "Mandioca");
        gfx_show(&disp);
        vTaskDelay(pdMS_TO_TICKS(150));

        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 2, "Batata");
        gfx_show(&disp);
        vTaskDelay(pdMS_TO_TICKS(150));

        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 4, "Inhame");
        gfx_show(&disp);
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    alarm_fired = true;
    return 0;
}

void pin_callback(uint gpio, uint32_t events) {
    if (events == 0x4) { // stop
        uint32_t time = to_us_since_boot(get_absolute_time());
        xQueueSendFromISR(xQueueTime, &time, 0);
    } else if (events == 0x8){
        uint32_t time = to_us_since_boot(get_absolute_time());
        xQueueSendFromISR(xQueueTime, &time, 0);
    }
}

void trigger_task(void *p) {
    gpio_init(PIN_TRIGGER);
    gpio_set_dir(PIN_TRIGGER, GPIO_OUT);

    while(true){
        gpio_put(PIN_TRIGGER, 1);
        sleep_us(10);
        gpio_put(PIN_TRIGGER, 0);
        sleep_us(2);

        alarm = add_alarm_in_ms(500, alarm_callback, NULL, false);  
    }
}

void echo_task(void *p) {
    gpio_init(PIN_ECHO);
    gpio_set_dir(PIN_ECHO, GPIO_IN);
    gpio_set_irq_enabled_with_callback(PIN_ECHO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &pin_callback);
    xSemaphoreGive(xSemaphoreTrigger);

    while (true){
        uint32_t now, before;

        if (xQueueReceive(xQueueTime, &now, 0) == pdTRUE && xQueueReceive(xQueueTime, &before, 0) == pdTRUE){
            double dist = ((now - before)*0.0343)/2;
            xQueueSend(xQueueDistance, &dist, 0);
            cancel_alarm(alarm);
        }
    }

    vTaskDelay(100);
}

void oled_task(void *p) {
    ssd1306_init();
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);
    oled1_btn_led_init();

    while (true){
        double dist;
        
        if (xQueueReceive(xQueueDistance, &dist, 0) == pdTRUE){    
            if (alarm_fired){
                gfx_clear_buffer(&disp);
                gfx_draw_string(&disp, 0, 0, 1, "Dist: FALHA");
                gfx_show(&disp);
                vTaskDelay(pdMS_TO_TICKS(150));
            } else {
                char msg[20];

                xQueueReceive(xQueueDistance, &dist, 0);
                sprintf(msg, "Dist: %.2f", dist);

                gfx_clear_buffer(&disp);
                gfx_draw_string(&disp, 0, 0, 1, msg);
                gfx_show(&disp);

                vTaskDelay(pdMS_TO_TICKS(150));
            }
        
            xSemaphoreGive(xSemaphoreTrigger);
        }
    }
}

int main() {
    stdio_init_all();

    xQueueDistance = xQueueCreate(32, sizeof(int));
    xQueueTime = xQueueCreate(32, sizeof(int));
    xSemaphoreTrigger = xSemaphoreCreateBinary();

    xTaskCreate(trigger_task, "trigger", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "echo", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "oled", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
