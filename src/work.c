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

	printf("%s CPU:%u MHz,Sys:%u MHz,STM:%u MHz,CacheEn:%d\n",
			__TIME__,
			SYSTEM_GetCpuClock()/1000000,
			SYSTEM_GetSysClock()/1000000,
			SYSTEM_GetStmClock()/1000000,
			SYSTEM_IsCacheEnabled());

	flush_stdout();

	volatile uint32_t a;
	volatile uint32_t b;
	volatile uint32_t c;
	volatile uint32_t d;
	volatile pack32 packA;
	volatile pack32 packB;
	volatile pack32 packC;
	volatile pack32 packD;
	volatile int32_t ai;
	volatile uint32_t res;
	volatile int32_t res_i;
	volatile uint64_t a64;
	volatile uint64_t res64;
	volatile float fA;
	volatile float fB;
	volatile float f_res;
	volatile pack64 p_res_64;

	printf("\nTest EQ\n");
	a = 0x11223344;
	b = 0x55667788;
	res = Ifx_Eq(a, b);
	printf("EQ[%08X,%08X] = %08X\n", a, b, res);
	a = 0x11223344;
	b = a;
	res = Ifx_Eq(a, b);
	printf("EQ[%08X,%08X] = %08X\n", a, b, res);
	flush_stdout();

	printf("\nTest EQ.B\n");
	packA.u32 = 0x11223344;
	packB.u32 = 0x11663388;
	res = Ifx_Eq_B(packA.u32, packB.u32);
	printf("EQ.B[%08X,%08X] = %08X\n", packA.u32, packB.u32, res);
	flush_stdout();

	printf("\nTest EQ.H\n");
	packA.u32 = 0x11223344;
	packB.u32 = 0x11223388;
	res = Ifx_Eq_H(packA.u32, packB.u32);
	printf("EQ.H[%08X,%08X] = %08X\n", packA.u32, packB.u32, res);
	flush_stdout();

	printf("\nTest EQ.W\n");
	packA.u32 = 0x11223344;
	packB.u32 = 0x11223388;
	res = Ifx_Eq_W(packA.u32, packB.u32);
	printf("EQ.W[%08X,%08X] = %08X\n", packA.u32, packB.u32, res);
	packA.u32 = 0x11223344;
	packB.u32 = packA.u32;
	res = Ifx_Eq_W(packA.u32, packB.u32);
	printf("EQ.W[%08X,%08X] = %08X\n", packA.u32, packB.u32, res);
	flush_stdout();

	printf("\nTest EQANY.B\n");
	packA.u32 = 0x11223344;
	packB.u32 = 0x11663388;
	res = Ifx_EqAny_B(packA.u32, packB.u32);
	printf("EQANY.B[%08X,%08X] = %08X\n", packA.u32, packB.u32, res);
	packA.u32 = 0x11223344;
	packB.u32 = 0x44;
	res = Ifx_EqAny_B(packA.u32, packB.u32);
	printf("EQANY.B[%08X,%08X] = %08X\n", packA.u32, packB.u32, res);
	flush_stdout();

	printf("\nTest EQANY.H\n");
	packA.u32 = 0x11223344;
	packB.u32 = 0x11663388;
	res = Ifx_EqAny_H(packA.u32, packB.u32);
	printf("EQANY.H[%08X,%08X] = %08X\n", packA.u32, packB.u32, res);
	packA.u32 = 0x11223344;
	packB.u32 = 0x3344;
	res = Ifx_EqAny_H(packA.u32, packB.u32);
	printf("EQANY.H[%08X,%08X] = %08X\n", packA.u32, packB.u32, res);
	flush_stdout();

	printf("\nTest EQ.A\n");
	res = Ifx_Eq_A(&a, &b);
	printf("EQ.A[%08X,%08X] = %08X\n", &a, &b, res);
	res = Ifx_Eq_A(&a, &a);
	printf("EQ.A[%08X,%08X] = %08X\n", &a, &a, res);
	flush_stdout();

	printf("\nTest EQZ.A\n");
	res = Ifx_Eq_Z_A(&a);
	printf("EQZ.A[%08X] = %08X\n", &a, res);
	void* pNull = 0;
	res = Ifx_Eq_Z_A(pNull);
	printf("EQZ.A[%08X] = %08X\n", pNull, res);
	flush_stdout();

	printf("\nTest FTOI\n");
	fA = -1234.5678;
	res_i = Ifx_Ftoi(fA);
	printf("FTOI[%f] = %d\n", fA, res_i);
	fA = -0.9999999;
	res_i = Ifx_Ftoi(fA);
	printf("FTOI[%f] = %d\n", fA, res_i);
	flush_stdout();

	printf("\nTest FTOI_Z\n");
	fA = -1234.5678;
	res_i = Ifx_Ftoi_Z(fA);
	printf("FTOIZ[%f] = %d\n", fA, res_i);
	fA = -0.9999999;
	res_i = Ifx_Ftoi_Z(fA);
	printf("FTOIZ[%f] = %d\n", fA, res_i);
	flush_stdout();

	printf("\nTest FTOU\n");
	fA = 1234.5678;
	res_i = Ifx_Ftou(fA);
	printf("FTOU[%f] = %d\n", fA, res_i);
	fA = 1.99999999;
	res_i = Ifx_Ftou(fA);
	printf("FTOU[%f] = %d\n", fA, res_i);
	flush_stdout();

	printf("\nTest FTOU_Z\n");
	fA = 1234.5678;
	res_i = Ifx_Ftou_Z(fA);
	printf("FTOU_Z[%f] = %d\n", fA, res_i);
	fA = 1.99999999;
	res_i = Ifx_Ftou_Z(fA);
	printf("FTOU_Z[%f] = %d\n", fA, res_i);
	flush_stdout();

	printf("\nTest EXTR\n");
	a = 0xF1223344;
	b = 8;
	res = Ifx_Extr(a, b);
	printf("EXTR[%08X,%08X, %u] = %08X\n", a, b, 4, res);
	a = 0xF1223344;
	b = 24;
	res = Ifx_Extr(a, b);
	printf("EXTR[%08X,%08X, %u] = %08X\n", a, b, 4, res);
	flush_stdout();

	printf("\nTest EXTR_U\n");
	a = 0xF1223344;
	b = 8;
	res = Ifx_Extr_U(a, b);
	printf("EXTR_U[%08X,%08X, %u] = %08X\n", a, b, 4, res);
	a = 0xF1223344;
	b = 24;
	res = Ifx_Extr_U(a, b);
	printf("EXTR_U[%08X,%08X, %u] = %08X\n", a, b, 4, res);
	flush_stdout();

	printf("\nTest FTOQ31\n");
	fA = 1.2345678;
	b = 4;
	res = Ifx_Ftoq31(fA, b);
	printf("FTOQ31[%f, %u] = %08X\n", fA, b, res);
	fA = 0.5678;
	b = 3;
	res = Ifx_Ftoq31(fA, b);
	printf("FTOQ31[%f, %u] = %08X\n", fA, b, res);
	flush_stdout();

	printf("\nTest FTOQ31Z\n");
	fA = 1.2345678;
	b = 2;
	res = Ifx_Ftoq31z(fA, b);
	printf("FTOQ31Z[%f, %u] = %08X\n", fA, b, res);
	fA = 0.5678;
	b = 1;
	res = Ifx_Ftoq31z(fA, b);
	printf("FTOQ31Z[%f, %u] = %08X\n", fA, b, res);
	flush_stdout();

	printf("\nTest Fcall EQ\n");
	a = 0x11223344;
	b = 0x55667788;
	res = Ifx_Fcall(a, b);
	printf("Fcall EQ[%08X,%08X] = %08X\n", a, b, res);
	a = 0x11223344;
	b = a;
	res = Ifx_Fcall(a, b);
	printf("Fcall EQ[%08X,%08X] = %08X\n", a, b, res);
	flush_stdout();

	printf("\nTest FcallA EQ\n");
	a = 0x11223344;
	b = 0x55667788;
	res = Ifx_Fcall_A(a, b);
	printf("FcallA EQ[%08X,%08X] = %08X\n", a, b, res);
	a = 0x11223344;
	b = a;
	res = Ifx_Fcall_A(a, b);
	printf("FcallA EQ[%08X,%08X] = %08X\n", a, b, res);
	flush_stdout();

	printf("\nTest FcallI EQ\n");
	a = 0x11223344;
	b = 0x55667788;
	res = Ifx_Fcall_I(Ifx_Eq_fast, a, b);
	printf("FcallI EQ[%p %08X,%08X] = %08X\n", Ifx_Eq_fast, a, b, res);
	a = 0x11223344;
	b = a;
	res = Ifx_Fcall_I(Ifx_Eq_fast, a, b);
	printf("FcallI EQ[%08X %08X,%08X] = %08X\n", Ifx_Eq_fast, a, b, res);
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

			printf("%s CPU:%u MHz,Sys:%u MHz, %u, CacheEn:%d\n",
					__TIME__,
					SYSTEM_GetCpuClock()/1000000,
					SYSTEM_GetSysClock()/1000000,
					HAL_GetTick(),
					SYSTEM_IsCacheEnabled());

			__asm__ volatile ("nop" ::: "memory");
			__asm volatile ("" : : : "memory");
			/* wait until sending has finished */
			while (_uart_sending())
				;
		}
	}

	return EXIT_SUCCESS;
}

