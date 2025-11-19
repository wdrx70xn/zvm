/*
 * Copyright 2021 Google LLC.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DRIVERS_SYSCON_SYSCON_COMMON_H_
#define DRIVERS_SYSCON_SYSCON_COMMON_H_

#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pass current syscon info to subsys(like zvm) */
int z_info_syscon(const char *name, uint32_t addr_base,
                              uint32_t addr_size, const void *priv);

int __weak z_info_syscon(const char *name, uint32_t addr_base,
                              uint32_t addr_size, const void *priv)
{
     ARG_UNUSED(addr_base);
     ARG_UNUSED(addr_size);
     ARG_UNUSED(priv);
     return 0;
}

/**
 * @brief Align and check register address
 *
 * @param reg Pointer to the register address in question.
 * @param reg_size The size of the syscon register region.
 * @param reg_width The width of a single register (in bytes).
 * @return 0 if the register read is valid.
 * @return -EINVAL is the read is invalid.
 */
static inline int syscon_sanitize_reg(uint16_t *reg, size_t reg_size, uint8_t reg_width)
{
	/* Avoid unaligned readings */
	*reg = ROUND_DOWN(*reg, reg_width);

	/* Check for out-of-bounds readings */
	if (*reg >= reg_size) {
		return -EINVAL;
	}

	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_SYSCON_SYSCON_COMMON_H_ */
