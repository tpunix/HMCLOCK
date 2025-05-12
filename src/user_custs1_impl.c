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

int year=2025, month=0, date=0, wday=2;
int hour=0, minute=0, second=0;

// 增加1天
void date_inc(void)
{
	uint8_t d2m[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
	int is_leap = (year%4)? 0 : (year%100)? 1: (year%400)? 0: 1;
	d2m[1] += is_leap;

	wday += 1;
	if(wday>=7)
		wday = 0;
	
	date += 1;
	if(date==d2m[month]){
		date = 0;
		month += 1;
		if(month>=12){
			month = 0;
			year += 1;
		}
	}
}

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
			retv = 4;
		}
	}

	return retv;
}

void clock_set(uint8_t *buf)
{
	year = buf[1] + buf[2]*256;
	month  = buf[3];
	date   = buf[4]-1;
	hour   = buf[5];
	minute = buf[6];
	second = buf[7];
	wday   = buf[8]-1;
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
	printk("\n%04d-%02d-%02d %02d:%02d:%02d\n", year, month+1, date+1, hour, minute, second);
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

static void draw_batt(void)
{
	int p = batt_cal(adcval);
	p /= 10;

	draw_rect(190, 8, 202, 14, BLACK);
	draw_box(188, 10, 189, 12, BLACK);

	draw_box(201-p, 9, 201, 13, BLACK);
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


char *wday_str[] = {"一", "二", "三", "四", "五", "六", "日"};

void clock_draw(int full)
{
	char tbuf[64];

	epd_init();

	memset(fb_bw, 0xff, scr_h*line_bytes);
	memset(fb_rr, 0x00, scr_h*line_bytes);

	draw_batt();
	draw_bt(180, 11);
	
	select_font(1);
	sprintf(tbuf, "%02d:%02d", hour, minute);
	draw_text(12, 20, tbuf, BLACK);

	sprintf(tbuf, "%4d年%2d月%2d日 星期%s", year, month+1, date+1, wday_str[wday]);
	select_font(0);
	draw_text(15, 85, tbuf, BLACK);

	epd_screen_update();
	
	if(full){
		epd_update(0xf4);
	}else{
		epd_load_lut(lut_p);
		epd_cmd1(0x2c, 0x55);
		epd_update(0xc0);
		epd_update(0x04);
	}
	epd_cmd1(0x10, 0x03);
	epd_power(0);
	epd_hw_close();
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
//		clock_draw(1);
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

