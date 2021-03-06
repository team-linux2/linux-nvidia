#ifndef T18X_TEGRA_ARI_H
#define T18X_TEGRA_ARI_H

/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * ----------------------------------------------------------------------------
 * t18x_ari.h
 *
 * Global ARI definitions.
 * ----------------------------------------------------------------------------
 */

enum {
	TEGRA_ARI_VERSION_MAJOR = 2,
	TEGRA_ARI_VERSION_MINOR = 19,
};

typedef enum {
	/* indexes below get the core lock */
	TEGRA_ARI_MISC = 0,
	/* index 1 is deprecated */
	/* index 2 is deprecated */
	/* index 3 is deprecated */
	TEGRA_ARI_ONLINE_CORE = 4,

	/* indexes below need cluster lock */
	TEGRA_ARI_MISC_CLUSTER = 41,
	TEGRA_ARI_IS_CCX_ALLOWED = 42,
	TEGRA_ARI_CC3_CTRL = 43,

	/* indexes below need ccplex lock */
	TEGRA_ARI_ENTER_CSTATE = 80,
	TEGRA_ARI_UPDATE_CSTATE_INFO = 81,
	TEGRA_ARI_IS_SC7_ALLOWED = 82,
	/* index 83 is deprecated */
	TEGRA_ARI_PERFMON = 84,
	TEGRA_ARI_UPDATE_CCPLEX_GSC = 85,
	/* index 86 is depracated */
	/* index 87 is deprecated */
	TEGRA_ARI_ROC_FLUSH_CACHE_ONLY = 88,
	TEGRA_ARI_ROC_FLUSH_CACHE_TRBITS = 89,
	TEGRA_ARI_MISC_CCPLEX = 90,
	TEGRA_ARI_MCA = 91,
	TEGRA_ARI_UPDATE_CROSSOVER = 92,
	TEGRA_ARI_CSTATE_STATS = 93,
	TEGRA_ARI_WRITE_CSTATE_STATS = 94,
	TEGRA_ARI_COPY_MISCREG_AA64_RST = 95,
	TEGRA_ARI_ROC_CLEAN_CACHE_ONLY = 96,
} tegra_ari_req_id_t;

typedef enum {
	TEGRA_ARI_MISC_ECHO = 0,
	TEGRA_ARI_MISC_VERSION = 1,
	TEGRA_ARI_MISC_FEATURE_LEAF_0 = 2,
} tegra_ari_misc_index_t;

typedef enum {
	TEGRA_ARI_MISC_CCPLEX_SHUTDOWN_POWER_OFF = 0,
	TEGRA_ARI_MISC_CCPLEX_SHUTDOWN_REBOOT = 1,
	TEGRA_ARI_MISC_CCPLEX_CORESIGHT_CG_CTRL = 2,
	TEGRA_ARI_MISC_CCPLEX_EDBGREQ = 3,
} tegra_ari_misc_ccplex_index_t;

typedef enum {
	TEGRA_ARI_CORE_C0 = 0,
	TEGRA_ARI_CORE_C1 = 1,
	TEGRA_ARI_CORE_C6 = 6,
	TEGRA_ARI_CORE_C7 = 7,
	TEGRA_ARI_CORE_WARMRSTREQ = 8,
} tegra_ari_core_sleep_state_t;

typedef enum {
	TEGRA_ARI_CLUSTER_CC0 = 0,
	TEGRA_ARI_CLUSTER_CC1 = 1,
	TEGRA_ARI_CLUSTER_CC6 = 6,
	TEGRA_ARI_CLUSTER_CC7 = 7,
} tegra_ari_cluster_sleep_state_t;

typedef enum {
	TEGRA_ARI_CCPLEX_CCP0 = 0,
	TEGRA_ARI_CCPLEX_CCP1 = 1,
	TEGRA_ARI_CCPLEX_CCP3 = 3,
} tegra_ari_ccplex_sleep_state_t;

