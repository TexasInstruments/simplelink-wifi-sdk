/*
 * Copyright (c) 2023-2026, Texas Instruments Incorporated
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
 *  ======== DCAN.c ========
 */
#include <stdint.h>

#include <third_party/dcan/DCAN.h>
#include <third_party/dcan/inc/DCAN_reg.h>

/*
 * Max values for various register fields.
 */
#define DCAN_DBTP_DSJW_MAX   (0xFU)
#define DCAN_DBTP_DTSEG2_MAX (0xFU)
#define DCAN_DBTP_DTSEG1_MAX (0x1FU)
#define DCAN_DBTP_DBRP_MAX   (0x1FU)

#define DCAN_NBTP_NSJW_MAX   (0x7FU)
#define DCAN_NBTP_NTSEG2_MAX (0x7FU)
#define DCAN_NBTP_NTSEG1_MAX (0xFFU)
#define DCAN_NBTP_NBRP_MAX   (0x1FFU)

#define DCAN_RWD_WDC_MAX (0xFFU)

#define DCAN_TDCR_TDCF_MAX (0x7FU)
#define DCAN_TDCR_TDCO_MAX (0x7FU)

#define DCAN_TSCC_TCP_MAX (0xFU)
#define DCAN_TOCC_TOP_MAX (0xFFFFU)

/*
 * Mask and shift for Tx Buffers elements.
 */
/* Tx Buffer Element Word 0 */
#define DCAN_TX_BUFFER_ELEM_XID_SHIFT (0U)
#define DCAN_TX_BUFFER_ELEM_XID_MASK  (0x1FFFFFFFU)
#define DCAN_TX_BUFFER_ELEM_SID_SHIFT (18U)
#define DCAN_TX_BUFFER_ELEM_SID_MASK  (0x1FFC0000U)
#define DCAN_TX_BUFFER_ELEM_RTR_SHIFT (29U)
#define DCAN_TX_BUFFER_ELEM_RTR_MASK  (0x20000000U)
#define DCAN_TX_BUFFER_ELEM_XTD_SHIFT (30U)
#define DCAN_TX_BUFFER_ELEM_XTD_MASK  (0x40000000U)
#define DCAN_TX_BUFFER_ELEM_ESI_SHIFT (31U)
#define DCAN_TX_BUFFER_ELEM_ESI_MASK  (0x80000000U)
/* Tx Buffer Element Word 1 */
#define DCAN_TX_BUFFER_ELEM_DLC_SHIFT (16U)
#define DCAN_TX_BUFFER_ELEM_DLC_MASK  (0x000F0000U)
#define DCAN_TX_BUFFER_ELEM_EFC_SHIFT (23U)
#define DCAN_TX_BUFFER_ELEM_EFC_MASK  (0x00800000U)
#define DCAN_TX_BUFFER_ELEM_MM_SHIFT  (24U)
#define DCAN_TX_BUFFER_ELEM_MM_MASK   (0xFF000000U)

/*
 * Mask and shift for Rx Buffers elements.
 */
/* Rx Buffer Element Word 0 */
#define DCAN_RX_BUFFER_ELEM_XID_SHIFT  (0U)
#define DCAN_RX_BUFFER_ELEM_XID_MASK   (0x1FFFFFFFU)
#define DCAN_RX_BUFFER_ELEM_SID_SHIFT  (18U)
#define DCAN_RX_BUFFER_ELEM_SID_MASK   (0x1FFC0000U)
#define DCAN_RX_BUFFER_ELEM_RTR_SHIFT  (29U)
#define DCAN_RX_BUFFER_ELEM_RTR_MASK   (0x20000000U)
#define DCAN_RX_BUFFER_ELEM_XTD_SHIFT  (30U)
#define DCAN_RX_BUFFER_ELEM_XTD_MASK   (0x40000000U)
#define DCAN_RX_BUFFER_ELEM_ESI_SHIFT  (31U)
#define DCAN_RX_BUFFER_ELEM_ESI_MASK   (0x80000000U)
/* Rx Buffer Element Word 1 */
#define DCAN_RX_BUFFER_ELEM_RXTS_SHIFT (0U)
#define DCAN_RX_BUFFER_ELEM_RXTS_MASK  (0x0000FFFFU)
#define DCAN_RX_BUFFER_ELEM_DLC_SHIFT  (16U)
#define DCAN_RX_BUFFER_ELEM_DLC_MASK   (0x000F0000U)
#define DCAN_RX_BUFFER_ELEM_FIDX_SHIFT (24U)
#define DCAN_RX_BUFFER_ELEM_FIDX_MASK  (0x7F000000U)
#define DCAN_RX_BUFFER_ELEM_ANMF_SHIFT (31U)
#define DCAN_RX_BUFFER_ELEM_ANMF_MASK  (0x80000000U)

/*
 * Mask and shift for Standard Message ID Filter Elements.
 */
#define DCAN_STD_ID_FILTER_SFID2_SHIFT (0U)
#define DCAN_STD_ID_FILTER_SFID2_MASK  (0x000003FFU)
#define DCAN_STD_ID_FILTER_SFID1_SHIFT (16U)
#define DCAN_STD_ID_FILTER_SFID1_MASK  (0x03FF0000U)
#define DCAN_STD_ID_FILTER_SFEC_SHIFT  (27U)
#define DCAN_STD_ID_FILTER_SFEC_MASK   (0x38000000U)
#define DCAN_STD_ID_FILTER_SFT_SHIFT   (30U)
#define DCAN_STD_ID_FILTER_SFT_MASK    (0xC0000000U)

/*
 * Extended Message ID Filter Element.
 */
#define DCAN_EXT_ID_FILTER_EFID2_SHIFT (0U)
#define DCAN_EXT_ID_FILTER_EFID2_MASK  (0x1FFFFFFFU)
#define DCAN_EXT_ID_FILTER_EFID1_SHIFT (0U)
#define DCAN_EXT_ID_FILTER_EFID1_MASK  (0x1FFFFFFFU)
#define DCAN_EXT_ID_FILTER_EFEC_SHIFT  (29U)
#define DCAN_EXT_ID_FILTER_EFEC_MASK   (0xE0000000U)
#define DCAN_EXT_ID_FILTER_EFT_SHIFT   (30U)
#define DCAN_EXT_ID_FILTER_EFT_MASK    (0xC0000000U)

/*
 * Mask and shift for Tx Event FIFO elements.
 */
#define DCAN_TX_EVENT_FIFO_ELEM_ID_SHIFT  (0U)
#define DCAN_TX_EVENT_FIFO_ELEM_ID_MASK   (0x1FFFFFFFU)
#define DCAN_TX_EVENT_FIFO_ELEM_RTR_SHIFT (29U)
#define DCAN_TX_EVENT_FIFO_ELEM_RTR_MASK  (0x20000000U)
#define DCAN_TX_EVENT_FIFO_ELEM_XTD_SHIFT (30U)
#define DCAN_TX_EVENT_FIFO_ELEM_XTD_MASK  (0x40000000U)
#define DCAN_TX_EVENT_FIFO_ELEM_ESI_SHIFT (31U)
#define DCAN_TX_EVENT_FIFO_ELEM_ESI_MASK  (0x80000000U)

