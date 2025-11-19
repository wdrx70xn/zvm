/**
 * @file
 *
 * @brief Public APIs for Ethernet PHY drivers.
 */

/*
 * Copyright (c) 2021 IP-Logix Inc.
 * Copyright 2022 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_DRIVERS_PHY_H_
#define ZEPHYR_INCLUDE_DRIVERS_PHY_H_

/**
 * @brief Ethernet PHY Interface
 * @defgroup ethernet_phy Ethernet PHY Interface
 * @ingroup networking
 * @{
 */
#include <zephyr/types.h>
#include <zephyr/device.h>
#include "rk3588_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Ethernet link speeds. */
enum phy_link_speed {
	/** 10Base-T Half-Duplex */
	LINK_HALF_10BASE_T		= BIT(0),
	/** 10Base-T Full-Duplex */
	LINK_FULL_10BASE_T		= BIT(1),
	/** 100Base-T Half-Duplex */
	LINK_HALF_100BASE_T		= BIT(2),
	/** 100Base-T Full-Duplex */
	LINK_FULL_100BASE_T		= BIT(3),
	/** 1000Base-T Half-Duplex */
	LINK_HALF_1000BASE_T		= BIT(4),
	/** 1000Base-T Full-Duplex */
	LINK_FULL_1000BASE_T		= BIT(5),
};

/**
 * @brief Check if phy link is full duplex.
 *
 * @param x Link capabilities
 *
 * @return True if link is full duplex, false if not.
 */
#define PHY_LINK_IS_FULL_DUPLEX(x)	(x & (BIT(1) | BIT(3) | BIT(5)))

/**
 * @brief Check if phy link speed is 1 Gbit/sec.
 *
 * @param x Link capabilities
 *
 * @return True if link is 1 Gbit/sec, false if not.
 */
#define PHY_LINK_IS_SPEED_1000M(x)	(x & (BIT(4) | BIT(5)))

/**
 * @brief Check if phy link speed is 100 Mbit/sec.
 *
 * @param x Link capabilities
 *
 * @return True if link is 1 Mbit/sec, false if not.
 */
#define PHY_LINK_IS_SPEED_100M(x)	(x & (BIT(2) | BIT(3)))

/** @brief Link state */
struct phy_link_state {
	/** Link speed */
	enum phy_link_speed speed;
	/** When true the link is active and connected */
	bool is_up;
};

/**
 * @typedef phy_callback_t
 * @brief Define the callback function signature for
 * `phy_link_callback_set()` function.
 *
 * @param dev       PHY device structure
 * @param state     Pointer to link_state structure.
 * @param user_data Pointer to data specified by user
 */
typedef void (*phy_callback_t)(const struct device *dev,
			       struct phy_link_state *state,
			       void *user_data);

/**
 * @cond INTERNAL_HIDDEN
 *
 * These are for internal use only, so skip these in
 * public documentation.
 */
__subsystem struct ethphy_driver_api {
	/** Get link state */
	int (*get_link)(const struct device *dev,
			struct phy_link_state *state);

	/** Configure link */
	int (*cfg_link)(const struct device *dev,
			enum phy_link_speed adv_speeds);

	/** Set callback to be invoked when link state changes. */
	int (*link_cb_set)(const struct device *dev, phy_callback_t cb,
			   void *user_data);

	/** Read PHY register */
	int (*read)(const struct device *dev, uint16_t reg_addr,
		    uint32_t *data);

	/** Write PHY register */
	int (*write)(const struct device *dev, uint16_t reg_addr,
		     uint32_t data);
};
/**
 * @endcond
 */

/**
 * @brief      Configure PHY link
 *
 * This route configures the advertised link speeds.
 *
 * @param[in]  dev     PHY device structure
 * @param      speeds  OR'd link speeds to be advertised by the PHY
 *
 * @retval 0 If successful.
 * @retval -EIO If communication with PHY failed.
 * @retval -ENOTSUP If not supported.
 */
static inline int phy_configure_link(const struct device *dev,
				     enum phy_link_speed speeds)
{
	const struct ethphy_driver_api *api =
		(const struct ethphy_driver_api *)dev->api;

	return api->cfg_link(dev, speeds);
}

