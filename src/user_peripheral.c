/**
 ****************************************************************************************
 *
 * @file user_peripheral.c
 *
 * @brief Peripheral project source code.
 *
 * Copyright (C) 2015-2023 Renesas Electronics Corporation and/or its affiliates.
 * All rights reserved. Confidential Information.
 *
 * This software ("Software") is supplied by Renesas Electronics Corporation and/or its
 * affiliates ("Renesas"). Renesas grants you a personal, non-exclusive, non-transferable,
 * revocable, non-sub-licensable right and license to use the Software, solely if used in
 * or together with Renesas products. You may make copies of this Software, provided this
 * copyright notice and disclaimer ("Notice") is included in all such copies. Renesas
 * reserves the right to change or discontinue the Software at any time without notice.
 *
 * THE SOFTWARE IS PROVIDED "AS IS". RENESAS DISCLAIMS ALL WARRANTIES OF ANY KIND,
 * WHETHER EXPRESS, IMPLIED, OR STATUTORY, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. TO THE
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

/**
 ****************************************************************************************
 * @addtogroup APP
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "rwip_config.h"             // SW configuration
#include "gattc_task.h"
#include "gap.h"
#include "app_easy_timer.h"
#include "user_peripheral.h"
#include "user_custs1_impl.h"
#include "user_custs1_def.h"
#include "co_bt.h"
#include "hw_otpc.h"

#include "epd.h"

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

// Manufacturer Specific Data ADV structure type
struct mnf_specific_data_ad_structure
{
    uint8_t ad_structure_size;
    uint8_t ad_structure_type;
    uint8_t company_id[APP_AD_MSD_COMPANY_ID_LEN];
    uint8_t proprietary_data[APP_AD_MSD_DATA_LEN];
};

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

uint8_t app_connection_idx                      __SECTION_ZERO("retention_mem_area0");
timer_hnd app_clock_timer_used                  __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
timer_hnd app_param_update_request_timer_used   __SECTION_ZERO("retention_mem_area0");

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
*/


/**
 ****************************************************************************************
 * @brief Add an AD structure in the Advertising or Scan Response Data of the
 *        GAPM_START_ADVERTISE_CMD parameter struct.
 * @param[in] cmd               GAPM_START_ADVERTISE_CMD parameter struct
 * @param[in] ad_struct_data    AD structure buffer
 * @param[in] ad_struct_len     AD structure length
 * @param[in] adv_connectable   Connectable advertising event or not. It controls whether
 *                              the advertising data use the full 31 bytes length or only
 *                              28 bytes (Document CCSv6 - Part 1.3 Flags).
 ****************************************************************************************
 */
static void app_add_ad_struct(struct gapm_start_advertise_cmd *cmd, void *ad_struct_data, uint8_t ad_struct_len, uint8_t adv_connectable)
{
    uint8_t adv_data_max_size = (adv_connectable) ? (ADV_DATA_LEN - 3) : (ADV_DATA_LEN);

    if ((adv_data_max_size - cmd->info.host.adv_data_len) >= ad_struct_len)
    {
        // Append manufacturer data to advertising data
        memcpy(&cmd->info.host.adv_data[cmd->info.host.adv_data_len], ad_struct_data, ad_struct_len);
        // Update Advertising Data Length
        cmd->info.host.adv_data_len += ad_struct_len;

    }
    else if ((SCAN_RSP_DATA_LEN - cmd->info.host.scan_rsp_data_len) >= ad_struct_len)
    {
        // Append manufacturer data to scan response data
        memcpy(&cmd->info.host.scan_rsp_data[cmd->info.host.scan_rsp_data_len], ad_struct_data, ad_struct_len);
        // Update Scan Response Data Length
        cmd->info.host.scan_rsp_data_len += ad_struct_len;

    }
    else
    {
        // Manufacturer Specific Data do not fit in either Advertising Data or Scan Response Data
        ASSERT_WARNING(0);
    }
}


/**
 ****************************************************************************************
 * @brief Parameter update request timer callback function.
 ****************************************************************************************
*/
static void param_update_request_timer_cb()
{
    app_easy_gap_param_update_start(app_connection_idx);
    app_param_update_request_timer_used = EASY_TIMER_INVALID_TIMER;
}

static int adv_count;
int otp_btaddr[2];
int otp_boot;
char adv_name[20];

