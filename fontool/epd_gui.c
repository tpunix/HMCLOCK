
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "gdifb.h"


/******************************************************************************/


typedef unsigned  char  u8;
typedef unsigned short  u16;
typedef unsigned   int  u32;


#define BLACK 0x00000000
#define WHITE 0x00ffffff
#define RED   0x000000ff
#define GREEN 0x0000ff00
#define BLUE  0x00ff0000



/******************************************************************************/


void draw_pixel(int x, int y, int color)
{
	gdifb_pixel(x, y, color);
}


void draw_hline(int y, int x1, int x2, int color)
{
	gdifb_line(x1, y, x2, y, color);
}


void draw_vline(int x, int y1, int y2, int color)
{
	gdifb_line(x, y1, x, y2, color);
}


void draw_rect(int x1, int y1, int x2, int y2, int color)
{
	gdifb_rect(x1, y1, x2, y2, color);
}


void draw_box(int x1, int y1, int x2, int y2, int color)
{
	gdifb_box(x1, y1, x2, y2, color);
}

/******************************************************************************/


#include "sfont.h"
#include "sfont16.h"
#include "font50.h"
#include "font66.h"

const u8 *font_list[6] = {
	sfont,
	sfont16,
	F_DSEG7_50,
	F_DSEG7_66,
};

const u8 *current_font = (u8*)sfont;

int select_font(int id)
{
	current_font = font_list[id];
	return 0;
}


static const u8 *find_font(const u8 *font, int ucs)
{
	int total = *(u16*)font;
	int i;

	for(i=0; i<total; i++){
		if(*(u16*)(font+2+i*4+0)==ucs){
			int offset = *(u16*)(font+2+i*4+2);
			//printf("  %04x at %04x\n", ucs, offset);
			return font+offset;
		}
	}

	return NULL;
}


int fb_draw_font(int x, int y, int ucs, int color)
{
	int r, c;
	const u8 *font_data = find_font(current_font, ucs);
	if(font_data==NULL){
		printf("fb_draw %04x: not found!\n", ucs);
	}

	int ft_adv = font_data[0];
	int ft_bw = font_data[1];
	int ft_bh = font_data[2];
	int ft_bx = (signed char)font_data[3];
	int ft_by = (signed char)font_data[4];
	int ft_lsize = (ft_bw+7)/8;
	font_data += 5;

	x += ft_bx;
	y += ft_by;

	for (r=0; r<ft_bh; r++) {
		for (c=0; c<ft_bw; c++) {
			int b = font_data[c>>3];
			int mask = 0x80>>(c%8);
			if(b&mask){
				draw_pixel(x+c, y, color);
			}
			mask >>= 1;
		}
		font_data += ft_lsize;
		y += 1;
	}

	return ft_adv;
}


static int utf8_to_ucs(char **ustr)
{
	u8 *str = (u8*)*ustr;
	int ucs = 0;

	if(*str==0){
		return 0;
	}else if(*str<0x80){
		*ustr = (char*)str+1;
		return *str;
	}else if(*str<0xe0){
		ucs = ((str[0]&0x1f)<<6) | (str[1]&0x3f);
		*ustr = (char*)str+2;
		return ucs;
	}else{
		ucs = ((str[0]&0x0f)<<12) | ((str[1]&0x3f)<<6) | (str[2]&0x3f);
		*ustr = (char*)str+3;
		return ucs;
	}
}


void draw_text(int x, int y, char *str, int color)
{
	int ch;

	while(1){
		ch = utf8_to_ucs(&str);
		if(ch==0)
			break;
		x += fb_draw_font(x, y, ch, color);
	}
}


/******************************************************************************/

char *wday_str[] = {"日", "一", "二", "三", "四", "五", "六"};

static int wday = 0;

typedef struct {
	int xres, yres;
	int font_char;
	int font_dseg;
	int top;
	int mid;
	int bottom;
	int t1;
	int m1;
	int b1;
}LAYOUT;

LAYOUT layouts[3] = {
	{212, 104, 0, 2, 6, 27,  82, 15, 20, 12},
	{250, 122, 1, 3, 6, 28,  98, 15, 16, 12},
	{296, 128, 1, 3, 6, 30, 102, 15, 36, 12},
};

int current_layout = 0;

void fb_test(void)
{
	LAYOUT *lt = &layouts[current_layout];

	// 显示公历日期
	select_font(lt->font_char);
	char tbuf[64];
	sprintf(tbuf, "%4d年%2d月%2d日 星期%s", 2025, 4, 29, wday_str[wday]);
	draw_text(lt->t1, lt->top, tbuf, BLACK);
	// 显示农历日期(不显示年)
	sprintf(tbuf, "%s月%s日", wday_str[wday], wday_str[wday]);
	draw_text(lt->b1, lt->bottom, tbuf, BLACK);

	wday += 1;
	if(wday==7)
		wday = 0;

	// 使用大字显示时间
	select_font(lt->font_dseg);
	sprintf(tbuf, "%02d:%02d", 2+wday, 31+wday);
	draw_text(lt->m1, lt->mid, tbuf, BLACK);
}


/******************************************************************************/

int main(int argc, char *argv[])
{
	int scr_w, scr_h;


//	scr_w = 212; scr_h = 104; current_layout = 0;
//	scr_w = 250; scr_h = 122; current_layout = 1;
	scr_w = 296; scr_h = 128; current_layout = 2;


	gdifb_init(scr_w, scr_h);
	draw_box(0, 0, scr_w-1, scr_h-1, WHITE);
	draw_rect(0, 0, scr_w-1, scr_h-1, RED);

	fb_test();

	gdifb_flush();
	gdifb_exit();

	return 0;
}


