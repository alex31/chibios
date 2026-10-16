/*
    ChibiOS - Copyright (C) 2006-2026 Giovanni Di Sirio.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/**
 * @file    vserial.c
 * @brief   Mini serial driver over a VIO UART unit, code.
 * @details Buffered serial driver for FreeRTOS sandbox guests. The host UART
 *          FIFOs are accessed through the VIO UART calls, the data is
 *          buffered in two stream buffers:
 *          - RX: the VRQ handler moves the received data from the UART RX
 *            FIFO into the RX buffer, tasks read from the RX buffer.
 *          - TX: tasks write into the TX buffer then trigger the VRQ, the
 *            VRQ handler moves the data from the TX buffer into the UART TX
 *            FIFO.
 *          .
 *          All the UART accesses are performed by the VRQ handler, it is
 *          triggered by the host on UART events and by the writers.
 * @note    The host disables a UART event source when the event occurs,
 *          the RX source is enabled again when a read finds the RX FIFO
 *          empty, the TX source when a write finds the TX FIFO full. The
 *          VRQ handler always reads until the RX FIFO is empty and writes
 *          until the TX FIFO is full or there is no more data.
 * @note    Stream buffers allow a single reader and a single writer, the
 *          driver can be used by one reader task and one writer task at
 *          a time, else the accesses must be serialized by the application.
 *
 * @addtogroup VSERIAL
 * @{
 */

#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

#include "sbuservio.h"

#include "vserial.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/**
 * @brief   Size of the transfer chunks between buffers and UART FIFOs.
 */
#define VSERIAL_CHUNK_SIZE                  16U

/**
 * @brief   UART events enabled to trigger the VRQ.
 */
#define VSERIAL_EVENTS                      (SB_VUART_EV_ALL_ERRORS |       \
                                             SB_VUART_EV_RX_NOTEMPTY |      \
                                             SB_VUART_EV_TX_NOTFULL)

/*===========================================================================*/
/* Module exported variables.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Module local types.                                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Module local variables.                                                   */
/*===========================================================================*/

static StreamBufferHandle_t xRxBuffer;
static StreamBufferHandle_t xTxBuffer;

/**
 * @brief   Accumulated errors.
 */
static volatile uint32_t ulErrors;

/**
 * @brief   Transfer chunks, only accessed by the VRQ handler.
 * @note    The TX chunk holds the data taken from the TX buffer and not yet
 *          accepted by the UART TX FIFO.
 */
static uint8_t ucRxChunk[VSERIAL_CHUNK_SIZE];
static uint8_t ucTxChunk[VSERIAL_CHUNK_SIZE];
static size_t xTxPos;
static size_t xTxLen;

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Module VRQ handlers.                                                      */
/*===========================================================================*/

/**
 * @brief   VIO UART VRQ handler.
 */
