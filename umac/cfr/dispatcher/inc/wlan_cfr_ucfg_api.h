/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#ifndef _WLAN_CFR_UCFG_API_H_
#define _WLAN_CFR_UCFG_API_H_

#include <wlan_objmgr_peer_obj.h>
#include <wlan_objmgr_pdev_obj.h>

/**
 * ucfg_cfr_start_capture() - function to start cfr capture
 * @pdev: pointer to pdev object
 * @peer: pointer to peer object
 * @cfr_params: config params to cfr capture
 *
 * Return: startus of start capture.
 */
int ucfg_cfr_start_capture(struct wlan_objmgr_pdev *pdev,
			   struct wlan_objmgr_peer *peer,
			   struct cfr_capture_params *cfr_params);

/**
 * ucfg_cfr_stop_capture() - function to stop cfr capture
 * @pdev: pointer to pdev object
 * @peer: pointer to peer object
 *
 * Return: startus of stop capture.
 */
int ucfg_cfr_stop_capture(struct wlan_objmgr_pdev *pdev,
			  struct wlan_objmgr_peer *peer);

/**
 * ucfg_cfr_list_peers() - Lists total number of peers with cfr capture enabled
 * @pdev: pointer to pdev object
 *
 * Return: number of peers with cfr capture enabled
 */
int ucfg_cfr_list_peers(struct wlan_objmgr_pdev *pdev);
#endif