static void read_otp_value(void)
{
	hw_otpc_init();
	hw_otpc_manual_read_on(false);
	
	otp_boot = *(u32*)(0x07f8fe00);
	otp_btaddr[0] = *(u32*)(0x07f8ffa8);
	otp_btaddr[1] = *(u32*)(0x07f8ffac);
	
	hw_otpc_disable();

	u32 ba0 = otp_btaddr[0];
	u32 ba1 = otp_btaddr[1];

	ba1 = (ba1<<8)|(ba0>>24);
	ba0 &= 0x00ffffff;
	ba0 ^= ba1;

	u8 *ba = (u8*)&ba0;
	sprintf(adv_name+2, "DLG-CLOCK-%02x%02x%02x", ba[2], ba[1], ba[0]);
	int name_len = strlen(adv_name+2);
	
	if(device_info.dev_name.length==0){
		device_info.dev_name.length = name_len;
		memcpy(device_info.dev_name.name, adv_name+2, name_len);
	}

	adv_name[0] = name_len+1;
	adv_name[1] = GAP_AD_TYPE_COMPLETE_NAME;
}

extern int Region$$Table$$Base;

void user_app_init(void)
{
	read_otp_value();

	printk("\n\nuser_app_init! %s\n", __TIME__);
    app_param_update_request_timer_used = EASY_TIMER_INVALID_TIMER;
	app_clock_timer_used = EASY_TIMER_INVALID_TIMER;

	adv_count = 0;
	fspi_config(0x00030605);
	epd_hw_init(0x23200700, 0x05210006, 104, 212, EPD_BWR|ROTATE_3);  // for 2.13 board BW

	selflash(otp_boot);

    default_app_on_init();
}

void user_app_before_sleep(void)
{
	epd_hw_close();
}


void epd_test(void);
void epd_test2(void);


static void app_clock_timer_cb(void)
{
	app_clock_timer_used = app_easy_timer(1000, app_clock_timer_cb);

	int stat = clock_update(10);
	clock_print();

	if(stat>0){
		int full = (stat>1)? 1:0;
		clock_draw(full);
	}

	if(adv_count<20){
		if(adv_count>=12){
			adv_count = 0;
			user_app_adv_start();
		}else{
			adv_count += 1;
		}
	}

}


void user_app_on_db_init_complete( void )
{
	app_clock_timer_used = app_easy_timer(1000, app_clock_timer_cb);
	printk("\nuser_app_on_db_init_complete! %08x\n", app_clock_timer_used);

	int adcval = adc1_update();
	printk("Voltage: %d\n", adcval);

	clock_print();
	clock_push();

	clock_draw(1);

	user_app_adv_start();
}

void user_app_adv_start(void)
{
    struct gapm_start_advertise_cmd* cmd = app_easy_gap_undirected_advertise_get_active();
	app_add_ad_struct(cmd, adv_name, adv_name[0]+1, 1);

	//default_advertise_operation();
    //app_easy_gap_undirected_advertise_start();
	app_easy_gap_undirected_advertise_with_timeout_start(user_default_hnd_conf.advertise_period, NULL);
	printk("\nuser_app_adv_start! %s\n", adv_name+2);
}


void user_app_connection(uint8_t connection_idx, struct gapc_connection_req_ind const *param)
{
	printk("user_app_connection: %d\n", connection_idx);

    if (app_env[connection_idx].conidx != GAP_INVALID_CONIDX)
    {
		adv_count = 20;
        app_connection_idx = connection_idx;

		printk("  interval: %d\n", param->con_interval);
		printk("  latency : %d\n", param->con_latency);
		printk("  sup_to  : %d\n", param->sup_to);
        // Check if the parameters of the established connection are the preferred ones.
        // If not then schedule a connection parameter update request.
        if ((param->con_interval < user_connection_param_conf.intv_min) ||
            (param->con_interval > user_connection_param_conf.intv_max) ||
            (param->con_latency != user_connection_param_conf.latency) ||
            (param->sup_to != user_connection_param_conf.time_out))
        {
            // Connection params are not these that we expect
            app_param_update_request_timer_used = app_easy_timer(APP_PARAM_UPDATE_REQUEST_TO, param_update_request_timer_cb);
        }
		
		clock_push();
    } else {
        // No connection has been established, restart advertising
        //user_app_adv_start();
		adv_count = 0;
    }

    default_app_on_connection(connection_idx, param);
}

void user_app_adv_undirect_complete(uint8_t status)
{
	printk("user_app_adv_undirect_complete: %02x\n", status);
}


void user_app_disconnect(struct gapc_disconnect_ind const *param)
{
	printk("user_app_disconnect! reason=%02x\n", param->reason);

    // Cancel the parameter update request timer
    if (app_param_update_request_timer_used != EASY_TIMER_INVALID_TIMER)
    {
        app_easy_timer_cancel(app_param_update_request_timer_used);
        app_param_update_request_timer_used = EASY_TIMER_INVALID_TIMER;
    }

	if(param->reason!=CO_ERROR_REMOTE_USER_TERM_CON){
		// 非主动断开连接时, 重新广播.
		user_app_adv_start();
	}

	adv_count = 0;
}


