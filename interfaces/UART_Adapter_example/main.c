/**
 ****************************************************************************************
 *
 * @file main.c
 *
 * @brief UART Adapters example with all three UARTs
 *
 * Copyright (C) 2015-2021 Renesas Electronics Corporation and/or its affiliates
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

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "osal.h"
#include "resmgmt.h"
#include "hw_cpm.h"
#include "hw_gpio.h"
#include "sys_watchdog.h"
#include "sys_clock_mgr.h"
#include "sys_power_mgr.h"

#include "platform_devices.h"
#include "ad_uart.h"

#define UART2_NOTIF_BYTE_SENT               ( 1 << 0 )
#define UART2_NOTIF_BYTE_NOT_SENT           ( 1 << 1 )
#define UART2_NOTIF_BYTE_RECEIVED           ( 1 << 2 )
#define UART2_NOTIF_BYTE_NOT_RECEIVED       ( 1 << 3 )

__RETAINED OS_QUEUE uart2_Q;    /* Q used to pass the data between the two UART2 tasks */
__RETAINED OS_MUTEX uart2_Mtx;  /* Mutex used to protect the open of the UART from more than one tasks */

__RETAINED ad_uart_handle_t uart2_h; /* The Uart2 Handler is global because it is used by more than one tasks */

#if dg_configUSE_WDOG
__RETAINED_RW int8_t idle_task_wdog_id = -1;
#endif

/*
 * Tasks functions declarations
 */
static void prv_Uart1_echo_Task( void *pvParameters );

static void prv_Uart2_async_RX_Task( void *pvParameters );
static void prv_Uart2_async_TX_Task( void *pvParameters );

static void prv_Uart3_rts_cts_flow_ctrl_echo_Task( void *pvParameters );

/*
 * Perform any application specific hardware configuration.  The clocks,
 * memory, etc. are configured before main() is called.
 */
static void prvSetupHardware( void );

/**
 * @brief system_init() initialize the system (e.g. selects the desired clocks), selects operation parameters (e.g. sleep mode)
 *               and creates the uart_test_task task which which actually uses all three UARTs.
 *               The system_init runs once and exits when finishes its work.
 */
