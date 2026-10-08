// SPDX-License-Identifier: GPL-2.0-only
/*
 * RFSA shared-buffer service from the 4.9 msm_sharedmem driver,
 * adapted to the QRTR QMI helpers used by the 4.19 kernel.
 * Copyright (c) 2014-2015, 2017, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "msm_sharedmem: " fmt

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/workqueue.h>

#include "remote_filesystem_access_v01.h"
#include "sharedmem_qmi.h"

#define RFSA_SERVICE_INSTANCE_NUM 1
#define SHARED_ADDR_ENTRY_NAME_MAX_LEN 10

struct shared_addr_entry {
	struct list_head node;
	u32 id;
	u64 address;
	u32 size;
	u64 request_count;
	bool is_addr_dynamic;
	char name[SHARED_ADDR_ENTRY_NAME_MAX_LEN + 1];
};

static LIST_HEAD(sharedmem_entries);
static DEFINE_MUTEX(sharedmem_lock);
static struct qmi_handle sharedmem_qmi;
static bool sharedmem_qmi_ready;
static struct dentry *sharedmem_debugfs;
static u32 rfsa_count;
static u32 rmts_count;

int sharedmem_qmi_add_entry(const struct sharemem_qmi_entry *entry)
{
	struct shared_addr_entry *item, *existing;

	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item)
		return -ENOMEM;

	strscpy(item->name, entry->client_name, sizeof(item->name));
	item->id = entry->client_id;
	item->address = entry->address;
	item->size = entry->size;
	item->is_addr_dynamic = entry->is_addr_dynamic;

	mutex_lock(&sharedmem_lock);
	list_for_each_entry(existing, &sharedmem_entries, node) {
		if (existing->id == item->id) {
			mutex_unlock(&sharedmem_lock);
			kfree(item);
			return -EEXIST;
		}
	}
	list_add_tail(&item->node, &sharedmem_entries);
	mutex_unlock(&sharedmem_lock);

	return 0;
}

void sharedmem_qmi_remove_entry(u32 client_id)
{
	struct shared_addr_entry *item, *tmp;

	mutex_lock(&sharedmem_lock);
	list_for_each_entry_safe(item, tmp, &sharedmem_entries, node) {
		if (item->id == client_id) {
			list_del(&item->node);
			kfree(item);
			break;
		}
	}
	mutex_unlock(&sharedmem_lock);
}

static u16 get_buffer_for_client(u32 id, u32 size, u64 *address)
{
	struct shared_addr_entry *item;
	u16 error = QMI_ERR_INVALID_ID_V01;

	if (!size)
		return QMI_ERR_NO_MEMORY_V01;

	mutex_lock(&sharedmem_lock);
	list_for_each_entry(item, &sharedmem_entries, node) {
		if (item->id != id)
			continue;
		if (size > item->size) {
			error = QMI_ERR_NO_MEMORY_V01;
		} else if (!item->address) {
			error = QMI_ERR_INTERNAL_V01;
		} else {
			*address = item->address;
			item->request_count++;
			error = QMI_ERR_NONE_V01;
		}
		break;
	}
	mutex_unlock(&sharedmem_lock);

	return error;
}

static void sharedmem_qmi_get_buffer(struct qmi_handle *qmi,
		struct sockaddr_qrtr *sq, struct qmi_txn *txn,
		const void *decoded_msg)
{
	const struct rfsa_get_buff_addr_req_msg_v01 *req = decoded_msg;
	struct rfsa_get_buff_addr_resp_msg_v01 resp = {};
	int ret;

	resp.resp.error = get_buffer_for_client(req->client_id, req->size,
					      &resp.address);
	if (resp.resp.error == QMI_ERR_NONE_V01) {
		resp.resp.result = QMI_RESULT_SUCCESS_V01;
		resp.address_valid = 1;
	} else {
		resp.resp.result = QMI_RESULT_FAILURE_V01;
	}

	pr_info_ratelimited("RFSA GET_BUFF client=%u size=%u error=%u\n",
			    req->client_id, req->size, resp.resp.error);
	ret = qmi_send_response(qmi, sq, txn,
			       QMI_RFSA_GET_BUFF_ADDR_RESP_MSG_V01,
			       RFSA_GET_BUFF_ADDR_RESP_MSG_MAX_LEN_V01,
			       rfsa_get_buff_addr_resp_msg_v01_ei, &resp);
	if (ret < 0)
		pr_err_ratelimited("RFSA response failed: %d\n", ret);
}

static const struct qmi_msg_handler sharedmem_qmi_handlers[] = {
	{
		.type = QMI_REQUEST,
		.msg_id = QMI_RFSA_GET_BUFF_ADDR_REQ_MSG_V01,
		.ei = rfsa_get_buff_addr_req_msg_v01_ei,
		.decoded_size = sizeof(struct rfsa_get_buff_addr_req_msg_v01),
		.fn = sharedmem_qmi_get_buffer,
	},
	{},
};

static int sharedmem_debug_show(struct seq_file *s, void *unused)
{
	struct shared_addr_entry *item;

	mutex_lock(&sharedmem_lock);
	list_for_each_entry(item, &sharedmem_entries, node) {
		seq_printf(s, "Client_name: %s\nClient_id: 0x%08X\n",
			   item->name, item->id);
		seq_printf(s, "Buffer Size: 0x%08X (%u)\n",
			   item->size, item->size);
		seq_printf(s, "Address: 0x%016llX\nAddress Allocation: %s\n",
			   item->address, item->is_addr_dynamic ?
			   "Dynamic" : "Static");
		seq_printf(s, "Request count: %llu\n\n", item->request_count);
	}
	seq_printf(s, "RFSA server start count = %u\n", rfsa_count);
	seq_printf(s, "RMTS server start count = %u\n", rmts_count);
	mutex_unlock(&sharedmem_lock);

	return 0;
}

static int sharedmem_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sharedmem_debug_show, NULL);
}

static const struct file_operations sharedmem_debug_ops = {
	.owner = THIS_MODULE,
	.open = sharedmem_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int server_increment(void *data, u64 value)
{
	u32 *count = data;

	mutex_lock(&sharedmem_lock);
	if (*count != ~0U)
		(*count)++;
	mutex_unlock(&sharedmem_lock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(server_count_ops, NULL, server_increment, "%llu\n");

static void sharedmem_qmi_init_worker(struct work_struct *work)
{
	int ret;

	ret = qmi_handle_init(&sharedmem_qmi,
			      RFSA_GET_BUFF_ADDR_REQ_MSG_MAX_LEN_V01,
			      NULL, sharedmem_qmi_handlers);
	if (ret < 0) {
		pr_err("RFSA QMI handle initialization failed: %d\n", ret);
		return;
	}

	ret = qmi_add_server(&sharedmem_qmi, RFSA_SERVICE_ID_V01,
			     RFSA_SERVICE_VERS_V01, RFSA_SERVICE_INSTANCE_NUM);
	if (ret < 0) {
		pr_err("RFSA QMI service registration failed: %d\n", ret);
		qmi_handle_release(&sharedmem_qmi);
		return;
	}

	sharedmem_qmi_ready = true;
	pr_info("RFSA service registered: id=0x%x version=%u instance=%u\n",
		RFSA_SERVICE_ID_V01, RFSA_SERVICE_VERS_V01,
		RFSA_SERVICE_INSTANCE_NUM);
}

static DECLARE_WORK(sharedmem_qmi_init_work, sharedmem_qmi_init_worker);

int sharedmem_qmi_init(void)
{
	sharedmem_debugfs = debugfs_create_dir("rmt_storage", NULL);
	if (!IS_ERR_OR_NULL(sharedmem_debugfs)) {
		debugfs_create_file("info", 0400, sharedmem_debugfs, NULL,
				    &sharedmem_debug_ops);
		debugfs_create_file("rfsa", 0200, sharedmem_debugfs, &rfsa_count,
				    &server_count_ops);
		debugfs_create_file("rmts", 0200, sharedmem_debugfs, &rmts_count,
				    &server_count_ops);
	}

	schedule_work(&sharedmem_qmi_init_work);
	return 0;
}

void sharedmem_qmi_exit(void)
{
	struct shared_addr_entry *item, *tmp;

	cancel_work_sync(&sharedmem_qmi_init_work);
	if (sharedmem_qmi_ready) {
		qmi_handle_release(&sharedmem_qmi);
		sharedmem_qmi_ready = false;
	}
	if (!IS_ERR_OR_NULL(sharedmem_debugfs))
		debugfs_remove_recursive(sharedmem_debugfs);

	mutex_lock(&sharedmem_lock);
	list_for_each_entry_safe(item, tmp, &sharedmem_entries, node) {
		list_del(&item->node);
		kfree(item);
	}
	mutex_unlock(&sharedmem_lock);
}
