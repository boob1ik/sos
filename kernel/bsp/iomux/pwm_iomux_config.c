/*
 * Copyright (C) 2012, [Your Company Here] All Rights Reserved.
 * IT IS EXPECTED THAT THIS TEXT BE REPLACED WITH THE BOARD SOFTWARE
 * PROVIDER'S COPYRIGHT INFORMATION. THIS TEXT WILL BE DISPLAYED AT
 * THE TOP OF ALL SOURCE FILES GENERATED FOR THIS BOARD DESIGN.
*/

// File: pwm_iomux_config.c

/* ------------------------------------------------------------------------------
 * <auto-generated>
 *     This code was generated by a tool.
 *     Runtime Version:3.4.0.4
 *
 *     Changes to this file may cause incorrect behavior and will be lost if
 *     the code is regenerated.
 * </auto-generated>
 * ------------------------------------------------------------------------------
*/

#include "iomux_config.h"
#include "registers/regsiomuxc.h"
#include "registers/regspwm.h"
#include "io.h"

void pwm_iomux_config(int instance)
{
    switch (instance)
    {
        case HW_PWM1:
            return pwm1_iomux_config();

        case HW_PWM2:
            return pwm2_iomux_config();

        case HW_PWM3:
            return pwm3_iomux_config();

        case HW_PWM4:
            return pwm4_iomux_config();

    }
}

