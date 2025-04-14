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
#include "sdcard.h"
//#include "jf-ecu32.h"
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
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"


#ifdef DS18B20
    #include "ds18b20.h"
    #include "onewire_bus_impl_rmt.h"
#endif


#ifdef IMU
    #include "imu.h"
#endif

/* Capteur EGT */
#include <max31855.h>

/* Tension batterie */
adc_oneshot_unit_handle_t adc1_handle;
adc_oneshot_unit_init_cfg_t init_config1 ;
bool do_calibration1 ;
adc_cali_handle_t adc1_cali_handle = NULL;


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



#ifdef DS18B20
// Sonde de temperature externe
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
#endif

/*********** ISR pour les RPM ******************/
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    //uint32_t gpio_num = (uint32_t) arg;
    static uint64_t period,period_tmp ;
    static uint32_t rpm_temp ;
    //static BaseType_t xHigherPriorityTaskWoken;
    gptimer_get_raw_count(gptimer, &period) ;
    gptimer_set_raw_count(gptimer, 0) ;

    period_tmp = (period + (3*turbine.RPM_period))/4 ; //filtre
    turbine.RPM_period  = period_tmp ;

    if(turbine.RPM_period > 200 && turbine.RPM_period < 200000)
    {
            /*if(xSemaphoreTakeFromISR(xRPMmutex,&xHigherPriorityTaskWoken ) == pdTRUE)
            {*/
                //turbine.RPM_period = period ; 
            //period_tmp = (period + (3*turbine.RPM_period))/4 ; //filtre
            //turbine.RPM_period  = period_tmp ;

            //if(period > 0 )
                rpm_temp = 60000000 / period ;
            //else
            //    rpm_temp = 0 ;
            turbine.RPM = rpm_temp ;
                //xSemaphoreGiveFromISR(xRPMmutex,&xHigherPriorityTaskWoken) ;
            /*}
            turbine.WDT_RPM = 1 ;*/
            
    }
    turbine.WDT_RPM = 5 ;
    turbine.RPM_Pulse++ ;
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
    static int adc_raw[2][10];
    static int voltage[2][10];
    //Courant bougie
    ESP_LOGI(TAG, "Task glow_current start");
    ina219_t dev;
    static float volt ;
    float current ;
    memset(&dev, 0, sizeof(ina219_t));
        
    if( config_ECU.INA219_Present == 1	) {
        ESP_ERROR_CHECK(ina219_init_desc(&dev, I2C_ADDR, I2C_PORT, SDA_GPIO, SCL_GPIO));
        ESP_LOGI(TAG, "Initializing INA219");
        ESP_ERROR_CHECK(ina219_init(&dev));

        ESP_LOGI(TAG, "Configuring INA219");
        ESP_ERROR_CHECK(ina219_configure(&dev, INA219_BUS_RANGE_16V, INA219_GAIN_0_25,
            INA219_RES_12BIT_128S, INA219_RES_12BIT_128S, INA219_MODE_CONT_SHUNT_BUS));

        ESP_LOGI(TAG, "Calibrating INA219");

        ESP_ERROR_CHECK(ina219_calibrate(&dev, (float)10 / 1000.0f)); // Résistance de shunt 10mOhm
        ESP_LOGI(TAG, "INA219 initialized\n");
    }
    /* First read ADC */
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN0, &adc_raw[0][0]));
    voltage[0][0] = voltage[0][0]*4.90 ;
    vTaskDelay(200 / portTICK_PERIOD_MS) ;
    
    

    while(1)
    {
        /* Glow current */
        //ESP_ERROR_CHECK(ina219_get_current(&dev, &turbine.glow.current)) ;
        //ESP_ERROR_CHECK(ina219_get_bus_voltage(&dev, &bus_voltage));
        //ESP_ERROR_CHECK(ina219_get_shunt_voltage(&dev, &shunt_voltage));
        //ESP_ERROR_CHECK(ina219_get_power(&dev, &power));
        if( config_ECU.INA219_Present == 1	) {
            ESP_ERROR_CHECK(ina219_get_current(&dev, &current)) ;
            //ESP_LOGI(TAG, "Glow current : %0.3fA",current) ;
            set_glow_current(&turbine.glow,current) ;

            //printf("%fV %0.3fA %fW %fV\n",bus_voltage/1000.0,current,power,shunt_voltage);
            xSemaphoreGive(SEM_glow_current) ;
            //ESP_LOGI(TAG, "Func Glow current : %0.3fA", get_glow_current(&turbine.glow)) ;
        }
        /* Battery voltage */
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN0, &adc_raw[0][0]));
        //ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC1_CHAN0, adc_raw[0][0]);
        if (do_calibration1) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw[0][0], &voltage[0][0]));
            voltage[0][0] = voltage[0][0]*4.90 ;
            //ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC1_CHAN0, voltage[0][0]);
            volt = (get_vbatt(&turbine) + (float)(voltage[0][0])/1000) / 2 ;
            /* For debugging */
            volt = 7.77f ;
            set_vbatt(&turbine, volt) ;
            //ESP_LOGI(TAG, "Output Voltage: %f mV",get_vbatt(&turbine));
        }
        vTaskDelay(200 / portTICK_PERIOD_MS) ;
    }
}