void user_catch_rest_hndl(ke_msg_id_t const msgid,
                          void const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
    switch(msgid)
    {
        case CUSTS1_VAL_WRITE_IND:
        {
			/* 写特征值通知. 值已经写入Database中了. */
			printk("CUSTS1_VAL_WRITE_IND!\n");
            struct custs1_val_write_ind const *msg_param = (struct custs1_val_write_ind const *)(param);

            switch (msg_param->handle)
            {
                case SVC1_IDX_CONTROL_POINT_VAL:
                    user_svc1_ctrl_wr_ind_handler(msgid, msg_param, dest_id, src_id);
                    break;

                case SVC1_IDX_LONG_VALUE_VAL:
                    user_svc1_long_val_wr_ind_handler(msgid, msg_param, dest_id, src_id);
                    break;

                default:
                    break;
            }
        } break;

        case CUSTS1_VAL_NTF_CFM:
        {
			/* Notification确认. 请求已经发出. */
        } break;

        case CUSTS1_VAL_IND_CFM:
        {
			/* Indication确认. 请求已经发出. */
        } break;

        case CUSTS1_ATT_INFO_REQ:
        {
			/* 读ATT_INFO请求. 需要返回数据. */
			printk("CUSTS1_ATT_INFO_REQ!\n");
            struct custs1_att_info_req const *msg_param = (struct custs1_att_info_req const *)param;

            switch (msg_param->att_idx)
            {
                case SVC1_IDX_LONG_VALUE_VAL:
                    user_svc1_long_val_att_info_req_handler(msgid, msg_param, dest_id, src_id);
                    break;

                default:
                    user_svc1_rest_att_info_req_handler(msgid, msg_param, dest_id, src_id);
                    break;
             }
        } break;

        case GAPC_PARAM_UPDATED_IND:
        {
            // Cast the "param" pointer to the appropriate message structure
            struct gapc_param_updated_ind const *msg_param = (struct gapc_param_updated_ind const *)(param);
			printk("GAPC_PARAM_UPDATED_IND!\n");
			printk("  interval: %d\n", msg_param->con_interval);
			printk("  latency : %d\n", msg_param->con_latency);
			printk("  sup_to  : %d\n", msg_param->sup_to);

            // Check if updated Conn Params filled to preferred ones
            if ((msg_param->con_interval >= user_connection_param_conf.intv_min) &&
                (msg_param->con_interval <= user_connection_param_conf.intv_max) &&
                (msg_param->con_latency == user_connection_param_conf.latency) &&
                (msg_param->sup_to == user_connection_param_conf.time_out))
            {
				printk("  match!\n");
            }
        } break;

        case CUSTS1_VALUE_REQ_IND:
        {
			/* 读特征值. 什么时候会有这个事件? */
			printk("CUSTS1_VALUE_REQ_IND!\n");
            struct custs1_value_req_ind const *msg_param = (struct custs1_value_req_ind const *) param;

            switch (msg_param->att_idx)
            {
                default:
                {
                    // Send Error message
                    struct custs1_value_req_rsp *rsp = KE_MSG_ALLOC(CUSTS1_VALUE_REQ_RSP,
                                                                    src_id,
                                                                    dest_id,
                                                                    custs1_value_req_rsp);

                    // Provide the connection index.
                    rsp->conidx  = app_env[msg_param->conidx].conidx;
                    // Provide the attribute index.
                    rsp->att_idx = msg_param->att_idx;
                    // Force current length to zero.
                    rsp->length = 0;
                    // Set Error status
                    rsp->status  = ATT_ERR_APP_ERROR;
                    // Send message
                    KE_MSG_SEND(rsp);
                } break;
             }
        } break;

        case GATTC_EVENT_REQ_IND:
        {
            // Confirm unhandled indication to avoid GATT timeout
            struct gattc_event_ind const *ind = (struct gattc_event_ind const *) param;
            struct gattc_event_cfm *cfm = KE_MSG_ALLOC(GATTC_EVENT_CFM, src_id, dest_id, gattc_event_cfm);
            cfm->handle = ind->handle;
            KE_MSG_SEND(cfm);
        } break;
		
		case GATTC_MTU_CHANGED_IND:
		{
			struct gattc_mtu_changed_ind *ind = (struct gattc_mtu_changed_ind *) param;
			printk("GATTC_MTU_CHANGED_IND: %d\n", ind->mtu);
		} break;

        default:
		{
			printk("Unhandled msgid=%08x\n", msgid);
		} break;
    }
}

/// @} APP
