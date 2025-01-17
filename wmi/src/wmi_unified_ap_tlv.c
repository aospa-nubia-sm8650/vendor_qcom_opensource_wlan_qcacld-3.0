/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
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

#include <osdep.h>
#include "wmi.h"
#include "wmi_unified_priv.h"
#include "wmi_unified_ap_api.h"
#include <wlan_utility.h>
#include "wmi_unified_rtt.h"
#include <wmi_unified_ap_11be_api.h>

wmi_channel_width convert_host_to_target_chwidth(uint32_t chan_width)
{
	switch (chan_width) {
	case CH_WIDTH_20MHZ:
		return WMI_CHAN_WIDTH_20;
	case CH_WIDTH_40MHZ:
		return WMI_CHAN_WIDTH_40;
	case CH_WIDTH_80MHZ:
		return WMI_CHAN_WIDTH_80;
	case CH_WIDTH_160MHZ:
		return WMI_CHAN_WIDTH_160;
	case CH_WIDTH_80P80MHZ:
		return WMI_CHAN_WIDTH_80P80;
	case CH_WIDTH_5MHZ:
		return WMI_CHAN_WIDTH_5;
	case CH_WIDTH_10MHZ:
		return WMI_CHAN_WIDTH_10;
#ifdef WLAN_FEATURE_11BE
	case CH_WIDTH_320MHZ:
		return WMI_CHAN_WIDTH_320;
#endif
	default:
		return WMI_CHAN_WIDTH_MAX;
	}
}

/**
 * send_peer_add_wds_entry_cmd_tlv() - send peer add command to fw
 * @wmi_handle: wmi handle
 * @param: pointer holding peer details
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS send_peer_add_wds_entry_cmd_tlv(wmi_unified_t wmi_handle,
					struct peer_add_wds_entry_params *param)
{
	wmi_peer_add_wds_entry_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		qdf_print("%s: wmi_buf_alloc failed", __func__);
		return QDF_STATUS_E_FAILURE;
	}
	cmd = (wmi_peer_add_wds_entry_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_add_wds_entry_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
		       (wmi_peer_add_wds_entry_cmd_fixed_param));
	WMI_CHAR_ARRAY_TO_MAC_ADDR(param->dest_addr, &cmd->wds_macaddr);
	WMI_CHAR_ARRAY_TO_MAC_ADDR(param->peer_addr, &cmd->peer_macaddr);
	cmd->flags = (param->flags & WMI_HOST_WDS_FLAG_STATIC) ?
		      WMI_WDS_FLAG_STATIC : 0;
	cmd->vdev_id = param->vdev_id;

	wmi_mtrace(WMI_PEER_ADD_WDS_ENTRY_CMDID, cmd->vdev_id, 0);

	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_PEER_ADD_WDS_ENTRY_CMDID)) {
		wmi_err("peer %pM vdev_id %d wds %pM flag %d failed!",
			 param->peer_addr, param->vdev_id,
			 param->dest_addr, param->flags);
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}
	wmi_debug("peer_macaddr %pM vdev_id %d, wds_macaddr %pM flag %d",
		  param->peer_addr, param->vdev_id,
		  param->dest_addr, param->flags);

	return QDF_STATUS_SUCCESS;
}

/**
 * send_peer_del_wds_entry_cmd_tlv() - send peer delete command to fw
 * @wmi_handle: wmi handle
 * @param: pointer holding peer details
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS send_peer_del_wds_entry_cmd_tlv(wmi_unified_t wmi_handle,
					struct peer_del_wds_entry_params *param)
{
	wmi_peer_remove_wds_entry_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		qdf_print("%s: wmi_buf_alloc failed", __func__);
		return QDF_STATUS_E_NOMEM;
	}
	cmd = (wmi_peer_remove_wds_entry_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_peer_remove_wds_entry_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN
		(wmi_peer_remove_wds_entry_cmd_fixed_param));
	WMI_CHAR_ARRAY_TO_MAC_ADDR(param->dest_addr, &cmd->wds_macaddr);
	cmd->vdev_id = param->vdev_id;
	wmi_mtrace(WMI_PEER_REMOVE_WDS_ENTRY_CMDID, cmd->vdev_id, 0);

	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_PEER_REMOVE_WDS_ENTRY_CMDID)) {
		wmi_err("vdev_id %d wds_addr %pM failed!",
			param->vdev_id, param->dest_addr);
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}
	wmi_debug("vdev_id %d wds_addr %pM",
		  param->vdev_id, param->dest_addr);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_MULTI_AST_DEL
/**
 * send_peer_del_wds_entry_cmd_tlv() - send peer ast delete command to fw
 * @wmi_handle: wmi handle
 * @param: pointer holding wds entries details
 *
 * Return: QDF_STATUS_SUCCESS on sending wmi cmd to FW, else error value
 */
static QDF_STATUS send_peer_del_multi_wds_entries_cmd_tlv(
		wmi_unified_t wmi_handle,
		struct peer_del_multi_wds_entry_params *param)
{
	int i;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	wmi_vdev_multiple_peer_group_cmd_fixed_param *cmd;
	uint32_t len = sizeof(*cmd);
	wmi_mac_addr *mac_addr;
	uint32_t max_macs_per_cmd, max_macs_cnt;
	uint32_t pending_cnt = param->num_entries;
	uint32_t num_macs = 0;
	struct peer_del_multi_wds_entry_val *param_list = param->dest_list;

	max_macs_per_cmd = (wmi_get_max_msg_len(wmi_handle) -
			    sizeof(*cmd) - WMI_TLV_HDR_SIZE) /
			    sizeof(wmi_mac_addr);

	if (param->num_entries &&
	    param->num_entries <= max_macs_per_cmd)
		max_macs_cnt = param->num_entries;
	else
		max_macs_cnt = max_macs_per_cmd;

	wmi_debug("Setting max macs limit as %u", max_macs_cnt);
	while (pending_cnt > 0) {
		len = sizeof(*cmd) + WMI_TLV_HDR_SIZE;
		if (pending_cnt >= max_macs_cnt)
			num_macs = max_macs_cnt;
		else
			num_macs = pending_cnt;

		len += num_macs * sizeof(wmi_mac_addr);
		buf = wmi_buf_alloc(wmi_handle, len);
		if (!buf) {
			wmi_err("Failed to allocate memory");
			return QDF_STATUS_E_NOMEM;
		}

		buf_ptr = (uint8_t *)wmi_buf_data(buf);
		cmd = (wmi_vdev_multiple_peer_group_cmd_fixed_param *)
						wmi_buf_data(buf);
		WMITLV_SET_HDR(
			&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_vdev_multiple_peer_group_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN
			(wmi_vdev_multiple_peer_group_cmd_fixed_param));

		cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
					wmi_handle, param->pdev_id);
		cmd->vdev_id = param->vdev_id;
		cmd->sub_cmd_id = WMI_PEER_REMOVE_WDS_ENTRY_CMDID;

		buf_ptr += sizeof(*cmd);
		WMITLV_SET_HDR(buf_ptr,
			       WMITLV_TAG_ARRAY_FIXED_STRUC,
			       num_macs * sizeof(wmi_mac_addr));

		mac_addr = (wmi_mac_addr *)(buf_ptr + WMI_TLV_HDR_SIZE);
		wmi_debug("send vdev_id %d num_macs:%d!",
			  param->vdev_id, num_macs);
		for (i = 0; i < num_macs; i++)
			WMI_CHAR_ARRAY_TO_MAC_ADDR(param_list[i].dest_addr,
						   &mac_addr[i]);

		pending_cnt -= num_macs;
		param_list += num_macs;

		wmi_mtrace(WMI_VDEV_MULTIPLE_PEER_GROUP_CMDID, cmd->vdev_id, 0);

		if (wmi_unified_cmd_send(wmi_handle, buf, len,
					 WMI_VDEV_MULTIPLE_PEER_GROUP_CMDID)) {
			wmi_err("vdev_id %d num_entries:%d failed!",
				param->vdev_id, param->num_entries);
			wmi_buf_free(buf);
			return QDF_STATUS_E_FAILURE;
		}
		wmi_debug("vdev_id %d num_entries:%d done!",
			  param->vdev_id, num_macs);
	}
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * send_peer_update_wds_entry_cmd_tlv() - send peer update command to fw
 * @wmi_handle: wmi handle
 * @param: pointer holding peer details
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS send_peer_update_wds_entry_cmd_tlv(wmi_unified_t wmi_handle,
				struct peer_update_wds_entry_params *param)
{
	wmi_peer_update_wds_entry_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		qdf_print("%s: wmi_buf_alloc failed", __func__);
		return QDF_STATUS_E_NOMEM;
	}

	/* wmi_buf_alloc returns zeroed command buffer */
	cmd = (wmi_peer_update_wds_entry_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_peer_update_wds_entry_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN
				(wmi_peer_update_wds_entry_cmd_fixed_param));
	cmd->flags = (param->flags & WMI_HOST_WDS_FLAG_STATIC) ? WMI_WDS_FLAG_STATIC : 0;
	cmd->vdev_id = param->vdev_id;
	if (param->wds_macaddr)
		WMI_CHAR_ARRAY_TO_MAC_ADDR(param->wds_macaddr,
				&cmd->wds_macaddr);
	if (param->peer_macaddr)
		WMI_CHAR_ARRAY_TO_MAC_ADDR(param->peer_macaddr,
				&cmd->peer_macaddr);
	wmi_mtrace(WMI_PEER_UPDATE_WDS_ENTRY_CMDID, cmd->vdev_id, 0);

	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_PEER_UPDATE_WDS_ENTRY_CMDID)) {
		wmi_err("peer %pM vdev_id %d wds %pM flags %d failed!",
			param->peer_macaddr, param->vdev_id,
			param->wds_macaddr, param->flags);
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	wmi_debug("peer_addr %pM vdev_id %d wds_addr %pM flags %d",
		  param->peer_macaddr, param->vdev_id,
		  param->wds_macaddr, param->flags);
	return QDF_STATUS_SUCCESS;
}

/**
 * send_pdev_get_tpc_config_cmd_tlv() - send get tpc config command to fw
 * @wmi_handle: wmi handle
 * @param: pointer to get tpc config params
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS
send_pdev_get_tpc_config_cmd_tlv(wmi_unified_t wmi_handle,
				uint32_t param)
{
	wmi_pdev_get_tpc_config_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(wmi_pdev_get_tpc_config_cmd_fixed_param);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_NOMEM;
	}
	cmd = (wmi_pdev_get_tpc_config_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_pdev_get_tpc_config_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN
		(wmi_pdev_get_tpc_config_cmd_fixed_param));

	cmd->param = param;
	wmi_mtrace(WMI_PDEV_GET_TPC_CONFIG_CMDID, NO_SESSION, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_PDEV_GET_TPC_CONFIG_CMDID)) {
		wmi_err("Send pdev get tpc config cmd failed");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;

	}
	wmi_debug("send success");

	return QDF_STATUS_SUCCESS;
}

/**
 * send_set_ctl_table_cmd_tlv() - send ctl table cmd to fw
 * @wmi_handle: wmi handle
 * @param: pointer to hold ctl table param
 *
 *  @return QDF_STATUS_SUCCESS  on success and -ve on failure.
 */
static QDF_STATUS
send_set_ctl_table_cmd_tlv(wmi_unified_t wmi_handle,
			   struct ctl_table_params *param)
{
	uint16_t len, ctl_tlv_len;
	uint8_t *buf_ptr;
	wmi_buf_t buf;
	wmi_pdev_set_ctl_table_cmd_fixed_param *cmd;
	uint32_t *ctl_array;

	if (!param->ctl_array)
		return QDF_STATUS_E_FAILURE;

