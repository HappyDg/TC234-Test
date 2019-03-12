#include <machine/intrinsics.h>
#include "bspconfig_tc23x.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "FreeRTOS.h"
#include "task.h"
#include "croutine.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "event_groups.h"

#include TC_INCLUDE(TCPATH/IfxCpu_reg.h)
#include TC_INCLUDE(TCPATH/IfxCpu_bf.h)
#include TC_INCLUDE(TCPATH/IfxEth_reg.h)
#include TC_INCLUDE(TCPATH/IfxEth_bf.h)
#include TC_INCLUDE(TCPATH/IfxScu_reg.h)
#include TC_INCLUDE(TCPATH/IfxScu_bf.h)
#include TC_INCLUDE(TCPATH/IfxInt_reg.h)
#include TC_INCLUDE(TCPATH/IfxInt_bf.h)
#include TC_INCLUDE(TCPATH/IfxSrc_reg.h)
#include TC_INCLUDE(TCPATH/IfxSrc_bf.h)
#include TC_INCLUDE(TCPATH/IfxQspi_reg.h)
#include TC_INCLUDE(TCPATH/IfxQspi_bf.h)
#include TC_INCLUDE(TCPATH/IfxEth_reg.h)
#include TC_INCLUDE(TCPATH/IfxEth_bf.h)

#include "core_tc23x.h"
#include "system_tc2x.h"
#include "cint_trap_tc23x.h"
#include "interrupts_tc23x.h"
#include "led.h"
#include "uart_int.h"
#include "asm_prototype.h"
#include "dts.h"
#include "timer.h"
#include "gpsr.h"
#include "scu_eru.h"
#include "data_flash.h"

#include "partest.h"

#include "enc28j60.h"
#include "ip_arp_udp_tcp.h"
#include "net.h"
#include "timer.h"

#define MESSAGE_Q_NUM   2
QueueHandle_t Message_Queue;

SemaphoreHandle_t BinarySemaphore;

SemaphoreHandle_t CountSemaphore;

SemaphoreHandle_t MutexSemaphore;
/*----------------------------------------------------------*/

/* Constants for the ComTest tasks. */
#define mainCOM_TEST_BAUD_RATE		( ( uint32_t ) BAUDRATE )

#define mainCOM_TEST_LED			( 4 )
/* Set mainCREATE_SIMPLE_LED_FLASHER_DEMO_ONLY to 1 to create a simple demo.
Set mainCREATE_SIMPLE_LED_FLASHER_DEMO_ONLY to 0 to create a much more
comprehensive test application.  See the comments at the top of this file, and
the documentation page on the http://www.FreeRTOS.org web site for more
information. */
#define mainCREATE_SIMPLE_LED_FLASHER_DEMO_ONLY		1

/*-----------------------------------------------------------*/

static void prvSetupHardware( void );

/*-----------------------------------------------------------*/

TaskHandle_t StartTask_Handler;
void start_task(void *pvParameters);

TaskHandle_t g_task0_handler;
void maintaince_task(void *pvParameters);

TaskHandle_t g_info_task_handler;
void print_task(void *pvParameters);

void test_tlf35584(void) {
	tlf35584_cmd_t tmp_tlf_cmd;
	tlf35584_cmd_t res_tlf_cmd;

	for(uint8_t i=0; i<0x34; ++i) {
		tmp_tlf_cmd.B.cmd = 0;
		tmp_tlf_cmd.B.addr = i;
		tmp_tlf_cmd.B.data = 0;
		tmp_tlf_cmd.B.parity = 0;
		pack32 z_parity;
		z_parity.u32 = Ifx_PARITY((uint32_t)tmp_tlf_cmd.U);
		tmp_tlf_cmd.B.parity = z_parity.u8[0] ^ z_parity.u8[1];

		QSPI2_DATAENTRY0.B.E = (uint32_t)tmp_tlf_cmd.U;
		/* wait until transfer is complete */
		while (!QSPI2_STATUS.B.TXF)
			;
		/* clear TX flag */
		QSPI2_FLAGSCLEAR.B.TXC = 1;
		/* wait for receive is finished */
		while (!QSPI2_STATUS.B.RXF)
			;
		/* clear RX flag */
		QSPI2_FLAGSCLEAR.B.RXC = 1;

		//read
		res_tlf_cmd.U = QSPI2_RXEXIT.U;
		printf("%04X [%02X]=%02X\n",
				res_tlf_cmd.U,
				tmp_tlf_cmd.B.addr,
				res_tlf_cmd.B.data
		);
		flush_stdout();
	}

	{
		tmp_tlf_cmd.B.cmd = 0;
		tmp_tlf_cmd.B.addr = 0x3f;
		tmp_tlf_cmd.B.data = 0;
		tmp_tlf_cmd.B.parity = 0;
		pack32 z_parity;
		z_parity.u32 = Ifx_PARITY((uint32_t)tmp_tlf_cmd.U);
		tmp_tlf_cmd.B.parity = z_parity.u8[0] ^ z_parity.u8[1];

		QSPI2_DATAENTRY0.B.E = (uint32_t)tmp_tlf_cmd.U;
		/* wait until transfer is complete */
		while (!QSPI2_STATUS.B.TXF)
			;
		/* clear TX flag */
		QSPI2_FLAGSCLEAR.B.TXC = 1;
		/* wait for receive is finished */
		while (!QSPI2_STATUS.B.RXF)
			;
		/* clear RX flag */
		QSPI2_FLAGSCLEAR.B.RXC = 1;
		//read
		res_tlf_cmd.U = QSPI2_RXEXIT.U;
		printf("%04X [%02X]=%02X\n",
				res_tlf_cmd.U,
				tmp_tlf_cmd.B.addr,
				res_tlf_cmd.B.data
		);
		flush_stdout();
	}
}

