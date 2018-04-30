/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/arm64_ras.h>

/* Error Records Per Core */
/* ERR_CTLR bits for IFU */
#define ERR_CTL_IFU_ITLB_SNP_ERR	RAS_BIT(43)
#define ERR_CTL_IFU_MITGRP_ERR		RAS_BIT(42)
#define ERR_CTL_IFU_IMQDP_ERR		RAS_BIT(41)
#define ERR_CTL_IFU_L2UC_ERR		RAS_BIT(40)
#define ERR_CTL_IFU_ICTPSNP_ERR		RAS_BIT(38)
#define ERR_CTL_IFU_ICMHSNP_ERR		RAS_BIT(37)
#define ERR_CTL_IFU_ITLBP_ERR		RAS_BIT(36)
#define ERR_CTL_IFU_THERR_ERR		RAS_BIT(35)
#define ERR_CTL_IFU_ICDP_ERR		RAS_BIT(34)
#define ERR_CTL_IFU_ICTP_ERR		RAS_BIT(33)
#define ERR_CTL_IFU_ICMH_ERR		RAS_BIT(32)

/* ERR_CTLR bits for RET_JSR */
#define ERR_CTL_RET_JSR_FRFP_ERR	RAS_BIT(35)
#define ERR_CTL_RET_JSR_IRFP_ERR	RAS_BIT(34)
#define ERR_CTL_RET_JSR_GB_ERR		RAS_BIT(33)
#define ERR_CTL_RET_JSR_TO_ERR		RAS_BIT(32)

/* ERR_CTLR bits for MTS_JSR */
#define ERR_CTL_MTS_JSR_ERRUC_ERR	RAS_BIT(32)
#define ERR_CTL_MTS_JSR_ERRC_ERR	RAS_BIT(33)
#define ERR_CTL_MTS_JSR_NAFLL_ERR	RAS_BIT(34)
#define ERR_CTL_MTS_JSR_CARVE_ERR	RAS_BIT(35)
#define ERR_CTL_MTS_JSR_CRAB_ERR	RAS_BIT(36)
#define ERR_CTL_MTS_JSR_MMIO_ERR	RAS_BIT(37)

/* ERR_CTLR bits for LSD_1 */
#define ERR_CTL_LSD1_CCTP_ERR		RAS_BIT(32)
#define ERR_CTL_LSD1_MCMH_ERR		RAS_BIT(33)
#define ERR_CTL_LSD1_CCMH_ERR		RAS_BIT(34)
#define ERR_CTL_LSD1_MCDLP_ERR		RAS_BIT(35)
#define ERR_CTL_LSD1_CCDLECC_S_ERR	RAS_BIT(36)
#define ERR_CTL_LSD1_CCDLECC_D_ERR	RAS_BIT(37)
#define ERR_CTL_LSD1_CCDSECC_S_ERR	RAS_BIT(38)
#define ERR_CTL_LSD1_CCDSECC_D_ERR	RAS_BIT(39)
#define ERR_CTL_LSD1_CCDEMLECC_ERR	RAS_BIT(40)

/* ERR_CTLR bits for LSD_2 */
#define ERR_CTL_LSD2_BTCCVPP_ERR	RAS_BIT(32)
#define ERR_CTL_LSD2_BTCCPPP_ERR	RAS_BIT(33)
#define ERR_CTL_LSD2_BTCCMH_ERR		RAS_BIT(34)
#define ERR_CTL_LSD2_VRCDECC_S_ERR	RAS_BIT(35)
#define ERR_CTL_LSD2_VRCDECC_D_ERR	RAS_BIT(36)
#define ERR_CTL_LSD2_VRCDP_ERR		RAS_BIT(37)
#define ERR_CTL_LSD2_MCDEP_ERR		RAS_BIT(38)
#define ERR_CTL_LSD2_CCDEECC_S_ERR	RAS_BIT(39)
#define ERR_CTL_LSD2_CCDEECC_D_ERR	RAS_BIT(40)
#define ERR_CTL_LSD2_L2REQ_UNCORR_ERR	RAS_BIT(41)

