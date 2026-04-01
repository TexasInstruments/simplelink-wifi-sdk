/*
 * Copyright (c) 2026, Texas Instruments Incorporated
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
 *  ======== CANCC35XX.c ========
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <ti/drivers/CAN.h>
#include <ti/drivers/can/common/CANDCAN.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/Power.h>

#include <ti/drivers/utils/StructRingBuf.h>
#include <ti/drivers/utils/Math.h>

#include <ti/drivers/dpl/ClockP.h>
#include <ti/drivers/dpl/HwiP.h>

#include <third_party/dcan/DCAN.h>

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/can.h)
#include DeviceFamily_constructPath(inc/hw_ints.h)
#include DeviceFamily_constructPath(inc/hw_memmap.h)
#include DeviceFamily_constructPath(inc/hw_types.h)

/* In CC35xx hw_ints.h, DCAN_IRQ0 line is named as SWINT_0. */

#define INT_CAN_LINE0_IRQ_REQ INT_EVT_SWINT0_REQ

/* Host clock frequency in kHz. */
#define HOST_CLOCK_FREQUENCY 80000UL

/* CAN subsystem clock divider */
#define CANCC35XX_CANSS_CLK_DIVIDER CANSS_CLK_DIVIDE_BY_2

/* DCAN message RAM size. */
#define CANCC35XX_MRAM_SIZE 4096U

/* DCAN message RAM base address. */

#define CANCC35XX_MRAM_BASE (DCAN_BASE + CANCC35XX_MRAM_SIZE)

/* Hwi object for handling CAN interrupt */
static HwiP_Struct canHwi;

/* Counter for number of times Rx Ring buffer was full when there was a Rx
 * message available in Rx FIFO0/1 resulting in a lost message. This can be used
 * to fine tune the size of the Rx ring buffer when the CAN bus is under maximum
 * load.
 */
static volatile uint32_t rxRingBufFullCnt = 0U;

/* Tracks the maximum usage of the Rx ring buffer to allow tuning of the Rx ring
 * buffer size when the CAN bus is under maximum load and CPU utilization is
 * high.
 */
static volatile int32_t rxRingBufMaxCnt = 0U;

/* Tracks the maximum fill level of the Rx FIFOs to allow tuning of the Rx FIFO
 * size when the CAN bus is under maximum load and CPU utilization is high.
 * This array is indexed by the Rx FIFO number.
 */
static volatile uint8_t rxFifoMaxFillLevel[2U] = {0U, 0U};

static DCAN_TxBufElement txElem;
static DCAN_RxBufElement rxElem;

/* Default device-specific message RAM configuration:
 *  - Each standard filter element occupies 4 bytes.
 *  - Each extended filter element occupies 8 bytes.
 *  - Each Rx/Tx buffer occupies 72 bytes when CAN FD is enabled or 16 bytes
 *    for classic CAN.
 *  - Each Tx Event occupies 8 bytes.
 */
const CAN_MsgRamConfig CANCC35XX_defaultMsgRamConfig = {
    .stdFilterNum       = 0U,
    .extFilterNum       = 0U,
    .stdMsgIDFilterList = NULL,
    .extMsgIDFilterList = NULL,

    .rxFifoNum[0U]  = 30U,
    .rxFifoNum[1U]  = 0U,
    .rxBufNum       = 3U,
    .txBufNum       = 3U,
    .txFifoQNum     = 20U,
    .txFifoQMode    = 1U, /* Tx Queue mode */
    .txEventFifoNum = 8U,
};

/* Externs */
static int_fast16_t CANCC35XXXX_init(const CAN_Config *config);
extern void CANCC35XXXX_close(CAN_Handle handle);

/* Forward declarations */
void CANCC35XX_hwiFxn(uintptr_t arg);
void CANCC35XX_irqHandler(void *arg);
static bool CANCC35XX_isRxStructRingBufFull(CAN_Handle handle);
static void CANCC35XX_handleRxFifo(CAN_Handle handle, uint32_t fifoNum);
static void CANCC35XX_handleRxBuf(CAN_Handle handle);
static int_fast16_t CANCC35XX_setBitRate(const CAN_Config *config);
static int_fast16_t CANCC35XX_init(const CAN_Config *config,
                                   const CAN_MsgRamConfig *msgRamConfig,
                                   const CAN_BitRateTimingRaw *bitTiming,
                                   uint32_t tsPrescaler);
