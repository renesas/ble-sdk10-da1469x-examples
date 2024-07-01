/**
 ****************************************************************************************
 *
 * @file main.c
 *
 * @brief Software update over UART Example
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
#include <stdbool.h>
#include "osal.h"
#include "sys_clock_mgr.h"
#include "sys_power_mgr.h"
#include "sys_watchdog.h"
#include "suoserial_task.h"
#include "hw_gpio.h"
#if defined(SUOUSB_SUPPORT)
# if (dg_configUSB_DMA_SUPPORT == 1)
#  include "hw_dma.h"
# endif /* dg_configUSB_DMA_SUPPORT */
# include "sys_usb.h"
# if (dg_configUSE_SYS_CHARGER == 1)
#  include "sys_charger.h"
#  include "custom_charging_profile.h"
# endif /* dg_configUSE_SYS_CHARGER */
#endif


#if dg_configUSE_WDOG
__RETAINED int8_t idle_task_wdog_id;
#endif

static void prvSetupHardware(void);

static void system_init(void *pvParameters)
{
#if defined CONFIG_RETARGET
        extern void retarget_init(void);
#endif

        /* Prepare clocks. Note: cm_cpu_clk_set() and cm_sys_clk_set() can be called only from a
         * task since they will suspend the task until the XTAL16M has settled and, maybe, the PLL
         * is locked.
         */
        cm_sys_clk_init(sysclk_XTAL32M);
        cm_apb_set_clock_divider(apb_div1);
        cm_ahb_set_clock_divider(ahb_div1);
        cm_lp_clk_init();

        /* Initialize platform watchdog */
        sys_watchdog_init();

#if dg_configUSE_WDOG
        // Register the Idle task first.
        idle_task_wdog_id = sys_watchdog_register(false);
        ASSERT_WARNING(idle_task_wdog_id != -1);
        sys_watchdog_configure_idle_id(idle_task_wdog_id);
#endif

        /* Prepare the hardware to run this demo. */
        prvSetupHardware();

#if defined CONFIG_RETARGET
        retarget_init();
#endif

        /* Set the desired sleep mode. */
        pm_set_wakeup_mode(true);
        pm_sleep_mode_set(pm_mode_extended_sleep);

#if defined(SUOUART_SUPPORT)
        suoserial_start_task();
#elif defined(SUOUSB_SUPPORT)
# if (dg_configUSE_USB_ENUMERATION == 1)
# if (dg_configUSB_DMA_SUPPORT == 1)
        sys_usb_driver_conf_t cfg;

        /* Set the desired DMA channels and parameters
         * to be used with USB DATA interface.
         * The USB subsystem will try to use these resources on
         * every plug-in.
         * However, if the DMA resources are not available
         * when the USB plug-in event occurs, then it will
         * automatically fallback to USB operation in interrupt
         * mode to ensure that the USB will be functional.
         */

        cfg.usb.use_dma = true; /* enable the use of DMA with USB */
        cfg.usb.rx_dma_channel = HW_DMA_CHANNEL_0; /* Select the DMA channel for USB RX.
                                                    * Avoid channel 7 which is the only
                                                    * way to write the keys to Crypto Engine
                                                    */
        cfg.usb.tx_dma_channel = HW_DMA_CHANNEL_1; /* Select the DMA channel for USB RX.
                                                    * Avoid channel 7 which is the only
                                                    * way to write the keys to Crypto Engine
                                                    */
        cfg.usb.rx_dma_prio = HW_DMA_PRIO_4;       /* Set the priority of the RX DMA */
        cfg.usb.tx_dma_prio = HW_DMA_PRIO_5;       /* Set the priority of the TX DMA */

        sys_usb_cfg(&cfg);                         /* Apply the configuration.
                                                    * Call this before the sys_usb_init()
                                                    */
# endif /* dg_configUSB_DMA_SUPPORT */
# endif /* dg_configUSE_USB_ENUMERATION */

        sys_usb_init();

# if (dg_configUSE_SYS_CHARGER == 1)
        sys_charger_init(&sys_charger_conf);
# endif /* dg_configUSE_SYS_CHARGER */
#endif

        /* the work of the SysInit task is done */
        OS_TASK_DELETE(OS_GET_CURRENT_TASK());
}


static void periph_init(void)
{
#if defined(SUOUSB_SUPPORT)
        /* USB data pin configuration */
        hw_gpio_set_pin_function(HW_GPIO_PORT_0, HW_GPIO_PIN_14, HW_GPIO_MODE_INPUT,
                                 HW_GPIO_FUNC_USB);
        hw_gpio_set_pin_function(HW_GPIO_PORT_0, HW_GPIO_PIN_15, HW_GPIO_MODE_INPUT,
                                 HW_GPIO_FUNC_USB);
#endif
}


/**
 * @brief BLE FW demo main creates a BLE task
 */
int main(void)
{
        OS_TASK handle;
        OS_BASE_TYPE status;

        /* Start SysInit task. */
        status = OS_TASK_CREATE("SysInit",      /* The text name assigned to the task, for
                                                   debug only; not used by the kernel. */
                        system_init,            /* The System Initialization task. */
                        ( void * ) 0,           /* The parameter passed to the task. */
                        1024,                   /* The number of bytes to allocate to the
                                                   stack of the task. */
                        OS_TASK_PRIORITY_HIGHEST, /* The priority assigned to the task. */
                        handle);                /* The task handle */

        OS_ASSERT(status == OS_TASK_CREATE_SUCCESS);

        /* Start the tasks and timer running. */
        vTaskStartScheduler();

        /* If all is well, the scheduler will now be running, and the following
         line will never be reached.  If the following line does execute, then
         there was insufficient FreeRTOS heap memory available for the idle and/or
         timer tasks to be created.  See the memory management section on the
         FreeRTOS web site for more details. */
        for (;;)
                ;
}

static void prvSetupHardware(void)
{
        /* Init hardware */
        pm_system_init(periph_init);
}

/**
 * @brief Malloc fail hook
 */
void vApplicationMallocFailedHook(void)
{
        /* vApplicationMallocFailedHook() will only be called if
        configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
        function that will get called if a call to pvPortMalloc() fails.
        pvPortMalloc() is called internally by the kernel whenever a task, queue,
        timer or semaphore is created.  It is also called by various parts of the
        demo application.  If heap_1.c or heap_2.c are used, then the size of the
        heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
        FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
        to query the size of free heap space that remains (although it does not
        provide information on how the remaining heap might be fragmented). */
        ASSERT_ERROR(0);
}

/**
 * @brief Application idle task hook
 */
void vApplicationIdleHook(void)
{
        /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
         to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
         task. It is essential that code added to this hook function never attempts
         to block in any way (for example, call xQueueReceive() with a block time
         specified, or call vTaskDelay()).  If the application makes use of the
         vTaskDelete() API function (as this demo application does) then it is also
         important that vApplicationIdleHook() is permitted to return to its calling
         function, because it is the responsibility of the idle task to clean up
         memory allocated by the kernel to any task that has since been deleted. */

#if dg_configUSE_WDOG
        sys_watchdog_notify(idle_task_wdog_id);
#endif
}

/**
 * @brief Application stack overflow hook
 */
void vApplicationStackOverflowHook( OS_TASK pxTask, char *pcTaskName)
{
        (void)pcTaskName;
        (void)pxTask;

        /* Run time stack overflow checking is performed if
         configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
        function is called if a stack overflow is detected. */
        ASSERT_ERROR(0);
}

/**
 * @brief Application tick hook
 */
void vApplicationTickHook(void)
{
}

