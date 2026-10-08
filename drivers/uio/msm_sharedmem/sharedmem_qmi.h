/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __SHAREDMEM_QMI_H__
#define __SHAREDMEM_QMI_H__

#include <linux/types.h>

struct sharemem_qmi_entry {
	const char *client_name;
	u32 client_id;
	u64 address;
	u32 size;
	bool is_addr_dynamic;
};

int sharedmem_qmi_init(void);
void sharedmem_qmi_exit(void);
int sharedmem_qmi_add_entry(const struct sharemem_qmi_entry *entry);
void sharedmem_qmi_remove_entry(u32 client_id);

#endif /* __SHAREDMEM_QMI_H__ */
