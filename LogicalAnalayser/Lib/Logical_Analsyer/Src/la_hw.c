//
// Created by yuqiri on 2025/11/29.
//

#include "la_hw.h"

extern UART_HandleTypeDef huart1;
/* scan */

/* circle buffer def */
#define LA_RX_BUFSIZE 256*1024
static CircleBuf la_rx_buf;
static p_circle_buf g_la_rx_buf;
static uint8_t la_circle_buf[LA_RX_BUFSIZE];

la_status LA_Init(p_circle_buf pCircleBuf,
                    uint32_t Len,
                    uint8_t *buf,/* circle buf */
                            UART_HandleTypeDef *huart,
                            uint8_t *pData,
                            uint16_t Size/* DMA */)
{
    /* circle buffer init */
    Circle_Buf_Init( g_la_rx_buf,  Len,  buf);

    /* set UMA to IDLE */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, pData,  Size);

    return HAL_OK;
}


la_status LA_Receive()
{
    if (LA_Init() == HAL_OK)
    {

    }
}
