/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
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

/**
 * DOC: wlan_reg_channel_api.h
 * This file provides prototypes of the routines needed for the
 * external components to utilize the services provided by the
 * regulatory component with respect to channel list access.
 */

#ifndef __WLAN_REG_CHANNEL_API_H
#define __WLAN_REG_CHANNEL_API_H

#include <reg_services_public_struct.h>

enum ht40_intol {
	HT40_INTOL_PLUS = 0,
	HT40_INTOL_MINUS = 1,
};

#define IS_HT40_INTOL_MINUS (_bitmap) ((_bitmap) & BIT(HT40_INTOL_MINUS))
#define IS_HT40_INTOL_PLUS (_bitmap) ((_bitmap) & BIT(HT40_INTOL_PLUS))

#ifdef CONFIG_HOST_FIND_CHAN

#define WLAN_CHAN_PASSIVE       0x0000000000100000 /* Passive channel flag */

#define WLAN_CHAN_DFS              0x0002  /* DFS set on primary segment */
#define WLAN_CHAN_DFS_CFREQ2       0x0004  /* DFS set on secondary segment */
#define WLAN_CHAN_DISALLOW_ADHOC   0x0040  /* ad-hoc is not allowed */
#define WLAN_CHAN_PSC              0x0400  /* 6GHz PSC frequency */

/**
 * wlan_reg_set_chan_blocked() - Set is_chan_hop_blocked to true for a frequency
 * in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 *
 * Return: void.
 */
void wlan_reg_set_chan_blocked(struct wlan_objmgr_pdev *pdev,
			       qdf_freq_t freq);

/**
 * wlan_reg_is_chan_blocked() - Check if is_chan_hop_blocked to true for a
 * frequency in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 *
 * Return: true if is_chan_hop_blocked is true for the input frequency, else
 * false.
 */
bool wlan_reg_is_chan_blocked(struct wlan_objmgr_pdev *pdev,
			      qdf_freq_t freq);

/**
 * wlan_reg_is_chan_blocked() - Clear is_chan_hop_blocked for channel in the
 * current chan list.
 * @pdev: Pointer to pdev.
 *
 * Return: void.
 */
void wlan_reg_clear_allchan_blocked(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_set_chan_ht40intol() - Set ht40intol_flags to the value for a
 * frequency in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 * @ht40intol_flags: ht40intol_flags to be set.
 *
 * Return: void.
 */
void wlan_reg_set_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
				 enum ht40_intol ht40intol_flags);

/**
 * wlan_reg_clear_chan_ht40intol() - Clear the ht40intol_flags from the
 * regulatory channel corresponding to the frequency in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 * @ht40intol_flags: ht40intol_flags to be cleared.
 *
 * Return: void.
 */
void wlan_reg_clear_chan_ht40intol(struct wlan_objmgr_pdev *pdev,
				   qdf_freq_t freq,
				   enum ht40_intol ht40intol_flags);

/**
 * wlan_reg_is_chan_ht40intol() - Check if the ht40intol flag is set to the
 * given enum for a frequency in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 * @ht40intol_flags: The ht40intol flag (plus/minus) to check.
 *
 * Return: true if is_chan_htintol is set to given value for the input
 * frequency, else false.
 */
bool wlan_reg_is_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
				enum ht40_intol ht40intol_flags);

/**
 * wlan_reg_clear_allchan_ht40intol() - Clear ht40intol_flags for all channels
 * in the current chan list.
 * @pdev: Pointer to pdev.
 *
 * Return: void.
 */