static void system_init( void *pvParameters )
{
        OS_TASK uart_test_task_h = NULL;
        OS_BASE_TYPE ret;

#if defined CONFIG_RETARGET
        extern void retarget_init(void);
#endif

        cm_sys_clk_init(sysclk_XTAL32M);

        cm_apb_set_clock_divider(apb_div1);
        cm_ahb_set_clock_divider(ahb_div1);
        cm_lp_clk_init();

#if dg_configUSE_WDOG
        /* Initialize platform watchdog */
        sys_watchdog_init();

        /* Register the Idle task first */
        idle_task_wdog_id = sys_watchdog_register(false);
        ASSERT_WARNING(idle_task_wdog_id != -1);
        sys_watchdog_configure_idle_id(idle_task_wdog_id);
#endif

        /* Prepare the hardware to run this demo. */
        prvSetupHardware();

#if defined CONFIG_RETARGET
        retarget_init();
#endif

        /* true - wait for the XTAL32M to be ready before run the app code on wake-up */
        pm_set_wakeup_mode(true);

        /* Set the desired sleep mode. */
        pm_sleep_mode_set(pm_mode_extended_sleep);

        /* Set the desired wakeup mode. */
        pm_set_sys_wakeup_mode(pm_sys_wakeup_mode_fast);

        /* UART1 echo task without flow control */
        OS_TASK_CREATE( "U1 ECHO",                                      /* The text name assigned to the task, for
                                                                           debug only; not used by the kernel. */
                        prv_Uart1_echo_Task,                            /* The function that implements the task. */
                        NULL,                                           /* The parameter passed to the task. */
                        configMINIMAL_STACK_SIZE * OS_STACK_WORD_SIZE,  /* The number of bytes to allocate to the
                                                                           stack of the task. */
                        OS_TASK_PRIORITY_NORMAL,                        /* The priority assigned to the task. */
                        uart_test_task_h );                             /* The task handle */
        OS_ASSERT(uart_test_task_h);                                    /* Check that the task created OK */

        ret = OS_MUTEX_CREATE(uart2_Mtx);                               /* Create the MUTEX to be used for
                                                                         * protected opening of UART2 */
        OS_ASSERT(ret == OS_MUTEX_CREATE_SUCCESS);                      /* Check that mutex created OK */

        OS_QUEUE_CREATE(uart2_Q, sizeof(char), 100);                    /* Create the uart2_Q */
        OS_ASSERT(uart2_Q);                                             /* Check that Q created OK */


        /* UART2 RX task with RTS/CTS flow control*/
        OS_TASK_CREATE( "U2 RX RTS/CTS",                                /* The text name assigned to the task, for
                                                                           debug only; not used by the kernel. */
                        prv_Uart2_async_RX_Task,                        /* The function that implements the task. */
                        NULL,                                           /* The parameter passed to the task. */
                        configMINIMAL_STACK_SIZE * OS_STACK_WORD_SIZE,  /* The number of bytes to allocate to the
                                                                           stack of the task. */
                        OS_TASK_PRIORITY_NORMAL,                        /* The priority assigned to the task. */
                        uart_test_task_h );                             /* The task handle */
        OS_ASSERT(uart_test_task_h);                                    /* Check that the task created OK */

        /* UART2 TX task with RTS/CTS flow control */
        OS_TASK_CREATE( "U2 TX RTS/CTS",                                /* The text name assigned to the task, for
                                                                           debug only; not used by the kernel. */
                        prv_Uart2_async_TX_Task,                        /* The function that implements the task. */
                        NULL,                                           /* The parameter passed to the task. */
                        configMINIMAL_STACK_SIZE * OS_STACK_WORD_SIZE,  /* The number of bytes to allocate to the
                                                                           stack of the task. */
                        OS_TASK_PRIORITY_NORMAL,                        /* The priority assigned to the task. */
                        uart_test_task_h );                             /* The task handle */
        OS_ASSERT(uart_test_task_h);                                    /* Check that the task created OK */

        /* UART3 ECHO task with RTS/CTS flow control*/
        OS_TASK_CREATE( "U3 ECHO RTS/CTS",                              /* The text name assigned to the task, for
                                                                           debug only; not used by the kernel. */
                        prv_Uart3_rts_cts_flow_ctrl_echo_Task,          /* The function that implements the task. */
                        NULL,                                           /* The parameter passed to the task. */
                        configMINIMAL_STACK_SIZE * OS_STACK_WORD_SIZE,  /* The number of bytes to allocate to the
                                                                           stack of the task. */
                        OS_TASK_PRIORITY_NORMAL,                        /* The priority assigned to the task. */
                        uart_test_task_h );                             /* The task handle */
        OS_ASSERT(uart_test_task_h);                                    /* Check that the task created OK */


        /* the work of the SysInit task is done
         * The task will be terminated */
        OS_TASK_DELETE( OS_GET_CURRENT_TASK() );
}

/**
 * @brief main() creates a 'system_init' task and starts the FreeRTOS scheduler.
 */
int main( void )
{
        OS_BASE_TYPE status;
        static OS_TASK xsystem_init_Handle;


        /* Start the two tasks as described in the comments at the top of this
        file. */
        status = OS_TASK_CREATE("SysInit",                              /* The text name assigned to the task, for
                                                                           debug only; not used by the kernel. */
                        system_init,                                    /* The System Initialization task. */
                        ( void * ) 0,                                   /* The parameter passed to the task. */
                        configMINIMAL_STACK_SIZE * OS_STACK_WORD_SIZE,  /* The number of bytes to allocate to the
                                                                           stack of the task. */
                        OS_TASK_PRIORITY_HIGHEST,                       /* The priority assigned to the task.
                                                                           we need this task to run first with
                                                                           no interruption, that is why choose
                                                                           HIGHEST priority*/
                        xsystem_init_Handle );                          /* The task handle */

        OS_ASSERT(status == OS_TASK_CREATE_SUCCESS);                    /* Check that the task created OK */

        /* Start the tasks and timer running. */
        vTaskStartScheduler();

        /* If all is well, the scheduler will now be running, and the following
        line will never be reached.  If the following line does execute, then
        there was insufficient FreeRTOS heap memory available for the idle and/or
        timer tasks to be created.  See the memory management section on the
        FreeRTOS web site for more details. */
        for ( ;; );

}