uint8_t scan_i2c(int *addresses) 
{
    uint8_t nb_periph = 0 ;
    i2c_dev_t devT = { 0 };
    ESP_ERROR_CHECK(i2cdev_init());
    devT.cfg.sda_io_num = SDA_GPIO ;
    devT.cfg.scl_io_num = SCL_GPIO ;
    devT.cfg.master.clk_speed = 100000; // 100kHz
    
    esp_err_t res;
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    printf("00:         ");
    for (uint8_t addr = 3; addr < 0x78; addr++)
    {
        if (addr % 16 == 0)
            printf("\n%.2x:", addr);

        devT.addr = addr;
        res = i2c_dev_probe(&devT, I2C_DEV_WRITE);

        if (res == 0){
            
            printf(" %.2x", addr);
            nb_periph++ ;
            addresses = realloc(addresses,sizeof(uint8_t)*nb_periph) ;
            addresses[nb_periph] = addr ;
        }

            
        else
            printf(" --");
    }
    printf("\n\n");
    return nb_periph ;
}

/*********** Tache de lecture des EGT et DS18B20 période 200ms ******************/
void task_egt(void *pvParameter)
{
    max31855_t dev = { 0 };
    // **********  Configure SPI bus
    // Now initialized in init_inputs()

    /*spi_bus_config_t cfg = {
       .mosi_io_num = -1,
       .miso_io_num = MISO_GPIO_NUM,
       .sclk_io_num = CLK_GPIO_NUM,
       .quadwp_io_num = -1,
       .quadhd_io_num = -1,
       .max_transfer_sz = 0,
       .flags = 0
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &cfg, 1));*/

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
            //if (scv) ESP_LOGW(TAG, "Thermocouple shorted to VCC!");
            //if (scg) ESP_LOGW(TAG, "Thermocouple shorted to GND!");
            //if (oc) ESP_LOGW(TAG, "No connection to thermocouple!");
            if (scv) add_error_msg(E_K,"K shorted to VCC!");
            if (scg) add_error_msg(E_K,"K shorted to GND!");
            if (oc) add_error_msg(E_K,"K not connected");
            //ESP_LOGI(TAG, "Temperature: %.2f°C, cold junction temperature: %.4f°C", tc_t, cj_t);
            //ESP_LOGI("wifi", "free Heap:%d,%d", esp_get_free_heap_size(), heap_caps_get_free_size(MALLOC_CAP_8BIT));*/
            turbine.EGT = (turbine.EGT + tc_t)/2 ; //filtre
            xSemaphoreGive(SEM_EGT) ;
        }
        #ifdef DS18B20
        if(turbine.ds18b20_device_num > 0)
            ds18b20_get_temperature(turbine.ds18b20s[0],&turbine.DS18B20_temp) ;
        #endif
        for(int i=9; i>0; i--)
            turbine.EGTs[i] = turbine.EGTs[i-1] ;
        turbine.EGTs[0] = get_EGT(&turbine) ;
        set_delta_EGT(&turbine,((turbine.EGTs[0]-turbine.EGTs[9])+get_delta_EGT(&turbine))/2) ;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* Initialisation des entrées de la carte */
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

    /* SPI pins for SDcard and MAX31855*/
    ESP_LOGI("SPI","Set pins level for SDCARD");
   /*gpio_set_direction(PIN_NUM_MISO, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_NUM_MOSI, GPIO_MODE_OUTPUT);

    gpio_set_direction(PIN_NUM_CS, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_CS, GPIO_MODE_OUTPUT);
    
    gpio_set_level(PIN_NUM_CS,1) ;
    gpio_set_level(PIN_NUM_MOSI,1) ;
    gpio_set_level(PIN_NUM_CLK,1) ;
    gpio_set_level(PIN_NUM_CS,1) ;
    gpio_pullup_en(PIN_NUM_MOSI) ;
    gpio_pullup_en(PIN_NUM_CS) ;
    gpio_pullup_en(PIN_NUM_CLK) ;
    gpio_pullup_en(PIN_NUM_CS) ;*/
    sdmmc_card_t card ;
    ESP_LOGI("SDCARD","Init SDCARD");
    init_sdcard(&card) ;


    //** Interruption RPM
    gpio_set_direction(RPM_PIN, GPIO_MODE_INPUT);
    gpio_pullup_dis(RPM_PIN);
    gpio_pulldown_dis(RPM_PIN);
    //gpio_pullup_en(SDA_GPIO);
    //gpio_pullup_en(SCL_GPIO);
    //gpio_pulldown_en(RPM_PIN);
    gpio_set_intr_type(RPM_PIN, GPIO_INTR_POSEDGE);    
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3|ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(RPM_PIN, gpio_isr_handler, (void *)RPM_PIN);
    gpio_intr_enable(RPM_PIN) ;

    ESP_LOGI("RPM","Initialized");

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

     /* PPMs */
    receive_config.signal_range_min_ns = 0; //100 * 1000;    // 700ms
    receive_config.signal_range_max_ns = 3000 * 1000; //10000000UL; // 3000ms
    
   
    ESP_ERROR_CHECK(rmt_enable(rx_ppm_chan)) ;
    ESP_ERROR_CHECK(rmt_receive(rx_ppm_chan, raw_symbols, sizeof(raw_symbols), &receive_config));
    ESP_LOGI(TAG, "Receive RX ranges initialized\n");
    ESP_ERROR_CHECK(rmt_enable(rx_ppm_aux_chan)) ;
    ESP_ERROR_CHECK(rmt_receive(rx_ppm_aux_chan, aux_raw_symbols, sizeof(aux_raw_symbols), &receive_config));
    ESP_LOGI(TAG, "Receive AUX ranges initialized\n");

    ESP_LOGI("PPM","Initialized");

    /* INA219 */
    /* Initalistion du port I2C */
    ESP_ERROR_CHECK(i2cdev_init());
    ESP_LOGI(TAG, "I2C initialized\n");
    //scan_i2c() ;
    

    /* ADC tension batterie */
    init_config1.unit_id = ADC_UNIT_1;
    init_config1.ulp_mode = ADC_ULP_MODE_DISABLE;
    
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle,ADC1_CHAN0, &config));
    //-------------ADC1 Calibration Init---------------//
    do_calibration1 = adc_calibration_init(ADC_UNIT_1, ADC_ATTEN, &adc1_cali_handle);


    #ifdef DS18B20
    /* DS18B20 */
    init_ds18b20() ;
    ESP_LOGI(TAG, "DS18B20 initialized\n");
    #endif

    #ifdef IMU
    //init MPU6050
    i2c_sensor_mpu6050_init() ;
    ESP_LOGI(TAG, "MPU6050 initialized\n");

    #endif
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

