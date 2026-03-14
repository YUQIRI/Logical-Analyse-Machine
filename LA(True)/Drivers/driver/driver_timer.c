/*  Copyright (s) 2019 深圳百问网科技有限公司
 *  All rights reserved
 * 
 * 文件名称：driver_timer.c
 * 摘要：
 *  
 * 修改历史     版本号        Author       修改内容
 *--------------------------------------------------
 * 2023.8.03      v01         百问科技      创建文件
 *--------------------------------------------------
*/

#include "driver_timer.h"

#include "stm32f1xx_hal.h"
#include "cmsis_version.h"

//#define USE_SYSTICK

extern TIM_HandleTypeDef        htim4;

/**********************************************************************
 * 函数名称： udelay
 * 功能描述： us级别的延时函数(复制rt-thread的代码)
 * 输入参数： us - 延时多少us
 * 输出参数： 无
 * 返 回 值： 无
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2023/08/03	     V1.0	  韦东山	      创建
 ***********************************************************************/
void udelay(int us)
{
#ifdef USE_SYSTICK	
	uint32_t told = SysTick->VAL;
	uint32_t tnow;
	
	uint32_t load = SysTick->LOAD;
	
	/* LOAD+1个时钟对应1ms
	 * n us对应 n*(load+1)/1000个时钟
   */
	uint32_t ticks = us*(load+1)/1000;
	
	uint32_t cnt = 0;
	
	while (1)
	{
		tnow = SysTick->VAL;
		if (told >= tnow)
			cnt += told - tnow;
		else
			cnt += told + load + 1 - tnow;
		
		told = tnow;
		if (cnt >= ticks)
			break;
	}	
#else
	uint32_t told = __HAL_TIM_GET_COUNTER(&htim4); // timer4的计数值
	uint32_t tnow;
	
	uint32_t load =  __HAL_TIM_GET_AUTORELOAD(&htim4); // timer4的auto-reload值
	
	/* LOAD+1个时钟对应1ms
	 * n us对应 n*(load+1)/1000个时钟
   */
	uint32_t ticks = us*(load+1)/1000;
	
	uint32_t cnt = 0;
	
	while (1)
	{
		tnow = __HAL_TIM_GET_COUNTER(&htim4);
		if (tnow >= told)
			cnt += tnow - told;
		else
			cnt += load + 1 - told + tnow;
		
		told = tnow;
		if (cnt >= ticks)
			break;
	}	
#endif	
}

/**********************************************************************
 * 函数名称： mdelay
 * 功能描述： ms级别的延时函数
 * 输入参数： ms - 延时多少ms
 * 输出参数： 无
 * 返 回 值： 无
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2023/08/03	     V1.0	  韦东山	      创建
 ***********************************************************************/
void mdelay(int ms)
{
	for (int i = 0; i < ms; i++)
		udelay(1000);
}

/**********************************************************************
 * 函数名称： system_get_ns
 * 功能描述： 获得系统时间(单位ns)
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 系统时间(单位ns)
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2023/08/03	     V1.0	  韦东山	      创建
 ***********************************************************************/
uint64_t system_get_ns(void)
{
#ifdef USE_SYSTICK	
	uint64_t ns = HAL_GetTick(); /* ms */
	ns = ns*1000000;

	uint32_t tnow = SysTick->VAL;	
	uint32_t load = SysTick->LOAD;
	
	uint64_t cnt;
	
	cnt = load+1-tnow; /* 没有考虑tnow等于0的情况 */
	
	ns += cnt * 1000000 / (load+1) ;
	return ns;
#else
	uint64_t ns = HAL_GetTick(); /* ms */
	ns = ns*1000000;

	uint32_t tnow = __HAL_TIM_GET_COUNTER(&htim4); // timer4的计数值
	uint32_t load = __HAL_TIM_GET_AUTORELOAD(&htim4); // timer4的auto-reload值
	
	uint64_t cnt;
	
	cnt = tnow; /* 没有考虑tnow等于0的情况 */
	
	ns += cnt * 1000000 / (load+1) ;
	return ns;
#endif	
}

/**********************************************************************
 * 函数名称： Disable_TickIRQ
 * 功能描述： 禁止tick中断
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 无
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/07/05	     V1.0	  韦东山	      创建
 ***********************************************************************/
void Disable_TickIRQ(void)
{
    HAL_NVIC_DisableIRQ(TIM4_IRQn);
}

/**********************************************************************
 * 函数名称： Enable_TickIRQ
 * 功能描述： 使能tick中断
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 无
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/07/05	     V1.0	  韦东山	      创建
 ***********************************************************************/
void Enable_TickIRQ(void)
{
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
}


