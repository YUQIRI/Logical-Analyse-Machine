//
// Created by yuqiri on 2025/11/29.
//

#include "la_hw.h"
extern UART_HandleTypeDef *huart;
extern UART_HandleTypeDef huart1;
/* scan */

/* circle buffer def */
#define LA_TIMOUT 1000u
#define LA_RX_BUFSIZE 10
static uint8_t la_rx_char;
//static CircleBuf la_rx_buf;
static p_circle_buf g_la_rx_buf;
static uint8_t la_tem_buf[LA_RX_BUFSIZE];

static volatile int la_rx_flag = 0;
//static volatile int la_tx_flag = 0;

/* 大字节序定义 */
#define BYTE0(v) ((v >> 0)  & 0xff) //LSB
#define BYTE1(v) ((v >> 8)  & 0xff)
#define BYTE2(v) ((v >> 16) & 0xff)
#define BYTE3(v) ((v >> 24) & 0xff) //MSB

void LA_Init(void)
{
    /* circle buffer init */
    Circle_Buf_Init( g_la_rx_buf,  LA_RX_BUFSIZE,  la_tem_buf);

    /* set DMA to IDLE */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, la_tem_buf,  LA_RX_BUFSIZE);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    la_rx_flag = 1;
    if (huart == &huart1)
    {
        /* save data to buf */
        for (int i = 0; i < LA_RX_BUFSIZE; i++)
        {
            Circle_Buf_Write(g_la_rx_buf, la_tem_buf[i]);
        }

        /* reset DMA to IDLE */
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, &la_rx_char,  LA_RX_BUFSIZE);
    }
}

la_status LA_UARTGetCharTimeout(uint8_t *pVal, int timeout)
{
    while (-1 == Circle_Buf_Read(g_la_rx_buf, pVal) && timeout)
    {
        HAL_Delay(1);
        timeout--;
    }
    if (timeout == 0)
        return LA_TIMEOUT;
    else
        return LA_OK;
}

void LA_RxCplt_Wait(void)
{
    while (la_rx_flag != 0);
    la_rx_flag = 0;
}

int LA_Read_Data_Byte()
{
    uint8_t rx_byte = 0;
    Circle_Buf_Read(g_la_rx_buf, la_tem_buf);
    rx_byte += la_tem_buf[g_la_rx_buf->Circle_Buffer_R];
    return rx_byte;
}

int LA_Read_Data()
{
    uint8_t rx_byte = 0;
    Circle_Buf_Read(g_la_rx_buf, la_tem_buf);
    rx_byte += BYTE0(la_tem_buf[g_la_rx_buf->Circle_Buffer_R]);
    Circle_Buf_Read(g_la_rx_buf, la_tem_buf);
    rx_byte += BYTE1(la_tem_buf[g_la_rx_buf->Circle_Buffer_R]);
    Circle_Buf_Read(g_la_rx_buf, la_tem_buf);
    rx_byte += BYTE2(la_tem_buf[g_la_rx_buf->Circle_Buffer_R]);
    Circle_Buf_Read(g_la_rx_buf, la_tem_buf);
    rx_byte += BYTE3(la_tem_buf[g_la_rx_buf->Circle_Buffer_R]);
    return rx_byte;
}

la_status LA_Send_byte(uint8_t *sendata)
{

        HAL_UART_Transmit(&huart1, sendata, 1, LA_TIMEOUT);
        return LA_OK;

}

la_status LA_Send_4Byte(uint8_t *sendata)
{

        HAL_UART_Transmit(&huart1, sendata, 4, LA_TIMEOUT);
        return LA_OK;

}

la_status LA_Send_String(uint8_t *sendata)
{

        HAL_UART_Transmit(&huart1, sendata, sizeof(sendata), LA_TIMEOUT);
        return LA_OK;

}

la_status LA_Send_BigEndian(uint32_t data, uint8_t *sendata)
{

        sendata[0] = BYTE3(data);
        sendata[1] = BYTE2(data);
        sendata[2] = BYTE1(data);
        sendata[3] = BYTE0(data);
        HAL_UART_Transmit(&huart1, sendata, sizeof(sendata), LA_TIMEOUT);
        return LA_OK;

}