/**
 * @brief UART 1 echo task without UART flow control.
 *        The task reads a character on RX and sends it back on TX
 */
static void prv_Uart1_echo_Task( void *pvParameters )
{
        char c=0;
        uint32_t bytes;
        ad_uart_handle_t uart1_h;

        uart1_h = ad_uart_open(&uart1_uart_conf);                               /* Open the UART with the desired configuration    */
        ASSERT_ERROR(uart1_h != NULL);                                          /* Check if the UART1 opened OK */

        do {
                bytes = ad_uart_read(uart1_h, &c, 1, OS_EVENT_FOREVER);         /* Wait for one char synchronously                 */
                if (bytes > 0) {                                                /* if there is a successful read...                */
                        ad_uart_write(uart1_h, &c, bytes);                      /*       then write back the char to UART (echo)   */
                }
        } while( c != 27 );                                                     /* Exit the task if received ESC character (ASCII=27) */

        while (ad_uart_close(uart1_h, false) == AD_UART_ERROR_CONTROLLER_BUSY); /* Wait until the UART has finished all the transactions
                                                                                 * before exiting. */

        OS_TASK_DELETE( OS_GET_CURRENT_TASK() );                                /* Delete the task before exiting. It is not allowed in
                                                                                 * FreeRTOS a task to exit without being deleted from
                                                                                 * the OS's queues */
}

/**
 * @brief UART 2 async TX callback.
 *        The function is called when the UART TX completes
 *        Then from the callback we notify the Task that TX completed
 *        IMPORTANT NOTE: The callback function runs in ISR context
 */
void uart2_write_arync_cb(void *user_data, uint16_t transferred)
{
        OS_TASK task_h = (OS_TASK)user_data;

        OS_TASK_NOTIFY_FROM_ISR(task_h, UART2_NOTIF_BYTE_SENT, OS_NOTIFY_SET_BITS);
}

/**
 * @brief UART 2 TX task.
 *        Receives bytes from uart2_Q and send them to UART2 pins
 *        RTS/CTS flow control is used
 */
static void prv_Uart2_async_TX_Task( void *pvParameters )
{
        char c=0;
        OS_BASE_TYPE ret __UNUSED;
        uint32_t notif;

        OS_MUTEX_GET(uart2_Mtx, OS_MUTEX_FOREVER);                      /* Make sure there will be protected the opening
                                                                         * of the UART2 because there are two tasks that will try to open it. */
                                                                        /* The protection can be achieved also by calling the
                                                                         * OS_ENTER_CRITICAL_SECTION();/OS_LEAVE_CRITICAL_SECTION(); */

        if (uart2_h == NULL) {
                uart2_h = ad_uart_open(&uart2_uart_conf);               /* Open the UART2 only if is not opened by the other task. */
        }
        OS_MUTEX_PUT(uart2_Mtx);                                        /* Release protection for the opening of UART2 */

        ASSERT_ERROR(uart2_h != NULL);                                  /* Check if the UART2 opened with success */

        do {
                ret = OS_QUEUE_GET(uart2_Q , &c, OS_QUEUE_FOREVER);     /* Get a character from the Q. Task will be suspended if Q is empty */
                OS_ASSERT(ret == OS_OK);                                /* Check that the Q operation was OK */

                ret = ad_uart_write_async(uart2_h, &c, 1, uart2_write_arync_cb, OS_GET_CURRENT_TASK());
                                                                        /* Wait for one char asynchronously TX */
                if (ret == AD_UART_ERROR_NONE) {                        /* if the async write successfully issued */
                        ret = OS_TASK_NOTIFY_WAIT(0, OS_TASK_NOTIFY_ALL_BITS, &notif, OS_TASK_NOTIFY_FOREVER);
                                                                        /*       wait to be notified from the callback */
                }

        } while ( c != 27 );                                              /* Exit the task if received ESC character (ASCII=27) */

        while (ad_uart_close(uart2_h, false) == AD_UART_ERROR_CONTROLLER_BUSY); /* Wait until the UART has finished all the transactions
                                                                                 * before exiting. */

        OS_QUEUE_DELETE(uart2_Q);                                       /* Q is not needed anymore, so delete it */
        OS_TASK_DELETE( OS_GET_CURRENT_TASK() );                        /* Delete the task before exiting. It is not allowed in
                                                                         * FreeRTOS a task to exit without being deleted from
                                                                         * the OS's queues */
}

