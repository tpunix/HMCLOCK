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

#include "gpio.h"               // GPIO控制相关头文件
#include "app_api.h"            // 应用程序API
#include "app.h"                // 应用程序核心功能
#include "prf_utils.h"          // BLE配置文件工具
#include "custs1.h"             // 自定义服务1
#include "custs1_task.h"        // 自定义服务1任务
#include "user_custs1_def.h"    // 自定义服务1定义
#include "user_custs1_impl.h"   // 自定义服务1实现
#include "user_peripheral.h"    // 用户外设相关
#include "user_periph_setup.h"  // 用户外设设置
#include "adc.h"                // ADC(模数转换)相关

#include "epd.h"                // 电子墨水屏驱动

/*
 * 全局变量定义
 * 这些变量使用__SECTION_ZERO("retention_mem_area0")属性放置在掉电保持内存区域
 ****************************************************************************************
 */

// 定时器ID，用于系统定时任务
ke_msg_id_t timer_used      __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
// 指示计数器，用于BLE通知计数
uint16_t indication_counter __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
// 非数据库值计数器
uint16_t non_db_val_counter __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
// ADC采样值，用于电池电量检测
int adcval;

static uint8_t h24_format = 1; // 24小时制标志

static void get_holiday(void);

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */


/**
 * @brief 更新ADC采样值并通过BLE发送电池电压数据
 * 
 * 该函数执行以下操作：
 * 1. 校准ADC偏移量
 * 2. 获取电池电压采样值
 * 3. 将采样值转换为实际电压值
 * 4. 通过BLE发送电压值给连接的设备
 * 
 * @return 返回计算后的电压值
 */
int adc1_update(void)
{
    // 校准ADC偏移，使用单端输入模式
    adc_offset_calibrate(ADC_INPUT_MODE_SINGLE_ENDED);
    // 获取电池电压采样值
    adcval = adc_get_vbat_sample(false);
    // 将ADC值转换为实际电压值 (单位: mV)
    int volt = (adcval*225)>>7;

    // 分配内存并构造BLE消息
    struct custs1_val_set_req *req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_SET_REQ, 
                                                      prf_get_task_from_id(TASK_ID_CUSTS1), 
                                                      TASK_APP, 
                                                      custs1_val_set_req, 
                                                      DEF_SVC1_ADC_VAL_1_CHAR_LEN);
    // 设置连接索引
    req->conidx = app_env->conidx;
    // 设置特征值句柄
    req->handle = SVC1_IDX_ADC_VAL_1_VAL;
    // 设置数据长度
    req->length = DEF_SVC1_ADC_VAL_1_CHAR_LEN;
    // 设置电压值（16位，低字节在前）
    req->value[0] = volt&0xff;
    req->value[1] = volt>>8;
    // 发送BLE消息
    KE_MSG_SEND(req);
    
    return volt;
}


/****************************************************************************************/

/**
 * 农历年份数据表（2020-2051年）
 * 每个16位数据包含以下信息：
 * - 高4位(bit 15-12): 闰月月份，0表示当年无闰月
 * - 低12位(bit 11-0): 每个月的大小月标记，1表示大月(30天)，0表示小月(29天)
 * 例如：0x07954
 * - 0: 无闰月
 * - 7954: 从正月到十二月的大小月情况
 */
static const uint16_t lunar_year_info[32] =
{
    0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530, //2020-2029
    0x05aa0, 0x076a3, 0x096d0, 0x04afb, 0x04ad0, 0x0a4d0, 0x0d0b6, 0x0d250, 0x0d520, 0x0dd45, //2030-2039
    0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x0b255, 0x06d20, 0x0ada0, //2040-2049
    0x04b63, 0x09370,                                                                         //2050-2051
};

// 额外的农历年份信息，用于标记特殊年份的闰月情况
static const uint32_t lunar_year_info2 = 0x48010000;