static void CANCC35XX_enableLoopback(bool externalModeEnable);

/* Definitions for extern functions defined in DCAN.h */

void DCAN_writeReg(uint32_t offset, uint32_t value)
{
    uint32_t addr;

    /* Determine if the offset is for a DCAN register or message RAM */
    if (offset < CANCC35XX_MRAM_BASE)
    {
        /* DCAN register: add the offset to CAN peripheral base address */
        addr = DCAN_BASE + offset;
    }
    else
    {
        /* DCAN message RAM location: directly use the offset as the address */
        addr = offset;
    }

    HWREG(addr) = value;
}

/*
 *  ======== DCAN_getMRAMOffset ========
 */
uint32_t DCAN_getMRAMOffset(void)
{
    return (CANCC35XX_MRAM_BASE);
}

/*
 *  ======== DCAN_writeMsgRam ========
 */
void DCAN_writeMsgRam(uint32_t offset, const uint8_t *src, size_t numBytes)
{
    (void)memcpy((void *)offset, src, numBytes);
}

/*
 *  ======== DCAN_readReg ========
 */

uint32_t DCAN_readReg(uint32_t offset)
{
    uint32_t addr = offset;
    uint32_t ret  = 0;

    if (addr < CANCC35XX_MRAM_BASE)
    {
        addr += DCAN_BASE;
    }

    ret = HWREG(addr);

    return ret;
}

/*
 *  ======== DCAN_readMsgRam ========
 */
void DCAN_readMsgRam(uint8_t *dst, uint32_t offset, size_t numBytes)
{
    (void)memcpy(dst, (void *)offset, numBytes);
}

/*
 *   ========= Initialize CC35XX DCAN peripheral ========
 */

static int_fast16_t CANCC35XXXX_init(const CAN_Config *config)
{
    /* Disable clock as calling Power_setDependency only enables clock and
     * does not configure clock source and enable RAM paper spin.
     */

    HWREG(DCAN_BASE + DCAN_O_CLKCFG) = 0;

    /* Wait until bits are reset. */

    while (HWREG(DCAN_BASE + DCAN_O_CLKCFG) &
                 (DCAN_CLKCFG_CLKEN | DCAN_CLKCFG_RAMEN | DCAN_CLKCFG_CLKSEL_HOST_DIV2_CLK))
    {}

    /* Enable clock for the CAN peripheral, enable RAM paper spin
     * and select host clock as clock source.
     */

    HWREG(DCAN_BASE + DCAN_O_CLKCFG) = (DCAN_CLKCFG_CLKEN | DCAN_CLKCFG_RAMEN | DCAN_CLKCFG_CLKSEL_HOST_DIV2_CLK);

    /* Wait until the clock enable operation has succeeded. */

    while (!(HWREG(DCAN_BASE + DCAN_O_CLKCFG) & DCAN_CLKCFG_CLKEN)) {}

    return CAN_STATUS_SUCCESS;
}

void CANCC35XXXX_close(CAN_Handle handle)
{
    (void)handle;
}

void CANCC35XX_hwiFxn(uintptr_t arg)
{
    CANCC35XX_irqHandler((void *)arg);
}

/*
 *  ======== CANCC35XX_irqHandler ========
 */