#define DCAN_TX_EVENT_FIFO_ELEM_TXTS_SHIFT (0U)
#define DCAN_TX_EVENT_FIFO_ELEM_TXTS_MASK  (0x0000FFFFU)
#define DCAN_TX_EVENT_FIFO_ELEM_DLC_SHIFT  (16U)
#define DCAN_TX_EVENT_FIFO_ELEM_DLC_MASK   (0x000F0000U)
#define DCAN_TX_EVENT_FIFO_ELEM_ET_SHIFT   (22U)
#define DCAN_TX_EVENT_FIFO_ELEM_ET_MASK    (0x00C00000U)
#define DCAN_TX_EVENT_FIFO_ELEM_MM_SHIFT   (24U)
#define DCAN_TX_EVENT_FIFO_ELEM_MM_MASK    (0xFF000000U)

/* Start Address bit shift for any DCAN registers containing a word-aligned
 * start address field.
 */
#define DCAN_START_ADDR_SHIFT (2U)

/*!
 *  @brief Macro to extract a field value. This macro extracts the field value
 *         from a 32-bit variable (which contains the register value).
 *         This macro does not read from actual register address, and only
 *         extracts the field from a variable.
 *
 *  @param regVal         32-bit variable containing the register value.
 *  @param REG_FIELD      Peripheral register bit field name, whose value has to
 *                        be extracted.
 */
#define DCAN_GET_FIELD(regVal, REG_FIELD) (((regVal) & (uint32_t)REG_FIELD##_MASK) >> (uint32_t)REG_FIELD##_SHIFT)

/*!
 *  @brief Macro to set a specific field value. This macro first clears the
 *         specified field value and then performs "OR" of the field value which
 *         is shifted and masked. This will set the field value at its
 *         desired position.
 *
 *  @param regVal         32-bit variable containing the register value.
 *  @param REG_FIELD      Peripheral register bit field name, to which specified
 *                        value has to be set.
 *  @param fieldVal       Value of the field which has to be set.
 */
#define DCAN_SET_FIELD(regVal, REG_FIELD, fieldVal)                    \
    ((regVal) = ((regVal) & (uint32_t)(~(uint32_t)REG_FIELD##_MASK)) | \
                ((((uint32_t)(fieldVal)) << (uint32_t)REG_FIELD##_SHIFT) & (uint32_t)REG_FIELD##_MASK))

/*!
 *  @brief This macro calls read field API for 32 bit register. It also
 *         frames the mask and shift from register field macro.
 *
 *  @param offset         Register address offset.
 *  @param REG_FIELD      Peripheral register bit field name, from which
 *                        specified bit-field value has to be read.
 *  @return Value of the bit-field
 */
#define DCAN_READ_FIELD(offset, REG_FIELD) \
    (DCAN_read_field_raw((uint32_t)(offset), ((uint32_t)REG_FIELD##_MASK), ((uint32_t)REG_FIELD##_SHIFT)))

/*!
 *  @brief This macro calls read-modify-write API for 32 bit register. It also
 *         frames the mask and shift from register field macro.
 *
 *  @param offset         Register address offset.
 *  @param REG_FIELD      Peripheral register bit field name, to which specified
 *                        value has to be set.
 *  @param fieldVal       Value of the field which has to be set.
 */
#define DCAN_MODIFY_FIELD(offset, REG_FIELD, fieldVal)    \
    (DCAN_modify_field_raw((uint32_t)(offset),            \
                           ((uint32_t)REG_FIELD##_MASK),  \
                           ((uint32_t)REG_FIELD##_SHIFT), \
                           (uint32_t)(fieldVal)))

static inline void DCAN_modify_field_raw(uint32_t offset, uint32_t mask, uint32_t shift, uint32_t value);
static inline uint32_t DCAN_read_field_raw(uint32_t offset, uint32_t mask, uint32_t shift);
static inline void DCAN_enableConfigChange(void);
static inline void DCAN_disableConfigChange(void);
static void DCAN_readMsgNoCpy(uint32_t elemAddr, DCAN_RxBufElementNoCpy *elem);
static void DCAN_readMsg(uint32_t elemAddr, DCAN_RxBufElement *elem);
static void DCAN_writeMsgNoCpy(uint32_t elemAddr, const DCAN_TxBufElementNoCpy *elem);
static void DCAN_writeMsg(uint32_t elemAddr, const DCAN_TxBufElement *elem);

/*! Payload bytes indexed by Data Length Code (DLC) field. */
static const size_t DCAN_dataSize[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

/*! Element Size (RAM words) indexed by TXESC.TBDS, RXESC.RBDS or RXESC.FnDS */
static const uint32_t DCAN_elementSizeWords[8] = {4, 5, 6, 7, 8, 10, 14, 18};

/*!
 *  @brief   This function reads a 32 bit register, modifies specific set of
 *           bits and writes back to the register.
 *
 *  @param   offset  Register address offset.
 *  @param   mask    Mask for the bit field.
 *  @param   shift   Bit field shift from LSB.
 *  @param   value   Value to be written to bit-field.
 *
 *  @note    Use DCAN_MODIFY_FIELD macro instead of this function directly
 *           to reduce static analysis violations.
 */
static inline void DCAN_modify_field_raw(uint32_t offset, uint32_t mask, uint32_t shift, uint32_t value)
{
    uint32_t regVal = DCAN_readReg(offset);
    regVal &= (uint32_t)~mask;
    regVal |= (value << shift) & mask;
    DCAN_writeReg(offset, regVal);
}

/*!
 *  @brief   This function reads a 32 bit register, masks specific set of bits
 *           and right shifts the value to LSB position.
 *
 *  @param   offset  Register address offset.
 *  @param   mask    Mask for the bit field.
 *  @param   shift   Bit field shift from LSB.
 *
 *  @return  Bit-field value (absolute value shifted to LSB position)
 *
 *  @note    Use DCAN_READ_FIELD macro instead of this function directly
 *           to reduce static analysis violations.
 */
static inline uint32_t DCAN_read_field_raw(uint32_t offset, uint32_t mask, uint32_t shift)
{
    uint32_t regVal = DCAN_readReg(offset);
    regVal          = (regVal & mask) >> shift;
    return (regVal);
}

/*
 *  ======== DCAN_enableConfigChange ========
 */
static inline void DCAN_enableConfigChange(void)
{
    uint32_t regVal;

    regVal = DCAN_readReg(DCAN_CCCR);

    /* Clock stop request must be cleared when written in Standby mode */
    DCAN_SET_FIELD(regVal, DCAN_CCCR_CSR, 0U);

    DCAN_SET_FIELD(regVal, DCAN_CCCR_CCE, 1U);

    DCAN_writeReg(DCAN_CCCR, regVal);
}

/*
 *  ======== DCAN_disableConfigChange ========
 */
static inline void DCAN_disableConfigChange(void)
{
    uint32_t regVal;

    regVal = DCAN_readReg(DCAN_CCCR);

    /* Clock stop request must be cleared when written in Standby mode */
    DCAN_SET_FIELD(regVal, DCAN_CCCR_CSR, 0U);

    DCAN_SET_FIELD(regVal, DCAN_CCCR_CCE, 0U);

    DCAN_writeReg(DCAN_CCCR, regVal);
}

/*
 *  ======== DCAN_readMsgNoCpy ========
 */
static void DCAN_readMsgNoCpy(uint32_t elemAddr, DCAN_RxBufElementNoCpy *elem)
{
    size_t dataSize;
    uint32_t regVal;
    uint32_t tempElemAddr = elemAddr;

    regVal    = DCAN_readReg(tempElemAddr);
    elem->rtr = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_RTR);
    elem->xtd = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_XTD);
    elem->esi = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_ESI);

    if (0U != elem->xtd)
    {
        elem->id = DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_XID);
    }
    else
    {
        elem->id = DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_SID);
    }

    tempElemAddr += 4U;

    regVal     = DCAN_readReg(tempElemAddr);
    elem->rxts = (uint16_t)DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_RXTS);
    elem->dlc  = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_DLC);
    elem->fidx = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_FIDX);
    elem->anmf = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_ANMF);

    tempElemAddr += 4U;

    dataSize = DCAN_dataSize[elem->dlc];

    DCAN_readMsgRam(elem->data, tempElemAddr, dataSize);
}

/*
 *  ======== DCAN_readMsg ========
 */
static void DCAN_readMsg(uint32_t elemAddr, DCAN_RxBufElement *elem)
{
    size_t dataSize;
    uint32_t regVal;
    uint32_t tempElemAddr = elemAddr;

    regVal    = DCAN_readReg(tempElemAddr);
    elem->rtr = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_RTR);
    elem->xtd = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_XTD);
    elem->esi = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_ESI);

    if (0U != elem->xtd)
    {
        elem->id = DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_XID);
    }
    else
    {
        elem->id = DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_SID);
    }

    tempElemAddr += 4U;

    regVal     = DCAN_readReg(tempElemAddr);
    elem->rxts = (uint16_t)DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_RXTS);
    elem->dlc  = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_DLC);
    elem->fidx = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_FIDX);
    elem->anmf = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_RX_BUFFER_ELEM_ANMF);

    tempElemAddr += 4U;

    dataSize = DCAN_dataSize[elem->dlc];

    DCAN_readMsgRam(elem->data, tempElemAddr, dataSize);
}

