/*
 * Copyright (c) 2022-2026, Texas Instruments Incorporated
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
 */

/*
 *  ======== PowerWFF3_freertos.c ========
 */

#include <stdint.h>
#include <string.h>

/* Driver header files */
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerWFF3.h>

/* Utilities header files */
#include <ti/drivers/utils/Math.h>

/* FreeRTOS header files */
#include <FreeRTOS.h>
#include <task.h>
#include <portmacro.h>

/* Log header file */
#include <ti/log/Log.h>

/* Driverlib header files */
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/systick.h)
#include DeviceFamily_constructPath(cmsis/cc35xx.h)
#include DeviceFamily_constructPath(cmsis/core/core_cm33.h)
#include DeviceFamily_constructPath(inc/hw_types.h)
#include DeviceFamily_constructPath(inc/hw_memmap.h)
#include DeviceFamily_constructPath(inc/hw_systim.h)
#include DeviceFamily_constructPath(inc/hw_rtc.h)
#include DeviceFamily_constructPath(inc/hw_hostmcu_aon.h)

/* Function prototypes */
extern int_fast16_t PowerWFF3_notify(uint_fast16_t eventType);

/* Number of microseconds between each FreeRTOS OS tick */
#define FREERTOS_TICKPERIOD_US (1000000 / configTICK_RATE_HZ)

/* Max number of ClockP ticks into the future supported by this ClockP
 * implementation.
 * Under the hood, ClockP uses the SysTimer whose events trigger immediately if
 * the compare value is less than 2^22 systimer ticks in the past
 * (4.194sec at 1us resolution). Therefore, the max number of SysTimer ticks you
 * can schedule into the future is 2^32 - 2^22 - 1 ticks (~= 4290 sec at 1us
 * resolution).
 */
#define MAX_SYSTIMER_DELTA 0xFFBFFFFFU

/* Number of CPU clock cycles per FreeRTOS tick. In other words, the FreeRTOS
 * tick period measured in CPU clock cycles.
 */
#define CPU_CLOCKS_PER_FREERTOS_TICK (configCPU_CLOCK_HZ / configTICK_RATE_HZ)

/* Number of CPU clock cycles per microsecond */
#define CPU_CLOCKS_PER_US (configCPU_CLOCK_HZ / 1000000U)

#define SYSTIMER_CHANNEL_COUNT 2U

/* This global is used to pass the expected ticks that the OS will sleep from
 * vPortSuppressTicksAndSleep() to the Power policy.
 */
static volatile uint32_t PowerWFF3_idleTimeOS = 0U;

/* Global to stash the SysTimer timeouts while we enter standby */
uint32_t sysTimerTimeouts[SYSTIMER_CHANNEL_COUNT];

/* Shift values to convert between the different resolutions of the SysTimer
 * channels. Channel 0 can technically support either 1us or 250ns. Until the
 * channel is actively used, we will hard-code it to 1us resolution to improve
 * runtime.
 */
const uint8_t sysTimerResolutionShift[SYSTIMER_CHANNEL_COUNT] = {
    0, /* 1us */
    0, /* 1us */
};

/*
 *  ======== PowerWFF3_sleepPolicy ========
 */