/**
 * 24节气时间数据表
 * 存储了一年中24个节气相对于"小寒"的时间间隔（以秒为单位）
 * 
 * 节气顺序：
 * 1-6月：小寒、大寒、立春、雨水、惊蛰、春分
 * 7-12月：清明、谷雨、立夏、小满、芒种、夏至
 * 13-18月：小暑、大暑、立秋、处暑、白露、秋分
 * 19-24月：寒露、霜降、立冬、小雪、大雪、冬至
 */
static int jieqi_info[] =
{
    0,        1272283,  2547462,  3828568,  5117483,  6416376,  // 小寒到春分
    7726093,  9047327,  10379235, 11721860, 13072410, 14428379, // 清明到夏至
    15787551, 17145937, 18501082, 19850188, 21190911, 22520708, // 小暑到秋分
    23839844, 25146961, 26443845, 27730671, 29010666, 30284551, // 寒露到冬至
};

/**
 * 2020年"小寒"节气的基准时间
 * 相对于2020年1月1日的秒数
 * 用于计算其他年份的节气时间
 */
#define xiaohan_2020  451804


/**
 * 全局时间变量定义
 * 公历日期相关变量：
 * year: 年份，如2025
 * month: 月份，0-11表示1-12月
 * date: 日期，0-30表示1-31日
 * wday: 星期，0-6表示星期日到星期六
 * 
 * 农历日期相关变量：
 * l_year: 农历年在lunar_year_info数组中的索引，如4表示2024年
 * l_month: 农历月份，0-11表示正月到腊月，最高位1表示闰月
 * l_date: 农历日期，0-29表示初一到三十
 * 
 * 时间相关变量：
 * hour: 小时，0-23
 * minute: 分钟，0-59
 * second: 秒，0-59
 */
int year=2025, month=0, date=0, wday=3;
int l_year=4, l_month=11, l_date=1;
int hour=0, minute=0, second=0;
// 上次对时后，经过的分钟数
int cal_minute=-1;


/**
 * 获取农历月份的天数
 * 
 * @param mon 月份编号，最高位为1表示闰月
 * @param yinfo_out 输出参数，用于返回年份信息
 * @return 返回该月的天数（29或30）
 */
static int get_lunar_mdays(int mon, int *yinfo_out)
{
    // 获取闰月标志（最高位）
    int lflag = mon&0x80;
    // 获取实际月份（去除闰月标志）
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

	if(cal_minute>=0)
		cal_minute += 1;

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

	cal_minute = 0;

	app_clock_timer_restart();
}