/* Get WDT_RPM */
uint8_t get_WDT_RPM(struct _engine_ * engine)
{
    uint8_t WDT_tmp ;
    gpio_intr_disable(RPM_PIN) ;
    WDT_tmp = engine->WDT_RPM ;
    gpio_intr_enable(RPM_PIN) ;
    return WDT_tmp ;

}
/*Remet à 0 les RPM par manque d'impulsion */
void Reset_RPM() 
{
    gpio_intr_disable(RPM_PIN) ;
    turbine.RPM_period = 0 ;
    turbine.RPM = 0 ;
    gpio_intr_enable(RPM_PIN) ;
}

/* Renvoie la valeur du manche de gaz */
uint32_t get_gaz(struct _engine_ * engine)
{
    return engine->GAZ;
}

/* Renvoie la valeur de la voie auxilaire */
uint32_t get_aux(struct _engine_ * engine)
{
    return engine->Aux ;
}

/* Renvoie la valeur des RPM */
uint32_t get_RPM(struct _engine_ * engine)
{
    uint32_t rpm_tmp ;
    gpio_intr_disable(RPM_PIN) ;
    rpm_tmp = engine->RPM ;
    gpio_intr_enable(RPM_PIN) ;
    return rpm_tmp ;
}

/* Renvoie la valeur des EGT */
uint32_t get_EGT(struct _engine_ * engine)
{
    return engine->EGT ;
}