typedef enum {
	TEGRA_ARI_SYSTEM_SC0 = 0,
	TEGRA_ARI_SYSTEM_SC1 = 1,
	TEGRA_ARI_SYSTEM_SC2 = 2,
	TEGRA_ARI_SYSTEM_SC3 = 3,
	TEGRA_ARI_SYSTEM_SC4 = 4,
	TEGRA_ARI_SYSTEM_SC7 = 7,
	TEGRA_ARI_SYSTEM_SC8 = 8,
} tegra_ari_system_sleep_state_t;

typedef enum {
	TEGRA_ARI_CROSSOVER_C1_C6 = 0,
	TEGRA_ARI_CROSSOVER_CC1_CC6 = 1,
	TEGRA_ARI_CROSSOVER_CC1_CC7 = 2,
	TEGRA_ARI_CROSSOVER_CCP1_CCP3 = 3,
	TEGRA_ARI_CROSSOVER_CCP3_SC2 = 4,
	TEGRA_ARI_CROSSOVER_CCP3_SC3 = 5,
	TEGRA_ARI_CROSSOVER_CCP3_SC4 = 6,
	TEGRA_ARI_CROSSOVER_CCP3_SC7 = 7,
	TEGRA_ARI_CROSSOVER_CCP3_SC1 = 8,
} tegra_ari_crossover_index_t;

typedef enum {
	TEGRA_ARI_CSTATE_STATS_CLEAR = 0,
	TEGRA_ARI_CSTATE_STATS_SC7_ENTRIES = 1,
	TEGRA_ARI_CSTATE_STATS_SC4_ENTRIES,
	TEGRA_ARI_CSTATE_STATS_SC3_ENTRIES,
	TEGRA_ARI_CSTATE_STATS_SC2_ENTRIES,
	TEGRA_ARI_CSTATE_STATS_CCP3_ENTRIES,
	TEGRA_ARI_CSTATE_STATS_A57_CC6_ENTRIES,
	TEGRA_ARI_CSTATE_STATS_A57_CC7_ENTRIES,
	TEGRA_ARI_CSTATE_STATS_D15_CC6_ENTRIES,
	TEGRA_ARI_CSTATE_STATS_D15_CC7_ENTRIES,
	TEGRA_ARI_CSTATE_STATS_D15_0_C6_ENTRIES,
	TEGRA_ARI_CSTATE_STATS_D15_1_C6_ENTRIES,
	TEGRA_ARI_CSTATE_STATS_D15_0_C7_ENTRIES = 14,
	TEGRA_ARI_CSTATE_STATS_D15_1_C7_ENTRIES,
	TEGRA_ARI_CSTATE_STATS_A57_0_C7_ENTRIES = 18,
	TEGRA_ARI_CSTATE_STATS_A57_1_C7_ENTRIES,
	TEGRA_ARI_CSTATE_STATS_A57_2_C7_ENTRIES,
	TEGRA_ARI_CSTATE_STATS_A57_3_C7_ENTRIES,
	TEGRA_ARI_CSTATE_STATS_LAST_CSTATE_ENTRY_D15_0,
	TEGRA_ARI_CSTATE_STATS_LAST_CSTATE_ENTRY_D15_1,
	TEGRA_ARI_CSTATE_STATS_LAST_CSTATE_ENTRY_A57_0 = 26,
	TEGRA_ARI_CSTATE_STATS_LAST_CSTATE_ENTRY_A57_1,
	TEGRA_ARI_CSTATE_STATS_LAST_CSTATE_ENTRY_A57_2,
	TEGRA_ARI_CSTATE_STATS_LAST_CSTATE_ENTRY_A57_3,
	TEGRA_ARI_CSTATE_STATS_MAX = TEGRA_ARI_CSTATE_STATS_LAST_CSTATE_ENTRY_A57_3,
} tegra_ari_cstate_stats_index_t;