void wlan_reg_clear_allchan_ht40intol(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_is_phymode_chwidth_allowed() - Check if requested phymode is allowed
 * @pdev: pdev pointer.
 * @phy_in: phymode that the user requested.
 * @ch_width: Channel width that the user requested.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 *
 * Return: true if phymode is allowed, else false.
 */
bool wlan_reg_is_phymode_chwidth_allowed(struct wlan_objmgr_pdev *pdev,
					 enum reg_phymode phy_in,
					 enum phy_ch_width ch_width,
					 qdf_freq_t primary_freq,
					 enum supported_6g_pwr_types
					 in_6g_pwr_mode);

/**
 * wlan_reg_get_max_phymode_and_chwidth() - Find the maximum regmode and
 * channel width combo supported by the device.
 * @phy_in: Maximum reg_phymode.
 * @ch_width: Maximum channel width.
 * @primary_freq: Input primary frequency.
 *
 * Return QDF_STATUS_SUCCESS if a combination is found, else return failure.
 */
QDF_STATUS wlan_reg_get_max_phymode_and_chwidth(struct wlan_objmgr_pdev *pdev,
						enum reg_phymode *phy_in,
						enum phy_ch_width *ch_width);

/**
 * wlan_reg_get_txpow_ant_gain() - Find the tx power and antenna gain for
 * the given frequency.
 * @pdev: pdev pointer.
 * @freq: Given frequency.
 * @txpower: tx power to be filled.
 * @ant_gain: Antenna gain to be filled.
 * @reg_chan_list: regulatory channel list.
 *
 */
void wlan_reg_get_txpow_ant_gain(struct wlan_objmgr_pdev *pdev,
				 qdf_freq_t freq,
				 uint32_t *txpower,
				 uint8_t *ant_gain,
				 struct regulatory_channel *reg_chan_list);

/**
 * wlan_reg_get_chan_flags() - Find the channel flags for freq1 and freq2.
 * @pdev: pdev pointer.
 * @freq1: Frequency in primary segment.
 * @freq2: Frequency in secondary segment.
 * @sec_flags: Secondary flags to be filled.
 * @pri_flags: Primary flags to be filled.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 * @reg_chan_list: regulatory channel list.
 */
void wlan_reg_get_chan_flags(struct wlan_objmgr_pdev *pdev,
			     qdf_freq_t freq1,
			     qdf_freq_t freq2,
			     uint16_t *sec_flags,
			     uint64_t *pri_flags,
			     enum supported_6g_pwr_types in_6g_pwr_mode,
			     struct regulatory_channel *reg_chan_list);

/**
 * wlan_reg_is_band_present() - Check if input band channels are present
 * in the regulatory current channel list.
 * @pdev: pdev pointer.
 * @reg_band: regulatory band.
 *
 */
bool wlan_reg_is_band_present(struct wlan_objmgr_pdev *pdev,
			      enum reg_wifi_band reg_band);

#else
static inline
void wlan_reg_set_chan_blocked(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
}

static inline
bool wlan_reg_is_chan_blocked(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	return false;
}

static inline void wlan_reg_clear_allchan_blocked(struct wlan_objmgr_pdev *pdev)
{
}

static inline void
wlan_reg_set_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			    enum ht40_intol ht40intol_flags)
{
}

static inline void
wlan_reg_clear_chan_ht40intol(struct wlan_objmgr_pdev *pdev,
			      qdf_freq_t freq,
			      enum ht40_intol ht40intol_flags)
{
}

static inline bool
wlan_reg_is_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			   enum ht40_intol ht40intol_flags)
{
	return false;
}

static inline void
wlan_reg_clear_allchan_ht40intol(struct wlan_objmgr_pdev *pdev)
{
}

static inline bool
wlan_reg_is_phymode_chwidth_allowed(struct wlan_objmgr_pdev *pdev,
				    enum reg_phymode phy_in,
				    enum phy_ch_width ch_width,
				    qdf_freq_t primary_freq)
{
	return false;
}

static inline QDF_STATUS
wlan_reg_get_max_phymode_and_chwidth(struct wlan_objmgr_pdev *pdev,
				     enum reg_phymode *phy_in,
				     enum phy_ch_width *ch_width)
{
	return QDF_STATUS_E_FAILURE;
}

static inline void
wlan_reg_get_txpow_ant_gain(struct wlan_objmgr_pdev *pdev,
			    qdf_freq_t freq,
			    uint32_t *txpower,
			    uint8_t *ant_gain,
			    struct regulatory_channel *reg_chan_list)
{
}

static inline void
wlan_reg_get_chan_flags(struct wlan_objmgr_pdev *pdev,
			qdf_freq_t freq1,
			qdf_freq_t freq2,
			uint16_t *sec_flags,
			uint64_t *pri_flags,
			enum supported_6g_pwr_types in_6g_pwr_mode,
			struct regulatory_channel *reg_chan_list)
{
}

static inline
bool wlan_reg_is_band_present(struct wlan_objmgr_pdev *pdev,
			      enum reg_wifi_band reg_band)
{
	return false;
}
#endif /* CONFIG_HOST_FIND_CHAN */

/**
 * wlan_reg_is_nol_freq() - Checks the channel is a nol chan or not
 * @freq: Channel center frequency
 *
 * Return: true if channel is nol else false
 */
bool wlan_reg_is_nol_for_freq(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq);

/**
 * wlan_reg_is_nol_hist_for_freq() - Checks the channel is a nol history channel
 * or not.
 * @freq: Channel center frequency
 *
 * Return: true if channel is nol else false
 */
bool wlan_reg_is_nol_hist_for_freq(struct wlan_objmgr_pdev *pdev,
				   qdf_freq_t freq);