void CANCC35XX_irqHandler(void *arg)
{
    CAN_Handle handle  = (CAN_Handle)arg;
    CAN_Object *object = (CAN_Object *)handle->object;
    int32_t rxCnt;
    DCAN_ProtocolStatus protStatus;
    DCAN_TxFifoQStatus fifoQStatus;
    DCAN_TxEventFifoStatus txEventFifoStatus;
    static uint32_t canIntStatus = 0;
    static uint32_t event        = 0;
    static uint32_t intStatus    = 0;
    uint32_t txOccurred          = 0U;

    /* Get the masked CAN interrupt line 0 status */
    canIntStatus = CANGetMaskedIntStatus(CAN_INT_LINE0);

    /* Check DCAN interrupt line 0 event status */
    if ((canIntStatus & CAN_INT_INTL0) != 0U)
    {
        /* Get the masked DCAN interrupt status */

        intStatus = (uint32_t)DCAN_getIntStatus();

        intStatus = intStatus & object->intMask;

        DCAN_clearIntStatus(intStatus);

        CANClearInt(CAN_INT_LINE0, CAN_INT_INTL0);

        HwiP_clearInterrupt(INT_CAN_LINE0_IRQ_REQ);

        if ((intStatus & DCAN_INT_SRC_BUS_OFF_STATUS) != 0U)
        {
            DCAN_getProtocolStatus(&protStatus);

            /* Node is Bus Off when transmit error count >= 256 */
            if (protStatus.busOffStatus == 1U)
            {
                /* Error recovery - normal operation will resume after 129 occurrences of Bus Idle */
                DCAN_setOpMode(DCAN_OPERATION_MODE_NORMAL);
                event = CAN_EVENT_BUS_OFF;
            }
            else
            {
                event = CAN_EVENT_BUS_ON;
            }

            /* Bus Off interrupt is always enabled regardless of event mask
             * so we must check the event mask before executing the callback.
             */
            if ((event & object->eventMask) != 0U)
            {
                /* Call the event callback function provided by the application */
                object->eventCbk(handle, event, 0U, object->userArg);
            }
        }

        if ((intStatus & DCAN_INT_SRC_ERR_PASSIVE) != 0U)
        {
            DCAN_getProtocolStatus(&protStatus);

            if (protStatus.errPassive == 1U)
            {
                /* Node is Error Passive when either transmit or receiver error count >= 128 */
                event = CAN_EVENT_ERR_PASSIVE;
            }
            else
            {
                /* Node is Error Active when both transmit and receiver error count < 128 */
                event = CAN_EVENT_ERR_ACTIVE;
            }

            if ((event & object->eventMask) != 0U)
            {
                /* Call the event callback function provided by the application */
                object->eventCbk(handle, event, 0U, object->userArg);
            }
        }

        if ((intStatus & DCAN_INT_SRC_RX_FIFO0_NEW_MSG) != 0U)
        {
            CANCC35XX_handleRxFifo(handle, DCAN_RX_FIFO_NUM_0);
        }

        if ((intStatus & DCAN_INT_SRC_RX_FIFO1_NEW_MSG) != 0U)
        {
            CANCC35XX_handleRxFifo(handle, DCAN_RX_FIFO_NUM_1);
        }

        if ((intStatus & DCAN_INT_SRC_DEDICATED_RX_BUFF_MSG) != 0U)
        {
            CANCC35XX_handleRxBuf(handle);
        }

        if ((intStatus & DCAN_INT_SRC_TRANS_COMPLETE) != 0U)
        {
            /* Read TX buffer transmission status if the Tx finished event mask
             * is set so it can be provided to the event callback.
             */

            if ((object->eventMask & CAN_EVENT_TX_FINISHED) != 0U)
            {
                txOccurred = DCAN_getTxBufTransmissionStatus();
            }

            if ((object->txFifoQNum != 0U) && (StructRingBuf_getCount(&object->txStructRingBuf) > 0))
            {
                DCAN_getTxFifoQStatus(&fifoQStatus);

                if (fifoQStatus.fifoFull == 0U)
                {
                    /* Return value can be ignored since count was checked */
                    (void)StructRingBuf_get(&object->txStructRingBuf, &txElem);

                    /* Return value can be ignored as the FIFO is not full */
                    (void)CAN_write(handle, &txElem);
                }
            }

            /* Source Tx complete interrupt is always enabled if the Tx ring
             * buffer size is non-zero so we must check the event mask before
             * executing the callback.
             */
            if ((object->eventMask & CAN_EVENT_TX_FINISHED) != 0U)
            {
                /* Call the event callback function provided by the application */
                object->eventCbk(handle, CAN_EVENT_TX_FINISHED, txOccurred, object->userArg);
            }
        }

        if ((intStatus & DCAN_INT_SRC_TX_EVT_FIFO_ELEM_LOST) != 0U)
        {
            if ((object->eventMask & CAN_EVENT_TX_EVENT_LOST) != 0U)
            {
                /* Call the event callback function provided by the application */
                object->eventCbk(handle, CAN_EVENT_TX_EVENT_LOST, 0U, object->userArg);
            }
        }

        if ((intStatus & DCAN_INT_SRC_TX_EVT_FIFO_NEW_ENTRY) != 0U)
        {
            DCAN_getTxEventFifoStatus(&txEventFifoStatus);

            if ((object->eventMask & CAN_EVENT_TX_EVENT_AVAIL) != 0U)
            {
                /* Call the event callback function provided by the application */
                object->eventCbk(handle, CAN_EVENT_TX_EVENT_AVAIL, txEventFifoStatus.fillLvl, object->userArg);
            }
        }

        if ((intStatus & DCAN_INT_SRC_RX_MASK) != 0U)
        {
            event = CAN_EVENT_RX_DATA_AVAIL;

            rxCnt = StructRingBuf_getCount(&object->rxStructRingBuf);

            /* Update the maximum usage of the Rx ring buffer */
            if (rxCnt > rxRingBufMaxCnt)
            {
                rxRingBufMaxCnt = rxCnt;
            }

            if (rxCnt > 0)
            {
                /* Call the event callback function provided by the application */
                object->eventCbk(handle, CAN_EVENT_RX_DATA_AVAIL, (uint32_t)rxCnt, object->userArg);
            }
        }

        if ((intStatus & DCAN_INT_SRC_RX_FIFO0_MSG_LOST) != 0U)
        {
            /* Try to read messages from Rx FIFO 0 to free buffers to avoid losing additional messages */
            CANCC35XX_handleRxFifo(handle, DCAN_RX_FIFO_NUM_0);

            /* Call the event callback function provided by the application */
            object->eventCbk(handle, CAN_EVENT_RX_FIFO_MSG_LOST, 0U, object->userArg);
        }

        if ((intStatus & DCAN_INT_SRC_RX_FIFO1_MSG_LOST) != 0U)
        {
            /* Try to read messages from Rx FIFO 1 to free buffers to avoid losing additional messages */
            CANCC35XX_handleRxFifo(handle, DCAN_RX_FIFO_NUM_1);

            /* Call the event callback function provided by the application */
            object->eventCbk(handle, CAN_EVENT_RX_FIFO_MSG_LOST, 1U, object->userArg);
        }

        CANSSSetEndOfInt(CANSS_EOI_DCAN0);
    }
}

