//
// Created by yuqiri on 2025/11/26.
//

#include "la_sump.h"
#include "la_hw.h"
#include "string.h"



/* Command opcodes */
#define CMD_RESET                     0x00
#define CMD_ARM_BASIC_TRIGGER         0x01
#define CMD_ID                        0x02
#define CMD_METADATA                  0x04
#define CMD_FINISH_NOW                0x05 /* extension of Demon Core */
#define CMD_QUERY_INPUT_DATA          0x06 /* extension of Demon Core */
#define CMD_QUERY_CAPTURE_STATE       0x07 /* extension of Demon Core */
#define CMD_RETURN_CAPTURE_DATA       0x08 /* extension of Demon Core */
#define CMD_ARM_ADVANCED_TRIGGER      0x0F /* extension of Demon Core */
#define CMD_XON                       0x11
#define CMD_XOFF                      0x13
#define CMD_SET_DIVIDER               0x80
#define CMD_CAPTURE_SIZE              0x81
#define CMD_SET_FLAGS                 0x82
#define CMD_CAPTURE_DELAYCOUNT        0x83 /* extension of Pepino */
#define CMD_CAPTURE_READCOUNT         0x84 /* extension of Pepino */
#define CMD_SET_ADVANCED_TRIG_SEL     0x9E /* extension of Demon Core */
#define CMD_SET_ADVANCED_TRIG_WRITE   0x9F /* extension of Demon Core */
#define CMD_SET_BASIC_TRIGGER_MASK0   0xC0 /* 4 stages: 0xC0, 0xC4, 0xC8, 0xCC */
#define CMD_SET_BASIC_TRIGGER_VALUE0  0xC1 /* 4 stages: 0xC1, 0xC5, 0xC9, 0xCD */
#define CMD_SET_BASIC_TRIGGER_CONFIG0 0xC2 /* 4 stages: 0xC2, 0xC6, 0xCA, 0xCE */

/* Metadata tokens */
#define METADATA_TOKEN_END                    0x0
#define METADATA_TOKEN_DEVICE_NAME            0x1
#define METADATA_TOKEN_FPGA_VERSION           0x2
#define METADATA_TOKEN_ANCILLARY_VERSION      0x3
#define METADATA_TOKEN_NUM_PROBES_LONG        0x20
#define METADATA_TOKEN_SAMPLE_MEMORY_BYTES    0x21
#define METADATA_TOKEN_DYNAMIC_MEMORY_BYTES   0x22
#define METADATA_TOKEN_MAX_SAMPLE_RATE_HZ     0x23
#define METADATA_TOKEN_PROTOCOL_VERSION_LONG  0x24
#define METADATA_TOKEN_CAPABILITIES           0x25 /* not implemented in Demon Core v3.07 */
#define METADATA_TOKEN_NUM_PROBES_SHORT       0x40
#define METADATA_TOKEN_PROTOCOL_VERSION_SHORT 0x41

#define RxReceiveTimeout HAL_MAX_DELAY
#define LA_MAX_CHANNELS 0x10
#define LA_MAX_FREQUENCY 10000000


la_status LA_UARTGetCharTimeout(uint8_t *pVal, int timeout);

/* 扫描（scan） */
#define LA_NAME "LA_YUQIRI"
#define LA_TIMEOUT 1000
#define LA_RX_BUFSIZE 100
#define LA_TIMEFOREVER ~0ul
// static uint8_t la_rx_char;
// static CircleBuf la_rx_buf;
// static p_circle_buf g_la_rx_buf;
// static uint8_t la_tem_buf[LA_RX_BUFSIZE];

static uint8_t la_temp_buf[LA_RX_BUFSIZE];
//static volatile int la_rx_flag = 0;
//static volatile int la_tx_flag = 0;

/* 参数设置（configuration） */

static uint32_t triggerMask 	= 0;
static uint32_t triggerValue 	= 0;
static uint32_t triggerConfig	= 0;
static uint32_t Divider 		= 0;
static uint32_t ReadCount 		= 0;
static uint32_t DelayCount	 	= 0;
static uint32_t LA_Flags		= 0;
static uint32_t sampleNumber	= 0;
static uint32_t Flags		= 0;

static void setTriggerMask(uint32_t s){
	triggerMask = s;
}

static void setTriggerValue(uint32_t s){
	triggerValue = s;
}

static void setTriggerConfig(uint32_t s){
	triggerConfig = s;
}

static void setDivider(uint32_t s){
	Divider = s;
}

static void setLA_Flags(uint32_t s){
	LA_Flags = s;
}

static uint32_t min(uint32_t a,uint32_t b){
	return (a < b) ? a : b ;
}

static void setsampleNumber(uint32_t s){
	sampleNumber = min((uint32_t )LA_RX_BUFSIZE,s);
}

static void setDelayCount(uint32_t s){
	DelayCount = s;
}
					
static void setReadCount(uint32_t s){
	ReadCount = s;
}

