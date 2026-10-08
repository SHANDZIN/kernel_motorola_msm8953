// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2014-2015, 2017, The Linux Foundation. All rights reserved. */
#include <linux/soc/qcom/qmi.h>

#include "remote_filesystem_access_v01.h"

struct qmi_elem_info rfsa_get_buff_addr_req_msg_v01_ei[] = {
	{
		.data_type   = QMI_UNSIGNED_4_BYTE,
		.elem_len    = 1,
		.elem_size   = sizeof(u32),
		.array_type  = NO_ARRAY,
		.tlv_type    = 0x01,
		.offset      = offsetof(struct rfsa_get_buff_addr_req_msg_v01,
					   client_id),
	},
	{
		.data_type   = QMI_UNSIGNED_4_BYTE,
		.elem_len    = 1,
		.elem_size   = sizeof(u32),
		.array_type  = NO_ARRAY,
		.tlv_type    = 0x02,
		.offset      = offsetof(struct rfsa_get_buff_addr_req_msg_v01,
					   size),
	},
	{
		.data_type   = QMI_EOTI,
		.array_type  = NO_ARRAY,
		.tlv_type    = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info rfsa_get_buff_addr_resp_msg_v01_ei[] = {
	{
		.data_type   = QMI_STRUCT,
		.elem_len    = 1,
		.elem_size   = sizeof(struct qmi_response_type_v01),
		.array_type  = NO_ARRAY,
		.tlv_type    = 0x02,
		.offset      = offsetof(struct rfsa_get_buff_addr_resp_msg_v01,
					   resp),
		.ei_array    = qmi_response_type_v01_ei,
	},
	{
		.data_type   = QMI_OPT_FLAG,
		.elem_len    = 1,
		.elem_size   = sizeof(u8),
		.array_type  = NO_ARRAY,
		.tlv_type    = 0x10,
		.offset      = offsetof(struct rfsa_get_buff_addr_resp_msg_v01,
					address_valid),
	},
	{
		.data_type   = QMI_UNSIGNED_8_BYTE,
		.elem_len    = 1,
		.elem_size   = sizeof(u64),
		.array_type  = NO_ARRAY,
		.tlv_type    = 0x10,
		.offset      = offsetof(struct rfsa_get_buff_addr_resp_msg_v01,
					address),
	},
	{
		.data_type   = QMI_EOTI,
		.array_type  = NO_ARRAY,
		.tlv_type    = QMI_COMMON_TLV_TYPE,
	},
};
