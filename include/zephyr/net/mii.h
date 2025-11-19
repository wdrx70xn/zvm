/*
 * Copyright (c) 2016 Piotr Mienkowski
 * Copyright 2022 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 * @brief Definitions for IEEE 802.3, Section 2 MII compatible PHY transceivers
 */

#ifndef ZEPHYR_INCLUDE_NET_MII_H_
#define ZEPHYR_INCLUDE_NET_MII_H_

/**
 * @brief Ethernet MII (media independent interface) functions
 * @defgroup ethernet_mii Ethernet MII Support Functions
 * @ingroup ethernet
 * @{
 */

/* MII management registers */
/** Basic Mode Control Register */
#define MII_BMCR       0x0
/** Basic Mode Status Register */
#define MII_BMSR       0x1
/** PHY ID 1 Register */
#define MII_PHYID1R    0x2
/** PHY ID 2 Register */
#define MII_PHYID2R    0x3
/** Auto-Negotiation Advertisement Register */
#define MII_ANAR       0x4
/** Auto-Negotiation Link Partner Ability Reg */
#define MII_ANLPAR     0x5
/** Auto-Negotiation Expansion Register */
#define MII_ANER       0x6
/** Auto-Negotiation Next Page Transmit Register */
#define MII_ANNPTR     0x7
/** Auto-Negotiation Link Partner Received Next Page Reg */
#define MII_ANLPRNPR   0x8
/** 1000BASE-T Control Register */
#define MII_1KTCR 0x9
/** 1000BASE-T Status Register */
#define MII_1KSTSR 0xa
/** MMD Access Control Register */
#define MII_MMD_ACR    0xd
/** MMD Access Address Data Register */
#define MII_MMD_AADR   0xe
/** Extended Status Register */
#define MII_ESTAT      0xf

/* Basic Mode Control Register (BMCR) bit definitions */
/** PHY reset */
#define MII_BMCR_RESET             (1 << 15)
/** enable loopback mode */
#define MII_BMCR_LOOPBACK          (1 << 14)
/** 10=1000Mbps 01=100Mbps; 00=10Mbps */
#define MII_BMCR_SPEED_LSB         (1 << 13)
/** Auto-Negotiation enable */
#define MII_BMCR_AUTONEG_ENABLE    (1 << 12)
/** power down mode */
#define MII_BMCR_POWER_DOWN        (1 << 11)
/** isolate electrically PHY from MII */
#define MII_BMCR_ISOLATE           (1 << 10)
/** restart auto-negotiation */
#define MII_BMCR_AUTONEG_RESTART   (1 << 9)
/** full duplex mode */
#define MII_BMCR_DUPLEX_MODE       (1 << 8)
/** 10=1000Mbps 01=100Mbps; 00=10Mbps */
#define MII_BMCR_SPEED_MSB         (1 << 6)
/** Link Speed Field */
#define   MII_BMCR_SPEED_MASK      (1 << 6 | 1 << 13)
/** select speed 10 Mb/s */
#define   MII_BMCR_SPEED_10        (0 << 6 | 0 << 13)
/** select speed 100 Mb/s */
#define   MII_BMCR_SPEED_100       (0 << 6 | 1 << 13)
/** select speed 1000 Mb/s */
#define   MII_BMCR_SPEED_1000      (1 << 6 | 0 << 13)

/* Basic Mode Status Register (BMSR) bit definitions */
/** 100BASE-T4 capable */
#define MII_BMSR_100BASE_T4        (1 << 15)
/** 100BASE-X full duplex capable */
#define MII_BMSR_100BASE_X_FULL    (1 << 14)
/** 100BASE-X half duplex capable */
#define MII_BMSR_100BASE_X_HALF    (1 << 13)
/** 10 Mb/s full duplex capable */
#define MII_BMSR_10_FULL           (1 << 12)
/** 10 Mb/s half duplex capable */
#define MII_BMSR_10_HALF           (1 << 11)
/** 100BASE-T2 full duplex capable */
#define MII_BMSR_100BASE_T2_FULL   (1 << 10)
/** 100BASE-T2 half duplex capable */
#define MII_BMSR_100BASE_T2_HALF   (1 << 9)
/** extend status information in reg 15 */
#define MII_BMSR_EXTEND_STATUS     (1 << 8)
/** PHY accepts management frames with preamble suppressed */
#define MII_BMSR_MF_PREAMB_SUPPR   (1 << 6)
/** Auto-negotiation process completed */
#define MII_BMSR_AUTONEG_COMPLETE  (1 << 5)
/** remote fault detected */
#define MII_BMSR_REMOTE_FAULT      (1 << 4)
/** PHY is able to perform Auto-Negotiation */
#define MII_BMSR_AUTONEG_ABILITY   (1 << 3)
/** link is up */
#define MII_BMSR_LINK_STATUS       (1 << 2)
/** jabber condition detected */
#define MII_BMSR_JABBER_DETECT     (1 << 1)
/** extended register capabilities */
#define MII_BMSR_EXTEND_CAPAB      (1 << 0)

