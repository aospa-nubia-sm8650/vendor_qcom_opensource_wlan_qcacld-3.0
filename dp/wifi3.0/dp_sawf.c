/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <dp_types.h>
#include <dp_peer.h>
#include <dp_internal.h>
#include <dp_htt.h>
#include <dp_sawf_htt.h>
#include <dp_sawf.h>
#include <dp_hist.h>
#include <hal_tx.h>
#include "hal_hw_headers.h"
#include "hal_api.h"
#include "hal_rx.h"
#include "qdf_trace.h"
#include "dp_tx.h"
#include "dp_peer.h"
#include "dp_internal.h"
#include "osif_private.h"
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_telemetry_agent.h>
#include <wlan_sawf.h>

/**
 ** SAWF_metadata related information.
 **/
#define SAWF_VALID_TAG 0xAA
#define SAWF_TAG_SHIFT 0x18
#define SAWF_SERVICE_CLASS_SHIFT 0x10
#define SAWF_SERVICE_CLASS_MASK 0xff
#define SAWF_PEER_ID_SHIFT 0x6
#define SAWF_PEER_ID_MASK 0x3ff
#define SAWF_MSDUQ_MASK 0x3f

/**
 ** SAWF_metadata extraction.
 **/
#define SAWF_TAG_GET(x) ((x) >> SAWF_TAG_SHIFT)
#define SAWF_SERVICE_CLASS_GET(x) (((x) >> SAWF_SERVICE_CLASS_SHIFT) \
	& SAWF_SERVICE_CLASS_MASK)
#define SAWF_PEER_ID_GET(x) (((x) >> SAWF_PEER_ID_SHIFT) \
	& SAWF_PEER_ID_MASK)
#define SAWF_MSDUQ_GET(x) ((x) & SAWF_MSDUQ_MASK)
#define SAWF_TAG_IS_VALID(x) \
	((SAWF_TAG_GET(x) == SAWF_VALID_TAG) ? true : false)

#define DP_TX_TCL_METADATA_TYPE_SET(_var, _val) \
	HTT_TX_TCL_METADATA_TYPE_V2_SET(_var, _val)
#define DP_TCL_METADATA_TYPE_SVC_ID_BASED \
	HTT_TCL_METADATA_V2_TYPE_SVC_ID_BASED

#define SAWF_TELEMETRY_MOV_AVG_PACKETS 1000
#define SAWF_TELEMETRY_MOV_AVG_WINDOWS 10

#define SAWF_TELEMETRY_SLA_PACKETS 100000
#define SAWF_TELEMETRY_SLA_TIME    10

struct sawf_telemetry_params sawf_telemetry_cfg;

static inline uint32_t dp_sawf_get_mov_avg_num_pkt(void)
{
	return sawf_telemetry_cfg.mov_avg.packet;
}

static inline uint32_t dp_sawf_get_sla_num_pkt(void)
{
	return sawf_telemetry_cfg.sla.num_packets;
}

uint16_t dp_sawf_msduq_peer_id_set(uint16_t peer_id, uint8_t msduq)
{
	uint16_t peer_msduq = 0;

	peer_msduq |= (peer_id & SAWF_PEER_ID_MASK) << SAWF_PEER_ID_SHIFT;
	peer_msduq |= (msduq & SAWF_MSDUQ_MASK);
	return peer_msduq;
}

bool dp_sawf_tag_valid_get(qdf_nbuf_t nbuf)
{
	if (SAWF_TAG_IS_VALID(qdf_nbuf_get_mark(nbuf)))
		return true;

	return false;
}

uint32_t dp_sawf_queue_id_get(qdf_nbuf_t nbuf)
{
	uint32_t mark = qdf_nbuf_get_mark(nbuf);
	uint8_t msduq = 0;

	msduq = SAWF_MSDUQ_GET(mark);

	if (!SAWF_TAG_IS_VALID(mark) || msduq == SAWF_MSDUQ_MASK)
		return DP_SAWF_DEFAULT_Q_INVALID;

	return msduq;
}

void dp_sawf_tcl_cmd(uint16_t *htt_tcl_metadata, qdf_nbuf_t nbuf)
{
	uint32_t mark = qdf_nbuf_get_mark(nbuf);
	uint16_t service_id = SAWF_SERVICE_CLASS_GET(mark);

	if (!SAWF_TAG_IS_VALID(mark))
		return;

	*htt_tcl_metadata = 0;
	DP_TX_TCL_METADATA_TYPE_SET(*htt_tcl_metadata,
				    DP_TCL_METADATA_TYPE_SVC_ID_BASED);
	HTT_TX_FLOW_METADATA_TID_OVERRIDE_SET(*htt_tcl_metadata, 1);
	HTT_TX_TCL_METADATA_SVC_CLASS_ID_SET(*htt_tcl_metadata, service_id);
}

QDF_STATUS
dp_peer_sawf_ctx_alloc(struct dp_soc *soc,
		       struct dp_peer *peer)
{
	struct dp_peer_sawf *sawf_ctx;

	/*
	 * In MLO case, primary link peer holds SAWF ctx.
	 * Secondary link peers does not hold SAWF ctx.
	 */
	if (!dp_peer_is_primary_link_peer(peer)) {
		peer->sawf = NULL;
		return QDF_STATUS_SUCCESS;
	}

	sawf_ctx = qdf_mem_malloc(sizeof(struct dp_peer_sawf));
	if (!sawf_ctx) {
		dp_sawf_err("Failed to allocate peer SAWF ctx");
		return QDF_STATUS_E_FAILURE;
	}

	peer->sawf = sawf_ctx;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_peer_sawf_ctx_free(struct dp_soc *soc,
		      struct dp_peer *peer)
{
	if (!peer->sawf) {
		/*
		 * In MLO case, primary link peer holds SAWF ctx.
		 */
		if (!dp_peer_is_primary_link_peer(peer)) {
			return QDF_STATUS_SUCCESS;
		}
		dp_sawf_err("Failed to free peer SAWF ctx");
		return QDF_STATUS_E_FAILURE;
	}

	if (peer->sawf->telemetry_ctx)
		telemetry_sawf_peer_ctx_free(peer->sawf->telemetry_ctx);

	if (peer->sawf)
		qdf_mem_free(peer->sawf);

	return QDF_STATUS_SUCCESS;
}

struct dp_peer_sawf *dp_peer_sawf_ctx_get(struct dp_peer *peer)
{
	struct dp_peer_sawf *sawf_ctx;

	sawf_ctx = peer->sawf;
	if (!sawf_ctx) {
		dp_sawf_err("Failed to allocate peer SAWF ctx");
		return NULL;
	}

	return sawf_ctx;
}

QDF_STATUS
dp_sawf_def_queues_get_map_report(struct cdp_soc_t *soc_hdl,
				  uint8_t *mac_addr)
{
	struct dp_soc *dp_soc;
	struct dp_peer *peer;
	uint8_t tid, tid_active, svc_id;
	struct dp_peer_sawf *sawf_ctx;

	dp_soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_INVAL;
	}