extern void interface_init(void);
extern void server_loop(void);

extern const uint8_t g_ip_addr[4];
extern const uint8_t g_mac_addr[6];

const char ON_STR[] = "On";
const char OFF_STR[] = "Off";

const char ON_0_STR[] = "On0";
const char OFF_0_STR[] = "Off0";
const char ON_1_STR[] = "On1";
const char OFF_1_STR[] = "Off1";
const char ON_2_STR[] = "On2";
const char OFF_2_STR[] = "Off2";
const char ON_3_STR[] = "On3";
const char OFF_3_STR[] = "Off3";

int16_t analyse_get_url(char *str) {
	char* p_str = str;

	if (0==strncmp(p_str, ON_0_STR, strlen(ON_0_STR))) {
		return CMD_LD0_ON;
	} else if (0==strncmp(p_str, OFF_0_STR, strlen(OFF_0_STR))) {
		return CMD_LD0_OFF;
	} else if (0==strncmp(p_str, ON_1_STR, strlen(ON_1_STR))) {
		return CMD_LD1_ON;
	} else if (0==strncmp(p_str, OFF_1_STR, strlen(OFF_1_STR))) {
		return CMD_LD1_OFF;
	} else if (0==strncmp(p_str, ON_2_STR, strlen(ON_2_STR))) {
		return CMD_LD2_ON;
	} else if (0==strncmp(p_str, OFF_2_STR, strlen(OFF_2_STR))) {
		return CMD_LD2_OFF;
	} else if (0==strncmp(p_str, ON_3_STR, strlen(ON_3_STR))) {
		return CMD_LD3_ON;
	} else if (0==strncmp(p_str, OFF_3_STR, strlen(OFF_3_STR))) {
		return CMD_LD3_OFF;
	} else {
		return(-1);
	}
}