/* Auto-negotiation Advertisement Register (ANAR) bit definitions */
/* Auto-negotiation Link Partner Ability Register (ANLPAR) bit definitions */
/** next page */
#define MII_ADVERTISE_NEXT_PAGE    (1 << 15)
/** link partner acknowledge response */
#define MII_ADVERTISE_LPACK        (1 << 14)
/** remote fault */
#define MII_ADVERTISE_REMOTE_FAULT (1 << 13)
/** try for asymmetric pause */
#define MII_ADVERTISE_ASYM_PAUSE   (1 << 11)
/** try for pause */
#define MII_ADVERTISE_PAUSE        (1 << 10)
/** try for 100BASE-T4 support */
#define MII_ADVERTISE_100BASE_T4   (1 << 9)
/** try for 100BASE-X full duplex support */
#define MII_ADVERTISE_100_FULL     (1 << 8)
/** try for 100BASE-X support */
#define MII_ADVERTISE_100_HALF     (1 << 7)
/** try for 10 Mb/s full duplex support */
#define MII_ADVERTISE_10_FULL      (1 << 6)
/** try for 10 Mb/s half duplex support */
#define MII_ADVERTISE_10_HALF      (1 << 5)
/** Selector Field Mask */
#define MII_ADVERTISE_SEL_MASK     (0x1F << 0)
/** Selector Field */
#define MII_ADVERTISE_SEL_IEEE_802_3   0x01

/* 1000BASE-T Control Register bit definitions */
/** try for 1000BASE-T full duplex support */
#define MII_ADVERTISE_1000_FULL    (1 << 9)
/** try for 1000BASE-T half duplex support */
#define MII_ADVERTISE_1000_HALF    (1 << 8)

/** Advertise all speeds */
#define MII_ADVERTISE_ALL (MII_ADVERTISE_10_HALF | MII_ADVERTISE_10_FULL |\
			   MII_ADVERTISE_100_HALF | MII_ADVERTISE_100_FULL |\
			   MII_ADVERTISE_SEL_IEEE_802_3)

/* Extended Status Register bit definitions */
/** 1000BASE-X full-duplex capable */
#define MII_ESTAT_1000BASE_X_FULL  (1 << 15)
/** 1000BASE-X half-duplex capable */
#define MII_ESTAT_1000BASE_X_HALF  (1 << 14)
/** 1000BASE-T full-duplex capable */
#define MII_ESTAT_1000BASE_T_FULL  (1 << 13)
/** 1000BASE-T half-duplex capable */
#define MII_ESTAT_1000BASE_T_HALF  (1 << 12)

/**
 * @}
 */

/* Generic MII registers. */
// #define MII_BMCR		0x00	/* Basic mode control register */
// #define MII_BMSR		0x01	/* Basic mode status register  */
#define MII_PHYSID1		0x02	/* PHYS ID 1                   */
#define MII_PHYSID2		0x03	/* PHYS ID 2                   */
#define MII_ADVERTISE		0x04	/* Advertisement control reg   */
#define MII_LPA			0x05	/* Link partner ability reg    */
#define MII_EXPANSION		0x06	/* Expansion register          */
#define MII_CTRL1000		0x09	/* 1000BASE-T control          */
#define MII_STAT1000		0x0a	/* 1000BASE-T status           */
#define	MII_MMD_CTRL		0x0d	/* MMD Access Control Register */
#define	MII_MMD_DATA		0x0e	/* MMD Access Data Register */
#define MII_ESTATUS		0x0f	/* Extended Status             */
#define MII_DCOUNTER		0x12	/* Disconnect counter          */
#define MII_FCSCOUNTER		0x13	/* False carrier counter       */
#define MII_NWAYTEST		0x14	/* N-way auto-neg test reg     */
#define MII_RERRCOUNTER		0x15	/* Receive error counter       */
#define MII_SREVISION		0x16	/* Silicon revision            */
#define MII_RESV1		0x17	/* Reserved...                 */
#define MII_LBRERROR		0x18	/* Lpback, rx, bypass error    */
#define MII_PHYADDR		0x19	/* PHY address                 */
#define MII_RESV2		0x1a	/* Reserved...                 */
#define MII_TPISTATUS		0x1b	/* TPI status for 10mbps       */
#define MII_NCONFIG		0x1c	/* Network interface config    */


