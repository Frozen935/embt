/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <embt/base/time.h>
#include <embt/osi/sync.h>
#include <embt/base/base.h>
#include <errno.h>
#include <embt/bluetooth/mesh.h>

#include "msg.h"
#include <embt/base/log.h>

LOG_MODULE_REGISTER(bt_mesh_msg, CONFIG_BT_MESH_ACCESS_LOG_LEVEL);

void bt_mesh_model_msg_init(struct net_buf_simple *msg, uint32_t opcode)
{
	net_buf_simple_init(msg, 0);

	switch (BT_MESH_MODEL_OP_LEN(opcode)) {
	case 1:
		net_buf_simple_add_u8(msg, opcode);
		break;
	case 2:
		net_buf_simple_add_be16(msg, opcode);
		break;
	case 3:
		net_buf_simple_add_u8(msg, ((opcode >> 16) & 0xff));
		/* Using LE for the CID since the model layer is defined as
		 * little-endian in the mesh spec and using BT_MESH_MODEL_OP_3
		 * will declare the opcode in this way.
		 */
		net_buf_simple_add_le16(msg, opcode & 0xffff);
		break;
	default:
		LOG_WRN("Unknown opcode format");
		break;
	}
}

void bt_mesh_msg_ack_ctx_clear(struct bt_mesh_msg_ack_ctx *ack)
{
	ack->op = 0U;
	ack->user_data = NULL;
	ack->dst = BT_MESH_ADDR_UNASSIGNED;
}

int bt_mesh_msg_ack_ctx_prepare(struct bt_mesh_msg_ack_ctx *ack,
				uint32_t op, uint16_t dst, void *user_data)
{
	if (ack->op) {
		LOG_WRN("Another synchronous operation pending");
		return -EBUSY;
	}

	ack->op = op;
	ack->user_data = user_data;
	ack->dst = dst;

	return 0;
}

void bt_mesh_msg_ack_ctx_init(struct bt_mesh_msg_ack_ctx *ack)
{
	bt_base_require(osi_mutex_init(&ack->lock, OS_MUTEX_NORMAL));
	bt_base_require(osi_sem_init(&ack->sem, 0, 1));
	ack->waiting = false;
	ack->cancelled = false;
}

void bt_mesh_msg_ack_ctx_reset(struct bt_mesh_msg_ack_ctx *ack)
{
	bt_base_require(osi_mutex_lock(&ack->lock, OS_TIMEOUT_FOREVER));
	if (ack->waiting) {
		ack->cancelled = true;
		bt_base_require(osi_sem_give(&ack->sem));
	} else {
		bt_base_require(osi_sem_reset(&ack->sem));
	}
	bt_base_require(osi_mutex_unlock(&ack->lock));
}

void bt_mesh_msg_ack_ctx_rx(struct bt_mesh_msg_ack_ctx *ack)
{
	bt_base_require(osi_mutex_lock(&ack->lock, OS_TIMEOUT_FOREVER));
	bt_base_require(osi_sem_give(&ack->sem));
	bt_base_require(osi_mutex_unlock(&ack->lock));
}

int bt_mesh_msg_ack_ctx_wait(struct bt_mesh_msg_ack_ctx *ack, bt_timeout_t timeout)
{
	int err;

	bt_base_require(osi_mutex_lock(&ack->lock, OS_TIMEOUT_FOREVER));
	if (ack->waiting) {
		bt_base_require(osi_mutex_unlock(&ack->lock));
		return -EBUSY;
	}
	ack->waiting = true;
	ack->cancelled = false;
	bt_base_require(osi_mutex_unlock(&ack->lock));
	err = osi_sem_take(&ack->sem, timeout);
	bt_base_require(osi_mutex_lock(&ack->lock, OS_TIMEOUT_FOREVER));
	if (ack->cancelled || err == -EAGAIN) {
		err = -ETIMEDOUT;
	}
	ack->waiting = false;
	bt_base_require(osi_sem_reset(&ack->sem));
	bt_mesh_msg_ack_ctx_clear(ack);
	bt_base_require(osi_mutex_unlock(&ack->lock));
	return err;
}

bool bt_mesh_msg_ack_ctx_match(const struct bt_mesh_msg_ack_ctx *ack,
			       uint32_t op, uint16_t addr, void **user_data)
{
	if (ack->op != op || (BT_MESH_ADDR_IS_UNICAST(ack->dst) && ack->dst != addr)) {
		return false;
	}

	if (user_data != NULL) {
		*user_data = ack->user_data;
	}

	return true;
}

int bt_mesh_msg_send(const struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
		     struct net_buf_simple *buf)
{
	if (!ctx && !model->pub) {
		return -ENOTSUP;
	}

	if (ctx) {
		return bt_mesh_model_send(model, ctx, buf, NULL, 0);
	}

	net_buf_simple_reset(model->pub->msg);
	net_buf_simple_add_mem(model->pub->msg, buf->data, buf->len);

	return bt_mesh_model_publish(model);
}

int bt_mesh_msg_ackd_send(const struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
			  struct net_buf_simple *buf, const struct bt_mesh_msg_rsp_ctx *rsp)
{
	int err;

	if (rsp) {
		err = bt_mesh_msg_ack_ctx_prepare(rsp->ack, rsp->op,
						  ctx ? ctx->addr : model->pub->addr,
						  rsp->user_data);
		if (err) {
			return err;
		}
	}

	err = bt_mesh_msg_send(model, ctx, buf);

	if (!rsp) {
		return err;
	}

	if (!err) {
		return bt_mesh_msg_ack_ctx_wait(rsp->ack, BT_MSEC(rsp->timeout));
	}

	bt_mesh_msg_ack_ctx_clear(rsp->ack);

	return err;
}
