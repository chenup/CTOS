#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/fs.h>
#include <linux/err.h>
#include <linux/printk.h>
#include <linux/tee_client_api.h>
#include "../tee_private.h"

#define STA_FILTER_UUID \
        { 0xff5def46, 0xe2ad, 0x42bf, \
            { 0x83, 0x40, 0x2e, 0x0f, 0xa2, 0xf8, 0xdf, 0x44 } }

extern struct tee_device *global_device;
TEEC_Session global_session;

bool se_filter(char* buf, uint32_t size)
{
	TEEC_Operation op;
	TEEC_Result res;
	uint32_t org;

	memset(&op, 0, sizeof(op));
	op.params[0].tmpref.buffer = buf;
	op.params[0].tmpref.size = size;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE);
	res = TEEC_InvokeCommand(&global_session, 163, &op, &org);
	if(res==TEEC_SUCCESS && op.params[1].value.a) {
		printk("filter SUC\n");
		return true;
	}
	return false;
}

void tee_filter(struct tee_device *tee_dev)
{
	TEEC_Result res;
	uint32_t ret_orig;
	TEEC_UUID uuid = STA_FILTER_UUID;
	char buf[2000];
	int i=0;
	memset(buf, 'c', 2000);
	//buf[2] = 0;

	if(tee_dev == NULL)
		printk("tee_dev = NULL");
	global_device = tee_dev;
	res = TEEC_InitializeContext();
	if(res != TEEC_SUCCESS)
		printk("initialize ctx error!");
	res = TEEC_OpenSession(&global_session, &uuid,
				TEEC_LOGIN_PUBLIC, NULL, NULL, &ret_orig);
	if(res != TEEC_SUCCESS)
		printk("open session error!");
	se_filter(buf, 2000);
	printk("\nsnow tee filter\n");
}
