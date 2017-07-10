/*
 * Copyright (c) 2015-2016, Linaro Limited
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/tee_drv.h>
#include <linux/uaccess.h>
#include "tee_private.h"
//#include <linux/tee_private_cp.h>

#define TEE_IOCTL_PARAM_SIZE(x) (sizeof(struct tee_param) * (x))

/*
 * Unprivileged devices in the lower half range and privileged devices in
 * the upper half range.
 */

static struct class *tee_class;

struct tee_context *teedev_open_cp(struct tee_device *teedev)
{
	int rc;
	struct tee_context *ctx;

	if (!tee_device_get(teedev))
		return ERR_PTR(-EINVAL);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		rc = -ENOMEM;
		goto err;
	}

	ctx->teedev = teedev;
	INIT_LIST_HEAD(&ctx->list_shm);
	rc = teedev->desc->ops->open(ctx);
	if (rc)
		goto err;

	return ctx;
err:
	kfree(ctx);
	tee_device_put(teedev);
	return ERR_PTR(rc);

}

static void teedev_close_context(struct tee_context *ctx)
{
	struct tee_shm *shm;

	ctx->teedev->desc->ops->release(ctx);
	mutex_lock(&ctx->teedev->mutex);
	list_for_each_entry(shm, &ctx->list_shm, link)
		shm->ctx = NULL;
	mutex_unlock(&ctx->teedev->mutex);
	tee_device_put(ctx->teedev);
	kfree(ctx);
}

int tee_ioctl_shm_alloc_cp(struct tee_context *ctx,
			       struct tee_ioctl_shm_alloc_data __user *udata, struct tee_shm *shm_out)
{
	long ret;
	struct tee_ioctl_shm_alloc_data data;
	struct tee_shm *shm;

	if (copy_from_user(&data, udata, sizeof(data)))
		return -EFAULT;

	/* Currently no input flags are supported */
	if (data.flags)
		return -EINVAL;

	data.id = -1;

	shm = tee_shm_alloc(ctx, data.size, TEE_SHM_MAPPED | TEE_SHM_DMA_BUF);
	if (IS_ERR(shm))
		return PTR_ERR(shm);

	data.id = shm->id;
	data.flags = shm->flags;
	data.size = shm->size;

	shm_out->kaddr = shm->kaddr;

	if (copy_to_user(udata, &data, sizeof(data)))
		ret = -EFAULT;
	else
		ret = tee_shm_get_fd(shm);

	/*
	 * When user space closes the file descriptor the shared memory
	 * should be freed or if tee_shm_get_fd() failed then it will
	 * be freed immediately.
	 */
	tee_shm_put(shm);

	return ret;
}

int tee_ioctl_shm_register_fd(struct tee_context *ctx,
			struct tee_ioctl_shm_register_fd_data __user *udata)
{
	struct tee_ioctl_shm_register_fd_data data;
	struct tee_shm *shm;
	long ret;

	if (copy_from_user(&data, udata, sizeof(data)))
		return -EFAULT;

	/* Currently no input flags are supported */
	if (data.flags)
		return -EINVAL;

	shm = tee_shm_register_fd(ctx, data.fd);
	if (IS_ERR_OR_NULL(shm))
		return -EINVAL;

	data.id = shm->id;
	data.flags = shm->flags;
	data.size = shm->size;

	if (copy_to_user(udata, &data, sizeof(data)))
		ret = -EFAULT;
	else
		ret = tee_shm_get_fd(shm);

	/*
	 * When user space closes the file descriptor the shared memory
	 * should be freed or if tee_shm_get_fd() failed then it will
	 * be freed immediately.
	 */
	tee_shm_put(shm);
	return ret;
}

static int params_from_user(struct tee_context *ctx, struct tee_param *params,
			    size_t num_params,
			    struct tee_ioctl_param __user *uparams)
{
	size_t n;

	for (n = 0; n < num_params; n++) {
		struct tee_shm *shm;
		struct tee_ioctl_param ip;

		if (copy_from_user(&ip, uparams + n, sizeof(ip)))
			return -EFAULT;

		/* All unused attribute bits has to be zero */
		if (ip.attr & ~TEE_IOCTL_PARAM_ATTR_TYPE_MASK)
			return -EINVAL;

		params[n].attr = ip.attr;
		switch (ip.attr) {
		case TEE_IOCTL_PARAM_ATTR_TYPE_NONE:
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT:
			break;
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT:
			params[n].u.value.a = ip.a;
			params[n].u.value.b = ip.b;
			params[n].u.value.c = ip.c;
			break;
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT:
			/*
			 * If we fail to get a pointer to a shared memory
			 * object (and increase the ref count) from an
			 * identifier we return an error. All pointers that
			 * has been added in params have an increased ref
			 * count. It's the callers responibility to do
			 * tee_shm_put() on all resolved pointers.
			 */
			shm = tee_shm_get_from_id(ctx, ip.c);
			if (IS_ERR(shm))
				return PTR_ERR(shm);

			params[n].u.memref.shm_offs = ip.a;
			params[n].u.memref.size = ip.b;
			params[n].u.memref.shm = shm;
			break;
		default:
			/* Unknown attribute */
			return -EINVAL;
		}
	}
	return 0;
}

