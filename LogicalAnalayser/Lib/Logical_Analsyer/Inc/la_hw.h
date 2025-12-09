//
// Created by yuqiri on 2025/11/29.
//

#ifndef LA_HW_H
#define LA_HW_H

#include "la_types.h"
#include "main.h"
#include "Circle_Buffer.h"


void LA_Init(void);
void LA_RxCplt_Wait(void);

int LA_Read_Data_Byte();
int LA_Read_Data();

la_status LA_Send_byte(uint8_t *sendata);
la_status LA_Send_4Byte(uint8_t *sendata);
la_status LA_Send_String(uint8_t *sendata);
la_status LA_Send_BigEndian(uint32_t data, uint8_t *sendata);
la_status LA_UARTGetCharTimeout(uint8_t *pVal, int timeout);

#endif //LA_HW_H