void clock_push(void)
{
	struct custs1_val_set_req *req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_SET_REQ, prf_get_task_from_id(TASK_ID_CUSTS1), TASK_APP, custs1_val_set_req, 11);

	req->conidx = app_env->conidx;
	req->handle = SVC1_IDX_LONG_VALUE_VAL;
	req->length = 11;
	req->value[0] = year&0xff;
	req->value[1] = year>>8;
	req->value[2] = month;
	req->value[3] = date+1;
	req->value[4] = hour;
	req->value[5] = minute;
	req->value[6] = second;
	req->value[7] = (cal_minute&0xff);
	req->value[8] = (cal_minute>>8 )&0xff;
	req->value[9] = (cal_minute>>16)&0xff;
	req->value[10]= (cal_minute>>24)&0xff;
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
    // 农历传统节日（0x80 表示农历日期，0xc0 表示月末）
    {"腊八",     0x80|12,  8},   // 农历腊月初八
    {"小年",     0x80|12, 23},   // 农历腊月廿三
    {"除夕",     0xc0|12, 30},   // 农历腊月最后一天
    {"春节",     0x80| 1,  1},   // 农历正月初一
    {"元宵节",   0x80| 1, 15},   // 农历正月十五
    {"龙抬头",   0x80| 2,  2},   // 农历二月初二
    {"寒食节",   0x80| 3,  3},   // 农历三月初三
    {"端午节",   0x80| 5,  5},   // 农历五月初五
    {"七夕节",   0x80| 7,  7},   // 农历七月初七
    {"中元节",   0x80| 7, 15},   // 农历七月十五
    {"中秋节",   0x80| 8, 15},   // 农历八月十五
    {"重阳节",   0x80| 9,  9},   // 农历九月初九
    {"下元节",   0x80|10, 15},   // 农历十月十五

    // 阳历法定节假日和纪念日
    {"元旦",         1,  1},
    {"湿地日",       2,  2},
    {"情人节",       2, 14},
    {"妇女节",       3,  8},
    {"植树节",       3, 12},
    {"权益日",       3, 15},
    {"愚人节",       4,  1},
    {"读书日",       4, 23},
    {"航天日",       4, 24},
    {"劳动节",       5,  1},
    {"青年节",       5,  4},
    {"护士节",       5, 12},
    {"儿童节",       6,  1},
    {"环境日",       6,  5},
    {"遗产日",       6,  8},
    {"建党节",       7,  1},
    {"建军节",       8,  1},
    {"抗战日",       9,  3},
    {"教师节",       9, 10},
    {"安全日",       9, 15},
    {"烈士日",       9, 30},
    {"国庆节",      10,  1},
	{"程序员节",    10, 24},
	{"万圣节",      10, 31},
    {"消防日",      11,  9},
    {"记者节",      11,  8},
	{"光棍节",      11, 11},
    {"宪法日",      12,  4},
    {"志愿日",      12,  5},
    {"公祭日",      12, 13},
	{"圣诞节",      12, 25},

    // 特殊周期性节日
    {"母亲节",       5, 0x97},  // 5月第二个周日
    {"父亲节",       6, 0xa7},  // 6月第三个周日
    {"感恩节",      11, 0xa4},  // 11月第四个周四

    {"",             0,  0}     // 结束标记
};


static char *jieqi_str = "小寒";
static char *holiday_str = "元旦";

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


/****************************************************************************************/


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

/**
 * 绘制电池电量图标
 * 
 * @param x 图标左上角的x坐标
 * @param y 图标中心的y坐标
 * 
 * 图标说明：
 * - 外框大小：16x8像素
 * - 电量显示：根据实际电量百分比填充内部
 * - 电池正极：2x2像素
 */
static void draw_batt(int x, int y)
{
    // 获取电池电量百分比并转换为显示段数（0-10）
    int p = batt_cal(adcval);
    p /= 10;

    // 绘制电池外框
    draw_rect(x, y-4, x+14, y+4, BLACK);
    // 绘制电池正极
    draw_box(x-2, y-1, x-1, y+1, BLACK);

    // 绘制电量填充部分
    draw_box(x+12-p, y-2, x+12, y+2, BLACK);
}


const u8 font_bt[] = {
	0x08, 0x08, 0x0f, 0x00, 0x00,
	0x10, 0x18, 0x14, 0x92, 0x51, 0x32, 0x14, 0x18,
	0x14, 0x32, 0x51, 0x92, 0x14, 0x18, 0x10,
};

/**
 * 绘制蓝牙图标
 * 
 * @param x 图标中心的x坐标
 * @param y 图标中心的y坐标
 * 
 * 图标说明: 一个8x15的字符
 */
static void draw_bt(int x, int y)
{
	fb_draw_font_info(x, y, font_bt, BLACK);
}


/****************************************************************************************/

typedef struct {
	int xres, yres;
	int font_char;
	int font_dseg;
	u16 x[8];
	u16 y[8];
}LAYOUT;

// 坐标0: 公历日期
// 坐标1: 蓝牙图标
// 坐标2: 电池图标
// 坐标3: 时间
// 坐标4: 农历日期
// 坐标5: 节气
// 坐标6: 节日
// 坐标7: 上下午