typedef enum {
	TEGRA_ARI_GSC_ALL = 0,
	TEGRA_ARI_GSC_BPMP = 6,
	TEGRA_ARI_GSC_APE = 7,
	TEGRA_ARI_GSC_SPE = 8,
	TEGRA_ARI_GSC_SCE = 9,
	TEGRA_ARI_GSC_APR = 10,
	TEGRA_ARI_GSC_TZRAM = 11,
	TEGRA_ARI_GSC_SE = 12,

	TEGRA_ARI_GSC_BPMP_TO_SPE = 16,
	TEGRA_ARI_GSC_SPE_TO_BPMP = 17,
	TEGRA_ARI_GSC_CPU_TZ_TO_BPMP = 18,
	TEGRA_ARI_GSC_BPMP_TO_CPU_TZ = 19,
	TEGRA_ARI_GSC_CPU_NS_TO_BPMP = 20,
	TEGRA_ARI_GSC_BPMP_TO_CPU_NS = 21,
	TEGRA_ARI_GSC_IPC_SE_SPE_SCE_BPMP = 22,
	TEGRA_ARI_GSC_SC7_RESUME_FW = 23,

	TEGRA_ARI_GSC_TZ_DRAM_IDX = 34,
	TEGRA_ARI_GSC_VPR_IDX = 35,
} tegra_ari_gsc_index_t;

/* This macro will produce enums for __name##_LSB, __name##_MSB and __name##_MSK */
#define TEGRA_ARI_ENUM_MASK_LSB_MSB(__name,__lsb,__msb) __name##_LSB = __lsb, __name##_MSB = __msb, __name##_MSK = ((1ull << __msb) | (((1ull << __msb) - 1) ^ ((1ull << __lsb) - 1)))

typedef enum {
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_UPDATE_CSTATE_INFO__CLUSTER_CSTATE, 0, 2),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_UPDATE_CSTATE_INFO__CLUSTER_CSTATE_PRESENT, 7, 7),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_UPDATE_CSTATE_INFO__CCPLEX_CSTATE, 8, 9),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_UPDATE_CSTATE_INFO__CCPLEX_CSTATE_PRESENT, 15, 15),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_UPDATE_CSTATE_INFO__SYSTEM_CSTATE, 16, 19),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_UPDATE_CSTATE_INFO__IGNORE_CROSSOVERS, 22, 22),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_UPDATE_CSTATE_INFO__SYSTEM_CSTATE_PRESENT, 23, 23),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_UPDATE_CSTATE_INFO__WAKE_MASK_PRESENT, 31, 31),
} tegra_ari_update_cstate_info_bitmasks_t;

typedef enum {
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MISC_CCPLEX_CORESIGHT_CG_CTRL__EN, 0, 0),
} tegra_ari_misc_ccplex_bitmasks_t;

typedef enum {
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_CC3_CTRL__IDLE_FREQ, 0, 8),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_CC3_CTRL__IDLE_VOLT, 16, 23),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_CC3_CTRL__ENABLE, 31, 31),
} tegra_ari_cc3_ctrl_bitmasks_t;

typedef enum {
	TEGRA_ARI_MCA_NOP = 0,
	TEGRA_ARI_MCA_READ_SERR = 1,
	TEGRA_ARI_MCA_WRITE_SERR = 2,
	TEGRA_ARI_MCA_CLEAR_SERR = 4,
	TEGRA_ARI_MCA_REPORT_SERR = 5,
	TEGRA_ARI_MCA_READ_INTSTS = 6,
	TEGRA_ARI_MCA_WRITE_INTSTS = 7,
	TEGRA_ARI_MCA_READ_PREBOOT_SERR = 8,
} tegra_ari_mca_commands_t;