	ctl_tlv_len = WMI_TLV_HDR_SIZE +
		roundup(param->ctl_cmd_len, sizeof(uint32_t));
	len = sizeof(*cmd) + ctl_tlv_len;

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}

	buf_ptr = wmi_buf_data(buf);
	qdf_mem_zero(buf_ptr, len);

	cmd = (wmi_pdev_set_ctl_table_cmd_fixed_param *)buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_pdev_set_ctl_table_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				wmi_pdev_set_ctl_table_cmd_fixed_param));
	cmd->ctl_len = param->ctl_cmd_len;
	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
								wmi_handle,
								param->pdev_id);

	buf_ptr += sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
		       (cmd->ctl_len));
	buf_ptr += WMI_TLV_HDR_SIZE;
	ctl_array = (uint32_t *)buf_ptr;

	WMI_HOST_IF_MSG_COPY_CHAR_ARRAY(&ctl_array[0], &param->ctl_band,
					sizeof(param->ctl_band));
	WMI_HOST_IF_MSG_COPY_CHAR_ARRAY(&ctl_array[1], param->ctl_array,
					param->ctl_cmd_len -
					sizeof(param->ctl_band));

	wmi_mtrace(WMI_PDEV_SET_CTL_TABLE_CMDID, NO_SESSION, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_PDEV_SET_CTL_TABLE_CMDID)) {
		wmi_err("Failed to send command");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * send_set_mimogain_table_cmd_tlv() - send mimogain table cmd to fw
 * @wmi_handle: wmi handle
 * @param: pointer to hold mimogain table param
 *
 *  @return QDF_STATUS_SUCCESS  on success and -ve on failure.
 */
static QDF_STATUS
send_set_mimogain_table_cmd_tlv(wmi_unified_t wmi_handle,
				struct mimogain_table_params *param)
{
	uint16_t len, table_tlv_len;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	wmi_pdev_set_mimogain_table_cmd_fixed_param *cmd;
	uint32_t *gain_table;

	if (!param->array_gain)
		return QDF_STATUS_E_FAILURE;

	/* len must be multiple of a single array gain table */
	if (param->tbl_len %
	    ((WMI_HOST_TX_NUM_CHAIN-1) * WMI_HOST_TPC_REGINDEX_MAX *
	     WMI_HOST_ARRAY_GAIN_NUM_STREAMS) != 0) {
		wmi_err("Array gain table len not correct");
		return QDF_STATUS_E_FAILURE;
	}

	table_tlv_len = WMI_TLV_HDR_SIZE +
		roundup(param->tbl_len, sizeof(uint32_t));
	len = sizeof(*cmd) + table_tlv_len;

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}

	buf_ptr = wmi_buf_data(buf);
	qdf_mem_zero(buf_ptr, len);

	cmd = (wmi_pdev_set_mimogain_table_cmd_fixed_param *)buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_pdev_set_mimogain_table_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
		       wmi_pdev_set_mimogain_table_cmd_fixed_param));

	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
								wmi_handle,
								param->pdev_id);
	WMI_MIMOGAIN_ARRAY_GAIN_LEN_SET(cmd->mimogain_info, param->tbl_len);
	WMI_MIMOGAIN_MULTI_CHAIN_BYPASS_SET(cmd->mimogain_info,
					    param->multichain_gain_bypass);

	buf_ptr += sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
		       (param->tbl_len));
	buf_ptr += WMI_TLV_HDR_SIZE;
	gain_table = (uint32_t *)buf_ptr;

	WMI_HOST_IF_MSG_COPY_CHAR_ARRAY(gain_table,
					param->array_gain,
					param->tbl_len);

	wmi_mtrace(WMI_PDEV_SET_MIMOGAIN_TABLE_CMDID, NO_SESSION, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_PDEV_SET_MIMOGAIN_TABLE_CMDID)) {
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * enum packet_power_tlv_flags: target defined
 * packet power rate flags for TLV
 * @WMI_TLV_FLAG_ONE_CHAIN: one chain
 * @WMI_TLV_FLAG_TWO_CHAIN: two chain
 * @WMI_TLV_FLAG_THREE_CHAIN: three chain
 * @WMI_TLV_FLAG_FOUR_CHAIN: four chain
 * @WMI_TLV_FLAG_FIVE_CHAIN: five chain
 * @WMI_TLV_FLAG_SIX_CHAIN: six chain
 * @WMI_TLV_FLAG_SEVEN_CHAIN: seven chain
 * @WMI_TLV_FLAG_EIGHT_CHAIN:eight chain
 * @WMI_TLV_FLAG_STBC: STBC is set
 * @WMI_TLV_FLAG_40MHZ: 40MHz chan width
 * @WMI_TLV_FLAG_80MHZ: 80MHz chan width
 * @WMI_TLV_FLAG_160MHZ: 160MHz chan width
 * @WMI_TLV_FLAG_TXBF: Tx Bf enabled
 * @WMI_TLV_FLAG_RTSENA: RTS enabled
 * @WMI_TLV_FLAG_CTSENA: CTS enabled
 * @WMI_TLV_FLAG_LDPC: LDPC is set
 * @WMI_TLV_FLAG_SGI: Short gaurd interval
 * @WMI_TLV_FLAG_SU: SU Data
 * @WMI_TLV_FLAG_DL_MU_MIMO_AC: DL AC MU data
 * @WMI_TLV_FLAG_DL_MU_MIMO_AX: DL AX MU data
 * @WMI_TLV_FLAG_DL_OFDMA: DL OFDMA data
 * @WMI_TLV_FLAG_UL_OFDMA: UL OFDMA data
 * @WMI_TLV_FLAG_UL_MU_MIMO: UL MU data
 *
 * @WMI_TLV_FLAG_BW_MASK: bandwidth mask
 * @WMI_TLV_FLAG_BW_SHIFT: bandwidth shift
 * @WMI_TLV_FLAG_SU_MU_OFDMA_MASK: su/mu/ofdma mask
 * @WMI_TLV_FLAG_SU_MU_OFDMA_shift: su/mu/ofdma shift
 */
enum packet_power_tlv_flags {
	WMI_TLV_FLAG_ONE_CHAIN	 = 0x00000001,
	WMI_TLV_FLAG_TWO_CHAIN	 = 0x00000003,
	WMI_TLV_FLAG_THREE_CHAIN       = 0x00000007,
	WMI_TLV_FLAG_FOUR_CHAIN	= 0x0000000F,
	WMI_TLV_FLAG_FIVE_CHAIN	= 0x0000001F,
	WMI_TLV_FLAG_SIX_CHAIN	 = 0x0000003F,
	WMI_TLV_FLAG_SEVEN_CHAIN       = 0x0000007F,
	WMI_TLV_FLAG_EIGHT_CHAIN       = 0x0000008F,
	WMI_TLV_FLAG_STBC	      = 0x00000100,
	WMI_TLV_FLAG_40MHZ	     = 0x00000200,
	WMI_TLV_FLAG_80MHZ	     = 0x00000300,
	WMI_TLV_FLAG_160MHZ	    = 0x00000400,
	WMI_TLV_FLAG_TXBF	      = 0x00000800,
	WMI_TLV_FLAG_RTSENA	    = 0x00001000,
	WMI_TLV_FLAG_CTSENA	    = 0x00002000,
	WMI_TLV_FLAG_LDPC	      = 0x00004000,
	WMI_TLV_FLAG_SGI	       = 0x00008000,
	WMI_TLV_FLAG_SU		= 0x00100000,
	WMI_TLV_FLAG_DL_MU_MIMO_AC     = 0x00200000,
	WMI_TLV_FLAG_DL_MU_MIMO_AX     = 0x00300000,
	WMI_TLV_FLAG_DL_OFDMA	  = 0x00400000,
	WMI_TLV_FLAG_UL_OFDMA	  = 0x00500000,
	WMI_TLV_FLAG_UL_MU_MIMO	= 0x00600000,

	WMI_TLV_FLAG_CHAIN_MASK	= 0xff,
	WMI_TLV_FLAG_BW_MASK	   = 0x3,
	WMI_TLV_FLAG_BW_SHIFT	  = 9,
	WMI_TLV_FLAG_SU_MU_OFDMA_MASK  = 0x7,
	WMI_TLV_FLAG_SU_MU_OFDMA_SHIFT = 20,
};

/**
 * convert_to_power_info_rate_flags() - convert packet_power_info_params
 * to FW understandable format
 * @param: pointer to hold packet power info param
 *
 * @return FW understandable 32 bit rate flags
 */
static uint32_t
convert_to_power_info_rate_flags(struct packet_power_info_params *param)
{
	uint32_t rateflags = 0;

	if (param->chainmask)
		rateflags |=
			(param->chainmask & WMI_TLV_FLAG_CHAIN_MASK);
	if (param->chan_width)
		rateflags |=
			((param->chan_width & WMI_TLV_FLAG_BW_MASK)
			 << WMI_TLV_FLAG_BW_SHIFT);
	if (param->su_mu_ofdma)
		rateflags |=
			((param->su_mu_ofdma & WMI_TLV_FLAG_SU_MU_OFDMA_MASK)
			 << WMI_TLV_FLAG_SU_MU_OFDMA_SHIFT);
	if (param->rate_flags & WMI_HOST_FLAG_STBC)
		rateflags |= WMI_TLV_FLAG_STBC;
	if (param->rate_flags & WMI_HOST_FLAG_LDPC)
		rateflags |= WMI_TLV_FLAG_LDPC;
	if (param->rate_flags & WMI_HOST_FLAG_TXBF)
		rateflags |= WMI_TLV_FLAG_TXBF;
	if (param->rate_flags & WMI_HOST_FLAG_RTSENA)
		rateflags |= WMI_TLV_FLAG_RTSENA;
	if (param->rate_flags & WMI_HOST_FLAG_CTSENA)
		rateflags |= WMI_TLV_FLAG_CTSENA;
	if (param->rate_flags & WMI_HOST_FLAG_SGI)
		rateflags |= WMI_TLV_FLAG_SGI;

	return rateflags;
}

/**
 * send_packet_power_info_get_cmd_tlv() - send request to get packet power
 * info to fw
 * @wmi_handle: wmi handle
 * @param: pointer to hold packet power info param
 *
 * @return QDF_STATUS_SUCCESS  on success and -ve on failure.
 */
static QDF_STATUS
send_packet_power_info_get_cmd_tlv(wmi_unified_t wmi_handle,
				   struct packet_power_info_params *param)
{
	wmi_pdev_get_tpc_cmd_fixed_param *cmd;
	wmi_buf_t wmibuf;
	uint8_t *buf_ptr;
	u_int32_t len = sizeof(wmi_pdev_get_tpc_cmd_fixed_param);

	wmibuf = wmi_buf_alloc(wmi_handle, len);
	if (wmibuf == NULL)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(wmibuf);

	cmd = (wmi_pdev_get_tpc_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_pdev_get_tpc_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				wmi_pdev_get_tpc_cmd_fixed_param));
	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
								wmi_handle,
								param->pdev_id);
	cmd->rate_flags = convert_to_power_info_rate_flags(param);
	cmd->nss = param->nss;
	cmd->preamble = param->preamble;
	cmd->hw_rate = param->hw_rate;

	wmi_info("commandID %d, wmi_pdev_get_tpc_cmd=0x%x,"
		 "rate_flags: 0x%x, nss: %d, preamble: %d, hw_rate: %d",
		 WMI_PDEV_GET_TPC_CMDID, *((u_int32_t *)cmd),
		 cmd->rate_flags, cmd->nss, cmd->preamble, cmd->hw_rate);

	wmi_mtrace(WMI_PDEV_GET_TPC_CMDID, NO_SESSION, 0);
	if (wmi_unified_cmd_send(wmi_handle, wmibuf, len,
				 WMI_PDEV_GET_TPC_CMDID)) {
			wmi_err("Failed to get tpc command");
			wmi_buf_free(wmibuf);
			return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * send_get_halphy_cal_status_cmd_tlv() - send request to get halphy cal
 * status to fw
 * @wmi_handle: wmi handle
 * @param: pointer to hold halphy cal status param
 *
 * @return QDF_STATUS_SUCCESS  on success and -ve on failure.
 */
static QDF_STATUS
send_get_halphy_cal_status_cmd_tlv(wmi_unified_t wmi_handle,
				   struct halphy_cal_status_params *param)
{
	wmi_pdev_get_halphy_cal_status_cmd_fixed_param *cmd;
	wmi_buf_t wmibuf;
	uint8_t *buf_ptr;
	u_int32_t len = sizeof(wmi_pdev_get_halphy_cal_status_cmd_fixed_param);

	wmibuf = wmi_buf_alloc(wmi_handle, len);
	if (wmibuf == NULL)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(wmibuf);

	cmd = (wmi_pdev_get_halphy_cal_status_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_pdev_get_halphy_cal_status_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				wmi_pdev_get_halphy_cal_status_cmd_fixed_param));
	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
								wmi_handle,
								param->pdev_id);

	wmi_info("commandID %d, wmi_pdev_get_halphy_cal_status_cmd=0x%x",
		 WMI_PDEV_GET_HALPHY_CAL_STATUS_CMDID, *((u_int32_t *)cmd));

	wmi_mtrace(WMI_PDEV_GET_HALPHY_CAL_STATUS_CMDID, NO_SESSION, 0);
	if (wmi_unified_cmd_send(wmi_handle, wmibuf, len,
				 WMI_PDEV_GET_HALPHY_CAL_STATUS_CMDID)) {
		wmi_err("Failed to get halphy cal status command");
		wmi_buf_free(wmibuf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * send_nf_dbr_dbm_info_get_cmd_tlv() - send request to get nf to fw
 * @wmi_handle: wmi handle
 * @mac_id: radio context
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS
send_nf_dbr_dbm_info_get_cmd_tlv(wmi_unified_t wmi_handle, uint8_t mac_id)
{
	wmi_buf_t buf;
	QDF_STATUS ret;
	wmi_pdev_get_nfcal_power_fixed_param *cmd;
	int32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (buf == NULL)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_pdev_get_nfcal_power_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_pdev_get_nfcal_power_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
				(wmi_pdev_get_nfcal_power_fixed_param));
	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
								wmi_handle,
								mac_id);

	wmi_mtrace(WMI_PDEV_GET_NFCAL_POWER_CMDID, NO_SESSION, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_PDEV_GET_NFCAL_POWER_CMDID);
	if (ret != 0) {
		wmi_err("Sending get nfcal power cmd failed");
		wmi_buf_free(buf);
	}

	return ret;
}