/**
 * @brief      Get PHY link state
 *
 * Returns the current state of the PHY link. This can be used by
 * to determine when a link is up and the negotiated link speed.
 *
 *
 * @param[in]  dev    PHY device structure
 * @param      state  Pointer to receive PHY state
 *
 * @retval 0 If successful.
 * @retval -EIO If communication with PHY failed.
 */
static inline int phy_get_link_state(const struct device *dev,
				     struct phy_link_state *state)
{
	const struct ethphy_driver_api *api =
		(const struct ethphy_driver_api *)dev->api;

	return api->get_link(dev, state);
}

/**
 * @brief      Set link state change callback
 *
 * Sets a callback that is invoked when link state changes. This is the
 * preferred method for ethernet drivers to be notified of the PHY link
 * state change.
 *
 * @param[in]  dev        PHY device structure
 * @param      callback   Callback handler
 * @param      user_data  Pointer to data specified by user.
 *
 * @retval 0 If successful.
 * @retval -ENOTSUP If not supported.
 */
static inline int phy_link_callback_set(const struct device *dev,
					phy_callback_t callback,
					void *user_data)
{
	const struct ethphy_driver_api *api =
		(const struct ethphy_driver_api *)dev->api;

	return api->link_cb_set(dev, callback, user_data);
}

/**
 * @brief      Read PHY registers
 *
 * This routine provides a generic interface to read from a PHY register.
 *
 * @param[in]  dev       PHY device structure
 * @param[in]  reg_addr  Register address
 * @param      value     Pointer to receive read value
 *
 * @retval 0 If successful.
 * @retval -EIO If communication with PHY failed.
 */
static inline int phy_read(const struct device *dev, uint16_t reg_addr,
			   uint32_t *value)
{
	const struct ethphy_driver_api *api =
		(const struct ethphy_driver_api *)dev->api;

	return api->read(dev, reg_addr, value);
}

/**
 * @brief      Write PHY register
 *
 * This routine provides a generic interface to write to a PHY register.
 *
 * @param[in]  dev       PHY device structure
 * @param[in]  reg_addr  Register address
 * @param[in]  value     Value to write
 *
 * @retval 0 If successful.
 * @retval -EIO If communication with PHY failed.
 */
static inline int phy_write(const struct device *dev, uint16_t reg_addr,
			    uint32_t value)
{
	const struct ethphy_driver_api *api =
		(const struct ethphy_driver_api *)dev->api;

	return api->write(dev, reg_addr, value);
}


#ifdef __cplusplus
}
#endif


/**
 * @}
 */

// #define true 1
// #define false 0

/* Duplex, half or full. */
#define DUPLEX_HALF    0x00
#define DUPLEX_FULL    0x01
#define DUPLEX_UNKNOWN 0xff

// #define	ETIMEDOUT	110	/* Connection timed out */

/* The following are all involved in forcing a particular link
 * mode for the device for setting things.  When getting the
 * devices settings, these indicate the current mode and whether
 * it was forced up into this mode or autonegotiated.
 */

/* The forced speed, in units of 1Mb. All values 0 to INT_MAX are legal.
 * Update drivers/net/phy/phy.c:phy_speed_to_str() and
 * drivers/net/bonding/bond_3ad.c:__get_link_speed() when adding new values.
 */
#define SPEED_10     10
#define SPEED_100    100
#define SPEED_1000   1000
#define SPEED_2500   2500
#define SPEED_5000   5000
#define SPEED_10000  10000
#define SPEED_14000  14000
#define SPEED_20000  20000
#define SPEED_25000  25000
#define SPEED_40000  40000
#define SPEED_50000  50000
#define SPEED_56000  56000
#define SPEED_100000 100000

#define SPEED_UNKNOWN -1

/* Enable or disable autonegotiation. */
#define AUTONEG_DISABLE 0x00
#define AUTONEG_ENABLE  0x01

