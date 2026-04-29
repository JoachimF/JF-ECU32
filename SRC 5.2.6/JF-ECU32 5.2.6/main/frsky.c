/*
  frsky.c

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
#ifdef FRSKY

#include "frsky.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"

void config_frsky(void)
{
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 0, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
    ESP_ERROR_CHECK(uart_set_line_inverse(UART_NUM_2, UART_SIGNAL_TXD_INV)) ;
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));
    
}

static void send_byte(uint8_t c, bool header)
{
    if ((c == 0x5D || c == 0x5E) && !header)
    {
        //uart0_write(0x5D);
        uart_write_bytes(UART_NUM_2, 0x5D, 1);
        c ^= 0x60;
    }
    //uart0_write(c);
    uart_write_bytes(UART_NUM_2, c, 1);
    if (debug)
        printf("%X ", c);
}

static void send_packet(uint8_t data_id, uint16_t value)
{
    uint8_t *u8p;
    // header
    send_byte(0x5E, true);
    // data_id
    send_byte(data_id, false);
    // value
    u8p = (uint8_t *)&value;
    send_byte(u8p[0], false);
    send_byte(u8p[1], false);
    // footer
    send_byte(0x5E, true);

    // blink
    //vTaskResume(led_task_handle);
}

static uint16_t format(uint8_t data_id, float value)
{
    if (data_id == FRSKY_D_VOLTS_BP_ID)
        return value * 2;

    if (data_id == FRSKY_D_VOLTS_AP_ID)
        return ((value * 2) - (int16_t)(value * 2)) * 10000;

    if (data_id == FRSKY_D_GPS_HOUR_MIN_ID) // minutes d'allumage
    {
        return value / 100;
    }

    if (data_id == FRSKY_D_GPS_SEC_ID) //secondes d'allumage
    {
        return value - (uint32_t)(value / 100) * 100;
    }

    if (data_id == FRSKY_D_CURRENT_ID || data_id == FRSKY_D_VFAS_ID) //courant bougie
        return round(value * 10);

    if (data_id == FRSKY_D_RPM_ID) //RPM / 1000
        return value / 60;

    return round(value);
}

static void frsky_task(void *parameters)
{
    //frsky_d_sensor_parameters_t parameter = *(frsky_d_sensor_parameters_t *)parameters;
    //xTaskNotifyGive(receiver_task_handle);
    while (1)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        xSemaphoreTake(semaphore, portMAX_DELAY);
        uint16_t data_formatted = format(parameter.data_id, *parameter.value);
        send_packet(parameter.data_id, data_formatted);
        xSemaphoreGive(semaphore);
    }
}

#endif