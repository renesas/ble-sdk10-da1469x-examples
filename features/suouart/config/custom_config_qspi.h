/**
 ****************************************************************************************
 *
 * @file custom_config_qspi.h
 *
 * @brief Board Support Package. User Configuration file for cached QSPI mode.
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

#ifndef CUSTOM_CONFIG_QSPI_H_
#define CUSTOM_CONFIG_QSPI_H_

#include "bsp_definitions.h"

#undef CONFIG_USE_BLE

#undef CONFIG_RETARGET

#define dg_configSUOUART_SUPPORT                 ( 1 )

#define dg_configUSE_LP_CLK                     ( LP_CLK_32768 )
#define dg_configEXEC_MODE                      MODE_IS_CACHED
#define dg_configCODE_LOCATION                  NON_VOLATILE_IS_FLASH

#define dg_configUSE_WDOG                       ( 0 )

#define dg_configFLASH_CONNECTED_TO             (FLASH_CONNECTED_TO_1V8P)
#define dg_configFLASH_POWER_DOWN               ( 1 )
#define dg_configPOWER_1V8P_ACTIVE              ( 1 )
#define dg_configPOWER_1V8P_SLEEP               ( 1 )
#define dg_configPOWER_1V8_ACTIVE               ( 0 )
#define dg_configPOWER_1V8_SLEEP                ( 0 )

#define dg_configUSE_SYS_CHARGER                ( 0 )

#define dg_configUSE_HW_TRNG                    ( 1 )

/*************************************************************************************************\
 * FreeRTOS specific config
 */
#define OS_FREERTOS                             /* Define this to use FreeRTOS */
#define configTOTAL_HEAP_SIZE                   ( 40 * 1024 ) /* This is the FreeRTOS Total Heap Size */

/*************************************************************************************************\
 * Peripheral specific config
 */
#define dg_configFLASH_ADAPTER                  ( 1 )
#define dg_configNVMS_ADAPTER                   ( 1 )
#define dg_configNVMS_VES                       ( 0 )

#  define dg_configGPADC_ADAPTER                ( 1 )

#define dg_configUSE_USB_ENUMERATION            ( 1 )
#define dg_configUSB_DMA_SUPPORT                ( 0 )

#define USE_PARTITION_TABLE_4MB_WITH_SUOTA      ( 1 )

#define dg_configUART_ADAPTER                   ( 1 )     /* Enable the UART Adapter abstraction layer and API */

#if (dg_configUART_ADAPTER == 1)
#undef CONFIG_RETARGET                                    /* Using UART2 for example, must disable RETARGET as it users UART2 */
#endif

/* Include bsp default values */
#include "bsp_defaults.h"
/* Include middleware default values */
#include "middleware_defaults.h"

#endif /* CUSTOM_CONFIG_QSPI_H_ */
