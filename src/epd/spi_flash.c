
#include "epd.h"



#define SPI_CTRL0  *(volatile unsigned short*)(0x50001200)
#define SPI_RXTX0  *(volatile unsigned short*)(0x50001202)
#define SPI_RXTX1  *(volatile unsigned short*)(0x50001204)
#define SPI_IACK   *(volatile unsigned short*)(0x50001206)
#define SPI_CTRL1  *(volatile unsigned short*)(0x50001208)


static int spio_clk;
static int spio_cs;
static int spio_di;
static int spio_do;

#define BIT_8  0
#define BIT_16 1
#define BIT_32 2

static int spi_bitmode;

/******************************************************************************/


#define FSPI_CS(n)  gpio_set(spio_cs,  (n));

void fspi_set_bitmode(int mode)
{
	spi_bitmode = mode;

	SPI_CTRL0 &= 0xfe7e;
	SPI_CTRL0 |= (mode&3)<<7;
	SPI_CTRL0 |= 0x0001;
}

int fspi_trans(int data)
{
	if(spi_bitmode==BIT_32){
		SPI_RXTX1 = data>>16;
	}
	SPI_RXTX0 = data;

	while((SPI_CTRL0&0x2000)==0);
	SPI_IACK = 0;

	data = SPI_RXTX0;
	if(spi_bitmode==BIT_8){
		data &= 0xff;
	}else if(spi_bitmode==BIT_32){
		data |= (SPI_RXTX1)<<16;
	}
	return data;
}


int fspi_config(u32 gpio_word)
{
	spio_clk = (gpio_word>>24)&0xff;
	spio_cs  = (gpio_word>>16)&0xff;
	spio_do  = (gpio_word>> 8)&0xff;
	spio_di  = (gpio_word>> 0)&0xff;

	return 0;
}


int fspi_init(void)
{
	SetBits16(CLK_PER_REG, SPI_ENABLE, 1);

	gpio_config(spio_cs,  0x0300, 1);
	gpio_config(spio_clk, 0x0307, 0);
	gpio_config(spio_do,  0x0306, 1);
	gpio_config(spio_di,  0x0105, 1);

	SPI_CTRL0 = 0x0010;
	fspi_set_bitmode(BIT_32);

	FSPI_CS(0);
	fspi_trans(0xab000000);
	FSPI_CS(1);

	return 0;
}


int fspi_exit(void)
{
	SPI_CTRL0 = 0;
	SetBits16(CLK_PER_REG, SPI_ENABLE, 0);

	gpio_config(spio_cs,  0x0300, 1);
	gpio_config(spio_clk, 0x0300, 0);
	gpio_config(spio_do,  0x0300, 0);
	gpio_config(spio_di,  0x0100, 0);
	return 0;
}


/******************************************************************************/
/* SPI flash                                                                  */
/******************************************************************************/ 

int sf_readid(void)
{
	fspi_set_bitmode(BIT_32);
	FSPI_CS(0);
	fspi_trans(0x90000000);
	int id = fspi_trans(0);
	FSPI_CS(1);

	return id;
}

int sf_status(void)
{
	int status;

	fspi_set_bitmode(BIT_16);
	FSPI_CS(0);
	status = fspi_trans(0x0500);
	FSPI_CS(1);

	return status;
}

int sf_wstat(int stat)
{

	// status write enable
	fspi_set_bitmode(BIT_8);
	FSPI_CS(0);
	fspi_trans(0x50);
	FSPI_CS(1);

	fspi_set_bitmode(BIT_16);
	FSPI_CS(0);
	fspi_trans(0x0100|stat);
	FSPI_CS(1);

	return 0;
}

int sf_wen(int en)
{
	fspi_set_bitmode(BIT_8);
	FSPI_CS(0);
	if(en)
		fspi_trans(0x06);
	else
		fspi_trans(0x04);
	FSPI_CS(1);

	return 0;
}

int sf_wait()
{
	int status;

	while(1){
		status = sf_status();
		if((status&1)==0)
			break;
	}
	
	return 0;
}


int sf_sector_erase(int addr)
{
	sf_wen(1);

	fspi_set_bitmode(BIT_32);
	FSPI_CS(0);
	fspi_trans(0x20000000|addr);
	FSPI_CS(1);

	sf_wait();
	
	return 0;
}

int sf_page_write(int addr, u8 *buf, int size)
{
	int i;

	sf_wen(1);

	fspi_set_bitmode(BIT_32);
	FSPI_CS(0);

	fspi_trans(0x02000000|addr);
	for(i=0; i<size; i+=4){
		fspi_trans(*(u32*)(buf+i));
	}

	FSPI_CS(1);
	sf_wait();

	return 0;
}


int sf_read(int addr, int len, u8 *buf)
{
	int i;

	fspi_set_bitmode(BIT_32);
	FSPI_CS(0);

	fspi_trans(0x03000000|addr);

	for(i=0; i<len; i+=4){
		*(u32*)(buf+i) = fspi_trans(0);
	}

	FSPI_CS(1);

	return len;
}

/******************************************************************************/