/**
 * send_set_ht_ie_cmd_tlv() - send ht ie command to fw
 * @wmi_handle: wmi handle
 * @param: pointer to ht ie param
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS
send_set_ht_ie_cmd_tlv(wmi_unified_t wmi_handle,
		       struct ht_ie_params *param)
{
	wmi_pdev_set_ht_ie_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	QDF_STATUS ret;
	int32_t len;
	uint8_t *buf_ptr;

	len = sizeof(*cmd)  + WMI_TLV_HDR_SIZE +
	      roundup(param->ie_len, sizeof(uint32_t));

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_pdev_set_ht_ie_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_pdev_set_ht_ie_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				wmi_pdev_set_ht_ie_cmd_fixed_param));
	cmd->reserved0 = 0;
	cmd->ie_len = param->ie_len;
	cmd->tx_streams = param->tx_streams;
	cmd->rx_streams = param->rx_streams;

	buf_ptr += sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, cmd->ie_len);
	buf_ptr += WMI_TLV_HDR_SIZE;
	if (param->ie_len)
		WMI_HOST_IF_MSG_COPY_CHAR_ARRAY(buf_ptr, param->ie_data,
						cmd->ie_len);

	wmi_mtrace(WMI_PDEV_SET_HT_CAP_IE_CMDID, NO_SESSION, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_PDEV_SET_HT_CAP_IE_CMDID);

	if (ret != 0) {
		wmi_err("Sending set ht ie cmd failed");
		wmi_buf_free(buf);
	}

	return ret;
}

/**
 * send_set_vht_ie_cmd_tlv() - send vht ie command to fw
 * @wmi_handle: wmi handle
 * @param: pointer to vht ie param
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS
send_set_vht_ie_cmd_tlv(wmi_unified_t wmi_handle,
			struct vht_ie_params *param)
{
	wmi_pdev_set_vht_ie_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	QDF_STATUS ret;
	int32_t len;
	uint8_t *buf_ptr;

	len = sizeof(*cmd)  + WMI_TLV_HDR_SIZE +
	      roundup(param->ie_len, sizeof(uint32_t));

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_pdev_set_vht_ie_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_pdev_set_vht_ie_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				wmi_pdev_set_vht_ie_cmd_fixed_param));
	cmd->reserved0 = 0;
	cmd->ie_len = param->ie_len;
	cmd->tx_streams = param->tx_streams;
	cmd->rx_streams = param->rx_streams;

	buf_ptr += sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, cmd->ie_len);
	buf_ptr += WMI_TLV_HDR_SIZE;
	if (param->ie_len)
		WMI_HOST_IF_MSG_COPY_CHAR_ARRAY(buf_ptr, param->ie_data,
						cmd->ie_len);

	wmi_mtrace(WMI_PDEV_SET_VHT_CAP_IE_CMDID, NO_SESSION, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_PDEV_SET_VHT_CAP_IE_CMDID);

	if (ret != 0) {
		wmi_err("Sending set vht ie cmd failed");
		wmi_buf_free(buf);
	}

	return ret;
}

/**
 * send_set_quiet_mode_cmd_tlv() - send set quiet mode command to fw
 * @wmi_handle: wmi handle
 * @param: pointer to quiet mode params
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS
send_set_quiet_mode_cmd_tlv(wmi_unified_t wmi_handle,
			    struct set_quiet_mode_params *param)
{
	wmi_pdev_set_quiet_cmd_fixed_param *quiet_cmd;
	wmi_buf_t buf;
	QDF_STATUS ret;
	int32_t len;

	len = sizeof(*quiet_cmd);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}

	quiet_cmd = (wmi_pdev_set_quiet_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&quiet_cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_pdev_set_quiet_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				wmi_pdev_set_quiet_cmd_fixed_param));
	quiet_cmd = (wmi_pdev_set_quiet_cmd_fixed_param *)wmi_buf_data(buf);
	quiet_cmd->enabled = param->enabled;
	quiet_cmd->period = (param->period)*(param->intval);
	quiet_cmd->duration = param->duration;
	quiet_cmd->next_start = param->offset;
	quiet_cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
							wmi_handle,
							WMI_HOST_PDEV_ID_SOC);

	wmi_mtrace(WMI_PDEV_SET_QUIET_MODE_CMDID, NO_SESSION, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_PDEV_SET_QUIET_MODE_CMDID);

	if (ret != 0) {
		wmi_err("Sending set quiet cmd failed");
		wmi_buf_free(buf);
	}

	return ret;
}

/**
 * send_set_bcn_offload_quiet_mode_cmd_tlv() - send set quiet mode command to fw
 * @wmi_handle: wmi handle
 * @param: pointer to quiet mode params in bcn offload mode
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS
send_set_bcn_offload_quiet_mode_cmd_tlv(wmi_unified_t wmi_handle,
			    struct set_bcn_offload_quiet_mode_params *param)
{
	wmi_vdev_bcn_offload_quiet_config_cmd_fixed_param *quiet_cmd;
	wmi_buf_t buf;
	QDF_STATUS ret;
	int32_t len;
	uint8_t *buf_ptr;

	len = sizeof(*quiet_cmd);
	len += quiet_mlo_params_size(param);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	quiet_cmd = (wmi_vdev_bcn_offload_quiet_config_cmd_fixed_param *)
			wmi_buf_data(buf);
	WMITLV_SET_HDR(&quiet_cmd->tlv_header,
	    WMITLV_TAG_STRUC_wmi_vdev_bcn_offload_quiet_config_cmd_fixed_param,
	    WMITLV_GET_STRUCT_TLVLEN(
		wmi_vdev_bcn_offload_quiet_config_cmd_fixed_param));
	quiet_cmd = (wmi_vdev_bcn_offload_quiet_config_cmd_fixed_param *)
		wmi_buf_data(buf);
	quiet_cmd->vdev_id = param->vdev_id;
	quiet_cmd->period = param->period;
	quiet_cmd->duration = param->duration;
	quiet_cmd->next_start = param->next_start;
	quiet_cmd->flags = param->flag;

	buf_ptr += sizeof(wmi_vdev_bcn_offload_quiet_config_cmd_fixed_param);
	buf_ptr = bcn_offload_quiet_add_ml_partner_links(buf_ptr, param);

	wmi_mtrace(WMI_VDEV_BCN_OFFLOAD_QUIET_CONFIG_CMDID, NO_SESSION, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_VDEV_BCN_OFFLOAD_QUIET_CONFIG_CMDID);

	if (ret != 0) {
		wmi_err("Sending set quiet cmd failed");
		wmi_buf_free(buf);
	}

	return ret;
}

/**
 * send_bcn_offload_control_cmd_tlv - send beacon ofload control cmd to fw
 * @wmi_handle: wmi handle
 * @bcn_ctrl_param: pointer to bcn_offload_control param
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static
QDF_STATUS send_bcn_offload_control_cmd_tlv(wmi_unified_t wmi_handle,
			struct bcn_offload_control *bcn_ctrl_param)
{
	wmi_buf_t buf;
	wmi_bcn_offload_ctrl_cmd_fixed_param *cmd;
	QDF_STATUS ret;
	uint32_t len;

	len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		qdf_print("%s: wmi_buf_alloc failed", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	cmd = (wmi_bcn_offload_ctrl_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_bcn_offload_ctrl_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN
			(wmi_bcn_offload_ctrl_cmd_fixed_param));
	cmd->vdev_id = bcn_ctrl_param->vdev_id;
	switch (bcn_ctrl_param->bcn_ctrl_op) {
	case BCN_OFFLD_CTRL_TX_DISABLE:
		cmd->bcn_ctrl_op = WMI_BEACON_CTRL_TX_DISABLE;
		break;
	case BCN_OFFLD_CTRL_TX_ENABLE:
		cmd->bcn_ctrl_op = WMI_BEACON_CTRL_TX_ENABLE;
		break;
	case BCN_OFFLD_CTRL_SWBA_DISABLE:
		cmd->bcn_ctrl_op = WMI_BEACON_CTRL_SWBA_EVENT_DISABLE;
		break;
	case BCN_OFFLD_CTRL_SWBA_ENABLE:
		cmd->bcn_ctrl_op = WMI_BEACON_CTRL_SWBA_EVENT_ENABLE;
		break;
	default:
		wmi_err("WMI_BCN_OFFLOAD_CTRL_CMDID unknown CTRL Operation %d",
			bcn_ctrl_param->bcn_ctrl_op);
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
		break;
	}
	wmi_mtrace(WMI_BCN_OFFLOAD_CTRL_CMDID, cmd->vdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
			WMI_BCN_OFFLOAD_CTRL_CMDID);

	if (QDF_IS_STATUS_ERROR(ret)) {
		wmi_err("WMI_BCN_OFFLOAD_CTRL_CMDID send returned Error %d",
				ret);
		wmi_buf_free(buf);
	}

	return ret;
}

/**
 * extract_swba_num_vdevs_tlv() - extract swba num vdevs from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param num_vdevs: Pointer to hold num vdevs
 * @param num_quiet_active_vdevs: Pointer to hold number of Quiet active vdevs
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_swba_num_vdevs_tlv(wmi_unified_t wmi_handle,
	void *evt_buf, uint32_t *num_vdevs, uint32_t *num_quiet_active_vdevs)
{
	WMI_HOST_SWBA_EVENTID_param_tlvs *param_buf;
	wmi_host_swba_event_fixed_param *swba_event;
	uint32_t vdev_map;

	param_buf = (WMI_HOST_SWBA_EVENTID_param_tlvs *) evt_buf;
	if (!param_buf) {
		wmi_err("Invalid swba event buffer");
		return QDF_STATUS_E_INVAL;
	}

	swba_event = param_buf->fixed_param;
	*num_vdevs = swba_event->num_vdevs;
	if (!(*num_vdevs)) {
		vdev_map = swba_event->vdev_map;
		*num_vdevs = wmi_vdev_map_to_num_vdevs(vdev_map);
	}

	*num_quiet_active_vdevs = param_buf->num_quiet_offload_info;

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_swba_tim_info_tlv() - extract swba tim info from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param idx: Index to bcn info
 * @param tim_info: Pointer to hold tim info
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_swba_tim_info_tlv(wmi_unified_t wmi_handle,
	void *evt_buf, uint32_t idx, wmi_host_tim_info *tim_info)
{
	WMI_HOST_SWBA_EVENTID_param_tlvs *param_buf;
	wmi_tim_info *tim_info_ev;

	param_buf = (WMI_HOST_SWBA_EVENTID_param_tlvs *) evt_buf;
	if (!param_buf) {
		wmi_err("Invalid swba event buffer");
		return QDF_STATUS_E_INVAL;
	}

	tim_info_ev = &param_buf->tim_info[idx];

	if (tim_info_ev->tim_len != 0) {
		tim_info->tim_len = tim_info_ev->tim_len;
		tim_info->tim_mcast = tim_info_ev->tim_mcast;
		qdf_mem_copy(tim_info->tim_bitmap, tim_info_ev->tim_bitmap,
			     (sizeof(uint32_t) * WMI_TIM_BITMAP_ARRAY_SIZE));
		tim_info->tim_changed = tim_info_ev->tim_changed;
		tim_info->tim_num_ps_pending = tim_info_ev->tim_num_ps_pending;
		tim_info->vdev_id = tim_info_ev->vdev_id;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_swba_noa_info_tlv() - extract swba NoA information from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param idx: Index to bcn info
 * @param p2p_desc: Pointer to hold p2p NoA info
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_swba_noa_info_tlv(wmi_unified_t wmi_handle,
	void *evt_buf, uint32_t idx, wmi_host_p2p_noa_info *p2p_desc)
{
	WMI_HOST_SWBA_EVENTID_param_tlvs *param_buf;
	wmi_p2p_noa_info *p2p_noa_info;
	uint8_t i = 0;

	param_buf = (WMI_HOST_SWBA_EVENTID_param_tlvs *) evt_buf;
	if (!param_buf) {
		wmi_err("Invalid swba event buffer");
		return QDF_STATUS_E_INVAL;
	}

	p2p_noa_info = &param_buf->p2p_noa_info[idx];

	p2p_desc->modified = false;
	p2p_desc->num_descriptors = 0;
	if (WMI_UNIFIED_NOA_ATTR_IS_MODIFIED(p2p_noa_info)) {
		p2p_desc->modified = true;
		p2p_desc->index =
			(uint8_t) WMI_UNIFIED_NOA_ATTR_INDEX_GET(p2p_noa_info);
		p2p_desc->oppPS =
			(uint8_t) WMI_UNIFIED_NOA_ATTR_OPP_PS_GET(p2p_noa_info);
		p2p_desc->ctwindow =
			(uint8_t) WMI_UNIFIED_NOA_ATTR_CTWIN_GET(p2p_noa_info);
		p2p_desc->num_descriptors =
			(uint8_t) WMI_UNIFIED_NOA_ATTR_NUM_DESC_GET
							(p2p_noa_info);
		for (i = 0; i < p2p_desc->num_descriptors; i++) {
			p2p_desc->noa_descriptors[i].type_count =
				(uint8_t) p2p_noa_info->noa_descriptors[i].
				type_count;
			p2p_desc->noa_descriptors[i].duration =
				p2p_noa_info->noa_descriptors[i].duration;
			p2p_desc->noa_descriptors[i].interval =
				p2p_noa_info->noa_descriptors[i].interval;
			p2p_desc->noa_descriptors[i].start_time =
				p2p_noa_info->noa_descriptors[i].start_time;
		}
		p2p_desc->vdev_id = p2p_noa_info->vdev_id;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_swba_quiet_info_tlv() - extract swba quiet info from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param idx: Index to bcn info
 * @param quiet_info: Pointer to hold quiet info
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_swba_quiet_info_tlv(wmi_unified_t wmi_handle,
					      void *evt_buf, uint32_t idx,
					      wmi_host_quiet_info *quiet_info)
{
	WMI_HOST_SWBA_EVENTID_param_tlvs *param_buf;
	wmi_quiet_offload_info *quiet_info_ev;

	param_buf = (WMI_HOST_SWBA_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf) {
		wmi_err("Invalid swba event buffer");
		return QDF_STATUS_E_INVAL;
	}

	if (!param_buf->quiet_offload_info)
		return QDF_STATUS_E_NULL_VALUE;

	quiet_info_ev = &param_buf->quiet_offload_info[idx];

	quiet_info->vdev_id = quiet_info_ev->vdev_id;
	quiet_info->tbttcount = quiet_info_ev->tbttcount;
	quiet_info->period = quiet_info_ev->period;
	quiet_info->duration = quiet_info_ev->duration;
	quiet_info->offset = quiet_info_ev->offset;

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_offchan_data_tx_compl_param_tlv() -
 *	    extract Offchan data tx completion event params
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param param: Pointer to hold offchan data TX completion params
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_offchan_data_tx_compl_param_tlv(
		wmi_unified_t wmi_handle, void *evt_buf,
		struct wmi_host_offchan_data_tx_compl_event *param)
{
	WMI_OFFCHAN_DATA_TX_COMPLETION_EVENTID_param_tlvs *param_buf;
	wmi_offchan_data_tx_compl_event_fixed_param *cmpl_params;

	param_buf = (WMI_OFFCHAN_DATA_TX_COMPLETION_EVENTID_param_tlvs *)
		evt_buf;
	if (!param_buf) {
		wmi_err("Invalid offchan data Tx compl event");
		return QDF_STATUS_E_INVAL;
	}
	cmpl_params = param_buf->fixed_param;

	param->pdev_id = wmi_handle->ops->convert_pdev_id_target_to_host(
							wmi_handle,
							cmpl_params->pdev_id);
	param->desc_id = cmpl_params->desc_id;
	param->status = cmpl_params->status;

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_pdev_tpc_config_ev_param_tlv() - extract pdev tpc configuration
 * param from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param param: Pointer to hold tpc configuration
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS extract_pdev_tpc_config_ev_param_tlv(wmi_unified_t wmi_handle,
		void *evt_buf,
		wmi_host_pdev_tpc_config_event *param)
{
	wmi_pdev_tpc_config_event_fixed_param *event =
		(wmi_pdev_tpc_config_event_fixed_param *)evt_buf;

	if (!event) {
		wmi_err("Invalid event buffer");
		return QDF_STATUS_E_INVAL;
	}

	param->pdev_id = event->pdev_id;
	param->regDomain = event->regDomain;
	param->chanFreq = event->chanFreq;
	param->phyMode = event->phyMode;
	param->twiceAntennaReduction = event->twiceAntennaReduction;
	param->twiceAntennaGain = event->twiceAntennaGain;
	param->twiceMaxRDPower = event->twiceMaxRDPower;
	param->powerLimit = event->powerLimit;
	param->rateMax = event->rateMax;
	param->numTxChain = event->numTxChain;
	param->ctl = event->ctl;
	param->flags = event->flags;

	qdf_mem_copy(param->maxRegAllowedPower, event->maxRegAllowedPower,
		sizeof(param->maxRegAllowedPower));
	qdf_mem_copy(param->maxRegAllowedPowerAGCDD,
		event->maxRegAllowedPowerAGCDD,
		sizeof(param->maxRegAllowedPowerAGCDD));
	qdf_mem_copy(param->maxRegAllowedPowerAGSTBC,
		event->maxRegAllowedPowerAGSTBC,
		sizeof(param->maxRegAllowedPowerAGSTBC));
	qdf_mem_copy(param->maxRegAllowedPowerAGTXBF,
		event->maxRegAllowedPowerAGTXBF,
		sizeof(param->maxRegAllowedPowerAGTXBF));
	wmi_debug("extract success");

	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_RSSI_DB2DBM
/**
 * extract_pdev_rssi_dbm_conv_ev_param_tlv() - extract rssi_db2dbm event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param buf: Pointer to hold rssi_db2dbm param
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_pdev_rssi_dbm_conv_ev_param_tlv(
				wmi_unified_t wmi_handle,
				void *evt_buf,
				struct rssi_db2dbm_param *buf)
{
	WMI_PDEV_RSSI_DBM_CONVERSION_PARAMS_INFO_EVENTID_param_tlvs *param_buf;
	wmi_rssi_dbm_conversion_params_info_event_fixed_param *ev;
	wmi_rssi_dbm_conversion_params_info *rssi_info_buf;
	wmi_rssi_dbm_conversion_temp_offset_info *tmp_offset_buf;


	if (!evt_buf) {
		wmi_err("RSSI_DB2DBM event buffer is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	param_buf = (WMI_PDEV_RSSI_DBM_CONVERSION_PARAMS_INFO_EVENTID_param_tlvs *)evt_buf;
	ev = (wmi_rssi_dbm_conversion_params_info_event_fixed_param *)param_buf->fixed_param;
	buf->pdev_id = wmi_handle->ops->convert_pdev_id_target_to_host(wmi_handle,
								   ev->pdev_id);
	tmp_offset_buf = param_buf->rssi_temp_offset;

	if (tmp_offset_buf &&
	    (WMITLV_TAG_STRUC_wmi_rssi_dbm_conversion_temp_offset_info == WMITLV_GET_TLVTAG(tmp_offset_buf->tlv_header)) &&
	    (WMITLV_GET_STRUCT_TLVLEN(wmi_rssi_dbm_conversion_temp_offset_info) == WMITLV_GET_TLVLEN(tmp_offset_buf->tlv_header))) {
		qdf_mem_copy(&(buf->temp_off_param),
			     (((u_int8_t *)tmp_offset_buf) + WMI_TLV_HDR_SIZE),
			     sizeof(struct rssi_temp_off_param));
		buf->rssi_temp_off_present = true;
	}

	rssi_info_buf = param_buf->rssi_dbm_conversion_params;

	if (rssi_info_buf &&
	    (WMITLV_TAG_STRUC_wmi_rssi_dbm_conversion_params_info == WMITLV_GET_TLVTAG(rssi_info_buf->tlv_header)) &&
	    (WMITLV_GET_STRUCT_TLVLEN(wmi_rssi_dbm_conversion_params_info) == WMITLV_GET_TLVLEN(rssi_info_buf->tlv_header))) {
		qdf_mem_copy(&(buf->rssi_dbm_param),
			     (((u_int8_t *)rssi_info_buf) + WMI_TLV_HDR_SIZE),
			     sizeof(struct rssi_dbm_conv_param));
		buf->rssi_dbm_info_present = true;
		buf->rssi_dbm_param.curr_bw = wmi_map_ch_width(buf->rssi_dbm_param.curr_bw);
	}

	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * extract_peer_sta_kickout_ev_tlv() - extract peer sta kickout event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param ev: Pointer to hold peer param
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_peer_sta_kickout_ev_tlv(wmi_unified_t wmi_handle,
	void *evt_buf, wmi_host_peer_sta_kickout_event *ev)
{
	WMI_PEER_STA_KICKOUT_EVENTID_param_tlvs *param_buf = NULL;
	wmi_peer_sta_kickout_event_fixed_param *kickout_event = NULL;

	param_buf = (WMI_PEER_STA_KICKOUT_EVENTID_param_tlvs *) evt_buf;
	kickout_event = param_buf->fixed_param;

	WMI_MAC_ADDR_TO_CHAR_ARRAY(&kickout_event->peer_macaddr,
							ev->peer_macaddr);

	ev->reason = kickout_event->reason;
	ev->rssi = kickout_event->rssi;

	return QDF_STATUS_SUCCESS;
}

/**
 * send_multiple_vdev_restart_req_cmd_tlv() - send multiple vdev restart req
 * @wmi_handle: wmi handle
 * @param: wmi multiple vdev restart req param
 *
 * Send WMI_PDEV_MULTIPLE_VDEV_RESTART_REQUEST_CMDID parameters to fw.
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
static QDF_STATUS send_multiple_vdev_restart_req_cmd_tlv(
				wmi_unified_t wmi_handle,
				struct multiple_vdev_restart_params *param)
{
	wmi_buf_t buf;
	QDF_STATUS qdf_status;
	wmi_pdev_multiple_vdev_restart_request_cmd_fixed_param *cmd;
	int i;
	uint8_t *buf_ptr;
	uint32_t *vdev_ids, *phymode;
	uint32_t *preferred_tx_streams, *preferred_rx_streams;
	wmi_channel *chan_info;
	struct mlme_channel_param *tchan_info;
	uint16_t len = sizeof(*cmd);

	if (!param->num_vdevs) {
		wmi_err("vdev's not found for MVR cmd");
		qdf_status = QDF_STATUS_E_FAULT;
		goto end;
	}
	len += sizeof(wmi_channel);

	/* Add length of num_vdevs + tlv_hdr_size for vdev_id */
	len += sizeof(uint32_t) * param->num_vdevs + WMI_TLV_HDR_SIZE;
	/* Add length of num_vdevs + tlv_hdr_size for phymode */
	len += sizeof(uint32_t) * param->num_vdevs + WMI_TLV_HDR_SIZE;
	/* Add length of num_vdevs + tlv_hdr_size for Preferred TX Streams */
	len += sizeof(uint32_t) * param->num_vdevs + WMI_TLV_HDR_SIZE;
	/* Add length of num_vdevs + tlv_hdr_size for Preferred RX Streams */
	len += sizeof(uint32_t) * param->num_vdevs + WMI_TLV_HDR_SIZE;

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("Failed to allocate memory");
		qdf_status = QDF_STATUS_E_NOMEM;
		goto end;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_pdev_multiple_vdev_restart_request_cmd_fixed_param *)
	       buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
	WMITLV_TAG_STRUC_wmi_pdev_multiple_vdev_restart_request_cmd_fixed_param,
	WMITLV_GET_STRUCT_TLVLEN
		(wmi_pdev_multiple_vdev_restart_request_cmd_fixed_param));
	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
								wmi_handle,
								param->pdev_id);
	cmd->requestor_id = param->requestor_id;
	cmd->disable_hw_ack = param->disable_hw_ack;
	cmd->cac_duration_ms = param->cac_duration_ms;
	cmd->num_vdevs = param->num_vdevs;

	wmi_info("cmd->pdev_id: %d ,cmd->requestor_id: %d ,"
		 "cmd->disable_hw_ack: %d , cmd->cac_duration_ms:%d ,"
		 " cmd->num_vdevs: %d ",
		 cmd->pdev_id, cmd->requestor_id,
		 cmd->disable_hw_ack, cmd->cac_duration_ms, cmd->num_vdevs);
	buf_ptr += sizeof(*cmd);

	WMITLV_SET_HDR(buf_ptr,
		       WMITLV_TAG_ARRAY_UINT32,
		       sizeof(uint32_t) * param->num_vdevs);
	vdev_ids = (uint32_t *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for (i = 0; i < param->num_vdevs; i++)
		vdev_ids[i] = param->vdev_ids[i];

	buf_ptr += (sizeof(uint32_t) * param->num_vdevs) + WMI_TLV_HDR_SIZE;

	WMITLV_SET_HDR(buf_ptr,
		       WMITLV_TAG_STRUC_wmi_channel,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_channel));
	chan_info = (wmi_channel *)buf_ptr;
	tchan_info = &(param->ch_param);
	chan_info->mhz = tchan_info->mhz;
	chan_info->band_center_freq1 = tchan_info->cfreq1;
	chan_info->band_center_freq2 = tchan_info->cfreq2;
	if (tchan_info->is_chan_passive)
		WMI_SET_CHANNEL_FLAG(chan_info,
				     WMI_CHAN_FLAG_PASSIVE);
	if (tchan_info->dfs_set)
		WMI_SET_CHANNEL_FLAG(chan_info, WMI_CHAN_FLAG_DFS);

	if (tchan_info->dfs_set_cfreq2)
		WMI_SET_CHANNEL_FLAG(chan_info, WMI_CHAN_FLAG_DFS_CFREQ2);

	if (tchan_info->allow_vht)
		WMI_SET_CHANNEL_FLAG(chan_info,
				     WMI_CHAN_FLAG_ALLOW_VHT);
	else  if (tchan_info->allow_ht)
		WMI_SET_CHANNEL_FLAG(chan_info,
				     WMI_CHAN_FLAG_ALLOW_HT);
	WMI_SET_CHANNEL_MODE(chan_info, tchan_info->phy_mode);
	WMI_SET_CHANNEL_MIN_POWER(chan_info, tchan_info->minpower);
	WMI_SET_CHANNEL_MAX_POWER(chan_info, tchan_info->maxpower);
	WMI_SET_CHANNEL_REG_POWER(chan_info, tchan_info->maxregpower);
	WMI_SET_CHANNEL_ANTENNA_MAX(chan_info, tchan_info->antennamax);
	WMI_SET_CHANNEL_REG_CLASSID(chan_info, tchan_info->reg_class_id);
	WMI_SET_CHANNEL_MAX_TX_POWER(chan_info, tchan_info->maxregpower);

	wmi_info("tchan_info->is_chan_passive: %d ,"
		 "tchan_info->dfs_set : %d ,tchan_info->allow_vht:%d ,"
		 "tchan_info->allow_ht: %d ,tchan_info->antennamax: %d ,"
		 "tchan_info->phy_mode: %d ,tchan_info->minpower: %d,"
		 "tchan_info->maxpower: %d ,tchan_info->maxregpower: %d ,"
		 "tchan_info->reg_class_id: %d ,"
		 "tchan_info->maxregpower : %d ",
		 tchan_info->is_chan_passive, tchan_info->dfs_set,
		 tchan_info->allow_vht, tchan_info->allow_ht,
		 tchan_info->antennamax, tchan_info->phy_mode,
		 tchan_info->minpower, tchan_info->maxpower,
		 tchan_info->maxregpower, tchan_info->reg_class_id,
		 tchan_info->maxregpower);

	buf_ptr += sizeof(*chan_info);
	WMITLV_SET_HDR(buf_ptr,
		       WMITLV_TAG_ARRAY_UINT32,
		       sizeof(uint32_t) * param->num_vdevs);
	phymode = (uint32_t *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for (i = 0; i < param->num_vdevs; i++)
		WMI_MULTIPLE_VDEV_RESTART_FLAG_SET_PHYMODE(
				phymode[i], param->mvr_param[i].phymode);

	buf_ptr += (sizeof(uint32_t) * param->num_vdevs) + WMI_TLV_HDR_SIZE;
	WMITLV_SET_HDR(buf_ptr,
		       WMITLV_TAG_ARRAY_UINT32,
		       sizeof(uint32_t) * param->num_vdevs);
	preferred_tx_streams = (uint32_t *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for (i = 0; i < param->num_vdevs; i++)
		preferred_tx_streams[i] = param->mvr_param[i].preferred_tx_streams;


	buf_ptr += (sizeof(uint32_t) * param->num_vdevs) + WMI_TLV_HDR_SIZE;
	WMITLV_SET_HDR(buf_ptr,
		       WMITLV_TAG_ARRAY_UINT32,
		       sizeof(uint32_t) * param->num_vdevs);
	preferred_rx_streams = (uint32_t *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for (i = 0; i < param->num_vdevs; i++)
		preferred_rx_streams[i] = param->mvr_param[i].preferred_rx_streams;

	/* Target expects flag for phymode processing */
	WMI_MULTIPLE_VDEV_RESTART_FLAG_SET_PHYMODE_PRESENT(cmd->flags, 1);
	/*
	 * Target expects to be informed that MVR response is
	 * expected by host corresponding to this request.
	 */
	WMI_MULTIPLE_VDEV_RESTART_FLAG_SET_MVRR_EVENT_SUPPORT(cmd->flags, 1);

	wmi_mtrace(WMI_PDEV_MULTIPLE_VDEV_RESTART_REQUEST_CMDID, NO_SESSION, 0);
	qdf_status = wmi_unified_cmd_send(wmi_handle, buf, len,
				WMI_PDEV_MULTIPLE_VDEV_RESTART_REQUEST_CMDID);

	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		wmi_err("Failed to send");
		wmi_buf_free(buf);
	}

end:
	return qdf_status;
}

static QDF_STATUS send_multiple_vdev_set_param_cmd_tlv(
				wmi_unified_t wmi_handle,
				struct multiple_vdev_set_param *param)
{
	wmi_buf_t buf;
	QDF_STATUS qdf_status;
	wmi_pdev_multiple_vdev_set_param_cmd_fixed_param *cmd;
	int i;
	uint8_t *buf_ptr;
	uint32_t *vdev_ids;
	uint16_t len = sizeof(*cmd);

	if (!param->num_vdevs) {
		wmi_err("vdev's not found for multi vdev set cmd");
		qdf_status = QDF_STATUS_E_FAULT;
		goto end;
	}

	/* Add length of num_vdevs + tlv_hdr_size for vdev_id */
	len += sizeof(uint32_t) * param->num_vdevs + WMI_TLV_HDR_SIZE;

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("Failed to allocate memory");
		qdf_status = QDF_STATUS_E_NOMEM;
		goto end;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_pdev_multiple_vdev_set_param_cmd_fixed_param *)
	       buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
	WMITLV_TAG_STRUC_wmi_pdev_multiple_vdev_set_param_cmd_fixed_param,
	WMITLV_GET_STRUCT_TLVLEN
		(wmi_pdev_multiple_vdev_set_param_cmd_fixed_param));
	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
								wmi_handle,
								param->pdev_id);
	cmd->param_id = wmi_handle->ops->convert_host_vdev_param_tlv(
			param->param_id);
	cmd->param_value = param->param_value;

	buf_ptr += sizeof(*cmd);

	WMITLV_SET_HDR(buf_ptr,
		       WMITLV_TAG_ARRAY_UINT32,
		       sizeof(uint32_t) * param->num_vdevs);
	vdev_ids = (uint32_t *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for (i = 0; i < param->num_vdevs; i++) {
		vdev_ids[i] = param->vdev_ids[i];
	}

	wmi_mtrace(WMI_PDEV_MULTIPLE_VDEV_SET_PARAM_CMDID, NO_SESSION, 0);
	qdf_status = wmi_unified_cmd_send(wmi_handle, buf, len,
				WMI_PDEV_MULTIPLE_VDEV_SET_PARAM_CMDID);

	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		wmi_err("Failed to send");
		wmi_buf_free(buf);
	}

