//
// Created by yuqiri on 2025/11/26.
//

#include "string.h"
#include "main.h"
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"


//#define USB_ASM_TO_SAMPLE/*汇编采集数据，提高采样率*/
/* 全局串口句柄（确保main.c中初始化huart1，波特率115200） */
extern UART_HandleTypeDef huart1;

/* ===================== 核心配置宏定义 ===================== */
#define LA_TIMOUT         100u        // 串口发送超时时间(ms)
#define UART_RX_BUF_SIZE  128         // 串口接收缓冲区大小
#define LA_RX_DATASIZE    3500         // 采样缓冲区大小
#define LA_MAX_CHANNELS   8           // 最大通道数
#define LA_MAX_FREQUENCY  1000000ul   // 最大采样率(1MHz)
#define LA_TIMEFOREVER    0xFFFFFFFFul// 永久超时

/* 返回码定义 */
typedef enum {
    LA_OK       = 0,
    LA_ERROR    = -1
} la_status;

/* ===================== SUMP协议指令常量 ===================== */
#define CMD_RESET                     0x00
#define CMD_ARM_BASIC_TRIGGER         0x01
#define CMD_ID                        0x02
#define CMD_METADATA                  0x04
#define CMD_XON                       0x11
#define CMD_XOFF                      0x13
#define CMD_SET_DIVIDER               0x80
#define CMD_CAPTURE_SIZE              0x81
#define CMD_SET_FLAGS                 0x82
#define CMD_CAPTURE_DELAYCOUNT        0x83
#define CMD_CAPTURE_READCOUNT         0x84
#define CMD_SET_BASIC_TRIGGER_MASK0   0xC0
#define CMD_SET_BASIC_TRIGGER_VALUE0  0xC1
#define CMD_SET_BASIC_TRIGGER_CONFIG0 0xC2

/* ===================== SUMP元数据Token ===================== */
#define METADATA_TOKEN_END                    0x00
#define METADATA_TOKEN_DEVICE_NAME            0x01
#define METADATA_TOKEN_NUM_PROBES_LONG        0x20
#define METADATA_TOKEN_SAMPLE_MEMORY_BYTES    0x21
#define METADATA_TOKEN_DYNAMIC_MEMORY_BYTES   0x22
#define METADATA_TOKEN_MAX_SAMPLE_RATE_HZ     0x23
#define METADATA_TOKEN_NUM_PROBES_SHORT       0x40
#define METADATA_TOKEN_PROTOCOL_VERSION_SHORT 0x41

/* ===================== 字节操作宏 ===================== */
#define BYTE0(v) (v & 0xff)        // 最低位字节
#define BYTE1(v) ((v >> 8) & 0xff)
#define BYTE2(v) ((v >> 16) & 0xff)
#define BYTE3(v) ((v >> 24) & 0xff) // 最高位字节

/* ===================== 全局变量 ===================== */
static volatile uint8_t sampling_en = 0; // 采样使能标志（1-采样中，0-停止）
volatile int get_stop_cmd = 0;               // 停止采样指令
static uint32_t g_samplingRate = 0;          // 当前采样率
static uint32_t g_sampleDelay = 0;           // 采样延迟
static uint32_t g_sampleNumber = 0;          // 采样数量
static uint32_t g_triggerMask = 0;           // 触发掩码
static uint32_t g_triggerValue = 0;          // 触发值
static uint8_t g_triggerState = 0;           // 触发使能状态
static uint8_t g_rxdata_buf[LA_RX_DATASIZE]; // 采样数据缓冲区
static uint32_t g_rxcnt_buf[LA_RX_DATASIZE]; // 采样计数缓冲区
static int32_t g_cur_pos = 0;                // 当前采样位置
static int32_t g_cur_sample_cnt = 0;         // 当前采样计数
static uint32_t g_virtual_bufferSize = LA_RX_DATASIZE; // 虚拟缓冲区大小
static uint32_t g_flags = 0;                 // 协议标志位

/* ===================== 极简环形缓冲区（无第三方依赖） ===================== */
static uint8_t uart_rx_buf[UART_RX_BUF_SIZE]; // 串口接收缓冲区
static uint16_t uart_rx_head = 0;             // 接收缓冲区头指针
static uint16_t uart_rx_tail = 0;             // 接收缓冲区尾指针
static uint8_t la_tem_buf[UART_RX_BUF_SIZE];  // DMA临时接收缓冲区