/* Basic mode control register. */
#define BMCR_RESV		0x003f	/* Unused...                   */
#define BMCR_SPEED1000		0x0040	/* MSB of Speed (1000)         */
#define BMCR_CTST		0x0080	/* Collision test              */
#define BMCR_FULLDPLX		0x0100	/* Full duplex                 */
#define BMCR_ANRESTART		0x0200	/* Auto negotiation restart    */
#define BMCR_ISOLATE		0x0400	/* Isolate data paths from MII */
#define BMCR_PDOWN		0x0800	/* Enable low power state      */
#define BMCR_ANENABLE		0x1000	/* Enable auto negotiation     */
#define BMCR_SPEED100		0x2000	/* Select 100Mbps              */
#define BMCR_LOOPBACK		0x4000	/* TXD loopback bits           */
#define BMCR_RESET		0x8000	/* Reset to default state      */
#define BMCR_SPEED10		0x0000	/* Select 10Mbps               */

/* Basic mode status register. */
#define BMSR_ERCAP		0x0001	/* Ext-reg capability          */
#define BMSR_JCD		0x0002	/* Jabber detected             */
#define BMSR_LSTATUS		0x0004	/* Link status                 */
#define BMSR_ANEGCAPABLE	0x0008	/* Able to do auto-negotiation */
#define BMSR_RFAULT		0x0010	/* Remote fault detected       */
#define BMSR_ANEGCOMPLETE	0x0020	/* Auto-negotiation complete   */
#define BMSR_RESV		0x00c0	/* Unused...                   */
#define BMSR_ESTATEN		0x0100	/* Extended Status in R15      */
#define BMSR_100HALF2		0x0200	/* Can do 100BASE-T2 HDX       */
#define BMSR_100FULL2		0x0400	/* Can do 100BASE-T2 FDX       */
#define BMSR_10HALF		0x0800	/* Can do 10mbps, half-duplex  */
#define BMSR_10FULL		0x1000	/* Can do 10mbps, full-duplex  */
#define BMSR_100HALF		0x2000	/* Can do 100mbps, half-duplex */
#define BMSR_100FULL		0x4000	/* Can do 100mbps, full-duplex */
#define BMSR_100BASE4		0x8000	/* Can do 100mbps, 4k packets  */

/* Advertisement control register. */
#define ADVERTISE_SLCT		0x001f	/* Selector bits               */
#define ADVERTISE_CSMA		0x0001	/* Only selector supported     */
#define ADVERTISE_10HALF	0x0020	/* Try for 10mbps half-duplex  */
#define ADVERTISE_1000XFULL	0x0020	/* Try for 1000BASE-X full-duplex */
#define ADVERTISE_10FULL	0x0040	/* Try for 10mbps full-duplex  */
#define ADVERTISE_1000XHALF	0x0040	/* Try for 1000BASE-X half-duplex */
#define ADVERTISE_100HALF	0x0080	/* Try for 100mbps half-duplex */
#define ADVERTISE_1000XPAUSE	0x0080	/* Try for 1000BASE-X pause    */
#define ADVERTISE_100FULL	0x0100	/* Try for 100mbps full-duplex */
#define ADVERTISE_1000XPSE_ASYM	0x0100	/* Try for 1000BASE-X asym pause */
#define ADVERTISE_100BASE4	0x0200	/* Try for 100mbps 4k packets  */
#define ADVERTISE_PAUSE_CAP	0x0400	/* Try for pause               */
#define ADVERTISE_PAUSE_ASYM	0x0800	/* Try for asymetric pause     */
#define ADVERTISE_RESV		0x1000	/* Unused...                   */
#define ADVERTISE_RFAULT	0x2000	/* Say we can detect faults    */
#define ADVERTISE_LPACK		0x4000	/* Ack link partners response  */
#define ADVERTISE_NPAGE		0x8000	/* Next page bit               */

#define ADVERTISE_FULL		(ADVERTISE_100FULL | ADVERTISE_10FULL | \
				  ADVERTISE_CSMA)
#define ADVERTISE_ALL		(ADVERTISE_10HALF | ADVERTISE_10FULL | \
				  ADVERTISE_100HALF | ADVERTISE_100FULL)

/* Link partner ability register. */
#define LPA_SLCT		0x001f	/* Same as advertise selector  */
#define LPA_10HALF		0x0020	/* Can do 10mbps half-duplex   */
#define LPA_1000XFULL		0x0020	/* Can do 1000BASE-X full-duplex */
#define LPA_10FULL		0x0040	/* Can do 10mbps full-duplex   */
#define LPA_1000XHALF		0x0040	/* Can do 1000BASE-X half-duplex */
#define LPA_100HALF		0x0080	/* Can do 100mbps half-duplex  */
#define LPA_1000XPAUSE		0x0080	/* Can do 1000BASE-X pause     */
#define LPA_100FULL		0x0100	/* Can do 100mbps full-duplex  */
#define LPA_1000XPAUSE_ASYM	0x0100	/* Can do 1000BASE-X pause asym*/
#define LPA_100BASE4		0x0200	/* Can do 100mbps 4k packets   */
#define LPA_PAUSE_CAP		0x0400	/* Can pause                   */
#define LPA_PAUSE_ASYM		0x0800	/* Can pause asymetrically     */
#define LPA_RESV		0x1000	/* Unused...                   */
#define LPA_RFAULT		0x2000	/* Link partner faulted        */
#define LPA_LPACK		0x4000	/* Link partner acked us       */
#define LPA_NPAGE		0x8000	/* Next page bit               */