end:
	return qdf_status;
}
/**
 * extract_dcs_interference_type_tlv() - extract dcs interference type
 * from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param param: Pointer to hold dcs interference param
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS extract_dcs_interference_type_tlv(
		wmi_unified_t wmi_handle,
		void *evt_buf, struct wmi_host_dcs_interference_param *param)
{
	WMI_DCS_INTERFERENCE_EVENTID_param_tlvs *param_buf;

	param_buf = (WMI_DCS_INTERFERENCE_EVENTID_param_tlvs *) evt_buf;
	if (!param_buf)
		return QDF_STATUS_E_INVAL;

	param->interference_type = param_buf->fixed_param->interference_type;
	param->pdev_id = wmi_handle->ops->convert_pdev_id_target_to_host(
					wmi_handle,
					param_buf->fixed_param->pdev_id);

	return QDF_STATUS_SUCCESS;
}

/*
 * extract_dcs_awgn_info_tlv() - extract DCS AWGN interference from event
 * @wmi_handle      : wmi handle
 * @param evt_buf   : pointer to event buffer
 * @param awgn_info : Pointer to hold cw interference
 *
 * Return: QDF_STATUS_SUCCESS for success or QDF_STATUS_E_* for error
 */
static QDF_STATUS extract_dcs_awgn_info_tlv(wmi_unified_t wmi_handle,
				void *evt_buf,
				struct wmi_host_dcs_awgn_info *awgn_info)
{
	WMI_DCS_INTERFERENCE_EVENTID_param_tlvs *param_buf;
	wmi_dcs_awgn_int_t *ev;

	param_buf = (WMI_DCS_INTERFERENCE_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf)
		return QDF_STATUS_E_INVAL;

	ev = param_buf->awgn_int;

	awgn_info->channel_width       = ev->channel_width;
	awgn_info->center_freq         = ev->chan_freq;
	awgn_info->center_freq0        = ev->center_freq0;
	awgn_info->center_freq1        = ev->center_freq1;
	awgn_info->chan_bw_intf_bitmap = ev->chan_bw_interference_bitmap;
	wmi_debug("DCS AWGN extraction: channel_width: %u, center_freq: %u, "
		  "center_freq0: %u, center_freq1: %u, chan_bw_intf_bitmap: %x",
		  awgn_info->channel_width, awgn_info->channel_width,
		  awgn_info->center_freq0, awgn_info->center_freq1,
		  awgn_info->chan_bw_intf_bitmap);

	return QDF_STATUS_SUCCESS;
}

/*
 * extract_dcs_cw_int_tlv() - extract dcs cw interference from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param cw_int: Pointer to hold cw interference
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS extract_dcs_cw_int_tlv(wmi_unified_t wmi_handle,
		void *evt_buf,
		wmi_host_ath_dcs_cw_int *cw_int)
{
	WMI_DCS_INTERFERENCE_EVENTID_param_tlvs *param_buf;
	wlan_dcs_cw_int *ev;

	param_buf = (WMI_DCS_INTERFERENCE_EVENTID_param_tlvs *) evt_buf;
	if (!param_buf)
		return QDF_STATUS_E_INVAL;

	ev = param_buf->cw_int;

	cw_int->channel = ev->channel;

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_dcs_im_tgt_stats_tlv() - extract dcs im target stats from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param wlan_stat: Pointer to hold wlan stats
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS extract_dcs_im_tgt_stats_tlv(wmi_unified_t wmi_handle,
		void *evt_buf,
		wmi_host_dcs_im_tgt_stats_t *wlan_stat)
{
	WMI_DCS_INTERFERENCE_EVENTID_param_tlvs *param_buf;
	wlan_dcs_im_tgt_stats_t *ev;

	param_buf = (WMI_DCS_INTERFERENCE_EVENTID_param_tlvs *) evt_buf;
	if (!param_buf)
		return QDF_STATUS_E_INVAL;

	ev = param_buf->wlan_stat;
	wlan_stat->reg_tsf32 = ev->reg_tsf32;
	wlan_stat->last_ack_rssi = ev->last_ack_rssi;
	wlan_stat->tx_waste_time = ev->tx_waste_time;
	wlan_stat->rx_time = ev->rx_time;
	wlan_stat->phyerr_cnt = ev->phyerr_cnt;
	wlan_stat->mib_stats.listen_time = ev->listen_time;
	wlan_stat->mib_stats.reg_tx_frame_cnt = ev->reg_tx_frame_cnt;
	wlan_stat->mib_stats.reg_rx_frame_cnt = ev->reg_rx_frame_cnt;
	wlan_stat->mib_stats.reg_rxclr_cnt = ev->reg_rxclr_cnt;
	wlan_stat->mib_stats.reg_cycle_cnt = ev->reg_cycle_cnt;
	wlan_stat->mib_stats.reg_rxclr_ext_cnt = ev->reg_rxclr_ext_cnt;
	wlan_stat->mib_stats.reg_ofdm_phyerr_cnt = ev->reg_ofdm_phyerr_cnt;
	wlan_stat->mib_stats.reg_cck_phyerr_cnt = ev->reg_cck_phyerr_cnt;
	wlan_stat->chan_nf = ev->chan_nf;
	wlan_stat->my_bss_rx_cycle_count = ev->my_bss_rx_cycle_count;

	return QDF_STATUS_SUCCESS;
}

/*
 * extract_peer_create_response_event_tlv() - extract peer create response event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param vdev_id: Pointer to hold vdev_id
 * @param mac_addr: Pointer to hold peer mac address
 * @param status: Peer create status
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_peer_create_response_event_tlv(wmi_unified_t wmi_hdl,
	void *evt_buf, struct wmi_host_peer_create_response_event *param)
{
	WMI_PEER_CREATE_CONF_EVENTID_param_tlvs *param_buf;
	wmi_peer_create_conf_event_fixed_param *ev;

	param_buf = (WMI_PEER_CREATE_CONF_EVENTID_param_tlvs *)evt_buf;

	ev = (wmi_peer_create_conf_event_fixed_param *) param_buf->fixed_param;
	if (!ev) {
		wmi_err("Invalid peer_create response");
		return QDF_STATUS_E_FAILURE;
	}

	param->vdev_id = ev->vdev_id;
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&ev->peer_macaddr,
			&param->mac_address.bytes[0]);
	param->status = ev->status;

	return QDF_STATUS_SUCCESS;
}

/*
 * extract_peer_delete_response_event_tlv() - extract peer delete response event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param vdev_id: Pointer to hold vdev_id
 * @param mac_addr: Pointer to hold peer mac address
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_peer_delete_response_event_tlv(wmi_unified_t wmi_hdl,
	void *evt_buf, struct wmi_host_peer_delete_response_event *param)
{
	WMI_PEER_DELETE_RESP_EVENTID_param_tlvs *param_buf;
	wmi_peer_delete_resp_event_fixed_param *ev;

	param_buf = (WMI_PEER_DELETE_RESP_EVENTID_param_tlvs *)evt_buf;

	ev = (wmi_peer_delete_resp_event_fixed_param *) param_buf->fixed_param;
	if (!ev) {
		wmi_err("Invalid peer_delete response");
		return QDF_STATUS_E_FAILURE;
	}

	param->vdev_id = ev->vdev_id;
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&ev->peer_macaddr,
			&param->mac_address.bytes[0]);

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_pdev_tpc_ev_param_tlv() - extract tpc param from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param param: Pointer to hold tpc param
 *
 * @return QDF_STATUS_SUCCESS  on success and -ve on failure.
 */
