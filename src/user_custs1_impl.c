/**
 ****************************************************************************************
 *
 * @file user_custs1_impl.c
 *
 * @brief Peripheral project Custom1 Server implementation source code.
 *
 * Copyright (C) 2015-2023 Renesas Electronics Corporation and/or its affiliates.
 * All rights reserved. Confidential Information.
 *
 * This software ("Software") is supplied by Renesas Electronics Corporation and/or its
 * affiliates ("Renesas"). Renesas grants you a personal, non-exclusive, non-transferable,
 * revocable, non-sub-licensable right and license to use the Software, solely if used in
 * or together with Renesas products. You may make copies of this Software, provided this
 * copyright notice and disclaimer ("Notice") is included in all such copies. Renesas
 * reserves the right to change or discontinue the Software at any time without notice.
 *
 * THE SOFTWARE IS PROVIDED "AS IS". RENESAS DISCLAIMS ALL WARRANTIES OF ANY KIND,
 * WHETHER EXPRESS, IMPLIED, OR STATUTORY, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. TO THE
 * MAXIMUM EXTENT PERMITTED UNDER LAW, IN NO EVENT SHALL RENESAS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE, EVEN IF RENESAS HAS BEEN ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES. USE OF THIS SOFTWARE MAY BE SUBJECT TO TERMS AND CONDITIONS CONTAINED IN
 * AN ADDITIONAL AGREEMENT BETWEEN YOU AND RENESAS. IN CASE OF CONFLICT BETWEEN THE TERMS
 * OF THIS NOTICE AND ANY SUCH ADDITIONAL LICENSE AGREEMENT, THE TERMS OF THE AGREEMENT
 * SHALL TAKE PRECEDENCE. BY CONTINUING TO USE THIS SOFTWARE, YOU AGREE TO THE TERMS OF
 * THIS NOTICE.IF YOU DO NOT AGREE TO THESE TERMS, YOU ARE NOT PERMITTED TO USE THIS
 * SOFTWARE.
 *
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "gpio.h"
#include "app_api.h"
#include "app.h"
#include "prf_utils.h"
#include "custs1.h"
#include "custs1_task.h"
#include "user_custs1_def.h"
#include "user_custs1_impl.h"
#include "user_peripheral.h"
#include "user_periph_setup.h"
#include "adc.h"

#include "epd.h"

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

ke_msg_id_t timer_used      __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint16_t indication_counter __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint16_t non_db_val_counter __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
int adcval;


static void get_holiday(void);

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */

int adc1_update(void)
{
	adc_offset_calibrate(ADC_INPUT_MODE_SINGLE_ENDED);
	adcval = adc_get_vbat_sample(false);
	int volt = (adcval*225)>>7;

	struct custs1_val_set_req *req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_SET_REQ, prf_get_task_from_id(TASK_ID_CUSTS1), TASK_APP, custs1_val_set_req, DEF_SVC1_ADC_VAL_1_CHAR_LEN);
	req->conidx = app_env->conidx;
	req->handle = SVC1_IDX_ADC_VAL_1_VAL;
	req->length = DEF_SVC1_ADC_VAL_1_CHAR_LEN;
	req->value[0] = volt&0xff;
	req->value[1] = volt>>8;
	KE_MSG_SEND(req);
	
	return volt;
}


/****************************************************************************************/

static const uint16_t lunar_year_info[32] =
{
	0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530, //2020-2029
	0x05aa0, 0x076a3, 0x096d0, 0x04afb, 0x04ad0, 0x0a4d0, 0x0d0b6, 0x0d250, 0x0d520, 0x0dd45, //2030-2039
	0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x0b255, 0x06d20, 0x0ada0, //2040-2049
	0x04b63, 0x09370,                                                                         //2050-2051
};
static const uint32_t lunar_year_info2 = 0x48010000;


// 每个节气相对于"小寒"的秒数
static int jieqi_info[] =
{
	0,        1272283,  2547462,  3828568,  5117483,  6416376,
	7726093,  9047327,  10379235, 11721860, 13072410, 14428379,
	15787551, 17145937, 18501082, 19850188, 21190911, 22520708,
	23839844, 25146961, 26443845, 27730671, 29010666, 30284551,
};

// 已知2020年"小寒"相对于1月1日的秒数
#define xiaohan_2020  451804


int year=2025, month=0, date=0, wday=3;
int l_year=4, l_month=11, l_date=1;
int hour=0, minute=0, second=0;


