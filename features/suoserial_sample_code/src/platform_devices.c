/**
 ****************************************************************************************
 *
 * @file platform_devices.c
 *
 * @brief Configuration of devices connected to board
 *
 * Copyright (C) 2016-2022 Renesas Electronics Corporation and/or its affiliates
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

#ifdef __cplusplus
extern "C" {
#endif

#if (dg_configUART_ADAPTER == 1)
#include "ad_uart.h"

/**************************** UART configuration start ****************************/

static const ad_uart_io_conf_t suouart_io_conf = {
        /* UART RX pin configuration */
        .rx = {
                /* RX pin configuration selection */
                .port = SER1_RX_PORT,
                .pin = SER1_RX_PIN,
                /* RX pin configuration when device is active ('On') */
                {
                        .mode = SER1_RX_MODE,
                        .function = SER1_RX_FUNC,
                        .high = true,
                },
                /* RX pin configuration when device is in sleep ('Off') */
                {
                        .mode = SER1_RX_MODE,
                        .function = SER1_RX_FUNC,
                        .high = true,
                },
        },

        /* UART TX pin configuration */
        .tx = {
                /* TX pin configuration selection */
                .port = SER1_TX_PORT,
                .pin = SER1_TX_PIN,
                /* TX pin configuration when device is active ('On') */
                {
                        .mode = SER1_TX_MODE,
                        .function = SER1_TX_FUNC,
                        .high = true,
                },
                /* TX pin configuration when device is in sleep ('Off') */
                {
                        .mode = SER1_TX_MODE,
                        .function = SER1_TX_FUNC,
                        .high = true,
                },
        },

        /* Select the Voltage Level for the UART GPIOs */
        .voltage_level = HW_GPIO_POWER_V33,
};

static const ad_uart_driver_conf_t suouart_driver_conf = {
        /* UART controller operation parameters */
        {
                .baud_rate = HW_UART_BAUDRATE_115200,   /* Select the baud rate */
                .data = HW_UART_DATABITS_8,             /* Select the data bits */
                .parity = HW_UART_PARITY_NONE,          /* select the Parity */
                .stop = HW_UART_STOPBITS_1,             /* Select the number of Stop Bits */
                .auto_flow_control = 0,                 /* Enable/Disable the HW flow control.
                                                         * 0 - Disable the use of RTS/CTS flow control.
                                                         * 1 - Enable the use of RTS/CTS flow control.
                                                         *
                                                         *     NOTE: UART1 does not have RTS/CTS capabilities
                                                         *           RTS/CTS can be used only with UART2/UART3 */
                .use_fifo = 1,                          /* Enable/Disable the use of the UART HW FIFO */
                .use_dma = 1,                           /* Enable/Disable the use of DMA for UART transfers. */
                .tx_dma_channel = HW_DMA_CHANNEL_1,     /* If DMA is used, select the DMA channels to use for RX/TX */
                .rx_dma_channel = HW_DMA_CHANNEL_0,     /* The DMA is activated only for transfers >=2 bytes */
                .tx_fifo_tr_lvl = 0,                    /* Set the TX FIFO threshold level for generating the threshold interrupts */
                .rx_fifo_tr_lvl = 0,                    /* Select the FIFO threshold trigger level in the RX FIFO at which the Received Data Available
                                                         * Interrupt is generated.
                                                         * It also determines when the dma_rx_req_n signal is asserted when DMA Mode (FCR[3]) = 1.
                                                         * See DA1469x datasheet for details */
        }
};

const ad_uart_controller_conf_t suouart_conf = {
        .id = SER1_UART,                                 /* Select the HW UART block to configure */
        .io = &suouart_io_conf,                          /* Select the GPIOs settings to use */
        .drv = &suouart_driver_conf,                     /* Select the UART controller operation parameters to use */
};

/**************************** UART configuration end ****************************/
#endif /* dg_configUART_ADAPTER */

#ifdef __cplusplus
}
#endif