/* Link mode bit indices */
enum ethtool_link_mode_bit_indices {
	ETHTOOL_LINK_MODE_10baseT_Half_BIT = 0,
	ETHTOOL_LINK_MODE_10baseT_Full_BIT = 1,
	ETHTOOL_LINK_MODE_100baseT_Half_BIT = 2,
	ETHTOOL_LINK_MODE_100baseT_Full_BIT = 3,
	ETHTOOL_LINK_MODE_1000baseT_Half_BIT = 4,
	ETHTOOL_LINK_MODE_1000baseT_Full_BIT = 5,
	ETHTOOL_LINK_MODE_Autoneg_BIT = 6,
	ETHTOOL_LINK_MODE_TP_BIT = 7,
	ETHTOOL_LINK_MODE_AUI_BIT = 8,
	ETHTOOL_LINK_MODE_MII_BIT = 9,
	ETHTOOL_LINK_MODE_FIBRE_BIT = 10,
	ETHTOOL_LINK_MODE_BNC_BIT = 11,
	ETHTOOL_LINK_MODE_10000baseT_Full_BIT = 12,
	ETHTOOL_LINK_MODE_Pause_BIT = 13,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT = 14,
	ETHTOOL_LINK_MODE_2500baseX_Full_BIT = 15,
	ETHTOOL_LINK_MODE_Backplane_BIT = 16,
	ETHTOOL_LINK_MODE_1000baseKX_Full_BIT = 17,
	ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT = 18,
	ETHTOOL_LINK_MODE_10000baseKR_Full_BIT = 19,
	ETHTOOL_LINK_MODE_10000baseR_FEC_BIT = 20,
	ETHTOOL_LINK_MODE_20000baseMLD2_Full_BIT = 21,
	ETHTOOL_LINK_MODE_20000baseKR2_Full_BIT = 22,
	ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT = 23,
	ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT = 24,
	ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT = 25,
	ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT = 26,
	ETHTOOL_LINK_MODE_56000baseKR4_Full_BIT = 27,
	ETHTOOL_LINK_MODE_56000baseCR4_Full_BIT = 28,
	ETHTOOL_LINK_MODE_56000baseSR4_Full_BIT = 29,
	ETHTOOL_LINK_MODE_56000baseLR4_Full_BIT = 30,
	ETHTOOL_LINK_MODE_25000baseCR_Full_BIT = 31,
	ETHTOOL_LINK_MODE_25000baseKR_Full_BIT = 32,
	ETHTOOL_LINK_MODE_25000baseSR_Full_BIT = 33,
	ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT = 34,
	ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT = 35,
	ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT = 36,
	ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT = 37,
	ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT = 38,
	ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT = 39,
	ETHTOOL_LINK_MODE_50000baseSR2_Full_BIT = 40,
	ETHTOOL_LINK_MODE_1000baseX_Full_BIT = 41,
	ETHTOOL_LINK_MODE_10000baseCR_Full_BIT = 42,
	ETHTOOL_LINK_MODE_10000baseSR_Full_BIT = 43,
	ETHTOOL_LINK_MODE_10000baseLR_Full_BIT = 44,
	ETHTOOL_LINK_MODE_10000baseLRM_Full_BIT = 45,
	ETHTOOL_LINK_MODE_10000baseER_Full_BIT = 46,
	ETHTOOL_LINK_MODE_2500baseT_Full_BIT = 47,
	ETHTOOL_LINK_MODE_5000baseT_Full_BIT = 48,
	ETHTOOL_LINK_MODE_FEC_NONE_BIT = 49,
	ETHTOOL_LINK_MODE_FEC_RS_BIT = 50,
	ETHTOOL_LINK_MODE_FEC_BASER_BIT = 51,

	/* Last allowed bit for __ETHTOOL_LINK_MODE_LEGACY_MASK is bit
	 * 31. Please do NOT define any SUPPORTED_* or ADVERTISED_*
	 * macro for bits > 31. The only way to use indices > 31 is to
	 * use the new ETHTOOL_GLINKSETTINGS/ETHTOOL_SLINKSETTINGS API.
	 */

	__ETHTOOL_LINK_MODE_LAST = ETHTOOL_LINK_MODE_FEC_BASER_BIT,
};

#define __ETHTOOL_LINK_MODE_LEGACY_MASK(base_name) (1UL << (ETHTOOL_LINK_MODE_##base_name##_BIT))

/* DEPRECATED macros. Please migrate to
 * ETHTOOL_GLINKSETTINGS/ETHTOOL_SLINKSETTINGS API. Please do NOT
 * define any new SUPPORTED_* macro for bits > 31.
 */