static QDF_STATUS extract_pdev_tpc_ev_param_tlv(wmi_unified_t wmi_handle,
		void *evt_buf,
		wmi_host_pdev_tpc_event *param)
{
	WMI_PDEV_TPC_EVENTID_param_tlvs *param_buf;
	wmi_pdev_tpc_event_fixed_param *event;

	param_buf = (WMI_PDEV_TPC_EVENTID_param_tlvs *)evt_buf;
	event = (wmi_pdev_tpc_event_fixed_param *)param_buf->fixed_param;

	param->pdev_id = wmi_handle->ops->convert_pdev_id_target_to_host(
								wmi_handle,
								event->pdev_id);
	qdf_mem_copy(param->tpc, param_buf->tpc, sizeof(param->tpc));

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_nfcal_power_ev_param_tlv() - extract noise floor calibration
 * power param from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param param: Pointer to hold nf cal power param
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS
extract_nfcal_power_ev_param_tlv(wmi_unified_t wmi_handle,
			void *evt_buf,
			wmi_host_pdev_nfcal_power_all_channels_event *param)
{
	WMI_PDEV_NFCAL_POWER_ALL_CHANNELS_EVENTID_param_tlvs *param_buf;
	wmi_pdev_nfcal_power_all_channels_event_fixed_param *event;
	wmi_pdev_nfcal_power_all_channels_nfdBr *ch_nfdbr;
	wmi_pdev_nfcal_power_all_channels_nfdBm *ch_nfdbm;
	wmi_pdev_nfcal_power_all_channels_freqNum *ch_freqnum;
	uint32_t i;

	param_buf =
		(WMI_PDEV_NFCAL_POWER_ALL_CHANNELS_EVENTID_param_tlvs *)evt_buf;
	event = param_buf->fixed_param;
	ch_nfdbr = param_buf->nfdbr;
	ch_nfdbm = param_buf->nfdbm;
	ch_freqnum = param_buf->freqnum;

	wmi_debug("pdev_id[%x], num_nfdbr[%d], num_nfdbm[%d] num_freqnum[%d]",
		  event->pdev_id, param_buf->num_nfdbr,
		  param_buf->num_nfdbm, param_buf->num_freqnum);

	if (param_buf->num_nfdbr >
	    WMI_HOST_RXG_CAL_CHAN_MAX * WMI_HOST_MAX_NUM_CHAINS) {
		wmi_err("invalid number of nfdBr");
		return QDF_STATUS_E_FAILURE;
	}

	if (param_buf->num_nfdbm >
	    WMI_HOST_RXG_CAL_CHAN_MAX * WMI_HOST_MAX_NUM_CHAINS) {
		wmi_err("invalid number of nfdBm");
		return QDF_STATUS_E_FAILURE;
	}

	if (param_buf->num_freqnum > WMI_HOST_RXG_CAL_CHAN_MAX) {
		wmi_err("invalid number of freqNum");
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < param_buf->num_nfdbr; i++) {
		param->nfdbr[i] = (int8_t)ch_nfdbr->nfdBr;
		param->nfdbm[i] = (int8_t)ch_nfdbm->nfdBm;
		ch_nfdbr++;
		ch_nfdbm++;
	}
	param->num_nfdbr_dbm = (uint16_t)param_buf->num_nfdbr;

	for (i = 0; i < param_buf->num_freqnum; i++) {
		param->freqnum[i] = ch_freqnum->freqNum;
		ch_freqnum++;
	}
	param->num_freq = (uint16_t)param_buf->num_freqnum;

	param->pdev_id = wmi_handle->ops->
		convert_pdev_id_target_to_host(wmi_handle,
					       event->pdev_id);

	return QDF_STATUS_SUCCESS;
}

#ifdef BIG_ENDIAN_HOST
/**
 * wds_addr_ev_conv_data_be() - LE to BE conversion of wds addr event
 * @param data_len - data length
 * @param data - pointer to data
 *
 * Return: QDF_STATUS - success or error status
 */
static QDF_STATUS wds_addr_ev_conv_data_be(uint16_t data_len, uint8_t *ev)
{
	uint8_t *datap = (uint8_t *)ev;
	int i;
	/* Skip swapping the first word */
	datap += sizeof(uint32_t);
	for (i = 0; i < ((data_len / sizeof(uint32_t))-1);
			i++, datap += sizeof(uint32_t)) {
		*(uint32_t *)datap = qdf_le32_to_cpu(*(uint32_t *)datap);
	}

	return QDF_STATUS_SUCCESS;
}
#else
/**
 * wds_addr_ev_conv_data_be() - Dummy operation for LE platforms
 * @param data_len - data length
 * @param data - pointer to data
 *
 * Return: QDF_STATUS - success or error status
 */
static QDF_STATUS wds_addr_ev_conv_data_be(uint32_t data_len, uint8_t *ev)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * extract_wds_addr_event_tlv() - extract wds address from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param wds_ev: Pointer to hold wds address
 *
 * @return QDF_STATUS_SUCCESS  on success and -ve on failure.
 */
static QDF_STATUS extract_wds_addr_event_tlv(wmi_unified_t wmi_handle,
		void *evt_buf,
		uint16_t len, wds_addr_event_t *wds_ev)
{
	WMI_WDS_PEER_EVENTID_param_tlvs *param_buf;
	wmi_wds_addr_event_fixed_param *ev;
	int i;

	param_buf = (WMI_WDS_PEER_EVENTID_param_tlvs *)evt_buf;
	ev = (wmi_wds_addr_event_fixed_param *)param_buf->fixed_param;

	if (wds_addr_ev_conv_data_be(len, (uint8_t *)ev) != QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_FAILURE;

	qdf_mem_copy(wds_ev->event_type, ev->event_type,
		     sizeof(wds_ev->event_type));
	for (i = 0; i < 4; i++) {
		wds_ev->peer_mac[i] =
			((u_int8_t *)&(ev->peer_mac.mac_addr31to0))[i];
		wds_ev->dest_mac[i] =
			((u_int8_t *)&(ev->dest_mac.mac_addr31to0))[i];
	}
	for (i = 0; i < 2; i++) {
		wds_ev->peer_mac[4+i] =
			((u_int8_t *)&(ev->peer_mac.mac_addr47to32))[i];
		wds_ev->dest_mac[4+i] =
			((u_int8_t *)&(ev->dest_mac.mac_addr47to32))[i];
	}
	wds_ev->vdev_id = ev->vdev_id;

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_peer_sta_ps_statechange_ev_tlv() - extract peer sta ps state
 * from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param ev: Pointer to hold peer param and ps state
 *
 * @return QDF_STATUS_SUCCESS  on success and -ve on failure.
 */
static QDF_STATUS extract_peer_sta_ps_statechange_ev_tlv(wmi_unified_t wmi_handle,
		void *evt_buf, wmi_host_peer_sta_ps_statechange_event *ev)
{
	WMI_PEER_STA_PS_STATECHG_EVENTID_param_tlvs *param_buf;
	wmi_peer_sta_ps_statechange_event_fixed_param *event;

	param_buf = (WMI_PEER_STA_PS_STATECHG_EVENTID_param_tlvs *)evt_buf;
	event = (wmi_peer_sta_ps_statechange_event_fixed_param *)
						param_buf->fixed_param;

	WMI_MAC_ADDR_TO_CHAR_ARRAY(&event->peer_macaddr, ev->peer_macaddr);
	ev->peer_ps_state = event->peer_ps_state;

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_inst_rssi_stats_event_tlv() - extract inst rssi stats from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param inst_rssi_resp: Pointer to hold inst rssi response
 *
 * @return QDF_STATUS_SUCCESS  on success and -ve on failure.
 */
static QDF_STATUS extract_inst_rssi_stats_event_tlv(
	wmi_unified_t wmi_handle, void *evt_buf,
	wmi_host_inst_stats_resp *inst_rssi_resp)
{
	WMI_INST_RSSI_STATS_EVENTID_param_tlvs *param_buf;
	wmi_inst_rssi_stats_resp_fixed_param *event;

	param_buf = (WMI_INST_RSSI_STATS_EVENTID_param_tlvs *)evt_buf;
	event = (wmi_inst_rssi_stats_resp_fixed_param *)param_buf->fixed_param;

	qdf_mem_copy(&(inst_rssi_resp->peer_macaddr),
		     &(event->peer_macaddr), sizeof(wmi_mac_addr));
	inst_rssi_resp->iRSSI = event->iRSSI;

	return QDF_STATUS_SUCCESS;
}

/**
 * send_pdev_caldata_version_check_cmd_tlv() - send caldata check cmd to fw
 * @wmi_handle: wmi handle
 * @param:	reserved param
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS
send_pdev_caldata_version_check_cmd_tlv(wmi_unified_t wmi_handle,
						uint32_t param)
{
	wmi_pdev_check_cal_version_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(wmi_pdev_check_cal_version_cmd_fixed_param);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		qdf_print("%s:wmi_buf_alloc failed", __func__);
		return QDF_STATUS_E_FAILURE;
	}
	cmd = (wmi_pdev_check_cal_version_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_pdev_check_cal_version_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN
			(wmi_pdev_check_cal_version_cmd_fixed_param));
	cmd->pdev_id = param; /* set to 0x0 as expected from FW */
	wmi_mtrace(WMI_PDEV_CHECK_CAL_VERSION_CMDID, NO_SESSION, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
			WMI_PDEV_CHECK_CAL_VERSION_CMDID)) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_pdev_caldata_version_check_ev_param_tlv() - extract caldata from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param param: Pointer to hold peer caldata version data
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS extract_pdev_caldata_version_check_ev_param_tlv(
			wmi_unified_t wmi_handle,
			void *evt_buf,
			wmi_host_pdev_check_cal_version_event *param)
{
	WMI_PDEV_CHECK_CAL_VERSION_EVENTID_param_tlvs *param_tlvs;
	wmi_pdev_check_cal_version_event_fixed_param *event;

	param_tlvs = (WMI_PDEV_CHECK_CAL_VERSION_EVENTID_param_tlvs *) evt_buf;
	if (!param_tlvs) {
		wmi_err("invalid cal version event buf");
		return QDF_STATUS_E_FAILURE;
	}
	event =  param_tlvs->fixed_param;
	if (event->board_mcn_detail[WMI_BOARD_MCN_STRING_MAX_SIZE] != '\0')
		event->board_mcn_detail[WMI_BOARD_MCN_STRING_MAX_SIZE] = '\0';
	WMI_HOST_IF_MSG_COPY_CHAR_ARRAY(param->board_mcn_detail,
			event->board_mcn_detail, WMI_BOARD_MCN_STRING_BUF_SIZE);

	param->software_cal_version = event->software_cal_version;
	param->board_cal_version = event->board_cal_version;
	param->cal_ok  = event->cal_status;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
/**
 * set_rx_pkt_type_routing_tag_update_tlv() - add/delete protocol tag in CCE
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param param: pointer to protocol tag/routing struct
 *
 * @return:QDF_STATUS_SUCCESS for success or
 *			QDF_STATUS_E_NOMEM/QDF_STATUS_E_FAILURE on failure
*/
static QDF_STATUS set_rx_pkt_type_routing_tag_update_tlv(
			wmi_unified_t wmi_hdl,
			struct wmi_rx_pkt_protocol_routing_info *param)
{
	wmi_pdev_update_pkt_routing_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	QDF_STATUS status;
	uint32_t len = sizeof(wmi_pdev_update_pkt_routing_cmd_fixed_param);

	buf = wmi_buf_alloc(wmi_hdl, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_NOMEM;
	}
	cmd = (wmi_pdev_update_pkt_routing_cmd_fixed_param *)wmi_buf_data(buf);
		WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_pdev_update_pkt_routing_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
	wmi_pdev_update_pkt_routing_cmd_fixed_param));
	cmd->pdev_id = wmi_hdl->ops->convert_pdev_id_host_to_target(wmi_hdl,
							param->pdev_id);
	cmd->op_code = (A_UINT32) param->op_code;
	cmd->routing_type_bitmap = param->routing_type_bitmap;
	cmd->dest_ring = param->dest_ring;
	cmd->meta_data = param->meta_data;
	cmd->dest_ring_handler = param->dest_ring_handler;
	wmi_info("Set RX PKT ROUTING TYPE TAG - opcode: %u", param->op_code);
	wmi_info("routing_bitmap: %u, dest_ring: %u",
		 param->routing_type_bitmap, param->dest_ring);
	wmi_info("dest_ring_handler: %u, meta_data: 0x%x",
		 param->dest_ring_handler, param->meta_data);

	wmi_mtrace(WMI_PDEV_UPDATE_PKT_ROUTING_CMDID, cmd->pdev_id, 0);
	status = wmi_unified_cmd_send(wmi_hdl, buf, len,
				      WMI_PDEV_UPDATE_PKT_ROUTING_CMDID);
	if (status != QDF_STATUS_SUCCESS) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG */

/**
 * send_peer_vlan_config_cmd_tlv() - Send PEER vlan hw acceleration cmd to fw
 * @wmi: wmi handle
 * @peer_addr: peer mac addr
 * @param: struct peer_vlan_config_param *
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS send_peer_vlan_config_cmd_tlv(wmi_unified_t wmi,
					uint8_t peer_addr[QDF_MAC_ADDR_SIZE],
					struct peer_vlan_config_param *param)
{
	wmi_peer_config_vlan_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_peer_config_vlan_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_config_vlan_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
			       (wmi_peer_config_vlan_cmd_fixed_param));
	WMI_CHAR_ARRAY_TO_MAC_ADDR(peer_addr, &cmd->peer_macaddr);

	/* vdev id */
	cmd->vdev_id = param->vdev_id;

	/* Tx command - Check if cmd is Tx then configure Tx cmd */
	if (param->tx_cmd) {
		WMI_VLAN_TX_SET(cmd->peer_vlan_config_mask, param->tx_cmd);

		/* Setting insert_or_strip bit for Tx */
		WMI_TX_INSERT_OR_STRIP_SET(cmd->peer_vlan_config_mask,
					   param->tx_strip_insert);

		if (param->tx_strip_insert_inner && param->tx_strip_insert) {
		/* Setting the strip_insert_vlan_inner bit fo Tx */
			WMI_TX_STRIP_INSERT_VLAN_INNER_SET(cmd->peer_vlan_config_mask,
				param->tx_strip_insert_inner);
		/* If Insert inner tag bit is set, then fill inner_tci */
			WMI_TX_INSERT_VLAN_INNER_TCI_SET(cmd->insert_vlan_tci,
						param->insert_vlan_inner_tci);
		}

		if (param->tx_strip_insert_outer && param->tx_strip_insert) {
			/* Setting the strip_insert_vlan_outer bit fo Tx */
			WMI_TX_STRIP_INSERT_VLAN_OUTER_SET(cmd->peer_vlan_config_mask,
					param->tx_strip_insert_outer);
			/* If Insert outer tag bit is set, then fill outer_tci */
			WMI_TX_INSERT_VLAN_OUTER_TCI_SET(cmd->insert_vlan_tci,
						param->insert_vlan_outer_tci);
		}
	}

	/* Rx command - Check if cmd is Rx then configure Rx cmd */
	if (param->rx_cmd) {
		WMI_VLAN_RX_SET(cmd->peer_vlan_config_mask, param->rx_cmd);

		/* Setting the strip_vlan_c_tag_decap bit in RX */
		WMI_RX_STRIP_VLAN_C_TAG_SET(cmd->peer_vlan_config_mask,
				param->rx_strip_c_tag);

		/* Setting the strip_vlan_s_tag_decap bit in RX */
		WMI_RX_STRIP_VLAN_S_TAG_SET(cmd->peer_vlan_config_mask,
					    param->rx_strip_s_tag);

		/* Setting the insert_vlan_c_tag_decap bit in RX */
		WMI_RX_INSERT_VLAN_C_TAG_SET(cmd->peer_vlan_config_mask,
					     param->rx_insert_c_tag);

		/* Setting the insert_vlan_s_tag_decap bit in RX */
		WMI_RX_INSERT_VLAN_S_TAG_SET(cmd->peer_vlan_config_mask,
					     param->rx_insert_s_tag);
	}

	if (wmi_unified_cmd_send(wmi, buf, len, WMI_PEER_CONFIG_VLAN_CMDID)) {
		wmi_err("Failed to send peer hw vlan acceleration command");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_SUPPORT_FILS
/**
 * send_vdev_fils_enable_cmd_tlv() - enable/Disable FD Frame command to fw
 * @wmi_handle: wmi handle
 * @param: pointer to hold FILS discovery enable param
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE on failure
 */
static QDF_STATUS
send_vdev_fils_enable_cmd_tlv(wmi_unified_t wmi_handle,
			      struct config_fils_params *param)
{
	wmi_enable_fils_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	QDF_STATUS status;
	uint32_t len = sizeof(wmi_enable_fils_cmd_fixed_param);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_NOMEM;
	}
	cmd = (wmi_enable_fils_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_enable_fils_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
		       wmi_enable_fils_cmd_fixed_param));
	cmd->vdev_id = param->vdev_id;
	cmd->fd_period = param->fd_period;
	cmd->flags = param->send_prb_rsp_frame;
	wmi_info("Setting FD period to %d vdev id : %d",
		 param->fd_period, param->vdev_id);

	wmi_mtrace(WMI_ENABLE_FILS_CMDID, cmd->vdev_id, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf, len,
				      WMI_ENABLE_FILS_CMDID);
	if (status != QDF_STATUS_SUCCESS) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_swfda_vdev_id_tlv() - extract swfda vdev id from event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @vdev_id: pointer to hold vdev id
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_INVAL on failure
 */
static QDF_STATUS
extract_swfda_vdev_id_tlv(wmi_unified_t wmi_handle,
			  void *evt_buf, uint32_t *vdev_id)
{
	WMI_HOST_SWFDA_EVENTID_param_tlvs *param_buf;
	wmi_host_swfda_event_fixed_param *swfda_event;

	param_buf = (WMI_HOST_SWFDA_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf) {
		wmi_err("Invalid swfda event buffer");
		return QDF_STATUS_E_INVAL;
	}
	swfda_event = param_buf->fixed_param;
	*vdev_id = swfda_event->vdev_id;

	return QDF_STATUS_SUCCESS;
}

/**
 * send_fils_discovery_send_cmd_tlv() - WMI FILS Discovery send function
 * @wmi_handle: wmi handle
 * @param: pointer to hold FD send cmd parameter
 *
 * Return : QDF_STATUS_SUCCESS on success and QDF_STATUS_E_NOMEM on failure.
 */
static QDF_STATUS
send_fils_discovery_send_cmd_tlv(wmi_unified_t wmi_handle,
				 struct fd_params *param)
{
	QDF_STATUS ret;
	wmi_fd_send_from_host_cmd_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	qdf_dma_addr_t dma_addr;

	wmi_buf = wmi_buf_alloc(wmi_handle, sizeof(*cmd));
	if (!wmi_buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_NOMEM;
	}
	cmd = (wmi_fd_send_from_host_cmd_fixed_param *)wmi_buf_data(wmi_buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_fd_send_from_host_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
		       wmi_fd_send_from_host_cmd_fixed_param));
	cmd->vdev_id = param->vdev_id;
	cmd->data_len = qdf_nbuf_len(param->wbuf);
	dma_addr = qdf_nbuf_get_frag_paddr(param->wbuf, 0);
	qdf_dmaaddr_to_32s(dma_addr, &cmd->frag_ptr_lo, &cmd->frag_ptr_hi);
	cmd->frame_ctrl = param->frame_ctrl;

	wmi_mtrace(WMI_PDEV_SEND_FD_CMDID, cmd->vdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, wmi_buf, sizeof(*cmd),
				   WMI_PDEV_SEND_FD_CMDID);
	if (ret != QDF_STATUS_SUCCESS) {
		wmi_err("Failed to send fils discovery frame: %d", ret);
		wmi_buf_free(wmi_buf);
	}

	return ret;
}
#endif /* WLAN_SUPPORT_FILS */

/**
 * send_mcast_group_update_cmd_tlv() - send mcast group update cmd to fw
 * @wmi_handle: wmi handle
 * @param: pointer to hold mcast update param
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS
send_mcast_group_update_cmd_tlv(wmi_unified_t wmi_handle,
				struct mcast_group_update_params *param)
{
	wmi_peer_mcast_group_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	QDF_STATUS ret;
	int32_t len;
	int offset = 0;
	static char dummymask[4] = { 0xFF, 0xFF, 0xFF, 0xFF};

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}
	cmd = (wmi_peer_mcast_group_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_mcast_group_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				wmi_peer_mcast_group_cmd_fixed_param));
	/* confirm the buffer is 4-byte aligned */
	QDF_ASSERT((((size_t) cmd) & 0x3) == 0);
	qdf_mem_zero(cmd, sizeof(*cmd));

	cmd->vdev_id = param->vap_id;
	/* construct the message assuming our endianness matches the target */
	cmd->flags |= WMI_PEER_MCAST_GROUP_FLAG_ACTION_M &
		(param->action << WMI_PEER_MCAST_GROUP_FLAG_ACTION_S);
	cmd->flags |= WMI_PEER_MCAST_GROUP_FLAG_WILDCARD_M &
		(param->wildcard << WMI_PEER_MCAST_GROUP_FLAG_WILDCARD_S);
	if (param->is_action_delete)
		cmd->flags |= WMI_PEER_MCAST_GROUP_FLAG_DELETEALL_M;

	if (param->is_mcast_addr_len)
		cmd->flags |=  WMI_PEER_MCAST_GROUP_FLAG_IPV6_M;

	if (param->is_filter_mode_snoop)
		cmd->flags |= WMI_PEER_MCAST_GROUP_FLAG_SRC_FILTER_EXCLUDE_M;

	/* unicast address spec only applies for non-wildcard cases */
	if (!param->wildcard && param->ucast_mac_addr) {
		WMI_CHAR_ARRAY_TO_MAC_ADDR(param->ucast_mac_addr,
					   &cmd->ucast_mac_addr);
	}

	if (param->mcast_ip_addr) {
		QDF_ASSERT(param->mcast_ip_addr_bytes <=
			   sizeof(cmd->mcast_ip_addr));
		offset = sizeof(cmd->mcast_ip_addr) -
			 param->mcast_ip_addr_bytes;
		qdf_mem_copy(((uint8_t *)&cmd->mcast_ip_addr) + offset,
			     param->mcast_ip_addr,
			     param->mcast_ip_addr_bytes);
	}
	if (!param->mask)
		param->mask = &dummymask[0];

	qdf_mem_copy(((uint8_t *)&cmd->mcast_ip_mask) + offset,
		     param->mask,
		     param->mcast_ip_addr_bytes);

	if (param->srcs && param->nsrcs) {
		cmd->num_filter_addr = param->nsrcs;
		QDF_ASSERT((param->nsrcs * param->mcast_ip_addr_bytes) <=
			sizeof(cmd->filter_addr));

		qdf_mem_copy(((uint8_t *) &cmd->filter_addr), param->srcs,
			     param->nsrcs * param->mcast_ip_addr_bytes);
	}

	wmi_mtrace(WMI_PEER_MCAST_GROUP_CMDID, cmd->vdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				      WMI_PEER_MCAST_GROUP_CMDID);

	if (ret != QDF_STATUS_SUCCESS) {
		wmi_err("WMI Failed");
		wmi_buf_free(buf);
	}

	return ret;
}

/**
 * send_pdev_qvit_cmd_tlv() - send qvit command to fw
 * @wmi_handle: wmi handle
 * @param: pointer to pdev_qvit_params
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS
send_pdev_qvit_cmd_tlv(wmi_unified_t wmi_handle,
		       struct pdev_qvit_params *param)
{
	wmi_buf_t buf;
	QDF_STATUS ret = QDF_STATUS_E_INVAL;
	uint8_t *cmd;
	static uint8_t msgref = 1;
	uint8_t segnumber = 0, seginfo, numsegments;
	uint16_t chunk_len, total_bytes;
	uint8_t *bufpos;
	QVIT_SEG_HDR_INFO_STRUCT seghdrinfo;

	bufpos = param->utf_payload;
	total_bytes = param->len;
	ASSERT(total_bytes / MAX_WMI_QVIT_LEN ==
	       (uint8_t) (total_bytes / MAX_WMI_QVIT_LEN));
	numsegments = (uint8_t) (total_bytes / MAX_WMI_QVIT_LEN);

	if (param->len - (numsegments * MAX_WMI_QVIT_LEN))
		numsegments++;

	while (param->len) {
		if (param->len > MAX_WMI_QVIT_LEN)
			chunk_len = MAX_WMI_QVIT_LEN;    /* MAX message */
		else
			chunk_len = param->len;

		buf = wmi_buf_alloc(wmi_handle,
				    (chunk_len + sizeof(seghdrinfo) +
				     WMI_TLV_HDR_SIZE));
		if (!buf) {
			wmi_err("wmi_buf_alloc failed");
			return QDF_STATUS_E_NOMEM;
		}

		cmd = (uint8_t *) wmi_buf_data(buf);

		seghdrinfo.len = total_bytes;
		seghdrinfo.msgref = msgref;
		seginfo = ((numsegments << 4) & 0xF0) | (segnumber & 0xF);
		seghdrinfo.segmentInfo = seginfo;

		segnumber++;

		WMITLV_SET_HDR(cmd, WMITLV_TAG_ARRAY_BYTE,
			       (chunk_len + sizeof(seghdrinfo)));
		cmd += WMI_TLV_HDR_SIZE;
		qdf_mem_copy(cmd, &seghdrinfo, sizeof(seghdrinfo));
		qdf_mem_copy(&cmd[sizeof(seghdrinfo)], bufpos, chunk_len);

		wmi_mtrace(WMI_PDEV_QVIT_CMDID, NO_SESSION, 0);
		ret = wmi_unified_cmd_send(wmi_handle, buf,
					   (chunk_len + sizeof(seghdrinfo) +
					    WMI_TLV_HDR_SIZE),
					   WMI_PDEV_QVIT_CMDID);

		if (ret != 0) {
			wmi_err("Failed to send WMI_PDEV_QVIT_CMDID command");
			wmi_buf_free(buf);
			break;
		}

		param->len -= chunk_len;
		bufpos += chunk_len;
	}
	msgref++;

	return ret;
}

/**
 * send_wmm_update_cmd_tlv() - send wmm update command to fw
 * @wmi_handle: wmi handle
 * @param: pointer to wmm update param
 *
 * Return: 0 for success or error code
 */
static QDF_STATUS
send_wmm_update_cmd_tlv(wmi_unified_t wmi_handle,
			struct wmm_update_params *param)
{
	wmi_pdev_set_wmm_params_cmd_fixed_param *cmd;
	wmi_wmm_params *wmm_param;
	wmi_buf_t buf;
	QDF_STATUS ret;
	int32_t len;
	int ac = 0;
	struct wmi_host_wmeParams *wmep;
	uint8_t *buf_ptr;

	len = sizeof(*cmd) + (WME_NUM_AC * sizeof(*wmm_param));
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}

	buf_ptr = (uint8_t *) wmi_buf_data(buf);
	cmd = (wmi_pdev_set_wmm_params_cmd_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_pdev_set_wmm_params_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
			       (wmi_pdev_set_wmm_params_cmd_fixed_param));

	cmd->reserved0 = WMI_HOST_PDEV_ID_SOC;

	buf_ptr += sizeof(wmi_pdev_set_wmm_params_cmd_fixed_param);

	for (ac = 0; ac < WME_NUM_AC; ac++) {
		wmep = &param->wmep_array[ac];
		wmm_param = (wmi_wmm_params *)buf_ptr;
		WMITLV_SET_HDR(&wmm_param->tlv_header,
			WMITLV_TAG_STRUC_wmi_wmm_params,
			WMITLV_GET_STRUCT_TLVLEN(wmi_wmm_params));
		wmm_param->aifs = wmep->wmep_aifsn;
		wmm_param->cwmin = ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmin);
		wmm_param->cwmax = ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmax);
		wmm_param->txoplimit = ATH_TXOP_TO_US(wmep->wmep_txopLimit);
		wmm_param->acm = wmep->wmep_acm;
		wmm_param->no_ack = wmep->wmep_noackPolicy;
		buf_ptr += sizeof(wmi_wmm_params);
	}
	wmi_mtrace(WMI_PDEV_SET_WMM_PARAMS_CMDID, NO_SESSION, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_PDEV_SET_WMM_PARAMS_CMDID);

	if (ret != 0) {
		wmi_err("Sending WMM update CMD failed");
		wmi_buf_free(buf);
	}

	return ret;
}

