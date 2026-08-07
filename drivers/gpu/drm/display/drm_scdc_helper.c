/*
 * Copyright (c) 2015 NVIDIA Corporation. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <linux/export.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <drm/display/drm_scdc_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <drm/drm_print.h>

/**
 * DOC: scdc helpers
 *
 * Status and Control Data Channel (SCDC) is a mechanism introduced by the
 * HDMI 2.0 specification. It is a point-to-point protocol that allows the
 * HDMI source and HDMI sink to exchange data. The same I2C interface that
 * is used to access EDID serves as the transport mechanism for SCDC.
 *
 * Note: The SCDC status is going to be lost when the display is
 * disconnected. This can happen physically when the user disconnects
 * the cable, but also when a display is switched on (such as waking up
 * a TV).
 *
 * This is further complicated by the fact that, upon a disconnection /
 * reconnection, KMS won't change the mode on its own. This means that
 * one can't just rely on setting the SCDC status on enable, but also
 * has to track the connector status changes using interrupts and
 * restore the SCDC status. The typical solution for this is to trigger an
 * empty modeset in drm_connector_helper_funcs.detect_ctx(), like what vc4 does
 * in vc4_hdmi_reset_link().
 */

#define SCDC_I2C_SLAVE_ADDRESS 0x54

/**
 * drm_scdc_read - read a block of data from SCDC
 * @adapter: I2C controller
 * @offset: start offset of block to read
 * @buffer: return location for the block to read
 * @size: size of the block to read
 *
 * Reads a block of data from SCDC, starting at a given offset.
 *
 * Returns:
 * 0 on success, negative error code on failure.
 */