/*
 *  ======== DCAN_writeMsgNoCpy ========
 */
static void DCAN_writeMsgNoCpy(uint32_t elemAddr, const DCAN_TxBufElementNoCpy *elem)
{
    size_t dataSize;
    uint32_t regVal;
    uint32_t tempElemAddr = elemAddr;

    regVal = 0U;
    if (0U != elem->xtd)
    {
        DCAN_SET_FIELD(regVal, DCAN_TX_BUFFER_ELEM_XID, elem->id);
    }
    else
    {
        DCAN_SET_FIELD(regVal, DCAN_TX_BUFFER_ELEM_SID, elem->id);
    }
    DCAN_SET_FIELD(regVal, DCAN_TX_BUFFER_ELEM_RTR, elem->rtr);
    DCAN_SET_FIELD(regVal, DCAN_TX_BUFFER_ELEM_XTD, elem->xtd);
    DCAN_writeReg(tempElemAddr, regVal);
    tempElemAddr += 4U;

    regVal = 0U;
    DCAN_SET_FIELD(regVal, DCAN_TX_BUFFER_ELEM_DLC, elem->dlc);
    DCAN_SET_FIELD(regVal, DCAN_TX_BUFFER_ELEM_EFC, elem->efc);
    DCAN_SET_FIELD(regVal, DCAN_TX_BUFFER_ELEM_MM, elem->mm);
    DCAN_writeReg(tempElemAddr, regVal);
    tempElemAddr += 4U;

    dataSize = DCAN_dataSize[elem->dlc];

    DCAN_writeMsgRam(tempElemAddr, elem->data, dataSize);
}

/*
 *  ======== DCAN_writeMsg ========
 */
static void DCAN_writeMsg(uint32_t elemAddr, const DCAN_TxBufElement *elem)
{
    size_t dataSize;
    uint32_t regVal;
    uint32_t tempElemAddr = elemAddr;

    regVal = 0U;
    if (0U != elem->xtd)
    {
        DCAN_SET_FIELD(regVal, DCAN_TX_BUFFER_ELEM_XID, elem->id);
    }
    else
    {
        DCAN_SET_FIELD(regVal, DCAN_TX_BUFFER_ELEM_SID, elem->id);
    }
    DCAN_SET_FIELD(regVal, DCAN_TX_BUFFER_ELEM_RTR, elem->rtr);
    DCAN_SET_FIELD(regVal, DCAN_TX_BUFFER_ELEM_XTD, elem->xtd);
    DCAN_writeReg(tempElemAddr, regVal);
    tempElemAddr += 4U;

    regVal = 0U;
    DCAN_SET_FIELD(regVal, DCAN_TX_BUFFER_ELEM_DLC, elem->dlc);
    DCAN_SET_FIELD(regVal, DCAN_TX_BUFFER_ELEM_EFC, elem->efc);
    DCAN_SET_FIELD(regVal, DCAN_TX_BUFFER_ELEM_MM, elem->mm);
    DCAN_writeReg(tempElemAddr, regVal);
    tempElemAddr += 4U;

    dataSize = DCAN_dataSize[elem->dlc];

    DCAN_writeMsgRam(tempElemAddr, elem->data, dataSize);
}

/*
 *  ======== DCAN_setOpMode ========
 */
void DCAN_setOpMode(DCAN_OperationMode mode)
{
    DCAN_MODIFY_FIELD(DCAN_CCCR, DCAN_CCCR_INIT, mode);

    while (DCAN_getOpMode() != mode)
    {
        /* Wait for the mode to be set */
    }
}

/*
 *  ======== DCAN_getOpMode ========
 */
DCAN_OperationMode DCAN_getOpMode(void)
{
    return (DCAN_READ_FIELD(DCAN_CCCR, DCAN_CCCR_INIT));
}

/*
 *  ======== DCAN_init ========
 */
int_fast16_t DCAN_init(const DCAN_InitParams *initParams)
{
    int_fast16_t status = DCAN_STATUS_ERROR;
    uint32_t regVal;

    if (DCAN_RWD_WDC_MAX >= initParams->wdcPreload)
    {
        DCAN_enableConfigChange();

        /* Configure DCAN mode and controls */
        regVal = DCAN_readReg(DCAN_CCCR);
        /* Clock stop request must be cleared when written in Standby mode */
        DCAN_SET_FIELD(regVal, DCAN_CCCR_CSR, 0U);
        DCAN_SET_FIELD(regVal, DCAN_CCCR_TXP, initParams->txpEnable);
        DCAN_SET_FIELD(regVal, DCAN_CCCR_EFBI, initParams->efbi);
        DCAN_SET_FIELD(regVal, DCAN_CCCR_PXHD, initParams->pxhDisable);
        DCAN_SET_FIELD(regVal, DCAN_CCCR_DAR, initParams->darEnable);
        DCAN_writeReg(DCAN_CCCR, regVal);

        /* Configure MSG RAM watchdog counter preload value */
        DCAN_MODIFY_FIELD(DCAN_RWD, DCAN_RWD_WDC, initParams->wdcPreload);

        DCAN_disableConfigChange();

        status = DCAN_STATUS_SUCCESS;
    }

    return status;
}

/*
 *  ======== DCAN_config ========
 */