void PowerWFF3_sleepPolicy()
{
#if (configUSE_TICKLESS_IDLE != 0)
    eSleepModeStatus eSleep;
    uint32_t constraints;
    uint32_t ticksBefore;
    uint32_t ticksAfter;
    uint32_t sleptTicks;
    uint32_t soonestDelta;
    uint64_t osDelta;
    uint32_t sysTimerDelta;
    uint32_t sysTimerIMASK;
    uint32_t sysTimerARMSET;
    uint32_t sysTimerLoopDelta;
    uint32_t sysTimerCurrTime;
    uint32_t wakeupSourceStatus;
    uint32_t wakeupSourceMask;
    uint8_t sysTimerIndex;
    int32_t sysTickValue;      /* The SysTick counter is only 24 bits wide, so
                                * it will fit in signed 32 bits.
                                */
    uint32_t missedTicks = 0U; /* Number of missed SysTick interrupts from the
                                * time interrupts are disabled until
                                * sysTickValue is set.
                                */
    uint32_t standbyDuration;
    uintptr_t key;
    bool sleepAllowed;
    bool idleAllowed;

    key = __get_PRIMASK();
    __set_PRIMASK(1);

    /* Final check with FreeRTOS to make sure it is still OK to go to sleep... */
    eSleep = eTaskConfirmSleepModeStatus();
    if (eSleep == eAbortSleep)
    {
        __set_PRIMASK(key);
        return;
    }

    /* Check state of constraints */
    constraints  = Power_getConstraintMask();
    sleepAllowed = (constraints & (1U << PowerWFF3_DISALLOW_SLEEP)) == 0U;
    idleAllowed  = (constraints & (1U << PowerWFF3_DISALLOW_IDLE)) == 0U;

    Log_printf(LogModule_Power,
               Log_VERBOSE,
               "PowerWFF3_sleepPolicy: Sleep constraint count = %d. Idle constraint count = %d",
               Power_getConstraintCount(PowerWFF3_DISALLOW_SLEEP),
               Power_getConstraintCount(PowerWFF3_DISALLOW_IDLE));

    /* Do not allow sleep if any of the configured wakeup sources are already
     * pending/set.
     * If a bit is set in the WUREQ register, it means that the corresponding
     * wakeup source is pending/set.
     * If a bit is set in the CFGWICSNS register, it means that the
     * corresponding wakeup source is enabled.
     * If any bit is set in both registers, it means that the corresponding
     * wakeup source is both enabled and pending and we should therefore not try
     * to enter sleep.
     */
    wakeupSourceStatus = HWREG(HOSTMCU_AON_BASE + HOSTMCU_AON_O_WUREQ);
    wakeupSourceMask   = HWREG(HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGWICSNS);
    if (wakeupSourceStatus & wakeupSourceMask)
    {
        sleepAllowed = false;

        Log_printf(LogModule_Power,
            Log_VERBOSE,
            "PowerWFF3_sleepPolicy: Sleep is not allowed due to wakeup source already set. HOSTMCU_AON.WUREQ = 0x%x, HOSTMCU_AON.CFGWICSNS = 0x%x",
            wakeupSourceStatus,
            wakeupSourceMask);
    }

    /* Do quick check to see if only WFI allowed; if yes, do it now. */
    if (sleepAllowed)
    {
        /* If we are allowed to enter sleep, check whether the next timeout is
         * far enough away for it to make sense.
         */

        /* Save SysTimer IMASK to restore afterwards */
        sysTimerIMASK = HWREG(SYSTIM_BASE + SYSTIM_O_IBM);

        /* Get current armed status of all SysTimer channels */
        sysTimerARMSET = HWREG(SYSTIM_BASE + SYSTIM_O_ARMSET);

        /* Get current time in 1us resolution */
        sysTimerCurrTime = HWREG(SYSTIM_BASE + SYSTIM_O_TIME1U);

        /* Store current SysTick value, it will be used to restore SysTick
         * after waking up.
         */
        sysTickValue = SysTickGetValue();

        if ((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) != 0U)
        {
            /* The SysTick interrupt is pending. This has happened after
             * interrupts were disabled earlier in this function. It might
             * also have happened after reading the current SysTick value
             * above. So read the SysTick value again, but also the SysTimer
             * value to ensure they are read in close succession.
             * This pending interrupt means we have missed a tick, keep track
             * of that missed tick.
             * Note, do not clear the pending SysTick interrupt here, that is
             * done later after sleeping if the SysTick gets disabled.
             */
            missedTicks++;

            /* Read SysTimer and SysTick again. */
            sysTimerCurrTime = HWREG(SYSTIM_BASE + SYSTIM_O_TIME1U);
            sysTickValue     = SysTickGetValue();
        }

        if (sysTickValue == 0)
        {
            /* If SysTick is currently at 0, the next interrupt is actually a
             * full FreeRTOS tick period in the future.
             */
            sysTickValue = CPU_CLOCKS_PER_FREERTOS_TICK;
        }

        /* We only want to check the SysTimer channels if at least one of them
         * is active.
         */
        if (sysTimerIMASK != 0)
        {
            /* Set initial SysTimer delta to max possible value. It needs to be
             * this large since we will shrink it down to the soonest timeout
             * with Math_MIN() comparisons.
             */
            sysTimerDelta = 0xFFFFFFFF;

            /* Loop over all SysTimer channels and compute the soonest timeout.
             * Since the channels have different time bases (1us vs 250ns),
             * we need to shift all of that to a 1us time base to compare them.
             * If no channel is active, we will use the max timeout value
             * supported by the SysTimer.
             */
            for (sysTimerIndex = 0; sysTimerIndex < SYSTIMER_CHANNEL_COUNT; sysTimerIndex++)
            {
                if (sysTimerIMASK & (1 << sysTimerIndex))
                {
                    /* Stash SysTimer channel compare value. Read CHnCCSR to
                     * avoid clearing any pending events as side effect of
                     * reading CHnCC.
                     */
                    sysTimerTimeouts[sysTimerIndex] = HWREG(SYSTIM_BASE + SYSTIM_O_CH0CCSR + (sysTimerIndex * sizeof(uint32_t)));

                    /* Store current channel timeout in native channel
                     * resolution
                     */
                    sysTimerLoopDelta = sysTimerTimeouts[sysTimerIndex];

                    /* Convert current time from 1us to native resolution and
                     * subtract from timeout to get delta in in native channel
                     * resolution.
                     * We compute the delta in the native resolution
                     * to correctly handle wrapping and underflow at the 32-bit
                     * boundary.
                     * To simplify code paths and SRAM, we shift up the 1us
                     * resolution time stamp instead of reading out and keeping
                     * track of the 250ns time stamp. The loss of resolution for
                     * wakeup is not material as we wake up sufficiently early
                     * to handle timing jitter in the wakeup duration.
                     */
                    sysTimerLoopDelta -= sysTimerCurrTime << sysTimerResolutionShift[sysTimerIndex];

                    /* If sysTimerDelta is larger than MAX_SYSTIMER_DELTA, the
                     * compare event happened in the past and we need to abort
                     * entering sleep to handle the timeout instead of waiting
                     * a really long time.
                     */
                    if (sysTimerLoopDelta > MAX_SYSTIMER_DELTA)
                    {
                        sysTimerLoopDelta = 0;
                    }

                    /* Convert delta to 1us resolution */
                    sysTimerLoopDelta = sysTimerLoopDelta >> sysTimerResolutionShift[sysTimerIndex];

                    /* Update the smallest SysTimer delta */
                    sysTimerDelta = Math_MIN(sysTimerDelta, sysTimerLoopDelta);
                }
            }
        }
        else
        {
            /* None of the SysTimer channels are active. Use the maximum
             * SysTimer delta instead. That lets us sleep for at least this
             * long if the OS timeout is even longer.
             */
            sysTimerDelta = MAX_SYSTIMER_DELTA;
        }

        if (missedTicks >= PowerWFF3_idleTimeOS)
        {
            /* If we have already missed the expected idle time, set the OS
             * delta to 0 to avoid underflow below.
             */
            osDelta = 0;
        }
        else
        {
            /* Check soonestDelta wake time pending for FreeRTOS in 1us
             * resolution. The next tick is not necessarily a full tick period
             * in the future. To account for this, the time that has already
             * passed since the last tick is subtracted.
             * Also cap value at MAX_SYSTIMER_DELTA to avoid overflow, since
             * that is the maximum value that can be used for the standby
             * duration below.
             * Note, to make sure the multiplication doesn't result in an
             * overflow, osDelta is declared as a 64-bit value since
             * PowerWFF3_idleTimeOS can be up to the maximum value of
             * TickType_t, which can be up to an unsigned 32-bit integer, and
             * that multiplied by FREERTOS_TICKPERIOD_US can overflow 32 bits.
             */
            osDelta = (uint64_t)(PowerWFF3_idleTimeOS - missedTicks) * FREERTOS_TICKPERIOD_US -
                      ((CPU_CLOCKS_PER_FREERTOS_TICK - sysTickValue) / CPU_CLOCKS_PER_US);
            if (osDelta > MAX_SYSTIMER_DELTA)
            {
                osDelta = MAX_SYSTIMER_DELTA;
            }
        }

        /* Get soonestDelta wake time and corresponding ClockP timeout.
         * Casting osDelta to uint32_t is safe since the value is capped
         * at MAX_SYSTIMER_DELTA which fits in uint32_t.
         */
        soonestDelta = Math_MIN(sysTimerDelta, (uint32_t)osDelta);

        /* Check soonestDelta time vs sleep latency */
        if (soonestDelta > PowerWFF3_TOTALTIMESLEEP)
        {
            Log_printf(LogModule_Power,
                       Log_INFO,
                       "PowerWFF3_sleepPolicy: Entering sleep. Soonest timeout = 0x%x",
                       (soonestDelta + sysTimerCurrTime));

            /* Disable scheduling */
            PowerWFF3_schedulerDisable();

            /* Stop FreeRTOS ticks */
            SysTickDisable();

            /* Ensure the device wakes up early enough to reinitialise the
             * HW and take care of housekeeping.
             */
            soonestDelta -= PowerWFF3_WAKEDELAYSLEEP;

            /* Save SysTimer tick count before sleep */
            ticksBefore = sysTimerCurrTime;

            /* RTC channel 0 compare is automatically armed upon writing the
             * compare value. It will automatically be disarmed when it
             * triggers.
             * Note, overflow in the addition is possible, but since we
             * guarantee that soonestDelta is at most MAX_SYSTIMER_DELTA,
             * this is safe.
             */
            HWREG(RTC_BASE + RTC_O_CH0CC1U) = ticksBefore + soonestDelta;

            /* Go to sleep mode */
            Power_sleep(PowerWFF3_SLEEP);

            /* Disarm RTC compare event in case we woke up from a different
             * wakeup event. If the RTC times out after clearing RIS and the
             * pending NVIC, we will be left with a pending interrupt after
             * enabling interrupts. Which will not be gracefully handled since
             * we don't have an interrupt handler for the RTC interrupt.
             */
            HWREG(RTC_BASE + RTC_O_ARMCLR) = RTC_ARMCLR_CH0_CLR;

            /* Clear the RTC wakeup event */
            HWREG(RTC_BASE + RTC_O_ICLR) = RTC_ICLR_EV0_CLR;

            /* TODO: Determine if write to RTC and NVIC can operate out of order
             * For now, have an RTC read after the RTC write to be sure.
             */
            HWREG(RTC_BASE + RTC_O_IMASK);

            /* Clear any pending interrupt in the NVIC */
            HwiP_clearInterrupt(INT_RTC_EVENT_IRQ);

            /* When waking up from sleep, the SysTimer may not have
             * synchronised with the RTC by now. Wait for SysTimer
             * synchronisation with the RTC to complete. This should not take
             * more than one Slow Clock period.
             *
             * We need to wait both for RUN to be set and SYNCUP to go low. Any
             * other register state will cause undefined behaviour.
             */
            while (HWREG(SYSTIM_BASE + SYSTIM_O_STA) != SYSTIM_STA_VAL_RUN) {}

            /* Restore SysTimer timeouts */
            memcpy((void *)(SYSTIM_BASE + SYSTIM_O_CH0CCSR), sysTimerTimeouts, sizeof(sysTimerTimeouts));

            /* Restore SysTimer armed state. This will rearm all previously
             * armed timeouts restored above and cause any that occurred in the
             * past to trigger immediately.
             */
            HWREG(SYSTIM_BASE + SYSTIM_O_ARMSET) = sysTimerARMSET;

            /* Restore SysTimer IMASK */
            HWREG(SYSTIM_BASE + SYSTIM_O_IBM) = sysTimerIMASK;

            /* Signal clients registered for sleep wakeup notification;
             * this should be used to initialize any timing critical or IO
             * dependent hardware.
             * The callback needs to go out after the SysTimer is restored
             * such that notifications can invoke RCL and ClockP APIs if needed.
             */
            PowerWFF3_notify(PowerWFF3_AWAKE_SLEEP);

            /* Clear any pending SysTick interrupts, that might have been
             * triggered before SysTick was disabled before entering sleep.
             */
            SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

            /* Reset SysTick counter to 0 so it reloads with the remaining
             * timeout later below. This can be done without knowing the standby
             * duration, so it is done before reading the SysTimer time, to
             * reduce the time between reading the SysTimer to enabling SysTick.
             */
            SysTick->VAL = 0;

            /* Get SysTimer tick count after sleep. This should be as close to
             * the SysTickEnable() call as possible.
             */
            ticksAfter = HWREG(SYSTIM_BASE + SYSTIM_O_TIME1U);

            /* Calculate elapsed time in standby */
            standbyDuration = (ticksAfter - ticksBefore);

            /* Compute elapsed FreeRTOS tick periods in standby */
            sleptTicks = standbyDuration / FREERTOS_TICKPERIOD_US;

            /* Some precision is lost in the division when calculating
             * sleptTicks above. Use the remainder to compute new SysTick
             * counter value. A negative value will be handled in if statement
             * below.
             */
            sysTickValue -= (standbyDuration % FREERTOS_TICKPERIOD_US) * CPU_CLOCKS_PER_US;

            if (sysTickValue <= 0)
            {
                /* The SysTick counter value is computed to be negative or zero,
                 * meaning the "next" tick would be in the past (or now), which
                 * is not possible. Instead the counter value is increased by
                 * one FreeRTOS tick period to make it positive. To account for
                 * this change, sleptTicks is incremented by one.
                 */
                sysTickValue += CPU_CLOCKS_PER_FREERTOS_TICK;
                sleptTicks++;
            }

            /* Set initial period since we cannot set the counter value
             * directly.
             */
            SysTickSetPeriod(sysTickValue);

            /* Restart FreeRTOS ticks */
            SysTickEnable();

            /* Set the SysTick period back to the FreeRTOS tick period for
             * the next and all subsequent timeouts.
             * Interrupts must still be disabled in between re-enabling the
             * SysTick to force a reload of the counter and changing the period
             * back to the FreeRTOS tick period.
             * Otherwise, a small leftover counter value could cause the reload
             * value to be low enough to infinitely trigger the ISR since we
             * do not reach the line below before timing out again.
             * If the timeout was small, it will time out and pend the
             * interrupt but not vector to it. Until after the new reload value
             * is applied.
             * We will only vector to the ISR once regardless of whether the
             * counter times out more than once.
             */
            SysTickSetPeriod(CPU_CLOCKS_PER_FREERTOS_TICK);

            /* Account for missed ticks at the start of this function. */
            sleptTicks += missedTicks;

            /* Update FreeRTOS tick count for time spent in STANDBY.
             * This is done after enabling SysTick to reduce time between
             * reading SysTimer and enabling SysTick.
             * Note: FreeRTOS expects that the value passed to vTaskStepTick()
             * is at most PowerWFF3_idleTimeOS. This is true since the wakeup
             * time is selected such that we wakeup before the scheduled tick.
             * For the same reason, it is safe to cast to TickType_t here since
             * sleptTicks is at most PowerWFF3_idleTimeOS, which is at most
             * the maximum value of TickType_t.
             */
            vTaskStepTick((TickType_t)sleptTicks);

            /* Re-enable scheduling. Also re-enables interrupts.
             * This must happen only after restoring the SysTimer because
             * otherwise a Clock might be started in an ISR and then the Power
             * policy clobbers the hardware state.
             */
            PowerWFF3_schedulerRestore();

            Log_printf(LogModule_Power,
                       Log_INFO,
                       "PowerWFF3_sleepPolicy: Exiting sleep. Time after = 0x%x",
                       ticksAfter);
        }
        else if (idleAllowed)
        {
            /* If we would be allowed to enter sleep but there is not enough
             * time for it to make sense from an overhead perspective, enter
             * idle instead.
             */
            Log_printf(LogModule_Power,
                   Log_INFO,
                   "PowerWFF3_sleepPolicy: Only WFI allowed");

            __WFI();
        }
    }
    else if (idleAllowed)
    {
        /* We are not allowed to enter sleep.
         * Enter idle instead if it is allowed.
         */
        Log_printf(LogModule_Power,
                   Log_INFO,
                   "PowerWFF3_sleepPolicy: Only WFI allowed");

        __WFI();
    }

    __set_PRIMASK(key);
#endif /* configUSE_TICKLESS_IDLE != 0 */
}

/*
 *  ======== PowerWFF3_schedulerDisable ========
 */
void PowerWFF3_schedulerDisable()
{
    vTaskSuspendAll();
}

/*
 *  ======== PowerWFF3_schedulerRestore ========
 */
void PowerWFF3_schedulerRestore()
{
    xTaskResumeAll();
}

/*
 *  ======== vPortSuppressTicksAndSleep ========
 *  FreeRTOS hook function invoked when Idle when configUSE_TICKLESS_IDLE
 */
void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime)
{
    /* Stash FreeRTOS' expected idle time */
    PowerWFF3_idleTimeOS = xExpectedIdleTime;

    /*
     * call Power-driver-specified idle function, to conditionally invoke the
     * Power policy
     */
    Power_idleFunc();
}