/**
 * @brief UART 2 async RX callback.
 *        The function is called when the UART RX completes
 *        Then from the callback we notify the Task depending on the
 *        UART RX result (read data or not)
 *        IMPORTANT NOTE: The callback function runs in ISR context
 */
void uart2_read_arync_cb(void *user_data, uint16_t transferred)
{
        OS_TASK task_h = (OS_TASK)user_data;

        if (transferred>0) {
                OS_TASK_NOTIFY_FROM_ISR(task_h, UART2_NOTIF_BYTE_RECEIVED, OS_NOTIFY_SET_BITS);
        } else {
                OS_TASK_NOTIFY_FROM_ISR(task_h, UART2_NOTIF_BYTE_NOT_RECEIVED, OS_NOTIFY_SET_BITS);
        }
}

/**
 * @brief UART 2 RX task.
 *        Receives bytes from UART pins and put them in uart2_Q
 *        RTS/CTS flow control is used
 */
static void prv_Uart2_async_RX_Task( void *pvParameters )
{
#if (dg_configUART_ADAPTER == 1)
        char c=0;
        OS_BASE_TYPE ret __UNUSED;
        uint32_t notif;

        OS_MUTEX_GET(uart2_Mtx, OS_MUTEX_FOREVER);                      /* Make sure there will be protected the opening
                                                                         * of the UART2 because there are two tasks that will try to open it. */
                                                                        /* The protection can be achieved also by calling the
                                                                         * OS_ENTER_CRITICAL_SECTION();/OS_LEAVE_CRITICAL_SECTION(); */

        if (uart2_h == NULL) {
                uart2_h = ad_uart_open(&uart2_uart_conf);               /* Open the UART2 only if is not opened by the other task. */
        }
        OS_MUTEX_PUT(uart2_Mtx);                                        /* Release protection for the opening of UART2 */

        ASSERT_ERROR(uart2_h != NULL);                                  /* Check if the UART2 opened with success */

        do {
                ret = ad_uart_read_async(uart2_h, &c, 1, uart2_read_arync_cb, OS_GET_CURRENT_TASK());
                                                                        /* Wait for one char asynchronously RX*/
                if (ret == AD_UART_ERROR_NONE) {                        /* if the async read successfully issued */
                        ret = OS_TASK_NOTIFY_WAIT(0, OS_TASK_NOTIFY_ALL_BITS, &notif, OS_TASK_NOTIFY_FOREVER);
                                                                        /*       wait to be notified from the callback */
                        OS_ASSERT(ret == OS_OK);                        /*       Check that the task resumed OK */
                        if (notif & UART2_NOTIF_BYTE_RECEIVED) {
                                OS_QUEUE_PUT(uart2_Q, &c, OS_QUEUE_FOREVER);    /*       then write back the char to UART (echo) */
                        }
                }
        } while( c != 27 );                                               /* Exit the task if received ESC character (ASCII=27) */

#endif

        OS_TASK_DELETE( OS_GET_CURRENT_TASK() );                        /* Delete the task before exiting. It is not allowed in
                                                                         * FreeRTOS a task to exit without being deleted from
                                                                         * the OS's queues */
}


/**
 * @brief UART 3 echo task.
 *        The task exits when ESC character (ASCII = 27) is received.
 *        The task is using synchronous read/write to UART
 */