/* ERR_CTLR bits for LSD_3 */
#define ERR_CTL_LSD3_L2TLBP_ERR		RAS_BIT(32)
#define ERR_CTL_LSD3_LATENT_ERR		RAS_BIT(63)

/* Error records per CCPLEX */
/* ERR_CTLR bits for CMU:CCPMU or DPMU*/
#define ERR_CTL_DPMU_DMCE_CRAB_ACC_ERR	RAS_BIT(32)
#define ERR_CTL_DPMU_CRAB_ACC_ERR	RAS_BIT(33)
#define ERR_CTL_DPMU_DMCE_UCODE_ERR	RAS_BIT(35)

/* ERR_CTLR bits for SCF:IOB*/
#define ERR_CTL_SCFIOB_REQ_PAR_ERR	RAS_BIT(41)
#define ERR_CTL_SCFIOB_PUT_PAR_ERR	RAS_BIT(40)
#define ERR_CTL_SCFIOB_PUT_CECC_ERR	RAS_BIT(32)
#define ERR_CTL_SCFIOB_PUT_UECC_ERR	RAS_BIT(39)
#define ERR_CTL_SCFIOB_EVP_ERR		RAS_BIT(33)
#define ERR_CTL_SCFIOB_TBX_ERR		RAS_BIT(34)
#define ERR_CTL_SCFIOB_CRI_ERR		RAS_BIT(35)
#define ERR_CTL_SCFIOB_MMCRAB_ERR	RAS_BIT(37)
#define ERR_CTL_SCFIOB_IHI_ERR		RAS_BIT(36)
#define ERR_CTL_SCFIOB_CBB_ERR		RAS_BIT(38)
#define ERRX_SCFIOB			1025

/* ERR_CTLR bits for SCF:SNOC*/
#define ERR_CTL_SCFSNOC_CPE_TO_ERR	RAS_BIT(34)
#define ERR_CTL_SCFSNOC_CPE_RSP_ERR	RAS_BIT(35)
#define ERR_CTL_SCFSNOC_CPE_REQ_ERR	RAS_BIT(36)
#define ERR_CTL_SCFSNOC_DVMU_TO_ERR	RAS_BIT(37)
#define ERR_CTL_SCFSNOC_DVMU_PAR_ERR	RAS_BIT(38)
#define ERR_CTL_SCFSNOC_MISC_CECC_ERR	RAS_BIT(32)
#define ERR_CTL_SCFSNOC_MISC_UECC_ERR	RAS_BIT(39)
#define ERR_CTL_SCFSNOC_MISC_PAR_ERR	RAS_BIT(40)
#define ERR_CTL_SCFSNOC_MISC_RSP_ERR	RAS_BIT(41)
#define ERR_CTL_SCFSNOC_CARVEOUT_ERR	RAS_BIT(33)

/* ERR_CTLR bits for CMU:CTU*/
#define ERR_CTL_CMUCTU_TRCDMA_PAR_ERR	RAS_BIT(32)
#define ERR_CTL_CMUCTU_MCF_PAR_ERR	RAS_BIT(33)
#define ERR_CTL_CMUCTU_TRL_PAR_ERR	RAS_BIT(34)
#define ERR_CTL_CMUCTU_CTU_DATA_PAR_ERR	RAS_BIT(35)
#define ERR_CTL_CMUCTU_TAG_PAR_ERR	RAS_BIT(36)
#define ERR_CTL_CMUCTU_CTU_SNP_ERR	RAS_BIT(37)
#define ERR_CTL_CMUCTU_TRCDMA_REQ_ERR	RAS_BIT(38)