uint16_t prepare_page(uint8_t *buf) {
	uint16_t plen;
	uint8_t tmp_compose_buf[512];

	plen=fill_tcp_data_p(buf,0,("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nPragma: no-cache\r\n\r\n"));

	plen=fill_tcp_data_p(buf,plen,("<!DOCTYPE html>\r\n<html lang=\"en\">\r\n"));

	sprintf((char*)tmp_compose_buf, "<title>Server[%s]</title>\r\n", __TIME__);
	plen=fill_tcp_data(buf,plen,(const char*)tmp_compose_buf);

	plen=fill_tcp_data_p(buf,plen,("<body>\r\n"));
	plen=fill_tcp_data_p(buf,plen,("<center>\r\n"));

	sprintf((char*)tmp_compose_buf, "<p>DieTempSensor: %f 'C\r\n", read_dts_celsius());
	plen=fill_tcp_data(buf,plen,(const char*)tmp_compose_buf);
	plen=fill_tcp_data_p(buf,plen,("<p>Server on TC234 Appkit board\r\n"));

	sprintf((char*)tmp_compose_buf, "<p>cpu: %u M, Sys:%u M, STM0.TIM1:%08X, soft_spi:%u, newlib:%s\r\n",
			SYSTEM_GetCpuClock()/1000000,
			SYSTEM_GetSysClock()/1000000,
			MODULE_STM0.TIM1.U,
			SOFT_SPI,
			_NEWLIB_VERSION);
	plen=fill_tcp_data(buf,plen,(const char*)tmp_compose_buf);

	//LED 0 Control hypelink
	plen=fill_tcp_data_p(buf,plen,("\r\n<p>LED 0 Status:"));
	if ((LED_ON_STAT == led_stat(0))){
		plen=fill_tcp_data_p(buf,plen,("<font color=\"red\">"));
		plen=fill_tcp_data_p(buf,plen,(ON_STR));
		plen=fill_tcp_data_p(buf,plen,("</font>"));
	} else {
		plen=fill_tcp_data_p(buf,plen,("<font color=\"green\">"));
		plen=fill_tcp_data_p(buf,plen,(OFF_STR));
		plen=fill_tcp_data_p(buf,plen,("</font>"));
	}

	plen=fill_tcp_data_p(buf,plen,("\t\t<a href=\""));
	sprintf((char*)tmp_compose_buf, "http://%u.%u.%u.%u/",
			g_ip_addr[0],g_ip_addr[1],g_ip_addr[2],g_ip_addr[3]);
	plen=fill_tcp_data(buf,plen, (const char*)tmp_compose_buf);
	if ((LED_ON_STAT == led_stat(0))){
		plen=fill_tcp_data_p(buf,plen,(OFF_0_STR));
	}else{
		plen=fill_tcp_data_p(buf,plen,(ON_0_STR));
	}
	plen=fill_tcp_data_p(buf,plen,("\">Toggle</a>"));

	//LED 1 Control hypelink
	plen=fill_tcp_data_p(buf,plen,("\r\n<p>LED 1 Status:"));
	if ((LED_ON_STAT == led_stat(1))){
		plen=fill_tcp_data_p(buf,plen,("<font color=\"red\">"));
		plen=fill_tcp_data_p(buf,plen,(ON_STR));
		plen=fill_tcp_data_p(buf,plen,("</font>"));
	} else {
		plen=fill_tcp_data_p(buf,plen,("<font color=\"green\">"));
		plen=fill_tcp_data_p(buf,plen,(OFF_STR));
		plen=fill_tcp_data_p(buf,plen,("</font>"));
	}

	plen=fill_tcp_data_p(buf,plen,("\t\t<a href=\""));
	sprintf((char*)tmp_compose_buf, "http://%u.%u.%u.%u/",
			g_ip_addr[0],g_ip_addr[1],g_ip_addr[2],g_ip_addr[3]);
	plen=fill_tcp_data(buf,plen, (const char*)tmp_compose_buf);
	if ((LED_ON_STAT == led_stat(1))){
		plen=fill_tcp_data_p(buf,plen,(OFF_1_STR));
	}else{
		plen=fill_tcp_data_p(buf,plen,(ON_1_STR));
	}
	plen=fill_tcp_data_p(buf,plen,("\">Toggle</a>"));

	//LED 2 Control hypelink
	plen=fill_tcp_data_p(buf,plen,("\r\n<p>LED 2 Status:"));
	if ((LED_ON_STAT == led_stat(2))){
		plen=fill_tcp_data_p(buf,plen,("<font color=\"red\">"));
		plen=fill_tcp_data_p(buf,plen,(ON_STR));
		plen=fill_tcp_data_p(buf,plen,("</font>"));
	} else {
		plen=fill_tcp_data_p(buf,plen,("<font color=\"green\">"));
		plen=fill_tcp_data_p(buf,plen,(OFF_STR));
		plen=fill_tcp_data_p(buf,plen,("</font>"));
	}

	plen=fill_tcp_data_p(buf,plen,("\t\t<a href=\""));
	sprintf((char*)tmp_compose_buf, "http://%u.%u.%u.%u/",
			g_ip_addr[0],g_ip_addr[1],g_ip_addr[2],g_ip_addr[3]);
	plen=fill_tcp_data(buf,plen, (const char*)tmp_compose_buf);
	if ((LED_ON_STAT == led_stat(2))){
		plen=fill_tcp_data_p(buf,plen,(OFF_2_STR));
	}else{
		plen=fill_tcp_data_p(buf,plen,(ON_2_STR));
	}
	plen=fill_tcp_data_p(buf,plen,("\">Toggle</a>"));

	//LED 3 Control hypelink
	plen=fill_tcp_data_p(buf,plen,("\r\n<p>LED 3 Status:"));
	if ((LED_ON_STAT == led_stat(3))){
		plen=fill_tcp_data_p(buf,plen,("<font color=\"red\">"));
		plen=fill_tcp_data_p(buf,plen,(ON_STR));
		plen=fill_tcp_data_p(buf,plen,("</font>"));
	} else {
		plen=fill_tcp_data_p(buf,plen,("<font color=\"green\">"));
		plen=fill_tcp_data_p(buf,plen,(OFF_STR));
		plen=fill_tcp_data_p(buf,plen,("</font>"));
	}

	plen=fill_tcp_data_p(buf,plen,("\t\t<a href=\""));
	sprintf((char*)tmp_compose_buf, "http://%u.%u.%u.%u/",
			g_ip_addr[0],g_ip_addr[1],g_ip_addr[2],g_ip_addr[3]);
	plen=fill_tcp_data(buf,plen, (const char*)tmp_compose_buf);
	if ((LED_ON_STAT == led_stat(3))){
		plen=fill_tcp_data_p(buf,plen,(OFF_3_STR));
	}else{
		plen=fill_tcp_data_p(buf,plen,(ON_3_STR));
	}
	plen=fill_tcp_data_p(buf,plen,("\">Toggle</a>"));

	//Task Information
	plen=fill_tcp_data_p(buf,plen,("\r\n<p>TaskList:\r\n"));
	vTaskList(tmp_compose_buf);
	plen=fill_tcp_data(buf,plen, (const char*)tmp_compose_buf);
	plen=fill_tcp_data_p(buf,plen,("\r\n"));
	plen=fill_tcp_data_p(buf,plen,("\r\n<p>RunTimeStats:\r\n"));
	vTaskGetRunTimeStats(tmp_compose_buf);
	plen=fill_tcp_data(buf,plen, (const char*)tmp_compose_buf);
	plen=fill_tcp_data_p(buf,plen,("\r\n"));

	//Refresh hypelink
	plen=fill_tcp_data_p(buf,plen,("<p><a href=\""));
	sprintf((char*)tmp_compose_buf, "http://%u.%u.%u.%u/",
			g_ip_addr[0],g_ip_addr[1],g_ip_addr[2],g_ip_addr[3]);
	plen=fill_tcp_data(buf,plen, (const char*)tmp_compose_buf);
	plen=fill_tcp_data_p(buf,plen,("\">[Refresh]</a>\r\n<p><a href=\""));

	plen=fill_tcp_data_p(buf,plen,("<hr><p>Board Simple Server\r\n"));

	plen=fill_tcp_data_p(buf,plen,("</center>\r\n"));
	plen=fill_tcp_data_p(buf,plen,("</body>\r\n"));
	plen=fill_tcp_data_p(buf,plen,("</html>\r\n"));

	printf("content len:%u\n", plen);

	return plen;
}