	peer = dp_peer_find_hash_find(dp_soc, mac_addr, 0,
				      DP_VDEV_ALL, DP_MOD_ID_CDP);
	if (!peer) {
		dp_sawf_err("Invalid peer");
		return QDF_STATUS_E_FAILURE;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx) {
		dp_sawf_err("Invalid SAWF ctx");
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return QDF_STATUS_E_FAILURE;
	}

	dp_sawf_info("Peer ", QDF_MAC_ADDR_FMT,
		     QDF_MAC_ADDR_REF(mac_addr));
	dp_sawf_nofl_err("TID    Active    Service Class ID");
	for (tid = 0; tid < DP_SAWF_TID_MAX; ++tid) {
		svc_id = sawf_ctx->tid_reports[tid].svc_class_id;
		tid_active = svc_id &&
			     (svc_id != HTT_SAWF_SVC_CLASS_INVALID_ID);
		dp_sawf_nofl_err("%u        %u            %u",
				 tid, tid_active, svc_id);
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_sawf_def_queues_map_req(struct cdp_soc_t *soc_hdl,
			   uint8_t *mac_addr, uint8_t svc_class_id)
{
	struct dp_soc *dp_soc;
	struct dp_peer *peer;
	uint16_t peer_id;
	QDF_STATUS status;

	dp_soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_INVAL;
	}

	peer = dp_peer_find_hash_find(dp_soc, mac_addr, 0,
				      DP_VDEV_ALL, DP_MOD_ID_CDP);
	if (!peer) {
		dp_sawf_err("Invalid peer");
		return QDF_STATUS_E_FAILURE;
	}

	peer_id = peer->peer_id;

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	dp_sawf_info("peer " QDF_MAC_ADDR_FMT "svc id %u peer id %u",
		 QDF_MAC_ADDR_REF(mac_addr), svc_class_id, peer_id);

	status = dp_htt_h2t_sawf_def_queues_map_req(dp_soc->htt_handle,
						    svc_class_id, peer_id);

	if (status != QDF_STATUS_SUCCESS)
		return status;

	/*
	 * Request map repot conf from FW for all TIDs
	 */
	return dp_htt_h2t_sawf_def_queues_map_report_req(dp_soc->htt_handle,
							 peer_id, 0xff);
}

QDF_STATUS
dp_sawf_def_queues_unmap_req(struct cdp_soc_t *soc_hdl,
			     uint8_t *mac_addr, uint8_t svc_id)
{
	struct dp_soc *dp_soc;
	struct dp_peer *peer;
	uint16_t peer_id;
	QDF_STATUS status;
	uint8_t wildcard_mac[QDF_MAC_ADDR_SIZE] = {0xff, 0xff, 0xff,
						   0xff, 0xff, 0xff};

	dp_soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_INVAL;
	}

	if (!qdf_mem_cmp(mac_addr, wildcard_mac, QDF_MAC_ADDR_SIZE)) {
		/* wildcard unmap */
		peer_id = HTT_H2T_SAWF_DEF_QUEUES_UNMAP_PEER_ID_WILDCARD;
	} else {
		peer = dp_peer_find_hash_find(dp_soc, mac_addr, 0,
					      DP_VDEV_ALL, DP_MOD_ID_CDP);
		if (!peer) {
			dp_sawf_err("Invalid peer");
			return QDF_STATUS_E_FAILURE;
		}
		peer_id = peer->peer_id;
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	}

	dp_sawf_info("peer " QDF_MAC_ADDR_FMT "svc id %u peer id %u",
		     QDF_MAC_ADDR_REF(mac_addr), svc_id, peer_id);

	status =  dp_htt_h2t_sawf_def_queues_unmap_req(dp_soc->htt_handle,
						       svc_id, peer_id);

	if (status != QDF_STATUS_SUCCESS)
		return status;

	/*
	 * Request map repot conf from FW for all TIDs
	 */
	return dp_htt_h2t_sawf_def_queues_map_report_req(dp_soc->htt_handle,
							 peer_id, 0xff);
}

QDF_STATUS
dp_peer_sawf_stats_ctx_alloc(struct dp_soc *soc,
			     struct dp_txrx_peer *txrx_peer)
{
	struct dp_peer_sawf_stats *ctx;
	struct sawf_stats *stats;
	uint8_t q_idx;

	ctx = qdf_mem_malloc(sizeof(struct dp_peer_sawf_stats));
	if (!ctx) {
		dp_sawf_err("Failed to allocate peer SAWF stats");
		return QDF_STATUS_E_FAILURE;
	}

	txrx_peer->sawf_stats = ctx;
	stats = &ctx->stats;

