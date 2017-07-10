#ifndef _TEE_CORE_EXTERN_H_
#define _TEE_CORE_EXTERN_H_

#include <linux/tee_drv.h>
#include <linux/tee.h>
#include "tee_private.h"
//#include <linux/tee_private_cp.h>

extern struct tee_context *teedev_open_cp(struct tee_device *teedev);

extern int tee_ioctl_shm_alloc_cp(struct tee_context *ctx,
			       struct tee_ioctl_shm_alloc_data __user *udata, struct tee_shm *shm_out);

extern int tee_ioctl_open_session_cp(struct tee_context *ctx,
				  struct tee_ioctl_buf_data __user *ubuf);

extern int tee_ioctl_close_session_cp(struct tee_context *ctx,
			struct tee_ioctl_close_session_arg __user *uarg);

extern int tee_ioctl_cancel_cp(struct tee_context *ctx,
			    struct tee_ioctl_cancel_arg __user *uarg);

extern int tee_ioctl_shm_register_fd(struct tee_context *ctx,
			struct tee_ioctl_shm_register_fd_data __user *udata);

extern int tee_ioctl_invoke_cp(struct tee_context *ctx,
			    struct tee_ioctl_buf_data __user *ubuf);

extern int tee_ioctl_supp_recv_cp(struct tee_context *ctx,
			       struct tee_ioctl_buf_data __user *ubuf);

extern int tee_ioctl_supp_send_cp(struct tee_context *ctx,
			       struct tee_ioctl_buf_data __user *ubuf);

#endif