#define SUPPORTED_10baseT_Half       __ETHTOOL_LINK_MODE_LEGACY_MASK(10baseT_Half)
#define SUPPORTED_10baseT_Full       __ETHTOOL_LINK_MODE_LEGACY_MASK(10baseT_Full)
#define SUPPORTED_100baseT_Half      __ETHTOOL_LINK_MODE_LEGACY_MASK(100baseT_Half)
#define SUPPORTED_100baseT_Full      __ETHTOOL_LINK_MODE_LEGACY_MASK(100baseT_Full)
#define SUPPORTED_1000baseT_Half     __ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseT_Half)
#define SUPPORTED_1000baseT_Full     __ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseT_Full)
#define SUPPORTED_Autoneg            __ETHTOOL_LINK_MODE_LEGACY_MASK(Autoneg)
#define SUPPORTED_TP                 __ETHTOOL_LINK_MODE_LEGACY_MASK(TP)
#define SUPPORTED_AUI                __ETHTOOL_LINK_MODE_LEGACY_MASK(AUI)
#define SUPPORTED_MII                __ETHTOOL_LINK_MODE_LEGACY_MASK(MII)
#define SUPPORTED_FIBRE              __ETHTOOL_LINK_MODE_LEGACY_MASK(FIBRE)
#define SUPPORTED_BNC                __ETHTOOL_LINK_MODE_LEGACY_MASK(BNC)
#define SUPPORTED_10000baseT_Full    __ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseT_Full)
#define SUPPORTED_Pause              __ETHTOOL_LINK_MODE_LEGACY_MASK(Pause)
#define SUPPORTED_Asym_Pause         __ETHTOOL_LINK_MODE_LEGACY_MASK(Asym_Pause)
#define SUPPORTED_2500baseX_Full     __ETHTOOL_LINK_MODE_LEGACY_MASK(2500baseX_Full)
#define SUPPORTED_Backplane          __ETHTOOL_LINK_MODE_LEGACY_MASK(Backplane)
#define SUPPORTED_1000baseKX_Full    __ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseKX_Full)
#define SUPPORTED_10000baseKX4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseKX4_Full)
#define SUPPORTED_10000baseKR_Full   __ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseKR_Full)
#define SUPPORTED_10000baseR_FEC     __ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseR_FEC)
#define SUPPORTED_20000baseMLD2_Full __ETHTOOL_LINK_MODE_LEGACY_MASK(20000baseMLD2_Full)
#define SUPPORTED_20000baseKR2_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(20000baseKR2_Full)
#define SUPPORTED_40000baseKR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseKR4_Full)
#define SUPPORTED_40000baseCR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseCR4_Full)
#define SUPPORTED_40000baseSR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseSR4_Full)
#define SUPPORTED_40000baseLR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseLR4_Full)
#define SUPPORTED_56000baseKR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseKR4_Full)
#define SUPPORTED_56000baseCR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseCR4_Full)
#define SUPPORTED_56000baseSR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseSR4_Full)
#define SUPPORTED_56000baseLR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseLR4_Full)
/* Please do not define any new SUPPORTED_* macro for bits > 31, see
 * notice above.
 */

/*
 * DEPRECATED macros. Please migrate to
 * ETHTOOL_GLINKSETTINGS/ETHTOOL_SLINKSETTINGS API. Please do NOT
 * define any new ADERTISE_* macro for bits > 31.
 */
