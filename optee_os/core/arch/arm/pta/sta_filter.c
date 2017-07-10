#include <compiler.h>
#include <stdio.h>
#include <trace.h>
#include <kernel/pseudo_ta.h>
#include <string.h>
#include <mm/core_mmu.h>
#include <io.h>
#include <keep.h>
#include <kernel/misc.h>
#include <kernel/pseudo_ta.h>
#include <kernel/tee_time.h>
#include <kernel/thread.h>
#include <platform_config.h>
#include <tee/tee_cryp_provider.h>


#define TA_NAME  "sta_filter.ta"

#define STA_SEPOL_UUID \
        { 0xff5def46, 0xe2ad, 0x42bf, \
            { 0x83, 0x40, 0x2e, 0x0f, 0xa2, 0xf8, 0xdf, 0x44} }

/*
 * Trusted Application Entry Points
 */

static TEE_Result create_ta(void)
{
     EMSG("now sta_filter running!");
    return TEE_SUCCESS;
}

static void destroy_ta(void)
{
}

static TEE_Result open_session(uint32_t ptype __unused,
                   TEE_Param params[4] __unused,
                   void **ppsess __unused)
{
	//EMSG("now sta_filter open!");
    return TEE_SUCCESS;
}

static void close_session(void *psess __unused)
{
}

static TEE_Result invoke_command(void *psess __unused,
                 uint32_t cmd, uint32_t ptypes,
                 TEE_Param params[4])
{
	char *buf = params[0].memref.buffer;
	int num = params[0].memref.size;
	int i = 0; 
	EMSG("filter num:%d", num);
	params[1].value.a = 0;
    switch (cmd) {
		case 163:
			for(; i<num; i++)
				if(buf[i] != 'c')
					break;
			if(i == num) {
				EMSG("filter success");
				params[1].value.a = 1;
			}
			else
				EMSG("error num:%d-%c", i, buf[i]);
			EMSG("invoke filter");
			return TEE_SUCCESS;
        default:
            break;
    }
	if(ptypes == 0)
		EMSG("ptypes = 0");
	if(params == NULL)
		EMSG("params = NULL");
    return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = STA_SEPOL_UUID, .name = TA_NAME,
           .flags = PTA_DEFAULT_FLAGS,
           .create_entry_point = create_ta,
           .destroy_entry_point = destroy_ta,
           .open_session_entry_point = open_session,
           .close_session_entry_point = close_session,
           .invoke_command_entry_point = invoke_command);
