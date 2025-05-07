

#include "user_config.h"


typedef unsigned  char  u8;
typedef unsigned short  u16;
typedef unsigned   int  u32;


void delay_ms(int ms);

// GPIO
void gpio_config(int index, int mode, int value);
void gpio_set(int index, int value);
int gpio_get(int index);


// spi flash
int fspi_init(u32 gpio_word);
int epcs_readid(void);
int epcs_sector_erase(int addr);
int epcs_page_write(int addr, u8 *buf, int size);
int epcs_read(int addr, int len, u8 *buf);

// epd_hw
void epd_hw_init(u32 config0, u32 config1);
void epd_hw_open(void);
void epd_hw_close(void);
void epd_reset(void);
void epd_wait(void);
void epd_power(int on);

void epd_cmd(int cmd);
void epd_cmd1(int cmd, int d0);
void epd_cmd2(int cmd, int d0, int d1);
void epd_cmd3(int cmd, int d0, int d1, int d2);
void epd_cmd4(int cmd, int d0, int d1, int d2, int d3);
void epd_data(int data);
void epd_data_array(u8 *data, int len);
void epd_read(u8 *data, int len);
void epd_cmd_read(int cmd, u8 *data, int len);

// epd_xxx

void epd_init(int x, int y, int mode);
void epd_load_lut(u8 *lut);
void epd_update(int mode);
void epd_sleep(void);
void epd_window(int x1, int y1, int x2, int y2);
void epd_screen_update(void);
void epd_screen_clean(int mode);


extern u8 lut_p[];


// epd_gui
void draw_pixel(int x, int y, int color);
void draw_hline(int y, int x1, int x2, int color);
void draw_vline(int x, int y1, int y2, int color);
void draw_rect(int x1, int y1, int x2, int y2, int color);
void draw_box(int x1, int y1, int x2, int y2, int color);
void draw_char(int x, int y, int ch, int color);
void draw_text(int x, int y, char *str, int color);
int select_font(int id);
void fb_test(void);


#define EPD_BWR   0x20
#define MIRROR_H  0x40
#define MIRROR_V  0x80
#define ROTATE_0  0x00 // 0
#define ROTATE_1  0x01 // 90
#define ROTATE_2  0x02 // 180
#define ROTATE_3  0x03 // 270

#define BLACK     0
#define WHITE     1
#define RED       2


extern int scr_w;
extern int scr_h;
extern int scr_mode;
extern int line_bytes;
extern int scr_padding;

extern int win_w;
extern int win_h;

extern int fb_w;
extern int fb_h;

extern u8 fb_bw[];
extern u8 fb_rr[];