static int params_to_user(struct tee_ioctl_param __user *uparams,
			  size_t num_params, struct tee_param *params)
{
	size_t n;

	for (n = 0; n < num_params; n++) {
		struct tee_ioctl_param __user *up = uparams + n;
		struct tee_param *p = params + n;

		switch (p->attr) {
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT:
			if (put_user(p->u.value.a, &up->a) ||
			    put_user(p->u.value.b, &up->b) ||
			    put_user(p->u.value.c, &up->c))
				return -EFAULT;
			break;
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT:
			if (put_user((u64)p->u.value.b, &up->b))
				return -EFAULT;
		default:
			break;
		}
	}
	return 0;
}

static bool param_is_memref(struct tee_param *param)
{
	switch (param->attr & TEE_IOCTL_PARAM_ATTR_TYPE_MASK) {
	case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT:
	case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT:
	case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT:
		return true;
	default:
		return false;
	}
}

int tee_ioctl_open_session_cp(struct tee_context *ctx,
				  struct tee_ioctl_buf_data __user *ubuf)
{
	int rc;
	size_t n;
	struct tee_ioctl_buf_data buf;
	struct tee_ioctl_open_session_arg __user *uarg;
	struct tee_ioctl_open_session_arg arg;
	struct tee_ioctl_param __user *uparams = NULL;
	struct tee_param *params = NULL;
	bool have_session = false;

	if (!ctx->teedev->desc->ops->open_session)
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, sizeof(buf)))
		return -EFAULT;

	if (buf.buf_len > TEE_MAX_ARG_SIZE ||
	    buf.buf_len < sizeof(struct tee_ioctl_open_session_arg))
		return -EINVAL;

	uarg = (struct tee_ioctl_open_session_arg __user *)(unsigned long)
		buf.buf_ptr;
	if (copy_from_user(&arg, uarg, sizeof(arg)))
		return -EFAULT;

	if (sizeof(arg) + TEE_IOCTL_PARAM_SIZE(arg.num_params) != buf.buf_len)
		return -EINVAL;

	if (arg.num_params) {
		params = kcalloc(arg.num_params, sizeof(struct tee_param),
				 GFP_KERNEL);
		if (!params)
			return -ENOMEM;
		uparams = (struct tee_ioctl_param __user *)(uarg + 1);
		rc = params_from_user(ctx, params, arg.num_params, uparams);
		if (rc)
			goto out;
	}

	rc = ctx->teedev->desc->ops->open_session(ctx, &arg, params);
	if (rc)
		goto out;
	have_session = true;

	if (put_user(arg.session, &uarg->session) ||
	    put_user(arg.ret, &uarg->ret) ||
	    put_user(arg.ret_origin, &uarg->ret_origin)) {
		rc = -EFAULT;
		goto out;
	}
	rc = params_to_user(uparams, arg.num_params, params);
out:
	/*
	 * If we've succeeded to open the session but failed to communicate
	 * it back to user space, close the session again to avoid leakage.
	 */
	if (rc && have_session && ctx->teedev->desc->ops->close_session)
		ctx->teedev->desc->ops->close_session(ctx, arg.session);

	if (params) {
		/* Decrease ref count for all valid shared memory pointers */
		for (n = 0; n < arg.num_params; n++)
			if (param_is_memref(params + n) &&
			    params[n].u.memref.shm)
				tee_shm_put(params[n].u.memref.shm);
		kfree(params);
	}

	return rc;
}

int tee_ioctl_invoke_cp(struct tee_context *ctx,
			    struct tee_ioctl_buf_data __user *ubuf)
{
	int rc;
	size_t n;
	struct tee_ioctl_buf_data buf;
	struct tee_ioctl_invoke_arg __user *uarg;
	struct tee_ioctl_invoke_arg arg;
	struct tee_ioctl_param __user *uparams = NULL;
	struct tee_param *params = NULL;

	if (!ctx->teedev->desc->ops->invoke_func)
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, sizeof(buf)))
		return -EFAULT;

	if (buf.buf_len > TEE_MAX_ARG_SIZE ||
	    buf.buf_len < sizeof(struct tee_ioctl_invoke_arg))
		return -EINVAL;

	uarg = (struct tee_ioctl_invoke_arg __user *)(unsigned long)buf.buf_ptr;
	if (copy_from_user(&arg, uarg, sizeof(arg)))
		return -EFAULT;

	if (sizeof(arg) + TEE_IOCTL_PARAM_SIZE(arg.num_params) != buf.buf_len)
		return -EINVAL;

	if (arg.num_params) {
		params = kcalloc(arg.num_params, sizeof(struct tee_param),
				 GFP_KERNEL);
		if (!params)
			return -ENOMEM;
		uparams = (struct tee_ioctl_param __user *)(uarg + 1);
		rc = params_from_user(ctx, params, arg.num_params, uparams);
		if (rc)
			goto out;
	}

	rc = ctx->teedev->desc->ops->invoke_func(ctx, &arg, params);

	// printk("invoke_func:transfer policy data!\n");

	if (rc)
		goto out;

	if (put_user(arg.ret, &uarg->ret) ||
	    put_user(arg.ret_origin, &uarg->ret_origin)) {
		rc = -EFAULT;
		goto out;
	}
	rc = params_to_user(uparams, arg.num_params, params);