/* ===================== 串口基础操作函数 ===================== */
/**
 * @brief 串口发送数据
 * @param datas 待发送数据指针
 * @param len 数据长度
 * @param timeout 超时时间(ms)
 * @return 0-成功, -1-失败
 */
int uart_send(uint8_t *datas, int len, int timeout) {
    if (HAL_UART_Transmit(&huart1, datas, len, timeout) == HAL_OK)
        return 0;
    else
        return -1;
}

/**
 * @brief 发送单个字节
 * @param val 待发送字节
 * @param timeout 超时时间(ms)
 * @return 0-成功, -1-失败
 */
static int send_byte(uint8_t val, int timeout) {
    // 等待串口发送完成，避免数据堆积
    while(HAL_UART_GetState(&huart1) == HAL_UART_STATE_BUSY_TX);
    return uart_send(&val, 1, timeout);
}

/**
 * @brief 大端序发送32位整数
 * @param val 待发送整数
 * @param timeout 超时时间(ms)
 * @return 0-成功, -1-失败
 */
static int send_uint32(uint32_t val, int timeout) {
    uint8_t buffer[4] = {BYTE3(val), BYTE2(val), BYTE1(val), BYTE0(val)};
    return uart_send(buffer, 4, timeout);
}

/**
 * @brief 发送字符串
 * @param str 待发送字符串
 * @param timeout 超时时间(ms)
 * @return 0-成功, -1-失败
 */
static int send_string(char *str, int timeout) {
    return uart_send((uint8_t *)str, strlen(str), timeout);
}

/* ===================== DMA IDLE中断回调函数 ===================== */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart == &huart1) {
        // 将DMA接收的数据写入环形缓冲区
        for (uint16_t i = 0; i < Size; i++) {
get_stop_cmd = 1;
            uart_rx_buf[uart_rx_head] = la_tem_buf[i];
            uart_rx_head = (uart_rx_head + 1) % UART_RX_BUF_SIZE;
        }
        // 重启DMA接收，避免断流
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, la_tem_buf, UART_RX_BUF_SIZE);
    }
}

/* ===================== 串口接收函数 ===================== */
/**
 * @brief 带超时接收单个字节
 * @param pVal 接收数据存储指针
 * @param timeout 超时时间(ms)
 * @return 0-成功, -1-超时/失败
 */
int uart_recv(uint8_t *pVal, int timeout) {
    // 等待缓冲区有数据或超时
    while (uart_rx_head == uart_rx_tail && timeout > 0) {
        HAL_Delay(1);
        timeout--;
    }
    // 超时判断
    if (uart_rx_head == uart_rx_tail)
        return -1;
    // 读取数据并更新尾指针
    *pVal = uart_rx_buf[uart_rx_tail];
    uart_rx_tail = (uart_rx_tail + 1) % UART_RX_BUF_SIZE;
    return 0;
}

/* ===================== 辅助函数 ===================== */
/**
 * @brief 取最小值
 * @param a 数值1
 * @param b 数值2
 * @return 较小值
 */
static uint32_t min(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}

/**
 * @brief 设置采样分频系数（计算实际采样率）
 * @param divider 分频系数
 */
static void setSamplingDivider(uint32_t divider) {
    int f = 72000000 / (divider + 1); // F103主频72MHz
    if (f > LA_MAX_FREQUENCY)
        f = LA_MAX_FREQUENCY;
    g_samplingRate = f;
}

/**
 * @brief 设置采样数量（限制缓冲区大小）
 * @param s 目标采样数量
 */
static void setSampleNumber(uint32_t s) {
    g_sampleNumber = min((uint32_t)LA_RX_DATASIZE, s);
}

/**
 * @brief 设置采样延迟
 * @param s 目标延迟值
 */
static void setSamplingDelay(uint32_t s) {
    g_sampleDelay = s;
    if (g_sampleDelay > 1000000)
        g_sampleDelay = 1000000;
}

/**
 * @brief 设置触发掩码
 * @param s 触发掩码值
 */
static void setTriggerMask(uint32_t s) {
    g_triggerMask = s;
}

/**
 * @brief 设置触发值
 * @param s 触发值
 */
static void setTriggerValue(uint32_t s) {
    g_triggerValue = s;
}

/**
 * @brief 设置触发使能状态
 * @param state 触发状态(0-禁用,1-启用)
 */