int_fast16_t DCAN_config(const DCAN_ConfigParams *config)
{
    int_fast16_t status = DCAN_STATUS_ERROR;
    uint32_t regVal;

    DCAN_enableConfigChange();

    /* Configure DCAN control registers */
    regVal = DCAN_readReg(DCAN_CCCR);
    /* Clock stop request must be cleared when written in Standby mode */
    DCAN_SET_FIELD(regVal, DCAN_CCCR_CSR, 0U);
    DCAN_SET_FIELD(regVal, DCAN_CCCR_MON, config->monEnable);
    DCAN_SET_FIELD(regVal, DCAN_CCCR_ASM, config->asmEnable);
    DCAN_writeReg(DCAN_CCCR, regVal);

    /* Configure Global Filter */
    regVal = 0;
    DCAN_SET_FIELD(regVal, DCAN_GFC_RRFE, config->filterConfig.rrfe);
    DCAN_SET_FIELD(regVal, DCAN_GFC_RRFS, config->filterConfig.rrfs);
    DCAN_SET_FIELD(regVal, DCAN_GFC_ANFE, config->filterConfig.anfe);
    DCAN_SET_FIELD(regVal, DCAN_GFC_ANFS, config->filterConfig.anfs);
    DCAN_writeReg(DCAN_GFC, regVal);

    if ((DCAN_TSCC_TCP_MAX >= config->tsPrescaler) && (DCAN_TOCC_TOP_MAX >= config->timeoutPreload))
    {
        /* Configure timestamp counter */
        regVal = 0;
        DCAN_SET_FIELD(regVal, DCAN_TSCC_TSS, config->tsSelect);
        DCAN_SET_FIELD(regVal, DCAN_TSCC_TCP, config->tsPrescaler);
        DCAN_writeReg(DCAN_TSCC, regVal);

        /* Configure timeout counter */
        regVal = 0;
        DCAN_SET_FIELD(regVal, DCAN_TOCC_TOS, config->timeoutSelect);
        DCAN_SET_FIELD(regVal, DCAN_TOCC_TOP, config->timeoutPreload);
        DCAN_writeReg(DCAN_TOCC, regVal);

        /* Enable timeout counter */
        DCAN_MODIFY_FIELD(DCAN_TOCC, DCAN_TOCC_ETOC, config->timeoutCntEnable);

        status = DCAN_STATUS_SUCCESS;
    }

    DCAN_disableConfigChange();

    return status;
}

/*
 *  ======== DCAN_getBitTime ========
 */
void DCAN_getBitTime(DCAN_BitTimingParams *bitTiming)
{
    uint32_t regVal;

    /* Read Nominal Bit Timing and Prescaler */
    regVal                       = DCAN_readReg(DCAN_NBTP);
    bitTiming->nomRatePrescaler  = DCAN_GET_FIELD(regVal, DCAN_NBTP_NBRP);
    bitTiming->nomTimeSeg1       = DCAN_GET_FIELD(regVal, DCAN_NBTP_NTSEG1);
    bitTiming->nomTimeSeg2       = DCAN_GET_FIELD(regVal, DCAN_NBTP_NTSEG2);
    bitTiming->nomSynchJumpWidth = DCAN_GET_FIELD(regVal, DCAN_NBTP_NSJW);

    /* Read Data Bit Timing and Prescaler */
    regVal                        = DCAN_readReg(DCAN_DBTP);
    bitTiming->dataRatePrescaler  = DCAN_GET_FIELD(regVal, DCAN_DBTP_DBRP);
    bitTiming->dataTimeSeg1       = DCAN_GET_FIELD(regVal, DCAN_DBTP_DTSEG1);
    bitTiming->dataTimeSeg2       = DCAN_GET_FIELD(regVal, DCAN_DBTP_DTSEG2);
    bitTiming->dataSynchJumpWidth = DCAN_GET_FIELD(regVal, DCAN_DBTP_DSJW);

    /* Read Transceiver Delay Compensation */
    regVal                    = DCAN_readReg(DCAN_TDCR);
    bitTiming->tdcConfig.tdcf = DCAN_GET_FIELD(regVal, DCAN_TDCR_TDCF);
    bitTiming->tdcConfig.tdco = DCAN_GET_FIELD(regVal, DCAN_TDCR_TDCO);
}

/*
 *  ======== DCAN_setBitTime ========
 */
int_fast16_t DCAN_setBitTime(const DCAN_BitTimingParams *bitTiming)
{
    int_fast16_t status = DCAN_STATUS_ERROR;
    uint32_t regVal;

    DCAN_enableConfigChange();

    if ((DCAN_NBTP_NSJW_MAX >= bitTiming->nomSynchJumpWidth) && (DCAN_NBTP_NTSEG2_MAX >= bitTiming->nomTimeSeg2) &&
        (DCAN_NBTP_NTSEG1_MAX >= bitTiming->nomTimeSeg1) && (DCAN_NBTP_NBRP_MAX >= bitTiming->nomRatePrescaler))
    {
        regVal = 0;
        DCAN_SET_FIELD(regVal, DCAN_NBTP_NBRP, bitTiming->nomRatePrescaler);
        DCAN_SET_FIELD(regVal, DCAN_NBTP_NTSEG1, bitTiming->nomTimeSeg1);
        DCAN_SET_FIELD(regVal, DCAN_NBTP_NTSEG2, bitTiming->nomTimeSeg2);
        DCAN_SET_FIELD(regVal, DCAN_NBTP_NSJW, bitTiming->nomSynchJumpWidth);
        DCAN_writeReg(DCAN_NBTP, regVal);

        status = DCAN_STATUS_SUCCESS;
    }

    DCAN_disableConfigChange();
    return status;
}

/*
 *  ======== DCAN_configMsgRam ========
 */
