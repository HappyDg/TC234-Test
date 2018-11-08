#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>

#include "tc27xc/IfxStm_reg.h"
#include "tc27xc/IfxStm_bf.h"
#include "tc27xc/IfxCpu_reg.h"
#include "tc27xc/IfxCpu_bf.h"

#include "system_tc2x.h"
#include "machine/intrinsics.h"
#include "interrupts.h"
#include "led.h"
#include "uart_int.h"

#include "asm_prototype.h"

#define SYSTIME_CLOCK	1000	/* timer event rate [Hz] */

volatile uint32_t g_sys_ticks;
volatile bool g_regular_task_flag;

/* timer callback handler */
static void my_timer_handler(void)
{
	++g_sys_ticks;

	if(0==g_sys_ticks%(2*SYSTIME_CLOCK))
	{
		LEDTOGGLE(1);
	}

}

const uint32_t HAL_GetTick(void)
{
	return g_sys_ticks;
}

#define SYSTIME_ISR_PRIO	2

#define STM0_BASE			((Ifx_STM *)&MODULE_STM0)

/* timer reload value (needed for subtick calculation) */
static unsigned int reload_value;

/* pointer to user specified timer callback function */
static TCF user_handler = (TCF)0;

static __inline Ifx_STM *systime_GetStmBase(void)
{
	switch (_mfcr(CPU_CORE_ID) & IFX_CPU_CORE_ID_CORE_ID_MSK)
	{
	case 0 :
	default :
		return STM0_BASE;
		break;
	}
}

/* timer interrupt routine */
static void tick_irq(int reload_value)
{
	Ifx_STM *StmBase = systime_GetStmBase();

	/* set new compare value */
	StmBase->CMP[0].U += (unsigned int)reload_value;
	if (user_handler)
	{
		user_handler();
	}
}

/* Initialise timer at rate <hz> */
void TimerInit(unsigned int hz)
{
	unsigned int CoreID = _mfcr(CPU_CORE_ID) & IFX_CPU_CORE_ID_CORE_ID_MSK;
	Ifx_STM *StmBase = systime_GetStmBase();
	unsigned int frequency = SYSTEM_GetStmClock();
	int irqId;

	reload_value = frequency / hz;

	switch (CoreID)
	{
	default :
	case 0 :
		irqId = SRC_ID_STM0SR0;
		break;
	}

	/* install handler for timer interrupt */
	InterruptInstall(irqId, tick_irq, SYSTIME_ISR_PRIO, (int)reload_value);

	/* reset interrupt flag */
	StmBase->ISCR.U = (IFX_STM_ISCR_CMP0IRR_MSK << IFX_STM_ISCR_CMP0IRR_OFF);
	/* prepare compare register */
	StmBase->CMP[0].U = StmBase->TIM0.U + reload_value;
	StmBase->CMCON.U  = 31;
	StmBase->ICR.B.CMP0EN = 1;
}

/* Install <handler> as timer callback function */
void TimerSetHandler(TCF handler)
{
	user_handler = handler;
}

static inline void flush_stdout(void)
{
	__asm__ volatile ("nop" ::: "memory");
	__asm volatile ("" : : : "memory");
	/* wait until sending has finished */
	while (_uart_sending())
	{
		;
	}
}

void TestFunc(void)
{
	printf("%s_%d\n", __func__, __LINE__);
}