static void setTriggerState(uint8_t state) {
    g_triggerState = state;
}

/**
 * @brief 设置协议标志位
 * @param s 标志位值
 */
static void setFlags(uint32_t s) {
    g_flags = s;
}

/* ===================== 中断控制函数 ===================== */
#define SYSTICK_CTRL    (*((volatile uint32_t *)0xE000E010))
#define SYSTICK_TICKINT (1 << 1)

/**
 * @brief 禁用SysTick中断（采样时避免干扰）
 */
void Disable_TickIRQ(void) {
    __disable_irq(); // 关闭全局中断
    SYSTICK_CTRL &= ~SYSTICK_TICKINT;
}

/**
 * @brief 启用SysTick中断
 */
void Enable_TickIRQ(void) {
    SYSTICK_CTRL |= SYSTICK_TICKINT;
    __enable_irq();  // 开启全局中断
}

/* ===================== 采样核心函数 ===================== */
/**
 * @brief 开始采样（PB8~PB15对应Channel0~7）
 */
/* ===================== 采样核心函数（标准SUMP协议版） ===================== */

static void start(void)
{
    uint8_t data;
	uint8_t data_pre;
	
    // 固定抓 512 个点，足够显示很多次 PWM 变化
    const int sample_total = LA_RX_DATASIZE;

    volatile uint16_t *data_reg = (volatile uint16_t *)0x40010C08; // GPIOB_IDR

    get_stop_cmd = 0;
    g_cur_sample_cnt = 0;

    Disable_TickIRQ();
    memset(g_rxdata_buf, 0, sizeof(g_rxdata_buf));

    // 触发直接跳过，立即采样
    if (g_triggerState && g_triggerMask)
    {
        for (int i = 0; i < 10000; i++); // 小延时，防止卡死
    }
	
	//先读一次pb8-15数据,存data_pre，进入循环了再一直读pb8-15数据
	data = ((*data_reg) >> 8);
	data_pre = data;
		
    // ==============================
    // 核心：连续采样 512 个点，每个点都存
    // ==============================
    while (g_cur_sample_cnt < sample_total)
    {
        // 读 PB8 → channel0
        data = ((*data_reg) >> 8);
		
        // 点变化了再存，合并、判断
		if(data_pre != data)
		{
			g_rxdata_buf[g_cur_sample_cnt] = data;
			data_pre = data;
			g_cur_sample_cnt++;
		}
        

        // ==============================
        // 关键延时！必须够慢才能抓到 PWM 变化
        // ==============================
        for (int i = 0; i < 30; i++)
        {
            __NOP();
        }
    }

    Enable_TickIRQ();
}
/* ===================== 批量上传函数（标准SUMP协议版） ===================== */
static void upload(void)
{
    // 只发 512 字节，标准 SUMP 格式，PulseView 绝对不崩溃
    for (int i = 0; i < LA_RX_DATASIZE; i++)
    {
        while (HAL_UART_GetState(&huart1) == HAL_UART_STATE_BUSY_TX);
        send_byte(g_rxdata_buf[i],LA_TIMOUT);
    }
}

/**
 * @brief 运行采样+上传流程
 */
static void run(void) {
    start();
    upload();
}

/* ===================== 初始化函数 ===================== */
/**
 * @brief 逻辑分析仪初始化（启动DMA接收）
 */
void LA_Init(void) {
    // 启动串口DMA IDLE接收
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, la_tem_buf, UART_RX_BUF_SIZE);
}