void protocol_init(void){
	//using static configuration now
	printf("MAC:%02X,%02X,%02X,%02X,%02X,%02X\n",
			g_mac_addr[0],g_mac_addr[1],g_mac_addr[2],g_mac_addr[3],g_mac_addr[4],g_mac_addr[5]);
	printf("IP:%d.%d.%d.%d\n",
			g_ip_addr[0],g_ip_addr[1],g_ip_addr[2],g_ip_addr[3]);
	printf("Port:%d\n",HTTP_PORT);
}

const IfxFlash_flashSector IfxFlash_dFlashTableHsmLog[IFXFLASH_DFLASH_NUM_HSM_LOG_SECTORS] = {
		{0xaf110000, 0xaf111fff},   // HSM0
		{0xaf112000, 0xaf113fff},   // HSM1
		{0xaf114000, 0xaf115fff},   // HSM2
		{0xaf116000, 0xaf117fff},   // HSM3
		{0xaf118000, 0xaf119fff},   // HSM4
		{0xaf11a000, 0xaf11bfff},   // HSM5
		{0xaf11c000, 0xaf11dfff},   // HSM6
		{0xaf11e000, 0xaf11ffff},   // HSM7
};

const IfxFlash_flashSector IfxFlash_dFlashTablePhys[IFXFLASH_DFLASH_NUM_PHYSICAL_SECTORS] = {
		{IFXFLASH_DFLASH_START, IFXFLASH_DFLASH_END},
};

void IfxFlash_eraseSector(uint32_t sectorAddr)
{
	volatile uint32_t *addr1 = (volatile uint32_t *)(IFXFLASH_CMD_BASE_ADDRESS | 0xaa50);
	volatile uint32_t *addr2 = (volatile uint32_t *)(IFXFLASH_CMD_BASE_ADDRESS | 0xaa58);
	volatile uint32_t *addr3 = (volatile uint32_t *)(IFXFLASH_CMD_BASE_ADDRESS | 0xaaa8);
	volatile uint32_t *addr4 = (volatile uint32_t *)(IFXFLASH_CMD_BASE_ADDRESS | 0xaaa8);

	*addr1 = sectorAddr;
	*addr2 = 1;
	*addr3 = 0x80;
	*addr4 = 0x50;

	_dsync();
}

