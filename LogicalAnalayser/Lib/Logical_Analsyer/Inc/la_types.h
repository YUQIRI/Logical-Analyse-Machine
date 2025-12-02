//
// Created by yuqiri on 2025/11/29.
//

#ifndef LA_TYPES_H
#define LA_TYPES_H

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

//返回码
typedef enum {
    LA_OK       = 0,
    LA_ERROR    = -1,
    LA_BUSY     = -2,
    LA_TIMEOUT  = -3,
    LA_INVAL    = -4
} la_status;

//事件回调
typedef void (*la_event_cb)(void);

#endif //LA_TYPES_H