/* ===================== SUMP协议解析主函数 ===================== */
void LogicalAnalyser() {
    uint8_t cmd_buffer[5];  // 指令缓冲区（最多5字节）
    uint8_t cmd_index = 0;  // 指令字节计数
    uint8_t c;              // 接收的单个字节

    while (1) {
        // 接收串口数据（永久超时）
        if (uart_recv(&c, LA_TIMEFOREVER) == 0) {
            cmd_buffer[cmd_index] = c;

            // 协议指令解析
            switch (cmd_buffer[0]) {
                case CMD_RESET: { // 重置指令
                    cmd_index = 0;
                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
                    break;
                }
                case CMD_ID: { // 设备ID指令（返回"1ALS"）
                    uart_send((uint8_t *)"1ALS", 4, LA_TIMOUT);
                    cmd_index = 0;
                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
                    break;
                }
                case CMD_METADATA: { // 元数据指令（返回设备信息）
                    send_byte(METADATA_TOKEN_DEVICE_NAME, LA_TIMOUT);        // 设备名Token
                    send_string("LA_YUQIRI", LA_TIMOUT);                     // 设备名
                    send_byte(METADATA_TOKEN_END, LA_TIMOUT);                // 字符串结束
                    
                    send_byte(METADATA_TOKEN_NUM_PROBES_LONG, LA_TIMOUT);    // 长格式通道数Token
                    send_uint32(LA_MAX_CHANNELS, LA_TIMOUT);                // 8通道
                    
                    send_byte(METADATA_TOKEN_SAMPLE_MEMORY_BYTES, LA_TIMOUT);// 采样内存Token
                    send_uint32(g_virtual_bufferSize, LA_TIMOUT);           // 缓冲区大小
                    
                    send_byte(METADATA_TOKEN_DYNAMIC_MEMORY_BYTES, LA_TIMOUT);// 动态内存Token
                    send_uint32(0, LA_TIMOUT);                              // 0（无动态内存）
                    
                    send_byte(METADATA_TOKEN_MAX_SAMPLE_RATE_HZ, LA_TIMOUT); // 最大采样率Token
                    send_uint32(LA_MAX_FREQUENCY, LA_TIMOUT);               // 1MHz
                    
                    send_byte(METADATA_TOKEN_NUM_PROBES_SHORT, LA_TIMOUT);   // 短格式通道数Token
                    send_byte(LA_MAX_CHANNELS, LA_TIMOUT);                  // 8通道
                    
                    send_byte(METADATA_TOKEN_PROTOCOL_VERSION_SHORT, LA_TIMOUT); // 协议版本Token
                    send_byte(0x02, LA_TIMOUT);                             // 版本0x02
                    
					    // 新增：告诉上位机支持差分采样（关键）
					send_byte(0x08, LA_TIMOUT); // DIFFERENTIAL标志
					send_byte(0x01, LA_TIMOUT); // 开启差分采样
    
					
                    send_byte(METADATA_TOKEN_END, LA_TIMOUT);                // 元数据结束
                    
                    cmd_index = 0;
                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
                    break;
                }
                case CMD_ARM_BASIC_TRIGGER: { // 触发采样指令
                    run(); // 执行采样+上传
                    cmd_index = 0;
                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
                    break;
                }
                case CMD_SET_DIVIDER: { // 设置分频系数指令
                    cmd_index++;
                    if (cmd_index < 5) break; // 等待5字节接收完成
                    setSamplingDivider(*(uint32_t *)(cmd_buffer + 1));
                    cmd_index = 0;
                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
                    break;
                }
                case CMD_CAPTURE_SIZE: { // 设置采样大小指令
					cmd_index++;
					if (cmd_index < 5) break;
					uint16_t delayCount = *((uint16_t *)(cmd_buffer + 1));
					uint16_t readCount = *((uint16_t *)(cmd_buffer + 3));
					// 限制采样深度为1024，避免过大导致崩溃
					setSampleNumber(min(1024, 4 * readCount)); 
					setSamplingDelay(4 * delayCount);
					cmd_index = 0;
					memset(cmd_buffer, 0, sizeof(cmd_buffer));
					break;
				}
                case CMD_SET_BASIC_TRIGGER_MASK0: { // 设置触发掩码
                    cmd_index++;
                    if (cmd_index < 5) break;
                    setTriggerMask(*(uint32_t *)(cmd_buffer + 1));
                    cmd_index = 0;
                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
                    break;
                }
                case CMD_SET_BASIC_TRIGGER_VALUE0: { // 设置触发值
                    cmd_index++;
                    if (cmd_index < 5) break;
                    setTriggerValue(*(uint32_t *)(cmd_buffer + 1));
                    cmd_index = 0;
                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
                    break;
                }
                case CMD_SET_BASIC_TRIGGER_CONFIG0: { // 设置触发配置
                    cmd_index++;
                    if (cmd_index < 5) break;
                    uint8_t serial = (*((uint8_t *)(cmd_buffer + 4)) & 0x04) > 0 ? 1 : 0;
                    uint8_t state = (*((uint8_t *)(cmd_buffer + 4)) & 0x08) > 0 ? 1 : 0;
                    setTriggerState(serial ? 0 : state);
                    cmd_index = 0;
                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
                    break;
                }
                case CMD_SET_FLAGS: { // 设置标志位
                    cmd_index++;
                    if (cmd_index < 5) break;
                    setFlags(*(uint32_t *)(cmd_buffer + 1));
                    cmd_index = 0;
                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
                    break;
                }
				case CMD_XON: { // 恢复采样指令
					get_stop_cmd = 0;
					sampling_en = 1; // 重新使能采样
					cmd_index = 0;
					memset(cmd_buffer, 0, sizeof(cmd_buffer));
					break;
				}
				case CMD_XOFF: { // 停止采样指令
					get_stop_cmd = 1;
					sampling_en = 0; // 关闭采样使能
					cmd_index = 0;
					memset(cmd_buffer, 0, sizeof(cmd_buffer));
					break;
				}
                default: { // 未知指令，重置缓冲区
                    cmd_index = 0;
                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
                }
            }
        }
    }
}

