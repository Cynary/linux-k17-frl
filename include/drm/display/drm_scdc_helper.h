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

#ifndef DRM_SCDC_HELPER_H
#define DRM_SCDC_HELPER_H

#include <linux/types.h>

#include <drm/display/drm_scdc.h>

struct drm_connector;
struct i2c_adapter;

ssize_t drm_scdc_read(struct i2c_adapter *adapter, u8 offset, void *buffer,
		      size_t size);
ssize_t drm_scdc_write(struct i2c_adapter *adapter, u8 offset,
		       const void *buffer, size_t size);

enum drm_scdc_frl_ltp {
	SCDC_FRL_NO_LTP = 0,
	SCDC_FRL_LTP1,
	SCDC_FRL_LTP2,
	SCDC_FRL_LTP3,
	SCDC_FRL_LTP4,
	SCDC_FRL_LTP5,
	SCDC_FRL_LTP6,
	SCDC_FRL_LTP7,
	SCDC_FRL_LTP8,
	SCDC_FRL_CHNG_FFE = 0xE,
	SCDC_FRL_CHNG_RATE = 0xF,
};

/**
 * drm_scdc_readb - read a single byte from SCDC
 * @adapter: I2C adapter
 * @offset: offset of register to read
 * @value: return location for the register value
 *
 * Reads a single byte from SCDC. This is a convenience wrapper around the
 * drm_scdc_read() function.
 *
 * Returns:
 * 0 on success or a negative error code on failure.
 */
static inline int drm_scdc_readb(struct i2c_adapter *adapter, u8 offset,
				 u8 *value)
{
	return drm_scdc_read(adapter, offset, value, sizeof(*value));
}

/**
 * drm_scdc_writeb - write a single byte to SCDC
 * @adapter: I2C adapter
 * @offset: offset of register to read
 * @value: return location for the register value
 *
 * Writes a single byte to SCDC. This is a convenience wrapper around the
 * drm_scdc_write() function.
 *
 * Returns:
 * 0 on success or a negative error code on failure.
 */
static inline int drm_scdc_writeb(struct i2c_adapter *adapter, u8 offset,
				  u8 value)
{
	return drm_scdc_write(adapter, offset, &value, sizeof(value));
}

bool drm_scdc_get_scrambling_status(struct drm_connector *connector);

bool drm_scdc_set_scrambling(struct drm_connector *connector, bool enable);
bool drm_scdc_set_high_tmds_clock_ratio(struct drm_connector *connector, bool set);
u8 drm_scdc_read_update_flags(struct i2c_adapter *adapter);
int drm_scdc_clear_update_flags(struct i2c_adapter *adapter, u8 update_flags);
u8 drm_scdc_read_status_flags(struct i2c_adapter *adapter);
int drm_scdc_config_frl(struct i2c_adapter *adapter, int frl_rate,
			int num_lanes, int ffe_levels);
int drm_scdc_disable_frl(struct i2c_adapter *adapter);
int drm_scdc_get_ltp(struct i2c_adapter *adapter,
		     enum drm_scdc_frl_ltp ltp[4]);

#endif