static int get_lunar_mdays(int mon, int *yinfo_out)
{
	int lflag = mon&0x80;
	mon &= 0x7f;

	// 取得当年的信息
	int yinfo = lunar_year_info[l_year];
	if(lunar_year_info2&(1<<l_year))
		yinfo |= 0x10000;

	// 取得当月的天数
	int mdays = 29;
	if(lflag){
		if(yinfo&0x10000) mdays += 1;
	}else{
		if(yinfo&(0x8000>>mon))	mdays += 1;
	}

	if(yinfo_out)
		*yinfo_out = yinfo;
	return mdays;
}


// 农历增加一天
void ldate_inc(void)
{
	int lflag = l_month&0x80;
	int mon = l_month&0x7f;
	int yinfo;

	int mdays = get_lunar_mdays(l_month, &yinfo);

	l_date += 1;
	if(l_date==mdays){
		l_date = 0;
		mon += 1;
		if(lflag==0 && mon==(yinfo&0x0f)){
			lflag = 0x80;
			mon -= 1;
		}else{
			lflag = 0;
		}
		if(mon==12){
			mon = 0;
			l_year += 1;
		}
		l_month = lflag|mon;
	}
}


// 给出年月日，返回是否是节气日
int jieqi(int year, int month, int date)
{
	uint8_t d2m[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
	int is_leap = (year%4)? 0 : (year%100)? 1: (year%400)? 0: 1;
	d2m[1] += is_leap;

	// 计算当前日期是本年第几天
	int i, d=0;
	for(i=0; i<month; i++){
		d += d2m[i];
	}
	d += date;

	// 计算闰年的天数。因为只考虑2020-2052年，这里做了简化。
	int Y = year-2020;
	int L = (Y)? (Y-1)/4+1 : 0;
	// 计算当年小寒的秒数
	int xiaohan_sec = xiaohan_2020 + 20950*Y - L*86400;
	//    20926是一个回归年(365.2422)不足一天的秒数(.2422*86400).
	//    直接用有明显的误差。这里稍微增大了一点(20926+24)。

	for(i=0; i<24; i++){
		int sec = xiaohan_sec + jieqi_info[i];
		int day = sec/86400;
		if(day==d)
			return i;
	}

	return -1;
}


/****************************************************************************************/


static int get_month_day(int mon)
{
	uint8_t d2m[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
	int is_leap = (year%4)? 0 : (year%100)? 1: (year%400)? 0: 1;
	d2m[1] += is_leap;

	return d2m[month];
}


// 增加1天
void date_inc(void)
{
	wday += 1;
	if(wday>=7)
		wday = 0;
	
	date += 1;
	if(date==get_month_day(month)){
		date = 0;
		month += 1;
		if(month>=12){
			month = 0;
			year += 1;
		}
	}
}

// 0: 状态不变
// 1: 分钟改变
// 2: 分钟改变10分钟
// 3: 小时改变
// 4: 天数改变

int clock_update(int inc)
{
	int retv = 0;

	second += inc;
	if(second<60)
		return retv;
	second -= 60;

	minute += 1;
	retv = 1;
	if((minute%10)==0)
		retv = 2;

	if(minute>=60){
		minute = 0;
		hour += 1;
		retv = 3;
		if(hour>=24){
			hour = 0;
			date_inc();
			ldate_inc();
			get_holiday();
			retv = 4;
		}
	}

	return retv;
}

void clock_set(uint8_t *buf)
{
	year   = buf[1] + buf[2]*256;
	month  = buf[3];
	date   = buf[4]-1;
	hour   = buf[5];
	minute = buf[6];
	second = buf[7];
	wday   = buf[8];
	l_year = buf[9];
	l_month= buf[10];
	l_date = buf[11]-1;

	get_holiday();
}


void clock_push(void)
{
	uint8_t buf[8];
	struct custs1_val_set_req *req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_SET_REQ, prf_get_task_from_id(TASK_ID_CUSTS1), TASK_APP, custs1_val_set_req, 8);

	req->conidx = app_env->conidx;
	req->handle = SVC1_IDX_LONG_VALUE_VAL;
	req->length = 8;
	req->value[0] = year&0xff;
	req->value[1] = year>>8;
	req->value[2] = month;
	req->value[3] = date+1;
	req->value[4] = hour;
	req->value[5] = minute;
	req->value[6] = second;
	req->value[7] = 0;
	KE_MSG_SEND(req);
}


void clock_print(void)
{
	printk("\n%04d-%02d-%02d %02d:%02d:%02d  L: %d-%d\n", year, month+1, date+1, hour, minute, second, l_month+1, l_date+1);
}


/****************************************************************************************/

static char *jieqi_name[] = {
	"小寒", "大寒", "立春", "雨水", "惊蛰", "春分",
	"清明", "谷雨", "立夏", "小满", "芒种", "夏至",
	"小暑", "大暑", "立秋", "处暑", "白露", "秋分",
	"寒露", "霜降", "立冬", "小雪", "大雪", "冬至",
};
static char *wday_str[] = {"日", "一", "二", "三", "四", "五", "六"};
static char *lday_str_lo[] = {"一", "二", "三", "四", "五", "六", "七", "八", "九", "十", "冬", "腊", "正"};
static char *lday_str_hi[] = {"初", "十", "廿", "二", "三"};

static int epd_wait_state;
static timer_hnd epd_wait_hnd;

typedef struct {
	char *name;
	uint8_t mon;
	uint8_t day;
}HOLIDAY_INFO;

HOLIDAY_INFO hday_info[] = {
	{"除夕",   0xc0|12, 30},
	{"春节",   0x80| 1,  1},
	{"元宵节", 0x80| 1, 15},
	{"龙抬头", 0x80| 2,  2},
	{"端午节", 0x80| 5,  5},
	{"七夕节", 0x80| 7,  7},
	{"中秋节", 0x80| 8, 15},
	{"重阳节", 0x80| 9,  9},
	{"腊八节", 0x80|12,  8},

	{"元旦节",       1,  1},
	{"情人节",       2, 14},
	{"妇女节",       3,  8},
	{"植树节",       3, 12},
	{"愚人节",       4,  1},
	{"劳动节",       5,  1},
	{"青年节",       5,  4},
	{"母亲节",       5, 0x97}, // 5月第二个周日
	{"儿童节",       6,  1},
	{"父亲节",       6, 0xa7}, // 6月第三个周日
	{"教师节",       9, 10},
	{"国庆节",      10,  1},
	{"程序员节",    10, 24},
	{"万圣节",      11,  1},
	{"光棍节",      11, 11},
	{"平安夜",      12, 24},
	{"圣诞节",      12, 25},
	{"",             0,  0},
};

static char *jieqi_str = "小寒";
static char *holiday_str = "元旦节";

static void ldate_str(char *buf)
{
	char *lflag = (l_month&0x80)? "闰" : "";
	int lm = l_month&0x7f;
	if(lm==0){
		lm = 12;
	}

	int hi = l_date/10;
	int lo = l_date%10;
	
	if(lo==9){
		if(hi==1)
			hi = 3;
		else if(hi==2)
			hi = 4;
	}
	
	sprintf(buf, "%s%s月%s%s", lflag, lday_str_lo[lm], lday_str_hi[hi], lday_str_lo[lo]);
}


static void set_holiday(int index)
{
	if(holiday_str==NULL){
		holiday_str = hday_info[index].name;
	}else if(jieqi_str==NULL){
		// 已经有一个农历节日了，将其转移到节气位置。
		jieqi_str = holiday_str;
		holiday_str = hday_info[index].name;
	}else{
		// printf("OOPS! 节日溢出!\n");
	}
}

void get_holiday(void)
{
	int i;

	jieqi_str = NULL;
	holiday_str = NULL;

	i = jieqi(year, month, date);
	if(i>=0){
		jieqi_str = jieqi_name[i];
	}

	i = 0;
	while(hday_info[i].mon){
		int mon = hday_info[i].mon;
		int day = hday_info[i].day;
		int mflag = mon&0xc0;
		int dflag = day;
		mon = (mon&0x0f)-1;
		day = (day&0x1f)-1;
		if(mflag&0x80){
			// 农历节日
			if(mflag&0x40){
				// 当月最后一天
				int mdays = get_lunar_mdays(l_month, NULL);
				day = mdays-1;
			}
			if(l_month==mon && l_date==day){
				set_holiday(i);
			}
		}else{
			// 公历节日
			if(dflag&0x80){
				// 第几个周天
				int wc = date/7;
				int hwc = (dflag>>4)&0x03;
				day &= 0x07;
				if(month==mon && wc==hwc && wday==day){
					set_holiday(i);
				}
			}else if(month==mon && date==day){
				set_holiday(i);
			}
		}
		i += 1;
	}

	return;
}


static uint8_t batt_cal(uint16_t adc_sample)
{
    uint8_t batt_lvl;

    if (adc_sample > 1705)
        batt_lvl = 100;
    else if (adc_sample <= 1705 && adc_sample > 1584)
        batt_lvl = 28 + (uint8_t)(( ( ((adc_sample - 1584) << 16) / (1705 - 1584) ) * 72 ) >> 16) ;
    else if (adc_sample <= 1584 && adc_sample > 1360)
        batt_lvl = 4 + (uint8_t)(( ( ((adc_sample - 1360) << 16) / (1584 - 1360) ) * 24 ) >> 16) ;
    else if (adc_sample <= 1360 && adc_sample > 1136)
        batt_lvl = (uint8_t)(( ( ((adc_sample - 1136) << 16) / (1360 - 1136) ) * 4 ) >> 16) ;
    else
        batt_lvl = 0;

    return batt_lvl;
}

static void draw_batt(int x, int y)
{
	int p = batt_cal(adcval);
	p /= 10;

	draw_rect(x, y-4, x+14, y+4, BLACK);
	draw_box(x-2, y-1, x-1, y+1, BLACK);

	draw_box(x+12-p, y-2, x+12, y+2, BLACK);
}

static void draw_bt(int x, int y)
{
	int i;

	draw_vline(x, y-4, y+4, BLACK);
	
	for(i=0; i<5; i++){
		draw_pixel(x-2+i, y-2+i, BLACK);
		draw_pixel(x-2+i, y+2-i, BLACK);
	}
	draw_pixel(x+1, y-3, BLACK);
	draw_pixel(x+1, y+3, BLACK);
}


static void epd_wait_timer(void)
{
	if(epd_busy()){
		epd_wait_hnd = app_easy_timer(40, epd_wait_timer);
	}else{
		epd_wait_hnd = EASY_TIMER_INVALID_TIMER;
		epd_cmd1(0x10, 0x01);
		epd_power(0);
		epd_hw_close();
		arch_set_sleep_mode(ARCH_EXT_SLEEP_ON);
	}
}


void clock_draw(int flags)
{
	char tbuf[64];

	epd_hw_open();

	epd_update_mode(flags&3);

	memset(fb_bw, 0xff, scr_h*line_bytes);
	memset(fb_rr, 0x00, scr_h*line_bytes);

	// 显示电池电量
	draw_batt(190, 13);
	if(flags&DRAW_BT){
		// 显示蓝牙图标
		draw_bt(180, 13);
	}

	// 使用大字显示时间
	select_font(1);
	sprintf(tbuf, "%02d:%02d", hour, minute);
	draw_text(12, 25, tbuf, BLACK);

	// 显示公历日期
	sprintf(tbuf, "%4d年%2d月%2d日   星期%s", year, month+1, date+1, wday_str[wday]);
	select_font(0);
	draw_text(15, 8, tbuf, BLACK);

	// 显示农历日期(不显示年)
	ldate_str(tbuf);
	draw_text(12, 85, tbuf, BLACK);
	// 显示节气与节假日
	if(jieqi_str)
		draw_text( 98, 85, jieqi_str, BLACK);
	if(flags&DRAW_BT){
		draw_text(152, 85, bt_id, BLACK);
	}else if(holiday_str){
		draw_text(152, 85, holiday_str, BLACK);
	}

	// 墨水屏更新显示
	epd_init();
	epd_screen_update();
	epd_update();
	// 更新时如果深度休眠，会花屏。 这里暂时关闭休眠。
	arch_set_sleep_mode(ARCH_SLEEP_OFF);
	epd_wait_hnd = app_easy_timer(40, epd_wait_timer);
}


/****************************************************************************************/


void user_svc1_ctrl_wr_ind_handler(ke_msg_id_t const msgid, struct custs1_val_write_ind const *param, ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
	printk("Control Point: %02x\n", param->value[0]);

}

void user_svc1_long_val_wr_ind_handler(ke_msg_id_t const msgid, struct custs1_val_write_ind const *param, ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
	printk("Long value: %d\n", param->length);
	if(param->value[0]==0x91){
		clock_set((uint8_t*)param->value);
		clock_draw(DRAW_BT|UPDATE_FAST);
		clock_print();
	}
}

void user_svc1_long_val_att_info_req_handler(ke_msg_id_t const msgid, struct custs1_att_info_req const *param, ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct custs1_att_info_rsp *rsp = KE_MSG_ALLOC(CUSTS1_ATT_INFO_RSP, src_id, dest_id, custs1_att_info_rsp);
    rsp->conidx  = app_env[param->conidx].conidx;
    rsp->att_idx = param->att_idx;
    rsp->length  = 0;
    rsp->status  = ATT_ERR_NO_ERROR;

    KE_MSG_SEND(rsp);
}

void user_svc1_rest_att_info_req_handler(ke_msg_id_t const msgid,
                                            struct custs1_att_info_req const *param,
                                            ke_task_id_t const dest_id,
                                            ke_task_id_t const src_id)
{
    struct custs1_att_info_rsp *rsp = KE_MSG_ALLOC(CUSTS1_ATT_INFO_RSP,
                                                   src_id,
                                                   dest_id,
                                                   custs1_att_info_rsp);
    // Provide the connection index.
    rsp->conidx  = app_env[param->conidx].conidx;
    // Provide the attribute index.
    rsp->att_idx = param->att_idx;
    // Force current length to zero.
    rsp->length  = 0;
    // Provide the ATT error code.
    rsp->status  = ATT_ERR_WRITE_NOT_PERMITTED;

    KE_MSG_SEND(rsp);
}