out:
	if (params) {
		/* Decrease ref count for all valid shared memory pointers */
		for (n = 0; n < arg.num_params; n++)
			if (param_is_memref(params + n) &&
			    params[n].u.memref.shm)
				tee_shm_put(params[n].u.memref.shm);
		kfree(params);
	}
	return rc;
}

int tee_ioctl_cancel_cp(struct tee_context *ctx,
			    struct tee_ioctl_cancel_arg __user *uarg)
{
	struct tee_ioctl_cancel_arg arg;

	if (!ctx->teedev->desc->ops->cancel_req)
		return -EINVAL;

	if (copy_from_user(&arg, uarg, sizeof(arg)))
		return -EFAULT;

	return ctx->teedev->desc->ops->cancel_req(ctx, arg.cancel_id,
						  arg.session);
}

int tee_ioctl_close_session_cp(struct tee_context *ctx,
			struct tee_ioctl_close_session_arg __user *uarg)
{
	struct tee_ioctl_close_session_arg arg;

	if (!ctx->teedev->desc->ops->close_session)
		return -EINVAL;

	if (copy_from_user(&arg, uarg, sizeof(arg)))
		return -EFAULT;

	return ctx->teedev->desc->ops->close_session(ctx, arg.session);
}

struct match_dev_data {
	struct tee_ioctl_version_data *vers;
	const void *data;
	int (*match)(struct tee_ioctl_version_data *, const void *);
};

static int match_dev(struct device *dev, const void *data)
{
	const struct match_dev_data *match_data = data;
	struct tee_device *teedev = container_of(dev, struct tee_device, dev);
	teedev->desc->ops->get_version(teedev, match_data->vers);
	return match_data->match(match_data->vers, match_data->data);
}

struct tee_context *tee_client_open_context_cp(struct tee_context *start,
			int (*match)(struct tee_ioctl_version_data *,
				const void *),
			const void *data, struct tee_ioctl_version_data *vers)
{
	struct device *dev = NULL;
	struct device *put_dev = NULL;
	struct tee_context *ctx = NULL;
	struct tee_ioctl_version_data v;
	struct match_dev_data match_data = { vers ? vers : &v, data, match };

	if (start)
		dev = &start->teedev->dev;

	do {
		dev = class_find_device(tee_class, dev, &match_data, match_dev);
		if (!dev) {
			ctx = ERR_PTR(-ENOENT);
			break;
		}

		put_device(put_dev);
		put_dev = dev;

		ctx = teedev_open_cp(container_of(dev, struct tee_device, dev));
	} while (IS_ERR(ctx) && PTR_ERR(ctx) != -ENOMEM);

	put_device(put_dev);
	return ctx;
}
EXPORT_SYMBOL_GPL(tee_client_open_context_cp);

void tee_client_close_context_cp(struct tee_context *ctx)
{
	teedev_close_context(ctx);
}

EXPORT_SYMBOL_GPL(tee_client_close_context_cp);

void tee_client_get_version_cp(struct tee_context *ctx,
			struct tee_ioctl_version_data *vers)
{
	ctx->teedev->desc->ops->get_version(ctx->teedev, vers);
}
EXPORT_SYMBOL_GPL(tee_client_get_version_cp);


int tee_client_open_session_cp(struct tee_context *ctx,
			struct tee_ioctl_open_session_arg *arg,
			struct tee_param *param)
{
	if (!ctx->teedev->desc->ops->open_session)
		return -EINVAL;
	return ctx->teedev->desc->ops->open_session(ctx, arg, param);
}
EXPORT_SYMBOL_GPL(tee_client_open_session_cp);

int tee_client_close_session_cp(struct tee_context *ctx, u32 session)
{
	if (!ctx->teedev->desc->ops->close_session)
		return -EINVAL;
	return ctx->teedev->desc->ops->close_session(ctx, session);
}
EXPORT_SYMBOL_GPL(tee_client_close_session_cp);

int tee_client_invoke_func_cp(struct tee_context *ctx,
			struct tee_ioctl_invoke_arg *arg,
			struct tee_param *param)
{
	if (!ctx->teedev->desc->ops->invoke_func)
		return -EINVAL;
	return ctx->teedev->desc->ops->invoke_func(ctx, arg, param);
}
EXPORT_SYMBOL_GPL(tee_client_invoke_func_cp);