/*Gestion des deltas*/
void set_delta_RPM(_engine_t * engine,int32_t delta)
{
    engine->RPM_delta = delta ;
}

int32_t get_delta_RPM(_engine_t * engine) 
{
    return engine->RPM_delta ;
}

void set_delta_EGT(_engine_t * engine, int32_t delta) 
{
    engine->EGT_delta = delta ;
}

int32_t get_delta_EGT(_engine_t * engine)
{
    return engine->EGT_delta ;
}

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

/*static void adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}*/

uint8_t get_lipo_elements(void)
{
    float volt = get_vbatt(&turbine) ;
    //ESP_LOGI(TAG,"Vbatt : %0.2f",volt) ;

    if(volt < 4.3 )
    {
        return 1;
    }
    if(volt > 6.6 && volt < 8.5)
    {
        return 2;
    }
    if(volt > 9.9 && volt < 12.7)
    {
        return 3;
    }
    if(volt > 13.2 && volt < 17)
    {
        return 4;
    }
    return 0 ;
}


bool battery_check(void)
{
    uint8_t nb_lipo_conf,nb_lipo ;
    float volt, vmin ;

    nb_lipo_conf = get_conf_lipo_elements() ;
    //ESP_LOGI(TAG,"Batterie configurée %d éléments",nb_lipo_conf) ;
    nb_lipo = get_lipo_elements() ;
    //ESP_LOGI(TAG,"Batterie branché %d éléments",nb_lipo) ;
    volt = get_vbatt(&turbine) ;
    vmin = get_Vmin_decollage() ;
    set_batOk(1) ;
    if(nb_lipo_conf != nb_lipo)
    {
        set_batOk(0) ;
        add_error_msg(E_BATTCONF,"Batterie mal configurée");
        ESP_LOGI(TAG,"Batterie mal configurée") ;		
    }
	
    if(nb_lipo > 3 || nb_lipo < 2)
    {
        set_batOk(0) ;
        add_error_msg(E_BATTWRONG,"Batterie non compatible");
        //ESP_LOGI(TAG,"Batterie non compatible") ;		
    }
    
    if(volt < vmin)
    {
        set_batOk(0) ;
        add_error_msg(E_BATTLOW,"Batterie trop faible");
        //ESP_LOGI(TAG,"Batterie trop faible") ;		
    }
    if(isBatOk())
        return 1 ;
    else
        return 0 ;
}