void DCAN_configMsgRam(const DCAN_MsgRamConfig *msgRamConfig)
{
    uint32_t regVal;
    uint32_t regValRXESC = 0U;

    DCAN_enableConfigChange();

    /* Configure Standard Message Filters section */
    if (0U != msgRamConfig->sidFilterListSize)
    {
        regVal = 0U;
        DCAN_SET_FIELD(regVal, DCAN_SIDFC_FLSSA, (msgRamConfig->sidFilterStartAddr >> DCAN_START_ADDR_SHIFT));
        DCAN_SET_FIELD(regVal, DCAN_SIDFC_LSS, msgRamConfig->sidFilterListSize);
        DCAN_writeReg(DCAN_SIDFC, regVal);
    }

    /* Configure Extended Message Filters section */
    if (0U != msgRamConfig->xidFilterListSize)
    {
        regVal = 0U;
        DCAN_SET_FIELD(regVal, DCAN_XIDFC_FLESA, (msgRamConfig->xidFilterStartAddr >> DCAN_START_ADDR_SHIFT));
        DCAN_SET_FIELD(regVal, DCAN_XIDFC_LSE, msgRamConfig->xidFilterListSize);
        DCAN_writeReg(DCAN_XIDFC, regVal);
    }

    /* Configure Rx FIFO 0 section */
    if (0U != msgRamConfig->rxFifo0Size)
    {
        regVal = 0U;
        DCAN_SET_FIELD(regVal, DCAN_RXF0C_F0SA, (msgRamConfig->rxFifo0StartAddr >> DCAN_START_ADDR_SHIFT));
        DCAN_SET_FIELD(regVal, DCAN_RXF0C_F0S, msgRamConfig->rxFifo0Size);
        DCAN_SET_FIELD(regVal, DCAN_RXF0C_F0WM, msgRamConfig->rxFifo0Watermark);
        DCAN_SET_FIELD(regVal, DCAN_RXF0C_F0OM, msgRamConfig->rxFifo0OpMode);
        DCAN_writeReg(DCAN_RXF0C, regVal);

        /* Configure Rx FIFO0 elements size */
        DCAN_SET_FIELD(regValRXESC, DCAN_RXESC_F0DS, msgRamConfig->rxFifo0ElemSize);
    }

    /* Configure Rx FIFO 1 section */
    if (0U != msgRamConfig->rxFifo1Size)
    {
        regVal = 0U;
        DCAN_SET_FIELD(regVal, DCAN_RXF1C_F1SA, (msgRamConfig->rxFifo1StartAddr >> DCAN_START_ADDR_SHIFT));
        DCAN_SET_FIELD(regVal, DCAN_RXF1C_F1S, msgRamConfig->rxFifo1Size);
        DCAN_SET_FIELD(regVal, DCAN_RXF1C_F1WM, msgRamConfig->rxFifo1Watermark);
        DCAN_SET_FIELD(regVal, DCAN_RXF1C_F1OM, msgRamConfig->rxFifo1OpMode);
        DCAN_writeReg(DCAN_RXF1C, regVal);

        /* Configure Rx FIFO1 elements size */
        DCAN_SET_FIELD(regValRXESC, DCAN_RXESC_F1DS, msgRamConfig->rxFifo1ElemSize);
    }

    /* Configure Rx Buffer Start Address */
    DCAN_MODIFY_FIELD(DCAN_RXBC, DCAN_RXBC_RBSA, (msgRamConfig->rxBufStartAddr >> DCAN_START_ADDR_SHIFT));
    /* Configure Rx Buffer elements size */
    DCAN_SET_FIELD(regValRXESC, DCAN_RXESC_RBDS, msgRamConfig->rxBufElemSize);
    DCAN_writeReg(DCAN_RXESC, regValRXESC);

    /* Configure Tx Event FIFO section */
    if (0U != msgRamConfig->txEventFifoSize)
    {
        regVal = 0U;
        DCAN_SET_FIELD(regVal, DCAN_TXEFC_EFSA, (msgRamConfig->txEventFifoStartAddr >> DCAN_START_ADDR_SHIFT));
        DCAN_SET_FIELD(regVal, DCAN_TXEFC_EFS, msgRamConfig->txEventFifoSize);
        DCAN_SET_FIELD(regVal, DCAN_TXEFC_EFWM, msgRamConfig->txEventFifoWatermark);
        DCAN_writeReg(DCAN_TXEFC, regVal);
    }

    /* Configure Tx Buffer and FIFO/Q section */
    if ((0U != msgRamConfig->txFifoQSize) || (0U != msgRamConfig->txBufNum))
    {
        regVal = 0U;
        DCAN_SET_FIELD(regVal, DCAN_TXBC_TBSA, (msgRamConfig->txBufStartAddr >> DCAN_START_ADDR_SHIFT));
        DCAN_SET_FIELD(regVal, DCAN_TXBC_NDTB, msgRamConfig->txBufNum);
        DCAN_SET_FIELD(regVal, DCAN_TXBC_TFQS, msgRamConfig->txFifoQSize);
        DCAN_SET_FIELD(regVal, DCAN_TXBC_TFQM, msgRamConfig->txFifoQMode);
        DCAN_writeReg(DCAN_TXBC, regVal);

        /* Configure Tx Buffer element size */
        DCAN_MODIFY_FIELD(DCAN_TXESC, DCAN_TXESC_TBDS, msgRamConfig->txBufElemSize);
    }

    DCAN_disableConfigChange();
}

/*
 *  ======== DCAN_getTxBufAddr ========
 */
static uint32_t DCAN_getTxBufAddr(uint32_t bufIdx)
{
    uint32_t elemAddr;
    uint32_t elemSize;
    uint32_t elemSizeIdx;
    uint32_t startAddr;

    startAddr = DCAN_READ_FIELD(DCAN_TXBC, DCAN_TXBC_TBSA);
    /* Shift address field to correct position */
    startAddr = (startAddr << DCAN_START_ADDR_SHIFT);

    elemSizeIdx = DCAN_READ_FIELD(DCAN_TXESC, DCAN_TXESC_TBDS);
    /* Get element size words and convert to bytes */
    elemSize    = DCAN_elementSizeWords[elemSizeIdx] << 2U;
    elemAddr    = startAddr + (elemSize * bufIdx);
    elemAddr += DCAN_getMRAMOffset();

    return elemAddr;
}

/*
 *  ======== DCAN_writeTxMsgNoCpy ========
 */
void DCAN_writeTxMsgNoCpy(uint32_t bufIdx, const DCAN_TxBufElementNoCpy *elem)
{
    uint32_t elemAddr = DCAN_getTxBufAddr(bufIdx);

    DCAN_writeMsgNoCpy(elemAddr, elem);
}

/*
 *  ======== DCAN_writeTxMsg ========
 */
void DCAN_writeTxMsg(uint32_t bufIdx, const DCAN_TxBufElement *elem)
{
    uint32_t elemAddr = DCAN_getTxBufAddr(bufIdx);

    DCAN_writeMsg(elemAddr, elem);
}

/*
 *  ======== DCAN_setTxBufAddReq ========
 */
void DCAN_setTxBufAddReq(uint32_t bufIdx)
{
    DCAN_writeReg(DCAN_TXBAR, ((uint32_t)1U << bufIdx));
}

/*
 *  ======== DCAN_getTxFifoQStatus ========
 */
void DCAN_getTxFifoQStatus(DCAN_TxFifoQStatus *fifoQStatus)
{
    uint32_t regVal = DCAN_readReg(DCAN_TXFQS);

    fifoQStatus->fifoFull = DCAN_GET_FIELD(regVal, DCAN_TXFQS_TFQF);
    fifoQStatus->putIdx   = DCAN_GET_FIELD(regVal, DCAN_TXFQS_TFQPI);
    fifoQStatus->getIdx   = DCAN_GET_FIELD(regVal, DCAN_TXFQS_TFGI);
    fifoQStatus->freeLvl  = DCAN_GET_FIELD(regVal, DCAN_TXFQS_TFFL);
}

/*
 *  ======== DCAN_getNewDataStatus ========
 */
void DCAN_getNewDataStatus(DCAN_RxNewDataStatus *newDataStatus)
{
    newDataStatus->statusLow  = DCAN_readReg(DCAN_NDAT1);
    newDataStatus->statusHigh = DCAN_readReg(DCAN_NDAT2);
}

/*
 *  ======== DCAN_clearNewDataStatus ========
 */
void DCAN_clearNewDataStatus(const DCAN_RxNewDataStatus *newDataStatus)
{
    DCAN_writeReg(DCAN_NDAT1, newDataStatus->statusLow);
    DCAN_writeReg(DCAN_NDAT2, newDataStatus->statusHigh);
}

/*
 *  ======== DCAN_readRxMsgNoCpy ========
 */