//#include "la_sump.h"
//#include "string.h"
//#include "la_types.h"
//#include "usart.h"
//#include "main.h"
//#include "stdint.h"
//#include "stdbool.h"
//#include "stddef.h"

//extern UART_HandleTypeDef huart1;

///* ===================== 核心宏定义修正 ===================== */
//#define CMD_RESET                     0x00
//#define CMD_ARM_BASIC_TRIGGER         0x01
//#define CMD_ID                        0x02
//#define CMD_METADATA                  0x04
//#define CMD_XON                       0x11
//#define CMD_XOFF                      0x13
//#define CMD_SET_DIVIDER               0x80
//#define CMD_CAPTURE_SIZE              0x81
//#define CMD_SET_FLAGS                 0x82
//#define CMD_CAPTURE_DELAYCOUNT        0x83
//#define CMD_CAPTURE_READCOUNT         0x84
//#define CMD_SET_BASIC_TRIGGER_MASK0   0xC0
//#define CMD_SET_BASIC_TRIGGER_VALUE0  0xC1
//#define CMD_SET_BASIC_TRIGGER_CONFIG0 0xC2

//#define METADATA_TOKEN_END                    0x00
//#define METADATA_TOKEN_DEVICE_NAME            0x01
//#define METADATA_TOKEN_NUM_PROBES_LONG        0x20  // 必须加！
//#define METADATA_TOKEN_SAMPLE_MEMORY_BYTES    0x21
//#define METADATA_TOKEN_DYNAMIC_MEMORY_BYTES   0x22
//#define METADATA_TOKEN_MAX_SAMPLE_RATE_HZ     0x23
//#define METADATA_TOKEN_NUM_PROBES_SHORT       0x40
//#define METADATA_TOKEN_PROTOCOL_VERSION_SHORT 0x41

//#define BYTE0(v) (v & 0xff)    // LSB
//#define BYTE1(v) ((v >> 8) & 0xff)
//#define BYTE2(v) ((v >> 16) & 0xff)
//#define BYTE3(v) ((v >> 24) & 0xff) // MSB

//#define TIMEOUT_FOREVER 0xFFFFFFFFul
//#define TIMEOUT_DEFAULT 100
//#define BUFFER_SIZE 512            // 减小缓冲区，避免越界
//#define MAX_FREQUENCY 1000000      // 1MHz
//#define LA_NAME "LA_YUQIRI"        // 设备名
//#define LA_MAX_CHANNELS 8          // 8通道

///* ===================== 全局变量初始化 ===================== */
//volatile int get_stop_cmd = 0;
//static uint32_t g_samplingRate = 0;
//static uint32_t g_sampleDelay = 0;
//static uint32_t g_sampleNumber = 0;
//static uint32_t g_triggerMask = 0;
//static uint32_t g_triggerValue = 0;
//static uint8_t g_triggerState = 0;
//static uint8_t g_rxdata_buf[BUFFER_SIZE];
//static uint32_t g_rxcnt_buf[BUFFER_SIZE];
//static int32_t g_cur_pos = 0;       // 初始化！
//static int32_t g_cur_sample_cnt = 0;// 初始化！
//static uint32_t g_virtual_bufferSize = BUFFER_SIZE; // 改为实际缓冲区大小
//static uint32_t g_flags = 0;

///* ===================== 串口接收缓冲区（解决阻塞问题） ===================== */
//#define UART_RX_BUF_SIZE 128
//static uint8_t uart_rx_buf[UART_RX_BUF_SIZE];
//static uint16_t uart_rx_head = 0;
//static uint16_t uart_rx_tail = 0;