/*
 *  ======== CANCC35XX_isRxStructRingBufFull ========
 */
static bool CANCC35XX_isRxStructRingBufFull(CAN_Handle handle)
{
    CAN_Object *object = (CAN_Object *)handle->object;
    bool isFull        = StructRingBuf_isFull(&object->rxStructRingBuf);

    if (isFull)
    {
        rxRingBufFullCnt++;

        if ((CAN_EVENT_RX_RING_BUFFER_FULL & object->eventMask) != 0U)
        {
            /* Call the event callback function provided by the application */
            object->eventCbk(handle, CAN_EVENT_RX_RING_BUFFER_FULL, rxRingBufFullCnt, object->userArg);
        }
    }

    return isFull;
}

/*
 *  ======== CANCC35XX_handleRxFifo ========
 */
static void CANCC35XX_handleRxFifo(CAN_Handle handle, uint32_t fifoNum)
{
    CAN_Object *object           = (CAN_Object *)handle->object;
    DCAN_RxFifoStatus fifoStatus = {0};

    DCAN_getRxFifoStatus(fifoNum, &fifoStatus);

    if (fifoStatus.fillLvl == 0U)
    {
        /* If a new message is received between the time DCAN_IR is cleared and
         * the Rx FIFO status is read, the New Message flag will be set which
         * will cause the interrupt to be pended again. However, there may be no
         * messages available (fill level will be zero) so this handler should
         * return early in that case.
         */
        return;
    }

    /* Track the maximum fill level of the Rx FIFOs to allow tuning of the Rx FIFO
     * size when the CAN bus is under maximum load.
     */
    if (fifoStatus.fillLvl > rxFifoMaxFillLevel[fifoNum])
    {
        rxFifoMaxFillLevel[fifoNum] = (uint8_t)fifoStatus.fillLvl;
    }

    if (!CANCC35XX_isRxStructRingBufFull(handle))
    {
        DCAN_readRxFifo(fifoNum, fifoStatus.getIdx, &rxElem);

        /* Return value can be ignored since ring buffer is not full */
        (void)StructRingBuf_put(&object->rxStructRingBuf, &rxElem);

        fifoStatus.fillLvl--;

        while ((fifoStatus.fillLvl > 0U) && !CANCC35XX_isRxStructRingBufFull(handle))
        {
            fifoStatus.getIdx++;

            /* Check for rollover */
            if (fifoStatus.getIdx >= object->rxFifoNum[fifoNum])
            {
                fifoStatus.getIdx = 0U;
            }

            DCAN_readRxFifo(fifoNum, fifoStatus.getIdx, &rxElem);

            /* Return value can be ignored since ring buffer is not full */
            (void)StructRingBuf_put(&object->rxStructRingBuf, &rxElem);

            fifoStatus.fillLvl--;
        }

        /* Write the last buffer index read to acknowledge the sequence of
         * messages read above. This single write is used instead of
         * acknowledging each message individually to minimize latency caused by
         * the special handling of register writes for errata SYS_211. Return
         * value can be ignored since the inputs are known to be valid.
         */
        (void)DCAN_setRxFifoAck(fifoNum, fifoStatus.getIdx);
    }
}

