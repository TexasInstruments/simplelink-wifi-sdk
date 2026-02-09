/*
 * Copyright (c) 2024-2025, Texas Instruments Incorporated - http://www.ti.com
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
 *  ======== sps.syscfg.js ========
 */

"use strict";

/* get Common /ti/drivers utility functions */
let Common      = system.getScript("/ti/drivers/Common.js");

/* get device ID */
const deviceId = system.deviceData.deviceId;

const moduleDesc = `
This module is used to enable Secure Protected Storage (SPS). SPS protects the data that is stored in the flash by encrypting it using the AESECB cryptographic algorithm. The data is encrypted before it is written to the flash and decrypted when it is read from the flash.`;

/*
 *  ======== addStaticModules ========
 */
function addStaticModules(inst)
{
    let forcedModules = [];
    if (deviceId.match(/CC35/)) {
        /* SPS requires PSA Key Store module */
        forcedModules.push("CryptoKeyKeyStore_PSA");
    }
    return Common.autoForceModules(forcedModules)();
}

let base = {
    displayName: "Secure Protected Storage",
    description: "Secure Protected Storage",
    longDescription: moduleDesc,
    defaultInstanceName: "CONFIG_SPS_",
    alwaysShowLongDescription : true,
    maxInstances: 1,
    templates : {
        boardh      : "/ti/common/templates/sps_config.h.xdt"
    },
    moduleStatic: {
        config: [
            {
                name        : "enableSPS",
                displayName : "Enable SPS feature",
                description : "Enabling SPS feature will encrypt data to be stored in flash",
                default     : false,
                onChange    : (inst, ui) => {}
            }
        ],
        modules: addStaticModules
    }
};

exports = base;