//// 串口IDLE中断回调（必须在usart.c中调用）
//void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
//{
//    if (huart == &huart1)
//    {
//        for (uint16_t i = 0; i < Size; i++)
//        {
//            uart_rx_buf[uart_rx_head] = ((uint8_t*)huart->pRxBuffPtr)[i];
//            uart_rx_head = (uart_rx_head + 1) % UART_RX_BUF_SIZE;
//        }
//        // 重启DMA接收
//        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, (uint8_t*)huart->pRxBuffPtr, UART_RX_BUF_SIZE);
//    }
//}

///* ===================== 串口函数修正 ===================== */
//// 带超时的接收单字节（非阻塞，基于环形缓冲区）
//int UART1GetCharTimeout(uint8_t *pVal, int timeout)
//{
//    while (uart_rx_head == uart_rx_tail && timeout > 0)
//    {
//        HAL_Delay(1);
//        timeout--;
//    }
//    
//    if (uart_rx_head == uart_rx_tail)
//        return -1; // 超时
//    
//    *pVal = uart_rx_buf[uart_rx_tail];
//    uart_rx_tail = (uart_rx_tail + 1) % UART_RX_BUF_SIZE;
//    return 0; // 成功
//}

//// 发送数据（带返回值检查）
//int UART1Send(uint8_t *datas, int len, int timeout)
//{
//    if (HAL_UART_Transmit(&huart1, datas, len, timeout) == HAL_OK)
//        return 0;
//    else
//        return -1;
//}

//static int uart_recv(uint8_t *pVal, int timeout) {
//    return UART1GetCharTimeout(pVal, timeout);
//}

//static int uart_send(uint8_t *datas, int len, int timeout) {
//    return UART1Send(datas, len, timeout);
//}

//static int send_byte(uint8_t val, int timeout) {
//    return uart_send(&val, 1, timeout);
//}

//static int send_uint32(uint32_t val, int timeout) {
//    uint8_t buffer[4] = {BYTE3(val), BYTE2(val), BYTE1(val), BYTE0(val)};
//    return uart_send(buffer, 4, timeout);
//}

//static int send_string(char *str, int timeout) {
//    return uart_send((uint8_t *)str, strlen(str), timeout);
//}

///* ===================== 辅助函数 ===================== */
//static uint32_t getBufferSize(void) {
//    return g_virtual_bufferSize;
//}

//static uint32_t getMaxFrequency(void) {
//    return MAX_FREQUENCY;
//}

//static uint32_t min(uint32_t a, uint32_t b) {
//    return (a < b) ? a : b;
//}

//static void setSampleNumber(uint32_t s) {
//    g_sampleNumber = min((uint32_t)BUFFER_SIZE, s); // 限制为实际缓冲区大小
//}

//static void setSamplingDelay(uint32_t s) {
//    g_sampleDelay = s;
//    if (g_sampleDelay > 1000000)
//        g_sampleDelay = 1000000;
//}

//static void setSamplingDivider(uint32_t divider) {
//    int f = 72000000 / (divider + 1); // 修正：72MHz主频（F103）
//    if (f > MAX_FREQUENCY)
//        f = MAX_FREQUENCY;
//    g_samplingRate = f;
//}

//static void setTriggerMask(uint32_t s) {
//    g_triggerMask = s;
//}

//static void setTriggerValue(uint32_t s) {
//    g_triggerValue = s;
//}

//static void setTriggerState(uint8_t state) {
//    g_triggerState = state;
//}

//static void setFlags(uint32_t s) {
//    g_flags = s;
//}

///* ===================== 中断控制 ===================== */
//#define SYSTICK_CTRL    (*((volatile uint32_t *)0xE000E010))
//#define SYSTICK_TICKINT (1 << 1)

//void Disable_TickIRQ(void) {
//    __disable_irq(); // 关闭全局中断
//    SYSTICK_CTRL &= ~SYSTICK_TICKINT;
//}

//void Enable_TickIRQ(void) {
//    SYSTICK_CTRL |= SYSTICK_TICKINT;
//    __enable_irq();  // 开启全局中断
//}