#define WMI_TSF_SHIFT_UPPER 32
#define WMI_TSF_MASK_UPPER_32 0xFFFFFFFF00000000

/**
 * extract_mgmt_tx_compl_param_tlv() - extract MGMT tx completion event params
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param param: Pointer to hold MGMT TX completion params
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_mgmt_tx_compl_param_tlv(wmi_unified_t wmi_handle,
	void *evt_buf, wmi_host_mgmt_tx_compl_event *param)
{
	WMI_MGMT_TX_COMPLETION_EVENTID_param_tlvs *param_buf;
	wmi_mgmt_tx_compl_event_fixed_param *cmpl_params;

	param_buf = (WMI_MGMT_TX_COMPLETION_EVENTID_param_tlvs *)
		evt_buf;
	if (!param_buf) {
		wmi_err("Invalid mgmt Tx completion event");
		return QDF_STATUS_E_INVAL;
	}
	cmpl_params = param_buf->fixed_param;

	param->pdev_id = wmi_handle->ops->convert_pdev_id_target_to_host(
							wmi_handle,
							cmpl_params->pdev_id);
	param->desc_id = cmpl_params->desc_id;
	param->status = cmpl_params->status;
	param->ppdu_id = cmpl_params->ppdu_id;
	param->retries_count = cmpl_params->retries_count;
	param->tx_tsf = cmpl_params->tx_tsf_u32;
	param->tx_tsf = ((param->tx_tsf << WMI_TSF_SHIFT_UPPER) &
			 WMI_TSF_MASK_UPPER_32) |
			cmpl_params->tx_tsf_l32;

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_chan_info_event_tlv() - extract chan information from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param chan_info: Pointer to hold chan information
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_chan_info_event_tlv(wmi_unified_t wmi_handle,
	void *evt_buf, wmi_host_chan_info_event *chan_info)
{
	WMI_CHAN_INFO_EVENTID_param_tlvs *param_buf;
	wmi_chan_info_event_fixed_param *ev;

	param_buf = (WMI_CHAN_INFO_EVENTID_param_tlvs *) evt_buf;

	ev = (wmi_chan_info_event_fixed_param *) param_buf->fixed_param;
	if (!ev) {
		wmi_err("Failed to allocmemory");
		return QDF_STATUS_E_FAILURE;
	}

	chan_info->err_code = ev->err_code;
	chan_info->freq = ev->freq;
	chan_info->cmd_flags = ev->cmd_flags;
	chan_info->noise_floor = ev->noise_floor;
	chan_info->rx_clear_count = ev->rx_clear_count;
	chan_info->cycle_count = ev->cycle_count;
	chan_info->tx_frame_cnt = ev->tx_frame_cnt;
	chan_info->mac_clk_mhz = ev->mac_clk_mhz;
	chan_info->pdev_id = wlan_get_pdev_id_from_vdev_id(
			(struct wlan_objmgr_psoc *)wmi_handle->soc->wmi_psoc,
			ev->vdev_id, WLAN_SCAN_ID);
	chan_info->chan_tx_pwr_range = ev->chan_tx_pwr_range;
	chan_info->chan_tx_pwr_tp = ev->chan_tx_pwr_tp;
	chan_info->my_bss_rx_cycle_count = ev->my_bss_rx_cycle_count;
	chan_info->rx_11b_mode_data_duration = ev->rx_11b_mode_data_duration;
	chan_info->tx_frame_cnt = ev->tx_frame_cnt;
	chan_info->rx_frame_count = ev->rx_frame_count;
	chan_info->mac_clk_mhz = ev->mac_clk_mhz;
	chan_info->vdev_id = ev->vdev_id;
	qdf_mem_copy(chan_info->per_chain_noise_floor,
		     ev->per_chain_noise_floor,
		     sizeof(chan_info->per_chain_noise_floor));

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_channel_hopping_event_tlv() - extract channel hopping param
 * from event
 * @wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param ch_hopping: Pointer to hold channel hopping param
 *
 * @return QDF_STATUS_SUCCESS  on success and -ve on failure.
 */
static QDF_STATUS extract_channel_hopping_event_tlv(
	wmi_unified_t wmi_handle, void *evt_buf,
	wmi_host_pdev_channel_hopping_event *ch_hopping)
{
	WMI_PDEV_CHANNEL_HOPPING_EVENTID_param_tlvs *param_buf;
	wmi_pdev_channel_hopping_event_fixed_param *event;

	param_buf = (WMI_PDEV_CHANNEL_HOPPING_EVENTID_param_tlvs *)evt_buf;
	event = (wmi_pdev_channel_hopping_event_fixed_param *)
						param_buf->fixed_param;

	ch_hopping->noise_floor_report_iter = event->noise_floor_report_iter;
	ch_hopping->noise_floor_total_iter = event->noise_floor_total_iter;
	ch_hopping->pdev_id = wmi_handle->ops->convert_pdev_id_target_to_host(
								wmi_handle,
								event->pdev_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * send_peer_chan_width_switch_cmd_tlv() - send peer channel width params
 * @wmi_handle: WMI handle
 * @param: Peer channel width switching params
 *
 * Return: QDF_STATUS_SUCCESS on success or error code
 */

static QDF_STATUS
send_peer_chan_width_switch_cmd_tlv(wmi_unified_t wmi_handle,
				    struct peer_chan_width_switch_params *param)
{
	wmi_buf_t buf;
	wmi_peer_chan_width_switch_cmd_fixed_param *cmd;
	int32_t len;
	uint32_t max_peers_per_command, max_peers_per_buf;
	wmi_chan_width_peer_list *cmd_peer_list;
	int16_t pending_peers = param->num_peers;
	struct peer_chan_width_switch_info *param_peer_list =
						param->chan_width_peer_list;
	uint8_t ix;

	/* Max peers per WMI buffer */
	max_peers_per_buf = (wmi_get_max_msg_len(wmi_handle) -
			     sizeof(*cmd) - WMI_TLV_HDR_SIZE) /
			    sizeof(*cmd_peer_list);

	/*
	 * Use param value only if it's greater than 0 and less than
	 * the max peers per WMI buf.
	 */
	if (param->max_peers_per_cmd &&
	    (param->max_peers_per_cmd <= max_peers_per_buf)) {
		max_peers_per_command = param->max_peers_per_cmd;
	} else {
		max_peers_per_command = max_peers_per_buf;
	}

	wmi_debug("Setting peer limit as %u", max_peers_per_command);

	while (pending_peers > 0) {
		len = sizeof(*cmd) + WMI_TLV_HDR_SIZE;
		if (pending_peers >= max_peers_per_command) {
			len += (max_peers_per_command * sizeof(*cmd_peer_list));
		} else {
			len += (pending_peers * sizeof(*cmd_peer_list));
		}

		buf = wmi_buf_alloc(wmi_handle, len);
		if (!buf) {
			wmi_err("wmi_buf_alloc failed");
			return QDF_STATUS_E_FAILURE;
		}

		cmd = (wmi_peer_chan_width_switch_cmd_fixed_param *)
							wmi_buf_data(buf);

		WMITLV_SET_HDR(&cmd->tlv_header,
		    WMITLV_TAG_STRUC_wmi_peer_chan_width_switch_cmd_fixed_param,
		    WMITLV_GET_STRUCT_TLVLEN(
				wmi_peer_chan_width_switch_cmd_fixed_param));

		cmd->num_peers = (pending_peers >= max_peers_per_command) ?
					max_peers_per_command : pending_peers;

		WMI_PEER_CHAN_WIDTH_SWITCH_SET_VDEV_ID(cmd->vdev_var, param->vdev_id);
		WMI_PEER_CHAN_WIDTH_SWITCH_SET_VALID_VDEV_ID(cmd->vdev_var);

		WMITLV_SET_HDR(((void *)cmd + sizeof(*cmd)),
                               WMITLV_TAG_ARRAY_STRUC,
			       cmd->num_peers *
			       sizeof(wmi_chan_width_peer_list));

		cmd_peer_list = (wmi_chan_width_peer_list *)
				((void *)cmd + sizeof(*cmd) +
				 WMI_TLV_HDR_SIZE);

		for (ix = 0; ix < cmd->num_peers; ix++) {
			WMITLV_SET_HDR(&cmd_peer_list[ix].tlv_header,
				WMITLV_TAG_STRUC_wmi_chan_width_peer_list,
				WMITLV_GET_STRUCT_TLVLEN(
					wmi_chan_width_peer_list));

			WMI_CHAR_ARRAY_TO_MAC_ADDR(param_peer_list[ix].mac_addr,
					   &cmd_peer_list[ix].peer_macaddr);

			cmd_peer_list[ix].chan_width =
				convert_host_to_target_chwidth(param_peer_list[ix].chan_width);

			wmi_debug("Peer[%u]: chan_width = %u", ix,
				  cmd_peer_list[ix].chan_width);
		}

		pending_peers -= cmd->num_peers;
		param_peer_list += cmd->num_peers;

		if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_PEER_CHAN_WIDTH_SWITCH_CMDID)) {
			wmi_err("Sending peers for chwidth switch failed");
			wmi_buf_free(buf);
			return QDF_STATUS_E_FAILURE;
		}

	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS extract_multi_vdev_restart_resp_event_tlv(
		wmi_unified_t wmi_hdl, void *evt_buf,
		struct multi_vdev_restart_resp *param)
{
	WMI_PDEV_MULTIPLE_VDEV_RESTART_RESP_EVENTID_param_tlvs *param_buf;
	wmi_pdev_multiple_vdev_restart_resp_event_fixed_param *ev;

	param_buf =
	(WMI_PDEV_MULTIPLE_VDEV_RESTART_RESP_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf) {
		wmi_err("Invalid buf multi_vdev restart response");
		return QDF_STATUS_E_INVAL;
	}

	ev = (wmi_pdev_multiple_vdev_restart_resp_event_fixed_param *)
							param_buf->fixed_param;
	if (!ev) {
		wmi_err("Invalid ev multi_vdev restart response");
		return QDF_STATUS_E_INVAL;
	}


	param->pdev_id = wmi_hdl->ops->convert_target_pdev_id_to_host(
								wmi_hdl,
								ev->pdev_id);
	param->status = ev->status;

	if (!param_buf->num_vdev_ids_bitmap)
		return QDF_STATUS_E_FAILURE;

	qdf_mem_copy(param->vdev_id_bmap, param_buf->vdev_ids_bitmap,
		     sizeof(param->vdev_id_bmap));

	wmi_debug("vdev_id_bmap is as follows");
	qdf_trace_hex_dump(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_DEBUG,
			   param->vdev_id_bmap, sizeof(param->vdev_id_bmap));

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS send_multisoc_tbtt_sync_cmd_tlv(wmi_unified_t wmi,
		struct rnr_tbtt_multisoc_sync_param *param)
{
	wmi_pdev_tbtt_offset_sync_cmd_fixed_param *tbtt_sync_cmd;
	struct rnr_bss_tbtt_info_param *tmp_bss;
	wmi_buf_t buf;
	wmi_pdev_rnr_bss_tbtt_info *bss_tbtt_info;
	int32_t len = 0;
	int idx;
	uint8_t *buf_ptr = NULL;

	switch (param->cmd_type) {
	case WMI_PDEV_GET_TBTT_OFFSET:
		len = sizeof(wmi_pdev_tbtt_offset_sync_cmd_fixed_param);
		break;
	case WMI_PDEV_SET_TBTT_OFFSET:
		len = sizeof(wmi_pdev_tbtt_offset_sync_cmd_fixed_param) +
			WMI_TLV_HDR_SIZE +
			(param->rnr_vap_count *
			 sizeof(wmi_pdev_rnr_bss_tbtt_info));
		break;
	default:
		wmi_err("cmd_type: %d invalid", param->cmd_type);
		return QDF_STATUS_E_FAILURE;
	}
	tmp_bss = param->rnr_bss_tbtt;
	buf = wmi_buf_alloc(wmi, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_NOMEM;
	}
	buf_ptr = wmi_buf_data(buf);
	tbtt_sync_cmd = (wmi_pdev_tbtt_offset_sync_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&tbtt_sync_cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_pdev_tbtt_offset_sync_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_pdev_tbtt_offset_sync_cmd_fixed_param));

	tbtt_sync_cmd->cmd_type = param->cmd_type;
	tbtt_sync_cmd->pdev_id = wmi->ops->convert_host_pdev_id_to_target(
							wmi,
							param->pdev_id);
	if (tbtt_sync_cmd->cmd_type == WMI_PDEV_SET_TBTT_OFFSET &&
	    param->rnr_vap_count) {
		buf_ptr += sizeof(wmi_pdev_tbtt_offset_sync_cmd_fixed_param);
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
			       (param->rnr_vap_count *
			       sizeof(wmi_pdev_rnr_bss_tbtt_info)));
		bss_tbtt_info = (wmi_pdev_rnr_bss_tbtt_info *)(buf_ptr +
				 WMI_TLV_HDR_SIZE);
		for (idx = 0; idx < param->rnr_vap_count; idx++) {
			WMITLV_SET_HDR(&bss_tbtt_info->tlv_header,
				WMITLV_TAG_STRUC_wmi_pdev_rnr_bss_tbtt_info,
				WMITLV_GET_STRUCT_TLVLEN(wmi_pdev_rnr_bss_tbtt_info));
			WMI_CHAR_ARRAY_TO_MAC_ADDR(tmp_bss->bss_mac,
					&bss_tbtt_info->bss_mac);
			bss_tbtt_info->beacon_intval =
				tmp_bss->beacon_intval;
			bss_tbtt_info->opclass = tmp_bss->opclass;
			bss_tbtt_info->chan_idx =
				tmp_bss->chan_idx;
			bss_tbtt_info->next_qtime_tbtt_high =
				tmp_bss->next_qtime_tbtt_high;
			bss_tbtt_info->next_qtime_tbtt_low =
				tmp_bss->next_qtime_tbtt_low;
			QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_DEBUG,
				  "Beacon Intval: %d, Chan: %d, opclass: %d",
				  bss_tbtt_info->beacon_intval,
				  bss_tbtt_info->chan_idx,
				  bss_tbtt_info->opclass);
			bss_tbtt_info++;
			tmp_bss++;
		}
	}
	QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_DEBUG,
		  "Cmd Type: %d, Pdev id: %d Vap count: %d", tbtt_sync_cmd->cmd_type,
		  tbtt_sync_cmd->pdev_id, param->rnr_vap_count);
	if (wmi_unified_cmd_send(wmi, buf, len, WMI_PDEV_TBTT_OFFSET_SYNC_CMDID)) {
		wmi_err("Failed to send multisoc tbtt sync command");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * set_radio_tx_mode_select_cmd_tlv - set radio tx mode select command
 * @wmi: wmi handle
 * @param: Tx mode select param
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
set_radio_tx_mode_select_cmd_tlv(wmi_unified_t wmi,
		struct wmi_pdev_enable_tx_mode_selection *param)
{
	wmi_pdev_enable_duration_based_tx_mode_selection_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_pdev_enable_duration_based_tx_mode_selection_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_pdev_enable_duration_based_tx_mode_selection_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_pdev_enable_duration_based_tx_mode_selection_cmd_fixed_param));
	cmd->pdev_id = wmi->ops->convert_pdev_id_host_to_target(
			wmi, param->pdev_id);
	cmd->duration_based_tx_mode_selection = param->enable_tx_mode_selection;
	wmi_mtrace(WMI_PDEV_ENABLE_DURATION_BASED_TX_MODE_SELECTION_CMDID, cmd->pdev_id, 0);
	if (wmi_unified_cmd_send(wmi, buf, len,
				WMI_PDEV_ENABLE_DURATION_BASED_TX_MODE_SELECTION_CMDID)) {
		wmi_err("%s: Failed to send WMI_PDEV_ENABLE_DURATION_BASED_TX_MODE_SELECTION_CMDID",
				__func__);
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS send_lcr_cmd_tlv(wmi_unified_t wmi_handle,
				   struct wmi_wifi_pos_lcr_info *lcr_info)
{
	struct wmi_rtt_oem_req_head *req_head;
	struct wmi_rtt_oem_lcr_cfg_head *lcr_config;
	uint8_t req_head_len = sizeof(struct wmi_rtt_oem_req_head);
	uint32_t lcr_cfg_head_len = sizeof(struct wmi_rtt_oem_lcr_cfg_head);
	uint8_t *buf;
	uint32_t buf_len;
	uint8_t *civic_info;
	uint8_t lcr_info_len;
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;

	buf_len = req_head_len + lcr_cfg_head_len;

	buf = qdf_mem_malloc(buf_len * sizeof(uint8_t));
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_zero(buf, buf_len);

	/* Add the wmi_rtt_oem_req_head TLV */
	req_head = (struct wmi_rtt_oem_req_head *)buf;
	WMIRTT_TLV_SET_HDR(&req_head->tlv_header,
			   WMIRTT_TLV_TAG_STRUC_wmi_rtt_oem_req_head,
			   req_head_len);

	req_head->sub_type = RTT_MSG_SUBTYPE_CONFIGURE_LCR;
	req_head->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
							wmi_handle,
							lcr_info->pdev_id);
	WMI_RTT_REQ_ID_SET((req_head->req_id), lcr_info->req_id);

	lcr_config = (struct wmi_rtt_oem_lcr_cfg_head *)(buf + req_head_len);
	WMIRTT_TLV_SET_HDR(&lcr_config->tlv_header,
			   WMIRTT_TLV_TAG_STRUC_wmi_rtt_oem_lcr_cfg_head,
			   lcr_cfg_head_len);

	/* The following subtraction and addition of the value 2 to length is
	 * being done because Country code which is 2 bytes comes separately
	 * from the actual Civic Info string.
	 */
	lcr_info_len = (lcr_info->civic_len > (CIVIC_INFO_LEN - 2)) ?
				   (CIVIC_INFO_LEN - 2) : lcr_info->civic_len;

	WMI_RTT_LOC_CIVIC_LENGTH_SET(lcr_config->loc_civic_params,
				     (lcr_info_len + 2));

	civic_info = (uint8_t *)lcr_config->civic_info;
	civic_info[0] = lcr_info->country_code[0];
	civic_info[1] = lcr_info->country_code[1];
	qdf_mem_copy(&civic_info[2], lcr_info->civic_info, lcr_info->civic_len);

	if (wmi_handle->ops->send_start_oem_data_cmd)
		ret = wmi_handle->ops->send_start_oem_data_cmd(wmi_handle,
								buf_len, buf);

	qdf_mem_free(buf);
	return ret;
}