#define ADVERTISED_10baseT_Half       __ETHTOOL_LINK_MODE_LEGACY_MASK(10baseT_Half)
#define ADVERTISED_10baseT_Full       __ETHTOOL_LINK_MODE_LEGACY_MASK(10baseT_Full)
#define ADVERTISED_100baseT_Half      __ETHTOOL_LINK_MODE_LEGACY_MASK(100baseT_Half)
#define ADVERTISED_100baseT_Full      __ETHTOOL_LINK_MODE_LEGACY_MASK(100baseT_Full)
#define ADVERTISED_1000baseT_Half     __ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseT_Half)
#define ADVERTISED_1000baseT_Full     __ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseT_Full)
#define ADVERTISED_Autoneg            __ETHTOOL_LINK_MODE_LEGACY_MASK(Autoneg)
#define ADVERTISED_TP                 __ETHTOOL_LINK_MODE_LEGACY_MASK(TP)
#define ADVERTISED_AUI                __ETHTOOL_LINK_MODE_LEGACY_MASK(AUI)
#define ADVERTISED_MII                __ETHTOOL_LINK_MODE_LEGACY_MASK(MII)
#define ADVERTISED_FIBRE              __ETHTOOL_LINK_MODE_LEGACY_MASK(FIBRE)
#define ADVERTISED_BNC                __ETHTOOL_LINK_MODE_LEGACY_MASK(BNC)
#define ADVERTISED_10000baseT_Full    __ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseT_Full)
#define ADVERTISED_Pause              __ETHTOOL_LINK_MODE_LEGACY_MASK(Pause)
#define ADVERTISED_Asym_Pause         __ETHTOOL_LINK_MODE_LEGACY_MASK(Asym_Pause)
#define ADVERTISED_2500baseX_Full     __ETHTOOL_LINK_MODE_LEGACY_MASK(2500baseX_Full)
#define ADVERTISED_Backplane          __ETHTOOL_LINK_MODE_LEGACY_MASK(Backplane)
#define ADVERTISED_1000baseKX_Full    __ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseKX_Full)
#define ADVERTISED_10000baseKX4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseKX4_Full)
#define ADVERTISED_10000baseKR_Full   __ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseKR_Full)
#define ADVERTISED_10000baseR_FEC     __ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseR_FEC)
#define ADVERTISED_20000baseMLD2_Full __ETHTOOL_LINK_MODE_LEGACY_MASK(20000baseMLD2_Full)
#define ADVERTISED_20000baseKR2_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(20000baseKR2_Full)
#define ADVERTISED_40000baseKR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseKR4_Full)
#define ADVERTISED_40000baseCR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseCR4_Full)
#define ADVERTISED_40000baseSR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseSR4_Full)
#define ADVERTISED_40000baseLR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseLR4_Full)
#define ADVERTISED_56000baseKR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseKR4_Full)
#define ADVERTISED_56000baseCR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseCR4_Full)
#define ADVERTISED_56000baseSR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseSR4_Full)
#define ADVERTISED_56000baseLR4_Full  __ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseLR4_Full)

#define ADVERTISED_1000baseX_Half (1 << 21)
#define ADVERTISED_1000baseX_Full (1 << 22)

#if 0
 /* Indicates what features are supported by the interface. */
#define SUPPORTED_10baseT_Half      (1 << 0)
#define SUPPORTED_10baseT_Full      (1 << 1)
#define SUPPORTED_100baseT_Half     (1 << 2)
#define SUPPORTED_100baseT_Full     (1 << 3)
#define SUPPORTED_1000baseT_Half    (1 << 4)
#define SUPPORTED_1000baseT_Full    (1 << 5)
#define SUPPORTED_Autoneg           (1 << 6)
#define SUPPORTED_TP                (1 << 7)
#define SUPPORTED_AUI               (1 << 8)
#define SUPPORTED_MII               (1 << 9)
#define SUPPORTED_FIBRE             (1 << 10)
#define SUPPORTED_BNC               (1 << 11)
#define SUPPORTED_10000baseT_Full   (1 << 12)
#define SUPPORTED_Pause             (1 << 13)
#define SUPPORTED_Asym_Pause        (1 << 14)
#define SUPPORTED_2500baseX_Full    (1 << 15)
#define SUPPORTED_Backplane         (1 << 16)
#define SUPPORTED_1000baseKX_Full   (1 << 17)
#define SUPPORTED_10000baseKX4_Full (1 << 18)
#define SUPPORTED_10000baseKR_Full  (1 << 19)
#define SUPPORTED_10000baseR_FEC    (1 << 20)
#endif

#define SUPPORTED_1000baseX_Half (1 << 21)
#define SUPPORTED_1000baseX_Full (1 << 22)

#define PHY_FIXED_ID 0xa5a55a5a

#define PHY_MAX_ADDR 32

#define PHY_FLAG_BROKEN_RESET (1 << 0) /* soft reset not supported */

