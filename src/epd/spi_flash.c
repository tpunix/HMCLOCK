
#include "epd.h"




static int spio_clk;
static int spio_cs;
static int spio_di;
static int spio_do;

/******************************************************************************/


#define FSPI_DELAY     10

void fspi_delay(void)
{
	int i;

	for(i=0; i<FSPI_DELAY; i++){
		__asm("nop");
	}
}


#define FSPI_CS(n)  gpio_set(spio_cs,  (n));
#define FSPI_CK(n)  gpio_set(spio_clk, (n));
#define FSPI_SI(n)  gpio_set(spio_di,  (n));
#define FSPI_SO()   gpio_get(spio_do);


int fspi_trans(int byte)
{
	int i, data;

	data = 0;
	for(i=0; i<8; i++){
		FSPI_SI(byte&0x80);
		FSPI_CK(0);
		fspi_delay();

		data <<= 1;
		data |= FSPI_SO();

		FSPI_CK(1);
		fspi_delay();
		byte <<= 1;
	}

	return data;
}


int fspi_init(u32 gpio_word)
{
	spio_clk = (gpio_word>>24)&0xff;
	spio_cs  = (gpio_word>>16)&0xff;
	spio_di  = (gpio_word>> 8)&0xff;
	spio_do  = (gpio_word>> 0)&0xff;

	gpio_config(spio_clk, 0x0300, 0);
	gpio_config(spio_cs,  0x0300, 1);
	gpio_config(spio_di,  0x0300, 1);
	gpio_config(spio_do,  0x0100, 1);
	
	FSPI_CS(0);
	fspi_delay();
	fspi_trans(0xab);
	FSPI_CS(1);

	return 0;
}


/******************************************************************************/
/* SPI flash                                                                  */
/******************************************************************************/ 

int epcs_readid(void)
{
	int mid, pid;

	FSPI_CS(0);
	fspi_delay();

	fspi_trans(0x90);
	fspi_trans(0);
	fspi_trans(0);
	fspi_trans(0);

	mid = fspi_trans(0);
	pid = fspi_trans(0);

	FSPI_CS(1);
	fspi_delay();

	return (mid<<8)|pid;
}

int epcs_status(void)
{
	int status;

	FSPI_CS(0);
	fspi_delay();
	fspi_trans(0x05);
	status = fspi_trans(0);
	FSPI_CS(1);
	fspi_delay();

	return status;
}

int epcs_wstat(int stat)
{
	// status write enable
	FSPI_CS(0);
	fspi_delay();
	fspi_trans(0x50);
	FSPI_CS(1);
	fspi_delay();

	FSPI_CS(0);
	fspi_delay();
	fspi_trans(0x01);
	fspi_trans(stat);
	FSPI_CS(1);
	fspi_delay();

	return 0;
}

int epcs_wen(int en)
{
	FSPI_CS(0);
	fspi_delay();
	if(en)
		fspi_trans(0x06);
	else
		fspi_trans(0x04);
	FSPI_CS(1);
	fspi_delay();

	return 0;
}

int epcs_wait()
{
	int status;

	while(1){
		status = epcs_status();
		if((status&1)==0)
			break;
		fspi_delay();
	}
	
	return 0;
}


int epcs_sector_erase(int addr)
{
	epcs_wen(1);

	FSPI_CS(0);
	fspi_delay();

	fspi_trans(0x20);
	fspi_trans((addr>>16)&0xff);
	fspi_trans((addr>> 8)&0xff);
	fspi_trans((addr>> 0)&0xff);

	FSPI_CS(1);
	fspi_delay();

	epcs_wait();
	
	return 0;
}

int epcs_page_write(int addr, u8 *buf, int size)
{
	int i;

	epcs_wen(1);

	FSPI_CS(0);
	fspi_delay();

	fspi_trans(0x02);
	fspi_trans((addr>>16)&0xff);
	fspi_trans((addr>> 8)&0xff);
	fspi_trans((addr>> 0)&0xff);

	for(i=0; i<size; i++){
		fspi_trans(buf[i]);
	}

	FSPI_CS(1);
	fspi_delay();
	
	epcs_wait();

	return 0;
}


int epcs_read(int addr, int len, u8 *buf)
{
	int i;

	FSPI_CS(0);
	fspi_delay();

	fspi_trans(0x0b);
	fspi_trans((addr>>16)&0xff);
	fspi_trans((addr>> 8)&0xff);
	fspi_trans((addr>> 0)&0xff);

	fspi_trans(0);

	for(i=0; i<len; i++){
		buf[i] = fspi_trans(0);
	}

	FSPI_CS(1);
	fspi_delay();

	return len;
}

/******************************************************************************/

