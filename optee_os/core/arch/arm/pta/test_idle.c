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
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>


#define TA_NAME  "test_idle.ta"

#define STA_TEST_IDLE_UUID \
        { 0xff5def46, 0xe2ad, 0x42bf, \
            { 0x83, 0x40, 0x2e, 0x0f, 0xa2, 0xf8, 0xdf, 0x44} }

/*
 * Trusted Application Entry Points
 */

static TEE_Result create_ta(void)
{
     EMSG("now test_idle running!");
    return TEE_SUCCESS;
}

static void destroy_ta(void)
{
}

static TEE_Result open_session(uint32_t ptype __unused,
                   TEE_Param params[4] __unused,
                   void **ppsess __unused)
{
    return TEE_SUCCESS;
}

static void close_session(void *psess __unused)
{
}

static TEE_Result invoke_command(void *psess __unused,
                 uint32_t cmd, uint32_t ptypes __unused,
                 TEE_Param params[4] __unused)
{
	uint32_t old_ctlr;
	vaddr_t gicc_base;
    switch (cmd) {
		case 163:
			EMSG("invoke test_idle");
			gicc_base = (vaddr_t)phys_to_virt(0x2c000000,MEM_AREA_IO_SEC);
			old_ctlr = read32(gicc_base + 0);
			write32(old_ctlr & 0xfffffffd, gicc_base + 0);
			EMSG("GICC_CTLR: 0x%08x", read32(gicc_base + 0));
			while(true)
			{
				asm(wfi);
				EMSG("invoke cpu");
			}
			return TEE_SUCCESS;
        default:
            break;
    }
    return TEE_ERROR_BAD_PARAMETERS;
}

pseudo_ta_register(.uuid = STA_TEST_IDLE_UUID, .name = TA_NAME,
           .flags = PTA_DEFAULT_FLAGS,
           .create_entry_point = create_ta,
           .destroy_entry_point = destroy_ta,
           .open_session_entry_point = open_session,
           .close_session_entry_point = close_session,
           .invoke_command_entry_point = invoke_command);