#define PHY_DEFAULT_FEATURES (SUPPORTED_Autoneg | SUPPORTED_TP | SUPPORTED_MII)

#define PHY_10BT_FEATURES (SUPPORTED_10baseT_Half | SUPPORTED_10baseT_Full)

#define PHY_100BT_FEATURES (SUPPORTED_100baseT_Half | SUPPORTED_100baseT_Full)

#define PHY_1000BT_FEATURES (SUPPORTED_1000baseT_Half | SUPPORTED_1000baseT_Full)

#define PHY_BASIC_FEATURES (PHY_10BT_FEATURES | PHY_100BT_FEATURES | PHY_DEFAULT_FEATURES)

#define PHY_GBIT_FEATURES (PHY_BASIC_FEATURES | PHY_1000BT_FEATURES)

#define PHY_10G_FEATURES (PHY_GBIT_FEATURES | SUPPORTED_10000baseT_Full)

#ifndef PHY_ANEG_TIMEOUT
#define PHY_ANEG_TIMEOUT 4000
#endif

/*
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

struct list_head {
	struct list_head *next, *prev;
};

struct phy_device;

#define MDIO_NAME_LEN 32

struct mii_dev {
	char name[MDIO_NAME_LEN];
	void *priv;
	int (*read)(struct mii_dev *bus, int addr, int devad, int reg);
	int (*write)(struct mii_dev *bus, int addr, int devad, int reg, u16 val);
	int (*reset)(struct mii_dev *bus);
	struct phy_device *phymap[PHY_MAX_ADDR];
	u32 phy_mask;
};

/* struct phy_driver: a structure which defines PHY behavior
 *
 * uid will contain a number which represents the PHY.  During
 * startup, the driver will poll the PHY to find out what its
 * UID--as defined by registers 2 and 3--is.  The 32-bit result
 * gotten from the PHY will be masked to
 * discard any bits which may change based on revision numbers
 * unimportant to functionality
 *
 */
struct phy_driver {
	char *name;
	unsigned int uid;
	unsigned int mask;
	unsigned int mmds;

	u32 features;

	/* Called to do any driver startup necessities */
	/* Will be called during phy_connect */
	int (*probe)(struct phy_device *phydev);

	/* Called to configure the PHY, and modify the controller
	 * based on the results.  Should be called after phy_connect */
	int (*config)(struct phy_device *phydev);

	/* Called when starting up the controller */
	int (*startup)(struct phy_device *phydev);

	/* Called when bringing down the controller */
	int (*shutdown)(struct phy_device *phydev);

	int (*readext)(struct phy_device *phydev, int addr, int devad, int reg);
	int (*writeext)(struct phy_device *phydev, int addr, int devad, int reg, u16 val);

	/* Phy specific driver override for reading a MMD register */
	int (*read_mmd)(struct phy_device *phydev, int devad, int reg);

	/* Phy specific driver override for writing a MMD register */
	int (*write_mmd)(struct phy_device *phydev, int devad, int reg, u16 val);

	struct list_head list;
};

/**
 * struct udevice - An instance of a driver
 *
 * This holds information about a device, which is a driver bound to a
 * particular port or peripheral (essentially a driver instance).
 *
 * A device will come into existence through a 'bind' call, either due to
 * a U_BOOT_DEVICE() macro (in which case platdata is non-NULL) or a node
 * in the device tree (in which case of_offset is >= 0). In the latter case
 * we translate the device tree information into platdata in a function
 * implemented by the driver ofdata_to_platdata method (called just before the
 * probe method if the device has a device tree node.
 *
 * All three of platdata, priv and uclass_priv can be allocated by the
 * driver, or you can use the auto_alloc_size members of struct driver and
 * struct uclass_driver to have driver model do this automatically.
 *
 * @driver: The driver used by this device
 * @name: Name of device, typically the FDT node name
 * @platdata: Configuration data for this device
 * @parent_platdata: The parent bus's configuration data for this device
 * @uclass_platdata: The uclass's configuration data for this device
 * @node: Reference to device tree node for this device
 * @driver_data: Driver data word for the entry that matched this device with
 *		its driver
 * @parent: Parent of this device, or NULL for the top level device
 * @priv: Private data for this device
 * @uclass: Pointer to uclass for this device
 * @uclass_priv: The uclass's private data for this device
 * @parent_priv: The parent's private data for this device
 * @uclass_node: Used by uclass to link its devices
 * @child_head: List of children of this device
 * @sibling_node: Next device in list of all devices
 * @flags: Flags for this device DM_FLAG_...
 * @req_seq: Requested sequence number for this device (-1 = any)
 * @seq: Allocated sequence number for this device (-1 = none). This is set up
 * when the device is probed and will be unique within the device's uclass.
 * @devres_head: List of memory allocations associated with this device.
 *		When CONFIG_DEVRES is enabled, devm_kmalloc() and friends will
 *		add to this list. Memory so-allocated will be freed
 *		automatically when the device is removed / unbound
 */