#define LPA_DUPLEX		(LPA_10FULL | LPA_100FULL)
#define LPA_100			(LPA_100FULL | LPA_100HALF | LPA_100BASE4)

/* Expansion register for auto-negotiation. */
#define EXPANSION_NWAY		0x0001	/* Can do N-way auto-nego      */
#define EXPANSION_LCWP		0x0002	/* Got new RX page code word   */
#define EXPANSION_ENABLENPAGE	0x0004	/* This enables npage words    */
#define EXPANSION_NPCAPABLE	0x0008	/* Link partner supports npage */
#define EXPANSION_MFAULTS	0x0010	/* Multiple faults detected    */
#define EXPANSION_RESV		0xffe0	/* Unused...                   */

#define ESTATUS_1000_TFULL	0x2000	/* Can do 1000BT Full          */
#define ESTATUS_1000_THALF	0x1000	/* Can do 1000BT Half          */

/* N-way test register. */
#define NWAYTEST_RESV1		0x00ff	/* Unused...                   */
#define NWAYTEST_LOOPBACK	0x0100	/* Enable loopback for N-way   */
#define NWAYTEST_RESV2		0xfe00	/* Unused...                   */

/* 1000BASE-T Control register */
#define ADVERTISE_1000FULL	0x0200  /* Advertise 1000BASE-T full duplex */
#define ADVERTISE_1000HALF	0x0100  /* Advertise 1000BASE-T half duplex */
#define CTL1000_AS_MASTER	0x0800
#define CTL1000_ENABLE_MASTER	0x1000

/* 1000BASE-T Status register */
#define LPA_1000LOCALRXOK	0x2000	/* Link partner local receiver status */
#define LPA_1000REMRXOK		0x1000	/* Link partner remote receiver status */
#define LPA_1000FULL		0x0800	/* Link partner 1000BASE-T full duplex */
#define LPA_1000HALF		0x0400	/* Link partner 1000BASE-T half duplex */

/* Flow control flags */
#define FLOW_CTRL_TX		0x01
#define FLOW_CTRL_RX		0x02

/* MMD Access Control register fields */
#define MII_MMD_CTRL_DEVAD_MASK	0x1f	/* Mask MMD DEVAD*/
#define MII_MMD_CTRL_ADDR	0x0000	/* Address */
#define MII_MMD_CTRL_NOINCR	0x4000	/* no post increment */
#define MII_MMD_CTRL_INCR_RDWT	0x8000	/* post increment on reads & writes */
#define MII_MMD_CTRL_INCR_ON_WT	0xC000	/* post increment on writes only */

/* This structure is used in all SIOCxMIIxxx ioctl calls */
struct mii_ioctl_data {
	u16		phy_id;
	u16		reg_num;
	u16		val_in;
	u16		val_out;
};

/* Expansion register for auto-negotiation. */
#define EXPANSION_NWAY		0x0001	/* Can do N-way auto-nego      */
#define EXPANSION_LCWP		0x0002	/* Got new RX page code word   */
#define EXPANSION_ENABLENPAGE	0x0004	/* This enables npage words    */
#define EXPANSION_NPCAPABLE	0x0008	/* Link partner supports npage */
#define EXPANSION_MFAULTS	0x0010	/* Multiple faults detected    */
#define EXPANSION_RESV		0xffe0	/* Unused...                   */

#define ESTATUS_1000_XFULL	0x8000	/* Can do 1000BX Full */
#define ESTATUS_1000_XHALF	0x4000	/* Can do 1000BX Half */
#define ESTATUS_1000_TFULL	0x2000	/* Can do 1000BT Full          */
#define ESTATUS_1000_THALF	0x1000	/* Can do 1000BT Half          */

/* MII_STAT1000 masks */
#define PHY_1000BTSR_MSCF	0x8000
#define PHY_1000BTSR_MSCR	0x4000
#define PHY_1000BTSR_LRS	0x2000
#define PHY_1000BTSR_RRS	0x1000
#define PHY_1000BTSR_1000FD	0x0800
#define PHY_1000BTSR_1000HD	0x0400

#endif /* ZEPHYR_INCLUDE_NET_MII_H_ */