static QDF_STATUS send_lci_cmd_tlv(wmi_unified_t wmi_handle,
				   struct wifi_pos_lci_info *lci_info)
{
	struct wmi_rtt_oem_req_head *req_head;
	struct wmi_rtt_oem_lci_cfg_head *lci_config;
	uint8_t req_head_len = sizeof(struct wmi_rtt_oem_req_head);
	uint32_t lci_cfg_head_len = sizeof(struct wmi_rtt_oem_lci_cfg_head);
	uint8_t *buf;
	uint32_t buf_len;
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;

	buf_len = req_head_len + lci_cfg_head_len;

	buf = qdf_mem_malloc(buf_len * sizeof(uint8_t));
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_zero(buf, buf_len);

	/* Adding the wmi_rtt_oem_req_head TLV */
	req_head = (struct wmi_rtt_oem_req_head *)buf;
	WMIRTT_TLV_SET_HDR(&req_head->tlv_header,
			   WMIRTT_TLV_TAG_STRUC_wmi_rtt_oem_req_head,
			   sizeof(struct wmi_rtt_oem_req_head));
	req_head->sub_type = RTT_MSG_SUBTYPE_CONFIGURE_LCI;
	req_head->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
				wmi_handle, lci_info->pdev_id);
	WMI_RTT_REQ_ID_SET((req_head->req_id), lci_info->req_id);

	/* Adding the wmi_rtt_oem_lci_cfg_head TLV */
	lci_config = (struct wmi_rtt_oem_lci_cfg_head *)(buf + req_head_len);
	WMIRTT_TLV_SET_HDR(&lci_config->tlv_header,
			   WMIRTT_TLV_TAG_STRUC_wmi_rtt_oem_lci_cfg_head,
			   sizeof(struct wmi_rtt_oem_lci_cfg_head));

	lci_config->latitude = lci_info->latitude;
	lci_config->longitude = lci_info->longitude;
	lci_config->altitude = lci_info->altitude;
	WMI_RTT_LCI_LAT_UNC_SET(lci_config->lci_cfg_param_info,
				lci_info->latitude_unc);
	WMI_RTT_LCI_LON_UNC_SET(lci_config->lci_cfg_param_info,
				lci_info->longitude_unc);
	WMI_RTT_LCI_ALT_UNC_SET(lci_config->lci_cfg_param_info,
				lci_info->altitude_unc);
	WMI_RTT_LCI_Z_MOTION_PAT_SET(lci_config->lci_cfg_param_info,
				     lci_info->motion_pattern);
	lci_config->floor = lci_info->floor;
	WMI_RTT_LCI_Z_HEIGHT_ABV_FLR_SET(lci_config->floor_param_info,
					 lci_info->height_above_floor);
	WMI_RTT_LCI_Z_HEIGHT_UNC_SET(lci_config->floor_param_info,
				     lci_info->height_unc);
	lci_config->usage_rules = lci_info->usage_rules;

	if (wmi_handle->ops->send_start_oem_data_cmd)
		ret = wmi_handle->ops->send_start_oem_data_cmd(wmi_handle,
								buf_len, buf);

	qdf_mem_free(buf);
	return ret;
}

/**
 * send_vdev_set_intra_bss_cmd_tlv() - enable/disable intra_bss wmi command
 * @wmi_handle: wmi handle
 * @param: param buffer passed by upper layers
 *
 * Copy the buffer passed by the upper layers and send it
 * down to the firmware.
 *
 * Return: None
 */
static QDF_STATUS
send_vdev_set_intra_bss_cmd_tlv(struct wmi_unified *wmi_handle,
				struct wmi_intra_bss_params *param)
{
	wmi_vdev_enable_disable_intra_bss_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}

	cmd = (wmi_vdev_enable_disable_intra_bss_cmd_fixed_param *)
	       wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_enable_disable_intra_bss_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
		       wmi_vdev_enable_disable_intra_bss_cmd_fixed_param));

	cmd->vdev_id = param->vdev_id;
	cmd->enable = param->enable;

	wmi_mtrace(WMI_VDEV_ENABLE_DISABLE_INTRA_BSS_CMDID, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_VDEV_ENABLE_DISABLE_INTRA_BSS_CMDID)) {
		wmi_err("Failed to set intra-bss param");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * send_peer_set_intra_bss_cmd_tlv() - enable/disable intra_bss wmi command
 * @wmi_handle: wmi handle
 * @param: param buffer passed by upper layers
 *
 * Copy the buffer passed by the upper layers and send it
 * down to the firmware.
 *
 * Return: None
 */
static
QDF_STATUS send_peer_set_intra_bss_cmd_tlv(wmi_unified_t wmi_handle,
					   struct wmi_intra_bss_params *param)
{
	wmi_peer_enable_disable_intra_bss_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_peer_enable_disable_intra_bss_cmd_fixed_param *)
		wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_enable_disable_intra_bss_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
		       (wmi_peer_enable_disable_intra_bss_cmd_fixed_param));

	WMI_CHAR_ARRAY_TO_MAC_ADDR(param->peer_macaddr, &cmd->mac_addr);
	cmd->vdev_id = param->vdev_id;
	cmd->enable = param->enable;

	wmi_mtrace(WMI_PEER_ENABLE_DISABLE_INTRA_BSS_CMDID, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_PEER_ENABLE_DISABLE_INTRA_BSS_CMDID)) {
		wmi_err("Fail to send WMI_PEER_INTRA_BSS_ENABLE_CMDID");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	wmi_debug("peer_macaddr " QDF_MAC_ADDR_FMT " vdev_id %d",
		  QDF_MAC_ADDR_REF(param->peer_macaddr), param->vdev_id);

	return QDF_STATUS_SUCCESS;
}

#if !defined(CNSS_GENL) && defined(WLAN_RTT_MEASUREMENT_NOTIFICATION)
static QDF_STATUS extract_measreq_chan_info_tlv(
		uint32_t data_len, uint8_t *data,
		struct rtt_channel_info *chinfo)
{
	struct wmi_rtt_oem_req_head *req_head = NULL;
	struct wmi_rtt_oem_channel_info *oem_chan_info = NULL;
	uint16_t oem_chaninfo_offset = 0;
	struct wmi_rtt_oem_measreq_per_channel_info *per_channel_info = NULL;
	uint16_t per_channel_info_offset = 0;
	struct wmi_rtt_oem_measreq_peer_info *peer_info = NULL;
	uint16_t peer_info_offset = 0;
	int i = 0;
	char *peer_msg = NULL;

	/* RTT measurement request buffer has the following TLVs
	 * WMIRTT_TLV_TAG_STRUC_wmi_rtt_oem_req_head
	 * WMIRTT_TLV_TAG_STRUC_wmi_rtt_oem_measreq_head
	 * WMIRTT_TLV_TAG_STRUC_loop_start
	 * WMIRTT_TLV_TAG_STRUC_wmi_rtt_oem_channel_info
	 * WMIRTT_TLV_TAG_STRUC_wmi_rtt_oem_measreq_per_channel_info
	 * WMIRTT_TLV_TAG_STRUC_loop_start
	 * WMIRTT_TLV_TAG_STRUC_wmi_rtt_oem_measreq_peer_info
	 */
	req_head = (struct wmi_rtt_oem_req_head *)data;

	if (WMIRTT_TLV_GET_TLVTAG(req_head->tlv_header) !=
			WMIRTT_TLV_TAG_STRUC_wmi_rtt_oem_req_head) {
		wmi_err("wrong tag(%u), expected wmi_rtt_oem_req_head tag",
			WMIRTT_TLV_GET_TLVTAG(req_head->tlv_header));
		return QDF_STATUS_E_INVAL;
	}

	if (req_head->sub_type != RTT_MSG_SUBTYPE_MEASUREMENT_REQ) {
		wmi_debug("Not a measurement request, skip channel info extraction functions");
		return QDF_STATUS_SUCCESS;
	}

	/* Read freq, cfreq1, cfreq2 and phymode from wmi_rtt_oem_channel_info
	 * TLV.
	 */
	oem_chaninfo_offset = sizeof(struct wmi_rtt_oem_req_head) +
		sizeof(struct wmi_rtt_oem_measreq_head) + RTT_TLV_HDR_SIZE;

	if ((oem_chaninfo_offset + sizeof(struct wmi_rtt_oem_channel_info)) >
			data_len) {
		wmi_err("wmi_rtt_oem_channel_info structure is not present in the data buffer");
		return QDF_STATUS_E_INVAL;
	}

	oem_chan_info = (struct wmi_rtt_oem_channel_info *)(data +
			oem_chaninfo_offset);

	if (WMIRTT_TLV_GET_TLVTAG(oem_chan_info->tlv_header) !=
			WMIRTT_TLV_TAG_STRUC_wmi_rtt_oem_channel_info) {
		wmi_err("wrong tag(%u), expected wmi_rtt_oem_channel_info tag",
			WMIRTT_TLV_GET_TLVTAG(oem_chan_info->tlv_header));
		return QDF_STATUS_E_INVAL;
	}

	chinfo->freq = oem_chan_info->mhz;
	chinfo->cfreq1 = oem_chan_info->band_center_freq1;
	chinfo->cfreq2 = oem_chan_info->band_center_freq2;
	chinfo->phymode = oem_chan_info->info & ~(RTT_PHY_MODE_MASK);

	/* Read number of bssids present in the measurement request buffer */
	per_channel_info_offset = oem_chaninfo_offset +
		sizeof(struct wmi_rtt_oem_channel_info);
	if ((per_channel_info_offset +
	     sizeof(struct wmi_rtt_oem_measreq_per_channel_info)) > data_len) {
		wmi_err("wmi_rtt_oem_measreq_per_channel_info structure is not present in the data buffer");
		return QDF_STATUS_E_INVAL;
	}

	per_channel_info = (struct wmi_rtt_oem_measreq_per_channel_info *)(
			data + per_channel_info_offset);

	if (WMIRTT_TLV_GET_TLVTAG(per_channel_info->tlv_header) !=
			WMIRTT_TLV_TAG_STRUC_wmi_rtt_oem_measreq_per_channel_info) {
		wmi_err("wrong tag(%u), expected wmi_rtt_oem_measreq_per_channel_info tag",
			WMIRTT_TLV_GET_TLVTAG(per_channel_info->tlv_header));
		return QDF_STATUS_E_INVAL;
	}

	chinfo->num_bssids = WMI_RTT_NUM_STA_GET(per_channel_info->sta_num);

	/* Read dest_macaddr and BW */
	peer_info_offset = per_channel_info_offset +
		sizeof(struct wmi_rtt_oem_measreq_per_channel_info) +
		RTT_TLV_HDR_SIZE;
	if ((peer_info_offset + (chinfo->num_bssids *
					sizeof(struct wmi_rtt_oem_measreq_peer_info)
					)) > data_len) {
		wmi_err("wmi_rtt_oem_measreq_peer_info structure is not present in the data buffer");
		return QDF_STATUS_E_INVAL;
	}

	peer_msg = data + peer_info_offset;

	for (i = 0; i < chinfo->num_bssids; i++) {
		peer_info = (struct wmi_rtt_oem_measreq_peer_info *)(peer_msg);
		if (WMIRTT_TLV_GET_TLVTAG(peer_info->tlv_header) !=
				WMIRTT_TLV_TAG_STRUC_wmi_rtt_oem_measreq_peer_info) {
			wmi_err("wrong tag(%u), expected wmi_rtt_oem_measreq_peer_info tag",
				WMIRTT_TLV_GET_TLVTAG(peer_info->tlv_header));
			return QDF_STATUS_E_INVAL;
		}

		chinfo->bssid_info[i].bw =
			WMI_RTT_BW_GET(peer_info->control_flag);
		WMI_MAC_ADDR_TO_CHAR_ARRAY(&peer_info->dest_mac,
					   chinfo->bssid_info[i].macaddr);
		peer_msg += sizeof(struct wmi_rtt_oem_measreq_peer_info);
	}

	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_RTT_MEASUREMENT_NOTIFICATION */

#ifdef WLAN_SUPPORT_MESH_LATENCY
/**
 * config_vdev_tid_latency_info_cmd_tlv() - enable vdev tid latency command
 * @wmi: wmi handle
 * @param: vdev tid latency config param
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
config_vdev_tid_latency_info_cmd_tlv(wmi_unified_t wmi_handle,
		struct wmi_vdev_tid_latency_config_params *param)
{
	wmi_vdev_latency_info *vdev_tid_latency_info;
	wmi_vdev_tid_latency_config_fixed_param *cmd;
	wmi_buf_t buf;
	QDF_STATUS retval;
	uint32_t len = 0;
	uint8_t i = 0;
	uint8_t *buf_ptr = 0;

	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE;
	len += param->num_vdev * sizeof(wmi_vdev_latency_info);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_vdev_tid_latency_config_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_vdev_tid_latency_config_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_vdev_tid_latency_config_fixed_param));
	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
			wmi_handle,
			param->pdev_id);
	cmd->vdev_id = param->vdev_id;

	buf_ptr += sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
			sizeof(wmi_vdev_latency_info) * param->num_vdev);
	buf_ptr += WMI_TLV_HDR_SIZE;
	vdev_tid_latency_info = (wmi_vdev_latency_info *)buf_ptr;
	for (i = 0; i < param->num_vdev; i++) {
		WMITLV_SET_HDR(&vdev_tid_latency_info->tlv_header,
				WMITLV_TAG_STRUC_wmi_vdev_latency_info,
				WMITLV_GET_STRUCT_TLVLEN(
					wmi_vdev_latency_info));
		vdev_tid_latency_info->service_interval =
			param->latency_info[i].service_interval;
		vdev_tid_latency_info->burst_size =
			param->latency_info[i].burst_size;
		WMI_VDEV_LATENCY_SET_TIDNUM(
				vdev_tid_latency_info->vdev_latency_info,
				param->latency_info[i].latency_tid);
		WMI_VDEV_LATENCY_SET_AC(
				vdev_tid_latency_info->vdev_latency_info,
				param->latency_info[i].ac);
		WMI_VDEV_LATENCY_SET_DL_VALID(
				vdev_tid_latency_info->vdev_latency_info,
				param->latency_info[i].dl_enable);
		WMI_VDEV_LATENCY_SET_UL_VALID(
				vdev_tid_latency_info->vdev_latency_info,
				param->latency_info[i].ul_enable);
		vdev_tid_latency_info++;
	}

	wmi_mtrace(WMI_VDEV_TID_LATENCY_CONFIG_CMDID, NO_SESSION, 0);
	retval = wmi_unified_cmd_send(wmi_handle, buf, len,
			WMI_VDEV_TID_LATENCY_CONFIG_CMDID);
	if (QDF_IS_STATUS_ERROR(retval)) {
		wmi_err("WMI Failed");
		wmi_buf_free(buf);
	}

	return retval;
}

/**
 * config_peer_latency_info_cmd_tlv() - enable peer latency command
 * @wmi: wmi handle
 * @param: peer latency config param
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
config_peer_latency_info_cmd_tlv(wmi_unified_t wmi_handle,
		struct wmi_peer_latency_config_params *param)
{
	wmi_tid_latency_info *tid_latency_info;
	wmi_peer_tid_latency_config_fixed_param *cmd;
	wmi_buf_t buf;
	QDF_STATUS retval;
	uint32_t len = 0;
	uint8_t i = 0;
	uint8_t *buf_ptr = 0;

	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE;
	len += param->num_peer * sizeof(wmi_tid_latency_info);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_peer_tid_latency_config_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_peer_tid_latency_config_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_peer_tid_latency_config_fixed_param));
	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
			wmi_handle,
			param->pdev_id);

	buf_ptr += sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
			sizeof(wmi_tid_latency_info) * param->num_peer);
	buf_ptr += WMI_TLV_HDR_SIZE;
	tid_latency_info = (wmi_tid_latency_info *)buf_ptr;
	for (i = 0; i < param->num_peer; i++) {
		WMITLV_SET_HDR(&tid_latency_info->tlv_header,
				WMITLV_TAG_STRUC_wmi_tid_latency_info,
				WMITLV_GET_STRUCT_TLVLEN(
					wmi_tid_latency_info));
		tid_latency_info->service_interval =
			param->latency_info[i].service_interval;
		tid_latency_info->burst_size_diff =
			param->latency_info[i].burst_size;
		WMI_CHAR_ARRAY_TO_MAC_ADDR(param->latency_info[i].peer_mac,
				&tid_latency_info->dest_macaddr);
		WMI_TID_LATENCY_SET_TIDNUM(tid_latency_info->latency_tid_info,
				param->latency_info[i].latency_tid);
		WMI_TID_LATENCY_SET_AC(tid_latency_info->latency_tid_info,
				param->latency_info[i].ac);
		WMI_TID_LATENCY_SET_DL_ENABLE(tid_latency_info->latency_tid_info,
				param->latency_info[i].dl_enable);
		WMI_TID_LATENCY_SET_UL_ENABLE(tid_latency_info->latency_tid_info,
				param->latency_info[i].ul_enable);
		WMI_LATENCY_SET_BURST_SIZE_SUM(tid_latency_info->latency_tid_info,
				param->latency_info[i].add_or_sub);
		tid_latency_info++;
	}

	wmi_mtrace(WMI_PEER_TID_LATENCY_CONFIG_CMDID, NO_SESSION, 0);
	retval = wmi_unified_cmd_send(wmi_handle, buf, len,
			WMI_PEER_TID_LATENCY_CONFIG_CMDID);
	if (QDF_IS_STATUS_ERROR(retval)) {
		wmi_err("WMI Failed");
		wmi_buf_free(buf);
	}

	return retval;
}
#endif

/**
 * send_soc_tqm_reset_enable_disable_cmd_tlv() - This API to send the
 * WMI_SOC_TQM_RESET_ENABLE_DISABLE_CMDID to FW
 * @wmi_handle: wmi handle
 * @enable: Enable/Disable configuration
 *
 * Return: Success if the cmd is sent successfully to the firmware
 */
static
QDF_STATUS send_soc_tqm_reset_enable_disable_cmd_tlv(wmi_unified_t wmi_handle,
						     uint32_t enable)
{
	wmi_soc_tqm_reset_enable_disable_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint32_t len;

	len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_soc_tqm_reset_enable_disable_cmd_fixed_param *)
			wmi_buf_data(buf);
	WMITLV_SET_HDR
		(&cmd->tlv_header,
		 WMITLV_TAG_STRUC_wmi_soc_tqm_reset_enable_disable_cmd_fixed_param,
		 WMITLV_GET_STRUCT_TLVLEN(wmi_soc_tqm_reset_enable_disable_cmd_fixed_param));

