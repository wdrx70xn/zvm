/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2020-2021 Rockchip Electronics Co., Ltd.
 */

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_RK3568_CLOCK_H
#define ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_RK3568_CLOCK_H
#ifdef __cplusplus
  extern "C" {
#endif

#ifndef __ASSEMBLY__

#ifndef __IO
#define __IO
#endif

/* PMUCRU Register Structure Define */
struct PMUCRU_REG {
    __IO uint32_t PPLL_CON[5];                        /* Address Offset: 0x0000 */
         uint32_t RESERVED0014[11];                   /* Address Offset: 0x0014 */
    __IO uint32_t HPLL_CON[5];                        /* Address Offset: 0x0040 */
         uint32_t RESERVED0054[11];                   /* Address Offset: 0x0054 */
    __IO uint32_t MODE_CON00;                         /* Address Offset: 0x0080 */
         uint32_t RESERVED0084[31];                   /* Address Offset: 0x0084 */
    __IO uint32_t CRU_CLKSEL_CON[10];                 /* Address Offset: 0x0100 */
         uint32_t RESERVED0128[22];                   /* Address Offset: 0x0128 */
    __IO uint32_t CRU_CLKGATE_CON[3];                 /* Address Offset: 0x0180 */
         uint32_t RESERVED018C[29];                   /* Address Offset: 0x018C */
    __IO uint32_t CRU_SOFTRST_CON[1];                 /* Address Offset: 0x0200 */
};

/* CRU Register Structure Define */
struct CRU_REG {
    __IO uint32_t APLL_CON[5];                        /* Address Offset: 0x0000 */
         uint32_t RESERVED0014[3];                    /* Address Offset: 0x0014 */
    __IO uint32_t DPLL_CON[5];                        /* Address Offset: 0x0020 */
         uint32_t RESERVED0034[3];                    /* Address Offset: 0x0034 */
    __IO uint32_t GPLL_CON[5];                        /* Address Offset: 0x0040 */
         uint32_t RESERVED0054[3];                    /* Address Offset: 0x0054 */
    __IO uint32_t CPLL_CON[5];                        /* Address Offset: 0x0060 */
         uint32_t RESERVED0074[3];                    /* Address Offset: 0x0074 */
    __IO uint32_t NPLL_CON[2];                        /* Address Offset: 0x0080 */
         uint32_t RESERVED0088[6];                    /* Address Offset: 0x0088 */
    __IO uint32_t VPLL_CON[2];                        /* Address Offset: 0x00A0 */
         uint32_t RESERVED00A8[6];                    /* Address Offset: 0x00A8 */
    __IO uint32_t MODE_CON00;                         /* Address Offset: 0x00C0 */
    __IO uint32_t MISC_CON[3];                        /* Address Offset: 0x00C4 */
    __IO uint32_t GLB_CNT_TH;                         /* Address Offset: 0x00D0 */
    __IO uint32_t GLB_SRST_FST;                       /* Address Offset: 0x00D4 */
    __IO uint32_t GLB_SRST_SND;                       /* Address Offset: 0x00D8 */
    __IO uint32_t GLB_RST_CON;                        /* Address Offset: 0x00DC */
    __IO uint32_t GLB_RST_ST;                         /* Address Offset: 0x00E0 */
         uint32_t RESERVED00E4[7];                    /* Address Offset: 0x00E4 */
    __IO uint32_t CRU_CLKSEL_CON[85];                 /* Address Offset: 0x0100 */
         uint32_t RESERVED0254[43];                   /* Address Offset: 0x0254 */
    __IO uint32_t CRU_CLKGATE_CON[36];                /* Address Offset: 0x0300 */
         uint32_t RESERVED0390[28];                   /* Address Offset: 0x0390 */
    __IO uint32_t CRU_SOFTRST_CON[30];                /* Address Offset: 0x0400 */
         uint32_t RESERVED0478[2];                    /* Address Offset: 0x0478 */
    __IO uint32_t SSGTBL0_3;                          /* Address Offset: 0x0480 */
    __IO uint32_t SSGTBL4_7;                          /* Address Offset: 0x0484 */
    __IO uint32_t SSGTBL8_11;                         /* Address Offset: 0x0488 */
    __IO uint32_t SSGTBL12_15;                        /* Address Offset: 0x048C */
    __IO uint32_t SSGTBL16_19;                        /* Address Offset: 0x0490 */
    __IO uint32_t SSGTBL20_23;                        /* Address Offset: 0x0494 */
    __IO uint32_t SSGTBL24_27;                        /* Address Offset: 0x0498 */
    __IO uint32_t SSGTBL28_31;                        /* Address Offset: 0x049C */
    __IO uint32_t SSGTBL32_35;                        /* Address Offset: 0x04A0 */
    __IO uint32_t SSGTBL36_39;                        /* Address Offset: 0x04A4 */
    __IO uint32_t SSGTBL40_43;                        /* Address Offset: 0x04A8 */
    __IO uint32_t SSGTBL44_47;                        /* Address Offset: 0x04AC */
    __IO uint32_t SSGTBL48_51;                        /* Address Offset: 0x04B0 */
    __IO uint32_t SSGTBL52_55;                        /* Address Offset: 0x04B4 */
    __IO uint32_t SSGTBL56_59;                        /* Address Offset: 0x04B8 */
    __IO uint32_t SSGTBL60_63;                        /* Address Offset: 0x04BC */
    __IO uint32_t SSGTBL64_67;                        /* Address Offset: 0x04C0 */
    __IO uint32_t SSGTBL68_71;                        /* Address Offset: 0x04C4 */
    __IO uint32_t SSGTBL72_75;                        /* Address Offset: 0x04C8 */
    __IO uint32_t SSGTBL76_79;                        /* Address Offset: 0x04CC */
    __IO uint32_t SSGTBL80_83;                        /* Address Offset: 0x04D0 */
    __IO uint32_t SSGTBL84_87;                        /* Address Offset: 0x04D4 */
    __IO uint32_t SSGTBL88_91;                        /* Address Offset: 0x04D8 */
    __IO uint32_t SSGTBL92_95;                        /* Address Offset: 0x04DC */
    __IO uint32_t SSGTBL96_99;                        /* Address Offset: 0x04E0 */
    __IO uint32_t SSGTBL100_103;                      /* Address Offset: 0x04E4 */
    __IO uint32_t SSGTBL104_107;                      /* Address Offset: 0x04E8 */
    __IO uint32_t SSGTBL108_111;                      /* Address Offset: 0x04EC */
    __IO uint32_t SSGTBL112_115;                      /* Address Offset: 0x04F0 */
    __IO uint32_t SSGTBL116_119;                      /* Address Offset: 0x04F4 */
    __IO uint32_t SSGTBL120_123;                      /* Address Offset: 0x04F8 */
    __IO uint32_t SSGTBL124_127;                      /* Address Offset: 0x04FC */
    __IO uint32_t AUTOCS_CORE_CON0;                   /* Address Offset: 0x0500 */
    __IO uint32_t AUTOCS_CORE_CON1;                   /* Address Offset: 0x0504 */
    __IO uint32_t AUTOCS_GPU_CON0;                    /* Address Offset: 0x0508 */
    __IO uint32_t AUTOCS_GPU_CON1;                    /* Address Offset: 0x050C */
    __IO uint32_t AUTOCS_BUS_CON0;                    /* Address Offset: 0x0510 */
    __IO uint32_t AUTOCS_BUS_CON1;                    /* Address Offset: 0x0514 */
    __IO uint32_t AUTOCS_TOP_CON0;                    /* Address Offset: 0x0518 */
    __IO uint32_t AUTOCS_TOP_CON1;                    /* Address Offset: 0x051C */
    __IO uint32_t AUTOCS_RKVDEC_CON0;                 /* Address Offset: 0x0520 */
    __IO uint32_t AUTOCS_RKVDEC_CON1;                 /* Address Offset: 0x0524 */
    __IO uint32_t AUTOCS_RKVENC_CON0;                 /* Address Offset: 0x0528 */
    __IO uint32_t AUTOCS_RKVENC_CON1;                 /* Address Offset: 0x052C */
    __IO uint32_t AUTOCS_VPU_CON0;                    /* Address Offset: 0x0530 */
    __IO uint32_t AUTOCS_VPU_CON1;                    /* Address Offset: 0x0534 */
    __IO uint32_t AUTOCS_PERI_CON0;                   /* Address Offset: 0x0538 */
    __IO uint32_t AUTOCS_PERI_CON1;                   /* Address Offset: 0x053C */
    __IO uint32_t AUTOCS_GPLL_CON0;                   /* Address Offset: 0x0540 */
    __IO uint32_t AUTOCS_GPLL_CON1;                   /* Address Offset: 0x0544 */
    __IO uint32_t AUTOCS_CPLL_CON0;                   /* Address Offset: 0x0548 */
    __IO uint32_t AUTOCS_CPLL_CON1;                   /* Address Offset: 0x054C */
         uint32_t RESERVED0550[12];                   /* Address Offset: 0x0550 */
    __IO uint32_t SDMMC0_CON[2];                      /* Address Offset: 0x0580 */
    __IO uint32_t SDMMC1_CON[2];                      /* Address Offset: 0x0588 */
    __IO uint32_t SDMMC2_CON[2];                      /* Address Offset: 0x0590 */
    __IO uint32_t EMMC_CON[2];                        /* Address Offset: 0x0598 */
};

/* pmucru system configuration. */
struct rk3568_clk_syscon {
	uint32_t reg_base;
	uint32_t reg_size;
     void *priv_data;
};

/* Add current clock syscon info to subsys(like zvm) */
int z_info_sys_clkcon(const char *name, uint32_t addr_base,
                              uint32_t addr_size, void *priv);

int __weak z_info_sys_clkcon(const char *name, uint32_t addr_base,
                              uint32_t addr_size, void *priv)
{
     ARG_UNUSED(addr_base);
     ARG_UNUSED(addr_size);
     ARG_UNUSED(priv);
     return 0;
}

#endif /*  __ASSEMBLY__  */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_RK3568_CLOCK_H_ */