void IfxFlash_loadPage2X32(uint32_t pageAddr, uint32_t wordL, uint32_t wordU)
{
	volatile uint32_t *addr1 = (volatile uint32_t *)(IFXFLASH_CMD_BASE_ADDRESS | 0x55f0);

	*addr1 = wordL;
	addr1++;
	*addr1 = wordU;

	_dsync();
}

uint8_t IfxFlash_waitUnbusy(uint32_t flash, IfxFlash_FlashType flashType)
{
	if (flash == 0)
	{
		while (FLASH0_FSR.U & (1 << flashType))
		{}
	}

#if IFXFLASH_NUM_FLASH_MODULES > 1
else if (flash == 1)
{
	while (FLASH1_FSR.U & (1 << flashType))
	{}
}
#endif
else
{
	return 1; // invalid flash selected
}
	_dsync();
	return 0;     // finished
}

uint8_t IfxFlash_enterPageMode(uint32_t pageAddr)
{
	volatile uint32_t *addr1 = (volatile uint32_t *)(IFXFLASH_CMD_BASE_ADDRESS | 0x5554);

	if ((pageAddr & 0xff000000) == 0xa0000000)    // program flash
	{
		*addr1 = 0x50;
		return 0;
	}
	else if ((pageAddr & 0xff000000) == 0xaf000000)       // data flash
	{
		*addr1 = 0x5D;
		return 0;
	}

	_dsync();
	return 1; // invalid flash address
}


void IfxFlash_writePage(uint32_t pageAddr)
{
	volatile uint32_t *addr1 = (volatile uint32_t *)(IFXFLASH_CMD_BASE_ADDRESS | 0xaa50);
	volatile uint32_t *addr2 = (volatile uint32_t *)(IFXFLASH_CMD_BASE_ADDRESS | 0xaa58);
	volatile uint32_t *addr3 = (volatile uint32_t *)(IFXFLASH_CMD_BASE_ADDRESS | 0xaaa8);
	volatile uint32_t *addr4 = (volatile uint32_t *)(IFXFLASH_CMD_BASE_ADDRESS | 0xaaa8);

	*addr1 = pageAddr;
	*addr2 = 0x00;
	*addr3 = 0xa0;
	*addr4 = 0xaa;

	_dsync();
}

//const uint32_t test_dflash_data[] = {0xAB886301, 0x68A9089B, 0xB421ED43, 0xE29F12EB, 0x5089EBB9, 0x59B7FB59, 0xD026A00F, 0x632764D6, 0x308CF937, 0x7631265D, 0x366B7F45, 0x4A19298E, 0x627BA510, 0x486D51C3, 0x1D61EFF0, 0x62F64F46, 0x8D716BF7, 0xF17F158A, 0xD7C75307, 0x0160A435, 0xE8CB0202, 0xF6C35094, 0x183F4A8C, 0x10BC6548, 0x57235099, 0xEDE43E96, 0x48937C58, 0xC8188B44, 0x60A7D5A7, 0xFDBD8EA4, 0x877B19A3, 0x3B35ECA4, };
//const uint32_t test_dflash_data[] = {
//		0x3C70E0F4, 0xB3126DD5, 0xE9B08592, 0x85EA2E14, 0xEDDFF0F5, 0x31112720, 0x6A66F3E2, 0xE5B1D330, 0xA223D471, 0xFCF89257, 0x55CE4051, 0x473562F7, 0x7E5D9E47, 0xEEC4BD06, 0xB8DD2625, 0x07ED2F5A, 0x735892DF, 0x7E0D206E, 0x1EBD451D, 0xCE031F24, 0xAA6AB31C, 0x2CD673B8, 0x7B194BDF, 0xA46A96A5, 0xFB7EEFB2, 0xC939C96F, 0x1CB9EF37, 0x469B11A8, 0x113E33DC, 0x41AC3E9A, 0x56AEE5F2, 0xBD3AD630
//};

void dbg_dump(uint32_t addr, uint32_t u32_n) {
	//DFlash test
	printf("DFlash[%08X]:\n", addr);
	for(uint32_t i=0; i<u32_n; i+=4) {
		printf("%08X ", *(uint32_t*)(addr+i));
	}
	printf("\n");
	flush_stdout();
}

