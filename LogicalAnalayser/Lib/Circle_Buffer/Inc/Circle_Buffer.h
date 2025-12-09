#ifndef CIRCLE_BUFFER_H
#define CIRCLE_BUFFER_H

#include <stdint.h>
#include <stdio.h>

typedef struct CircleBuffer
{
    uint32_t Circle_Buffer_W;
    uint32_t Circle_Buffer_R;
    uint32_t len;
    uint8_t  *buf;
}CircleBuf, *p_circle_buf;

void Circle_Buf_Init(p_circle_buf pCircleBuf, uint32_t Len, uint8_t *buf);
int Circle_Buf_Write(p_circle_buf pCircleBuf, uint8_t Val);
int Circle_Buf_Read(p_circle_buf pCircleBuf, uint8_t *pVal);


#endif