void DCAN_readRxMsgNoCpy(DCAN_MemType memType, uint32_t num, DCAN_RxBufElementNoCpy *elem)
{
    uint32_t elemAddr;
    uint32_t elemSize;
    uint32_t elemSizeIdx;
    uint32_t enableRead = 0U;
    uint32_t idx;
    uint32_t startAddr;

    if (DCAN_MEM_TYPE_BUF == memType)
    {
        startAddr   = DCAN_READ_FIELD(DCAN_RXBC, DCAN_RXBC_RBSA);
        elemSizeIdx = DCAN_READ_FIELD(DCAN_RXESC, DCAN_RXESC_RBDS);
        idx         = num;
        enableRead  = 1U;
    }
    else /* (DCAN_MEM_TYPE_FIFO == memType) */
    {
        switch (num)
        {
            case DCAN_RX_FIFO_NUM_0:
                startAddr   = DCAN_READ_FIELD(DCAN_RXF0C, DCAN_RXF0C_F0SA);
                elemSizeIdx = DCAN_READ_FIELD(DCAN_RXESC, DCAN_RXESC_F0DS);
                idx         = DCAN_READ_FIELD(DCAN_RXF0S, DCAN_RXF0S_F0GI);
                enableRead  = 1U;
                break;

            case DCAN_RX_FIFO_NUM_1:
                startAddr   = DCAN_READ_FIELD(DCAN_RXF1C, DCAN_RXF1C_F1SA);
                elemSizeIdx = DCAN_READ_FIELD(DCAN_RXESC, DCAN_RXESC_F1DS);
                idx         = DCAN_READ_FIELD(DCAN_RXF1S, DCAN_RXF1S_F1GI);
                enableRead  = 1U;
                break;

            default:
                /* Invalid option */
                break;
        }
    }

    if (0U != enableRead)
    {
        /* Shift address field to correct position */
        startAddr = (startAddr << DCAN_START_ADDR_SHIFT);
        elemSize  = DCAN_elementSizeWords[elemSizeIdx] << 2U; /* convert to bytes */
        elemAddr  = startAddr + (elemSize * idx);
        elemAddr += DCAN_getMRAMOffset();
        DCAN_readMsgNoCpy(elemAddr, elem);
    }
}

/*
 *  ======== DCAN_readRxMsg ========
 */
void DCAN_readRxMsg(DCAN_MemType memType, uint32_t num, DCAN_RxBufElement *elem)
{
    uint32_t elemAddr;
    uint32_t elemSize;
    uint32_t elemSizeIdx;
    uint32_t enableRead = 0U;
    uint32_t idx;
    uint32_t startAddr;

    if (DCAN_MEM_TYPE_BUF == memType)
    {
        startAddr   = DCAN_READ_FIELD(DCAN_RXBC, DCAN_RXBC_RBSA);
        elemSizeIdx = DCAN_READ_FIELD(DCAN_RXESC, DCAN_RXESC_RBDS);
        idx         = num;
        enableRead  = 1U;
    }
    else /* (DCAN_MEM_TYPE_FIFO == memType) */
    {
        switch (num)
        {
            case DCAN_RX_FIFO_NUM_0:
                startAddr   = DCAN_READ_FIELD(DCAN_RXF0C, DCAN_RXF0C_F0SA);
                elemSizeIdx = DCAN_READ_FIELD(DCAN_RXESC, DCAN_RXESC_F0DS);
                idx         = DCAN_READ_FIELD(DCAN_RXF0S, DCAN_RXF0S_F0GI);
                enableRead  = 1U;
                break;

            case DCAN_RX_FIFO_NUM_1:
                startAddr   = DCAN_READ_FIELD(DCAN_RXF1C, DCAN_RXF1C_F1SA);
                elemSizeIdx = DCAN_READ_FIELD(DCAN_RXESC, DCAN_RXESC_F1DS);
                idx         = DCAN_READ_FIELD(DCAN_RXF1S, DCAN_RXF1S_F1GI);
                enableRead  = 1U;
                break;

            default:
                /* Invalid option */
                break;
        }
    }

    if (0U != enableRead)
    {
        /* Shift address field to correct position */
        startAddr = (uint32_t)(startAddr << DCAN_START_ADDR_SHIFT);
        elemSize  = DCAN_elementSizeWords[elemSizeIdx] << 2U; /* convert to bytes */
        elemAddr  = startAddr + (elemSize * idx);
        elemAddr += DCAN_getMRAMOffset();
        DCAN_readMsg(elemAddr, elem);
    }
}

/*
 *  ======== DCAN_readRxFifo ========
 */
void DCAN_readRxFifo(uint32_t fifoNum, uint32_t fifoIndex, DCAN_RxBufElement *elem)
{
    uint32_t elemAddr;
    uint32_t elemSize;
    uint32_t elemSizeIdx;
    uint32_t enableRead = 0U;
    uint32_t startAddr;

    switch (fifoNum)
    {
        case DCAN_RX_FIFO_NUM_0:
            startAddr   = DCAN_READ_FIELD(DCAN_RXF0C, DCAN_RXF0C_F0SA);
            elemSizeIdx = DCAN_READ_FIELD(DCAN_RXESC, DCAN_RXESC_F0DS);
            enableRead  = 1U;
            break;

        case DCAN_RX_FIFO_NUM_1:
            startAddr   = DCAN_READ_FIELD(DCAN_RXF1C, DCAN_RXF1C_F1SA);
            elemSizeIdx = DCAN_READ_FIELD(DCAN_RXESC, DCAN_RXESC_F1DS);
            enableRead  = 1U;
            break;

        default:
            /* Invalid option */
            break;
    }

    if (0U != enableRead)
    {
        /* Shift address field to correct position */
        startAddr = (uint32_t)(startAddr << DCAN_START_ADDR_SHIFT);
        elemSize  = DCAN_elementSizeWords[elemSizeIdx] << 2U; /* convert to bytes */
        elemAddr  = startAddr + (elemSize * fifoIndex);
        elemAddr += DCAN_getMRAMOffset();
        DCAN_readMsg(elemAddr, elem);
    }
}

/*
 *  ======== DCAN_readTxEventFifo ========
 */
int_fast16_t DCAN_readTxEventFifo(DCAN_TxEventFifoElement *elem)
{
    int_fast16_t status = DCAN_STATUS_ERROR;
    uint32_t elemAddr;
    uint32_t elemSize;
    uint32_t fillLevel;
    uint32_t idx;
    uint32_t regVal;
    uint32_t startAddr;

    regVal    = DCAN_readReg(DCAN_TXEFS);
    fillLevel = DCAN_GET_FIELD(regVal, DCAN_TXEFS_EFFL);

    if (0U != fillLevel)
    {
        idx = DCAN_GET_FIELD(regVal, DCAN_TXEFS_EFGI);

        startAddr = DCAN_READ_FIELD(DCAN_TXEFC, DCAN_TXEFC_EFSA);
        elemSize  = DCAN_TX_EVENT_ELEM_SIZE;

        /* Shift address field to correct position */
        startAddr = (startAddr << DCAN_START_ADDR_SHIFT);
        elemAddr  = startAddr + (elemSize * idx);
        elemAddr += DCAN_getMRAMOffset();

        regVal    = DCAN_readReg(elemAddr);
        elem->id  = DCAN_GET_FIELD(regVal, DCAN_TX_EVENT_FIFO_ELEM_ID);
        elem->rtr = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_TX_EVENT_FIFO_ELEM_RTR);
        elem->xtd = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_TX_EVENT_FIFO_ELEM_XTD);
        elem->esi = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_TX_EVENT_FIFO_ELEM_ESI);

        elemAddr += 4U;
        regVal     = DCAN_readReg(elemAddr);
        elem->txts = (uint16_t)DCAN_GET_FIELD(regVal, DCAN_TX_EVENT_FIFO_ELEM_TXTS);
        elem->dlc  = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_TX_EVENT_FIFO_ELEM_DLC);
        elem->et   = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_TX_EVENT_FIFO_ELEM_ET);
        elem->mm   = (uint8_t)DCAN_GET_FIELD(regVal, DCAN_TX_EVENT_FIFO_ELEM_MM);

        /* Write the Event FIFO Acknowledge Index to increment the Tx Event FIFO
         * get index.
         */
        DCAN_MODIFY_FIELD(DCAN_TXEFA, DCAN_TXEFA_EFAI, idx);

        status = DCAN_STATUS_SUCCESS;
    }

    return status;
}