LAYOUT layouts[3] = {
	{212, 104, 0, 1,
		{15, 172, 190,  16,  12,  98, 150, 12},
		{ 6,   7,  14,  27,  82,  82,  82, 44},
	},
	{250, 122, 2, 3,
		{15, 206, 226,  12,  12, 118, 176, 15},
		{ 6,   8,  15,  28,  98,  98,  98, 50},
	},
	{296, 128, 2, 3,
		{15, 246, 268,  30,  12, 140, 220, 15,},
		{ 6,   8,  15,  30, 102, 102, 102, 52,},
	},
};

int current_layout = 0;

void select_layout(int xres, int yres)
{
	int i;

	for(i=0; i<3; i++){
		if(layouts[i].xres==xres && layouts[i].yres==yres){
			current_layout = i;
			return;
		}
	}
}


/**
 * 电子墨水屏更新等待定时器
 * 
 * 功能说明：
 * - 检查电子墨水屏是否处于忙状态
 * - 如果忙，则40ms后再次检查
 * - 如果空闲，则完成更新流程并进入省电模式
 * 
 * 电子墨水屏更新完成后的处理：
 * 1. 发送深度睡眠命令(0x10, 0x01)
 * 2. 关闭电源
 * 3. 关闭硬件接口
 * 4. 设置系统进入扩展睡眠模式
 */
static void epd_wait_timer(void)
{
    if(epd_busy()){
        // 屏幕仍在忙，40ms后再次检查
        epd_wait_hnd = app_easy_timer(40, epd_wait_timer);
    }else{
        // 屏幕更新完成
        epd_wait_hnd = EASY_TIMER_INVALID_TIMER;
        // 发送深度睡眠命令
        epd_cmd1(0x10, 0x01);
        // 关闭电源
        epd_power(0);
        // 关闭硬件接口
        epd_hw_close();
        // 设置系统进入扩展睡眠模式
        arch_set_sleep_mode(ARCH_EXT_SLEEP_ON);
    }
}

/**
 * 绘制时钟界面
 * 
 * @param flags 显示控制标志
 *             bit0-1: 更新模式（快速/正常）
 *             bit2: 是否显示蓝牙图标
 * 
 * 显示内容包括：
 * - 电池电量图标
 * - 蓝牙连接状态图标（可选）
 * - 大字号时间显示
 * - 公历日期和星期
 * - 农历日期
 * - 节气和节日信息
 */
