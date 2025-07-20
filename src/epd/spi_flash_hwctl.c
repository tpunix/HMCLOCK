
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
	
	id = __REV(id);
	id &= 0xffff;

	return id;
}

int sf_status(int id)
{
	int status;
	int cmd = (id)? 0x3500 : 0x0500;

	fspi_set_bitmode(BIT_16);
	FSPI_CS(0);
	status = fspi_trans(cmd);
	FSPI_CS(1);

	return status&0xff;
}

int sf_wstat(int id, int stat)
{
	int cmd = (id)? 0x3100 : 0x0100;
	cmd |= stat;

	// status write enable
	fspi_set_bitmode(BIT_8);
	FSPI_CS(0);
	fspi_trans(0x50);
	FSPI_CS(1);

	fspi_set_bitmode(BIT_16);
	FSPI_CS(0);
	fspi_trans(cmd);
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

int sf_wait(void)
{
	int status;

	while(1){
		status = sf_status(0);
		if((status&1)==0)
			break;
	}

	status |= sf_status(1)<<8;
	return status&0x4000;
}


int sf_sector_erase(int cmd, int addr, int wait)
{
	sf_wen(1);

	fspi_set_bitmode(BIT_32);
	FSPI_CS(0);
	fspi_trans((cmd<<24)|addr);
	FSPI_CS(1);

	if(wait){
		int status = sf_wait();
		return status;
	}else{
		return 0;
	}
}

int sf_erase(int addr, int size, int wait)
{
	GLOBAL_INT_DISABLE();

	int status;
	while(size>0){
//		if((addr%0x8000)==0 && size>=0x8000){
//			status = sf_sector_erase(ERASE_32K, addr, wait);
//			printk("sf_erase: %08x 32K  stat=%04x\n", addr, status);
//			addr += 0x8000;
//			size -= 0x8000;
//		}else
		{
			status = sf_sector_erase(ERASE_4K,  addr, wait);
			printk("sf_erase: %08x  4K  stat=%04x\n", addr, status);
			addr += 0x1000;
			size -= 0x1000;
		}
	}

	GLOBAL_INT_RESTORE();
	return 0;
}


int sf_page_write(int addr, u8 *buf, int size)
{
	int i;

	GLOBAL_INT_DISABLE();
	sf_wen(1);

	fspi_set_bitmode(BIT_32);
	FSPI_CS(0);

	fspi_trans(0x02000000|addr);
	for(i=0; i<size; i+=4){
		int data = *(u32*)(buf+i);
		fspi_trans(__REV(data));
	}

	FSPI_CS(1);
	GLOBAL_INT_RESTORE();
	return 0;
}


int sf_read(int addr, int len, u8 *buf)
{
	int i;

	fspi_set_bitmode(BIT_32);
	FSPI_CS(0);

	fspi_trans(0x03000000|addr);

	for(i=0; i<len; i+=4){
		int data = fspi_trans(0);
		*(u32*)(buf+i) = __REV(data);
	}

	FSPI_CS(1);

	return len;
}

/******************************************************************************/


static const uint32_t crc32_tab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t crc32(uint32_t crc, const void *buf, size_t size)
{
    const uint8_t *p;

    p = buf;
    crc = crc ^ ~0U;

    while (size--)
        crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

    return crc ^ ~0U;
}


/******************************************************************************/

extern int Region$$Table$$Base;
void sf_dumpp(int addr, int size);

int selflash(int otp_boot)
{
	u8 pbuf[256];
	u32 *p32 = (u32*)pbuf;
	int image_addr[2];
	int image_flag[2];

	fspi_init();
	int id = sf_readid();
	printk("Flash  ID: %08x\n", id);

	int region_table = (int)&Region$$Table$$Base;
	int firm_size = *(u32*)(region_table+0x10) - 0x07fc0000;
	printk("Firm size: %08x\n", firm_size);
	u32 firm_crc = crc32(0, (u8*)0x07fc0000, firm_size);
	printk("Firm  crc: %08x\n", firm_crc);
	printk("Firm  ver: %08x\n", EPD_VERSION);


	memset(pbuf, 0, 256);
	if(otp_boot==0x1234a5a5){
		// 从OTP启动。读product header。
		sf_read(0x38000, 16, pbuf);
		if(pbuf[0]!=0x70 || pbuf[1]!=0x52){
			printk("Build Product header ...\n");
			p32[0] = 0x00005270;
			p32[1] = 0x00004000;
			p32[2] = 0x0001f000;
			sf_sector_erase(ERASE_4K, 0x38000, 1);
			sf_page_write(0x38000, pbuf, 12);
			sf_wait();
		}
		image_addr[0] = p32[1];
		image_addr[1] = p32[2];

		// 读image header
		sf_read(image_addr[0], 32, pbuf+0 );
		sf_read(image_addr[1], 32, pbuf+32);
		printk("Product iamge0: %08x:  %08x %08x %08x %08x\n", image_addr[0], __REV(p32[0]), p32[1], p32[2], p32[7]);
		printk("        iamge1: %08x:  %08x %08x %08x %08x\n", image_addr[1], __REV(p32[8]), p32[9], p32[10],p32[15]);

		// 获取当前使用的image的id
		image_flag[0] = -1;
		image_flag[1] = -1;
		if(pbuf[ 0]==0x70 && pbuf[ 1]==0x51 && pbuf[ 2]==0xaa){
			image_flag[0] = (signed char)pbuf[ 3];
		}
		if(pbuf[32]==0x70 && pbuf[33]==0x51 && pbuf[34]==0xaa){
			image_flag[1] = (signed char)pbuf[35];
		}
		int active = (image_flag[0]>=image_flag[1]) ? 0 : 1;
		printk("Active image: %d  flag: %02x\n", active, image_flag[active]);
		
		if(EPD_VERSION != p32[active*8+7]){
			// 当前运行的固件与flash中的固件不同
			// 将当前固件写入非活动的image中
			int new_flag = image_flag[active]+1;
			int new_id = active^1;
			
			// 擦除flash
			printk("Erase %08x ...\n", image_addr[new_id]);
			sf_erase(image_addr[new_id], firm_size+64, 1);
			// 初始化image header
			memset(pbuf, 0xff, 64);
			p32[0] = (new_flag<<24)|0x00aa5170;
			p32[1] = firm_size;
			p32[2] = firm_crc;
			p32[7] = EPD_VERSION;
			pbuf[0x20] = 0;

			// 写入flash
			u8 *firm_data = (u8*)0x07fc0000;
			int addr = image_addr[new_id];
			for(int i=0; i<firm_size+64; i+=256){
				if(i){
					memcpy(pbuf, firm_data, 256);
					firm_data += 256;
				}else{
					memcpy(pbuf+64, firm_data, 192);
					firm_data += 192;
				}
				sf_page_write(addr, pbuf, 256);
				sf_wait();
				addr += 256;
			}
			printk("Firm update done.\n\n");
		}

	}else{
		// 从Flash启动。读Boot header。
		sf_read(0, 16, pbuf);
		printk("Boot Header: %08x %08x\n", *(u32*)(pbuf+0), *(u32*)(pbuf+4));
	}

	fspi_exit();
	return 0;
}

/******************************************************************************/


int ota_state = 0;
static u8  ota_buf[256];
static int firm_addr;
static int firm_size;
static int firm_flag;


void sf_dumpp(int addr, int size)
{
#if 1
	for(int i=0; i<size; i+=256){
		sf_read(addr, 256, ota_buf);
		for(int j=0; j<256; j++){
			if((j%16)==0){
				printk("\n%08x: ", addr+j);
			}
			printk(" %02x", ota_buf[j]);
		}
		printk("\n");

		addr += 256;
	}
#endif
}

int ota_handle(u8 *buf)
{
	u8 *pbuf = ota_buf;
	u32 *p32 = (u32*)pbuf;
	int image_addr[2];
	int image_flag[2];

	if(buf[0]==0xa0){
		// 升级开始
		// 先擦除非活动固件
		ota_state = 1;

		firm_size = *(u16*)(buf+2);
		printk("firm_size: %04x (%d)\n", firm_size, firm_size);

		fspi_init();
		int id = sf_readid();
		printk("flash id: %04x\n", id);

		sf_read(0x38000, 16, pbuf);
		image_addr[0] = p32[1];
		image_addr[1] = p32[2];
		printk("image0: %08x   image1: %08x\n", image_addr[0], image_addr[1]);

		// 读image header
		sf_read(image_addr[0], 32, pbuf+0 );
		sf_read(image_addr[1], 32, pbuf+32);

		// 获取当前使用的image的id
		image_flag[0] = -1;
		image_flag[1] = -1;
		if(pbuf[ 0]==0x70 && pbuf[ 1]==0x51 && pbuf[ 2]==0xaa){
			image_flag[0] = (signed char)pbuf[ 3];
		}
		if(pbuf[32]==0x70 && pbuf[33]==0x51 && pbuf[34]==0xaa){
			image_flag[1] = (signed char)pbuf[35];
		}
		int active = (image_flag[0]>=image_flag[1]) ? 0 : 1;
		firm_flag = image_flag[active]+1;
		int new_id = active^1;

		firm_addr = image_addr[new_id];
		// 擦除flash
		printk("Erase %08x - %08x ...\n", firm_addr, firm_addr+firm_size+64);
		sf_erase(firm_addr, firm_size+64, 1);

		arch_set_sleep_mode(ARCH_SLEEP_OFF);
	}else if(buf[0]==0xa2){
		// 传输page的前128字节
		memcpy(ota_buf, buf+8, 128);
	}else if(buf[0]==0xa3){
		// 传输page的后128字节
		memcpy(ota_buf+128, buf+8, 128);
		if(ota_state==1){
			ota_buf[3] = firm_flag;
			printk("Firm Header: %08x %08x %08x %08x\n",
				*(u32*)(ota_buf+0),
				*(u32*)(ota_buf+4),
				*(u32*)(ota_buf+8),
				*(u32*)(ota_buf+28)
			);
		}

		// 先等待上一次写操作完成
		//fspi_init();
		int addr = firm_addr+(ota_state-1)*256;
		sf_page_write(addr, ota_buf, 256);
		int status = sf_wait();
		//printk("page_write: %08x  status:%04x\n", addr, status);
		ota_state += 1;
	}else if(buf[0]==0xa4){
		ota_state = 0;
		arch_set_sleep_mode(ARCH_EXT_SLEEP_ON);
	}

	return 0;
}


/******************************************************************************/

