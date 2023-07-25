/**
 ****************************************************************************************
 *
 * @file dsps_uart.c
 *
 * @brief Simple SPS wrapper to UART
 *
 * Copyright (c) 2023 Renesas Electronics Corporation and/or its affiliates
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 ****************************************************************************************
 */
#if defined(DSPS_UART)

#include <string.h>
#include "osal.h"
#include "platform_devices.h"
#include "hw_uart.h"
#include "ad_uart.h"
#include "dsps_uart.h"
#include "dsps_common.h"
#include "misc.h"

#define UART_CLOSE_TIMEOUT_MS   1000

/* Return time in us for one byte transmission at 8N1 (10 bits per byte) */
static uint32_t byte_time(HW_UART_BAUDRATE baud)
{
        switch (baud) {
        case HW_UART_BAUDRATE_1000000 : return 10;
        case HW_UART_BAUDRATE_500000  : return 20;
        case HW_UART_BAUDRATE_230400  : return 44;
        case HW_UART_BAUDRATE_115200  : return 87;
        case HW_UART_BAUDRATE_57600   : return 174;
        case HW_UART_BAUDRATE_38400   : return 261;
        case HW_UART_BAUDRATE_28800   : return 348;
        case HW_UART_BAUDRATE_19200   : return 521;
        case HW_UART_BAUDRATE_14400   : return 695;
        case HW_UART_BAUDRATE_9600    : return 1042;
        case HW_UART_BAUDRATE_4800    : return 2084;
        default:
                /* Invalid baudrate requested */
                OS_ASSERT(0);
                return 0;
        }
}

ad_uart_handle_t uart_open(const ad_uart_controller_conf_t *ctr)
{
        ASSERT_WARNING(ctr != NULL);

        return (ad_uart_open(ctr));
}

int uart_close(ad_uart_handle_t handle)
{
        uint64_t timestamp = __sys_ticks_timestamp();

        while (ad_uart_close(handle, false) != AD_UART_ERROR_NONE) {
                if (__sys_ticks_timestamp() - timestamp >= OS_MS_2_TICKS(UART_CLOSE_TIMEOUT_MS)) {
                        /* Time has elapsed, close the adapter forcefully. */
                        ad_uart_close(handle, true);
                        break;
                }
                OS_DELAY_MS(10);
        }
        return true;
}

uint32_t uart_read_timeout(HW_UART_BAUDRATE baud, uint32_t rx_size)
{
        uint32_t timeout;

        timeout = byte_time(baud) * rx_size / 1000 + 10; /*Leave 10ms margin*/
        return timeout;
}

int read_from_uart(ad_uart_handle_t handle, char *buf, uint32_t len, OS_TICK_TIME timeout)
{
        ASSERT_WARNING(buf != NULL);

        return (ad_uart_read(handle, buf, len, timeout));
}

int write_to_uart(ad_uart_handle_t handle, const char *buf, uint32_t len)
{
        ASSERT_WARNING(buf != NULL);

        return (ad_uart_write(handle, buf, len));
}

void uart_hw_sps_flow_off(const ad_uart_controller_conf_t *ctr)
{
        ASSERT_WARNING(ctr != NULL);

        hw_uart_rts_setf(ctr->id, 0);
}

void uart_sw_sps_flow_off(ad_uart_handle_t handle)
{
        uint8_t flow_byte;

        flow_byte = UART_XOFF_BYTE;
        write_to_uart(handle, (const char *)&flow_byte, sizeof(flow_byte));
}

void uart_hw_sps_flow_on(const ad_uart_controller_conf_t *ctr)
{
        ASSERT_WARNING(ctr != NULL);

        hw_uart_afce_setf(ctr->id, 1);
        hw_uart_rts_setf(ctr->id, 1);
}

void uart_sw_sps_flow_on(ad_uart_handle_t handle)
{
        uint8_t flow_byte;

        flow_byte = UART_XON_BYTE;
        write_to_uart(handle, (const char *)&flow_byte, sizeof(flow_byte));
}
#endif