/*
 *  ======== DCAN_addStdMsgIDFilter ========
 */
void DCAN_addStdMsgIDFilter(uint32_t filtNum, const DCAN_StdMsgIDFilterElement *elem)
{
    uint32_t elemAddr;
    uint32_t regVal;
    uint32_t startAddr;

    startAddr = DCAN_READ_FIELD(DCAN_SIDFC, DCAN_SIDFC_FLSSA);
    /* Shift address field to correct position */
    startAddr = (startAddr << DCAN_START_ADDR_SHIFT);
    elemAddr  = startAddr + (filtNum * DCAN_STD_ID_FILTER_ELEM_SIZE);
    elemAddr += DCAN_getMRAMOffset();

    regVal = 0U;
    regVal |= (elem->sfid2 << DCAN_STD_ID_FILTER_SFID2_SHIFT);
    regVal |= (elem->sfid1 << DCAN_STD_ID_FILTER_SFID1_SHIFT);
    regVal |= (elem->sfec << DCAN_STD_ID_FILTER_SFEC_SHIFT);
    regVal |= (elem->sft << DCAN_STD_ID_FILTER_SFT_SHIFT);
    DCAN_writeReg(elemAddr, regVal);
}

/*
 *  ======== DCAN_addExtMsgIDFilter ========
 */
void DCAN_addExtMsgIDFilter(uint32_t filtNum, const DCAN_ExtMsgIDFilterElement *elem)
{
    uint32_t elemAddr;
    uint32_t regVal;
    uint32_t startAddr;

    startAddr = DCAN_READ_FIELD(DCAN_XIDFC, DCAN_XIDFC_FLESA);
    /* Shift address field to correct position */
    startAddr = (startAddr << DCAN_START_ADDR_SHIFT);
    elemAddr  = startAddr + (filtNum * DCAN_EXT_ID_FILTER_ELEM_SIZE);
    elemAddr += DCAN_getMRAMOffset();

    regVal = 0U;
    regVal |= (elem->efid1 << DCAN_EXT_ID_FILTER_EFID1_SHIFT);
    regVal |= (elem->efec << DCAN_EXT_ID_FILTER_EFEC_SHIFT);
    DCAN_writeReg(elemAddr, regVal);

    elemAddr += 4U;
    regVal = 0U;
    regVal |= (elem->efid2 << DCAN_EXT_ID_FILTER_EFID2_SHIFT);
    regVal |= (elem->eft << DCAN_EXT_ID_FILTER_EFT_SHIFT);
    DCAN_writeReg(elemAddr, regVal);
}

/*
 *  ======== DCAN_enableLoopbackMode ========
 */
void DCAN_enableLoopbackMode(DCAN_LpbkMode lpbkMode)
{
    uint32_t regVal;

    DCAN_enableConfigChange();

    regVal = DCAN_readReg(DCAN_CCCR);

    /* Clock stop request must be cleared when written in Standby mode */
    DCAN_SET_FIELD(regVal, DCAN_CCCR_CSR, 0U);

    if (DCAN_LPBK_MODE_INTERNAL == lpbkMode)
    {
        DCAN_SET_FIELD(regVal, DCAN_CCCR_MON, 1U);
    }
    else
    {
        DCAN_SET_FIELD(regVal, DCAN_CCCR_MON, 0U);
    }

    /* Enable write access to TEST reg */
    DCAN_SET_FIELD(regVal, DCAN_CCCR_TEST, 1U);

    DCAN_writeReg(DCAN_CCCR, regVal);

    DCAN_MODIFY_FIELD(DCAN_TEST, DCAN_TEST_LBCK, 1U);

    DCAN_disableConfigChange();
}

/*
 *  ======== DCAN_disableLoopbackMode ========
 */
void DCAN_disableLoopbackMode(void)
{
    uint32_t regVal;

    DCAN_enableConfigChange();

    /* Disable loopback mode */
    DCAN_MODIFY_FIELD(DCAN_TEST, DCAN_TEST_LBCK, 0U);

    regVal = DCAN_readReg(DCAN_CCCR);

    /* Clock stop request must be cleared when written in Standby mode */
    DCAN_SET_FIELD(regVal, DCAN_CCCR_CSR, 0U);

    /* Disable write access to TEST reg */
    DCAN_SET_FIELD(regVal, DCAN_CCCR_TEST, 0U);
    DCAN_SET_FIELD(regVal, DCAN_CCCR_MON, 0U);

    DCAN_writeReg(DCAN_CCCR, regVal);

    DCAN_disableConfigChange();
}

/*
 *  ======== DCAN_getProtocolStatus ========
 */
void DCAN_getProtocolStatus(DCAN_ProtocolStatus *protStatus)
{
    uint32_t regVal = DCAN_readReg(DCAN_PSR);

    protStatus->lastErrCode   = DCAN_GET_FIELD(regVal, DCAN_PSR_LEC);
    protStatus->act           = DCAN_GET_FIELD(regVal, DCAN_PSR_ACT);
    protStatus->errPassive    = DCAN_GET_FIELD(regVal, DCAN_PSR_EP);
    protStatus->warningStatus = DCAN_GET_FIELD(regVal, DCAN_PSR_EW);
    protStatus->busOffStatus  = DCAN_GET_FIELD(regVal, DCAN_PSR_BO);
    protStatus->dlec          = DCAN_GET_FIELD(regVal, DCAN_PSR_DLEC);
    protStatus->resi          = DCAN_GET_FIELD(regVal, DCAN_PSR_RESI);
    protStatus->pxe           = DCAN_GET_FIELD(regVal, DCAN_PSR_PXE);
    protStatus->tdcv          = DCAN_GET_FIELD(regVal, DCAN_PSR_TDCV);
}

/*
 *  ======== DCAN_enableInt ========
 */
void DCAN_enableInt(uint32_t intMask)
{
    uint32_t regVal = DCAN_readReg(DCAN_IE);

    regVal |= intMask;

    DCAN_writeReg(DCAN_IE, regVal);
}

/*
 *  ======== DCAN_disableInt ========
 */