void erase_sector(uint32_t s_addr) {
	/* erase program flash */
	unlock_safety_wdtcon();
	IfxFlash_eraseSector(s_addr);
	lock_safety_wdtcon();
}

void program_page(uint32_t page_addr, uint32_t u32_0, uint32_t u32_1) {
	/* wait until unbusy */
	IfxFlash_waitUnbusy(0, IfxFlash_FlashType_D0);

	IfxFlash_loadPage2X32(page_addr, u32_0, u32_1);
	/* write page */
	unlock_safety_wdtcon();
	IfxFlash_writePage(page_addr);
	lock_safety_wdtcon();

	/* wait until unbusy */
	IfxFlash_waitUnbusy(0, IfxFlash_FlashType_D0);
}

void DFlashDemo(uint8_t dflash_sec_n) {
	uint32_t errors = 0;
	uint32_t flash       = 0;
	uint32_t sector_addr = IFXFLASH_DFLASH_START + dflash_sec_n*DFLASH_SECTOR_SIZE;

	erase_sector(sector_addr);

	dbg_dump(sector_addr, DFLASH_SECTOR_SIZE/32);

	/* program the given no of pages */
	for (uint16_t page = 0; page < 16; ++page) {
		uint32_t pageAddr = sector_addr + page * IFXFLASH_DFLASH_PAGE_LENGTH;
		errors = IfxFlash_enterPageMode(pageAddr);

		uint32_t u32_0 = rand();
		uint32_t u32_1 = rand();
		program_page(pageAddr, u32_0, u32_1);
	}

	dbg_dump(sector_addr, DFLASH_SECTOR_SIZE/32);
}

int core0_main(int argc, char** argv) {
	prvSetupHardware();

	//	SYSTEM_EnaDisCache(1);

	uart_init(mainCOM_TEST_BAUD_RATE);

	config_dts();

	printf("%s %s\n", _NEWLIB_VERSION, __func__);

	const uint32_t FLASH_SIZE_TABLE_KB[]={256, 512, 1024, 1536, 2048, 2560, 3072, 4096, 5120, 1024*6, 1024*7, 1024*8};
	printf("CHIPID:%X\n"\
			"%c step\n" \
			"AurixGen:%u\n"\
			"EmuDevice?%s\n" \
			"Flash SIZE:%u KB\n"\
			"HSM:%s\n"\
			"SpeedGrade:%X\n",
			MODULE_SCU.CHIPID.B.CHID,
			(MODULE_SCU.CHIPID.B.CHREV/0x10)+'A',
			MODULE_SCU.CHIPID.B.CHTEC,
			(MODULE_SCU.CHIPID.B.EEA==1)?"Yes":"No",
					FLASH_SIZE_TABLE_KB[MODULE_SCU.CHIPID.B.FSIZE%0x0c],
					(MODULE_SCU.CHIPID.B.SEC==1)?"Yes":"No",
							MODULE_SCU.CHIPID.B.SP
	);
	flush_stdout();

	printf("Tricore %04X Core:%04X, CPU:%u MHz,Sys:%u MHz,STM:%u MHz,PLL:%u M,CE:%d\n",
			__TRICORE_NAME__,
			__TRICORE_CORE__,
			SYSTEM_GetCpuClock()/1000000,
			SYSTEM_GetSysClock()/1000000,
			SYSTEM_GetStmClock()/1000000,
			system_GetPllClock()/1000000,
			SYSTEM_IsCacheEnabled());
	flush_stdout();

	_syscall(123);

	DFlashDemo(0);
	DFlashDemo(1);
	DFlashDemo(2);
	//	test_tlf35584();

	interface_init();
	protocol_init();

	//	server_loop();

	/* The following function will only create more tasks and timers if
	mainCREATE_SIMPLE_LED_FLASHER_DEMO_ONLY is set to 0 (at the top of this
	file).  See the comments at the top of this file for more information. */
	//	prvOptionallyCreateComprehensveTestApplication();

	xTaskCreate((TaskFunction_t )start_task,
			(const char*    )"start_task",
			(uint16_t       )512,
			(void*          )NULL,
			(UBaseType_t    )tskIDLE_PRIORITY + 1,
			(TaskHandle_t*  )&StartTask_Handler);

	/* Now all the tasks have been started - start the scheduler. */
	vTaskStartScheduler();

	/* If all is well then the following line will never be reached.  If
	execution does reach here, then it is highly probably that the heap size
	is too small for the idle and/or timer tasks to be created within
	vTaskStartScheduler(). */
	while(1) {
		_nop();
	}

	return EXIT_SUCCESS;
}