struct udevice {
	const struct driver *driver;
	const char *name;
	void *platdata;
	void *parent_platdata;
	void *uclass_platdata;
	//	ofnode node;
	//	ulong driver_data;
	unsigned long driver_data;
	struct udevice *parent;
	void *priv;
	struct uclass *uclass;
	void *uclass_priv;
	void *parent_priv;
	struct list_head uclass_node;
	struct list_head child_head;
	struct list_head sibling_node;
	uint32_t flags;
	int req_seq;
	int seq;
#ifdef CONFIG_DEVRES
	struct list_head devres_head;
#endif
};

typedef enum {
	PHY_INTERFACE_MODE_MII,
	PHY_INTERFACE_MODE_GMII,
	PHY_INTERFACE_MODE_SGMII,
	PHY_INTERFACE_MODE_SGMII_2500,
	PHY_INTERFACE_MODE_QSGMII,
	PHY_INTERFACE_MODE_TBI,
	PHY_INTERFACE_MODE_RMII,
	PHY_INTERFACE_MODE_RGMII,
	PHY_INTERFACE_MODE_RGMII_ID,
	PHY_INTERFACE_MODE_RGMII_RXID,
	PHY_INTERFACE_MODE_RGMII_TXID,
	PHY_INTERFACE_MODE_RTBI,
	PHY_INTERFACE_MODE_XGMII,
	PHY_INTERFACE_MODE_XAUI,
	PHY_INTERFACE_MODE_RXAUI,
	PHY_INTERFACE_MODE_SFI,
	PHY_INTERFACE_MODE_INTERNAL,
	PHY_INTERFACE_MODE_NONE, /* Must be last */

	PHY_INTERFACE_MODE_COUNT,
} phy_interface_t;

struct phy_device {
	/* Information about the PHY type */
	/* And management functions */
	struct mii_dev *bus;
	const struct phy_driver *drv;
	void *priv;

	// #ifdef CONFIG_DM_ETH
	const struct device *dev;
	//	ofnode node;
	// #else
	//	struct eth_device *dev;
	// #endif

	/* forced speed & duplex (no autoneg)
	 * partner speed & duplex & pause (autoneg)
	 */
	int speed;
	int duplex;

	/* The most recently read link state */
	int link;
	int port;
	phy_interface_t interface;

	u32 advertising;
	u32 supported;
	u32 mmds;

	int autoneg;
	int addr;
	int pause;
	int asym_pause;
	u32 phy_id;
	bool is_c45;
	u32 flags;
};

static inline int rk_phy_read(struct phy_device *phydev, int devad, int regnum)
{
	struct mii_dev *bus = phydev->bus;
	// printk("in rk_phy_read\n");
	return bus->read(bus, phydev->addr, devad, regnum);
}

static inline int rk_phy_write(struct phy_device *phydev, int devad, int regnum, u16 val)
{
	struct mii_dev *bus = phydev->bus;
	// printk("in rk_phy_write\n");
	return bus->write(bus, phydev->addr, devad, regnum, val);
}

int phy_set_supported(struct phy_device *phydev, u32 max_speed);

struct phy_device *phy_connect(struct mii_dev *bus, int addr, const struct device *dev,
			       phy_interface_t interface);

int phy_shutdown(struct phy_device *phydev);

int phy_config(struct phy_device *phydev);

int phy_startup(struct phy_device *phydev);

#endif /* ZEPHYR_INCLUDE_DRIVERS_PHY_H_ */

