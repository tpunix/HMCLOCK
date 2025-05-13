

#include "epd.h"


/******************************************************************************/

static int epio_pwr;
static int epio_busy;
static int epio_rst;
static int epio_dc;
static int epio_cs;
static int epio_clk;
static int epio_sdi;


#define EPD_CLK(n)  gpio_set(epio_clk, (n))
#define EPD_SDI(n)  gpio_set(epio_sdi, (n))
#define EPD_CS(n)   gpio_set(epio_cs , (n))
#define EPD_DC(n)   gpio_set(epio_dc , (n))
#define EPD_RST(n)  gpio_set(epio_rst, (n))
#define EPD_PWR(n)  gpio_set(epio_pwr, (n))
#define EPD_BUSY()  gpio_get(epio_busy)

#define EPD_DIN()   gpio_config(epio_sdi, 0x0100, 0)
#define EPD_DOUT()  gpio_config(epio_sdi, 0x0300, 0)
#define EPD_SDO()   gpio_get(epio_sdi)


/******************************************************************************/

void gpio_config(int index, int mode, int value)
{
	int group = index>>4;
	index &= 0x0f;

	GPIO_ConfigurePin(group, index, mode, PID_GPIO, value);
}

void gpio_set(int index, int value)
{
	int group = index>>4;
	index &= 0x0f;

	if(value){
		GPIO_SetActive(group, index);
	}else{
		GPIO_SetInactive(group, index);
	}
}


int gpio_get(int index)
{
	int group = index>>4;
	index &= 0x0f;
	return GPIO_GetPinStatus(group, index)? 1: 0;
}


void delay_ms(int ms)
{
	ms *= 1800;
	while(ms){
		__asm("nop");
		ms -= 1;
	}
}


/******************************************************************************/


void epd_hw_init(u32 config0, u32 config1, int w, int h, int mode)
{
	epio_pwr  = (config0>>24)&0xff;
	epio_busy = (config0>>16)&0xff;
	epio_rst  = (config0>> 8)&0xff;
	epio_dc   = (config1>>24)&0xff;
	epio_cs   = (config1>>16)&0xff;
	epio_clk  = (config1>> 8)&0xff;
	epio_sdi  = (config1>> 0)&0xff;

	scr_w = w;
	scr_h = h;
	scr_mode = mode;
	line_bytes = (scr_w+7)>>3;
	scr_padding = line_bytes*8-scr_w;
}

void epd_hw_open(void)
{
	gpio_config(epio_pwr , 0x0300, 0);
	gpio_config(epio_busy, 0x0000, 1);
	gpio_config(epio_rst , 0x0300, 0);
	gpio_config(epio_dc  , 0x0300, 0);
	gpio_config(epio_cs  , 0x0300, 1);
	gpio_config(epio_clk , 0x0300, 0);
	gpio_config(epio_sdi , 0x0300, 0);
}

void epd_hw_close(void)
{
	gpio_config(epio_pwr , 0x0300, 0);
	gpio_config(epio_busy, 0x0000, 0);
	gpio_config(epio_rst , 0x0300, 0);
	gpio_config(epio_dc  , 0x0300, 0);
	gpio_config(epio_cs  , 0x0300, 0);
	gpio_config(epio_clk , 0x0300, 0);
	gpio_config(epio_sdi , 0x0300, 0);
}

static void epd_spi_write(int value)
{
	int i;

	for(i=0; i<8; i++){
		EPD_CLK(0);
		EPD_SDI(value&0x80);
		EPD_CLK(1);
		value <<= 1;
	}
}


static int epd_spi_read(void)
{
	int i, value=0;

	for(i=0; i<8; i++){
		EPD_CLK(0);
		value <<= 1;
		EPD_CLK(1);
		value |= EPD_SDO();
	}
	
	return value;
}


void epd_reset(int val)
{
	EPD_RST(val);
}


void epd_wait(void)
{
	while(EPD_BUSY());
}


int epd_busy(void)
{
	return EPD_BUSY();
}


void epd_power(int on)
{
	EPD_PWR(on);
}


void epd_cmd(int cmd)
{
	EPD_CS(0);
	EPD_DC(0);
	epd_spi_write(cmd);
	EPD_CS(1);
}


void epd_data(int data)
{
	EPD_CS(0);
	EPD_DC(1);
	epd_spi_write(data);
	EPD_CS(1);
}


void epd_cmd1(int cmd, int d0)
{
	EPD_CS(0);
	EPD_DC(0);
	epd_spi_write(cmd);
	EPD_DC(1);
	epd_spi_write(d0);
	EPD_CS(0);
}


void epd_cmd2(int cmd, int d0, int d1)
{
	EPD_CS(0);
	EPD_DC(0);
	epd_spi_write(cmd);
	EPD_DC(1);
	epd_spi_write(d0);
	epd_spi_write(d1);
	EPD_CS(0);
}


void epd_cmd3(int cmd, int d0, int d1, int d2)
{
	EPD_CS(0);
	EPD_DC(0);
	epd_spi_write(cmd);
	EPD_DC(1);
	epd_spi_write(d0);
	epd_spi_write(d1);
	epd_spi_write(d2);
	EPD_CS(0);
}


void epd_cmd4(int cmd, int d0, int d1, int d2, int d3)
{
	EPD_CS(0);
	EPD_DC(0);
	epd_spi_write(cmd);
	EPD_DC(1);
	epd_spi_write(d0);
	epd_spi_write(d1);
	epd_spi_write(d2);
	epd_spi_write(d3);
	EPD_CS(0);
}


void epd_data_array(u8 *data, int len)
{
	EPD_CS(0);
	EPD_DC(1);
	for(int i=0; i<len; i++){
		epd_spi_write(data[i]);
	}
	EPD_CS(1);
}


void epd_read(u8 *data, int len)
{
	EPD_DIN();

	EPD_CS(0);
	EPD_DC(1);

	for(int i=0; i<len; i++){
		data[i] = epd_spi_read();
	}
	EPD_CS(1);

	EPD_DOUT();
}


void epd_cmd_read(int cmd, u8 *data, int len)
{
	EPD_CS(0);
	EPD_DC(0);
	epd_spi_write(cmd);

	EPD_DC(1);
	EPD_DIN();

	for(int i=0; i<len; i++){
		data[i] = epd_spi_read();
	}

	EPD_DOUT();
	EPD_CS(1);

}

/******************************************************************************/