portVRQ_HANDLER(VSERIAL_VRQ) {
  BaseType_t xWoken = pdFALSE;
  size_t xn;

  /* Collecting the errors also enables the error sources again.*/
  ulErrors |= vuartGetAndClearErrors(VSERIAL_UNIT);

  /* Emptying the RX FIFO, the data not fitting the RX buffer is lost.*/
  do {
    xn = vuartRead(VSERIAL_UNIT, ucRxChunk, VSERIAL_CHUNK_SIZE);
    if ((xn > 0U) &&
        (xStreamBufferSendFromISR(xRxBuffer, ucRxChunk, xn, &xWoken) < xn)) {
      ulErrors |= SB_VUART_EV_OVERRUN_ERR;
    }
  } while (xn == VSERIAL_CHUNK_SIZE);

  /* Filling the TX FIFO until it is full or there is no more data.*/
  while (true) {
    if (xTxPos >= xTxLen) {
      xTxPos = 0U;
      xTxLen = xStreamBufferReceiveFromISR(xTxBuffer, ucTxChunk,
                                           VSERIAL_CHUNK_SIZE, &xWoken);
      if (xTxLen == 0U) {
        break;
      }
    }

    xTxPos += vuartWrite(VSERIAL_UNIT, &ucTxChunk[xTxPos], xTxLen - xTxPos);
    if (xTxPos < xTxLen) {
      break;
    }
  }

  portYIELD_FROM_ISR(xWoken);
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the driver.
 * @details The VIO UART unit is started using its default configuration.
 *
 * @return              The operation status.
 * @retval pdPASS       if the driver has been started.
 * @retval pdFAIL       if the UART unit is not assigned by the host, failed
 *                      to start or the buffers could not be allocated.
 */
BaseType_t xVSerialStart(void) {

  if (vuartInit(VSERIAL_UNIT, NULL, 0U) != SB_VIO_RET_SUCCESS) {
    return pdFAIL;
  }

  xRxBuffer = xStreamBufferCreate(VSERIAL_RX_BUFFER_SIZE, 1U);
  xTxBuffer = xStreamBufferCreate(VSERIAL_TX_BUFFER_SIZE, 1U);
  if ((xRxBuffer == NULL) || (xTxBuffer == NULL)) {
    if (xRxBuffer != NULL) {
      vStreamBufferDelete(xRxBuffer);
    }
    if (xTxBuffer != NULL) {
      vStreamBufferDelete(xTxBuffer);
    }
    (void)vuartDeinit(VSERIAL_UNIT);
    return pdFAIL;
  }

  /* The UART events are initially disabled.*/
  vPortVRQEnable(VSERIAL_VRQ);
  vuartWriteEnableFlags(VSERIAL_UNIT, VSERIAL_EVENTS);

  return pdPASS;
}

/**
 * @brief   Reads data.
 * @details The function returns as soon as some data is available.
 *
 * @param[out] pvBuffer     buffer for the received data
 * @param[in] xSize         maximum number of bytes to be read
 * @param[in] xTicksToWait  maximum time to wait for data
 * @return                  The number of bytes read, zero on timeout.
 */
size_t xVSerialRead(void *pvBuffer, size_t xSize, TickType_t xTicksToWait) {

  return xStreamBufferReceive(xRxBuffer, pvBuffer, xSize, xTicksToWait);
}

/**
 * @brief   Writes data.
 * @details The function returns when all the data has been buffered.
 *
 * @param[in] pvData        data to be transmitted
 * @param[in] xSize         number of bytes to be written
 * @param[in] xTicksToWait  maximum time to wait for buffer space
 * @return                  The number of bytes written, less than
 *                          @p xSize on timeout.
 */
size_t xVSerialWrite(const void *pvData, size_t xSize,
                     TickType_t xTicksToWait) {
  const uint8_t *pucData = (const uint8_t *)pvData;
  TimeOut_t xTimeOut;
  size_t xWritten = 0U;

  /* Data larger than the free buffer space is written in multiple steps,
     each step waits for buffer space.*/
  vTaskSetTimeOutState(&xTimeOut);
  while (xWritten < xSize) {
    size_t xn;

    xn = xStreamBufferSend(xTxBuffer, &pucData[xWritten], xSize - xWritten,
                           xTicksToWait);
    if (xn == 0U) {
      break;
    }
    xWritten += xn;

    /* Starting the transmission, if not already ongoing.*/
    vPortVRQTrigger(VSERIAL_VRQ);

    if (xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait) != pdFALSE) {
      break;
    }
  }

  return xWritten;
}

/**
 * @brief   Returns and clears the accumulated errors.
 * @note    RX buffer overflows are reported as
 *          @p SB_VUART_EV_OVERRUN_ERR.
 *
 * @return              The @p SB_VUART_EV_xxx errors mask.
 */
uint32_t ulVSerialGetAndClearErrors(void) {
  uint32_t ulMask;

  taskENTER_CRITICAL();
  ulMask   = ulErrors;
  ulErrors = 0U;
  taskEXIT_CRITICAL();

  return ulMask;
}

/** @} */