void clock_draw(int flags)
{
	char tbuf[64];
	LAYOUT *lt = &layouts[current_layout];

	if(ota_state){
		return;
	}

	epd_hw_open();

	epd_update_mode(flags&3);

	memset(fb_bw, 0xff, scr_h*line_bytes);
	memset(fb_rr, 0x00, scr_h*line_bytes);

	// 显示电池电量
	draw_batt(lt->x[2], lt->y[2]);
	if(flags&DRAW_BT){
		// 显示蓝牙图标
		draw_bt(lt->x[1], lt->y[1]);
	}

	// 使用大字显示时间
	if(h24_format){
		// 24小时制
		select_font(lt->font_dseg);
		sprintf(tbuf, "%02d:%02d", hour, minute);
		draw_text(lt->x[3], lt->y[3], tbuf, BLACK);
	}else{
		// 12小时制
		int h = hour;
		int ampm = 0;
		if(h>=12){
			if(h>12)
				h -= 12;
			ampm = 1;
		}else if(h==0){
			h = 12; // 0点显示为12点
		}
		select_font(lt->font_dseg);
		sprintf(tbuf, "%2d:%02d", h, minute);
		draw_text(lt->x[3], lt->y[3], tbuf, BLACK);

		// 显示上午/下午
		select_font(lt->font_char);
		if(ampm){
			strcpy(tbuf, "下午");
		}else{
			strcpy(tbuf, "上午");
		}
		draw_text(lt->x[7], lt->y[7], tbuf, BLACK);
	}

	// 显示公历日期
	sprintf(tbuf, "%4d年%2d月%2d日   星期%s", year, month+1, date+1, wday_str[wday]);
	select_font(lt->font_char);
	draw_text(lt->x[0], lt->y[0], tbuf, BLACK);

	// 显示农历日期(不显示年)
	ldate_str(tbuf);
	draw_text(lt->x[4], lt->y[4], tbuf, BLACK);
	// 显示节气
	if(jieqi_str)
		draw_text(lt->x[5], lt->y[5], jieqi_str, BLACK);
	// 显示节假日
	if(flags&DRAW_BT){
		draw_text(lt->x[6], lt->y[6], bt_id, BLACK);
	}else if(holiday_str){
		draw_text(lt->x[6], lt->y[6], holiday_str, BLACK);
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


/**
 * 控制点写入指示处理函数
 * 
 * @param msgid 消息ID
 * @param param 写入参数
 * @param dest_id 目标任务ID
 * @param src_id 源任务ID
 * 
 * 处理通过BLE接收到的控制命令
 */
void user_svc1_ctrl_wr_ind_handler(ke_msg_id_t const msgid, 
                                  struct custs1_val_write_ind const *param, 
                                  ke_task_id_t const dest_id, 
                                  ke_task_id_t const src_id)
{
    // 打印接收到的控制命令
    printk("Control Point: %02x\n", param->value[0]);
}

/**
 * 长值特征写入指示处理函数
 * 
 * @param msgid 消息ID
 * @param param 写入参数
 * @param dest_id 目标任务ID
 * @param src_id 源任务ID
 * 
 * 处理命令：
 * - 0x91: 时钟设置命令
 * - 0xA0及以上: OTA升级相关命令
 */
void user_svc1_long_val_wr_ind_handler(ke_msg_id_t const msgid, 
                                      struct custs1_val_write_ind const *param, 
                                      ke_task_id_t const dest_id, 
                                      ke_task_id_t const src_id)
{
	if(param->value[0]==0x91){
		// 设置时钟
		clock_set((uint8_t*)param->value);
		// 更新显示（带蓝牙图标，快速更新模式）
		clock_draw(DRAW_BT|UPDATE_FAST);
		// 打印当前时间信息
		clock_print();
	}else if(param->value[0]==0x90){
		//修改24-12小时制
		h24_format = !h24_format;
		clock_draw(DRAW_BT|UPDATE_FAST);
	}else if(param->value[0]==0x92){
		int diff_sec;
		diff_sec  = param->value[1];
		diff_sec |= param->value[2]<<8;
		diff_sec  = (diff_sec<<16)>>16;
		printk("Calibration: %02x\n", diff_sec);
		clock_fixup_set(diff_sec, cal_minute);
		cal_minute = 0;
	}else if(param->value[0]>=0xa0){
		ota_handle((u8*)param->value);
	}else if(param->value[0]>=0xa0){
		// 处理OTA升级命令
		ota_handle((u8*)param->value);
    }
}

/**
 * 长值特征属性信息请求处理函数
 * 
 * @param msgid 消息ID
 * @param param 请求参数
 * @param dest_id 目标任务ID
 * @param src_id 源任务ID
 * 
 * 响应BLE客户端的属性信息请求
 */
void user_svc1_long_val_att_info_req_handler(ke_msg_id_t const msgid, 
                                            struct custs1_att_info_req const *param, 
                                            ke_task_id_t const dest_id, 
                                            ke_task_id_t const src_id)
{
    // 分配响应消息内存
    struct custs1_att_info_rsp *rsp = KE_MSG_ALLOC(CUSTS1_ATT_INFO_RSP, 
                                                   src_id, 
                                                   dest_id, 
                                                   custs1_att_info_rsp);
    // 设置连接索引
    rsp->conidx  = app_env[param->conidx].conidx;
    // 设置属性索引
    rsp->att_idx = param->att_idx;
    // 设置长度为0
    rsp->length  = 0;
    // 设置状态为无错误
    rsp->status  = ATT_ERR_NO_ERROR;

    // 发送响应消息
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