int main(void)
{
	SYSTEM_Init();
	SYSTEM_EnaDisCache(1);

	/* initialise timer at SYSTIME_CLOCK rate */
	TimerInit(SYSTIME_CLOCK);
	/* add own handler for timer interrupts */
	TimerSetHandler(my_timer_handler);

	_init_uart(BAUDRATE);
	InitLED();

	printf("Tricore %04X Core:%04X, CPU:%u MHz,Sys:%u MHz,STM:%u MHz,CacheEn:%d\n",
			__TRICORE_NAME__,
			__TRICORE_CORE__,
			SYSTEM_GetCpuClock()/1000000,
			SYSTEM_GetSysClock()/1000000,
			SYSTEM_GetStmClock()/1000000,
			SYSTEM_IsCacheEnabled());

	flush_stdout();

	volatile uint32_t a;
	volatile uint32_t b;
	volatile uint32_t c;
	volatile uint32_t d;
	volatile uint32_t* p_a;
	volatile uint32_t* p_b;
	volatile uint32_t* p_c;
	volatile uint32_t* p_d;
	volatile pack32 packA;
	volatile pack32 packB;
	volatile pack32 packC;
	volatile pack32 packD;
	volatile int32_t ai;
	volatile int32_t bi;
	volatile int32_t ci;
	volatile int32_t di;
	volatile uint32_t res;
	volatile int32_t res_i;
	volatile uint64_t a64;
	volatile uint64_t res64;
	volatile float fA;
	volatile float fB;
	volatile float fC;
	volatile float f_res;
	volatile pack64 p_res_64;
	volatile pack64 p_a_64;
	volatile pack32 p_b_32;

	printf("\nTest OR\n");
	a = 0xFFFF0000;
	b = 0x22222222;
	c = Ifx_OR(a, b);
	printf("OR[%08X, %08X] = %08X\n", a, b, c);
	b = 0x11111111;
	c = Ifx_OR(a, b);
	printf("OR[%08X, %08X] = %08X\n", a, b, c);
	flush_stdout();

	printf("\nTest OR_T\n");
	a = 0;
	b = 0x22222222;
	c = Ifx_OR_T(a, b);
	printf("OR.T[%08X, %08X] = %08X\n", a, b, c);
	b = 0;
	c = Ifx_OR_T(a, b);
	printf("OR.T[%08X, %08X] = %08X\n", a, b, c);
	flush_stdout();

	printf("\nTest ORN\n");
	a = 0xFFFF0000;
	b = 0x22222222;
	c = Ifx_ORN(a, b);
	printf("ORN[%08X, %08X] = %08X\n", a, b, c);
	b = 0x11111111;
	c = Ifx_ORN(a, b);
	printf("ORN[%08X, %08X] = %08X\n", a, b, c);
	flush_stdout();

	printf("\nTest ORN_T\n");
	a = 0;
	b = 0x22222222;
	c = Ifx_ORN_T(a, b);
	printf("ORN.T[%08X, %08X] = %08X\n", a, b, c);
	b = 0;
	c = Ifx_ORN_T(a, b);
	printf("ORN.T[%08X, %08X] = %08X\n", a, b, c);
	flush_stdout();

	printf("\nTest OR.EQ\n");
	a = 0xFFFF0000;
	b = 0x22222222;
	c = 0x11111111;
	d = Ifx_OR_EQ(a, b, c);
	printf("OR.EQ[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	b = c;
	d = Ifx_OR_EQ(a, b, c);
	printf("OR.EQ[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	flush_stdout();

	printf("\nTest OR.NE\n");
	a = 0xFFFF0000;
	b = 0x22222222;
	c = 0x11111111;
	d = Ifx_OR_NE(a, b, c);
	printf("OR.NE[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	b = c;
	d = Ifx_OR_NE(a, b, c);
	printf("OR.NE[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	flush_stdout();

	printf("\nTest OR.GE\n");
	a = 0xFFFF0000;
	b = 0x22222222;
	c = 0x11111111;
	d = Ifx_OR_GE(a, b, c);
	printf("OR.GE[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	c = 0x99999999;
	d = Ifx_OR_GE(a, b, c);
	printf("OR.GE[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	flush_stdout();

	printf("\nTest OR.GE.U\n");
	a = 0xFFFF0000;
	b = 0x22222222;
	c = 0x11111111;
	d = Ifx_OR_GE_U(a, b, c);
	printf("OR.GE.U[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	c = 0x99999999;
	d = Ifx_OR_GE_U(a, b, c);
	printf("OR.GE.U[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	flush_stdout();

	printf("\nTest OR.LT\n");
	a = 0xFFFF0000;
	b = 0x22222222;
	c = 0x11111111;
	d = Ifx_OR_LT(a, b, c);
	printf("OR.LT[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	c = 0x99999999;
	d = Ifx_OR_LT(a, b, c);
	printf("OR.LT[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	flush_stdout();

	printf("\nTest OR.LT.U\n");
	a = 0xFFFF0000;
	b = 0x22222222;
	c = 0x11111111;
	d = Ifx_OR_LT_U(a, b, c);
	printf("OR.LT.U[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	c = 0x99999999;
	d = Ifx_OR_LT_U(a, b, c);
	printf("OR.LT.U[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	flush_stdout();

	printf("\nTest OR.AND.T\n");
	a = 0xFFFF0000;
	b = 0x22222222;
	c = 0x11111111;
	d = Ifx_OR_AND_T(a, b, c);
	printf("OR.AND.T[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	c = 0x99999999;
	d = Ifx_OR_AND_T(a, b, c);
	printf("OR.AND.T[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	flush_stdout();

	printf("\nTest OR.ANDN.T\n");
	a = 0xFFFF0000;
	b = 0x22222222;
	c = 0x11111111;
	d = Ifx_OR_ANDN_T(a, b, c);
	printf("OR.ANDN.T[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	c = 0x99999999;
	d = Ifx_OR_ANDN_T(a, b, c);
	printf("OR.ANDN.T[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	flush_stdout();

	printf("\nTest OR.OR.T\n");
	a = 0xFFFF0000;
	b = 0x22222222;
	c = 0x11111111;
	d = Ifx_OR_OR_T(a, b, c);
	printf("OR.OR.T[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	c = 0x99999999;
	d = Ifx_OR_OR_T(a, b, c);
	printf("OR.OR.T[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	flush_stdout();

	printf("\nTest OR.NOR.T\n");
	a = 0xFFFF0000;
	b = 0x22222222;
	c = 0x11111111;
	d = Ifx_OR_NOR_T(a, b, c);
	printf("OR.NOR.T[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	c = 0x99999999;
	d = Ifx_OR_NOR_T(a, b, c);
	printf("OR.NOR.T[%08X, %08X,%08X] = %08X\n", a, b, c, d);
	flush_stdout();

	g_regular_task_flag = true;
	while(1)
	{
		if(0==g_sys_ticks%(20*SYSTIME_CLOCK))
		{
			g_regular_task_flag = true;
		}

		if(g_regular_task_flag)
		{
			g_regular_task_flag = false;

			LEDTOGGLE(0);

			printf("Tricore %04X Core:%04X, CPU:%u MHz,Sys:%u MHz,STM:%u MHz,CacheEn:%d, %u\n",
					__TRICORE_NAME__,
					__TRICORE_CORE__,
					SYSTEM_GetCpuClock()/1000000,
					SYSTEM_GetSysClock()/1000000,
					SYSTEM_GetStmClock()/1000000,
					SYSTEM_IsCacheEnabled(),
					HAL_GetTick());

			flush_stdout();
		}
	}

	return EXIT_SUCCESS;
}