typedef enum {
	TEGRA_ARI_MCA_RD_WR_DPMU = 0,
	TEGRA_ARI_MCA_RD_WR_IOB = 1,
	TEGRA_ARI_MCA_RD_WR_MCB = 2,
	TEGRA_ARI_MCA_RD_WR_CCE = 3,
	TEGRA_ARI_MCA_RD_WR_CQX = 4,
	TEGRA_ARI_MCA_RD_WR_CTU = 5,
	TEGRA_ARI_MCA_RD_BANK_INFO = 0x0f,
	TEGRA_ARI_MCA_RD_BANK_TEMPLATE = 0x10,
	TEGRA_ARI_MCA_RD_WR_SECURE_ACCESS_REGISTER = 0x11,
	TEGRA_ARI_MCA_RD_WR_GLOBAL_CONFIG_REGISTER = 0x12,
} tegra_ari_mca_rd_wr_indexes_t;

typedef enum {
	TEGRA_ARI_MCA_RD_WR_ASERRX_CTRL = 0,
	TEGRA_ARI_MCA_RD_WR_ASERRX_STATUS = 1,
	TEGRA_ARI_MCA_RD_WR_ASERRX_ADDR = 2,
	TEGRA_ARI_MCA_RD_WR_ASERRX_MISC1 = 3,
	TEGRA_ARI_MCA_RD_WR_ASERRX_MISC2 = 4,
} tegra_ari_mca_read_asserx_subindexes_t;

typedef enum {
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_SECURE_REGISTER_SETTING_ENABLES_NS_PERMITTED, 0, 0),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_SECURE_REGISTER_READING_STATUS_NS_PERMITTED, 1, 1),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_SECURE_REGISTER_PENDING_MCA_ERRORS_NS_PERMITTED, 2, 2),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_SECURE_REGISTER_CLEARING_MCA_INTERRUPTS_NS_PERMITTED, 3, 3),
} tegra_ari_mca_secure_register_bitmasks_t;

typedef enum {
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_STAT_SERR_ERR_CODE, 0, 15),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_STAT_PWM_ERR, 16, 16),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_STAT_CRAB_ERR, 17, 17),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_STAT_RD_WR_N, 18, 18),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_STAT_UCODE_ERR, 19, 19),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_STAT_PWM, 20, 23),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_STAT_AV, 58, 58),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_STAT_MV, 59, 59),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_STAT_EN, 60, 60),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_STAT_UC, 61, 61),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_STAT_OVF, 62, 62),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_STAT_VAL, 63, 63),

	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_ADDR_ADDR, 0, 41),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_ADDR_UCODE_ERRCD, 42, 52),

	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_CTRL_EN_PWM_ERR, 0, 0),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_CTRL_EN_CRAB_ERR, 1, 1),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR0_CTRL_EN_UCODE_ERR, 3, 3),
} tegra_ari_mca_aserr0_bitmasks_t;

typedef enum {
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_SERR_ERR_CODE, 0, 15),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_MSI_ERR, 16, 16),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_IHI_ERR, 17, 17),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_CRI_ERR, 18, 18),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_MMCRAB_ERR, 19, 19),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_CSI_ERR, 20, 20),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_RD_WR_N, 21, 21),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_REQ_ERRT, 22, 23),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_RESP_ERRT, 24, 25),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_AV, 58, 58),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_MV, 59, 59),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_EN, 60, 60),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_UC, 61, 61),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_OVF, 62, 62),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_VAL, 63, 63),

	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_AXI_ID, 0, 7),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_CQX_ID, 8, 27),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_CQX_CID, 28, 31),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_STAT_CQX_CMD, 32, 35),

	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_CTRL_EN_MSI_ERR, 0, 0),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_CTRL_EN_IHI_ERR, 1, 1),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_CTRL_EN_CRI_ERR, 2, 2),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_CTRL_EN_MMCRAB_ERR, 3, 3),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_CTRL_EN_CSI_ERR, 4, 4),

	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR1_MISC_ADDR, 0, 41),
} tegra_ari_mca_aserr1_bitmasks_t;