///* ===================== 采样函数 ===================== */
//static void start(void) {
//    uint8_t data;
//    uint8_t pre_data;
//    uint32_t converted_sample_count = g_sampleNumber * (MAX_FREQUENCY / g_samplingRate);
//    
//    // GPIO寄存器修正：PA12_BSRR = 0x40010810，GPIOB_IDR=0x40010C08
//    volatile uint16_t *data_reg = (volatile uint16_t *)0x40010C08; // GPIOB_IDR
//    volatile uint32_t *pa12_reg = (volatile uint32_t *)0x40010810; // GPIOA_BSRR

//    get_stop_cmd = 0;
//    g_cur_pos = 0;
//    g_cur_sample_cnt = 0;

//    Disable_TickIRQ();
//    memset(g_rxcnt_buf, 0, sizeof(g_rxcnt_buf));

//    /* 等待触发条件 */
//    if (g_triggerState && g_triggerMask) {
//        while (1) {
//            data = (*data_reg) >> 8;
//            if ((data & g_triggerMask) == (g_triggerValue & g_triggerMask)) break;
//            if ((~data & g_triggerMask) == (~g_triggerValue & g_triggerMask)) break;
//            if (get_stop_cmd) {
//                Enable_TickIRQ();
//                return;
//            }
//        }
//    }

//    /* 初始化第一个数据 */
//    data = (*data_reg) >> 8;
//    g_rxdata_buf[0] = data;
//    g_rxcnt_buf[0] = 1;
//    g_cur_sample_cnt = 1;
//    pre_data = data;

//    /* 采样主循环 */
//    while (1) {
//        *pa12_reg = (1 << 12); // PA12置高
//        
//        data = (*data_reg) >> 8;
//        
//        // 缓冲区边界保护
//        if (g_cur_pos >= BUFFER_SIZE - 1) break;
//        
//        // 数据变化时切换位置
//        g_cur_pos += (data != pre_data) ? 1 : 0;
//        g_rxdata_buf[g_cur_pos] = data;
//        g_rxcnt_buf[g_cur_pos]++;
//        g_cur_sample_cnt++;
//        pre_data = data;

//        // 退出条件
//        if (get_stop_cmd) break;
//        if (g_cur_sample_cnt >= converted_sample_count) break;
//        
//        // 1MHz延时（60个nop≈1us）
//        __asm volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
//        __asm volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
//        __asm volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
//        __asm volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
//        __asm volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
//        __asm volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");

//        *pa12_reg = (1UL << (12 + 16)); // PA12置低（BSRR的高16位是置0）
//    }

//    Enable_TickIRQ();
//}

///* ===================== 数据上传 ===================== */
//static void upload(void) {
//    int32_t i = g_cur_pos;
//    uint32_t j;
//    uint32_t rate = MAX_FREQUENCY / g_samplingRate;
//    int cnt = 0;

//    for (; i >= 0; i--) {
//        for (j = 0; j < g_rxcnt_buf[i]; j++) {
//            cnt++;
//            if (cnt == rate) {
//                uart_send(&g_rxdata_buf[i], 1, 10);
//                cnt = 0;
//            }
//        }
//    }
//}

//static void run(void) {
//    start();
//    upload();
//}

///* ===================== 主协议解析 ===================== */
//void LogicalAnalyser_Init(void)
//{
//    // 初始化串口DMA接收（必须调用！）
//    uint8_t dummy_buf[UART_RX_BUF_SIZE];
//    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dummy_buf, UART_RX_BUF_SIZE);
//}

//void LogicalAnalyser() {
//    uint8_t cmd_buffer[5];
//    uint8_t cmd_index = 0;
//    uint8_t c;

//    while (1) {
//        if (uart_recv(&c, TIMEOUT_FOREVER) == 0) {
//            cmd_buffer[cmd_index] = c;