	/* Initialize delay stats hist */
	for (q_idx = 0; q_idx < DP_SAWF_Q_MAX; ++q_idx) {
		dp_hist_init(&stats->delay[q_idx].delay_hist,
			     CDP_HIST_TYPE_HW_TX_COMP_DELAY);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_peer_sawf_stats_ctx_free(struct dp_soc *soc,
			    struct dp_txrx_peer *txrx_peer)
{
	if (!txrx_peer->sawf_stats) {
		dp_sawf_err("Failed to free peer SAWF stats");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_free(txrx_peer->sawf_stats);
	txrx_peer->sawf_stats = NULL;

	return QDF_STATUS_SUCCESS;
}

struct dp_peer_sawf_stats *
dp_peer_sawf_stats_ctx_get(struct dp_txrx_peer *txrx_peer)
{
	struct dp_peer_sawf_stats *sawf_stats;

	sawf_stats = txrx_peer->sawf_stats;
	if (!sawf_stats) {
		dp_sawf_err("Failed to get SAWF stats ctx");
		return NULL;
	}

	return sawf_stats;
}

static QDF_STATUS
dp_sawf_compute_tx_delay(struct dp_tx_desc_s *tx_desc,
			 uint32_t *delay)
{
	int64_t current_timestamp, timestamp_hw_enqueue;

	timestamp_hw_enqueue = tx_desc->timestamp;
	current_timestamp = qdf_ktime_to_ms(qdf_ktime_real_get());

	if (timestamp_hw_enqueue == 0)
		return QDF_STATUS_E_FAILURE;

	*delay = (uint32_t)(current_timestamp -
			    timestamp_hw_enqueue);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
dp_sawf_compute_tx_hw_delay_us(struct dp_soc *soc,
			       struct dp_vdev *vdev,
			       struct hal_tx_completion_status *ts,
			       uint32_t *delay_us)
{
	QDF_STATUS status;
	uint32_t delay;

	/* Tx_rate_stats_info_valid is 0 and tsf is invalid then */
	if (!ts->valid) {
		dp_sawf_info("Invalid Tx rate stats info");
		return QDF_STATUS_E_FAILURE;
	}

	if (!vdev) {
		dp_sawf_err("vdev is null");
		return QDF_STATUS_E_INVAL;
	}

	if (soc->arch_ops.dp_tx_compute_hw_delay)
		status = soc->arch_ops.dp_tx_compute_hw_delay(soc, vdev, ts,
							      &delay);
	else
		status = QDF_STATUS_E_NOSUPPORT;

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	if (delay_us)
		*delay_us = delay;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
dp_sawf_update_tx_delay(struct dp_soc *soc,
			struct dp_vdev *vdev,
			struct hal_tx_completion_status *ts,
			struct dp_tx_desc_s *tx_desc,
			struct dp_peer_sawf *sawf_ctx,
			struct sawf_stats *stats,
			uint8_t tid,
			uint8_t host_q_idx)
{
	struct sawf_delay_stats *tx_delay;
	struct wlan_sawf_scv_class_params *svclass_params;
	uint32_t hw_delay;
	uint8_t svc_id;
	uint32_t num_pkt_win;
	uint32_t win_avg;

	if (QDF_IS_STATUS_ERROR(dp_sawf_compute_tx_hw_delay_us(soc, vdev, ts,
							       &hw_delay))) {
		return QDF_STATUS_E_FAILURE;
	}

	/* Update hist */
	tx_delay = &stats->delay[host_q_idx];
	dp_hist_update_stats(&tx_delay->delay_hist, hw_delay);

	tx_delay->num_pkt++;
	tx_delay->win_total += hw_delay;

	num_pkt_win = dp_sawf_get_mov_avg_num_pkt();
	if (!(tx_delay->num_pkt % num_pkt_win)) {
		win_avg = qdf_do_div(tx_delay->win_total, num_pkt_win);
		/* Update the avg per window */
		telemetry_sawf_update_delay_mvng(
				sawf_ctx->telemetry_ctx,
				tid, host_q_idx,
				win_avg);
		tx_delay->win_total = 0;
	}

	svc_id = sawf_ctx->msduq[host_q_idx].svc_id;
	if (!wlan_delay_bound_configured(svc_id))
		goto cont;

	svclass_params = wlan_get_svc_class_params(svc_id);
	if (svclass_params) {
		(hw_delay > svclass_params->delay_bound *
		 DP_SAWF_DELAY_BOUND_MS_MULTIPLER) ?
			tx_delay->failure++ : tx_delay->success++;

		if (!(tx_delay->num_pkt % dp_sawf_get_sla_num_pkt())) {
			/* Update the success/failre count */
			telemetry_sawf_update_delay(sawf_ctx->telemetry_ctx,
						    tid, host_q_idx,
						    tx_delay->success,
						    tx_delay->failure);
			tx_delay->failure = 0;
			tx_delay->success = 0;
		}
	}

cont:
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_sawf_get_msduq_map_info(struct dp_soc *soc, uint16_t peer_id,
			   uint8_t host_q_idx,
			   uint8_t *remaped_tid, uint8_t *target_q_idx)
{
	struct dp_peer *peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_sawf_msduq *msduq;
	uint8_t tid, q_idx;
	uint8_t mdsuq_index = 0;

	mdsuq_index = host_q_idx - DP_SAWF_DEFAULT_Q_MAX;

	if (mdsuq_index >= DP_SAWF_Q_MAX) {
		dp_sawf_err("Invalid Host Queue Index");
		return QDF_STATUS_E_FAILURE;
	}

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_CDP);
	if (!peer) {
		dp_sawf_err("Invalid peer for peer_id %d", peer_id);
		return QDF_STATUS_E_FAILURE;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx) {
		dp_sawf_err("ctx doesn't exist");
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return QDF_STATUS_E_FAILURE;
	}

	msduq = &sawf_ctx->msduq[mdsuq_index];
	/*
	 * Find tid and msdu queue idx from host msdu queue number
	 * host idx to be taken from the tx descriptor
	 */
	tid = msduq->remapped_tid;
	q_idx = msduq->htt_msduq;

	if (remaped_tid)
		*remaped_tid = tid;

	if (target_q_idx)
		*target_q_idx = q_idx;

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_sawf_tx_compl_update_peer_stats(struct dp_soc *soc,
				   struct dp_vdev *vdev,
				   struct dp_txrx_peer *txrx_peer,
				   struct dp_tx_desc_s *tx_desc,
				   struct hal_tx_completion_status *ts,
				   uint8_t host_tid)
{
	struct dp_peer *peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_peer_sawf_stats *stats_ctx;
	struct sawf_tx_stats *tx_stats;
	uint8_t host_msduq_idx, host_q_idx;
	uint16_t peer_id;
	qdf_size_t length;
	QDF_STATUS status;

	if (!dp_sawf_tag_valid_get(tx_desc->nbuf))
		return QDF_STATUS_E_INVAL;

	if (!ts || !ts->valid)
		return QDF_STATUS_E_INVAL;

	stats_ctx = dp_peer_sawf_stats_ctx_get(txrx_peer);

	if (!stats_ctx) {
		dp_sawf_err("Invalid SAWF stats ctx");
		return QDF_STATUS_E_FAILURE;
	}

	peer_id = SAWF_PEER_ID_GET(qdf_nbuf_get_mark(tx_desc->nbuf));
	host_msduq_idx = dp_sawf_queue_id_get(tx_desc->nbuf);
	if (host_msduq_idx == DP_SAWF_DEFAULT_Q_INVALID ||
	    host_msduq_idx < DP_SAWF_DEFAULT_Q_MAX)
		return QDF_STATUS_E_FAILURE;

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_SAWF);
	if (!peer) {
		dp_sawf_err("No peer for id %u", peer_id);
		return QDF_STATUS_E_FAILURE;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx) {
		dp_sawf_err("No SAWF ctx for peer_id %u", peer_id);
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return QDF_STATUS_E_FAILURE;
	}

	host_q_idx = host_msduq_idx - DP_SAWF_DEFAULT_Q_MAX;
	if (host_q_idx > DP_SAWF_Q_MAX - 1) {
		dp_sawf_err("Invalid host queue idx %u", host_q_idx);
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return QDF_STATUS_E_FAILURE;
	}

	length = qdf_nbuf_len(tx_desc->nbuf);

	DP_STATS_INCC_PKT(stats_ctx, tx_stats[host_q_idx].tx_success, 1,
			  length, (ts->status == HAL_TX_TQM_RR_FRAME_ACKED));

	DP_STATS_INCC_PKT(stats_ctx, tx_stats[host_q_idx].dropped.fw_rem, 1,
			  length, (ts->status == HAL_TX_TQM_RR_REM_CMD_REM));

	DP_STATS_INCC(stats_ctx, tx_stats[host_q_idx].dropped.fw_rem_notx, 1,
		      (ts->status == HAL_TX_TQM_RR_REM_CMD_NOTX));

	DP_STATS_INCC(stats_ctx, tx_stats[host_q_idx].dropped.fw_rem_tx, 1,
		      (ts->status == HAL_TX_TQM_RR_REM_CMD_TX));

	DP_STATS_INCC(stats_ctx, tx_stats[host_q_idx].dropped.age_out, 1,
		      (ts->status == HAL_TX_TQM_RR_REM_CMD_AGED));

	DP_STATS_INCC(stats_ctx, tx_stats[host_q_idx].dropped.fw_reason1, 1,
		      (ts->status == HAL_TX_TQM_RR_FW_REASON1));

	DP_STATS_INCC(stats_ctx, tx_stats[host_q_idx].dropped.fw_reason2, 1,
		      (ts->status == HAL_TX_TQM_RR_FW_REASON2));

	DP_STATS_INCC(stats_ctx, tx_stats[host_q_idx].dropped.fw_reason3, 1,
		      (ts->status == HAL_TX_TQM_RR_FW_REASON3));

	tx_stats = &stats_ctx->stats.tx_stats[host_q_idx];

	tx_stats->tx_failed = tx_stats->dropped.fw_rem.num +
				tx_stats->dropped.fw_rem_notx +
				tx_stats->dropped.fw_rem_tx +
				tx_stats->dropped.age_out +
				tx_stats->dropped.fw_reason1 +
				tx_stats->dropped.fw_reason2 +
				tx_stats->dropped.fw_reason3;

	DP_STATS_DEC(stats_ctx, tx_stats[host_q_idx].queue_depth, 1);

	telemetry_sawf_update_msdu_drop(sawf_ctx->telemetry_ctx,
					host_tid, host_q_idx,
					tx_stats->tx_success.num,
					tx_stats->tx_failed,
					tx_stats->dropped.age_out);

	status = dp_sawf_update_tx_delay(soc, vdev, ts, tx_desc,
					 sawf_ctx,
					 &stats_ctx->stats,
					 host_tid, host_q_idx);

	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	return status;
}

QDF_STATUS
dp_sawf_tx_enqueue_fail_peer_stats(struct dp_soc *soc,
				   struct dp_tx_desc_s *tx_desc)
{
	struct dp_peer_sawf_stats *sawf_ctx;
	struct dp_peer *peer;
	struct dp_txrx_peer *txrx_peer;
	uint8_t host_msduq_idx, host_q_idx;
	uint16_t peer_id;

	if (!dp_sawf_tag_valid_get(tx_desc->nbuf))
		return QDF_STATUS_E_INVAL;

	peer_id = SAWF_PEER_ID_GET(qdf_nbuf_get_mark(tx_desc->nbuf));

	host_msduq_idx = dp_sawf_queue_id_get(tx_desc->nbuf);
	if (host_msduq_idx == DP_SAWF_DEFAULT_Q_INVALID ||
	    host_msduq_idx < DP_SAWF_DEFAULT_Q_MAX)
		return QDF_STATUS_E_FAILURE;

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_TX);
	if (!peer) {
		dp_sawf_err("Invalid peer_id %u", peer_id);
		return QDF_STATUS_E_FAILURE;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_err("Invalid peer_id %u", peer_id);
		goto fail;
	}

	sawf_ctx = dp_peer_sawf_stats_ctx_get(txrx_peer);
	if (!sawf_ctx) {
		dp_sawf_err("Invalid SAWF stats ctx");
		goto fail;
	}

	host_q_idx = host_msduq_idx - DP_SAWF_DEFAULT_Q_MAX;
	if (host_q_idx > DP_SAWF_Q_MAX - 1) {
		dp_sawf_err("Invalid host queue idx %u", host_q_idx);
		goto fail;
	}

	DP_STATS_DEC(sawf_ctx, tx_stats[host_q_idx].queue_depth, 1);

	dp_peer_unref_delete(peer, DP_MOD_ID_TX);

	return QDF_STATUS_SUCCESS;
fail:
	dp_peer_unref_delete(peer, DP_MOD_ID_TX);
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_sawf_tx_enqueue_peer_stats(struct dp_soc *soc,
			      struct dp_tx_desc_s *tx_desc)
{
	struct dp_peer_sawf_stats *sawf_ctx;
	struct dp_peer *peer;
	struct dp_txrx_peer *txrx_peer;
	uint8_t host_msduq_idx, host_q_idx;
	uint16_t peer_id;

	if (!dp_sawf_tag_valid_get(tx_desc->nbuf))
		return QDF_STATUS_E_INVAL;

	peer_id = SAWF_PEER_ID_GET(qdf_nbuf_get_mark(tx_desc->nbuf));

	host_msduq_idx = dp_sawf_queue_id_get(tx_desc->nbuf);
	if (host_msduq_idx == DP_SAWF_DEFAULT_Q_INVALID ||
	    host_msduq_idx < DP_SAWF_DEFAULT_Q_MAX)
		return QDF_STATUS_E_FAILURE;

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_TX);
	if (!peer) {
		dp_sawf_err("Invalid peer_id %u", peer_id);
		return QDF_STATUS_E_FAILURE;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_err("Invalid peer_id %u", peer_id);
		goto fail;
	}

	sawf_ctx = dp_peer_sawf_stats_ctx_get(txrx_peer);
	if (!sawf_ctx) {
		dp_sawf_err("Invalid SAWF stats ctx");
		goto fail;
	}

	host_q_idx = host_msduq_idx - DP_SAWF_DEFAULT_Q_MAX;
	if (host_q_idx > DP_SAWF_Q_MAX - 1) {
		dp_sawf_err("Invalid host queue idx %u", host_q_idx);
		goto fail;
	}

	DP_STATS_INC(sawf_ctx, tx_stats[host_q_idx].queue_depth, 1);
	DP_STATS_INC_PKT(sawf_ctx, tx_stats[host_q_idx].tx_ingress, 1,
			 tx_desc->length);

	dp_peer_unref_delete(peer, DP_MOD_ID_TX);

	return QDF_STATUS_SUCCESS;
fail:
	dp_peer_unref_delete(peer, DP_MOD_ID_TX);
	return QDF_STATUS_E_FAILURE;
}

static void dp_sawf_dump_delay_stats(struct sawf_delay_stats *stats)
{
	uint8_t idx;

	/* CDP hist min, max and weighted average */
	dp_sawf_print_stats("Min  = %d Max  = %d Avg  = %d",
			    stats->delay_hist.min,
			    stats->delay_hist.max,
			    stats->delay_hist.avg);

	/* CDP hist bucket frequency */
	for (idx = 0; idx < CDP_HIST_BUCKET_MAX; idx++) {
		dp_sawf_print_stats("%s:  Packets = %llu",
				    dp_hist_tx_hw_delay_str(idx),
				    stats->delay_hist.hist.freq[idx]);
	}
}

static void dp_sawf_dump_tx_stats(struct sawf_tx_stats *tx_stats)
{
	dp_sawf_print_stats("tx_success: num = %u bytes = %lu",
		       tx_stats->tx_success.num,
		       tx_stats->tx_success.bytes);
	dp_sawf_print_stats("tx_ingress: num = %u bytes = %lu",
			    tx_stats->tx_ingress.num,
			    tx_stats->tx_ingress.bytes);
	dp_sawf_print_stats("dropped: fw_rem num = %u bytes = %lu",
		       tx_stats->dropped.fw_rem.num,
		       tx_stats->dropped.fw_rem.bytes);
	dp_sawf_print_stats("dropped: fw_rem_notx = %u",
		       tx_stats->dropped.fw_rem_notx);
	dp_sawf_print_stats("dropped: fw_rem_tx = %u",
		       tx_stats->dropped.age_out);
	dp_sawf_print_stats("dropped: fw_reason1 = %u",
		       tx_stats->dropped.fw_reason1);
	dp_sawf_print_stats("dropped: fw_reason2 = %u",
		       tx_stats->dropped.fw_reason2);
	dp_sawf_print_stats("dropped: fw_reason3 = %u",
		       tx_stats->dropped.fw_reason3);
	dp_sawf_print_stats("tx_failed = %u", tx_stats->tx_failed);
	dp_sawf_print_stats("queue_depth = %u", tx_stats->queue_depth);
}

static QDF_STATUS
dp_sawf_find_msdu_from_svc_id(struct dp_peer_sawf *ctx, uint8_t svc_id,
			      uint8_t *tid, uint8_t *q_idx)
{
	uint8_t index = 0;

	for (index = 0; index < DP_SAWF_Q_MAX; index++) {
		if (ctx->msduq[index].svc_id == svc_id) {
			*tid = ctx->msduq[index].remapped_tid;
			*q_idx = ctx->msduq[index].htt_msduq;
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_FAILURE;
}

static uint16_t
dp_sawf_get_host_msduq_id(struct dp_peer_sawf *sawf_ctx,
			  uint8_t tid, uint8_t queue)
{
	uint8_t host_q_id;

	if (tid < DP_SAWF_TID_MAX && queue < DP_SAWF_DEFINED_Q_PTID_MAX) {
		host_q_id = sawf_ctx->msduq_map[tid][queue].host_queue_id;
		if (host_q_id)
			return host_q_id;
	}

	return DP_SAWF_PEER_Q_INVALID;
}

static void
dp_sawf_copy_delay_stats(struct sawf_delay_stats *dst,
			 struct sawf_delay_stats *src)
{
	dp_copy_hist_stats(&src->delay_hist, &dst->delay_hist);
	dst->mov_avg = src->mov_avg;
}

static void
dp_sawf_copy_tx_stats(struct sawf_tx_stats *dst, struct sawf_tx_stats *src)
{
	dst->tx_success.num = src->tx_success.num;
	dst->tx_success.bytes = src->tx_success.bytes;

	dst->tx_ingress.num = src->tx_ingress.num;
	dst->tx_ingress.bytes = src->tx_ingress.bytes;

	dst->dropped.fw_rem.num = src->dropped.fw_rem.num;
	dst->dropped.fw_rem.bytes = src->dropped.fw_rem.bytes;
	dst->dropped.fw_rem_notx = src->dropped.fw_rem_notx;
	dst->dropped.fw_rem_tx = src->dropped.fw_rem_tx;
	dst->dropped.age_out = src->dropped.age_out;
	dst->dropped.fw_reason1 = src->dropped.fw_reason1;
	dst->dropped.fw_reason2 = src->dropped.fw_reason2;
	dst->dropped.fw_reason3 = src->dropped.fw_reason3;

	dst->svc_intval_stats.success_cnt = src->svc_intval_stats.success_cnt;
	dst->svc_intval_stats.failure_cnt = src->svc_intval_stats.failure_cnt;
	dst->burst_size_stats.success_cnt = src->burst_size_stats.success_cnt;
	dst->burst_size_stats.failure_cnt = src->burst_size_stats.failure_cnt;

	dst->tx_failed = src->tx_failed;
	dst->queue_depth = src->queue_depth;
	dst->throughput = src->throughput;
	dst->ingress_rate = src->ingress_rate;
}

QDF_STATUS
dp_sawf_get_peer_delay_stats(struct cdp_soc_t *soc,
			     uint32_t svc_id, uint8_t *mac, void *data)
{
	struct dp_soc *dp_soc;
	struct dp_peer *peer = NULL, *primary_link_peer = NULL;
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_peer_sawf_stats *stats_ctx;
	struct sawf_delay_stats *stats, *dst, *src;
	uint8_t tid, q_idx;
	uint16_t host_q_id, host_q_idx;
	QDF_STATUS status;
	uint32_t mov_avg;

	stats = (struct sawf_delay_stats *)data;
	if (!stats) {
		dp_sawf_err("Invalid data to fill");
		return QDF_STATUS_E_FAILURE;
	}

	dp_soc = cdp_soc_t_to_dp_soc(soc);
	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_FAILURE;
	}

	peer = dp_peer_find_hash_find(dp_soc, mac, 0,
				      DP_VDEV_ALL, DP_MOD_ID_CDP);
	if (!peer) {
		dp_sawf_err("Invalid peer");
		return QDF_STATUS_E_INVAL;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_err("txrx peer is NULL");
		goto fail;
	}

	stats_ctx = dp_peer_sawf_stats_ctx_get(txrx_peer);
	if (!stats_ctx) {
		dp_sawf_err("stats ctx doesn't exist");
		goto fail;
	}

	dst = stats;

	primary_link_peer = dp_get_primary_link_peer_by_id(dp_soc,
							   txrx_peer->peer_id,
							   DP_MOD_ID_CDP);
	if (!primary_link_peer) {
		dp_sawf_err("No primary link peer found");
		goto fail;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(primary_link_peer);
	if (!sawf_ctx) {
		dp_sawf_err("stats_ctx doesn't exist");
		goto fail;
	}

	if (svc_id == DP_SAWF_STATS_SVC_CLASS_ID_ALL) {
		for (tid = 0; tid < DP_SAWF_MAX_TIDS; tid++) {
			for (q_idx = 0; q_idx < DP_SAWF_MAX_QUEUES; q_idx++) {
				host_q_id = dp_sawf_get_host_msduq_id(sawf_ctx,
								      tid,
								      q_idx);
				if (host_q_id == DP_SAWF_PEER_Q_INVALID ||
				    host_q_id < DP_SAWF_DEFAULT_Q_MAX) {
					dst++;
					continue;
				}
				host_q_idx = host_q_id - DP_SAWF_DEFAULT_Q_MAX;
				if (host_q_idx > DP_SAWF_Q_MAX - 1) {
					dst++;
					continue;
				}

				src = &stats_ctx->stats.delay[host_q_idx];
				telemetry_sawf_get_mov_avg(
						sawf_ctx->telemetry_ctx,
						tid, host_q_idx, &mov_avg);
				src->mov_avg = mov_avg;

				dp_sawf_print_stats("-- TID: %u MSDUQ: %u --",
						    tid, q_idx);
				dp_sawf_dump_delay_stats(src);
				dp_sawf_copy_delay_stats(dst, src);

				dst++;
			}
		}
	} else {
		/*
		 * Find msduqs of the peer from service class ID
		 */
		status = dp_sawf_find_msdu_from_svc_id(sawf_ctx, svc_id,
						       &tid, &q_idx);
		if (QDF_IS_STATUS_ERROR(status)) {
			dp_sawf_err("No MSDU Queue for svc id %u",
				    svc_id);
			goto fail;
		}

		host_q_id = dp_sawf_get_host_msduq_id(sawf_ctx,
						      tid,
						      q_idx);
		if (host_q_id == DP_SAWF_PEER_Q_INVALID ||
		    host_q_id < DP_SAWF_DEFAULT_Q_MAX) {
			dp_sawf_err("No host queue for tid %u queue %u",
				    tid, q_idx);
			goto fail;
		}
		host_q_idx = host_q_id - DP_SAWF_DEFAULT_Q_MAX;
		if (host_q_idx > DP_SAWF_Q_MAX - 1) {
			dp_sawf_err("Invalid host queue idx %u", host_q_idx);
			goto fail;
		}

		src = &stats_ctx->stats.delay[host_q_idx];
		telemetry_sawf_get_mov_avg(sawf_ctx->telemetry_ctx,
					   tid, host_q_idx, &mov_avg);
		src->mov_avg = mov_avg;

		dp_sawf_print_stats("----TID: %u MSDUQ: %u ----", tid, q_idx);
		dp_sawf_dump_delay_stats(src);
		dp_sawf_copy_delay_stats(dst, src);

		dst->tid = tid;
		dst->msduq = q_idx;
	}

	dp_peer_unref_delete(primary_link_peer, DP_MOD_ID_CDP);
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
fail:
	if (primary_link_peer)
		dp_peer_unref_delete(primary_link_peer, DP_MOD_ID_CDP);
	if (peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_sawf_get_peer_tx_stats(struct cdp_soc_t *soc,
			  uint32_t svc_id, uint8_t *mac, void *data)
{
	struct dp_soc *dp_soc;
	struct dp_peer *peer = NULL, *primary_link_peer = NULL;
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_peer_sawf_stats *stats_ctx;
	struct sawf_tx_stats *stats, *dst, *src;
	uint8_t tid, q_idx;
	uint16_t host_q_id, host_q_idx;
	uint32_t throughput;
	QDF_STATUS status;

	stats = (struct sawf_tx_stats *)data;
	if (!stats) {
		dp_sawf_err("Invalid data to fill");
		return QDF_STATUS_E_FAILURE;
	}

	dp_soc = cdp_soc_t_to_dp_soc(soc);
	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_FAILURE;
	}

	peer = dp_peer_find_hash_find(dp_soc, mac, 0,
				      DP_VDEV_ALL, DP_MOD_ID_CDP);
	if (!peer) {
		dp_sawf_err("Invalid peer");
		return QDF_STATUS_E_INVAL;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_err("txrx peer is NULL");
		goto fail;
	}

	stats_ctx = dp_peer_sawf_stats_ctx_get(txrx_peer);
	if (!stats_ctx) {
		dp_sawf_err("stats ctx doesn't exist");
		goto fail;
	}

	dst = stats;

	primary_link_peer = dp_get_primary_link_peer_by_id(dp_soc,
							   txrx_peer->peer_id,
							   DP_MOD_ID_CDP);
	if (!primary_link_peer) {
		dp_sawf_err("No primary link peer found");
		goto fail;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(primary_link_peer);
	if (!sawf_ctx) {
		dp_sawf_err("stats_ctx doesn't exist");
		goto fail;
	}

	if (svc_id == DP_SAWF_STATS_SVC_CLASS_ID_ALL) {
		for (tid = 0; tid < DP_SAWF_MAX_TIDS; tid++) {
			for (q_idx = 0; q_idx < DP_SAWF_MAX_QUEUES; q_idx++) {
				host_q_id = dp_sawf_get_host_msduq_id(sawf_ctx,
								      tid,
								      q_idx);
				if (host_q_id == DP_SAWF_PEER_Q_INVALID ||
				    host_q_id < DP_SAWF_DEFAULT_Q_MAX) {
					dst++;
					continue;
				}
				host_q_idx = host_q_id - DP_SAWF_DEFAULT_Q_MAX;
				if (host_q_idx > DP_SAWF_Q_MAX - 1) {
					dst++;
					continue;
				}

				src = &stats_ctx->stats.tx_stats[host_q_idx];
				telemetry_sawf_get_rate(sawf_ctx->telemetry_ctx,
							tid, host_q_idx,
							&throughput);
				src->throughput = throughput;

				dp_sawf_print_stats("-- TID: %u MSDUQ: %u --",
						    tid, q_idx);
				dp_sawf_dump_tx_stats(src);
				dp_sawf_copy_tx_stats(dst, src);

				dst++;
			}
		}
	} else {
		/*
		 * Find msduqs of the peer from service class ID
		 */
		status = dp_sawf_find_msdu_from_svc_id(sawf_ctx, svc_id,
						       &tid, &q_idx);
		if (QDF_IS_STATUS_ERROR(status)) {
			dp_sawf_err("No MSDU Queue for svc id %u",
				    svc_id);
			goto fail;
		}

		host_q_id = dp_sawf_get_host_msduq_id(sawf_ctx,
						      tid,
						      q_idx);
		if (host_q_id == DP_SAWF_PEER_Q_INVALID ||
		    host_q_id < DP_SAWF_DEFAULT_Q_MAX) {
			dp_sawf_err("No host queue for tid %u queue %u",
				    tid, q_idx);
			goto fail;
		}
		host_q_idx = host_q_id - DP_SAWF_DEFAULT_Q_MAX;
		if (host_q_idx > DP_SAWF_Q_MAX - 1) {
			dp_sawf_err("Invalid host queue idx %u", host_q_idx);
			goto fail;
		}

		src = &stats_ctx->stats.tx_stats[host_q_idx];
		telemetry_sawf_get_rate(sawf_ctx->telemetry_ctx,
					tid, host_q_idx, &throughput);
		src->throughput = throughput;

		dp_sawf_print_stats("----TID: %u MSDUQ: %u ----",
				    tid, q_idx);
		dp_sawf_dump_tx_stats(src);
		dp_sawf_copy_tx_stats(dst, src);
		dst->tid = tid;
		dst->msduq = q_idx;
	}

	dp_peer_unref_delete(primary_link_peer, DP_MOD_ID_CDP);
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
fail:
	if (primary_link_peer)
		dp_peer_unref_delete(primary_link_peer, DP_MOD_ID_CDP);
	if (peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_sawf_get_tx_stats(void *arg, uint64_t *in_bytes, uint64_t *in_cnt,
		     uint64_t *tx_bytes, uint64_t *tx_cnt,
		     uint8_t tid, uint8_t msduq)
{
	struct dp_peer_sawf_stats *stats_ctx;
	struct sawf_tx_stats *tx_stats;

	stats_ctx = arg;
	tx_stats = &stats_ctx->stats.tx_stats[msduq];

	*in_bytes = tx_stats->tx_ingress.bytes;
	*in_cnt = tx_stats->tx_ingress.num;
	*tx_bytes = tx_stats->tx_success.bytes;
	*tx_cnt = tx_stats->tx_success.num;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_sawf_get_mpdu_sched_stats(void *arg, uint64_t *svc_int_pass,
			     uint64_t *svc_int_fail, uint64_t *burst_pass,
			     uint64_t *burst_fail, uint8_t tid, uint8_t msduq)
{
	struct dp_peer_sawf_stats *stats_ctx;
	struct sawf_tx_stats *tx_stats;

	stats_ctx = arg;
	tx_stats = &stats_ctx->stats.tx_stats[msduq];

	*svc_int_pass = tx_stats->svc_intval_stats.success_cnt;
	*svc_int_fail = tx_stats->svc_intval_stats.failure_cnt;
	*burst_pass = tx_stats->burst_size_stats.success_cnt;
	*burst_fail = tx_stats->burst_size_stats.failure_cnt;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_sawf_get_drop_stats(void *arg, uint64_t *pass, uint64_t *drop,
		       uint64_t *drop_ttl, uint8_t tid, uint8_t msduq)
{
	struct dp_peer_sawf_stats *stats_ctx;
	struct sawf_tx_stats *tx_stats;

	stats_ctx = arg;
	tx_stats = &stats_ctx->stats.tx_stats[msduq];

	*pass = tx_stats->tx_success.num;
	*drop = tx_stats->tx_failed;
	*drop_ttl = tx_stats->dropped.age_out;

	return QDF_STATUS_SUCCESS;
}

uint16_t dp_sawf_get_peerid(struct dp_soc *soc, uint8_t *dest_mac,
			    uint8_t vdev_id)
{
	struct dp_ast_entry *ast_entry = NULL;
	uint16_t peer_id;

	qdf_spin_lock_bh(&soc->ast_lock);
	ast_entry = dp_peer_ast_hash_find_by_vdevid(soc, dest_mac, vdev_id);

	if (!ast_entry) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		qdf_warn("%s NULL ast entry");
		return HTT_INVALID_PEER;
	}

	peer_id = ast_entry->peer_id;
	qdf_spin_unlock_bh(&soc->ast_lock);
	return peer_id;
}

uint32_t dp_sawf_get_search_index(struct dp_soc *soc, qdf_nbuf_t nbuf,
				  uint8_t vdev_id, uint16_t queue_id)
{
	struct dp_peer *peer = NULL;
	uint32_t search_index = DP_SAWF_INVALID_AST_IDX;
	uint16_t peer_id = SAWF_PEER_ID_GET(qdf_nbuf_get_mark(nbuf));
	uint8_t index = queue_id / DP_SAWF_TID_MAX;

	if (index >= DP_PEER_AST_FLOWQ_MAX) {
		qdf_warn("invalid index:%d", index);
		return DP_SAWF_INVALID_AST_IDX;
	}

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_SAWF);

	if (!peer) {
		qdf_warn("NULL peer");
		return DP_SAWF_INVALID_AST_IDX;
	}

	search_index = peer->peer_ast_flowq_idx[index].ast_idx;
	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	return search_index;
}

#ifdef QCA_SUPPORT_WDS_EXTENDED
static struct dp_peer *dp_sawf_get_peer_from_wds_ext_dev(
				struct net_device *netdev,
				uint8_t *dest_mac,
				struct dp_soc **soc)
{
	osif_peer_dev *osifp = NULL;
	osif_dev *osdev;
	osif_dev *parent_osdev;
	struct wlan_objmgr_vdev *vdev = NULL;

	/*
	 * Here netdev received need to be AP vlan netdev of type WDS EXT.
	 */
	osdev = ath_netdev_priv(netdev);
	if (osdev->dev_type != OSIF_NETDEV_TYPE_WDS_EXT) {
		qdf_debug("Dev type is not WDS EXT");
		return NULL;
	}

	/*
	 * Get the private structure of AP vlan dev.
	 */
	osifp = ath_netdev_priv(netdev);
	if (!osifp->parent_netdev) {
		qdf_debug("Parent dev cannot be NULL");
		return NULL;
	}

	/*
	 * Vdev ctx are valid only in parent netdev.
	 */
	parent_osdev = ath_netdev_priv(osifp->parent_netdev);
	vdev = parent_osdev->ctrl_vdev;
	*soc = (struct dp_soc *)wlan_psoc_get_dp_handle
	      (wlan_pdev_get_psoc(wlan_vdev_get_pdev(vdev)));
	if (!(*soc)) {
		qdf_debug("Soc cannot be NULL");
		return NULL;
	}

	return dp_peer_get_ref_by_id((*soc), osifp->peer_id, DP_MOD_ID_SAWF);
}
#endif

uint16_t dp_sawf_get_msduq(struct net_device *netdev, uint8_t *dest_mac,
			   uint32_t service_id)
{
	osif_dev  *osdev;
	wlan_if_t vap;
	struct wlan_objmgr_vdev *vdev;
	uint8_t vdev_id;
	uint8_t i = 1;
	struct dp_peer *peer = NULL;
	struct dp_peer *primary_link_peer = NULL;
	struct dp_soc *soc = NULL;
	uint16_t peer_id;
	uint8_t q_id;
	void *telemetry_ctx;
	struct dp_txrx_peer *txrx_peer;

	if (!netdev->ieee80211_ptr) {
		qdf_debug("non vap netdevice");
		return DP_SAWF_PEER_Q_INVALID;
	}

	osdev = ath_netdev_priv(netdev);
#ifdef QCA_SUPPORT_WDS_EXTENDED
	if (osdev->dev_type == OSIF_NETDEV_TYPE_WDS_EXT) {
		peer = dp_sawf_get_peer_from_wds_ext_dev(netdev, dest_mac, &soc);
		if (peer)
			goto process_peer;

		qdf_info("Peer not found from WDS EXT dev");
		return DP_SAWF_PEER_Q_INVALID;
	}
#endif
	vap = osdev->os_if;
	vdev = osdev->ctrl_vdev;
	soc = (struct dp_soc *)wlan_psoc_get_dp_handle
	      (wlan_pdev_get_psoc(wlan_vdev_get_pdev(vdev)));
	if (!soc) {
		qdf_info("Soc cannot be NULL");
		return DP_SAWF_PEER_Q_INVALID;
	}

	vdev_id = wlan_vdev_get_id(vdev);

	peer = soc->arch_ops.dp_find_peer_by_destmac(soc,
					dest_mac, vdev_id);
	if (!peer) {
		qdf_warn("NULL peer");
		return DP_SAWF_PEER_Q_INVALID;
	}

#ifdef QCA_SUPPORT_WDS_EXTENDED
process_peer:
#endif
	if (IS_MLO_DP_MLD_PEER(peer)) {
		primary_link_peer = dp_get_primary_link_peer_by_id(soc,
					peer->peer_id,
					DP_MOD_ID_SAWF);
		if (!primary_link_peer) {
			dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
			qdf_warn("NULL peer");
			return DP_SAWF_PEER_Q_INVALID;
		}

		/*
		 * Release the MLD-peer reference.
		 * Hold only primary link ref now.
		 */
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		peer = primary_link_peer;
	}
	peer_id = peer->peer_id;

	/*
	 * In MLO case, secondary links may not have SAWF ctx.
	 */
	if (!peer->sawf) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		qdf_warn("Peer SAWF ctx invalid");
		return DP_SAWF_PEER_Q_INVALID;
	}

	/*
	 * First loop to go through all msdu queues of peer which
	 * have been used. If flow has same service id as that of
	 * used msdu queues, return used msdu queue.
	 */
	for (i = 0; i < DP_SAWF_Q_MAX; i++) {
		if ((dp_sawf(peer, i, is_used) == 1) &&
		    dp_sawf(peer, i, svc_id) == service_id) {
			dp_sawf(peer, i, ref_count)++;
			dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
			q_id = i + DP_SAWF_DEFAULT_Q_MAX;
			return dp_sawf_msduq_peer_id_set(peer_id, q_id);
		}
	}

	txrx_peer = dp_get_txrx_peer(peer);
	/*
	 * Second loop to go through all unused msdu queues of peer.
	 * Allot new msdu queue for new service class.
	 */
	for (i = 0; i < DP_SAWF_Q_MAX; i++) {
		if (dp_sawf(peer, i, is_used) == 0) {
			dp_sawf(peer, i, is_used) = 1;
			dp_sawf(peer, i, svc_id) = service_id;
			dp_sawf(peer, i, ref_count)++;
			if (!peer->sawf->telemetry_ctx) {
				telemetry_ctx = telemetry_sawf_peer_ctx_alloc(
					soc, txrx_peer->sawf_stats, dest_mac,
					service_id, i);
				if (telemetry_ctx)
					peer->sawf->telemetry_ctx = telemetry_ctx;
			}
			dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
			q_id = i + DP_SAWF_DEFAULT_Q_MAX;
			return dp_sawf_msduq_peer_id_set(peer_id, q_id);
		}
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	/* request for more msdu queues. Return error*/
	return DP_SAWF_PEER_Q_INVALID;
}

qdf_export_symbol(dp_sawf_get_msduq);

/*
 * dp_sawf_mpdu_stats_req_send() - Send MPDU stats request to target
 * @soc_hdl: SOC handle
 * @stats_type: MPDU stats type
 * @enable: 1: Enable 0: Disable
 * @config_param0: Opaque configuration
 * @config_param1: Opaque configuration
 * @config_param2: Opaque configuration
 * @config_param3: Opaque configuration
 *
 * @Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
dp_sawf_mpdu_stats_req_send(struct cdp_soc_t *soc_hdl,
			    uint8_t stats_type, uint8_t enable,
			    uint32_t config_param0, uint32_t config_param1,
			    uint32_t config_param2, uint32_t config_param3)
{
	struct dp_soc *dp_soc;

	dp_soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_INVAL;
	}

	return dp_sawf_htt_h2t_mpdu_stats_req(dp_soc->htt_handle,
						   stats_type,
						   enable,
						   config_param0,
						   config_param1,
						   config_param2,
						   config_param3);
}

QDF_STATUS
dp_sawf_mpdu_stats_req(struct cdp_soc_t *soc_hdl, uint8_t enable)
{
	return dp_sawf_mpdu_stats_req_send(soc_hdl,
					   HTT_STRM_GEN_MPDUS_STATS,
					   enable,
					   0, 0, 0, 0);
}

QDF_STATUS
dp_sawf_mpdu_details_stats_req(struct cdp_soc_t *soc_hdl, uint8_t enable)
{
	return dp_sawf_mpdu_stats_req_send(soc_hdl,
					   HTT_STRM_GEN_MPDUS_DETAILS_STATS,
					   enable,
					   0, 0, 0, 0);
}

QDF_STATUS dp_sawf_update_mpdu_basic_stats(struct dp_soc *soc,
					   uint16_t peer_id,
					   uint8_t tid, uint8_t q_idx,
					   uint64_t svc_intval_success_cnt,
					   uint64_t svc_intval_failure_cnt,
					   uint64_t burst_size_success_cnt,
					   uint64_t burst_size_failue_cnt)
{
	struct dp_peer *peer;
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_peer_sawf_stats *sawf_stats_ctx;
	uint8_t host_q_id, host_q_idx;

	if (!soc) {
		dp_sawf_err("Invalid soc context");
		return QDF_STATUS_E_FAILURE;
	}

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_SAWF);
	if (!peer) {
		dp_sawf_err("Invalid peer");
		return QDF_STATUS_E_FAILURE;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx) {
		dp_sawf_err("stats_ctx doesn't exist");
		goto fail;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_err("txrx peer is NULL");
		goto fail;
	}

	sawf_stats_ctx = dp_peer_sawf_stats_ctx_get(txrx_peer);
	if (!sawf_stats_ctx) {
		dp_sawf_err("No sawf stats ctx for peer_id %d", peer_id);
		goto fail;
	}

	host_q_id = sawf_ctx->msduq_map[tid][q_idx].host_queue_id;
	if (!host_q_id || host_q_id < DP_SAWF_DEFAULT_Q_MAX) {
		dp_sawf_err("Invalid host queue id %u", host_q_id);
		goto fail;
	}

	host_q_idx = host_q_id - DP_SAWF_DEFAULT_Q_MAX;
	if (host_q_idx > DP_SAWF_Q_MAX - 1) {
		dp_sawf_err("Invalid host queue idx %u", host_q_idx);
		goto fail;
	}

	DP_STATS_INC(sawf_stats_ctx,
		     tx_stats[host_q_idx].svc_intval_stats.success_cnt,
		     svc_intval_success_cnt);
	DP_STATS_INC(sawf_stats_ctx,
		     tx_stats[host_q_idx].svc_intval_stats.failure_cnt,
		     svc_intval_failure_cnt);
	DP_STATS_INC(sawf_stats_ctx,
		     tx_stats[host_q_idx].burst_size_stats.success_cnt,
		     burst_size_success_cnt);
	DP_STATS_INC(sawf_stats_ctx,
		     tx_stats[host_q_idx].burst_size_stats.failure_cnt,
		     burst_size_failue_cnt);

	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
	return QDF_STATUS_SUCCESS;
fail:
	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS dp_sawf_set_mov_avg_params(uint32_t num_pkt,
				      uint32_t num_win)
{
	sawf_telemetry_cfg.mov_avg.packet = num_pkt;
	sawf_telemetry_cfg.mov_avg.window = num_win;

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(dp_sawf_set_mov_avg_params);

QDF_STATUS dp_sawf_set_sla_params(uint32_t num_pkt,
				  uint32_t time_secs)
{
	sawf_telemetry_cfg.sla.num_packets = num_pkt;
	sawf_telemetry_cfg.sla.time_secs = time_secs;

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(dp_sawf_set_sla_params);

QDF_STATUS dp_sawf_init_telemetry_params(void)
{
	sawf_telemetry_cfg.mov_avg.packet = SAWF_TELEMETRY_MOV_AVG_PACKETS;
	sawf_telemetry_cfg.mov_avg.window = SAWF_TELEMETRY_MOV_AVG_WINDOWS;

	sawf_telemetry_cfg.sla.num_packets = SAWF_TELEMETRY_SLA_PACKETS;
	sawf_telemetry_cfg.sla.time_secs = SAWF_TELEMETRY_SLA_TIME;

	return QDF_STATUS_SUCCESS;
}