typedef enum {
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR2_STAT_SERR_ERR_CODE, 0, 15),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR2_STAT_MC_ERR, 16, 16),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR2_STAT_SYSRAM_ERR, 17, 17),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR2_STAT_CLIENT_ID, 18, 19),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR2_STAT_AV, 58, 58),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR2_STAT_MV, 59, 59),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR2_STAT_EN, 60, 60),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR2_STAT_UC, 61, 61),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR2_STAT_OVF, 62, 62),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR2_STAT_VAL, 63, 63),

	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR2_ADDR_ID, 0, 17),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR2_ADDR_CMD, 18, 21),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR2_ADDR_ADDR, 22, 53),

	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR2_CTRL_EN_MC_ERR, 0, 0),
} tegra_ari_mca_aserr2_bitmasks_t;

typedef enum {
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_STAT_SERR_ERR_CODE, 0, 15),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_STAT_TO_ERR, 16, 16),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_STAT_STAT_ERR, 17, 17),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_STAT_DST_ERR, 18, 18),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_STAT_UNC_ERR, 19, 19),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_STAT_MH_ERR, 20, 20),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_STAT_PERR, 21, 21),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_STAT_PSN_ERR, 22, 22),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_STAT_AV, 58, 58),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_STAT_MV, 59, 59),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_STAT_EN, 60, 60),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_STAT_UC, 61, 61),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_STAT_OVF, 62, 62),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_STAT_VAL, 63, 63),

	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_ADDR_CMD, 0, 5),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_ADDR_ADDR, 6, 47),

	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_MISC1_TO, 0, 0),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_MISC1_DIV4, 1, 1),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_MISC1_TLIMIT, 2, 11),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_MISC1_PSN_ERR_CORR_MSK, 12, 25),

	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_MISC2_MORE_INFO, 0, 17),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_MISC2_TO_INFO, 18, 43),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_MISC2_SRC, 44, 45),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_MISC2_TID, 46, 52),

	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_CTRL_EN_TO_ERR, 0, 0),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_CTRL_EN_STAT_ERR, 1, 1),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_CTRL_EN_DST_ERR, 2, 2),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_CTRL_EN_UNC_ERR, 3, 3),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_CTRL_EN_MH_ERR, 4, 4),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_CTRL_EN_PERR, 5, 5),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR3_CTRL_EN_PSN_ERR, 6, 19),
} tegra_ari_mca_aserr3_bitmasks_t;

typedef enum {
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR4_STAT_SERR_ERR_CODE, 0, 15),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR4_STAT_SRC_ERR, 16, 16),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR4_STAT_DST_ERR, 17, 17),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR4_STAT_REQ_ERR, 18, 18),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR4_STAT_RSP_ERR, 19, 19),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR4_STAT_AV, 58, 58),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR4_STAT_MV, 59, 59),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR4_STAT_EN, 60, 60),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR4_STAT_UC, 61, 61),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR4_STAT_OVF, 62, 62),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR4_STAT_VAL, 63, 63),

	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR4_CTRL_EN_CPE_ERR, 0, 0),
} tegra_ari_mca_aserr4_bitmasks_t;

typedef enum {
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_STAT_SERR_ERR_CODE, 0, 15),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_STAT_CTUPAR, 16, 16),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_STAT_MULTI, 17, 17),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_STAT_AV, 58, 58),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_STAT_MV, 59, 59),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_STAT_EN, 60, 60),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_STAT_UC, 61, 61),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_STAT_OVF, 62, 62),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_STAT_VAL, 63, 63),

	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_ADDR_SRC, 0, 7),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_ADDR_ID, 8, 15),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_ADDR_DATA, 16, 26),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_ADDR_CMD, 32, 35),
	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_ADDR_ADDR, 36, 45),

	TEGRA_ARI_ENUM_MASK_LSB_MSB(TEGRA_ARI_MCA_ASERR5_CTRL_EN_CTUPAR, 0, 0),
} tegra_ari_mca_aserr5_bitmasks_t;

#undef TEGRA_ARI_ENUM_MASK_LSB_MSB