static void prv_Uart3_rts_cts_flow_ctrl_echo_Task( void *pvParameters )
{
        char c=0;
        uint32_t bytes;
        ad_uart_handle_t uart3_h;

        uart3_h = ad_uart_open(&uart3_uart_conf);                               /* Open the UART with the desired configuration    */
        ASSERT_ERROR(uart3_h != NULL);                                          /* Check if the UART1 opened OK */

        do {
                bytes = ad_uart_read(uart3_h, &c, 1, OS_EVENT_FOREVER);         /* Wait for one char synchronously                 */
                if (bytes > 0) {                                                /* if there is a successful read...                */
                        ad_uart_write(uart3_h, &c, bytes);                      /*       then write back the char to UART (echo)   */
                }
        } while( c != 27 );                                                     /* Exit the task if received ESC character (ASCII=27) */

        while (ad_uart_close(uart3_h, false) == AD_UART_ERROR_CONTROLLER_BUSY); /* Wait until the UART has finished all the transactions
                                                                                 * before exiting. */

        OS_TASK_DELETE( OS_GET_CURRENT_TASK() );                        /* Delete the task before exiting. It is not allowed in
                                                                         * FreeRTOS a task to exit without being deleted from
                                                                         * the OS's queues */
}

/**
 * @brief Initialize the peripherals domain after power-up.
 *
 */
static void periph_init(void)
{
        /* since the UART adapter is used, all the GPIOs configuration for the UARTs
         * is done by the adapters. Also the latch/unlatch is handled by the adapter too.
         * The UART configuration is in the platform_devices.c and the necessary declarations
         * of the three UART configuration instances are in the platform_devices.h
         */
}

/**
 * @brief Hardware Initialization
 */
static void prvSetupHardware( void )
{
        /* Init hardware */
        pm_system_init(periph_init);
}

/**
 * @brief Malloc fail hook
 */
void vApplicationMallocFailedHook( void )
{
        /* vApplicationMallocFailedHook() will only be called if
        configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
        function that will get called if a call to OS_MALLOC() fails.
        OS_MALLOC() is called internally by the kernel whenever a task, queue,
        timer or semaphore is created.  It is also called by various parts of the
        demo application.  If heap_1.c or heap_2.c are used, then the size of the
        heap available to OS_MALLOC() is defined by configTOTAL_HEAP_SIZE in
        FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
        to query the size of free heap space that remains (although it does not
        provide information on how the remaining heap might be fragmented). */
        ASSERT_ERROR(0);
}

/**
 * @brief Application idle task hook
 */
void vApplicationIdleHook( void )
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

#if (dg_configTRACK_OS_HEAP == 1)
        OS_BASE_TYPE i = 0;
        OS_BASE_TYPE uxMinimumEverFreeHeapSize;

        // Generate raw status information about each task.
        UBaseType_t uxNbOfTaskEntries = uxTaskGetSystemState(pxTaskStatusArray,
                                                        mainMAX_NB_OF_TASKS, &ulTotalRunTime);

        for (i = 0; i < uxNbOfTaskEntries; i++) {
                /* Check Free Stack*/
                OS_BASE_TYPE uxStackHighWaterMark;

                uxStackHighWaterMark = uxTaskGetStackHighWaterMark(pxTaskStatusArray[i].xHandle);
                OS_ASSERT(uxStackHighWaterMark >= mainMIN_STACK_GUARD_SIZE);
        }

        /* Check Minimum Ever Free Heap against defined guard. */
        uxMinimumEverFreeHeapSize = xPortGetMinimumEverFreeHeapSize();
        OS_ASSERT(uxMinimumEverFreeHeapSize >= mainTOTAL_HEAP_SIZE_GUARD);
#endif /* (dg_configTRACK_OS_HEAP == 1) */

#if dg_configUSE_WDOG
        sys_watchdog_notify(idle_task_wdog_id);
#endif
}

/**
 * @brief Application stack overflow hook
 */
void vApplicationStackOverflowHook( OS_TASK pxTask, char *pcTaskName )
{
        ( void ) pcTaskName;
        ( void ) pxTask;

        /* Run time stack overflow checking is performed if
        configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
        function is called if a stack overflow is detected. */
        ASSERT_ERROR(0);
}

/**
 * @brief Application tick hook
 */
void vApplicationTickHook( void )
{
}


