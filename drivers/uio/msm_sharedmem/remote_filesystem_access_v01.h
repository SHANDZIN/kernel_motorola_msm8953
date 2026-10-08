/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2014-2015, 2017, The Linux Foundation. All rights reserved. */
#ifndef __REMOTE_FILESYSTEM_ACCESS_V01_H__
#define __REMOTE_FILESYSTEM_ACCESS_V01_H__

#include <linux/soc/qcom/qmi.h>

#define RFSA_SERVICE_ID_V01 0x1C
#define RFSA_SERVICE_VERS_V01 0x01

#define QMI_RFSA_GET_BUFF_ADDR_REQ_MSG_V01 0x0023
#define QMI_RFSA_GET_BUFF_ADDR_RESP_MSG_V01 0x0023

#define RFSA_GET_BUFF_ADDR_REQ_MSG_MAX_LEN_V01 14
#define RFSA_GET_BUFF_ADDR_RESP_MSG_MAX_LEN_V01 18

extern struct qmi_elem_info rfsa_get_buff_addr_req_msg_v01_ei[];
extern struct qmi_elem_info rfsa_get_buff_addr_resp_msg_v01_ei[];

struct rfsa_get_buff_addr_req_msg_v01 {
	u32 client_id;
	u32 size;
};

struct rfsa_get_buff_addr_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	u8 address_valid;
	u64 address;
};

#endif /* __REMOTE_FILESYSTEM_ACCESS_V01_H__ */
