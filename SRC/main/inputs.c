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
#include "error.h"
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
#include "freertos/semphr.h"
#include <ina219.h>
#include "ds18b20.h"
#include "onewire_bus_impl_rmt.h"
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

SemaphoreHandle_t SEM_glow_current = NULL ;
SemaphoreHandle_t SEM_EGT = NULL ;

rmt_symbol_word_t raw_symbols[64]; // 
rmt_symbol_word_t aux_raw_symbols[64]; // 
rmt_receive_config_t receive_config ;
rmt_channel_handle_t rx_ppm_chan = NULL;
rmt_channel_handle_t rx_ppm_aux_chan = NULL;

TaskHandle_t task_egt_h ;
TaskHandle_t task_glow_current_h ;
//Courant bougie
ina219_t dev;



void init_ds18b20(void)
{
    // install 1-wire bus
    turbine.bus_config.bus_gpio_num = ECU_ONEWIRE_BUS_GPIO ;
    turbine.bus = NULL;
    turbine.rmt_config.max_rx_bytes = 10 ; // 1byte ROM command + 8byte ROM number + 1byte device command
    turbine.iter = NULL ;
    esp_err_t search_result = ESP_OK ;
    turbine.ds18b20_device_num = 0 ;

    ESP_ERROR_CHECK(onewire_new_bus_rmt(&turbine.bus_config, &turbine.rmt_config, &turbine.bus));
    
    // create 1-wire device iterator, which is used for device search
    ESP_ERROR_CHECK(onewire_new_device_iter(turbine.bus, &turbine.iter));
    ESP_LOGI(TAG, "Device iterator created, start searching...");
    do {
        search_result = onewire_device_iter_get_next(turbine.iter, &turbine.next_onewire_device);
        if (search_result == ESP_OK) { // found a new device, let's check if we can upgrade it to a DS18B20
            ds18b20_config_t ds_cfg = {};
            // check if the device is a DS18B20, if so, return the ds18b20 handle
            if (ds18b20_new_device(&turbine.next_onewire_device, &ds_cfg, &turbine.ds18b20s[turbine.ds18b20_device_num]) == ESP_OK) {
                ESP_LOGI(TAG, "Found a DS18B20[%d], address: %016llX", turbine.ds18b20_device_num, turbine.next_onewire_device.address);
                turbine.ds18b20_device_num++;
            } else {
                ESP_LOGI(TAG, "Found an unknown device, address: %016llX", turbine.next_onewire_device.address);
            }
        }
    } while (search_result != ESP_ERR_NOT_FOUND);
    ESP_ERROR_CHECK(onewire_del_device_iter(turbine.iter));
    ESP_LOGI(TAG, "Searching done, %d DS18B20 device(s) found", turbine.ds18b20_device_num);

    // Now you have the DS18B20 sensor handle, you can use it to read the temperature
}

/*********** ISR pour les RPM ******************/
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    //uint32_t gpio_num = (uint32_t) arg;
    static uint64_t period,period_tmp ;
    static uint32_t rpm_temp ;
    //static BaseType_t xHigherPriorityTaskWoken;
    gptimer_get_raw_count(gptimer, &period) ;
    if(period > 200)
    {
            gptimer_set_raw_count(gptimer, 0) ;
            /*if(xSemaphoreTakeFromISR(xRPMmutex,&xHigherPriorityTaskWoken ) == pdTRUE)
            {*/
                //turbine.RPM_period = period ; 
            period_tmp = (period + (3*turbine.RPM_period))/4 ; //filtre
            turbine.RPM_period  = period_tmp ;

            if(period > 0 )
                rpm_temp = 60000000 / period ;
            else
                rpm_temp = 0 ;
            turbine.RPM = rpm_temp ;
                //xSemaphoreGiveFromISR(xRPMmutex,&xHigherPriorityTaskWoken) ;
            /*}
            turbine.WDT_RPM = 1 ;*/
            turbine.WDT_RPM = 1 ;
    }
    turbine.RPM_sec++ ;
}

BaseType_t high_task_wakeup = pdTRUE;
/*********** ISR pour la voie des gaz ******************/
static bool IRAM_ATTR ppm_rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    QueueHandle_t receive_queue2 = (QueueHandle_t)user_data;
    // send the received RMT symbols to the parser task
    xQueueOverwriteFromISR(receive_queue2, edata, &high_task_wakeup);
    // return whether any task is woken up
    return high_task_wakeup == pdTRUE;
}