ssize_t drm_scdc_read(struct i2c_adapter *adapter, u8 offset, void *buffer,
		      size_t size)
{
	int ret;
	struct i2c_msg msgs[2] = {
		{
			.addr = SCDC_I2C_SLAVE_ADDRESS,
			.flags = 0,
			.len = 1,
			.buf = &offset,
		}, {
			.addr = SCDC_I2C_SLAVE_ADDRESS,
			.flags = I2C_M_RD,
			.len = size,
			.buf = buffer,
		}
	};

	ret = i2c_transfer(adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		return ret;
	if (ret != ARRAY_SIZE(msgs))
		return -EPROTO;

	return 0;
}
EXPORT_SYMBOL(drm_scdc_read);

/**
 * drm_scdc_write - write a block of data to SCDC
 * @adapter: I2C controller
 * @offset: start offset of block to write
 * @buffer: block of data to write
 * @size: size of the block to write
 *
 * Writes a block of data to SCDC, starting at a given offset.
 *
 * Returns:
 * 0 on success, negative error code on failure.
 */
ssize_t drm_scdc_write(struct i2c_adapter *adapter, u8 offset,
		       const void *buffer, size_t size)
{
	struct i2c_msg msg = {
		.addr = SCDC_I2C_SLAVE_ADDRESS,
		.flags = 0,
		.len = 1 + size,
		.buf = NULL,
	};
	void *data;
	int err;

	data = kmalloc(1 + size, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	msg.buf = data;

	memcpy(data, &offset, sizeof(offset));
	memcpy(data + 1, buffer, size);

	err = i2c_transfer(adapter, &msg, 1);

	kfree(data);

	if (err < 0)
		return err;
	if (err != 1)
		return -EPROTO;

	return 0;
}
EXPORT_SYMBOL(drm_scdc_write);

/**
 * drm_scdc_get_scrambling_status - what is status of scrambling?
 * @connector: connector
 *
 * Reads the scrambler status over SCDC, and checks the
 * scrambling status.
 *
 * Returns:
 * True if the scrambling is enabled, false otherwise.
 */
bool drm_scdc_get_scrambling_status(struct drm_connector *connector)
{
	u8 status;
	int ret;

	ret = drm_scdc_readb(connector->ddc, SCDC_SCRAMBLER_STATUS, &status);
	if (ret < 0) {
		drm_dbg_kms(connector->dev,
			    "[CONNECTOR:%d:%s] Failed to read scrambling status: %d\n",
			    connector->base.id, connector->name, ret);
		return false;
	}

	return status & SCDC_SCRAMBLING_STATUS;
}
EXPORT_SYMBOL(drm_scdc_get_scrambling_status);

/**
 * drm_scdc_set_scrambling - enable scrambling
 * @connector: connector
 * @enable: bool to indicate if scrambling is to be enabled/disabled
 *
 * Writes the TMDS config register over SCDC channel, and:
 * enables scrambling when enable = 1
 * disables scrambling when enable = 0
 *
 * Returns:
 * True if scrambling is set/reset successfully, false otherwise.
 */
bool drm_scdc_set_scrambling(struct drm_connector *connector,
			     bool enable)
{
	u8 config;
	int ret;

	ret = drm_scdc_readb(connector->ddc, SCDC_TMDS_CONFIG, &config);
	if (ret < 0) {
		drm_dbg_kms(connector->dev,
			    "[CONNECTOR:%d:%s] Failed to read TMDS config: %d\n",
			    connector->base.id, connector->name, ret);
		return false;
	}

	if (enable)
		config |= SCDC_SCRAMBLING_ENABLE;
	else
		config &= ~SCDC_SCRAMBLING_ENABLE;

	ret = drm_scdc_writeb(connector->ddc, SCDC_TMDS_CONFIG, config);
	if (ret < 0) {
		drm_dbg_kms(connector->dev,
			    "[CONNECTOR:%d:%s] Failed to enable scrambling: %d\n",
			    connector->base.id, connector->name, ret);
		return false;
	}

	return true;
}
EXPORT_SYMBOL(drm_scdc_set_scrambling);

/**
 * drm_scdc_set_high_tmds_clock_ratio - set TMDS clock ratio
 * @connector: connector
 * @set: ret or reset the high clock ratio
 *
 *
 *	TMDS clock ratio calculations go like this:
 *		TMDS character = 10 bit TMDS encoded value
 *
 *		TMDS character rate = The rate at which TMDS characters are
 *		transmitted (Mcsc)
 *
 *		TMDS bit rate = 10x TMDS character rate
 *
 *	As per the spec:
 *		TMDS clock rate for pixel clock < 340 MHz = 1x the character
 *		rate = 1/10 pixel clock rate
 *
 *		TMDS clock rate for pixel clock > 340 MHz = 0.25x the character
 *		rate = 1/40 pixel clock rate
 *
 *	Writes to the TMDS config register over SCDC channel, and:
 *		sets TMDS clock ratio to 1/40 when set = 1
 *
 *		sets TMDS clock ratio to 1/10 when set = 0
 *
 * Returns:
 * True if write is successful, false otherwise.
 */
bool drm_scdc_set_high_tmds_clock_ratio(struct drm_connector *connector,
					bool set)
{
	u8 config;
	int ret;

	ret = drm_scdc_readb(connector->ddc, SCDC_TMDS_CONFIG, &config);
	if (ret < 0) {
		drm_dbg_kms(connector->dev,
			    "[CONNECTOR:%d:%s] Failed to read TMDS config: %d\n",
			    connector->base.id, connector->name, ret);
		return false;
	}

	if (set)
		config |= SCDC_TMDS_BIT_CLOCK_RATIO_BY_40;
	else
		config &= ~SCDC_TMDS_BIT_CLOCK_RATIO_BY_40;

	ret = drm_scdc_writeb(connector->ddc, SCDC_TMDS_CONFIG, config);
	if (ret < 0) {
		drm_dbg_kms(connector->dev,
			    "[CONNECTOR:%d:%s] Failed to set TMDS clock ratio: %d\n",
			    connector->base.id, connector->name, ret);
		return false;
	}

	/*
	 * The spec says that a source should wait minimum 1ms and maximum
	 * 100ms after writing the TMDS config for clock ratio. Lets allow a
	 * wait of up to 2ms here.
	 */
	usleep_range(1000, 2000);
	return true;
}
EXPORT_SYMBOL(drm_scdc_set_high_tmds_clock_ratio);

/**
 * drm_scdc_read_update_flags - read the SCDC update flags
 * @adapter: I2C adapter for DDC channel
 *
 * Returns:
 * 8bit SCDC update
 */
u8 drm_scdc_read_update_flags(struct i2c_adapter *adapter)
{
	u8 update = 0;
	int ret;

	ret = drm_scdc_readb(adapter, SCDC_UPDATE_0, &update);
	if (ret < 0)
		DRM_DEBUG_KMS("Failed to read scdc update: %d\n", ret);

	return update;
}
EXPORT_SYMBOL(drm_scdc_read_update_flags);

/**
 * drm_scdc_clear_update_flags - Clears the given update flag bits by writing 1
 * @adapter: I2C adapter for DDC channel
 * @update_flags: update flag bits to be cleared
 *
 * Returns:
 * 0 on success, negative error code otherwise.
 */
int drm_scdc_clear_update_flags(struct i2c_adapter *adapter, u8 update_flags)
{
	int ret;

	/* Read Request Test is the only update flag the source cannot clear */
	if (update_flags & SCDC_READ_REQUEST_TEST) {
		DRM_DEBUG_KMS("SCDC Update flag/s 0x%x cannot be cleared\n",
			      update_flags);
		return -EINVAL;
	}

	ret = drm_scdc_writeb(adapter, SCDC_UPDATE_0, update_flags);
	if (ret < 0) {
		DRM_DEBUG_KMS("Failed to clear SCDC Update flag/s\n");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(drm_scdc_clear_update_flags);

/**
 * drm_scdc_read_status_flags - Read the status flags from offset 0x40
 * @adapter: I2C adapter for DDC channel
 *
 * Returns:
 * 8 bit value read from the offset 0x40
 */
u8 drm_scdc_read_status_flags(struct i2c_adapter *adapter)
{
	u8 update = 0;
	int ret;

	ret = drm_scdc_readb(adapter, SCDC_STATUS_FLAGS_0, &update);
	if (ret < 0)
		DRM_DEBUG_KMS("Failed to read scdc status flag: %d\n", ret);

	return update;
}
EXPORT_SYMBOL(drm_scdc_read_status_flags);

/**
 * drm_scdc_config_frl - configure the sink for starting FRL training
 * @adapter: I2C adapter for DDC channel
 * @frl_rate: FRL rate per lane in Gbps (3, 6, 8, 10, 12, 16, 20 or 24).
 * @num_lanes: no. of lanes required, can be either 3 or 4.
 * @ffe_levels: max TxFFE level supported for the given FRL rate;
 *              0-3 for rates up to 12 Gbps, 0-7 for 16/20/24 Gbps.
 *              A prohibited value is clamped to 0 (per spec).
 *
 * Returns:
 * 0 if the SCDC offsets for FRL training are successfully configured,
 * negative error code otherwise.
 */
int drm_scdc_config_frl(struct i2c_adapter *adapter, int frl_rate,
			int num_lanes, int ffe_levels)
{
	u8 write_buf = 0;
	int ret;

	if (num_lanes > 4 || num_lanes < 3) {
		DRM_DEBUG_KMS("No. of lanes can be 3 or 4 only\n");
		return -EINVAL;
	}
	if (ffe_levels > 7 || (frl_rate < 16 && ffe_levels > 3)) {
		DRM_DEBUG_KMS("Prohibited Max FFE level %d, defaulting to Max FFE level 0\n",
			      ffe_levels);
		ffe_levels = 0;
	}

	switch (frl_rate) {
	case 3:
		write_buf |= (num_lanes == 3) ? SCDC_FRL_RATE_3GBPS_3LANES : 0;
		break;
	case 6:
		write_buf |= (num_lanes == 3) ? SCDC_FRL_RATE_6GBPS_3LANES :
			     SCDC_FRL_RATE_6GBPS_4LANES;
		break;
	case 8:
		write_buf |= (num_lanes == 4) ? SCDC_FRL_RATE_8GBPS_4LANES : 0;
		break;
	case 10:
		write_buf |= (num_lanes == 4) ? SCDC_FRL_RATE_10GBPS_4LANES : 0;
		break;
	case 12:
		write_buf |= (num_lanes == 4) ? SCDC_FRL_RATE_12GBPS_4LANES : 0;
		break;
	case 16:
		write_buf |= (num_lanes == 4) ? SCDC_FRL_RATE_16GBPS_4LANES : 0;
		break;
	case 20:
		write_buf |= (num_lanes == 4) ? SCDC_FRL_RATE_20GBPS_4LANES : 0;
		break;
	case 24:
		write_buf |= (num_lanes == 4) ? SCDC_FRL_RATE_24GBPS_4LANES : 0;
		break;
	default:
		DRM_DEBUG_KMS("Invalid FRL rate =%dGbps\n", frl_rate);
		return -EINVAL;
	}

	if (!write_buf) {
		DRM_DEBUG_KMS("Invalid FRL rate and lane combination\n");
		return -EINVAL;
	}

	write_buf |= (ffe_levels << SCDC_FFE_LEVELS_SHIFT);

	ret = drm_scdc_writeb(adapter, SCDC_CONFIG_1, write_buf);
	if (ret < 0) {
		DRM_DEBUG_KMS("Failed to write SCDC config: %d\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(drm_scdc_config_frl);

/**
 * drm_scdc_disable_frl - Clear FRL Rate indicating TMDS
 * @adapter: I2C adapter for DDC channel
 *
 * Returns:
 * 0 if the FRL Rate is successfully reset, negative error code otherwise.
 */
int drm_scdc_disable_frl(struct i2c_adapter *adapter)
{
	u8 buf = 0;
	int ret;

	ret = drm_scdc_readb(adapter, SCDC_CONFIG_1, &buf);
	if (ret < 0) {
		DRM_DEBUG_KMS("Failed to read SCDC_CONFIG_1\n");
		return ret;
	}

	buf &= ~SCDC_FRL_RATE_MASK;

	ret = drm_scdc_writeb(adapter, SCDC_CONFIG_1, buf);
	if (ret < 0) {
		DRM_DEBUG_KMS("Failed to reset FRL rate\n");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(drm_scdc_disable_frl);

/**
 * drm_scdc_get_ltp - get the Link training patterns for the 4 lanes
 * @adapter: I2C adapter for DDC channel
 * @ltp: pointer array for reading Link training patterns for the 4 lanes.
 *
 * Returns:
 * 0 on success also filling ltp out argument, negative error code otherwise.
 */
int drm_scdc_get_ltp(struct i2c_adapter *adapter,
		     enum drm_scdc_frl_ltp ltp[4])
{
	u8 buf;
	int ret;

	ret = drm_scdc_readb(adapter, SCDC_STATUS_FLAGS_1, &buf);
	if (ret < 0) {
		DRM_DEBUG_KMS("failed to read link training pattern for lanes 0/1 ret = %d\n", ret);
		return ret;
	}

	ltp[0] = buf & SCDC_LN_0_2_LTP_MASK;
	ltp[1] = (buf & SCDC_LN_1_3_LTP_MASK) >> 4;

	ret = drm_scdc_readb(adapter, SCDC_STATUS_FLAGS_2, &buf);
	if (ret < 0) {
		DRM_DEBUG_KMS("failed to read link training pattern for lanes 2/3 ret = %d\n", ret);
		return ret;
	}

	ltp[2] = buf & SCDC_LN_0_2_LTP_MASK;
	ltp[3] = (buf & SCDC_LN_1_3_LTP_MASK) >> 4;

	return 0;
}
EXPORT_SYMBOL(drm_scdc_get_ltp);
