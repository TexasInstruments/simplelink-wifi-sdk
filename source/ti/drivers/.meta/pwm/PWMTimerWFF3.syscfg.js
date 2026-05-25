/*
 * Copyright (c) 2024 Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 *  ======== PWMTimerWFF3.syscfg.js ========
 */

"use strict";

/* get Common /ti/drivers utility functions */
let Common = system.getScript("/ti/drivers/Common.js");
let logError = Common.logError;

/*
 *  ======== devSpecific ========
 *  Device-specific extensions to be added to base PWM configuration
 */
let devSpecific = {
    config: [
        {
            name        : "prescalerDivider",
            displayName : "Timer Prescaler Divider",
            description : "Timer prescaler division factor",
            longDescription : "The prescaler of the underlying timer "
                + "can divide the incoming system clock to the timer "
                + "counter forcing an extended PWM signal period with "
                + "decreased precision. A value of 1 means division by "
                + "1. Division factor = value. Min. value is 1. Max. "
                + "value is 256.",
            default     : 1
        },
        {
            name        : "timerChannelNo",
            displayName : "Timer Channel Number",
            description : "GPTimer channel (0-3) used as the PWM output pin",
            longDescription : "Each GPTimer peripheral has four independent "
                + "capture/compare channels. Multiple PWM instances may share "
                + "the same GPTimer provided each selects a unique channel.",
            default     : 0,
            options     : [
                { name: 0, displayName: "Channel 0" },
                { name: 1, displayName: "Channel 1" },
                { name: 2, displayName: "Channel 2" },
                { name: 3, displayName: "Channel 3" }
            ]
        }
    ],

    sharedModuleInstances: sharedModuleInstances,

    filterHardware: filterHardware,

    /* override device-specific templates */
    templates: {
        boardc: "/ti/drivers/pwm/PWMTimerWFF3.Board.c.xdt",
        boardh: "/ti/drivers/pwm/PWMTimer.Board.h.xdt"
    }

};

/*
 *  ========= filterHardware ========
 *  Check 'component' signals for compatibility with GPT
 *
 *  param component - hardware object describing signals and
 *                    resources they're attached to
 *  returns Boolean indicating whether or not to allow the component to
 *           be assigned to an instance's $hardware config
 */
function filterHardware(component)
{
    for (let sig in component.signals) {
        let type = component.signals[sig].type;
        if (Common.typeMatches(type, ["GPT"])) {
            return true;
        }
    }

    return false;
}

/*
 *  ======== sharedModuleInstances ========
 *  Returns the GPTimer instance shared across PWM instances. Declaring this
 *  as a shared sub-instance allows multiple PWM instances to reference the
 *  same hardware timer peripheral, each assigned to a distinct channel.
 */
function sharedModuleInstances(inst)
{
    let sharedInstances = new Array();

    sharedInstances.push({
        name        : "timerObject",
        displayName : "GPTimer Instance",
        moduleName  : "/ti/drivers/GPTimer",
        hidden      : false,
        collapsed   : true
    });

    return sharedInstances;
}

function validate(inst, validation, $super)
{
    let prescalerDiv = inst.prescalerDivider;
    let message;

    if ($super.validate) {
        $super.validate(inst, validation);
    }

    if ((prescalerDiv <= 0) || (prescalerDiv > 256)) {
        message = 'Prescaler divider must be at least 1 and not above 256';
        logError(validation, inst, "prescalerDivider", message);
    }

    /* Flag duplicate channel assignments on the same GPTimer instance */
    let PWM = system.modules["/ti/drivers/PWM"];
    if (PWM) {
        for (let other of PWM.$instances) {
            if (other === inst) continue;
            if (other.timerObject.$name === inst.timerObject.$name &&
                other.timerChannelNo   === inst.timerChannelNo) {
                message = 'Channel ' + inst.timerChannelNo +
                    ' is already used by ' + other.$name +
                    ' on the same GPTimer instance';
                logError(validation, inst, "timerChannelNo", message);
                break;
            }
        }
    }
}

/*
 *  ======== extend ========
 *  Extends a base exports object to include any device specifics
 *
 *  This function is invoked by the generic PWM module to
 *  allow us to augment/override as needed for the WFF3.
 */
function extend(base)
{
    /* display which driver implementation can be used */
    base = Common.addImplementationConfig(base, "PWM", null,
        [{name: "PWMTimerWFF3"}], null);

    /* override base validate */
    devSpecific.validate = function (inst, validation)
    {
        return validate(inst, validation, base);
    };

    /* merge and overwrite base module attributes */
    let result = Object.assign({}, base, devSpecific);

    /* concatenate device-specific configs */
    result.config = base.config.concat(devSpecific.config);

    return result;
}

/*
 *  ======== exports ========
 *  Export device-specific extensions to base exports
 */
exports = {
    /* required function, called by base PWM module */
    extend: extend
};
