/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef WLAN_TELEMETRY_AGENT
#define WLAN_TELEMETRY_AGENT

#ifdef WLAN_CONFIG_TELEMETRY
#include <telemetry_agent_wifi_driver_if.h>

void wlan_telemetry_agent_application_init_notify
	(enum agent_notification_event);
QDF_STATUS wlan_telemetry_agent_init(void);
QDF_STATUS wlan_telemetry_agent_deinit(void);

/**
 * telemetry_sawf_peer_ctx_alloc - Allocate sawwf peer
 * @soc: opaque soc handle
 * @sawf_ctx: opaque sawf-peer ctx
 * @mac_addr: MAC address
 * @svc_id: service-id
 * @hostq_id: queue-id used in host
 *
 * Return: opaque pointer to telemetry-peer, NULL otherwise
 */
void *telemetry_sawf_peer_ctx_alloc(void *soc, void *sawf_ctx,
				    uint8_t *mac_addr,
				    uint8_t svc_id, uint8_t hostq_id);

/**
 * telemetry_sawf_peer_ctx_free - Free telemetry-peer
 * @telemetry_ctx: opaque telemetry ctx
 *
 * Return: none
 */
void telemetry_sawf_peer_ctx_free(void *telemetry_ctx);

/**
 * telemetry_sawf_updt_tid_msduq - Update tid and msduq-id
 * @telemetry_ctx: opaque telemetry ctx
 * @hostq_id: queue-id used in host
 * @tid: tid no
 * @msduq_idx: msdu-queue id
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS telemetry_sawf_updt_tid_msduq(void *telemetry_ctx,
					 uint8_t hostq_id,
					 uint8_t tid,
					 uint8_t msduq_idx);

/**
 * telemetry_sawf_set_mov_avg_params - Set moving average params
 * @num_pkt: no of pkts
 * @num_win: no of windows
 *
 * Return: 0 on success, -1 otherwise
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS telemetry_sawf_set_mov_avg_params(uint32_t num_pkt,
					     uint32_t num_win);

/**
 * telemetry_sawf_set_sla_params - Set sla params
 * @num_pkt: no of pkts
 * @time_sec: time in seconds
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS telemetry_sawf_set_sla_params(uint32_t num_pkt,
					 uint32_t time_sec);

/**
 * telemetry_sawf_set_sla_cfg - Set sla config
 * @svc_id: service-id
 * @min_tput_rate: minimum throuput rate
 * @max_tput_rate: maximum throuput rate
 * @burst_size: burst-size
 * @svc_intval: service interval
 * @delay_bound: delay boundary
 * @msdu_ttl: msdu TTL
 * @msdu_rate_loss: msdu loss rate
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS telemetry_sawf_set_sla_cfg(uint8_t svc_id,
				      uint8_t min_tput_rate,
				      uint8_t max_tput_rate,
				      uint8_t burst_size,
				      uint8_t svc_interval,
				      uint8_t delay_bound,
				      uint8_t msdu_ttl,
				      uint8_t msdu_rate_loss);

/**
 * telemetry_sawf_set_svclass_cfg - Set service-class config
 * @enable: flag to denote enable/disable
 * @svc_id: service-id
 * @min_tput_rate: minimum throuput rate
 * @max_tput_rate: maximum throuput rate
 * @burst_size: burst-size
 * @svc_intval: service interval
 * @delay_bound: delay boundary
 * @msdu_ttl: msdu TTL
 * @msdu_rate_loss: msdu loss rate
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS telemetry_sawf_set_svclass_cfg(bool enable, uint8_t svc_id,
					  uint32_t min_tput_rate,
					  uint32_t max_tput_rate,
					  uint32_t burst_size,
					  uint32_t svc_interval,
					  uint32_t delay_bound,
					  uint32_t msdu_ttl,
					  uint32_t msdu_rate_loss);

/**
 * telemetry_sawf_set_sla_detect_cfg - Set sla-detect config
 * @detect-type: detection-type
 * @min_tput_rate: minimum throuput rate
 * @max_tput_rate: maximum throuput rate
 * @burst_size: burst-size
 * @svc_intval: service interval
 * @delay_bound: delay boundary
 * @msdu_ttl: msdu TTL
 * @msdu_rate_loss: msdu loss rate
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS telemetry_sawf_set_sla_detect_cfg(uint8_t detect_type,
					     uint8_t min_tput_rate,
					     uint8_t max_tput_rate,
					     uint8_t burst_size,
					     uint8_t svc_intval,
					     uint8_t delay_bound,
					     uint8_t msdu_ttl,
					     uint8_t msdu_rate_loss);

/**
 * telemetry_sawf_update_delay - Update delay-stats in telemetry-agent
 * @telemetry_ctx: opaque telemetry ctx
 * @tid: tid no
 * @queue: queue-id
 * @pass: count of pkts that passed the delay-bound check
 * @fail: count of pkts that failed the delay-bound check
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS telemetry_sawf_update_delay(void *telemetry_ctx, uint8_t tid,
				       uint8_t queue, uint64_t pass,
				       uint64_t fail);

/**
 * telemetry_sawf_update_delay_mvng - Update sum of delay-stats of
 * a windows to telemetry-agent
 * @telemetry_ctx: opaque telemetry peer ctx
 * @tid: tid no
 * @queue: queue-id
 * @sum_windows: sum of delay-stats for a windows
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS telemetry_sawf_update_delay_mvng(void *telemetry_ctx,
					    uint8_t tid,
					    uint8_t queue,
					    uint64_t sum_windows);

/**
 * telemetry_sawf_update_msdu_drop - Update msdu-drop stats in
 * telemetry-agent
 * @telemetry_ctx: opaque telemetry peer ctx
 * @tid: tid no
 * @queue: queue-id
 * @success: count of pkts successfully Tx'ed
 * @failure_drop: count of pkts dropped
 * @failure_ttl: count of pkts dropped due to TTL-expiry
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS telemetry_sawf_update_msdu_drop(void *telemetry_ctx,
					   uint8_t tid, uint8_t queue,
					   uint64_t success,
					   uint64_t failure_drop,
					   uint64_t failure_ttl);

/**
 * telemetry_sawf_get_rate - Fetch rate stats from upper-layer
 * @telemetry_ctx: opaque telemetry peer ctx
 * @tid: tid no
 * @queue: queue-id
 * @rate: pointer to memory to fill rate-stats
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS telemetry_sawf_get_rate(void *telemetry_ctx, uint8_t tid,
				   uint8_t queue, uint32_t *rate);

/**
 * telemetry_sawf_get_mov_avg - Fetch moving avg stats from upper-layer
 * @telemetry_ctx: opaque telemetry peer ctx
 * @tid: tid no
 * @queue: queue-id
 * @mov_avg: pointer to memory to fill moving-average stats
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS telemetry_sawf_get_mov_avg(void *telemetry_ctx, uint8_t tid,
				      uint8_t queue, uint32_t *mov_avg);
#else
#define wlan_telemetry_agent_application_init_notify(param)

static inline
QDF_STATUS wlan_telemetry_agent_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS wlan_telemetry_agent_deinit(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void *telemetry_sawf_peer_ctx_alloc(void *soc, void *sawf_ctx,
				    uint8_t *mac_addr,
				    uint8_t svc_id, uint8_t hostq_id)
{
	return NULL;
}

static inline
void telemetry_sawf_peer_ctx_free(void *telemetry_ctx) {}

static inline
QDF_STATUS telemetry_sawf_updt_tid_msduq(void *telemetry_ctx,
					 uint8_t hostq_id,
					 uint8_t tid,
					 uint8_t msduq_idx)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS telemetry_sawf_set_mov_avg_params(uint32_t num_pkt,
					     uint32_t num_win)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS telemetry_sawf_set_sla_params(uint32_t num_pkt,
					 uint32_t time_sec)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS telemetry_sawf_set_sla_cfg(uint8_t svc_id,
				      uint8_t min_tput_rate,
				      uint8_t max_tput_rate,
				      uint8_t burst_size,
				      uint8_t svc_interval,
				      uint8_t delay_bound,
				      uint8_t msdu_ttl,
				      uint8_t msdu_rate_loss)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS telemetry_sawf_set_svclass_cfg(bool enable, uint8_t svc_id,
					  uint32_t min_tput_rate,
					  uint32_t max_tput_rate,
					  uint32_t burst_size,
					  uint32_t svc_interval,
					  uint32_t delay_bound,
					  uint32_t msdu_ttl,
					  uint32_t msdu_rate_loss)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS telemetry_sawf_set_sla_detect_cfg(uint8_t detect_type,
					     uint8_t min_tput_rate,
					     uint8_t max_tput_rate,
					     uint8_t burst_size,
					     uint8_t svc_intval,
					     uint8_t delay_bound,
					     uint8_t msdu_ttl,
					     uint8_t msdu_rate_loss)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS telemetry_sawf_update_delay(void *telemetry_ctx, uint8_t tid,
				       uint8_t queue, uint64_t pass,
				       uint64_t fail)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS telemetry_sawf_update_delay_mvng(void *telemetry_ctx,
					    uint8_t tid,
					    uint8_t queue,
					    uint64_t sum_windows)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS telemetry_sawf_update_msdu_drop(void *telemetry_ctx,
					   uint8_t tid, uint8_t queue,
					   uint64_t success,
					   uint64_t failure_drop,
					   uint64_t failure_ttl)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS telemetry_sawf_get_rate(void *telemetry_ctx, uint8_t tid,
				   uint8_t queue, uint32_t *rate)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS telemetry_sawf_get_mov_avg(void *telemetry_ctx, uint8_t tid,
				      uint8_t queue, uint32_t *mov_avg)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif /* WLAN_TELEMETRY_AGENT */