/*
 *  ======== CANCC35XX_handleRxBuf ========
 */
static void CANCC35XX_handleRxBuf(CAN_Handle handle)
{
    CAN_Object *object                      = (CAN_Object *)handle->object;
    DCAN_RxNewDataStatus clearNewDataStatus = {0U};
    DCAN_RxNewDataStatus newDataStatus;
    uint32_t bufNum;

    DCAN_getNewDataStatus(&newDataStatus);

    /* Check for Rx messages in buffers 0-31 */
    if (newDataStatus.statusLow != 0U)
    {
        for (bufNum = 0U; bufNum < Math_MIN(object->rxBufNum, 32U); bufNum++)
        {
            if ((newDataStatus.statusLow & ((uint32_t)1U << bufNum)) != 0U)
            {
                if (!CANCC35XX_isRxStructRingBufFull(handle))
                {
                    DCAN_readRxMsg(DCAN_MEM_TYPE_BUF, bufNum, &rxElem);

                    (void)StructRingBuf_put(&object->rxStructRingBuf, &rxElem);
                }

                clearNewDataStatus.statusLow |= ((uint32_t)1U << bufNum);
            }
        }
    }

    if (object->rxBufNum > 32U)
    {
        /* Check for Rx messages in buffers 32-63 */
        if (newDataStatus.statusHigh != 0U)
        {
            for (bufNum = 0U; bufNum < Math_MIN((object->rxBufNum - 32U), 32U); bufNum++)
            {
                if ((newDataStatus.statusHigh & ((uint32_t)1U << bufNum)) != 0U)
                {
                    if (!CANCC35XX_isRxStructRingBufFull(handle))
                    {
                        DCAN_readRxMsg(DCAN_MEM_TYPE_BUF, (bufNum + 32U), &rxElem);

                        (void)StructRingBuf_put(&object->rxStructRingBuf, &rxElem);
                    }

                    clearNewDataStatus.statusHigh |= ((uint32_t)1U << bufNum);
                }
            }
        }
    }

    DCAN_clearNewDataStatus(&clearNewDataStatus);
}

/*
 *  ======== CANCC35XX_setBitRate ========
 *  This function is hard-coded for 80 MHz DCAN clock from clock tree.
 *  The 80 MHz input clock to the DCAN peripheral is further divided
 *  by 2 to give a 40MHz clock.
 */