/**
 * wlan_reg_get_ap_chan_list() - Get AP channel list
 * @pdev       : Pointer to pdev
 * @chan_list  : Pointer to channel list
 * @ap_pwr_type: Enum for AP power type (for 6GHz)
 *
 *
 * NOTE: If get_cur_chan_list is true, then ap_pwr_type is ignored
 *
 * Return
 * QDF_STATUS_SUCCESS: Successfully retrieved channel list
 * QDF_STATUS_E_INVAL: Could not get channel list
 */
QDF_STATUS wlan_reg_get_ap_chan_list(struct wlan_objmgr_pdev *pdev,
				     struct regulatory_channel *chan_list,
				     bool get_cur_chan_list,
				     enum reg_6g_ap_type ap_pwr_type);

/**
 * wlan_reg_is_freq_width_dfs()- Check if the channel is dfs, given the channel
 * frequency and width combination.
 * @pdev: Pointer to pdev.
 * @freq: Channel center frequency.
 * @ch_width: Channel Width.
 *
 * Return: True if frequency + width has DFS subchannels, else false.
 */
bool wlan_reg_is_freq_width_dfs(struct wlan_objmgr_pdev *pdev,
				qdf_freq_t freq,
				enum phy_ch_width ch_width);

/**
 * wlan_reg_get_channel_params() - Sets channel parameteres for
 * given bandwidth
 * @pdev: The physical dev to program country code or regdomain
 * @freq: channel center frequency.
 * @sec_ch_2g_freq: Secondary channel center frequency.
 * @ch_params: pointer to the channel parameters.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 *
 * Return: None
 */
void wlan_reg_get_channel_params(struct wlan_objmgr_pdev *pdev,
				 qdf_freq_t freq,
				 qdf_freq_t sec_ch_2g_freq,
				 struct ch_params *ch_params,
				 enum supported_6g_pwr_types in_6g_pwr_mode);

/**
 * wlan_reg_get_wmodes_and_max_chwidth() - Filter out the wireless modes
 * that are not supported by the available regulatory channels.
 * @pdev: Pointer to pdev.
 * @mode_select: Wireless modes to be filtered.
 * @include_nol_chan: boolean to indicate whether NOL channels are to be
 * considered as available channels.
 *
 * Return: Max channel width
 */
uint16_t wlan_reg_get_wmodes_and_max_chwidth(struct wlan_objmgr_pdev *pdev,
					     uint64_t *mode_select,
					     bool include_nol_chan);

/**
 * wlan_reg_get_client_power_for_rep_ap() - Get the client power for the
 * repeater AP
 * @pdev: Pointer to pdev.
 * @ap_pwr_type: AP power type
 * @client_type: Client type
 * @chan_freq: Channel frequency
 * @is_psd: Pointer to is_psd
 * @reg_eirp: Pointer to EIRP power
 * @reg_psd: Pointer to PSD
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
wlan_reg_get_client_power_for_rep_ap(struct wlan_objmgr_pdev *pdev,
				     enum reg_6g_ap_type ap_pwr_type,
				     enum reg_6g_client_type client_type,
				     qdf_freq_t chan_freq,
				     bool *is_psd, uint16_t *reg_eirp,
				     uint16_t *reg_psd);

/**
 * wlan_reg_get_reg_chan_list_based_on_freq() - Chan list returned  based on
 * freq
 * @pdev: Pointer to pdev.
 * @freq: channel center frequency.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 *
 * Return: regulatory_channel.
 */
struct regulatory_channel
wlan_reg_get_reg_chan_list_based_on_freq(struct wlan_objmgr_pdev *pdev,
					 qdf_freq_t freq,
					 enum supported_6g_pwr_types
					 in_6g_pwr_mode);

/**
 * wlan_reg_get_first_valid_freq_on_cur_chan() - Get the first valid freq based
 * on cur chan list.
 * @pdev: Pointer to pdev.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 * @first_valid_freq: channel center frequency.
 * @bw: Bandwidth.
 * @sec_40_offset: 40 MHz channel's secondary offset
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
wlan_reg_get_first_valid_freq(struct wlan_objmgr_pdev *pdev,
			      enum supported_6g_pwr_types
			      in_6g_pwr_mode,
			      qdf_freq_t *first_valid_freq,
			      int bw, int sec_40_offset);

/**
 * wlan_reg_get_first_valid_freq_on_power_mode() - Get the first valid freq
 * based on pwr mode.
 * @pdev: Pointer to pdev.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 * @freq: channel center frequency.
 * @bw: Bandwidth.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
wlan_reg_get_first_valid_freq_on_power_mode(struct wlan_objmgr_pdev *pdev,
					    enum supported_6g_pwr_types in_6g_pwr_mode,
					    qdf_freq_t *first_valid_freq,
					    int bw);
#endif /* __WLAN_REG_CHANNEL_API_H */