/*********** ISR pour la voie aux ******************/
static bool IRAM_ATTR ppm_rmt_aux_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    QueueHandle_t receive_queue3 = (QueueHandle_t)user_data;
    // send the received RMT symbols to the parser task
    xQueueOverwriteFromISR(receive_queue3, edata, &high_task_wakeup);
    // return whether any task is woken up
    return high_task_wakeup == pdTRUE;
}


/*********** Tache pour lecture du courant de la bougie période 100ms ******************/
void task_glow_current(void *pvParameter)
{
    memset(&dev, 0, sizeof(ina219_t));

    ESP_ERROR_CHECK(ina219_init_desc(&dev, I2C_ADDR, I2C_PORT, SDA_GPIO, SCL_GPIO));
    ESP_LOGI(TAG, "Initializing INA219");
    ESP_ERROR_CHECK(ina219_init(&dev));

    ESP_LOGI(TAG, "Configuring INA219");
    ESP_ERROR_CHECK(ina219_configure(&dev, INA219_BUS_RANGE_16V, INA219_GAIN_0_125,
            INA219_RES_12BIT_1S, INA219_RES_12BIT_1S, INA219_MODE_CONT_SHUNT_BUS));

    ESP_LOGI(TAG, "Calibrating INA219");

    ESP_ERROR_CHECK(ina219_calibrate(&dev, (float)10 / 1000.0f)); // Résistance de shunt 10mOhm
    while(1)
    {
        ESP_ERROR_CHECK(ina219_get_current(&dev, &turbine.GLOW_CURRENT)) ;
        xSemaphoreGive(SEM_glow_current) ;
        vTaskDelay(100 / portTICK_PERIOD_MS) ;
    }
}