static int_fast16_t CANCC35XX_setBitRate(const CAN_Config *config)
{
    const CAN_HWAttrs *hwAttrs     = config->hwAttrs;
    int_fast16_t status            = CAN_STATUS_SUCCESS;
    DCAN_BitTimingParams bitTiming = {0};

    /* NOTE: Add 1 to each programmed bit time to get functional value and +1 for sync segment.
     *    Bit Time = TSEG1 + TSEG2 + 1
     *    Bit Rate = (DCAN clock / Prescaler) / (Bit Time)
     *    Sample Point % = ((TSEG1 + 1) / (Bit Time)) * 100
     * - All TSEG1 and TSEG2 values above refer to the functional values
     *
     * Sampling Point % was chosen to be < 80 according to CiA 601 CAN FD Node
     * and System Design, Part 3 System Design Recommendation v1.0.0.
     */
    switch (hwAttrs->nominalBitRate)
    {
        case 125000U:
            /* 125kbps nominal with 40MHz clk and 75% sample point: ((40E6 / 2) / (119 + 40 + 1) = 125E3) */
            bitTiming.nomRatePrescaler  = DCAN_PRESCALER(2U);
            bitTiming.nomTimeSeg1       = 118U;
            bitTiming.nomTimeSeg2       = 39U;
            bitTiming.nomSynchJumpWidth = 39U; /* typically set equal to seg 2 */
            break;

        case 250000U:
            /* 250kbps nominal with 40MHz clk and 75% sample point: ((40E6 / 1) / (119 + 40 + 1) = 250E3) */
            bitTiming.nomRatePrescaler  = DCAN_PRESCALER(1U);
            bitTiming.nomTimeSeg1       = 118U;
            bitTiming.nomTimeSeg2       = 39U;
            bitTiming.nomSynchJumpWidth = 39U; /* typically set equal to seg 2 */
            break;

        case 500000U:
            /* 500kbps nominal with 40MHz clk and 75% sample point ((40E6 / 1) / (59 + 20 + 1) = 500E3) */
            bitTiming.nomRatePrescaler  = DCAN_PRESCALER(1U);
            bitTiming.nomTimeSeg1       = 58U;
            bitTiming.nomTimeSeg2       = 19U;
            bitTiming.nomSynchJumpWidth = 19U; /* typically set equal to seg 2 */

            break;

        case 1000000U:
            /* 1Mbps nominal with 40MHz clk and 75% sample point ((40E6 / 1) / (29 + 10 + 1) = 1E6) */
            bitTiming.nomRatePrescaler  = DCAN_PRESCALER(1U);
            bitTiming.nomTimeSeg1       = 28U;
            bitTiming.nomTimeSeg2       = 9U;
            bitTiming.nomSynchJumpWidth = 9U; /* typically set equal to seg 2 */
            break;

        default:
            status = CAN_STATUS_ERROR;
            break;
    }

    if (status == CAN_STATUS_SUCCESS)
    {
        if (DCAN_setBitTime(&bitTiming) != DCAN_STATUS_SUCCESS)
        {
            status = CAN_STATUS_ERROR;
        }
    }

    return status;
}

/*
 *  ======== CANCC35XX_init ========
 */
