#include "../Inc/Circle_Buffer.h"

void Circle_Buf_Init(p_circle_buf pCircleBuf, uint32_t len, uint8_t *buf)
{
    pCircleBuf->len = len;
    pCircleBuf->buf = buf;
    pCircleBuf->Circle_Buffer_W = pCircleBuf->Circle_Buffer_R = 0;
}

int Circle_Buf_Write(p_circle_buf pCircleBuf, uint8_t Val)
{
    uint32_t next = pCircleBuf->Circle_Buffer_W + 1;

    if (next == pCircleBuf->len)
    {
        next = 0;
    }

    if (pCircleBuf->Circle_Buffer_R != next)
    {
        pCircleBuf->buf[pCircleBuf->Circle_Buffer_W] = Val;
        pCircleBuf->Circle_Buffer_W = next;
        return 0;
    }
    else
    {
        return -1;
    }

}

int Circle_Buf_Read(p_circle_buf pCircleBuf, uint8_t *pVal)
{
    if (pCircleBuf->Circle_Buffer_R != pCircleBuf->Circle_Buffer_W)
    {
        *pVal = pCircleBuf->buf[pCircleBuf->Circle_Buffer_R];
        pCircleBuf->Circle_Buffer_R++;

        if (pCircleBuf->Circle_Buffer_R == pCircleBuf->len)
        {
            pCircleBuf->Circle_Buffer_R = 0;
        }
        return 0;
    }
    else
    {
        return -1;
    }

}