void scan_i2c(void)
{
    i2c_dev_t dev = { 0 };
    dev.cfg.sda_io_num = SDA_GPIO ;
    dev.cfg.scl_io_num = SCL_GPIO ;
    dev.cfg.master.clk_speed = 100000; // 100kHz

    while (1)
    {
        esp_err_t res;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
        printf("00:         ");
        for (uint8_t addr = 3; addr < 0x78; addr++)
        {
            if (addr % 16 == 0)
                printf("\n%.2x:", addr);

            dev.addr = addr;
            res = i2c_dev_probe(&dev, I2C_DEV_WRITE);

            if (res == 0)
                printf(" %.2x", addr);
            else
                printf(" --");
        }
        printf("\n\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*********** Tache de lecture des EGT et DS18B20 période 200ms ******************/
void task_egt(void *pvParameter)
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
            /*if (scv) ESP_LOGW(TAG, "Thermocouple shorted to VCC!");
            if (scg) ESP_LOGW(TAG, "Thermocouple shorted to GND!");
            if (oc) ESP_LOGW(TAG, "No connection to thermocouple!");
            if (scv) add_error_msg(E_K,"K shorted to VCC!");
            if (scg) add_error_msg(E_K,"K shorted to GND!");
            if (oc) add_error_msg(E_K,"K not connected");
            //ESP_LOGI(TAG, "Temperature: %.2f°C, cold junction temperature: %.4f°C", tc_t, cj_t);
            //ESP_LOGI("wifi", "free Heap:%d,%d", esp_get_free_heap_size(), heap_caps_get_free_size(MALLOC_CAP_8BIT));*/
            turbine.EGT = (turbine.EGT + tc_t)/2 ;
            xSemaphoreGive(SEM_EGT) ;
        }
        if(turbine.ds18b20_device_num > 0)
            ds18b20_get_temperature(turbine.ds18b20s[0],&turbine.DS18B20_temp) ;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void init_inputs(void) 
{
    //** GPTIMER pour RPM
    ESP_LOGI("RPM","Init timer 1Mhz");
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_set_raw_count(gptimer,0));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    ESP_LOGI("RPM","Init RPM");
    //rpm_evt_queue = xQueueCreate(1, sizeof(unsigned long long));

    //** Interruption RPM
    gpio_set_direction(RPM_PIN, GPIO_MODE_INPUT);
    gpio_pullup_dis(RPM_PIN);
    gpio_pulldown_dis(RPM_PIN);
    //gpio_pullup_en(RPM_PIN);
    //gpio_pulldown_en(RPM_PIN);
    gpio_set_intr_type(RPM_PIN, GPIO_INTR_POSEDGE);    
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3|ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(RPM_PIN, gpio_isr_handler, (void *)RPM_PIN);
    gpio_intr_enable(RPM_PIN) ;

    ESP_LOGI("PPM","Init PPM");

    //voie des gaz

    rmt_rx_channel_config_t rmt_rx = {
        .clk_src = RMT_CLK_SRC_REF_TICK, //RMT_CLK_SRC_DEFAULT,       // select source clock
        .resolution_hz = 1 * 1000 * 1000,//1 * 1000 * 1000, // 1MHz tick resolution, i.e. 1 tick = 1us
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

    ESP_LOGI(TAG, "PPM AUX RX initializated\n");

    receive_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    receive_queue_aux = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));

    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = ppm_rmt_rx_done_callback,
    };
    rmt_rx_event_callbacks_t cbs_aux = {
        .on_recv_done = ppm_rmt_aux_done_callback,
    };

    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_ppm_chan, &cbs, receive_queue));
    ESP_LOGI(TAG, "RMT RX initialized\n");
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_ppm_aux_chan, &cbs_aux, receive_queue_aux));
    ESP_LOGI(TAG, "RMT AUX initialized\n");

    // the following timing requirement is based on NEC protocol
    receive_config.signal_range_min_ns = 0; //100 * 1000;    // 700ms
    receive_config.signal_range_max_ns = 3000 * 1000; //10000000UL; // 3000ms
    
    // ready to receive
    ESP_ERROR_CHECK(rmt_enable(rx_ppm_chan)) ;
    ESP_ERROR_CHECK(rmt_receive(rx_ppm_chan, raw_symbols, sizeof(raw_symbols), &receive_config));
    ESP_LOGI(TAG, "Receive RX ranges initialized\n");
    ESP_ERROR_CHECK(rmt_enable(rx_ppm_aux_chan)) ;
    ESP_ERROR_CHECK(rmt_receive(rx_ppm_aux_chan, aux_raw_symbols, sizeof(aux_raw_symbols), &receive_config));
    ESP_LOGI(TAG, "Receive AUX ranges initialized\n");
    
    //init_ds18b20() ;
    ESP_LOGI(TAG, "DS18B20 initialized\n");
}

/*bool Get_RPM(uint32_t *rpm) 
{
    uint64_t  period ;
    //ESP_LOGI(TAG,"Get RPM") ;
    if(xSemaphoreTake(xRPMmutex,( TickType_t ) 10) == pdTRUE ) 
    {
        period = turbine.RPM_period ;
        xSemaphoreGive(xRPMmutex) ;
        if(period > 0 )
            *rpm = 60000000 / period ;
        else
            *rpm = 0 ;
        turbine.RPM = *rpm ;
        ESP_LOGI(TAG,"Get RPM mutex RPM : %ld - Period : %ld",*rpm,period) ;
        return 1 ;
    }
    return 0 ;
}*/

void Reset_RPM() 
{
    //ESP_LOGI(TAG,"Reset RPM") ;
    /*if(xSemaphoreTake(xRPMmutex,( TickType_t ) 10) == pdTRUE )
    {*/
        turbine.RPM_period = 0 ;
        turbine.RPM = 0 ;
    /*    xSemaphoreGive(xRPMmutex) ;
        //ESP_LOGI(TAG,"Reset RPM mutex") ;
    }*/
}

uint32_t get_gaz(struct _engine_ * engine)
{
    return engine->GAZ;
}

uint32_t get_aux(struct _engine_ * engine)
{
    return engine->Aux ;
}

uint32_t get_RPM(struct _engine_ * engine)
{
    uint32_t rpm_tmp ;
    gpio_intr_disable(RPM_PIN) ;
    rpm_tmp = engine->RPM ;
    gpio_intr_enable(RPM_PIN) ;
    return rpm_tmp ;

}

uint32_t get_EGT(struct _engine_ * engine)
{
    return engine->EGT ;
}

float get_GLOW_CURRENT(struct _engine_ * engine)
{
    return engine->GLOW_CURRENT ;
}