static int_fast16_t CANCC35XX_init(const CAN_Config *config,
                                   const CAN_MsgRamConfig *msgRamConfig,
                                   const CAN_BitRateTimingRaw *bitTiming,
                                   uint32_t tsPrescaler)
{
    const CAN_HWAttrs *hwAttrs     = config->hwAttrs;
    CAN_Object *object             = config->object;
    int_fast16_t status            = CAN_STATUS_SUCCESS;
    DCAN_ConfigParams configParams = {0U};
    DCAN_InitParams initParams     = {0U};
    DCAN_RxNewDataStatus newDataStatus;
    const CAN_MsgRamConfig *tempMsgRamConfig;

    /* DCAN SW init mode should be set already but explicitly set it again */
    DCAN_setOpMode(DCAN_OPERATION_MODE_SW_INIT);

    /* Clear all CAN subsystem interrupt enables */
    CANSSDisableInt(CANSS_INT_EXT_TIMESTAMP_OVERFLOW);

    /* Clear all pending CAN subsystem interrupts */
    CANSSClearInt(CANSS_INT_EXT_TIMESTAMP_OVERFLOW);

    /* Clear interrupt masks */
    CANClearIntMask(CAN_INT_LINE0, CAN_INT_MASK);
    CANClearIntMask(CAN_INT_LINE1, CAN_INT_MASK);

    /* Clear all interrupts */
    CANClearInt(CAN_INT_LINE0, CAN_INT_MASK);
    CANClearInt(CAN_INT_LINE1, CAN_INT_MASK);

    /* Set CAN functional clock divider */
    CANSSSetClockDivider(CANCC35XX_CANSS_CLK_DIVIDER);

    /* Set external timestamp counter prescaler */
    CANSSSetExtTimestampPrescaler(tsPrescaler);

    /* Enable external timestamp counter */
    CANSSSetControl(CANSS_CTRL_EXT_TIMESTAMP_EN);

    /* Set CAN IRQ interrupt mask for DCAN Interrupt line 0 events */
    CANSetIntMask(CAN_INT_LINE0, CAN_INT_INTL0);

    if (DCAN_init(&initParams) != DCAN_STATUS_SUCCESS)
    {
        status = CAN_STATUS_ERROR;
    }

    if (status == CAN_STATUS_SUCCESS)
    {
        /* CAN FD requires using "external" timestamp value. "External" means
         * external to DCAN IP.
         */
        configParams.tsSelect = DCAN_TSCC_COUNTER_EXTERNAL;

        /* Reject remote frames since they are rarely used and do not even exist
         * in the CAN FD format.
         */
        configParams.filterConfig.rrfs = 1U;
        configParams.filterConfig.rrfe = 1U;

        if (hwAttrs->rejectNonMatchingMsgs)
        {
            /* Reject incoming messages that do not match a filter, the default
             * is to accept them into Rx FIFO0.
             */
            configParams.filterConfig.anfs = DCAN_GFC_NM_REJECT;
            configParams.filterConfig.anfe = DCAN_GFC_NM_REJECT;
        }

        if (DCAN_config(&configParams) != DCAN_STATUS_SUCCESS)
        {
            status = CAN_STATUS_ERROR;
        }
    }

    if (status == CAN_STATUS_SUCCESS)
    {
        /* Translate the provided CAN event mask to the DCAN interrupt mask */
        object->intMask = CANDCAN_getInterruptMask(object->eventMask);

        /* Always enable transmit complete IRQ if there is a Tx FIFO/Queue
         * and the Tx ring buffer size is non-zero.
         */
        if ((object->txFifoQNum != 0U) && (hwAttrs->txRingBufSize != 0U))
        {
            object->intMask |= (uint32_t)DCAN_INT_SRC_TRANS_COMPLETE;
        }

        DCAN_setIntLineSel(object->intMask, DCAN_INT_LINE_NUM_0);
        DCAN_enableInt(object->intMask);
        DCAN_enableIntLine(DCAN_INT_LINE_NUM_0);

        if ((object->intMask & DCAN_INT_SRC_TRANS_COMPLETE) != 0U)
        {
            /* Enable transmission interrupt for all buffers */
            DCAN_enableTxBufTransInt(0xFFFFFFFFU);
        }

        if (bitTiming != NULL)
        {
            status = CANDCAN_setBitTimingRaw(bitTiming);
        }
        else
        {
            status = CANCC35XX_setBitRate(config);
        }
    }

    if (status == CAN_STATUS_SUCCESS)
    {
        if (msgRamConfig == NULL)
        {
            /* If msg RAM config is NULL use default */
            tempMsgRamConfig = &CANCC35XX_defaultMsgRamConfig;
        }
        else
        {
            tempMsgRamConfig = msgRamConfig;
        }

        /* Copy config attributes needed for run-time */
        object->txBufNum       = tempMsgRamConfig->txBufNum;
        object->txFifoQNum     = tempMsgRamConfig->txFifoQNum;
        object->txEventFifoNum = tempMsgRamConfig->txEventFifoNum;
        object->rxBufNum       = tempMsgRamConfig->rxBufNum;
        object->rxFifoNum[0]   = tempMsgRamConfig->rxFifoNum[0];
        object->rxFifoNum[1]   = tempMsgRamConfig->rxFifoNum[1];

        /* Setup message RAM sections and filters */
        status = CANDCAN_configMsgRam(tempMsgRamConfig, CANCC35XX_MRAM_SIZE);
    }

    if (status == CAN_STATUS_SUCCESS)
    {
        DCAN_clearIntStatus(object->intMask);
        newDataStatus.statusLow  = 0xFFFFFFFFU;
        newDataStatus.statusHigh = 0xFFFFFFFFU;
        DCAN_clearNewDataStatus(&newDataStatus);
    }

    return status;
}

/*
 *  ======== CAN_initDevice ========
 */
