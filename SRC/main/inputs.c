/*
  inputs.c

  Copyright (C) 2022  Joachim Franken

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "inputs.h"
#include "jf-ecu32.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/rmt_rx.h"
#include "esp_log.h"
#include "esp_err.h"
#include <max31855.h>

static const char *TAG = "INPUTS";



// Timer
gptimer_handle_t gptimer = NULL;
gptimer_config_t timer_config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
};

// RPM
QueueHandle_t rpm_evt_queue = NULL;
QueueHandle_t receive_queue = NULL ;
QueueHandle_t receive_queue_aux = NULL ;

rmt_symbol_word_t raw_symbols[64]; // 
rmt_symbol_word_t aux_raw_symbols[64]; // 
rmt_receive_config_t receive_config ;
rmt_channel_handle_t rx_ppm_chan = NULL;
rmt_channel_handle_t rx_ppm_aux_chan = NULL;


static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    static unsigned long long time ;
    static unsigned long long time_prec ;
    static unsigned long long period ;
    //BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    unsigned long rpm ;

    uint32_t gpio_num = (uint32_t) arg;
    gptimer_get_raw_count(gptimer, &time) ;
    period = time - time_prec ;
    time_prec = time ;
    if(period > 0)
    {
        rpm = 600000 / (period/10) ;
        turbine.RPM = (rpm + (3*turbine.RPM))/4 ; //filtre
    }
    //xQueueOverwriteFromISR(rpm_evt_queue,&period,&xHigherPriorityTaskWoken);
}

BaseType_t high_task_wakeup = pdTRUE;
static bool IRAM_ATTR ppm_rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    QueueHandle_t receive_queue2 = (QueueHandle_t)user_data;
    // send the received RMT symbols to the parser task
    xQueueOverwriteFromISR(receive_queue2, edata, &high_task_wakeup);
    // return whether any task is woken up
    return high_task_wakeup == pdTRUE;
}

static bool IRAM_ATTR ppm_rmt_aux_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    QueueHandle_t receive_queue3 = (QueueHandle_t)user_data;
    // send the received RMT symbols to the parser task
    xQueueOverwriteFromISR(receive_queue3, edata, &high_task_wakeup);
    // return whether any task is woken up
    return high_task_wakeup == pdTRUE;
}

static void task_egt(void *pvParameter)
{
    max31855_t dev = { 0 };
    // Configure SPI bus
    spi_bus_config_t cfg = {
       .mosi_io_num = -1,
       .miso_io_num = MISO_GPIO_NUM,
       .sclk_io_num = CLK_GPIO_NUM,
       .quadwp_io_num = -1,
       .quadhd_io_num = -1,
       .max_transfer_sz = 0,
       .flags = 0
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &cfg, 1));

    // Init device
    ESP_ERROR_CHECK(max31855_init_desc(&dev, HOST, MAX31855_MAX_CLOCK_SPEED_HZ, CS_GPIO_NUM));

    float tc_t, cj_t;
    bool scv, scg, oc;
    while (1)
    {
        esp_err_t res = max31855_get_temperature(&dev, &tc_t, &cj_t, &scv, &scg, &oc);
        if (res != ESP_OK)
            ESP_LOGE(TAG, "Failed to measure: %d (%s)", res, esp_err_to_name(res));
        else
        {
            if (scv) ESP_LOGW(TAG, "Thermocouple shorted to VCC!");
            if (scg) ESP_LOGW(TAG, "Thermocouple shorted to GND!");
            if (oc) ESP_LOGW(TAG, "No connection to thermocouple!");
            //ESP_LOGI(TAG, "Temperature: %.2f°C, cold junction temperature: %.4f°C", tc_t, cj_t);
            //ESP_LOGI("wifi", "free Heap:%d,%d", esp_get_free_heap_size(), heap_caps_get_free_size(MALLOC_CAP_8BIT));
            turbine.EGT = tc_t ;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void init_inputs(void) 
{
    ESP_LOGI("RPM","Init timer 1Mhz");
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    ESP_LOGI("RPM","Init RPM");
    rpm_evt_queue = xQueueCreate(1, sizeof(unsigned long long));

    gpio_set_direction(RPM_PIN, GPIO_MODE_INPUT);
    gpio_pulldown_en(RPM_PIN);
    gpio_pullup_dis(RPM_PIN);
    gpio_set_intr_type(RPM_PIN, GPIO_INTR_POSEDGE);    

    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3|ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(RPM_PIN, gpio_isr_handler, (void *)RPM_PIN);
    gpio_intr_enable(RPM_PIN) ;

    ESP_LOGI("PPM","Init PPM");

    //voie des gaz

    rmt_rx_channel_config_t rmt_rx = {
        .clk_src = RMT_CLK_SRC_DEFAULT,       // select source clock
        .resolution_hz = 1 * 1000 * 1000, // 1MHz tick resolution, i.e. 1 tick = 1us
        .mem_block_symbols = 64,          // memory block size, 64 * 4 = 256Bytes
        .gpio_num = RMT_RX_GPIO_NUM,                    // GPIO number
        .flags.invert_in = false,         // don't invert input signal
        .flags.with_dma = false,          // don't need DMA backend
    };

    ESP_ERROR_CHECK(rmt_new_rx_channel(&rmt_rx, &rx_ppm_chan));
    ESP_LOGI(TAG, "PPM RX initialized\n");
    //Voie aux
    rmt_rx.gpio_num = RMT_AUX_GPIO_NUM ;
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rmt_rx, &rx_ppm_aux_chan));

    ESP_LOGI(TAG, "PPM AUX RX initialized\n");

    receive_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    receive_queue_aux = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));

    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = ppm_rmt_rx_done_callback,
    };
    rmt_rx_event_callbacks_t cbs_aux = {
        .on_recv_done = ppm_rmt_aux_done_callback,
    };

    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_ppm_chan, &cbs, receive_queue));
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_ppm_aux_chan, &cbs_aux, receive_queue_aux));

    // the following timing requirement is based on NEC protocol
    receive_config.signal_range_min_ns = 700000;     // the shortest duration for NEC signal is 560us, 1250ns < 560us, valid signal won't be treated as noise
    receive_config.signal_range_max_ns = 10000000; // the longest duration for NEC signal is 9000us, 12000000ns > 9000us, the receive won't stop early
    

    // ready to receive
    ESP_ERROR_CHECK(rmt_enable(rx_ppm_chan)) ;
    ESP_ERROR_CHECK(rmt_receive(rx_ppm_chan, raw_symbols, sizeof(raw_symbols), &receive_config));
    ESP_ERROR_CHECK(rmt_enable(rx_ppm_aux_chan)) ;
    ESP_ERROR_CHECK(rmt_receive(rx_ppm_aux_chan, aux_raw_symbols, sizeof(aux_raw_symbols), &receive_config));
    
    ESP_LOGI(TAG, "MAX31855 initialized\n");
    xTaskCreate(task_egt, TAG, configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL);
}