void start_task(void *pvParameters) {
	Message_Queue = xQueueCreate(MESSAGE_Q_NUM, sizeof(uint32_t));

	CountSemaphore = xSemaphoreCreateCounting(2, 2);

	MutexSemaphore = xSemaphoreCreateMutex();

	xTaskCreate((TaskFunction_t )maintaince_task,
			(const char*    )"maintaince_task",
			(uint16_t       )768,
			(void*          )NULL,
			(UBaseType_t    )tskIDLE_PRIORITY + 1,
			(TaskHandle_t*  )&g_task0_handler);

	xTaskCreate((TaskFunction_t )print_task,
			(const char*    )"print_task",
			(uint16_t       )10*1024,
			(void*          )NULL,
			(UBaseType_t    )tskIDLE_PRIORITY + 2,
			(TaskHandle_t*  )&g_info_task_handler);
	vTaskDelete(StartTask_Handler);
}

void maintaince_task(void *pvParameters) {
	//	char info_buf[512];

	while(1) {
		//		vTaskList(info_buf);
		//		if(NULL != MutexSemaphore) {
		//			if(pdTRUE == xSemaphoreTake(MutexSemaphore, portMAX_DELAY)) {
		//				printf("%s\r\n",info_buf);
		//
		//				vTaskGetRunTimeStats(info_buf);
		//				printf("RunTimeStats Len:%d\r\n", strlen(info_buf));
		//				printf("%s\r\n",info_buf);
		//
		//				printf("Tricore %04X Core:%04X, CPU:%u MHz,Sys:%u MHz,STM:%u MHz,PLL:%u M,Int:%u M,CE:%d\n",
		//						__TRICORE_NAME__,
		//						__TRICORE_CORE__,
		//						SYSTEM_GetCpuClock()/1000000,
		//						SYSTEM_GetSysClock()/1000000,
		//						SYSTEM_GetStmClock()/1000000,
		//						system_GetPllClock()/1000000,
		//						system_GetIntClock()/1000000,
		//						SYSTEM_IsCacheEnabled());
		//				flush_stdout();
		//
		//				xSemaphoreGive(MutexSemaphore);
		//			}
		//		}
		start_dts_measure();

		uint32_t NotifyValue=ulTaskNotifyTake( pdTRUE, /* Clear the notification value on exit. */
				portMAX_DELAY );/* Block indefinitely. */
		vTaskDelay(40 / portTICK_PERIOD_MS);
	}
}

