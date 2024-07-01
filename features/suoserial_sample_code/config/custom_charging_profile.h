/**
 ****************************************************************************************
 *
 * @file custom_charging_profile.h
 *
 * @brief Custom configuration file for charging profile.
 *
 * Copyright (C) 2018-2024 Renesas Electronics Corporation and/or its affiliates
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
#ifndef CUSTOM_CHARGING_PROFILE_H_
#define CUSTOM_CHARGING_PROFILE_H_

#if (dg_configUSE_SYS_CHARGER == 1)
static const sys_charger_configuration_t sys_charger_conf = {
        .ctrl_flags = HW_CHARGER_CTRL_ENABLE_DIE_TEMP_PROTECTION                        |
                      HW_CHARGER_CTRL_RESUME_FROM_DIE_PROTECTION_STATE                  |
                      HW_CHARGER_CTRL_RESUME_FROM_ERROR_STATE                           |
                      HW_CHARGER_CTRL_HALT_CHARGE_TIMERS_ON_TEMP_PROTECTION_STATES,

         /* Tbat monitoring settings */
        .tbat_monitor_mode = HW_CHARGER_TBAT_MONITOR_MODE_PERIODIC_FSM_ON,

        /* IRQ settings */
        .irq_ok_mask = HW_CHARGER_FSM_IRQ_OK_ALL ^
                       (HW_CHARGER_FSM_IRQ_OK_MASK(TBAT_STATUS_UPDATE)  |
                        HW_CHARGER_FSM_IRQ_OK_MASK(CV_TO_CC)            |
                        HW_CHARGER_FSM_IRQ_OK_MASK(CC_TO_CV)),
        .irq_nok_mask = HW_CHARGER_FSM_IRQ_NOK_ALL,

        /* Voltage settings */
        .ovp_level = HW_CHARGER_V_LEVEL_4900,
        .replenish_v_level = HW_CHARGER_V_LEVEL_4000,
        .precharged_v_thr = HW_CHARGER_V_LEVEL_3000,
        .cv_level = HW_CHARGER_V_LEVEL_4200,

        /* Current settings */
        .eoc_i_thr = HW_CHARGER_I_EOC_PERCENT_LEVEL_10,
        .precharge_cc_level = HW_CHARGER_I_PRECHARGE_LEVEL_10,
        .cc_level = HW_CHARGER_I_LEVEL_120,

        /* Default JEITA voltage settings */
        .jeita_ovp_cool_level = HW_CHARGER_V_LEVEL_4480,
        .jeita_ovp_warm_level = HW_CHARGER_V_LEVEL_4460,
        .jeita_replenish_v_cool_level = HW_CHARGER_V_LEVEL_4020,
        .jeita_replenish_v_warm_level = HW_CHARGER_V_LEVEL_4000,
        .jeita_precharged_v_cool_thr = HW_CHARGER_V_LEVEL_3150,
        .jeita_precharged_v_warm_thr = HW_CHARGER_V_LEVEL_3100,
        .jeita_cv_cool_level = HW_CHARGER_V_LEVEL_4200,
        .jeita_cv_warm_level = HW_CHARGER_V_LEVEL_4220,

        /* Default JEITA current settings */
        .jeita_precharge_cc_cool_level = HW_CHARGER_I_PRECHARGE_LEVEL_1_5,
        .jeita_precharge_cc_warm_level = HW_CHARGER_I_PRECHARGE_LEVEL_1_0,
        .jeita_cc_cool_level = HW_CHARGER_I_LEVEL_30,
        .jeita_cc_warm_level = HW_CHARGER_I_LEVEL_25,

        /* Default Tbat limits */
        .bat_temp_cold_limit = HW_CHARGER_BAT_TEMP_LIMIT_0,
        .bat_temp_cool_limit = HW_CHARGER_BAT_TEMP_LIMIT_10,
        .bat_temp_warm_limit = HW_CHARGER_BAT_TEMP_LIMIT_35,
        .bat_temp_hot_limit =  HW_CHARGER_BAT_TEMP_LIMIT_45,

        /* Tdie settings */
        .die_temp_limit = HW_CHARGER_DIE_TEMP_LIMIT_90,

        /* Default charging timeout settings */
        .max_precharge_timeout = 0x708,         /*  30min */
        .max_cc_charge_timeout = 0x1C20,        /* 120min */
        .max_cv_charge_timeout = 0x1C20,        /* 120min */
        .max_total_charge_timeout = 0x3F48      /* 270min */
};
#endif /* (dg_configUSE_SYS_CHARGER == 1) */

#endif /* CUSTOM_CHARGING_PROFILE_H_ */