typedef enum {
	TEGRA_NVG_CHANNEL_PMIC = 0,
	TEGRA_NVG_CHANNEL_POWER_PERF = 1,
	TEGRA_NVG_CHANNEL_POWER_MODES = 2,
	TEGRA_NVG_CHANNEL_WAKE_TIME = 3,
	TEGRA_NVG_CHANNEL_CSTATE_INFO = 4,
	TEGRA_NVG_CHANNEL_CROSSOVER_C1_C6 = 5,
	TEGRA_NVG_CHANNEL_CROSSOVER_CC1_CC6 = 6,
	TEGRA_NVG_CHANNEL_CROSSOVER_CC1_CC7 = 7,
	TEGRA_NVG_CHANNEL_CROSSOVER_CCP1_CCP3 = 8,
	TEGRA_NVG_CHANNEL_CROSSOVER_CCP3_SC2 = 9,
	TEGRA_NVG_CHANNEL_CROSSOVER_CCP3_SC3 = 10,
	TEGRA_NVG_CHANNEL_CROSSOVER_CCP3_SC4 = 11,
	TEGRA_NVG_CHANNEL_CROSSOVER_CCP3_SC7 = 12,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_CLEAR = 13,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_SC7_ENTRIES = 14,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_SC4_ENTRIES = 15,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_SC3_ENTRIES = 16,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_SC2_ENTRIES = 17,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_CCP3_ENTRIES = 18,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_A57_CC6_ENTRIES = 19,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_A57_CC7_ENTRIES = 20,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_D15_CC6_ENTRIES = 21,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_D15_CC7_ENTRIES = 22,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_D15_0_C6_ENTRIES = 23,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_D15_1_C6_ENTRIES = 24,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_D15_2_C6_ENTRIES = 25, /* Reserved (for Denver15 core 2) */
	TEGRA_NVG_CHANNEL_CSTATE_STATS_D15_3_C6_ENTRIES = 26, /* Reserved (for Denver15 core 3) */
	TEGRA_NVG_CHANNEL_CSTATE_STATS_D15_0_C7_ENTRIES = 27,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_D15_1_C7_ENTRIES = 28,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_D15_2_C7_ENTRIES = 29, /* Reserved (for Denver15 core 2) */
	TEGRA_NVG_CHANNEL_CSTATE_STATS_D15_3_C7_ENTRIES = 30, /* Reserved (for Denver15 core 3) */
	TEGRA_NVG_CHANNEL_CSTATE_STATS_A57_0_C7_ENTRIES = 31,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_A57_1_C7_ENTRIES = 32,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_A57_2_C7_ENTRIES = 33,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_A57_3_C7_ENTRIES = 34,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_LAST_CSTATE_ENTRY_D15_0 = 35,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_LAST_CSTATE_ENTRY_D15_1 = 36,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_LAST_CSTATE_ENTRY_D15_2 = 37, /* Reserved (for Denver15 core 2) */
	TEGRA_NVG_CHANNEL_CSTATE_STATS_LAST_CSTATE_ENTRY_D15_3 = 38, /* Reserved (for Denver15 core 3) */
	TEGRA_NVG_CHANNEL_CSTATE_STATS_LAST_CSTATE_ENTRY_A57_0 = 39,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_LAST_CSTATE_ENTRY_A57_1 = 40,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_LAST_CSTATE_ENTRY_A57_2 = 41,
	TEGRA_NVG_CHANNEL_CSTATE_STATS_LAST_CSTATE_ENTRY_A57_3 = 42,
	TEGRA_NVG_CHANNEL_IS_SC7_ALLOWED = 43,
	TEGRA_NVG_CHANNEL_ONLINE_CORE = 44,
	TEGRA_NVG_CHANNEL_CC3_CTRL = 45,
	TEGRA_NVG_CHANNEL_CROSSOVER_CCP3_SC1 = 46,
	TEGRA_NVG_CHANNEL_LAST_INDEX,
} tegra_nvg_channel_id_t;

#endif /* T18X_TEGRA_ARI_H */