void DCAN_disableInt(uint32_t intMask)
{
    uint32_t regVal = DCAN_readReg(DCAN_IE);

    regVal &= (uint32_t)~intMask;

    DCAN_writeReg(DCAN_IE, regVal);
}

/*
 *  ======== DCAN_setIntLineSel ========
 */
void DCAN_setIntLineSel(uint32_t intMask, DCAN_IntLineNum lineNum)
{
    uint32_t regVal;

    regVal = DCAN_readReg(DCAN_ILS);

    if (DCAN_INT_LINE_NUM_0 == lineNum)
    {
        /* 0 = Interrupt assigned to interrupt line 0 */
        regVal &= (uint32_t)~intMask;
    }
    else
    {
        /* 1 = Interrupt assigned to interrupt line 0 */
        regVal |= intMask;
    }

    DCAN_writeReg(DCAN_ILS, regVal);
}

/*
 *  ======== DCAN_enableIntLine ========
 */
void DCAN_enableIntLine(DCAN_IntLineNum lineNum)
{
    uint32_t mask   = ((uint32_t)1U << lineNum);
    uint32_t regVal = DCAN_readReg(DCAN_ILE);

    regVal |= mask;

    DCAN_writeReg(DCAN_ILE, regVal);
}

/*
 *  ======== DCAN_disableIntLine ========
 */
void DCAN_disableIntLine(DCAN_IntLineNum lineNum)
{
    uint32_t mask   = ((uint32_t)1U << lineNum);
    uint32_t regVal = DCAN_readReg(DCAN_ILE);

    regVal &= (uint32_t)~mask;

    DCAN_writeReg(DCAN_ILE, regVal);
}

/*
 *  ======== DCAN_getIntStatus ========
 */
uint32_t DCAN_getIntStatus(void)
{
    return (DCAN_readReg(DCAN_IR));
}

/*
 *  ======== DCAN_clearIntStatus ========
 */
void DCAN_clearIntStatus(uint32_t intMask)
{
    DCAN_writeReg(DCAN_IR, intMask);
}

/*
 *  ======== DCAN_getRxFifoStatus ========
 */
void DCAN_getRxFifoStatus(DCAN_RxFifoNum fifoNum, DCAN_RxFifoStatus *fifoStatus)
{
    uint32_t regVal;

    if (DCAN_RX_FIFO_NUM_0 == fifoNum)
    {
        regVal = DCAN_readReg(DCAN_RXF0S);
    }
    else
    {
        regVal = DCAN_readReg(DCAN_RXF1S);
    }

    /* All Rx FIFO1 status fields match Rx FIFO0 with the exception of the
     * Debug Message Status field (bits 31:30) which only exist in RXF1S
     */
    fifoStatus->fillLvl  = DCAN_GET_FIELD(regVal, DCAN_RXF0S_F0FL);
    fifoStatus->getIdx   = DCAN_GET_FIELD(regVal, DCAN_RXF0S_F0GI);
    fifoStatus->putIdx   = DCAN_GET_FIELD(regVal, DCAN_RXF0S_F0PI);
    fifoStatus->fifoFull = DCAN_GET_FIELD(regVal, DCAN_RXF0S_F0F);
    fifoStatus->msgLost  = DCAN_GET_FIELD(regVal, DCAN_RXF0S_RF0L);
}

/*
 *  ======== DCAN_setRxFifoAck ========
 */
int_fast16_t DCAN_setRxFifoAck(DCAN_RxFifoNum fifoNum, uint32_t idx)
{
    int_fast16_t status = DCAN_STATUS_ERROR;
    uint32_t numElements;

    if (DCAN_RX_FIFO_NUM_0 == fifoNum)
    {
        numElements = DCAN_READ_FIELD(DCAN_RXF0C, DCAN_RXF0C_F0S);

        if (numElements >= idx)
        {
            DCAN_MODIFY_FIELD(DCAN_RXF0A, DCAN_RXF0A_F0AI, idx);
            status = DCAN_STATUS_SUCCESS;
        }
    }
    else if (DCAN_RX_FIFO_NUM_1 == fifoNum)
    {
        numElements = DCAN_READ_FIELD(DCAN_RXF1C, DCAN_RXF1C_F1S);

        if (numElements >= idx)
        {
            DCAN_MODIFY_FIELD(DCAN_RXF1A, DCAN_RXF1A_F1AI, idx);
            status = DCAN_STATUS_SUCCESS;
        }
    }
    else
    {
        /* Do nothing */
    }

    return status;
}

/*
 *  ======== DCAN_getTxEventFifoStatus ========
 */
void DCAN_getTxEventFifoStatus(DCAN_TxEventFifoStatus *fifoStatus)
{
    uint32_t regVal = DCAN_readReg(DCAN_TXEFS);

    fifoStatus->fillLvl  = DCAN_GET_FIELD(regVal, DCAN_TXEFS_EFFL);
    fifoStatus->getIdx   = DCAN_GET_FIELD(regVal, DCAN_TXEFS_EFGI);
    fifoStatus->putIdx   = DCAN_GET_FIELD(regVal, DCAN_TXEFS_EFPI);
    fifoStatus->fifoFull = DCAN_GET_FIELD(regVal, DCAN_TXEFS_EFF);
    fifoStatus->eleLost  = DCAN_GET_FIELD(regVal, DCAN_TXEFS_TEFL);
}

/*
 *  ======== DCAN_getTxBufReqPend ========
 */
uint32_t DCAN_getTxBufReqPend(void)
{
    return (DCAN_readReg(DCAN_TXBRPAP));
}

/*
 *  ======== DCAN_cancelTxBufReq ========
 */
void DCAN_cancelTxBufReq(uint32_t bufIdx)
{
    uint32_t regVal = ((uint32_t)1U << bufIdx);

    DCAN_writeReg(DCAN_TXBCR, regVal);
}

/*
 *  ======== DCAN_getTxBufTransmissionStatus ========
 */
uint32_t DCAN_getTxBufTransmissionStatus(void)
{
    return (DCAN_readReg(DCAN_TXBTO));
}

/*
 *  ======== DCAN_getTxBufCancellationStatus ========
 */
uint32_t DCAN_getTxBufCancellationStatus(void)
{
    return (DCAN_readReg(DCAN_TXBCF));
}

/*
 *  ======== DCAN_enableTxBufTransInt ========
 */
void DCAN_enableTxBufTransInt(uint32_t bufMask)
{
    uint32_t regVal = DCAN_readReg(DCAN_TXBTIE);

    regVal |= bufMask;

    DCAN_writeReg(DCAN_TXBTIE, regVal);
}

/*
 *  ======== DCAN_disableTxBufTransInt ========
 */
void DCAN_disableTxBufTransInt(uint32_t bufMask)
{
    uint32_t regVal = DCAN_readReg(DCAN_TXBTIE);

    regVal &= (uint32_t)~bufMask;

    DCAN_writeReg(DCAN_TXBTIE, regVal);
}

/*
 *  ======== DCAN_getClkStopAck ========
 */
uint32_t DCAN_getClkStopAck(void)
{
    return (DCAN_READ_FIELD(DCAN_CCCR, DCAN_CCCR_CSA));
}

/*
 *  ======== DCAN_getTimestampCounter ========
 */
uint16_t DCAN_getTimestampCounter(void)
{
    return (uint16_t)DCAN_readReg(DCAN_TSCV);
}