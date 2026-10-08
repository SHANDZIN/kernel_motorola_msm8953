// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2013 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/soc/qcom/smem.h>

#include "mmi-unit-info.h"

#define SMEM_KERNEL_RESERVE 134
#define SMEM_KERNEL_RESERVE_SIZE 1024

static char serialno[BARCODE_MAX_LEN];
static char carrier[CARRIER_MAX_LEN];
static char device[DEVICE_MAX_LEN];
static char radio_str[RADIO_MAX_LEN];
static u32 radio;

static int __init setup_serialno(char *s)
{
	strscpy(serialno, s, sizeof(serialno));
	return 1;
}
__setup("androidboot.serialno=", setup_serialno);

static int __init setup_carrier(char *s)
{
	strscpy(carrier, s, sizeof(carrier));
	return 1;
}
__setup("androidboot.carrier=", setup_carrier);

static int __init setup_device(char *s)
{
	strscpy(device, s, sizeof(device));
	return 1;
}
__setup("androidboot.device=", setup_device);

static int __init setup_radio(char *s)
{
	if (kstrtou32(s, 16, &radio))
		radio = 0;
	strscpy(radio_str, s, sizeof(radio_str));
	return 1;
}
__setup("androidboot.radio=", setup_radio);

int mmi_unit_info_publish(void)
{
	struct device_node *chosen;
	struct mmi_unit_info *info;
	const char *baseband;
	void *smem;
	size_t size;
	int ret;

	BUILD_BUG_ON(sizeof(struct mmi_unit_info) != 356);

	chosen = of_find_node_by_path("/chosen");
	if (!chosen)
		return -ENODEV;

	info = kzalloc(SMEM_KERNEL_RESERVE_SIZE, GFP_KERNEL);
	if (!info) {
		of_node_put(chosen);
		return -ENOMEM;
	}

	info->version = MMI_UNIT_INFO_VER;
	of_property_read_u32(chosen, "linux,hwrev", &info->system_rev);
	of_property_read_u32(chosen, "linux,seriallow",
			     &info->system_serial_low);
	of_property_read_u32(chosen, "linux,serialhigh",
			     &info->system_serial_high);
	info->powerup_reason = U32_MAX;
	of_property_read_u32(chosen, "mmi,powerup_reason",
			     &info->powerup_reason);
	if (!of_property_read_string(chosen, "mmi,baseband", &baseband))
		strscpy(info->baseband, baseband, sizeof(info->baseband));
	of_node_put(chosen);

	strscpy(info->barcode, serialno, sizeof(info->barcode));
	strscpy(info->carrier, carrier, sizeof(info->carrier));
	strscpy(info->device, device, sizeof(info->device));
	info->radio = radio;
	strscpy(info->radio_str, radio_str, sizeof(info->radio_str));

	ret = qcom_smem_alloc(QCOM_SMEM_HOST_ANY, SMEM_KERNEL_RESERVE,
			      SMEM_KERNEL_RESERVE_SIZE);
	if (ret && ret != -EEXIST)
		goto out;

	smem = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_KERNEL_RESERVE, &size);
	if (IS_ERR(smem)) {
		ret = PTR_ERR(smem);
		goto out;
	}
	if (size < SMEM_KERNEL_RESERVE_SIZE) {
		ret = -EINVAL;
		goto out;
	}

	memcpy_toio(smem, info, SMEM_KERNEL_RESERVE_SIZE);
	/* Publish unit info before PIL releases the modem from reset. */
	wmb();
	pr_info("mmi_unit_info: version=%u device=%s revision=0x%x radio=%s\n",
		info->version, info->device, info->system_rev, info->radio_str);
	ret = 0;
out:
	if (ret)
		pr_err("mmi_unit_info: cannot publish modem unit info: %d\n", ret);
	kfree(info);
	return ret;
}
EXPORT_SYMBOL_GPL(mmi_unit_info_publish);