//            switch (cmd_buffer[0]) {
//                case CMD_RESET: {
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                    break;
//                }
//                case CMD_ID: {
//                    // 必须正确发送"1ALS"，这是SUMP识别的关键
//                    uart_send((uint8_t *)"1ALS", 4, TIMEOUT_DEFAULT);
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                    break;
//                }
//                case CMD_METADATA: {
//                    // 完整元数据发送（补全0x20字段）
//                    send_byte(0x01, TIMEOUT_DEFAULT);        // 设备名Token
//                    send_string(LA_NAME, TIMEOUT_DEFAULT);   // 设备名
//                    send_byte(0x00, TIMEOUT_DEFAULT);        // 字符串结束
//                    
//                    send_byte(0x20, TIMEOUT_DEFAULT);        // 长格式通道数Token
//                    send_uint32(LA_MAX_CHANNELS, TIMEOUT_DEFAULT); // 8通道
//                    
//                    send_byte(0x21, TIMEOUT_DEFAULT);        // 采样内存Token
//                    send_uint32(getBufferSize(), TIMEOUT_DEFAULT);
//                    
//                    send_byte(0x22, TIMEOUT_DEFAULT);        // 动态内存Token
//                    send_uint32(0, TIMEOUT_DEFAULT);
//                    
//                    send_byte(0x23, TIMEOUT_DEFAULT);        // 最大采样率Token
//                    send_uint32(getMaxFrequency(), TIMEOUT_DEFAULT);
//                    
//                    send_byte(0x40, TIMEOUT_DEFAULT);        // 短格式通道数Token
//                    send_byte(LA_MAX_CHANNELS, TIMEOUT_DEFAULT);
//                    
//                    send_byte(0x41, TIMEOUT_DEFAULT);        // 协议版本Token
//                    send_byte(0x02, TIMEOUT_DEFAULT);
//                    
//                    send_byte(0x00, TIMEOUT_DEFAULT);        // 元数据结束
//                    
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                    break;
//                }
//                case CMD_ARM_BASIC_TRIGGER: {
//                    run();
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                    break;
//                }
//                case CMD_XON: {
//                    get_stop_cmd = 0;
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                    break;
//                }
//                case CMD_XOFF: {
//                    get_stop_cmd = 1;
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                    break;
//                }
//                case CMD_CAPTURE_SIZE: {
//                    cmd_index++;
//                    if (cmd_index < 5) continue;
//                    // 修正：SUMP协议中CAPTURE_SIZE格式是 [delayCount(2字节)][readCount(2字节)]
//                    uint16_t delayCount = *((uint16_t *)(cmd_buffer + 1));
//                    uint16_t readCount = *((uint16_t *)(cmd_buffer + 3));
//                    setSampleNumber(4 * readCount);
//                    setSamplingDelay(4 * delayCount);
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                    break;
//                }
//                case CMD_SET_DIVIDER: {
//                    cmd_index++;
//                    if (cmd_index < 5) continue;
//                    uint32_t divider = *((uint32_t *)(cmd_buffer + 1));
//                    setSamplingDivider(divider);
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                    break;
//                }
//                case CMD_SET_BASIC_TRIGGER_MASK0: {
//                    cmd_index++;
//                    if (cmd_index < 5) continue;
//                    setTriggerMask(*(uint32_t *)(cmd_buffer + 1));
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                    break;
//                }
//                case CMD_SET_BASIC_TRIGGER_VALUE0: {
//                    cmd_index++;
//                    if (cmd_index < 5) continue;
//                    setTriggerValue(*(uint32_t *)(cmd_buffer + 1));
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                    break;
//                }
//                case CMD_SET_BASIC_TRIGGER_CONFIG0: {
//                    cmd_index++;
//                    if (cmd_index < 5) continue;
//                    uint8_t serial = (*((uint8_t *)(cmd_buffer + 4)) & 0x04) > 0 ? 1 : 0;
//                    uint8_t state = (*((uint8_t *)(cmd_buffer + 4)) & 0x08) > 0 ? 1 : 0;
//                    setTriggerState(serial ? 0 : state);
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                    break;
//                }
//                case CMD_SET_FLAGS: {
//                    cmd_index++;
//                    if (cmd_index < 5) continue;
//                    setFlags(*(uint32_t *)(cmd_buffer + 1));
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                    break;
//                }
//                case CMD_CAPTURE_DELAYCOUNT: {
//                    cmd_index++;
//                    if (cmd_index < 5) continue;
//                    uint32_t delayCount = *((uint32_t *)(cmd_buffer + 1));
//                    setSamplingDelay(4 * delayCount);
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                    break;
//                }
//                case CMD_CAPTURE_READCOUNT: {
//                    cmd_index++;
//                    if (cmd_index < 5) continue;
//                    uint32_t readCount = *((uint32_t *)(cmd_buffer + 1));
//                    setSampleNumber(4 * readCount);
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                    break;
//                }
//                default: {
//                    cmd_index = 0;
//                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
//                }
//            }
//        }
//    }
//}