void print_task(void *pvParameters) {
	char info_buf[512];
	volatile uint8_t net_buf[MAX_FRAMELEN];

	uint16_t payloadlen;
	uint16_t dat_p;
	int16_t cmd16;

	while(true) {
		payloadlen = enc28j60PacketReceive(MAX_FRAMELEN, net_buf);

		if(payloadlen==0) {
			vTaskDelay(20 / portTICK_PERIOD_MS);
			continue;
		} else if(eth_type_is_arp_and_my_ip(net_buf,payloadlen)) {
			//Process ARP Request
			make_arp_answer_from_request(net_buf);
			continue;
		} else if(eth_type_is_ip_and_my_ip(net_buf,payloadlen)==0) {
			//Only Process IP Packet destinated at me
			printf("$");
			continue;
		} else if(net_buf[IP_PROTO_P]==IP_PROTO_ICMP_V && net_buf[ICMP_TYPE_P]==ICMP_TYPE_ECHOREQUEST_V){
			//Process ICMP packet
			printf("Rxd ICMP from [%d.%d.%d.%d]\n",net_buf[ETH_ARP_SRC_IP_P],net_buf[ETH_ARP_SRC_IP_P+1],
					net_buf[ETH_ARP_SRC_IP_P+2],net_buf[ETH_ARP_SRC_IP_P+3]);
			make_echo_reply_from_request(net_buf, payloadlen);
			continue;
		} else if (net_buf[IP_PROTO_P]==IP_PROTO_TCP_V&&net_buf[TCP_DST_PORT_H_P]==0&&net_buf[TCP_DST_PORT_L_P]==HTTP_PORT) {
			//Process TCP packet with HTTP_PORT
			printf("Rxd TCP http pkt\n");
			if (net_buf[TCP_FLAGS_P] & TCP_FLAGS_SYN_V) {
				printf("Type SYN\n");
				make_tcp_synack_from_syn(net_buf);
				continue;
			}
			if (net_buf[TCP_FLAGS_P] & TCP_FLAGS_ACK_V) {
				printf("Type ACK\n");
				init_len_info(net_buf); // init some data structures
				dat_p=get_tcp_data_pointer();
				if (dat_p==0) {
					if (net_buf[TCP_FLAGS_P] & TCP_FLAGS_FIN_V) {
						make_tcp_ack_from_any(net_buf);
					}
					continue;
				}
				// Process Telnet request
				if (strncmp("GET ",(char *)&(net_buf[dat_p]),4)!=0){
					payloadlen=fill_tcp_data_p(net_buf,0,("Tricore\r\n\n\rHTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>200 OK</h1>"));
					goto SENDTCP;
				}
				//Process HTTP Request
				if (strncmp("/ ",(char *)&(net_buf[dat_p+4]),2)==0) {
					//Update Web Page Content
					payloadlen=prepare_page(net_buf);
					goto SENDTCP;
				}

				//Analysis the command in the URL
				cmd16 = analyse_get_url((char *)&(net_buf[dat_p+5]));
				if (cmd16 < 0) {
					payloadlen=fill_tcp_data_p(net_buf,0,("HTTP/1.0 401 Unauthorized\r\nContent-Type: text/html\r\n\r\n<h1>401 Unauthorized</h1>"));
					goto SENDTCP;
				}
				if (CMD_LD0_ON == cmd16)	{
					if(LED_ON_STAT != led_stat(0)) {
						led_on(0);
					} else {
						;
					}
				} else if (CMD_LD0_OFF == cmd16) {
					if(LED_OFF_STAT != led_stat(0)) {
						led_off(0);
					} else {
						;
					}
				} else if (CMD_LD1_ON == cmd16)	{
					if(LED_ON_STAT != led_stat(1)) {
						led_on(1);
					}
				} else if (CMD_LD1_OFF == cmd16) {
					if(LED_OFF_STAT != led_stat(1)) {
						led_off(1);
					}
				} else if (CMD_LD2_ON == cmd16)	{
					if(LED_ON_STAT != led_stat(2)) {
						led_on(2);
					}
				} else if (CMD_LD2_OFF == cmd16) {
					if(LED_OFF_STAT != led_stat(2)) {
						led_off(2);
					}
				} else if (CMD_LD3_ON == cmd16)	{
					if(LED_ON_STAT != led_stat(3)) {
						led_on(3);
					}
				} else if (CMD_LD3_OFF == cmd16) {
					if(LED_OFF_STAT != led_stat(3)) {
						led_off(3);
					}
				}
				//Update Web Page Content
				payloadlen=prepare_page(net_buf);

				SENDTCP:
				// send ack for http get
				make_tcp_ack_from_any(net_buf);
				// send data
				make_tcp_ack_with_data(net_buf,payloadlen);
				continue;
			}
		} else {
			//;
		}
	}
}

static void prvSetupHardware( void ) {
	system_clk_config_200_100();

	/* activate interrupt system */
	InterruptInit();

	SYSTEM_Init();

	/* Initialize LED outputs. */
	vParTestInitialise();
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created.  It is also called by various parts of the
	demo application.  If heap_1.c or heap_2.c are used, then the size of the
	heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
	FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
	to query the size of free heap space that remains (although it does not
	provide information on how the remaining heap might be fragmented). */
	taskDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void ) {
#if mainCREATE_SIMPLE_LED_FLASHER_DEMO_ONLY != 1
	{
		/* vApplicationTickHook() will only be called if configUSE_TICK_HOOK is set
		to 1 in FreeRTOSConfig.h.  It is a hook function that will get called during
		each FreeRTOS tick interrupt.  Note that vApplicationTickHook() is called
		from an interrupt context. */

		/* Call the periodic timer test, which tests the timer API functions that
		can be called from an ISR. */
		vTimerPeriodicISRTests();
	}
#endif /* mainCREATE_SIMPLE_LED_FLASHER_DEMO_ONLY */
}

/*-----------------------------------------------------------*/

void vApplicationIdleHook( void ) {
	/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
	to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
	task.  It is essential that code added to this hook function never attempts
	to block in any way (for example, call xQueueReceive() with a block time
	specified, or call vTaskDelay()).  If the application makes use of the
	vTaskDelete() API function (as this demo application does) then it is also
	important that vApplicationIdleHook() is permitted to return to its calling
	function, because it is the responsibility of the idle task to clean up
	memory allocated by the kernel to any task that has since been deleted. */
}
/*-----------------------------------------------------------*/