static void setFlags(uint32_t s){
	Flags = s;
}


/* 采样环节 */
static void start(void){
/* 创建模拟采样数据，测试是否能正常跑通 */
	for(uint32_t i = 0 ; i < sampleNumber; i++){
		la_temp_buf[i] = i;
	}
}

static void run(void){
	/* 数据采样 */
	start();
	
	/* 发送采样数据 */
	LA_Send(la_temp_buf , sampleNumber );
}



void LogicalAnalyser()
{

    uint8_t cmd_buf[5];
    uint8_t cmd_index = 0;

    uint8_t cmd_byte;
    while (1)
    {
        if (LA_UARTGetCharTimeout(&cmd_byte, LA_TIMEFOREVER) == LA_OK)
        {
            cmd_buf[cmd_index] = cmd_byte;
			
            switch (cmd_buf[0])
            {
            case CMD_RESET:
                {
                    break;
                }
            case CMD_ID:
                {
                    LA_Send_4Byte((uint8_t *)"1ALS");
                    break;
                }
            case CMD_METADATA:
                {
                    LA_Send_byte((uint8_t *)0x01);
                    LA_Send_String((uint8_t *)LA_NAME);
                    LA_Send_byte(0x00);
					
                    LA_Send_byte((uint8_t *)0x20);
                    LA_Send_BigEndian(LA_MAX_CHANNELS,la_temp_buf);
					
                    LA_Send_byte((uint8_t *)0x21);
                    LA_Send_BigEndian(LA_RX_BUFSIZE,la_temp_buf);
					
                    LA_Send_byte((uint8_t *)0x23);
                    LA_Send_BigEndian(LA_MAX_FREQUENCY,la_temp_buf);
					
					LA_Send_byte((uint8_t *)0x40);
                    LA_Send_byte((uint8_t *)0x02);
					
                    LA_Send_byte(0x00);
                    break;
                }
			case CMD_SET_BASIC_TRIGGER_MASK0:
				{
					/* 接收数据 */
					cmd_index++;
					if(cmd_index < 5){
						continue;
					}
					//循环5次，是因为上位机连续发5个字节，
					//若不循环五次，就只接收第一个字节，剩下4字节消失
					
					/* 配置mask参数，小字节序 */
					triggerMask = *(uint32_t * )(cmd_buf + 1);
					//第一个 * (解引用操作符) 的意思：
					//从 cmd_buffer 的第 1 个字节开始，
					//连续读取 4 个字节，
					//把它们解释为一个 32 位无符号整数
					setTriggerMask(triggerMask);
					break;
				}
			case CMD_SET_BASIC_TRIGGER_VALUE0:
				{
					cmd_index++;
					if(cmd_index < 5){
						continue;
					}
				
					triggerValue = *(uint32_t * )(cmd_buf + 1);
					setTriggerValue(triggerValue);
					break;
				}
			case CMD_SET_BASIC_TRIGGER_CONFIG0:
				{
					cmd_index++;
					if(cmd_index < 5){
						continue;
					}
				
					triggerConfig = *(uint32_t * )(cmd_buf + 1);
					setTriggerConfig(triggerConfig);
					break;
				}
			case CMD_SET_DIVIDER:
				{
					cmd_index++;
					if(cmd_index < 5){
						continue;
					}
				
					Divider = *(uint32_t * )(cmd_buf + 1);
					setDivider(Divider);
					break;
				}
			/* 下位机采样buffer超过256K，接收以下三条指令 */
			case CMD_CAPTURE_READCOUNT:
				{
					cmd_index++;
					if(cmd_index < 5){
						continue;
					}
				
					// Deprecated: sample count is now set via CMD_CAPTURE_SIZE

					break;
				}
			case CMD_CAPTURE_DELAYCOUNT:
				{
					cmd_index++;
					if(cmd_index < 5){
						continue;
					}
				
					uint32_t DelayCount = *(uint32_t * )(cmd_buf + 1 );
					setDelayCount(4 * DelayCount);
					break;
				}
			case CMD_CAPTURE_SIZE:
				{
					cmd_index++;
					if(cmd_index < 5){
						continue;
					}
					
					uint32_t DelayCount = *(uint32_t * )(cmd_buf + 1 );
					uint32_t ReadCount = *(uint32_t * )(cmd_buf + 1 );
					setDelayCount(4 * DelayCount);
					setReadCount(4 * ReadCount);
				}
				
			case CMD_SET_FLAGS:
				{
					cmd_index++;
					if(cmd_index < 5){
						continue;
					}
					
					setFlags(*(uint32_t *)(cmd_buf + 1));
				}
				
			/* 采样 */
			case CMD_ARM_BASIC_TRIGGER:
				{
					run();
				}
			case CMD_XON:
				{
					break;
				}
			case CMD_XOFF:
				{
					break;
				}
			default:
				{
				}
            }
			memset(cmd_buf,0,sizeof(cmd_buf));
        }
    }





}