int_fast16_t CAN_initDevice(uint_least8_t index, CAN_Params *params)
{
    const CAN_Config *config   = &CAN_config[index];
    const CAN_HWAttrs *hwAttrs = config->hwAttrs;
    HwiP_Handle hwiHandle      = NULL;
    HwiP_Params hwiParams;
    int_fast16_t status;

    /* Enable clock for DCAN peripheral */
    Power_setDependency(PowerWFF3_PERIPH_DCAN);

    /* Device variant initialization */
    status = CANCC35XXXX_init(config);

    if (status != CAN_STATUS_SUCCESS)
    {
        return status;
    }

    /* Wait for memory initialization to complete */
    while ((CANSSGetStatus() & CANSS_STATUS_MEM_INIT_DONE) == 0U) {}

    status = CANCC35XX_init(config, params->msgRamConfig, params->bitTiming, params->tsPrescaler);

    if (status == CAN_STATUS_SUCCESS)
    {
        /* Setup HWI handler */
        HwiP_Params_init(&hwiParams);
        hwiParams.arg      = (uintptr_t)config;
        hwiParams.priority = hwAttrs->intPriority;
        hwiHandle          = HwiP_construct(&canHwi, INT_CAN_LINE0_IRQ_REQ, CANCC35XX_hwiFxn, &hwiParams);
    }

    if (hwiHandle != NULL)
    {
        /* Configure IOs */
        GPIO_setConfigAndMux(hwAttrs->rxPin, GPIO_CFG_IN_PU, hwAttrs->rxPinMux);
        GPIO_setConfigAndMux(hwAttrs->txPin, GPIO_CFG_IN_PU, hwAttrs->txPinMux);

        DCAN_setOpMode(DCAN_OPERATION_MODE_NORMAL);

        /* Disallow sleep since CAN messages could be received from the external
         * CAN transceiver at any time.
         */
        Power_setConstraint(PowerWFF3_DISALLOW_SLEEP);
    }
    else
    {
        /* Disable clock for DCAN peripheral */
        Power_releaseDependency(PowerWFF3_PERIPH_DCAN);

        /* Device variant cleanup */
        CANCC35XXXX_close((CAN_Handle)config);

        status = CAN_STATUS_ERROR;
    }

    return status;
}

/*
 *  ======== CAN_close ========
 */
void CAN_close(CAN_Handle handle)
{
    CAN_Object *object         = handle->object;
    const CAN_HWAttrs *hwAttrs = handle->hwAttrs;

    /* Set DCAN SW init mode to disable Rx & Tx from CAN bus */
    DCAN_setOpMode(DCAN_OPERATION_MODE_SW_INIT);

    HwiP_destruct(&canHwi);

    GPIO_resetConfig(hwAttrs->rxPin);
    GPIO_resetConfig(hwAttrs->txPin);

    /* Disable clock for DCAN peripheral */
    Power_releaseDependency(PowerWFF3_PERIPH_DCAN);

    /* Release power dependency. */
    Power_releaseConstraint(PowerWFF3_DISALLOW_SLEEP);

    /* Device variant cleanup */
    CANCC35XXXX_close(handle);

    object->isOpen = false;
}

/*
 *  ======== CANCC35XX_enableLoopback ========
 */
static void CANCC35XX_enableLoopback(bool externalModeEnable)
{
    DCAN_setOpMode(DCAN_OPERATION_MODE_SW_INIT);

    if (externalModeEnable)
    {
        DCAN_enableLoopbackMode(DCAN_LPBK_MODE_EXTERNAL);
    }
    else
    {
        DCAN_enableLoopbackMode(DCAN_LPBK_MODE_INTERNAL);
    }

    DCAN_setOpMode(DCAN_OPERATION_MODE_NORMAL);
}

/*
 *  ======== CAN_enableLoopbackExt ========
 */
int_fast16_t CAN_enableLoopbackExt(CAN_Handle handle)
{
    (void)handle; /* unused arg */

    CANCC35XX_enableLoopback(true);

    return CAN_STATUS_SUCCESS;
}

/*
 *  ======== CAN_enableLoopbackInt ========
 */
int_fast16_t CAN_enableLoopbackInt(CAN_Handle handle)
{
    (void)handle; /* unused arg */

    CANCC35XX_enableLoopback(false);

    return CAN_STATUS_SUCCESS;
}

/*
 *  ======== CAN_disableLoopback ========
 */
int_fast16_t CAN_disableLoopback(CAN_Handle handle)
{
    (void)handle; /* unused arg */

    DCAN_setOpMode(DCAN_OPERATION_MODE_SW_INIT);

    DCAN_disableLoopbackMode();

    DCAN_setOpMode(DCAN_OPERATION_MODE_NORMAL);

    return CAN_STATUS_SUCCESS;
}

/*
 *  ======== CAN_getBitTiming ========
 */
void CAN_getBitTiming(CAN_Handle handle, CAN_BitTimingParams *bitTiming, uint32_t *clkFreq)
{
    (void)handle; /* unused arg */

    DCAN_getBitTime(bitTiming);

    /* Shift the oscillator frequency by the divider macro value to calculate
     * the CAN functional clock frequency.
     */
    *clkFreq = HOST_CLOCK_FREQUENCY >> CANCC35XX_CANSS_CLK_DIVIDER;
}