/* ERR_CTLR bits for SCF:L3_* */
#define ERR_CTL_SCFL3_ADR_ERR		RAS_BIT(38)
#define ERR_CTL_SCFL3_PERR_ERR		RAS_BIT(40)
#define ERR_CTL_SCFL3_UECC_ERR		RAS_BIT(39)
#define ERR_CTL_SCFL3_CECC_ERR		RAS_BIT(44)
#define ERR_CTL_SCFL3_MH_CAM_ERR	RAS_BIT(37)
#define ERR_CTL_SCFL3_MH_TAG_ERR	RAS_BIT(36)
#define ERR_CTL_SCFL3_UNSUPP_REQ_ERR	RAS_BIT(35)
#define ERR_CTL_SCFL3_PROT_ERR		RAS_BIT(34)
#define ERR_CTL_SCFL3_TO_ERR		RAS_BIT(33)
#define ERRX_SCFL3			768

/* ERR_CTLR bits for SCFCMU_CLOCKS */
#define ERR_CTL_SCFCMU_LUT0_PAR_ERR	RAS_BIT(32)
#define ERR_CTL_SCFCMU_LUT1_PAR_ERR	RAS_BIT(33)
#define ERR_CTL_SCFCMU_ADC0_MON_ERR	RAS_BIT(34)
#define ERR_CTL_SCFCMU_ADC1_MON_ERR	RAS_BIT(35)

/* Error records per Core Cluster */
/* ERR_CTLR bits for L2 */
#define ERR_CTL_L2_MLD_ECCC_ERR		RAS_BIT(32)
#define ERR_CTL_L2_URD_ECCC_ERR		RAS_BIT(33)
#define ERR_CTL_L2_MLD_ECCUD_ERR	RAS_BIT(34)
#define ERR_CTL_L2_MLD_ECCUC_ERR	RAS_BIT(36)
#define ERR_CTL_L2_URD_ECCUC_ERR	RAS_BIT(37)
#define ERR_CTL_L2_NTDP_ERR		RAS_BIT(38)
#define ERR_CTL_L2_URDP			RAS_BIT(39)
#define ERR_CTL_L2_MLTP_ERR		RAS_BIT(40)
#define ERR_CTL_L2_NTTP_ERR		RAS_BIT(41)
#define ERR_CTL_L2_URTP_ERR		RAS_BIT(42)
#define ERR_CTL_L2_L2MH_ERR		RAS_BIT(43)
#define ERR_CTL_L2_CORE02L2CP_ERR	RAS_BIT(44)
#define ERR_CTL_L2_CORE12L2CP_ERR	RAS_BIT(45)
#define ERR_CTL_L2_SCF2L2C_ECCC_ERR	RAS_BIT(46)
#define ERR_CTL_L2_SCF2L2C_ECCU_ERR	RAS_BIT(47)
#define ERR_CTL_L2_SCF2L2C_FILLDATAP_ERR	RAS_BIT(48)
#define ERR_CTL_L2_SCF2L2C_ADVNOTP_ERR	RAS_BIT(49)
#define ERR_CTL_L2_SCF2L2C_REQRSPP_ERR	RAS_BIT(50)
#define ERR_CTL_L2_SCF2L2C_DECWTERR_ERR	RAS_BIT(51)
#define ERR_CTL_L2_SCF2L2C_DECRDERR_ERR	RAS_BIT(52)
#define ERR_CTL_L2_SCF2L2C_SLVWTERR_ERR	RAS_BIT(53)
#define ERR_CTL_L2_SCF2L2C_SLVRDERR_ERR	RAS_BIT(54)
#define ERR_CTL_L2_L2PCL_ERR	RAS_BIT(55)
#define ERR_CTL_L2_URTTO_ERR	RAS_BIT(56)

/* ERR_CTLR bits for MMU */
#define ERR_CTL_MMU_ACPERR_ERR		RAS_BIT(32)
#define ERR_CTL_MMU_WCPERR_ERR		RAS_BIT(34)

/* ERR_CTLR bits for CLUSTER_CLOCKS */
#define ERR_CTL_CC_FREQ_MON_ERR		RAS_BIT(32)

/* Constants used by ras_trip */
#define ERRi_MISC0_CONST		0x2222222222222222UL
#define ERRi_MISC1_CONST		0x3333333333333333UL
#define ERRi_ADDR_CONST			0x4444444444444444UL
