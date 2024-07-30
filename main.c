/*
 * Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 */
 /** @file : main.c
 *
 * This is a tester application for wifi enterprise security
  *
 */
#include <stdio.h>
#include "cyhal.h"
#include "cybsp.h"
#include <cy_retarget_io.h>
#include <cybsp_wifi.h>
#include "command_console.h"
#include "wifi_utility.h"
#include "cy_wcm.h"
#include "cyabs_rtos.h"
#include "ent_sec_utility.h"
#include <inttypes.h>

#define THREAD_STACK (4*1024)
static cy_thread_t nxs_thread;
static uint64_t nxs_stack[(THREAD_STACK)/sizeof(uint64_t)];

/* Current supported erange is 2-5 */
#ifndef CY_ENT_WATCHDOG_INTERVAL_SEC
#define CY_ENT_WATCHDOG_INTERVAL_SEC    (5)
#endif

/* wcm parameters */
static cy_wcm_config_t wcm_config;

#define CONSOLE_COMMAND_MAX_PARAMS     (24)
#define CONSOLE_COMMAND_MAX_LENGTH     (90)
#define CONSOLE_COMMAND_HISTORY_LENGTH (6)

const char* console_delimiter_string = " ";

static char command_buffer[CONSOLE_COMMAND_MAX_LENGTH];
static char command_history_buffer[CONSOLE_COMMAND_MAX_LENGTH * CONSOLE_COMMAND_HISTORY_LENGTH];

#define CMD_CONSOLE_MAX_WIFI_RETRY_COUNT 15
#define IP_STR_LEN                       16

#define CY_RSLT_ERROR                    ( -1 )

static cy_rslt_t command_console_add_command(void) {

    cy_command_console_cfg_t console_cfg;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    console_cfg.serial             = (void *)&cy_retarget_io_uart_obj;
    console_cfg.line_len           = sizeof(command_buffer);
    console_cfg.buffer             = command_buffer;
    console_cfg.history_len        = CONSOLE_COMMAND_HISTORY_LENGTH;
    console_cfg.history_buffer_ptr = command_history_buffer;
    console_cfg.delimiter_string   = console_delimiter_string;
    console_cfg.params_num         = CONSOLE_COMMAND_MAX_PARAMS;
    console_cfg.thread_priority    = CY_RTOS_PRIORITY_NORMAL;
    console_cfg.delimiter_string   = " ";

    /* Initialize command console library */
    result = cy_command_console_init(&console_cfg);
    if ( result != CY_RSLT_SUCCESS )
    {
        printf ("Error in initializing command console library : %ld \n", (long)result);
        goto error;
    }

    /* Initialize Wi-Fi utility and add Wi-Fi commands */
    result = wifi_utility_init();
    if ( result != CY_RSLT_SUCCESS )
    {
        printf ("Error in initializing command console library : %ld \n", (long)result);
        goto error;
    }
    return CY_RSLT_SUCCESS;

error:
    return CY_RSLT_ERROR;

}

static void console_task(cy_thread_arg_t arg)
{
    cy_rslt_t res;

    /* Initialize wcm */
    wcm_config.interface = CY_WCM_INTERFACE_TYPE_AP_STA;
    res = cy_wcm_init(&wcm_config);
    if(res != CY_RSLT_SUCCESS)
    {
        printf("Wi-Fi Connection Manager initialization failed! Error code: 0x%08" PRIx32 "\n", (uint32_t)res);
        return;
    }
    printf("Wi-Fi Connection Manager initialized.\r\n");

    command_console_add_command();

    /* Initialize Enterprise Security utility commands */
    ent_utility_init();

    /* Configure watchdog interval */
    thread_ap_watchdog_ConfigureTime(CY_ENT_WATCHDOG_INTERVAL_SEC);

    while(1)
    {
        cy_rtos_delay_milliseconds(500);
    }
}


int main(void)
{
    cy_rslt_t result ;

    /* Initialize the board support package */
    result = cybsp_init() ;
    if( result != CY_RSLT_SUCCESS)
    {
        printf("cybsp_init failed %ld\n", (long)result);
        return CY_RSLT_ERROR;
    }
    CY_ASSERT(result == CY_RSLT_SUCCESS) ;
    /* Enable global interrupts */
    __enable_irq();

#ifdef COMPONENT_CAT5
    /* For H1CP, the BTSS sleep is enabled by default.
     * command-console will not work with sleep enabled as wake on uart is not enabled.
     * Once wake on uart is enabled, the sleep lock can be removed.
     * enterprise tester application will not operate in low power mode until wake on uart is enabled.
     */
    cyhal_syspm_lock_deepsleep();
#endif


    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    /* \x1b[2J\x1b[;H - ANSI ESC sequence to clear screen. */
    printf("\x1b[2J\x1b[;H");
    printf("**********************************************\r\n");
    printf("Enterprise security Tester App : %s\r\n", (ENT_TESTER_APP_VERSION));
    printf("**********************************************\r\n");

    result = cy_rtos_thread_create(&nxs_thread,
                                   &console_task,
                                   "ConsoleTask",
                                   &nxs_stack,
                                   THREAD_STACK,
                                   CY_RTOS_PRIORITY_LOW,
                                   0);

    if (result != CY_RSLT_SUCCESS)
    {
        printf("Main thread creation failed \r\n");
        CY_ASSERT(0);
    }

    while(1)
    {
        cy_rtos_delay_milliseconds(500);
    }

    return 0;
}