	cmd->enable = !!enable;
	wmi_debug("TQM reset enable:%d", cmd->enable);

	wmi_mtrace(WMI_SOC_TQM_RESET_ENABLE_DISABLE_CMDID, NO_SESSION, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_SOC_TQM_RESET_ENABLE_DISABLE_CMDID)) {
		wmi_err("Failed to send WMI_SOC_TQM_RESET_ENABLE_DISABLE_CMDID");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef CONFIG_SAWF_DEF_QUEUES
static QDF_STATUS
send_set_rate_upper_cap_cmd_tlv(struct wmi_unified *wmi_handle, uint8_t pdev_id,
				struct wmi_rc_params *param)
{
	struct pdev_params pparam;
	uint32_t value = 0;

	WMI_PDEV_UPPER_CAP_NSS_SET(value, param->upper_cap_nss);
	WMI_PDEV_UPPER_CAP_MCS_SET(value, param->upper_cap_mcs);
	WMI_PDEV_UPPER_CAP_NSS_VALID_SET(value, param->en_nss_cap);
	WMI_PDEV_UPPER_CAP_MCS_VALID_SET(value, param->en_mcs_cap);

	wmi_info("pdev_id:%u param_value:0x%.8x", pdev_id, value);

	qdf_mem_set(&pparam, sizeof(pparam), 0);
	pparam.param_id = wmi_pdev_param_rate_upper_cap;
	pparam.param_value = value;

	if (wmi_handle->ops->send_pdev_param_cmd)
		return wmi_handle->ops->send_pdev_param_cmd(wmi_handle, &pparam,
							    pdev_id);

	return QDF_STATUS_E_FAILURE;
}

static QDF_STATUS
send_set_rate_retry_mcs_drop_cmd_tlv(struct wmi_unified *wmi_handle,
				     uint8_t pdev_id,
				     struct wmi_rc_params *param)
{
	struct pdev_params pparam;
	uint32_t value = 0;

	WMI_PDEV_RATE_DROP_NUM_MCS_SET(value, param->mcs_drop);
	WMI_PDEV_RATE_DROP_RETRY_THRSH_SET(value, param->retry_thresh);
	WMI_PDEV_RATE_DROP_NUM_MCS_VALID_SET(value, param->en_mcs_drop);
	WMI_PDEV_RATE_DROP_RETRY_THRSH_VALID_SET(value, param->en_retry_thresh);

	wmi_info("pdev_id:%u param_value:0x%.8x", pdev_id, value);

	qdf_mem_set(&pparam, sizeof(pparam), 0);
	pparam.param_id = wmi_pdev_param_rate_retry_mcs_drop;
	pparam.param_value = value;

	if (wmi_handle->ops->send_pdev_param_cmd)
		return wmi_handle->ops->send_pdev_param_cmd(wmi_handle, &pparam,
							    pdev_id);

	return QDF_STATUS_E_FAILURE;
}

static QDF_STATUS
send_set_mcs_probe_intvl_cmd_tlv(struct wmi_unified *wmi_handle,
				 uint8_t pdev_id, struct wmi_rc_params *param)
{
	struct pdev_params pparam;
	uint32_t value = 0;

	WMI_PDEV_RATE_MIN_MCS_PROBE_INTERVAL_SET(value,
			param->min_mcs_probe_intvl);
	WMI_PDEV_RATE_MAX_MCS_PROBE_INTERVAL_SET(value,
			param->max_mcs_probe_intvl);

	wmi_info("pdev_id:%u param_value:0x%.8x", pdev_id, value);

	qdf_mem_set(&pparam, sizeof(pparam), 0);
	pparam.param_id = wmi_pdev_param_mcs_probe_intvl;
	pparam.param_value = value;

	if (wmi_handle->ops->send_pdev_param_cmd)
		return wmi_handle->ops->send_pdev_param_cmd(wmi_handle, &pparam,
							    pdev_id);

	return QDF_STATUS_E_FAILURE;
}

static QDF_STATUS
send_set_nss_probe_intvl_cmd_tlv(struct wmi_unified *wmi_handle,
				 uint8_t pdev_id, struct wmi_rc_params *param)
{
	struct pdev_params pparam;
	uint32_t value = 0;

	WMI_PDEV_RATE_MIN_NSS_PROBE_INTERVAL_SET(value,
			param->min_nss_probe_intvl);
	WMI_PDEV_RATE_MAX_NSS_PROBE_INTERVAL_SET(value,
			param->max_nss_probe_intvl);

	wmi_info("pdev_id:%u param_value:0x%.8x", pdev_id, value);

	qdf_mem_set(&pparam, sizeof(pparam), 0);
	pparam.param_id = wmi_pdev_param_nss_probe_intvl;
	pparam.param_value = value;

	if (wmi_handle->ops->send_pdev_param_cmd)
		return wmi_handle->ops->send_pdev_param_cmd(wmi_handle, &pparam,
							    pdev_id);

	return QDF_STATUS_E_FAILURE;
}

#define SAWF_CONFIG_PARAM_MAX 0xFFFFFFFF

static uint32_t sawf_tid_infer(wmi_sawf_svc_class_cfg_cmd_fixed_param *p_cmd)
{
	uint32_t delay_bound_present, time_to_live_present, svc_interval_present;
	uint8_t tid;

	delay_bound_present =
		p_cmd->delay_bound_ms != WMI_SAWF_SVC_CLASS_PARAM_DEFAULT_DELAY_BOUND;
	time_to_live_present =
		p_cmd->time_to_live_ms != WMI_SAWF_SVC_CLASS_PARAM_DEFAULT_TIME_TO_LIVE;
	svc_interval_present =
		p_cmd->svc_interval_ms != WMI_SAWF_SVC_CLASS_PARAM_DEFAULT_SVC_INTERVAL;

	tid = 0; /* default */
	if (delay_bound_present) {
		if (SAWF_TID_INFER_LESS_THAN(delay_bound_ms,
					     DELAY_BOUND_ULTRA_LOW)) {
			tid = 7;
		} else if (SAWF_TID_INFER_LESS_THAN(delay_bound_ms,
						    DELAY_BOUND_LOW)) {
			tid = 6;
		} else if (SAWF_TID_INFER_LESS_THAN(delay_bound_ms,
						    DELAY_BOUND_MID)) {
			tid = 4;
			if (time_to_live_present &&
			    (SAWF_TID_INFER_LESS_THAN(time_to_live_ms,
						      TIME_TO_LIVE_MID)))
				tid = 5;
		} else if (SAWF_TID_INFER_LESS_THAN(delay_bound_ms,
						    DELAY_BOUND_HIGH)) {
			tid = 4;
			if (svc_interval_present) {
				if (SAWF_TID_INFER_LESS_THAN(svc_interval_ms,
							     SVC_INTERVAL_ULTRA_LOW))
					tid = 7;
			else if (SAWF_TID_INFER_LESS_THAN(svc_interval_ms,
							  SVC_INTERVAL_LOW))
					tid = 6;
			}
		}
	} else if (svc_interval_present) {
		if (SAWF_TID_INFER_LESS_THAN(svc_interval_ms,
					     SVC_INTERVAL_ULTRA_LOW))
			tid = 7;
		else if (SAWF_TID_INFER_LESS_THAN(svc_interval_ms,
						  SVC_INTERVAL_LOW))
			tid = 6;
	} else if (time_to_live_present) {
		if (SAWF_TID_INFER_LESS_THAN(time_to_live_ms,
					     TIME_TO_LIVE_ULTRA_LOW))
			tid = 7;
		else if (SAWF_TID_INFER_LESS_THAN(time_to_live_ms,
						  TIME_TO_LIVE_LOW))
			tid = 6;
	}
	return tid;
}

static void sawf_create_set_defaults(struct wmi_sawf_params *param)
{
	if (param->min_thruput_rate == SAWF_CONFIG_PARAM_MAX)
		param->min_thruput_rate = WMI_SAWF_SVC_CLASS_PARAM_DEFAULT_MIN_THRUPUT;

	if (param->max_thruput_rate == SAWF_CONFIG_PARAM_MAX)
		param->max_thruput_rate = WMI_SAWF_SVC_CLASS_PARAM_DEFAULT_MAX_THRUPUT;

	if (param->burst_size == SAWF_CONFIG_PARAM_MAX)
		param->burst_size = WMI_SAWF_SVC_CLASS_PARAM_DEFAULT_BURST_SIZE;

	if (param->service_interval == SAWF_CONFIG_PARAM_MAX)
		param->service_interval = WMI_SAWF_SVC_CLASS_PARAM_DEFAULT_SVC_INTERVAL;

	if (param->delay_bound == SAWF_CONFIG_PARAM_MAX)
		param->delay_bound = WMI_SAWF_SVC_CLASS_PARAM_DEFAULT_DELAY_BOUND;

	if (param->msdu_ttl == SAWF_CONFIG_PARAM_MAX)
		param->msdu_ttl = WMI_SAWF_SVC_CLASS_PARAM_DEFAULT_TIME_TO_LIVE;

	if (param->priority == SAWF_CONFIG_PARAM_MAX)
		param->priority = WMI_SAWF_SVC_CLASS_PARAM_DEFAULT_PRIORITY;

	if (param->tid == SAWF_CONFIG_PARAM_MAX)
		param->tid = WMI_SAWF_SVC_CLASS_PARAM_DEFAULT_TID;

	if (param->msdu_rate_loss == SAWF_CONFIG_PARAM_MAX)
		param->msdu_rate_loss = WMI_SAWF_SVC_CLASS_PARAM_DEFAULT_MSDU_LOSS_RATE;
}


QDF_STATUS send_sawf_create_cmd_tlv(wmi_unified_t wmi_handle,
				    struct wmi_sawf_params *param)
{
	QDF_STATUS ret;
	wmi_sawf_svc_class_cfg_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint8_t tid;
	uint16_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_sawf_svc_class_cfg_cmd_fixed_param *)wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_sawf_svc_class_cfg_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
		       (wmi_sawf_svc_class_cfg_cmd_fixed_param));

	sawf_create_set_defaults(param);

	cmd->svc_class_id = param->svc_id;
	cmd->min_thruput_kbps = param->min_thruput_rate;
	cmd->max_thruput_kbps = param->max_thruput_rate;
	cmd->burst_size_bytes = param->burst_size;
	cmd->svc_interval_ms = param->service_interval;
	cmd->delay_bound_ms = param->delay_bound;
	cmd->time_to_live_ms = param->msdu_ttl;
	cmd->priority = param->priority;
	cmd->tid = param->tid;
	cmd->msdu_loss_rate_ppm = param->msdu_rate_loss;

	if (param->tid == WMI_SAWF_SVC_CLASS_PARAM_DEFAULT_TID) {
		tid = sawf_tid_infer(cmd);
		cmd->tid = tid;
		param->tid = tid;
	}

	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_SAWF_SVC_CLASS_CFG_CMDID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wmi_err("Failed to send set param command ret = %d", ret);
		wmi_buf_free(buf);
	}

	return ret;
}

QDF_STATUS send_sawf_disable_cmd_tlv(wmi_unified_t wmi_handle,
				     uint32_t svc_id)
{
	QDF_STATUS ret;
	wmi_sawf_svc_class_disable_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint16_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_sawf_svc_class_disable_cmd_fixed_param *)wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_sawf_svc_class_disable_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
		       (wmi_sawf_svc_class_disable_cmd_fixed_param));

	cmd->svc_class_id = svc_id;

	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
			WMI_SAWF_SVC_CLASS_DISABLE_CMDID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wmi_err("Failed to send set param command ret = %d", ret);
		wmi_buf_free(buf);
	}

	return ret;
}
#endif

void wmi_ap_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;
	ops->send_peer_add_wds_entry_cmd = send_peer_add_wds_entry_cmd_tlv;
	ops->send_peer_del_wds_entry_cmd = send_peer_del_wds_entry_cmd_tlv;
#ifdef WLAN_FEATURE_MULTI_AST_DEL
	ops->send_peer_del_multi_wds_entries_cmd =
					send_peer_del_multi_wds_entries_cmd_tlv;
#endif
	ops->send_peer_update_wds_entry_cmd =
					send_peer_update_wds_entry_cmd_tlv;
	ops->send_pdev_get_tpc_config_cmd = send_pdev_get_tpc_config_cmd_tlv;
	ops->send_set_ctl_table_cmd = send_set_ctl_table_cmd_tlv;
	ops->send_set_mimogain_table_cmd = send_set_mimogain_table_cmd_tlv;
	ops->send_packet_power_info_get_cmd =
					send_packet_power_info_get_cmd_tlv;
	ops->send_nf_dbr_dbm_info_get_cmd = send_nf_dbr_dbm_info_get_cmd_tlv;
	ops->send_set_ht_ie_cmd = send_set_ht_ie_cmd_tlv;
	ops->send_set_vht_ie_cmd = send_set_vht_ie_cmd_tlv;
	ops->send_set_quiet_mode_cmd = send_set_quiet_mode_cmd_tlv;
	ops->send_set_bcn_offload_quiet_mode_cmd =
		send_set_bcn_offload_quiet_mode_cmd_tlv;
	ops->send_bcn_offload_control_cmd = send_bcn_offload_control_cmd_tlv;
	ops->extract_swba_num_vdevs = extract_swba_num_vdevs_tlv;
	ops->extract_swba_tim_info = extract_swba_tim_info_tlv;
	ops->extract_swba_quiet_info = extract_swba_quiet_info_tlv;
	ops->extract_swba_noa_info = extract_swba_noa_info_tlv;
	ops->extract_offchan_data_tx_compl_param =
				extract_offchan_data_tx_compl_param_tlv;
	ops->extract_peer_sta_kickout_ev = extract_peer_sta_kickout_ev_tlv;
	ops->send_multiple_vdev_restart_req_cmd =
				send_multiple_vdev_restart_req_cmd_tlv;
	ops->send_multiple_vdev_set_param_cmd =
				send_multiple_vdev_set_param_cmd_tlv;
	ops->extract_dcs_interference_type = extract_dcs_interference_type_tlv;
	ops->extract_dcs_awgn_info = extract_dcs_awgn_info_tlv;
	ops->extract_dcs_cw_int = extract_dcs_cw_int_tlv;
	ops->extract_dcs_im_tgt_stats = extract_dcs_im_tgt_stats_tlv;
	ops->extract_peer_create_response_event =
				extract_peer_create_response_event_tlv;
	ops->extract_peer_delete_response_event =
				extract_peer_delete_response_event_tlv;
	ops->extract_pdev_tpc_ev_param = extract_pdev_tpc_ev_param_tlv;
	ops->extract_pdev_tpc_config_ev_param =
			extract_pdev_tpc_config_ev_param_tlv;
	ops->extract_nfcal_power_ev_param = extract_nfcal_power_ev_param_tlv;
	ops->extract_wds_addr_event = extract_wds_addr_event_tlv;
	ops->extract_peer_sta_ps_statechange_ev =
		extract_peer_sta_ps_statechange_ev_tlv;
	ops->extract_inst_rssi_stats_event = extract_inst_rssi_stats_event_tlv;
	ops->send_pdev_caldata_version_check_cmd =
			send_pdev_caldata_version_check_cmd_tlv;
	ops->extract_pdev_caldata_version_check_ev_param =
			extract_pdev_caldata_version_check_ev_param_tlv;
#ifdef WLAN_SUPPORT_FILS
	ops->send_vdev_fils_enable_cmd = send_vdev_fils_enable_cmd_tlv;
	ops->extract_swfda_vdev_id = extract_swfda_vdev_id_tlv;
	ops->send_fils_discovery_send_cmd = send_fils_discovery_send_cmd_tlv;
#endif /* WLAN_SUPPORT_FILS */
	ops->send_mcast_group_update_cmd = send_mcast_group_update_cmd_tlv;
	ops->send_pdev_qvit_cmd = send_pdev_qvit_cmd_tlv;
	ops->send_wmm_update_cmd = send_wmm_update_cmd_tlv;
	ops->extract_mgmt_tx_compl_param = extract_mgmt_tx_compl_param_tlv;
	ops->extract_chan_info_event = extract_chan_info_event_tlv;
	ops->extract_channel_hopping_event = extract_channel_hopping_event_tlv;
	ops->send_peer_chan_width_switch_cmd =
					send_peer_chan_width_switch_cmd_tlv;
#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
	ops->set_rx_pkt_type_routing_tag_cmd =
					set_rx_pkt_type_routing_tag_update_tlv;
#endif /* WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG */
	ops->send_peer_vlan_config_cmd = send_peer_vlan_config_cmd_tlv;
	ops->extract_multi_vdev_restart_resp_event =
				extract_multi_vdev_restart_resp_event_tlv;
	ops->multisoc_tbtt_sync_cmd = send_multisoc_tbtt_sync_cmd_tlv;
	ops->set_radio_tx_mode_select_cmd = set_radio_tx_mode_select_cmd_tlv;
	ops->send_lcr_cmd = send_lcr_cmd_tlv;
	ops->send_lci_cmd = send_lci_cmd_tlv;
#if !defined(CNSS_GENL) && defined(WLAN_RTT_MEASUREMENT_NOTIFICATION)
	ops->extract_measreq_chan_info = extract_measreq_chan_info_tlv;
#endif
#ifdef WLAN_SUPPORT_MESH_LATENCY
	ops->config_vdev_tid_latency_info_cmd = config_vdev_tid_latency_info_cmd_tlv;
	ops->config_peer_latency_info_cmd = config_peer_latency_info_cmd_tlv;
#endif
	ops->send_get_halphy_cal_status_cmd = send_get_halphy_cal_status_cmd_tlv;
	ops->send_peer_set_intra_bss_cmd = send_peer_set_intra_bss_cmd_tlv;
	ops->send_vdev_set_intra_bss_cmd = send_vdev_set_intra_bss_cmd_tlv;
	ops->send_soc_tqm_reset_enable_disable_cmd =
				send_soc_tqm_reset_enable_disable_cmd_tlv;
#ifdef CONFIG_SAWF_DEF_QUEUES
	ops->send_set_rate_upper_cap_cmd = send_set_rate_upper_cap_cmd_tlv;
	ops->send_set_rate_retry_mcs_drop_cmd =
		send_set_rate_retry_mcs_drop_cmd_tlv;
	ops->send_set_mcs_probe_intvl_cmd = send_set_mcs_probe_intvl_cmd_tlv;
	ops->send_set_nss_probe_intvl_cmd = send_set_nss_probe_intvl_cmd_tlv;
	ops->send_sawf_create_cmd = send_sawf_create_cmd_tlv;
	ops->send_sawf_disable_cmd = send_sawf_disable_cmd_tlv;
#endif
#ifdef QCA_RSSI_DB2DBM
	ops->extract_pdev_rssi_dbm_conv_ev_param = extract_pdev_rssi_dbm_conv_ev_param_tlv;
#endif
}
