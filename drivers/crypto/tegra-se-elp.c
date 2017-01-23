/*
 * Cryptographic API.
 * drivers/crypto/tegra-se-elp.c
 *
 * Support for Tegra Security Engine Elliptic crypto algorithms.
 *
 * Copyright (c) 2015-2017, NVIDIA Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation, and may be copied,
 * distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <soc/tegra/chip-id.h>
#include <crypto/akcipher.h>
#include <crypto/internal/akcipher.h>
#include <crypto/internal/rng.h>
#include <crypto/internal/kpp.h>
#include <crypto/kpp.h>
#include <linux/fips.h>
#include <crypto/ecdh.h>

#include <crypto/internal/akcipher.h>
#include <crypto/akcipher.h>
#include <crypto/ecdsa.h>

#include "tegra-se-elp.h"

#include "../../crypto/ecc.h"
#include "../../crypto/ecc_curve_defs.h"

#define DRIVER_NAME	"tegra-se-elp"

#define NR_RES	2
#define PKA1	0
#define RNG1	1

#define TEGRA_SE_MUTEX_WDT_UNITS	0x600000
#define RNG1_TIMEOUT			2000	/*micro seconds*/
#define PKA1_TIMEOUT			1000000	/*micro seconds*/
#define RAND_128			16	/*bytes*/
#define RAND_256			32	/*bytes*/
#define ADV_STATE_FREQ			3
#define ECC_MAX_WORDS	20
#define WORD_SIZE_BYTES	4
#define MAX_PKA1_SIZE	TEGRA_SE_PKA1_RSA4096_INPUT_SIZE

#define ECDSA_USE_SHAMIRS_TRICK		1

enum tegra_se_pka_mod_type {
	MOD_MULT,
	MOD_ADD,
	MOD_SUB,
	MOD_REDUCE,
	MOD_DIV,
	MOD_INV,
};

enum tegra_se_pka1_ecc_type {
	ECC_POINT_MUL,
	ECC_POINT_ADD,
	ECC_POINT_DOUBLE,
	ECC_POINT_VER,
	ECC_SHAMIR_TRICK,
	ECC_INVALID,
};

enum tegra_se_elp_precomp_vals {
	PRECOMP_RINV,
	PRECOMP_M,
	PRECOMP_R2,
};

enum tegra_se_rng1_cmd {
	RNG1_CMD_NOP,
	RNG1_CMD_GEN_NOISE,
	RNG1_CMD_GEN_NONCE,
	RNG1_CMD_CREATE_STATE,
	RNG1_CMD_RENEW_STATE,
	RNG1_CMD_REFRESH_ADDIN,
	RNG1_CMD_GEN_RANDOM,
	RNG1_CMD_ADVANCE_STATE,
	RNG1_CMD_KAT,
	RNG1_CMD_ZEROIZE = 15,
};

static char *rng1_cmd[] = {
	"RNG1_CMD_NOP",
	"RNG1_CMD_GEN_NOISE",
	"RNG1_CMD_GEN_NONCE",
	"RNG1_CMD_CREATE_STATE",
	"RNG1_CMD_RENEW_STATE",
	"RNG1_CMD_REFRESH_ADDIN",
	"RNG1_CMD_GEN_RANDOM",
	"RNG1_CMD_ADVANCE_STATE",
	"RNG1_CMD_KAT",
	"RNG1_CMD_ZEROIZE"
};

struct tegra_se_elp_chipdata {
	bool use_key_slot;
	bool rng1_supported;
};

struct tegra_se_elp_dev {
	struct device *dev;
	void __iomem *io_reg[2];
	int irq;	/* irq allocated */
	struct clk *c;
	struct completion complete;
	struct tegra_se_pka1_slot *slot_list;
	const struct tegra_se_elp_chipdata *chipdata;
	u32 *rdata;
};

static struct tegra_se_elp_dev *elp_dev;

struct tegra_se_rng1_request {
	u32 size;
	u32 *rdata;
	u32 *rdata1;
	u32 *rdata2;
	u32 *rdata3;
	bool test_full_cmd_flow;
	bool adv_state_on;
};

struct tegra_se_pka1_ecc_request {
	struct tegra_se_elp_dev *se_dev;
	struct tegra_se_pka1_slot *slot;
	u32 *message;
	u32 *result;
	u32 *modulus;
	u32 *m;
	u32 *r2;
	u32 size;
	u32 op_mode;
	u32 type;
	u32 *curve_param_a;
	u32 *curve_param_b;
	u32 *order;
	u32 *base_pt_x;
	u32 *base_pt_y;
	u32 *res_pt_x;
	u32 *res_pt_y;
	u32 *key;
	bool pv_ok;
};

struct tegra_se_pka1_mod_request {
	struct tegra_se_elp_dev *se_dev;
	u32 *result;
	u32 *modulus;
	u32 *m;
	u32 *r2;
	u32 size;
	u32 op_mode;
	u32 type;
	u32 *base_pt_x;
	u32 *base_pt_y;
	u32 *res_pt_x;
	u32 *res_pt_y;
};

struct tegra_se_pka1_rsa_context {
	struct tegra_se_elp_dev *se_dev;
	struct tegra_se_pka1_slot *slot;
	u32 *result;
	u32 *message;
	u32 *exponent;
	u32 *modulus;
	u32 *m;
	u32 *r2;
	u32 op_mode;
	u32 keylen;
};

/* Security Engine key slot */
struct tegra_se_pka1_slot {
	struct list_head node;
	u8 slot_num;	/* Key slot number */
	atomic_t available; /* Tells whether key slot is free to use */
};

static LIST_HEAD(key_slot);

static u32 pka1_op_size[] = {512, 768, 1024, 1536, 2048, 3072, 4096,
			     160, 192, 224, 256, 384, 512, 640,
			     160, 192, 224, 256, 384, 512, 640};

struct tegra_se_ecdh_context {
	struct tegra_se_elp_dev *se_dev;
	unsigned int curve_id;
	u32 private_key[ECC_MAX_WORDS];
	u32 public_key[2 * ECC_MAX_WORDS];
	u32 shared_secret[ECC_MAX_WORDS];
};

static struct tegra_se_ecc_point *tegra_se_ecc_alloc_point(
				struct tegra_se_elp_dev *se_dev, int nwords)
{
	struct tegra_se_ecc_point *p = devm_kzalloc(se_dev->dev,
					sizeof(struct tegra_se_ecc_point),
					GFP_KERNEL);
	int len = nwords * WORD_SIZE_BYTES;

	if (!p)
		return NULL;

	p->x = devm_kzalloc(se_dev->dev, len, GFP_KERNEL);
	if (!p->x)
		goto free_pt;

	p->y = devm_kzalloc(se_dev->dev, len, GFP_KERNEL);
	if (!p->y)
		goto exit;

	return p;
exit:
	devm_kfree(se_dev->dev, p->x);
free_pt:
	devm_kfree(se_dev->dev, p);
	return NULL;
}

static void tegra_se_ecc_free_point(struct tegra_se_elp_dev *se_dev,
				    struct tegra_se_ecc_point *p)
{
	if (!p)
		return;

	devm_kfree(se_dev->dev, p->x);
	devm_kfree(se_dev->dev, p->y);
	devm_kfree(se_dev->dev, p);
}

static const struct tegra_se_ecc_curve *tegra_se_ecc_get_curve(
						unsigned int curve_id)
{
	switch (curve_id) {
	/* In FIPS mode only allow P256 and higher */
	case ECC_CURVE_NIST_P192:
		return fips_enabled ? NULL : &curve_p192;
	case ECC_CURVE_NIST_P224:
		return fips_enabled ? NULL : &curve_p224;
	case ECC_CURVE_NIST_P256:
		return &curve_p256;
	case ECC_CURVE_NIST_P384:
		return &curve_p384;
	case ECC_CURVE_NIST_P521:
		return &curve_p521;
	default:
		return NULL;
	}
}

static inline void tegra_se_ecc_swap(const u32 *in, u32 *out, int nwords)
{
	int i;

	for (i = 0; i < nwords; i++)
		out[i] = swab32(in[nwords - 1 - i]);
}

static bool tegra_se_ecc_vec_is_zero(const u32 *vec, int nbytes)
{
	unsigned int zerobuf[ECC_MAX_WORDS * WORD_SIZE_BYTES] = {0};

	return !memcmp((u8 *)vec, zerobuf, nbytes);
}

/* Returns true if vec1 > vec2 */
static bool tegra_se_ecc_vec_cmp(const u8 *vec1, const u8 *vec2,
				 unsigned int nbytes)
{
	int i;

	for (i = nbytes - 1; i >= 0; i--) {
		if (vec1[i] > vec2[i])
			return true;
		else if (vec1[i] < vec2[i])
			return false;
	}

	return false;
}

static bool tegra_se_ecdh_params_is_valid(struct ecdh *params)
{
	const u32 *private_key = (const u32 *)params->key;
	int private_key_len = params->key_size;
	const struct tegra_se_ecc_curve *curve = tegra_se_ecc_get_curve(
							params->curve_id);
	const u32 *order = curve->n;
	int nbytes = curve->nbytes;

	if (!nbytes || !private_key)
		return false;

	if (private_key_len != nbytes)
		return false;

	if (tegra_se_ecc_vec_is_zero(private_key, nbytes))
		return false;

	/* Make sure the private key is in the range [1, n-1]. */
	if (!tegra_se_ecc_vec_cmp((u8 *)order, (u8 *)private_key, nbytes))
		return false;

	return true;
}

static int tegra_se_ecdh_set_params(struct tegra_se_ecdh_context *ctx,
				    struct ecdh *params)
{
	if (!tegra_se_ecdh_params_is_valid(params))
		return -EINVAL;

	ctx->curve_id = params->curve_id;

	memcpy(ctx->private_key, params->key, params->key_size);

	return 0;
}

static inline u32 num_words(int mode)
{
	u32 words = 0;

	switch (mode) {
	case SE_ELP_OP_MODE_ECC160:
	case SE_ELP_OP_MODE_ECC192:
	case SE_ELP_OP_MODE_ECC224:
	case SE_ELP_OP_MODE_ECC256:
	case SE_ELP_OP_MODE_MOD160:
	case SE_ELP_OP_MODE_MOD192:
	case SE_ELP_OP_MODE_MOD224:
	case SE_ELP_OP_MODE_MOD256:
		words = pka1_op_size[SE_ELP_OP_MODE_ECC256] / 32;
		break;
	case SE_ELP_OP_MODE_RSA512:
	case SE_ELP_OP_MODE_ECC384:
	case SE_ELP_OP_MODE_ECC512:
	case SE_ELP_OP_MODE_MOD384:
	case SE_ELP_OP_MODE_MOD512:
		words = pka1_op_size[SE_ELP_OP_MODE_RSA512] / 32;
		break;
	case SE_ELP_OP_MODE_RSA768:
	case SE_ELP_OP_MODE_RSA1024:
	case SE_ELP_OP_MODE_ECC521:
		words = pka1_op_size[SE_ELP_OP_MODE_RSA1024] / 32;
		break;
	case SE_ELP_OP_MODE_RSA1536:
	case SE_ELP_OP_MODE_RSA2048:
		words = pka1_op_size[SE_ELP_OP_MODE_RSA2048] / 32;
		break;
	case SE_ELP_OP_MODE_RSA3072:
	case SE_ELP_OP_MODE_RSA4096:
		words = pka1_op_size[SE_ELP_OP_MODE_RSA4096] / 32;
		break;
	default:
		dev_warn(elp_dev->dev, "Invalid operation mode\n");
		break;
	}

	return words;
}

static inline void se_elp_writel(struct tegra_se_elp_dev *se_dev, int elp_type,
				 unsigned int val, unsigned int reg_offset)
{
	writel(val, se_dev->io_reg[elp_type] + reg_offset);
}

static inline unsigned int se_elp_readl(struct tegra_se_elp_dev *se_dev,
					int elp_type, unsigned int reg_offset)
{
	return readl(se_dev->io_reg[elp_type] + reg_offset);
}

static void tegra_se_pka1_free_key_slot(struct tegra_se_pka1_slot *slot)
{
	if (!slot)
		return;

	atomic_set(&slot->available, 1);
}

static struct tegra_se_pka1_slot *tegra_se_pka1_alloc_key_slot(void)
{
	struct tegra_se_pka1_slot *slot;
	bool found = false;

	list_for_each_entry(slot, &key_slot, node) {
		if (atomic_read(&slot->available)) {
			atomic_set(&slot->available, 0);
			found = true;
			break;
		}
	}

	return found ? slot : NULL;
}

static int tegra_se_pka1_init_key_slot(struct tegra_se_elp_dev *se_dev)
{
	int i;

	se_dev->slot_list = devm_kzalloc(se_dev->dev,
					 sizeof(struct tegra_se_pka1_slot) *
					 TEGRA_SE_PKA1_KEYSLOT_COUNT,
					 GFP_KERNEL);
	if (!se_dev->slot_list)
		return -ENOMEM;

	for (i = 0; i < TEGRA_SE_PKA1_KEYSLOT_COUNT; i++) {
		atomic_set(&se_dev->slot_list[i].available, 1);
		se_dev->slot_list[i].slot_num = i;
		INIT_LIST_HEAD(&se_dev->slot_list[i].node);
		list_add_tail(&se_dev->slot_list[i].node, &key_slot);
	}

	return 0;
}

static u32 tegra_se_check_trng_op(struct tegra_se_elp_dev *se_dev)
{
	u32 trng_val;
	u32 val = se_elp_readl(se_dev, PKA1,
			       TEGRA_SE_PKA1_TRNG_STATUS_OFFSET);

	trng_val = TEGRA_SE_PKA1_TRNG_STATUS_SECURE(ELP_TRUE) |
			TEGRA_SE_PKA1_TRNG_STATUS_NONCE(ELP_FALSE) |
			TEGRA_SE_PKA1_TRNG_STATUS_SEEDED(ELP_TRUE) |
			TEGRA_SE_PKA1_TRNG_STATUS_LAST_RESEED(
						TRNG_LAST_RESEED_HOST);
	if ((val & trng_val) ||
	    (val & TEGRA_SE_PKA1_TRNG_STATUS_LAST_RESEED
					(TRNG_LAST_RESEED_RESEED)))
		return 0;

	return -EINVAL;
}

static u32 tegra_se_set_trng_op(struct tegra_se_elp_dev *se_dev)
{
	u32 val, i = 0;

	se_elp_writel(se_dev, PKA1,
		      TEGRA_SE_PKA1_TRNG_SMODE_SECURE(ELP_ENABLE) |
		      TEGRA_SE_PKA1_TRNG_SMODE_NONCE(ELP_DISABLE),
		      TEGRA_SE_PKA1_TRNG_SMODE_OFFSET);
	se_elp_writel(se_dev, PKA1,
		      TEGRA_SE_PKA1_CTRL_CONTROL_AUTO_RESEED(ELP_ENABLE),
		      TEGRA_SE_PKA1_CTRL_CONTROL_OFFSET);

	/* Poll seeded status */
	do {
		if (i > PKA1_TIMEOUT) {
			dev_err(se_dev->dev,
				"Poll TRNG seeded status timed out\n");
			return -EINVAL;
		}
		udelay(1);
		val = se_elp_readl(se_dev, PKA1,
				   TEGRA_SE_PKA1_TRNG_STATUS_OFFSET);
		i++;
	} while (val & TEGRA_SE_PKA1_TRNG_STATUS_SEEDED(ELP_FALSE));

	return 0;
}

static void tegra_se_restart_pka1_mutex_wdt(struct tegra_se_elp_dev *se_dev)
{
	se_elp_writel(se_dev, PKA1, TEGRA_SE_MUTEX_WDT_UNITS,
		      TEGRA_SE_PKA1_MUTEX_WATCHDOG_OFFSET);
}

static u32 tegra_se_acquire_pka1_mutex(struct tegra_se_elp_dev *se_dev)
{
	u32 val, i = 0;

	/* Acquire pka mutex */
	do {
		if (i > PKA1_TIMEOUT) {
			dev_err(se_dev->dev, "Acquire PKA1 Mutex timed out\n");
			return -EINVAL;
		}
		udelay(1);
		val = se_elp_readl(se_dev, PKA1, TEGRA_SE_PKA1_MUTEX_OFFSET);
		i++;
	} while (val != 0x01);

	/* One unit is 256 SE Cycles */
	tegra_se_restart_pka1_mutex_wdt(se_dev);
	se_elp_writel(se_dev, PKA1, TEGRA_SE_PKA1_MUTEX_TIMEOUT_ACTION,
		      TEGRA_SE_PKA1_MUTEX_TIMEOUT_ACTION_OFFSET);

	return 0;
}

static void tegra_se_release_pka1_mutex(struct tegra_se_elp_dev *se_dev)
{
	int i = 0;
	u32 val;

	se_elp_writel(se_dev, PKA1, 0x01,
		      TEGRA_SE_PKA1_MUTEX_OFFSET);
	/* poll SE_STATUS*/
	do {
		if (i > PKA1_TIMEOUT) {
			dev_warn(se_dev->dev, "PKA1 Scrub timed out\n");
			break;
		}
		udelay(1);
		val = se_elp_readl(se_dev, PKA1,
				   TEGRA_SE_PKA1_CTRL_STATUS_OFFSET);
		i++;
	} while (val & TEGRA_SE_PKA1_CTRL_SE_STATUS(SE_STATUS_BUSY));
}

static inline u32 pka1_bank_start(u32 bank)
{
	return PKA1_BANK_START_A + (bank * 0x400);
}

static inline u32 reg_bank_offset(u32 bank, u32 idx, u32 mode)
{
	return pka1_bank_start(bank) + ((idx * 4) * num_words(mode));
}

static void tegra_se_pka1_ecc_fill_input(struct tegra_se_pka1_ecc_request *req)
{
	struct tegra_se_elp_dev *se_dev = req->se_dev;
	u32 i;
	int len = 0;
	int nwords = req->size / 4;
	int nwords_521 = pka1_op_size[SE_ELP_OP_MODE_ECC521] / 32;
	u32 *MOD, *A, *B, *PX, *PY, *K, *QX, *QY;

	if (req->op_mode == SE_ELP_OP_MODE_ECC521) {
		MOD = req->modulus;
		for (i = 0; i < nwords; i++)
			se_elp_writel(se_dev, PKA1, *MOD++, reg_bank_offset(
				      TEGRA_SE_PKA1_MOD_BANK,
				      TEGRA_SE_PKA1_MOD_ID,
				      req->op_mode) + (i * 4));

		for (i = nwords; i < nwords_521; i++)
			se_elp_writel(se_dev, PKA1, 0x0, reg_bank_offset(
				      TEGRA_SE_PKA1_MOD_BANK,
				      TEGRA_SE_PKA1_MOD_ID,
				      req->op_mode) + (i * 4));
	}

	A = req->curve_param_a;

	for (i = 0; i < nwords; i++)
		se_elp_writel(se_dev, PKA1, *A++, reg_bank_offset(
			      TEGRA_SE_PKA1_ECC_A_BANK,
			      TEGRA_SE_PKA1_ECC_A_ID,
			      req->op_mode) + (i * 4));

	if (req->op_mode == SE_ELP_OP_MODE_ECC521) {
		for (i = nwords; i < nwords_521; i++)
			se_elp_writel(se_dev, PKA1, 0x0, reg_bank_offset(
				      TEGRA_SE_PKA1_ECC_A_BANK,
				      TEGRA_SE_PKA1_ECC_A_ID,
				      req->op_mode) + (i * 4));
	}

	if (req->type != ECC_POINT_DOUBLE) {
		PX = req->base_pt_x;
		PY = req->base_pt_y;
		for (i = 0; i < nwords; i++) {
			se_elp_writel(se_dev, PKA1, *PX++, reg_bank_offset(
				      TEGRA_SE_PKA1_ECC_XP_BANK,
				      TEGRA_SE_PKA1_ECC_XP_ID,
				      req->op_mode) + (i * 4));

			se_elp_writel(se_dev, PKA1, *PY++, reg_bank_offset(
				      TEGRA_SE_PKA1_ECC_YP_BANK,
				      TEGRA_SE_PKA1_ECC_YP_ID,
				      req->op_mode) + (i * 4));
		}
		if (req->op_mode == SE_ELP_OP_MODE_ECC521) {
			for (i = nwords; i < nwords_521; i++) {
				se_elp_writel(se_dev, PKA1, 0x0,
					      reg_bank_offset(
					      TEGRA_SE_PKA1_ECC_XP_BANK,
					      TEGRA_SE_PKA1_ECC_XP_ID,
					      req->op_mode) + (i * 4));
				se_elp_writel(se_dev, PKA1, 0x0,
					      reg_bank_offset(
					      TEGRA_SE_PKA1_ECC_YP_BANK,
					      TEGRA_SE_PKA1_ECC_YP_ID,
					      req->op_mode) + (i * 4));
			}
		}
	}

	if (req->type == ECC_POINT_VER ||
	    req->type == ECC_SHAMIR_TRICK) {
		/* For shamir trick, curve_param_b is parameter k
		 * and k should be of size CTRL_BASE_RADIX
		 */
		B = req->curve_param_b;
		for (i = 0; i < nwords; i++)
			se_elp_writel(se_dev, PKA1, *B++, reg_bank_offset(
				      TEGRA_SE_PKA1_ECC_B_BANK,
				      TEGRA_SE_PKA1_ECC_B_ID,
				      req->op_mode) + (i * 4));

		if (req->type == ECC_SHAMIR_TRICK)
			len = num_words(req->op_mode);

		if (req->type == ECC_POINT_VER &&
		    req->op_mode == SE_ELP_OP_MODE_ECC521)
			len = nwords_521;

		for (i = nwords; i < len; i++)
			se_elp_writel(se_dev, PKA1, 0x0, reg_bank_offset(
				      TEGRA_SE_PKA1_ECC_B_BANK,
				      TEGRA_SE_PKA1_ECC_B_ID,
				      req->op_mode) + (i * 4));
	}

	if (req->type == ECC_POINT_ADD ||
	    req->type == ECC_SHAMIR_TRICK ||
	    req->type == ECC_POINT_DOUBLE) {
		QX = req->res_pt_x;
		QY = req->res_pt_y;
		for (i = 0; i < nwords; i++) {
			se_elp_writel(se_dev, PKA1, *QX++, reg_bank_offset(
				      TEGRA_SE_PKA1_ECC_XQ_BANK,
				      TEGRA_SE_PKA1_ECC_XQ_ID,
				      req->op_mode) + (i * 4));

			se_elp_writel(se_dev, PKA1, *QY++, reg_bank_offset(
				      TEGRA_SE_PKA1_ECC_YQ_BANK,
				      TEGRA_SE_PKA1_ECC_YQ_ID,
				      req->op_mode) + (i * 4));
		}
		if (req->op_mode == SE_ELP_OP_MODE_ECC521) {
			for (i = nwords; i < nwords_521; i++) {
				se_elp_writel(se_dev, PKA1, 0x0,
					      reg_bank_offset(
					      TEGRA_SE_PKA1_ECC_XQ_BANK,
					      TEGRA_SE_PKA1_ECC_XQ_ID,
					      req->op_mode) + (i * 4));

				se_elp_writel(se_dev, PKA1, 0x0,
					      reg_bank_offset(
					      TEGRA_SE_PKA1_ECC_YQ_BANK,
					      TEGRA_SE_PKA1_ECC_YQ_ID,
					      req->op_mode) + (i * 4));
			}
		}
	}

	if ((req->type == ECC_POINT_MUL && !se_dev->chipdata->use_key_slot) ||
	    req->type == ECC_SHAMIR_TRICK) {
		/* For shamir trick, key is parameter l
		 * and k for ECC_POINT_MUL and l for ECC_SHAMIR_TRICK
		 * should be of size CTRL_BASE_RADIX
		 */
		K = req->key;
		for (i = 0; i < nwords; i++)
			se_elp_writel(se_dev, PKA1, *K++, reg_bank_offset(
				      TEGRA_SE_PKA1_ECC_K_BANK,
				      TEGRA_SE_PKA1_ECC_K_ID,
				      req->op_mode) + (i * 4));

		for (i = nwords; i < num_words(req->op_mode); i++)
			se_elp_writel(se_dev, PKA1, 0x0, reg_bank_offset(
				      TEGRA_SE_PKA1_ECC_K_BANK,
				      TEGRA_SE_PKA1_ECC_K_ID,
				      req->op_mode) + (i * 4));
	}
}

static u32 pka1_ctrl_base(u32 mode)
{
	struct tegra_se_elp_dev *se_dev = elp_dev;
	u32 val, base_radix;

	val = num_words(mode) * 32;
	switch (val) {
	case PKA1_OP_SIZE_256:
		base_radix = TEGRA_SE_PKA1_CTRL_BASE_256;
		break;
	case PKA1_OP_SIZE_512:
		base_radix = TEGRA_SE_PKA1_CTRL_BASE_512;
		break;
	case PKA1_OP_SIZE_1024:
		base_radix = TEGRA_SE_PKA1_CTRL_BASE_1024;
		break;
	case PKA1_OP_SIZE_2048:
		base_radix = TEGRA_SE_PKA1_CTRL_BASE_2048;
		break;
	case PKA1_OP_SIZE_4096:
		base_radix = TEGRA_SE_PKA1_CTRL_BASE_4096;
		break;
	default:
		dev_warn(se_dev->dev, "Invalid size: using PKA1_OP_SIZE_256\n");
		base_radix = TEGRA_SE_PKA1_CTRL_BASE_256;
		break;
	}

	return base_radix;
}

static int tegra_se_pka1_done_verify(struct tegra_se_elp_dev *se_dev)
{
	u32 val, abnormal_val;

	/* Write Status Register to acknowledge interrupt */
	val = se_elp_readl(se_dev, PKA1, TEGRA_SE_PKA1_STATUS_OFFSET);
	se_elp_writel(se_dev, PKA1, val, TEGRA_SE_PKA1_STATUS_OFFSET);

	val = se_elp_readl(se_dev, PKA1, TEGRA_SE_PKA1_RETURN_CODE_OFFSET);

	abnormal_val = TEGRA_SE_PKA1_RETURN_CODE_STOP_REASON(
			TEGRA_SE_PKA1_RETURN_CODE_STOP_REASON_ABNORMAL);
	if (abnormal_val & val) {
		dev_err(se_dev->dev, "PKA1 Operation ended Abnormally\n");
		return -EFAULT;
	}

	val = se_elp_readl(se_dev, PKA1, TEGRA_SE_PKA1_CTRL_SE_INT_STAT_OFFSET);
	if (val & TEGRA_SE_PKA1_CTRL_SE_INT_STAT_ERR(ELP_TRUE)) {
		dev_err(se_dev->dev, "tegra_se_pka1_irq err: %x\n", val);
		return -EFAULT;
	}

	return 0;
}

static irqreturn_t tegra_se_pka1_irq(int irq, void *dev)
{
	struct tegra_se_elp_dev *se_dev = dev;

	if (!tegra_se_pka1_done_verify(se_dev))
		complete(&se_dev->complete);

	return IRQ_HANDLED;
}

static void tegra_se_set_pka1_op_ready(struct tegra_se_elp_dev *se_dev)
{
	u32 val;

	se_elp_writel(se_dev, PKA1, 0, TEGRA_SE_PKA1_FLAGS_OFFSET);
	se_elp_writel(se_dev, PKA1, 0, TEGRA_SE_PKA1_FSTACK_PTR_OFFSET);

	/* Clear PKA1 Error Capture bits */
	se_elp_writel(se_dev, PKA1, 0, TEGRA_SE_PKA1_CTRL_ERROR_CAPTURE_OFFSET);

	/* Clear any pending interrupts */
	val = se_elp_readl(se_dev, PKA1, TEGRA_SE_PKA1_STATUS_OFFSET);
	se_elp_writel(se_dev, PKA1, val, TEGRA_SE_PKA1_STATUS_OFFSET);

	/* Unmask PKA1 interrupt bit */
	se_elp_writel(se_dev, PKA1,
		TEGRA_SE_PKA1_CTRL_SE_INT_PKA1_MASK(ELP_ENABLE),
		TEGRA_SE_PKA1_CTRL_SE_INT_MASK_OFFSET);

	/* Enable PKA1 interrupt */
	se_elp_writel(se_dev, PKA1,
		      TEGRA_SE_PKA1_INT_ENABLE_IE_IRQ_EN(ELP_ENABLE),
		      TEGRA_SE_PKA1_INT_ENABLE_OFFSET);

	reinit_completion(&se_dev->complete);
}

static void tegra_se_program_pka1_rsa(struct tegra_se_pka1_rsa_context *ctx)
{
	u32 val;
	struct tegra_se_elp_dev *se_dev = ctx->se_dev;
	u32 partial_radix = 0;

	tegra_se_set_pka1_op_ready(se_dev);

	se_elp_writel(se_dev, PKA1,
		      TEGRA_SE_PKA1_RSA_MOD_EXP_PRG_ENTRY_VAL,
		      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);

	if (ctx->op_mode == SE_ELP_OP_MODE_RSA768 ||
		ctx->op_mode == SE_ELP_OP_MODE_RSA1536 ||
		ctx->op_mode == SE_ELP_OP_MODE_RSA3072)
		partial_radix = pka1_op_size[ctx->op_mode] / 32;

	val = TEGRA_SE_PKA1_CTRL_BASE_RADIX(pka1_ctrl_base(ctx->op_mode)) |
		TEGRA_SE_PKA1_CTRL_PARTIAL_RADIX(partial_radix);
	val |= TEGRA_SE_PKA1_CTRL_GO(TEGRA_SE_PKA1_CTRL_GO_START);

	se_elp_writel(se_dev, PKA1, val, TEGRA_SE_PKA1_CTRL_PKA_CONTROL_OFFSET);
}

static void tegra_se_program_pka1_ecc(struct tegra_se_pka1_ecc_request *req)
{
	u32 val;
	struct tegra_se_elp_dev *se_dev = req->se_dev;
	u32 partial_radix = 0;

	tegra_se_set_pka1_op_ready(se_dev);

	if (req->type == ECC_POINT_MUL) {
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_ECC_POINT_MUL_PRG_ENTRY_VAL,
			      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);
		/*clear F0 for binding val*/
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_FLAGS_FLAG_F0(ELP_DISABLE),
			      TEGRA_SE_PKA1_FLAGS_OFFSET);
	} else if (req->type == ECC_POINT_ADD) {
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_ECC_POINT_ADD_PRG_ENTRY_VAL,
			      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);
	} else if (req->type == ECC_POINT_DOUBLE) {
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_ECC_POINT_DOUBLE_PRG_ENTRY_VAL,
			      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);
	} else if (req->type == ECC_POINT_VER) {
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_ECC_ECPV_PRG_ENTRY_VAL,
			      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);
	} else {
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_ECC_SHAMIR_TRICK_PRG_ENTRY_VAL,
			      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);
	}

	if (req->op_mode == SE_ELP_OP_MODE_ECC521) {
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_FLAGS_FLAG_F1(ELP_ENABLE),
			      TEGRA_SE_PKA1_FLAGS_OFFSET);
	}

	if (req->op_mode != SE_ELP_OP_MODE_ECC256 &&
		req->op_mode != SE_ELP_OP_MODE_ECC512)
		partial_radix = pka1_op_size[req->op_mode] / 32;

	val =  TEGRA_SE_PKA1_CTRL_BASE_RADIX(pka1_ctrl_base(req->op_mode)) |
			TEGRA_SE_PKA1_CTRL_PARTIAL_RADIX(partial_radix) |
			TEGRA_SE_PKA1_CTRL_GO(TEGRA_SE_PKA1_CTRL_GO_START);

	if (req->op_mode == SE_ELP_OP_MODE_ECC521)
		val |= TEGRA_SE_PKA1_CTRL_M521_MODE(M521_MODE_VAL);
	else
		val |= TEGRA_SE_PKA1_CTRL_M521_MODE(NORMAL_MODE_VAL);

	se_elp_writel(se_dev, PKA1, val, TEGRA_SE_PKA1_CTRL_PKA_CONTROL_OFFSET);
}

static int tegra_se_check_pka1_op_done(struct tegra_se_elp_dev *se_dev)
{
	int ret;

	ret = wait_for_completion_timeout(&se_dev->complete, msecs_to_jiffies(
					  PKA1_TIMEOUT / 1000));
	if (ret == 0) {
		dev_err(se_dev->dev, "Interrupt timed out\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static void tegra_se_read_pka1_rsa_result(struct tegra_se_pka1_rsa_context *ctx,
					  u32 nwords)
{
	u32 i;
	u32 *RES = ctx->result;

	for (i = 0; i < nwords; i++) {
		*RES = se_elp_readl(ctx->se_dev, PKA1, reg_bank_offset(
				    TEGRA_SE_PKA1_RSA_RESULT_BANK,
				    TEGRA_SE_PKA1_RSA_RESULT_ID,
				    ctx->op_mode) + (i * 4));
		RES++;
	}
}

static void tegra_se_read_pka1_ecc_result(struct tegra_se_pka1_ecc_request *req)
{
	u32 val, i;
	u32 *QX = req->res_pt_x;
	u32 *QY = req->res_pt_y;
	struct tegra_se_elp_dev *se_dev = req->se_dev;

	if (req->type == ECC_POINT_VER) {
		val = se_elp_readl(se_dev, PKA1, TEGRA_SE_PKA1_FLAGS_OFFSET);
		if (val & TEGRA_SE_PKA1_FLAGS_FLAG_ZERO(ELP_ENABLE))
			req->pv_ok = true;
		else
			req->pv_ok = false;
	} else if (req->type == ECC_POINT_DOUBLE) {
		for (i = 0; i < req->size / 4; i++) {
			*QX = se_elp_readl(se_dev, PKA1, reg_bank_offset(
					   TEGRA_SE_PKA1_ECC_XP_BANK,
					   TEGRA_SE_PKA1_ECC_XP_ID,
					   req->op_mode) + (i * 4));
			QX++;
		}
		for (i = 0; i < req->size / 4; i++) {
			*QY = se_elp_readl(se_dev, PKA1, reg_bank_offset(
					   TEGRA_SE_PKA1_ECC_YP_BANK,
					   TEGRA_SE_PKA1_ECC_YP_ID,
					   req->op_mode) + (i * 4));
			QY++;
		}
	} else {
		for (i = 0; i < req->size / 4; i++) {
			*QX = se_elp_readl(se_dev, PKA1, reg_bank_offset(
					   TEGRA_SE_PKA1_ECC_XQ_BANK,
					   TEGRA_SE_PKA1_ECC_XQ_ID,
					   req->op_mode) + (i * 4));
			QX++;
		}
		for (i = 0; i < req->size / 4; i++) {
			*QY = se_elp_readl(se_dev, PKA1,
					   reg_bank_offset(
					   TEGRA_SE_PKA1_ECC_YQ_BANK,
					   TEGRA_SE_PKA1_ECC_YQ_ID,
					   req->op_mode) + (i * 4));
			QY++;
		}
	}
}

enum tegra_se_pka1_keyslot_field {
	EXPONENT,
	MOD_RSA,
	M_RSA,
	R2_RSA,
	PARAM_A,
	PARAM_B,
	MOD_ECC,
	PARAM_N,
	XP,
	YP,
	XQ,
	YQ,
	KEY,
	M_ECC,
	R2_ECC,
};

static void tegra_se_pka1_set_key_param(u32 *param, u32 key_words, u32 slot_num,
					int op, u32 mode, u32 type)
{
	struct tegra_se_elp_dev *se_dev = elp_dev;
	int i;
	int len = 0;
	int nwords_521 = pka1_op_size[SE_ELP_OP_MODE_ECC521] / 32;

	for (i = 0; i < key_words; i++) {
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_KEYSLOT_ADDR_FIELD(op) |
			      TEGRA_SE_PKA1_KEYSLOT_ADDR_WORD(i),
			      TEGRA_SE_PKA1_KEYSLOT_ADDR_OFFSET(slot_num));
		se_elp_writel(se_dev, PKA1, *param++,
			      TEGRA_SE_PKA1_KEYSLOT_DATA_OFFSET(slot_num));
	}

	if (type == ECC_INVALID)
		return;

	if (mode == SE_ELP_OP_MODE_ECC521)
		len = nwords_521;
	if ((op == KEY) || (op == PARAM_B && type == ECC_SHAMIR_TRICK))
		len = num_words(mode);

	for (i = key_words; i < len; i++) {
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_KEYSLOT_ADDR_FIELD(op) |
			      TEGRA_SE_PKA1_KEYSLOT_ADDR_WORD(i),
			      TEGRA_SE_PKA1_KEYSLOT_ADDR_OFFSET(slot_num));
		se_elp_writel(se_dev, PKA1, 0x0,
			      TEGRA_SE_PKA1_KEYSLOT_DATA_OFFSET(slot_num));
	}
}

static void tegra_se_set_pka1_rsa_key(struct tegra_se_pka1_rsa_context *ctx)
{
	u32 key_words = ctx->keylen / WORD_SIZE_BYTES;
	u32 slot_num = ctx->slot->slot_num;
	u32 *MOD = ctx->modulus;
	u32 *M = ctx->m;
	u32 *R2 = ctx->r2;
	u32 *EXP = ctx->exponent;

	tegra_se_pka1_set_key_param(EXP, key_words, slot_num, EXPONENT,
				    ctx->op_mode, ECC_INVALID);
	tegra_se_pka1_set_key_param(MOD, key_words, slot_num, MOD_RSA,
				    ctx->op_mode, ECC_INVALID);
	tegra_se_pka1_set_key_param(M, key_words, slot_num, M_RSA,
				    ctx->op_mode, ECC_INVALID);
	tegra_se_pka1_set_key_param(R2, key_words, slot_num, R2_RSA,
				    ctx->op_mode, ECC_INVALID);
}

static void tegra_se_set_pka1_ecc_key(struct tegra_se_pka1_ecc_request *req)
{
	u32 key_words = req->size / WORD_SIZE_BYTES;
	u32 slot_num = req->slot->slot_num;
	u32 *MOD = req->modulus;
	u32 *M = req->m;
	u32 *R2 = req->r2;
	u32 *A = req->curve_param_a;
	u32 *B = req->curve_param_b;
	u32 *PX = req->base_pt_x;
	u32 *PY = req->base_pt_y;
	u32 *K = req->key;
	u32 *QX = req->res_pt_x;
	u32 *QY = req->res_pt_y;

	tegra_se_pka1_set_key_param(A, key_words, slot_num, PARAM_A,
				    req->op_mode, req->type);
	tegra_se_pka1_set_key_param(MOD, key_words, slot_num, MOD_ECC,
				    req->op_mode, req->type);

	if (req->type == ECC_POINT_VER ||
	    req->type == ECC_SHAMIR_TRICK)
		tegra_se_pka1_set_key_param(B, key_words, slot_num, PARAM_B,
					    req->op_mode, req->type);

	if (req->type != ECC_POINT_DOUBLE) {
		tegra_se_pka1_set_key_param(PX, key_words, slot_num, XP,
					    req->op_mode, req->type);
		tegra_se_pka1_set_key_param(PY, key_words, slot_num, YP,
					    req->op_mode, req->type);
	}

	if (req->type == ECC_POINT_MUL ||
	    req->type == ECC_SHAMIR_TRICK)
		tegra_se_pka1_set_key_param(K, key_words, slot_num, KEY,
					    req->op_mode, req->type);

	if (req->op_mode != SE_ELP_OP_MODE_ECC521) {
		tegra_se_pka1_set_key_param(M, key_words, slot_num, M_ECC,
					    req->op_mode, req->type);
		tegra_se_pka1_set_key_param(R2, key_words, slot_num, R2_ECC,
					    req->op_mode, req->type);
	}

	if (req->type == ECC_POINT_ADD ||
	    req->type == ECC_SHAMIR_TRICK ||
	    req->type == ECC_POINT_DOUBLE) {
		tegra_se_pka1_set_key_param(QX, key_words, slot_num, XQ,
					    req->op_mode, req->type);
		tegra_se_pka1_set_key_param(QY, key_words, slot_num, YQ,
					    req->op_mode, req->type);
	}
}

static int tegra_se_pka1_precomp(struct tegra_se_pka1_rsa_context *ctx,
				 struct tegra_se_pka1_ecc_request *ecc_req,
				 struct tegra_se_pka1_mod_request *mod_req,
				 u32 op)
{
	int ret, i, nwords, op_mode;
	u32 *MOD, *M, *R2;
	struct tegra_se_elp_dev *se_dev;

	if (ctx) {
		MOD = ctx->modulus;
		M = ctx->m;
		R2 = ctx->r2;
		nwords = ctx->keylen / WORD_SIZE_BYTES;
		op_mode = ctx->op_mode;
		se_dev = ctx->se_dev;
	} else if (ecc_req) {
		MOD = ecc_req->modulus;
		M = ecc_req->m;
		R2 = ecc_req->r2;
		nwords = ecc_req->size / WORD_SIZE_BYTES;
		op_mode = ecc_req->op_mode;
		se_dev = ecc_req->se_dev;
	} else {
		MOD = mod_req->modulus;
		M = mod_req->m;
		R2 = mod_req->r2;
		nwords = mod_req->size / WORD_SIZE_BYTES;
		op_mode = mod_req->op_mode;
		se_dev = mod_req->se_dev;
	}

	if (op_mode == SE_ELP_OP_MODE_ECC521)
		return 0;

	tegra_se_set_pka1_op_ready(se_dev);

	if (op == PRECOMP_RINV) {
		for (i = 0; i < nwords; i++) {
			se_elp_writel(se_dev, PKA1, *MOD++, reg_bank_offset(
				      TEGRA_SE_PKA1_MOD_BANK,
				      TEGRA_SE_PKA1_MOD_ID, op_mode) + (i * 4));
		}
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_RSA_RINV_PRG_ENTRY_VAL,
			      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);
	} else if (op == PRECOMP_M) {
		se_elp_writel(se_dev, PKA1, TEGRA_SE_PKA1_RSA_M_PRG_ENTRY_VAL,
			      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);
	} else {
		se_elp_writel(se_dev, PKA1, TEGRA_SE_PKA1_RSA_R2_PRG_ENTRY_VAL,
			      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);
	}

	se_elp_writel(se_dev, PKA1,
		      TEGRA_SE_PKA1_CTRL_BASE_RADIX(pka1_ctrl_base(op_mode)) |
		      TEGRA_SE_PKA1_CTRL_PARTIAL_RADIX(nwords) |
		      TEGRA_SE_PKA1_CTRL_GO(TEGRA_SE_PKA1_CTRL_GO_START),
		      TEGRA_SE_PKA1_CTRL_OFFSET);

	ret = tegra_se_check_pka1_op_done(se_dev);
	if (ret)
		return ret;

	if (op == PRECOMP_M) {
		for (i = 0; i < nwords; i++) {
			*M = se_elp_readl(se_dev, PKA1, reg_bank_offset(
					  TEGRA_SE_PKA1_M_BANK,
					  TEGRA_SE_PKA1_M_ID,
					  op_mode) + (i * 4));
			M++;
		}
	} else if (op == PRECOMP_R2) {
		for (i = 0; i < nwords; i++) {
			*R2 = se_elp_readl(se_dev, PKA1, reg_bank_offset(
					   TEGRA_SE_PKA1_R2_BANK,
					   TEGRA_SE_PKA1_R2_ID,
					   op_mode) + (i * 4));
			R2++;
		}
	}

	return ret;
}

static int tegra_se_pka1_ecc_do(struct tegra_se_pka1_ecc_request *req)
{
	int ret;
	u32 val, slot_num;
	struct tegra_se_pka1_slot *pslot;
	struct tegra_se_elp_dev *se_dev = req->se_dev;
	u32 i = 0;

	if (se_dev->chipdata->use_key_slot) {
		pslot = tegra_se_pka1_alloc_key_slot();
		if (!pslot) {
			dev_err(se_dev->dev, "no free key slot\n");
			return -ENOMEM;
		}
		req->slot = pslot;
		slot_num = req->slot->slot_num;

		tegra_se_set_pka1_ecc_key(req);

		val = TEGRA_SE_PKA1_CTRL_CONTROL_KEYSLOT(slot_num) |
			TEGRA_SE_PKA1_CTRL_CONTROL_LOAD_KEY(ELP_ENABLE);

		se_elp_writel(se_dev, PKA1, val,
			      TEGRA_SE_PKA1_CTRL_CONTROL_OFFSET);
		/* poll SE_STATUS */
		do {
			if (i > PKA1_TIMEOUT) {
				dev_err(se_dev->dev,
					"PKA1 Load Key timed out\n");
				return -EINVAL;
			}
			udelay(1);
			val = se_elp_readl(se_dev, PKA1,
					   TEGRA_SE_PKA1_CTRL_STATUS_OFFSET);
			i++;
		} while (val & TEGRA_SE_PKA1_CTRL_SE_STATUS(SE_STATUS_BUSY));
	}

	tegra_se_pka1_ecc_fill_input(req);

	tegra_se_program_pka1_ecc(req);

	ret = tegra_se_check_pka1_op_done(se_dev);
	if (ret)
		return ret;

	tegra_se_read_pka1_ecc_result(req);

	if (se_dev->chipdata->use_key_slot)
		tegra_se_pka1_free_key_slot(req->slot);

	return ret;
}

static int tegra_se_pka1_ecc_init(struct tegra_se_pka1_ecc_request *req)
{
	struct tegra_se_elp_dev *se_dev = elp_dev;
	int len = req->size;

	req->se_dev = se_dev;

	if (req->op_mode == SE_ELP_OP_MODE_ECC521) {
		req->se_dev = se_dev;
		return 0;
	}

	req->m = devm_kzalloc(se_dev->dev, len, GFP_KERNEL);
	if (!req->m)
		return -ENOMEM;

	req->r2 = devm_kzalloc(se_dev->dev, len, GFP_KERNEL);
	if (!req->r2) {
		devm_kfree(se_dev->dev, req->m);
		return -ENOMEM;
	}

	return 0;
}

static void tegra_se_pka1_ecc_exit(struct tegra_se_pka1_ecc_request *req)
{
	struct tegra_se_elp_dev *se_dev = req->se_dev;

	if (req->op_mode == SE_ELP_OP_MODE_ECC521)
		return;

	devm_kfree(se_dev->dev, req->m);
	devm_kfree(se_dev->dev, req->r2);
}

static int tegra_se_mod_op_mode(int nbytes)
{
	int mode;

	switch (nbytes) {
	case 20:
		mode = SE_ELP_OP_MODE_MOD160;
		break;
	case 24:
		mode = SE_ELP_OP_MODE_MOD192;
		break;
	case 28:
		mode = SE_ELP_OP_MODE_MOD224;
		break;
	case 32:
		mode = SE_ELP_OP_MODE_MOD256;
		break;
	case 48:
		mode = SE_ELP_OP_MODE_MOD384;
		break;
	case 64:
		mode = SE_ELP_OP_MODE_MOD512;
		break;
	default:
		mode = -EINVAL;
		break;
	}
	return mode;
}

static void tegra_se_pka1_mod_fill_input(struct tegra_se_pka1_mod_request *req)
{
	struct tegra_se_elp_dev *se_dev = req->se_dev;
	u32 *PX, *PY;
	u32 *MOD, *M, *R2;
	u32 i = 0;

	MOD = req->modulus;
	M = req->m;
	R2 = req->r2;

	if (req->type == MOD_MULT ||
	    req->type == MOD_ADD ||
	    req->type == MOD_SUB) {
		PX = req->base_pt_x;
		PY = req->base_pt_y;
		for (i = 0; i < req->size/4; i++) {
			se_elp_writel(se_dev, PKA1, *PX++,
				      reg_bank_offset(BANK_A,
						      0,
						      req->op_mode) + (i*4));

			se_elp_writel(se_dev, PKA1, *PY++,
				      reg_bank_offset(BANK_B,
						      0,
						      req->op_mode) + (i*4));
		}

	}
	if (req->type == MOD_REDUCE) {
		PX = req->base_pt_x;
		for (i = 0; i < req->size/4; i++) {
			se_elp_writel(se_dev, PKA1, *PX++,
				      reg_bank_offset(BANK_C,
						      0, req->op_mode) + (i*4));
		}

	}
	if (req->type == MOD_DIV) {
		PX = req->base_pt_x;
		PY = req->base_pt_y;
		for (i = 0; i < req->size/4; i++) {
			se_elp_writel(se_dev, PKA1, *PX++,
				      reg_bank_offset(BANK_A,
						      0,
						      req->op_mode) + (i*4));

			se_elp_writel(se_dev, PKA1, *PY++,
				      reg_bank_offset(BANK_C,
						      0,
						      req->op_mode) + (i*4));
		}
	}
	if (req->type == MOD_INV) {
		PX = req->base_pt_x;
		for (i = 0; i < req->size/4; i++) {
			se_elp_writel(se_dev, PKA1, *PX++,
				      reg_bank_offset(BANK_A,
						      0, req->op_mode) + (i*4));
		}
	}
}

static u32 pka_ctrl_partial_radix(u32 mode)
{
	u32 base_words;
	u32 operand_words;
	u32 partial_radix = 0;

	base_words = num_words(mode);
	operand_words = pka1_op_size[mode] / 32;
	if (base_words)
		partial_radix = operand_words % base_words;

	return partial_radix;
}

static void tegra_se_program_pka1_mod(struct tegra_se_pka1_mod_request *req)
{
	struct tegra_se_elp_dev *se_dev = req->se_dev;
	u32 om = req->op_mode;
	u32 val;

	if (req->type == MOD_MULT) {
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_ENTRY_MODMULT,
			      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);
	} else if (req->type == MOD_ADD) {
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_ENTRY_MODADD,
			      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);
	} else if (req->type == MOD_SUB) {
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_ENTRY_MODSUB,
			      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);
	} else if (req->type == MOD_REDUCE) {
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_ENTRY_REDUCE,
			      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);
	} else if (req->type == MOD_DIV) {
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_ENTRY_MODDIV,
			      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);
	} else if (req->type == MOD_INV) {
		se_elp_writel(se_dev, PKA1,
			      TEGRA_SE_PKA1_ENTRY_MODINV,
			      TEGRA_SE_PKA1_PRG_ENTRY_OFFSET);
	}

	se_elp_writel(se_dev, PKA1,
		      TEGRA_SE_PKA1_INT_ENABLE_IE_IRQ_EN(ELP_ENABLE),
		      TEGRA_SE_PKA1_INT_ENABLE_OFFSET);

	val = TEGRA_SE_PKA1_CTRL_BASE_RADIX(pka1_ctrl_base(req->op_mode)) |
		TEGRA_SE_PKA1_CTRL_PARTIAL_RADIX(pka_ctrl_partial_radix(om)) |
		TEGRA_SE_PKA1_CTRL_GO(TEGRA_SE_PKA1_CTRL_GO_START);

	se_elp_writel(se_dev, PKA1, val, TEGRA_SE_PKA1_CTRL_OFFSET);
}

static void tegra_se_read_pka1_mod_result(struct tegra_se_pka1_mod_request *req)
{
	struct tegra_se_elp_dev *se_dev = req->se_dev;
	u32 *RES = req->result;
	u32 val, i;

	if (req->type == MOD_MULT ||
	    req->type == MOD_ADD ||
	    req->type == MOD_SUB ||
	    req->type == MOD_REDUCE) {
		for (i = 0; i < req->size/4; i++) {
			val = se_elp_readl(se_dev, PKA1,
					reg_bank_offset(BANK_A,
							0,
							req->op_mode) + (i*4));
			*RES = le32_to_cpu(val);
			RES++;
		}
	} else if (req->type == MOD_DIV ||
		   req->type == MOD_INV) {
		for (i = 0; i < req->size/4; i++) {
			val = se_elp_readl(se_dev, PKA1,
					reg_bank_offset(BANK_C,
							0,
							req->op_mode) + (i*4));
			*RES = le32_to_cpu(val);
			RES++;
		}
	}
}

static int tegra_se_pka1_mod_do(struct tegra_se_pka1_mod_request *req)
{
	struct tegra_se_elp_dev *se_dev = req->se_dev;
	int ret;

	tegra_se_pka1_mod_fill_input(req);

	tegra_se_program_pka1_mod(req);

	ret = tegra_se_check_pka1_op_done(se_dev);
	if (ret)
		return ret;

	tegra_se_read_pka1_mod_result(req);

	return ret;
}

static int tegra_se_pka1_mod_init(struct tegra_se_pka1_mod_request *req)
{
	struct tegra_se_elp_dev *se_dev = elp_dev;
	int len = req->size;

	req->se_dev = se_dev;

	req->m = devm_kzalloc(se_dev->dev, len, GFP_KERNEL);
	if (!req->m)
		return -ENOMEM;

	req->r2 = devm_kzalloc(se_dev->dev, len, GFP_KERNEL);
	if (!req->r2) {
		devm_kfree(se_dev->dev, req->m);
		return -ENOMEM;
	}

	return 0;
}

static void tegra_se_pka1_mod_exit(struct tegra_se_pka1_mod_request *req)
{
	struct tegra_se_elp_dev *se_dev = req->se_dev;

	devm_kfree(se_dev->dev, req->m);
	devm_kfree(se_dev->dev, req->r2);
}

static void tegra_se_release_rng1_mutex(struct tegra_se_elp_dev *se_dev)
{
	se_elp_writel(se_dev, RNG1, 0x01, TEGRA_SE_RNG1_MUTEX_OFFSET);
}

static u32 tegra_se_acquire_rng1_mutex(struct tegra_se_elp_dev *se_dev)
{
	u32 val, i = 0;

	/* Acquire rng mutex */
	do {
		if (i > RNG1_TIMEOUT) {
			dev_err(se_dev->dev, "Acquire RNG1 Mutex timed out\n");
			return -EINVAL;
		}
		udelay(1);
		val = se_elp_readl(se_dev, RNG1, TEGRA_SE_RNG1_MUTEX_OFFSET);
		i++;
	} while (val != 0x01);

	/* One unit is 256 SE Cycles */
	se_elp_writel(se_dev, RNG1, TEGRA_SE_MUTEX_WDT_UNITS,
		      TEGRA_SE_RNG1_MUTEX_WATCHDOG_OFFSET);
	se_elp_writel(se_dev, RNG1, TEGRA_SE_RNG1_MUTEX_TIMEOUT_ACTION,
		      TEGRA_SE_RNG1_MUTEX_TIMEOUT_ACTION_OFFSET);

	return 0;
}

static u32 tegra_se_check_rng1_status(struct tegra_se_elp_dev *se_dev)
{
	static bool rng1_first = true;
	bool secure_mode;
	u32 val, i = 0;

	/*Wait until RNG is Idle */
	do {
		if (i > RNG1_TIMEOUT) {
			dev_err(se_dev->dev, "RNG1 Idle timed out\n");
			return -EINVAL;
		}
		udelay(1);
		val = se_elp_readl(se_dev, RNG1,
				   TEGRA_SE_RNG1_STATUS_OFFSET);
		i++;
	} while (val & TEGRA_SE_RNG1_STATUS_BUSY(ELP_TRUE));

	if (rng1_first) {
		val = se_elp_readl(se_dev, RNG1, TEGRA_SE_RNG1_STATUS_OFFSET);
		if (val & TEGRA_SE_RNG1_STATUS_SECURE(STATUS_SECURE))
			secure_mode = true;
		else
			secure_mode = false;

		/*Check health test is ok*/
		val = se_elp_readl(se_dev, RNG1,
				   TEGRA_SE_RNG1_ISTATUS_OFFSET);
		if (secure_mode)
			val &= TEGRA_SE_RNG1_ISTATUS_DONE(ISTATUS_ACTIVE);
		else
			val &= TEGRA_SE_RNG1_ISTATUS_DONE(ISTATUS_ACTIVE) |
			TEGRA_SE_RNG1_ISTATUS_NOISE_RDY(ISTATUS_ACTIVE);
		if (!val) {
			dev_err(se_dev->dev,
				"Wrong Startup value in RNG1_ISTATUS Reg\n");
			return -EINVAL;
		}
		rng1_first = false;
	}

	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_RNG1_ISTATUS_OFFSET);
	se_elp_writel(se_dev, RNG1, val, TEGRA_SE_RNG1_ISTATUS_OFFSET);

	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_RNG1_ISTATUS_OFFSET);
	if (val) {
		dev_err(se_dev->dev, "RNG1_ISTATUS Reg is not cleared\n");
		return -EINVAL;
	}

	return 0;
}

static void tegra_se_set_rng1_mode(unsigned int mode)
{
	struct tegra_se_elp_dev *se_dev = elp_dev;

	/*no additional input mode*/
	se_elp_writel(se_dev, RNG1, mode, TEGRA_SE_RNG1_SE_MODE_OFFSET);
}

static void tegra_se_set_rng1_smode(bool secure, bool nonce)
{
	u32 val = 0;
	struct tegra_se_elp_dev *se_dev = elp_dev;

	if (secure)
		val = TEGRA_SE_RNG1_SE_SMODE_SECURE(SMODE_SECURE);
	if (nonce)
		val = TEGRA_SE_RNG1_SE_SMODE_NONCE(ELP_ENABLE);

	/* need to write twice, switch secure/promiscuous
	 * mode would reset other bits
	 */
	se_elp_writel(se_dev, RNG1, val, TEGRA_SE_RNG1_SE_SMODE_OFFSET);
	se_elp_writel(se_dev, RNG1, val, TEGRA_SE_RNG1_SE_SMODE_OFFSET);
}

static int tegra_se_execute_rng1_ctrl_cmd(unsigned int cmd)
{
	u32 val, stat, i = 0;
	bool secure_mode;
	struct tegra_se_elp_dev *se_dev = elp_dev;

	se_elp_writel(se_dev, RNG1, 0xFFFFFFFF, TEGRA_SE_RNG1_INT_EN_OFFSET);
	se_elp_writel(se_dev, RNG1, 0xFFFFFFFF, TEGRA_SE_RNG1_IE_OFFSET);

	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_RNG1_STATUS_OFFSET);
	secure_mode = !!(val & TEGRA_SE_RNG1_STATUS_SECURE(STATUS_SECURE));

	switch (cmd) {
	case RNG1_CMD_GEN_NONCE:
	case RNG1_CMD_CREATE_STATE:
	case RNG1_CMD_RENEW_STATE:
	case RNG1_CMD_REFRESH_ADDIN:
	case RNG1_CMD_GEN_RANDOM:
	case RNG1_CMD_ADVANCE_STATE:
		stat = TEGRA_SE_RNG1_ISTATUS_DONE(ISTATUS_ACTIVE);
		break;
	case RNG1_CMD_GEN_NOISE:
		if (secure_mode)
			stat = TEGRA_SE_RNG1_ISTATUS_DONE(ISTATUS_ACTIVE);
		else
			stat = TEGRA_SE_RNG1_ISTATUS_DONE(ISTATUS_ACTIVE) |
				TEGRA_SE_RNG1_ISTATUS_NOISE_RDY(ISTATUS_ACTIVE);
		break;
	case RNG1_CMD_KAT:
		stat = TEGRA_SE_RNG1_ISTATUS_KAT_COMPLETED(ISTATUS_ACTIVE);
		break;
	case RNG1_CMD_ZEROIZE:
		stat = TEGRA_SE_RNG1_ISTATUS_ZEROIZED(ISTATUS_ACTIVE);
		break;
	case RNG1_CMD_NOP:
	default:
		dev_err(se_dev->dev,
			"Cmd %d has nothing to do (or) invalid\n", cmd);
		dev_err(se_dev->dev, "RNG1 cmd failure: %s\n", rng1_cmd[cmd]);
		return -EINVAL;
	}
	se_elp_writel(se_dev, RNG1, cmd, TEGRA_SE_RNG1_CTRL_OFFSET);

	do {
		if (i > RNG1_TIMEOUT) {
			dev_err(se_dev->dev, "\nRNG1 ISTAT poll timed out\n");
			return -EINVAL;
		}
		udelay(1);
		val = se_elp_readl(se_dev, RNG1, TEGRA_SE_RNG1_ISTATUS_OFFSET);
		i++;
	} while (val != stat);

	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_RNG1_IE_OFFSET);
	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_RNG1_INT_EN_OFFSET);

	i = 0;
	do {
		if (i > RNG1_TIMEOUT) {
			dev_err(se_dev->dev, "RNG1 INT status timed out\n");
			return -EINVAL;
		}
		udelay(1);
		val = se_elp_readl(se_dev, RNG1,
				   TEGRA_SE_RNG1_INT_STATUS_OFFSET);
		i++;
	} while (!(val & TEGRA_SE_RNG1_INT_STATUS_EIP0(STATUS_ACTIVE)));

	se_elp_writel(se_dev, RNG1, stat, TEGRA_SE_RNG1_ISTATUS_OFFSET);

	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_RNG1_INT_STATUS_OFFSET);
	if (val & TEGRA_SE_RNG1_INT_STATUS_EIP0(STATUS_ACTIVE)) {
		dev_err(se_dev->dev,
			"RNG1 intr not cleared (0x%x) after cmd %d execution\n",
			val, cmd);
		dev_err(se_dev->dev, "RNG1 Command Failure: %s\n",
			((cmd == RNG1_CMD_ZEROIZE) ? rng1_cmd[9] :
			rng1_cmd[cmd]));
		return -EINVAL;
	}

	return 0;
}

static int tegra_se_check_rng1_result(struct tegra_se_rng1_request *req)
{
	u32 i, val;
	struct tegra_se_elp_dev *se_dev = elp_dev;

	for (i = 0; i < 4; i++) {
		val = se_elp_readl(se_dev, RNG1,
				   TEGRA_SE_RNG1_RAND0_OFFSET + i * 4);
		if (!val) {
			dev_err(se_dev->dev, "No random data from RAND\n");
			return -EINVAL;
		}
		se_dev->rdata[i] = val;
	}

	return 0;
}

static int tegra_se_check_rng1_alarms(void)
{
	u32 val;
	struct tegra_se_elp_dev *se_dev = elp_dev;

	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_RNG1_ALARMS_OFFSET);
	if (val) {
		dev_err(se_dev->dev, "RNG1 Alarms not cleared (0x%x)\n", val);
		return -EINVAL;
	}

	return 0;
}

static void tegra_se_rng1_feed_npa_data(void)
{
	int i;
	u32 data, r;
	struct tegra_se_elp_dev *se_dev = elp_dev;

	for (i = 0; i < 16; i++) {
		get_random_bytes(&r, sizeof(int));
		data = r & 0xffffffff;
		se_elp_writel(se_dev, RNG1, data,
			      TEGRA_SE_RNG1_NPA_DATA0_OFFSET + i * 4);
	}
}

static int tegra_se_rng1_do(struct tegra_se_elp_dev *se_dev,
			       struct tegra_se_rng1_request *req)
{
	u32 *rand_num;
	int i, j, k, ret;
	bool adv_state = false;

	rand_num = devm_kzalloc(se_dev->dev,
				(sizeof(*rand_num) * (RAND_256 / 4)),
				GFP_KERNEL);
	if (!rand_num)
		return -ENOMEM;

	tegra_se_set_rng1_smode(true, false);
	tegra_se_set_rng1_mode(RNG1_MODE_SEC_ALG);
	/* Generate Noise */
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_NOISE);
	if (ret)
		return ret;
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_CREATE_STATE);
	if (ret)
		return ret;

	for (i = 0; i < req->size/RAND_128; i++) {
		if (i && req->adv_state_on && (i % ADV_STATE_FREQ == 0)) {
			ret = tegra_se_execute_rng1_ctrl_cmd
						(RNG1_CMD_ADVANCE_STATE);
			if (ret)
				return ret;
			adv_state = true;
		}
		ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_RANDOM);
		if (ret)
			return ret;

		ret = tegra_se_check_rng1_result(req);
		if (ret) {
			dev_err(se_dev->dev, "RNG1 Failed for Sub-Step 1\n");
			return ret;
		}

		for (k = (4 * i), j = 0; k < 4 * (i + 1); k++, j++)
			rand_num[k] = se_dev->rdata[j];

		if (adv_state) {
			ret = tegra_se_execute_rng1_ctrl_cmd
						(RNG1_CMD_ADVANCE_STATE);
			if (ret)
				return ret;
			if ((i+1) < req->size/RAND_128) {
				ret = tegra_se_execute_rng1_ctrl_cmd
							(RNG1_CMD_GEN_NOISE);
				if (ret)
					return ret;
				ret = tegra_se_execute_rng1_ctrl_cmd
							(RNG1_CMD_RENEW_STATE);
				if (ret)
					return ret;
			}
		}
	}

	for (k = 0; k < (RAND_256 / 4); k++)
		req->rdata[k] = rand_num[k];

	tegra_se_check_rng1_alarms();

	if (!req->test_full_cmd_flow)
		return ret;

	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_ZEROIZE);
	if (ret)
		return ret;

	tegra_se_set_rng1_smode(true, false);
	tegra_se_set_rng1_mode(RNG1_MODE_ADDIN_PRESENT);

	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_NOISE);
	if (ret)
		return ret;
	tegra_se_rng1_feed_npa_data();
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_CREATE_STATE);
	if (ret)
		return ret;
	tegra_se_rng1_feed_npa_data();
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_REFRESH_ADDIN);
	if (ret)
		return ret;

	for (i = 0; i < req->size/RAND_128; i++) {
		if (i && req->adv_state_on && (i % ADV_STATE_FREQ == 0)) {
			ret = tegra_se_execute_rng1_ctrl_cmd
						(RNG1_CMD_ADVANCE_STATE);
			if (ret)
				return ret;
			tegra_se_rng1_feed_npa_data();
			ret = tegra_se_execute_rng1_ctrl_cmd
						(RNG1_CMD_REFRESH_ADDIN);
			if (ret)
				return ret;
			adv_state = true;
		}
		ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_RANDOM);
		if (ret)
			return ret;

		ret = tegra_se_check_rng1_result(req);
		if (ret) {
			dev_err(se_dev->dev, "RNG1 Failed for Sub-Step 2\n");
			return ret;
		}

		for (k = (4 * i), j = 0; k < 4 * (i + 1); k++, j++)
			rand_num[k] = se_dev->rdata[j];

		if (adv_state) {
			ret = tegra_se_execute_rng1_ctrl_cmd
						(RNG1_CMD_ADVANCE_STATE);
			if (ret)
				return ret;
			if ((i + 1) < req->size / RAND_128) {
				ret = tegra_se_execute_rng1_ctrl_cmd
							(RNG1_CMD_GEN_NOISE);
				if (ret)
					return ret;
				tegra_se_rng1_feed_npa_data();
				ret = tegra_se_execute_rng1_ctrl_cmd
							(RNG1_CMD_RENEW_STATE);
				if (ret)
					return ret;
				tegra_se_rng1_feed_npa_data();
				ret = tegra_se_execute_rng1_ctrl_cmd
						(RNG1_CMD_REFRESH_ADDIN);
				if (ret)
					return ret;
			}
		}
	}

	for (k = 0; k < (RAND_256 / 4); k++)
		req->rdata1[k] = rand_num[k];

	tegra_se_check_rng1_alarms();

	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_ZEROIZE);
	if (ret)
		return ret;

	tegra_se_set_rng1_smode(true, true);
	tegra_se_set_rng1_mode(RNG1_MODE_ADDIN_PRESENT);

	tegra_se_rng1_feed_npa_data();
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_NONCE);
	if (ret)
		return ret;
	tegra_se_rng1_feed_npa_data();
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_NONCE);
	if (ret)
		return ret;
	tegra_se_rng1_feed_npa_data();
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_CREATE_STATE);
	if (ret)
		return ret;
	tegra_se_rng1_feed_npa_data();
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_REFRESH_ADDIN);
	if (ret)
		return ret;

	for (i = 0; i < req->size / RAND_128; i++) {
		if (i && req->adv_state_on && (i % ADV_STATE_FREQ == 0)) {
			ret = tegra_se_execute_rng1_ctrl_cmd
					(RNG1_CMD_ADVANCE_STATE);
			if (ret)
				return ret;
			tegra_se_rng1_feed_npa_data();
			ret =
			tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_REFRESH_ADDIN);
			if (ret)
				return ret;
			adv_state = true;
		}
		ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_RANDOM);
		if (ret)
			return ret;

		ret = tegra_se_check_rng1_result(req);
		if (ret) {
			dev_err(se_dev->dev, "RNG1 Failed for Sub-Step 3\n");
			return ret;
		}

		for (k = (4 * i), j = 0; k < 4 * (i + 1); k++, j++)
			rand_num[k] = se_dev->rdata[j];

		if (adv_state) {
			ret = tegra_se_execute_rng1_ctrl_cmd
						(RNG1_CMD_ADVANCE_STATE);
			if (ret)
				return ret;
			if ((i + 1) < req->size / RAND_128) {
				ret = tegra_se_execute_rng1_ctrl_cmd
							(RNG1_CMD_GEN_NONCE);
				if (ret)
					return ret;
				tegra_se_rng1_feed_npa_data();
				ret = tegra_se_execute_rng1_ctrl_cmd
							(RNG1_CMD_RENEW_STATE);
				if (ret)
					return ret;
				tegra_se_rng1_feed_npa_data();
				ret = tegra_se_execute_rng1_ctrl_cmd
						(RNG1_CMD_REFRESH_ADDIN);
				if (ret)
					return ret;
			}
		}
	}

	for (k = 0; k < (RAND_256 / 4); k++)
		req->rdata2[k] = rand_num[k];

	tegra_se_check_rng1_alarms();

	return tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_ZEROIZE);
}

int tegra_se_rng1_op(struct tegra_se_rng1_request *req)
{
	struct tegra_se_elp_dev *se_dev = elp_dev;
	int ret;

	clk_prepare_enable(se_dev->c);
	ret = tegra_se_acquire_rng1_mutex(se_dev);
	if (ret) {
		dev_err(se_dev->dev, "RNG1 Mutex acquire failed\n");
		clk_disable_unprepare(se_dev->c);
		return ret;
	}

	ret = tegra_se_check_rng1_status(se_dev);
	if (ret) {
		dev_err(se_dev->dev, "RNG1 initial state is wrong\n");
		goto rel_mutex;
	}

	ret = tegra_se_rng1_do(se_dev, req);
rel_mutex:
	tegra_se_release_rng1_mutex(se_dev);
	clk_disable_unprepare(se_dev->c);

	return ret;
}
EXPORT_SYMBOL(tegra_se_rng1_op);

static int tegra_se_pka1_get_precomp(struct tegra_se_pka1_rsa_context *ctx,
				     struct tegra_se_pka1_ecc_request *ecc_req,
				     struct tegra_se_pka1_mod_request *mod_req)
{
	int ret;
	struct tegra_se_elp_dev *se_dev;

	if (ctx)
		se_dev = ctx->se_dev;
	else if (ecc_req)
		se_dev = ecc_req->se_dev;
	else if (mod_req)
		se_dev = mod_req->se_dev;
	else
		return -EINVAL;

	ret = tegra_se_pka1_precomp(ctx, ecc_req, mod_req, PRECOMP_RINV);
	if (ret) {
		dev_err(se_dev->dev,
			"RINV: tegra_se_pka1_precomp Failed(%d)\n", ret);
		return ret;
	}
	ret = tegra_se_pka1_precomp(ctx, ecc_req, mod_req, PRECOMP_M);
	if (ret) {
		dev_err(se_dev->dev,
			"M: tegra_se_pka1_precomp Failed(%d)\n", ret);
		return ret;
	}
	ret = tegra_se_pka1_precomp(ctx, ecc_req, mod_req, PRECOMP_R2);
	if (ret)
		dev_err(se_dev->dev,
			"R2: tegra_se_pka1_precomp Failed(%d)\n", ret);

	return ret;
}

static int tegra_se_pka1_rsa_op(struct akcipher_request *req)
{
	struct crypto_akcipher *tfm;
	struct tegra_se_pka1_rsa_context *ctx;
	struct tegra_se_elp_dev *se_dev = elp_dev;
	int ret, cnt;
	u32 i, nwords;
	u32 *MSG;

	if (!req) {
		ret = -EINVAL;
		goto exit;
	}

	tfm = crypto_akcipher_reqtfm(req);
	if (!tfm) {
		ret = -EINVAL;
		goto exit;
	}

	ctx = akcipher_tfm_ctx(tfm);
	if (!ctx || (se_dev->chipdata->use_key_slot && !ctx->slot)) {
		ret = -EINVAL;
		goto exit;
	}

	if ((req->src_len < TEGRA_SE_PKA1_RSA512_INPUT_SIZE) ||
	    (req->src_len > TEGRA_SE_PKA1_RSA4096_INPUT_SIZE) ||
	    (req->dst_len < TEGRA_SE_PKA1_RSA512_INPUT_SIZE) ||
	    (req->dst_len > TEGRA_SE_PKA1_RSA4096_INPUT_SIZE)) {
		ret = -EINVAL;
		goto exit;
	}

	if (req->src_len != ctx->keylen) {
		ret = -EINVAL;
		goto exit;
	}

	cnt = sg_copy_to_buffer(req->src, 1, ctx->message, req->src_len);
	if (cnt != req->src_len) {
		ret = -ENODATA;
		goto exit;
	}

	nwords = req->src_len / WORD_SIZE_BYTES;
	MSG = ctx->message;
	for (i = 0; i < nwords; i++) {
		se_elp_writel(se_dev, PKA1, *MSG++, reg_bank_offset(
			      TEGRA_SE_PKA1_RSA_MSG_BANK,
			      TEGRA_SE_PKA1_RSA_MSG_ID,
			      ctx->op_mode) + (i * 4));
	}

	tegra_se_program_pka1_rsa(ctx);

	ret = tegra_se_check_pka1_op_done(se_dev);
	if (ret)
		goto exit;

	tegra_se_read_pka1_rsa_result(ctx, nwords);

	cnt = sg_copy_from_buffer(req->dst, 1, ctx->result, req->dst_len);
	if (cnt != req->dst_len) {
		ret = -ENODATA;
		goto exit;
	}
exit:
	clk_disable_unprepare(se_dev->c);

	return ret;
}

int tegra_se_pka1_ecc_op(struct tegra_se_pka1_ecc_request *req)
{
	struct tegra_se_elp_dev *se_dev;
	int ret;

	if (!req)
		return -EINVAL;

	ret = tegra_se_pka1_ecc_init(req);
	if (ret)
		return ret;

	se_dev = req->se_dev;
	clk_prepare_enable(se_dev->c);

	ret = tegra_se_acquire_pka1_mutex(se_dev);
	if (ret) {
		dev_err(se_dev->dev, "PKA1 Mutex acquire failed\n");
		goto clk_dis;
	}

	ret = tegra_se_pka1_get_precomp(NULL, req, NULL);
	if (ret)
		goto exit;

	ret = tegra_se_pka1_ecc_do(req);
exit:
	tegra_se_release_pka1_mutex(se_dev);
clk_dis:
	clk_disable_unprepare(se_dev->c);
	tegra_se_pka1_ecc_exit(req);

	return ret;
}
EXPORT_SYMBOL(tegra_se_pka1_ecc_op);

int tegra_se_pka1_mod_op(struct tegra_se_pka1_mod_request *req)
{
	struct tegra_se_elp_dev *se_dev;
	int ret;

	if (!req)
		return -EINVAL;

	ret = tegra_se_pka1_mod_init(req);
	if (ret)
		return ret;

	se_dev = req->se_dev;
	clk_prepare_enable(se_dev->c);

	ret = tegra_se_acquire_pka1_mutex(se_dev);
	if (ret) {
		dev_err(se_dev->dev, "PKA1 Mutex acquire failed\n");
		goto clk_dis;
	}

	ret = tegra_se_pka1_get_precomp(NULL, NULL, req);
	if (ret)
		goto exit;

	ret = tegra_se_pka1_mod_do(req);
exit:
	tegra_se_release_pka1_mutex(se_dev);
clk_dis:
	clk_disable_unprepare(se_dev->c);
	tegra_se_pka1_mod_exit(req);

	return ret;
}

static int tegra_se_mod_mult(int op_mode, u32 *result,
			     const u32 *left, const u32 *right,
			     const u32 *mod, int nbytes)
{
	struct tegra_se_pka1_mod_request req;
	int ret;

	req.op_mode = op_mode;
	req.size = nbytes;
	req.type = MOD_MULT;
	req.modulus = mod;
	req.base_pt_x = left;
	req.base_pt_y = right;
	req.result = result;
	ret = tegra_se_pka1_mod_op(&req);

	return ret;
}
static int tegra_se_mod_add(int op_mode, u32 *result,
			    const u32 *left, const u32 *right,
			    const u32 *mod, int nbytes)
{
	struct tegra_se_pka1_mod_request req;
	int ret;

	req.op_mode = op_mode;
	req.size = nbytes;
	req.type = MOD_ADD;
	req.modulus = mod;
	req.base_pt_x = left;
	req.base_pt_y = right;
	req.result = result;
	ret = tegra_se_pka1_mod_op(&req);

	return ret;
}

static int tegra_se_mod_inv(int op_mode, u32 *result,
			    const u32 *input,
			    const u32 *mod, int nbytes)
{
	struct tegra_se_pka1_mod_request req;
	int ret;

	req.op_mode = op_mode;
	req.size = nbytes;
	req.type = MOD_INV;
	req.modulus = mod;
	req.base_pt_x = input;
	req.result = result;
	ret = tegra_se_pka1_mod_op(&req);

	return ret;
}

static int tegra_se_mod_reduce(int op_mode, u32 *result,
			       const u32 *input,
			       const u32 *mod, int nbytes)
{
	struct tegra_se_pka1_mod_request req;
	int ret;

	req.op_mode = op_mode;
	req.size = nbytes;
	req.type = MOD_REDUCE;
	req.modulus = mod;
	req.base_pt_x = input;
	req.result = result;
	ret = tegra_se_pka1_mod_op(&req);

	return ret;
}

static int tegra_se_ecc_point_mult(struct tegra_se_ecc_point *result,
				   const struct tegra_se_ecc_point *point,
				   const u32 *private,
				   const struct tegra_se_ecc_curve *curve,
				   int nbytes)
{
	struct tegra_se_pka1_ecc_request ecc_req;
	int ret;

	ecc_req.op_mode = curve->mode;
	ecc_req.size = nbytes;
	ecc_req.type = ECC_POINT_MUL;
	ecc_req.curve_param_a = curve->a;
	ecc_req.modulus = curve->p;
	ecc_req.base_pt_x = point->x;
	ecc_req.base_pt_y = point->y;
	ecc_req.res_pt_x = result->x;
	ecc_req.res_pt_y = result->y;
	ecc_req.key = (u32 *)private;

	ret = tegra_se_pka1_ecc_op(&ecc_req);

	return ret;
}

static int tegra_se_ecc_point_add(struct tegra_se_ecc_point *p1,
				  struct tegra_se_ecc_point *p2,
				  const struct tegra_se_ecc_curve *curve,
				  int nbytes)
{
	struct tegra_se_pka1_ecc_request ecc_req;
	int ret;

	ecc_req.se_dev = elp_dev;
	ecc_req.op_mode = curve->mode;
	ecc_req.size = nbytes;
	ecc_req.type = ECC_POINT_ADD;
	ecc_req.curve_param_a = curve->a;
	ecc_req.modulus = curve->p;
	ecc_req.base_pt_x = p1->x;
	ecc_req.base_pt_y = p1->y;
	ecc_req.res_pt_x = p2->x;
	ecc_req.res_pt_y = p2->y;

	ret = tegra_se_pka1_ecc_op(&ecc_req);

	return ret;
}

static int tegra_se_ecc_shamir_trick(u32 *s1,
				     struct tegra_se_ecc_point *p1,
				     u32 *s2,
				     struct tegra_se_ecc_point *p2,
				     const struct tegra_se_ecc_curve *curve,
				     int nbytes)
{
	struct tegra_se_pka1_ecc_request ecc_req;
	int ret;

	ecc_req.se_dev = elp_dev;
	ecc_req.op_mode = curve->mode;
	ecc_req.size = nbytes;
	ecc_req.type = ECC_SHAMIR_TRICK;
	ecc_req.curve_param_a = curve->a;
	ecc_req.modulus = curve->p;
	ecc_req.curve_param_b = s1;
	ecc_req.key = s2;
	ecc_req.base_pt_x = p1->x;
	ecc_req.base_pt_y = p1->y;
	ecc_req.res_pt_x = p2->x;
	ecc_req.res_pt_y = p2->y;

	ret = tegra_se_pka1_ecc_op(&ecc_req);

	return ret;
}

static int tegra_se_ecdh_compute_shared_secret(struct tegra_se_elp_dev *se_dev,
					       unsigned int cid,
					       const u32 *private_key,
					       const u32 *public_key,
					       u32 *secret)
{
	struct tegra_se_ecc_point *product, *pk;
	const struct tegra_se_ecc_curve *curve = tegra_se_ecc_get_curve(cid);
	int nbytes = curve->nbytes;
	int nwords = nbytes / WORD_SIZE_BYTES;
	int ret = -ENOMEM;
	u32 priv[ECC_MAX_WORDS];

	if (!private_key || !public_key)
		return -EINVAL;

	pk = tegra_se_ecc_alloc_point(se_dev, nwords);
	if (!pk)
		return ret;

	product = tegra_se_ecc_alloc_point(se_dev, nwords);
	if (!product)
		goto exit;

	tegra_se_ecc_swap(public_key, pk->x, nwords);
	tegra_se_ecc_swap(&public_key[nwords], pk->y, nwords);
	tegra_se_ecc_swap(private_key, priv, nwords);

	ret = tegra_se_ecc_point_mult(product, pk, priv, curve, nbytes);
	if (ret)
		goto err_pt_mult;

	tegra_se_ecc_swap(product->x, secret, nwords);

	if (tegra_se_ecc_vec_is_zero(product->x, nbytes) ||
	    tegra_se_ecc_vec_is_zero(product->y, nbytes))
		ret = -ENODATA;
err_pt_mult:
	tegra_se_ecc_free_point(se_dev, product);
exit:
	tegra_se_ecc_free_point(se_dev, pk);

	return ret;
}

static int tegra_se_ecdh_gen_pub_key(struct tegra_se_elp_dev *se_dev,
				     unsigned int cid, const u32 *private_key,
				     u32 *public_key)
{
	struct tegra_se_ecc_point *G;
	int ret;
	const struct tegra_se_ecc_curve *curve = tegra_se_ecc_get_curve(cid);
	int nbytes = curve->nbytes;
	int nwords = nbytes / WORD_SIZE_BYTES;
	u32 priv[ECC_MAX_WORDS];

	if (!private_key)
		return -EINVAL;

	tegra_se_ecc_swap(private_key, priv, nwords);

	G = tegra_se_ecc_alloc_point(se_dev, nwords);
	if (!G)
		return -ENOMEM;

	ret = tegra_se_ecc_point_mult(G, &curve->g, priv, curve, nbytes);
	if (ret)
		goto err_pt_mult;

	if (tegra_se_ecc_vec_is_zero(G->x, nbytes) ||
	    tegra_se_ecc_vec_is_zero(G->y, nbytes))
		ret = -ENODATA;

	tegra_se_ecc_swap(G->x, public_key, nwords);
	tegra_se_ecc_swap(G->y, &public_key[nwords], nwords);

err_pt_mult:
	tegra_se_ecc_free_point(se_dev, G);
	return ret;
}

static int tegra_se_ecdh_compute_value(struct kpp_request *req)
{
	struct crypto_kpp *tfm;
	struct tegra_se_ecdh_context *ctx;
	int nbytes, ret, cnt;
	void *buffer;
	const struct tegra_se_ecc_curve *curve;

	if (!req)
		return -EINVAL;

	tfm = crypto_kpp_reqtfm(req);
	if (!tfm)
		return -ENODATA;

	ctx = kpp_tfm_ctx(tfm);
	if (!ctx)
		return -ENODATA;

	ctx->se_dev = elp_dev;

	curve = tegra_se_ecc_get_curve(ctx->curve_id);
	if (!curve)
		return -ENOTSUPP;

	nbytes = curve->nbytes;

	if (req->src) {
		cnt = sg_copy_to_buffer(req->src, 1, ctx->public_key,
					2 * nbytes);
		if (cnt != 2 * nbytes)
			return -ENODATA;

		ret = tegra_se_ecdh_compute_shared_secret(ctx->se_dev,
							  ctx->curve_id,
							  ctx->private_key,
							  ctx->public_key,
							  ctx->shared_secret);
		if (ret < 0)
			return ret;

		buffer = ctx->shared_secret;

		cnt = sg_copy_from_buffer(req->dst, 1, buffer, nbytes);
		if (cnt != nbytes)
			return -ENODATA;
	} else {
		ret = tegra_se_ecdh_gen_pub_key(ctx->se_dev, ctx->curve_id,
						ctx->private_key,
						ctx->public_key);
		if (ret < 0)
			return ret;

		buffer = ctx->public_key;

		cnt = sg_copy_from_buffer(req->dst, 1, buffer, 2 * nbytes);
		if (cnt != 2 * nbytes)
			return -ENODATA;
	}

	return 0;
}

static int tegra_se_ecdh_set_secret(struct crypto_kpp *tfm, void *buf,
				    unsigned int len)
{
	struct tegra_se_ecdh_context *ctx = kpp_tfm_ctx(tfm);
	struct ecdh params;
	int ret;

	ret = crypto_ecdh_decode_key(buf, len, &params);
	if (ret)
		return ret;

	ret = tegra_se_ecdh_set_params(ctx, &params);
	if (ret)
		return ret;

	return 0;
}

static int tegra_se_ecdh_max_size(struct crypto_kpp *tfm)
{
	struct tegra_se_ecdh_context *ctx = kpp_tfm_ctx(tfm);
	const struct tegra_se_ecc_curve *curve =
			tegra_se_ecc_get_curve(ctx->curve_id);
	int nbytes = curve->nbytes;

	/* Public key is made of two coordinates */
	return 2 * nbytes;
}

static int tegra_se_pka1_get_opmode(struct tegra_se_pka1_rsa_context *ctx)
{
	if (ctx->keylen == TEGRA_SE_PKA1_RSA512_INPUT_SIZE)
		ctx->op_mode = SE_ELP_OP_MODE_RSA512;
	else if (ctx->keylen == TEGRA_SE_PKA1_RSA768_INPUT_SIZE)
		ctx->op_mode = SE_ELP_OP_MODE_RSA768;
	else if (ctx->keylen == TEGRA_SE_PKA1_RSA1024_INPUT_SIZE)
		ctx->op_mode = SE_ELP_OP_MODE_RSA1024;
	else if (ctx->keylen == TEGRA_SE_PKA1_RSA1536_INPUT_SIZE)
		ctx->op_mode = SE_ELP_OP_MODE_RSA1536;
	else if (ctx->keylen == TEGRA_SE_PKA1_RSA2048_INPUT_SIZE)
		ctx->op_mode = SE_ELP_OP_MODE_RSA2048;
	else if (ctx->keylen == TEGRA_SE_PKA1_RSA3072_INPUT_SIZE)
		ctx->op_mode = SE_ELP_OP_MODE_RSA3072;
	else if (ctx->keylen == TEGRA_SE_PKA1_RSA4096_INPUT_SIZE)
		ctx->op_mode = SE_ELP_OP_MODE_RSA4096;
	else
		return -EINVAL;

	return 0;
}

static int tegra_se_pka1_rsa_setkey(struct crypto_akcipher *tfm,
				    const void *key, unsigned int keylen)
{
	struct tegra_se_pka1_rsa_context *ctx = akcipher_tfm_ctx(tfm);
	struct tegra_se_elp_dev *se_dev = elp_dev;
	struct tegra_se_pka1_slot *pslot;
	u32 i = 0, key_words = keylen / WORD_SIZE_BYTES;
	u32 slot_num, val = 0;
	u8 *pkeydata;
	u32 *EXP;
	int ret;

	if (!ctx || !key)
		return -EINVAL;

	pkeydata = (u8 *)key;

	clk_prepare_enable(se_dev->c);

	ret = tegra_se_acquire_pka1_mutex(se_dev);
	if (ret) {
		dev_err(se_dev->dev, "PKA1 Mutex acquire failed\n");
		goto clk_dis;
	}

	ctx->keylen = keylen;

	memcpy((u8 *)ctx->exponent, pkeydata, keylen);
	memcpy((u8 *)ctx->modulus, &pkeydata[keylen], keylen);

	ret = tegra_se_pka1_get_opmode(ctx);
	if (ret)
		goto rel_mutex;

	ret = tegra_se_pka1_get_precomp(ctx, NULL, NULL);
	if (ret)
		goto rel_mutex;

	if (se_dev->chipdata->use_key_slot) {
		if (!ctx->slot) {
			pslot = tegra_se_pka1_alloc_key_slot();
			if (!pslot) {
				dev_err(se_dev->dev, "no free key slot\n");
				ret = -ENOMEM;
				goto rel_mutex;
			}
			ctx->slot = pslot;
		}
		slot_num = ctx->slot->slot_num;

		tegra_se_set_pka1_rsa_key(ctx);

		val = TEGRA_SE_PKA1_CTRL_CONTROL_KEYSLOT(slot_num) |
			TEGRA_SE_PKA1_CTRL_CONTROL_LOAD_KEY(ELP_ENABLE);

		se_elp_writel(se_dev, PKA1, val,
			      TEGRA_SE_PKA1_CTRL_CONTROL_OFFSET);
		/* poll SE_STATUS */
		do {
			if (i > PKA1_TIMEOUT) {
				dev_err(se_dev->dev,
					"PKA1 Load Key timed out\n");
				return -EINVAL;
			}
			udelay(1);
			val = se_elp_readl(se_dev, PKA1,
					   TEGRA_SE_PKA1_CTRL_STATUS_OFFSET);
			i++;
		} while (val & TEGRA_SE_PKA1_CTRL_SE_STATUS(SE_STATUS_BUSY));
	} else {
		EXP = ctx->exponent;
		for (i = 0; i < key_words; i++) {
			se_elp_writel(se_dev, PKA1, *EXP++, reg_bank_offset(
				      TEGRA_SE_PKA1_RSA_EXP_BANK,
				      TEGRA_SE_PKA1_RSA_EXP_ID,
				      ctx->op_mode) + (i * 4));
		}
	}

	/*memset exponent and modulus to 0 before returning */
	memset((u8 *)ctx->exponent, 0x0, keylen);
	memset((u8 *)ctx->modulus, 0x0, keylen);

	return ret;
rel_mutex:
	tegra_se_release_pka1_mutex(se_dev);
clk_dis:
	clk_disable_unprepare(se_dev->c);
	return ret;

}

static int tegra_se_pka1_rsa_max_size(struct crypto_akcipher *tfm)
{
	struct tegra_se_pka1_rsa_context *ctx = akcipher_tfm_ctx(tfm);

	if (!ctx)
		return -EINVAL;

	if (ctx->keylen < TEGRA_SE_PKA1_RSA512_INPUT_SIZE ||
	    ctx->keylen > TEGRA_SE_PKA1_RSA4096_INPUT_SIZE)
		return -EINVAL;

	return ctx->keylen;
}

static int tegra_se_pka1_rsa_init(struct crypto_akcipher *tfm)
{
	struct tegra_se_pka1_rsa_context *ctx = akcipher_tfm_ctx(tfm);
	struct tegra_se_elp_dev *se_dev = elp_dev;

	if (!ctx)
		return -EINVAL;

	ctx->se_dev = se_dev;

	ctx->m = devm_kzalloc(se_dev->dev, MAX_PKA1_SIZE, GFP_KERNEL);
	if (!ctx->m)
		return -ENOMEM;

	ctx->r2 = devm_kzalloc(se_dev->dev, MAX_PKA1_SIZE, GFP_KERNEL);
	if (!ctx->r2)
		goto m_free;

	ctx->result = devm_kzalloc(se_dev->dev, MAX_PKA1_SIZE, GFP_KERNEL);
	if (!ctx->result)
		goto r2_free;

	ctx->message = devm_kzalloc(se_dev->dev, MAX_PKA1_SIZE, GFP_KERNEL);
	if (!ctx->message)
		goto res_free;

	ctx->exponent = devm_kzalloc(se_dev->dev, MAX_PKA1_SIZE, GFP_KERNEL);
	if (!ctx->exponent)
		goto msg_free;

	ctx->modulus = devm_kzalloc(se_dev->dev, MAX_PKA1_SIZE, GFP_KERNEL);
	if (!ctx->modulus)
		goto exp_free;

	return 0;
exp_free:
	devm_kfree(se_dev->dev, ctx->exponent);
msg_free:
	devm_kfree(se_dev->dev, ctx->message);
res_free:
	devm_kfree(se_dev->dev, ctx->result);
r2_free:
	devm_kfree(se_dev->dev, ctx->r2);
m_free:
	devm_kfree(se_dev->dev, ctx->m);

	return -ENOMEM;
}

static void tegra_se_pka1_rsa_exit(struct crypto_akcipher *tfm)
{
	struct tegra_se_pka1_rsa_context *ctx = akcipher_tfm_ctx(tfm);
	struct tegra_se_elp_dev *se_dev;

	if (!ctx)
		return;

	se_dev = ctx->se_dev;

	devm_kfree(se_dev->dev, ctx->m);
	devm_kfree(se_dev->dev, ctx->r2);
	devm_kfree(se_dev->dev, ctx->result);
	devm_kfree(se_dev->dev, ctx->message);
	devm_kfree(se_dev->dev, ctx->exponent);
	devm_kfree(se_dev->dev, ctx->modulus);

	tegra_se_release_pka1_mutex(se_dev);

	if (!ctx->slot)
		return;

	tegra_se_pka1_free_key_slot(ctx->slot);
	ctx->slot = NULL;
}

static struct kpp_alg ecdh_algs[] = {
	{
	.set_secret = tegra_se_ecdh_set_secret,
	.generate_public_key = tegra_se_ecdh_compute_value,
	.compute_shared_secret = tegra_se_ecdh_compute_value,
	.max_size = tegra_se_ecdh_max_size,
	.base = {
		.cra_name = "ecdh",
		.cra_driver_name = "tegra-se-ecdh",
		.cra_priority = 300,
		.cra_module = THIS_MODULE,
		.cra_ctxsize = sizeof(struct tegra_se_ecdh_context),
		}
	}
};

static struct akcipher_alg pka1_rsa_algs[] = {
	{
		.encrypt = tegra_se_pka1_rsa_op,
		.decrypt = tegra_se_pka1_rsa_op,
		.sign = tegra_se_pka1_rsa_op,
		.verify = tegra_se_pka1_rsa_op,
		.set_priv_key = tegra_se_pka1_rsa_setkey,
		.set_pub_key = tegra_se_pka1_rsa_setkey,
		.max_size = tegra_se_pka1_rsa_max_size,
		.init = tegra_se_pka1_rsa_init,
		.exit = tegra_se_pka1_rsa_exit,
		.base = {
			.cra_name = "rsa",
			.cra_driver_name = "tegra-se-pka1-rsa",
			.cra_blocksize = MAX_PKA1_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_pka1_rsa_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
		}
	},
};

struct tegra_se_ecdsa_ctx {
	struct tegra_se_elp_dev *se_dev;
	unsigned int curve_id;
	unsigned int ndigits;
	u64 private_key[ECC_MAX_DIGITS];
	u64 public_key[2 * ECC_MAX_DIGITS];
};

static unsigned int tegra_se_ecdsa_supported_curve(unsigned int curve_id)
{
	switch (curve_id) {
	case ECC_CURVE_NIST_P192: return 3;
	case ECC_CURVE_NIST_P256: return 4;
	default: return 0;
	}
}

static struct tegra_se_ecdsa_ctx *
tegra_se_ecdsa_get_ctx(struct crypto_akcipher *tfm)
{
	return akcipher_tfm_ctx(tfm);
}

int tegra_se_ecdsa_sign(struct akcipher_request *req)
{
	struct crypto_akcipher *tfm = crypto_akcipher_reqtfm(req);
	struct tegra_se_ecdsa_ctx *ctx = tegra_se_ecdsa_get_ctx(tfm);
	struct tegra_se_ecc_point *x1y1 = NULL;
	const struct tegra_se_ecc_curve *curve =
		tegra_se_ecc_get_curve(ctx->curve_id);
	int nbytes = curve->nbytes;
	int nwords = nbytes / WORD_SIZE_BYTES;
	unsigned int ndigits = nwords / 2;
	u64 z[ndigits], d[ndigits];
	u64 k[ndigits], k_inv[ndigits];
	u64 r[ndigits], s[ndigits];
	u64 dr[ndigits], zdr[ndigits];
	u8 *r_ptr, *s_ptr;
	int ret = -ENOMEM;
	int mod_op_mode;

	if (req->dst_len < 2 * nbytes) {
		req->dst_len = 2 * nbytes;
		return -EINVAL;
	}

	if (!curve)
		return -EINVAL;

	mod_op_mode = tegra_se_mod_op_mode(nbytes);
	if (mod_op_mode < 0)
		return mod_op_mode;

	ecdsa_parse_msg_hash(req, z, ndigits);

	/* d */
	vli_set(d, (const u64 *)ctx->private_key, ndigits);

	/* k */
	ret = ecdsa_get_rnd_bytes((u8 *)k, nbytes);
	if (ret)
		return ret;

#if defined(CONFIG_CRYPTO_MANAGER2)
	if (req->info)
		vli_copy_from_buf(k, ndigits, req->info, nbytes);
#endif

	x1y1 = tegra_se_ecc_alloc_point(elp_dev, nwords);
	if (!x1y1)
		return -ENOMEM;

	/* (x1, y1) = k x G */
	ret = tegra_se_ecc_point_mult(x1y1, &curve->g, k, curve, nbytes);
	if (ret)
		goto exit;

	/* r = x1 mod n */
	ret = tegra_se_mod_reduce(mod_op_mode, r, x1y1->x, curve->n, nbytes);
	if (ret)
		goto exit;

	/* k^-1 */
	ret = tegra_se_mod_inv(mod_op_mode, k_inv, k, curve->n, nbytes);
	if (ret)
		goto exit;

	/* d . r mod n */
	ret = tegra_se_mod_mult(mod_op_mode, dr, d, r, curve->n, nbytes);
	if (ret)
		goto exit;

	/* z + dr mod n */
	ret = tegra_se_mod_add(mod_op_mode, zdr, z, dr, curve->n, nbytes);
	if (ret)
		goto exit;

	/* k^-1 . ( z + dr) mod n */
	ret = tegra_se_mod_mult(mod_op_mode, s, k_inv, zdr, curve->n, nbytes);
	if (ret)
		goto exit;

	/* write signature (r,s) in dst */
	r_ptr = sg_virt(req->dst);
	s_ptr = (u8 *)sg_virt(req->dst) + nbytes;

	vli_copy_to_buf(r_ptr, nbytes, r, ndigits);
	vli_copy_to_buf(s_ptr, nbytes, s, ndigits);

	req->dst_len = 2 * nbytes;
 exit:
	tegra_se_ecc_free_point(elp_dev, x1y1);
	return ret;
}


int tegra_se_ecdsa_verify(struct akcipher_request *req)
{
	struct crypto_akcipher *tfm = crypto_akcipher_reqtfm(req);
	struct tegra_se_ecdsa_ctx *ctx = tegra_se_ecdsa_get_ctx(tfm);
	struct tegra_se_ecc_point *x1y1 = NULL, *x2y2 = NULL, *Q = NULL;
	const struct tegra_se_ecc_curve *curve =
		tegra_se_ecc_get_curve(ctx->curve_id);
	int nbytes = curve->nbytes;
	int nwords = nbytes / WORD_SIZE_BYTES;
	unsigned int ndigits = nwords / 2;
	u64 *ctx_qx, *ctx_qy;
	u64 r[ndigits], s[ndigits], v[ndigits];
	u64 z[ndigits], w[ndigits];
	u64 u1[ndigits], u2[ndigits];
	int mod_op_mode;
	int ret = -ENOMEM;

	if (!curve)
		return -EINVAL;

	mod_op_mode = tegra_se_mod_op_mode(nbytes);
	if (mod_op_mode < 0)
		return mod_op_mode;

	x1y1 = tegra_se_ecc_alloc_point(elp_dev, nwords);
	x2y2 = tegra_se_ecc_alloc_point(elp_dev, nwords);
	Q = tegra_se_ecc_alloc_point(elp_dev, nwords);
	if (!x1y1 || !x2y2 || !Q) {
		ret = -ENOMEM;
		goto exit;
	}

	ecdsa_parse_msg_hash(req, z, ndigits);

	/* Signature r,s */
	vli_copy_from_buf(r, ndigits, sg_virt(&req->src[1]), nbytes);
	vli_copy_from_buf(s, ndigits, sg_virt(&req->src[2]), nbytes);

	/* w = s^-1 mod n */
	ret = tegra_se_mod_inv(mod_op_mode, w, s, curve->n, nbytes);
	if (ret)
		goto exit;

	/* u1 = zw mod n */
	ret = tegra_se_mod_mult(mod_op_mode, u1, z, w, curve->n, nbytes);
	if (ret)
		goto exit;

	/* u2 = rw mod n */
	ret = tegra_se_mod_mult(mod_op_mode, u2, r, w, curve->n, nbytes);
	if (ret)
		goto exit;

#if defined(ECDSA_USE_SHAMIRS_TRICK)
	/* Q=(Qx,Qy) */
	ctx_qx = ctx->public_key;
	ctx_qy = ctx_qx + ECC_MAX_DIGITS;
	vli_set(Q->x, ctx_qx, ndigits);
	vli_set(Q->y, ctx_qy, ndigits);

	/* u1.G + u2.Q => P + Q in Q */
	ret = tegra_se_ecc_shamir_trick(u1, &curve->g, u2, Q, curve, nbytes);
	if (ret)
		goto exit;

	/* v = x mod n */
	ret = tegra_se_mod_reduce(mod_op_mode, v, Q->x, curve->n, nbytes);
	if (ret)
		goto exit;
#else
	/* u1 . G */
	ret = tegra_se_ecc_point_mult(x1y1, &curve->g, u1, curve, nbytes);
	if (ret)
		goto exit;

	/* Q=(Qx,Qy) */
	ctx_qx = ctx->public_key;
	ctx_qy = ctx_qx + ECC_MAX_DIGITS;
	vli_set(Q->x, ctx_qx, ndigits);
	vli_set(Q->y, ctx_qy, ndigits);

	/* u2 x Q */
	ret = tegra_se_ecc_point_mult(x2y2, Q, u2, curve, nbytes);
	if (ret)
		goto exit;

	/* x1y1 + x2y2 => P + Q; P + Q in x2 y2 */
	ret = tegra_se_ecc_point_add(x1y1, x2y2, curve, nbytes);
	if (ret)
		goto exit;

	/* v = x mod n */
	ret = tegra_se_mod_reduce(mod_op_mode, v, x2y2->x, curve->n, nbytes);
	if (ret)
		goto exit;
#endif
	/* validate signature */
	ret = vli_cmp(v, r, ndigits) == 0 ? 0 : -EBADMSG;
 exit:
	tegra_se_ecc_free_point(elp_dev, x1y1);
	tegra_se_ecc_free_point(elp_dev, x2y2);
	tegra_se_ecc_free_point(elp_dev, Q);
	return ret;
}

int tegra_se_ecdsa_dummy_enc(struct akcipher_request *req)
{
	return -EINVAL;
}

int tegra_se_ecdsa_dummy_dec(struct akcipher_request *req)
{
	return -EINVAL;
}

int tegra_se_ecdsa_set_pub_key(struct crypto_akcipher *tfm, const void *key,
		      unsigned int keylen)
{
	struct tegra_se_ecdsa_ctx *ctx = tegra_se_ecdsa_get_ctx(tfm);
	struct ecdsa params;
	unsigned int ndigits;
	unsigned int nbytes;
	u8 *params_qx, *params_qy;
	u64 *ctx_qx, *ctx_qy;
	int err = 0;

	if (crypto_ecdsa_parse_pub_key(key, keylen, &params))
		return -EINVAL;

	ndigits = tegra_se_ecdsa_supported_curve(params.curve_id);
	if (!ndigits)
		return -EINVAL;

	err = ecc_is_pub_key_valid(params.curve_id, ndigits,
				   params.key, params.key_size);
	if (err)
		return err;

	ctx->curve_id = params.curve_id;
	ctx->ndigits = ndigits;
	nbytes = ndigits << ECC_DIGITS_TO_BYTES_SHIFT;

	params_qx = params.key;
	params_qy = params_qx + ECC_MAX_DIGIT_BYTES;

	ctx_qx = ctx->public_key;
	ctx_qy = ctx_qx + ECC_MAX_DIGITS;

	vli_copy_from_buf(ctx_qx, ndigits, params_qx, nbytes);
	vli_copy_from_buf(ctx_qy, ndigits, params_qy, nbytes);

	memset(&params, 0, sizeof(params));
	return 0;
}

int tegra_se_ecdsa_set_priv_key(struct crypto_akcipher *tfm, const void *key,
		       unsigned int keylen)
{
	struct tegra_se_ecdsa_ctx *ctx = tegra_se_ecdsa_get_ctx(tfm);
	struct ecdsa params;
	unsigned int ndigits;
	unsigned int nbytes;

	if (crypto_ecdsa_parse_priv_key(key, keylen, &params))
		return -EINVAL;

	ndigits = tegra_se_ecdsa_supported_curve(params.curve_id);
	if (!ndigits)
		return -EINVAL;

	ctx->curve_id = params.curve_id;
	ctx->ndigits = ndigits;
	nbytes = ndigits << ECC_DIGITS_TO_BYTES_SHIFT;

	if (ecc_is_key_valid(ctx->curve_id, ctx->ndigits,
			     (const u8 *)params.key, params.key_size) < 0)
		return -EINVAL;

	vli_copy_from_buf(ctx->private_key, ndigits, params.key, nbytes);

	memset(&params, 0, sizeof(params));
	return 0;
}

int tegra_se_ecdsa_max_size(struct crypto_akcipher *tfm)
{
	struct tegra_se_ecdsa_ctx *ctx = tegra_se_ecdsa_get_ctx(tfm);
	int nbytes = ctx->ndigits << ECC_DIGITS_TO_BYTES_SHIFT;

	/* For r,s */
	return 2 * nbytes;
}

int tegra_se_ecdsa_init_tfm(struct crypto_akcipher *tfm)
{
	return 0;
}

void tegra_se_ecdsa_exit_tfm(struct crypto_akcipher *tfm)
{
}

static struct akcipher_alg ecdsa_alg = {
	.sign		= tegra_se_ecdsa_sign,
	.verify		= tegra_se_ecdsa_verify,
	.encrypt	= tegra_se_ecdsa_dummy_enc,
	.decrypt	= tegra_se_ecdsa_dummy_dec,
	.set_priv_key	= tegra_se_ecdsa_set_priv_key,
	.set_pub_key	= tegra_se_ecdsa_set_pub_key,
	.max_size	= tegra_se_ecdsa_max_size,
	.init		= tegra_se_ecdsa_init_tfm,
	.exit		= tegra_se_ecdsa_exit_tfm,
	.base = {
		.cra_name	= "ecdsa",
		.cra_driver_name = "ecdsa-tegra",
		.cra_priority	= 300,
		.cra_module	= THIS_MODULE,
		.cra_ctxsize	= sizeof(struct tegra_se_ecdsa_ctx),
	},
};

static struct tegra_se_elp_chipdata tegra18_se_chipdata = {
	.use_key_slot = true,
	.rng1_supported = true,
};

static struct tegra_se_elp_chipdata tegra21_se_chipdata = {
	.use_key_slot = true,
	.rng1_supported = false,
};

static int tegra_se_elp_probe(struct platform_device *pdev)
{
	struct tegra_se_elp_dev *se_dev;
	struct resource *res;
	struct device_node *node;
	int err, i;

	se_dev = devm_kzalloc(&pdev->dev, sizeof(struct tegra_se_elp_dev),
			      GFP_KERNEL);
	if (!se_dev)
		return -ENOMEM;

	se_dev->chipdata = of_device_get_match_data(&pdev->dev);

	platform_set_drvdata(pdev, se_dev);
	se_dev->dev = &pdev->dev;

	for (i = 0; i < NR_RES; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		se_dev->io_reg[i] = devm_ioremap_resource(se_dev->dev, res);
		if (IS_ERR(se_dev->io_reg[i]))
			return PTR_ERR(se_dev->io_reg[i]);

		if (!se_dev->chipdata->rng1_supported)
			break;
	}

	se_dev->irq = platform_get_irq(pdev, 0);
	if (se_dev->irq <= 0) {
		dev_err(&pdev->dev, "no irq resource\n");
		return -ENODEV;
	}

	err = devm_request_irq(se_dev->dev, se_dev->irq, tegra_se_pka1_irq, 0,
			dev_name(se_dev->dev), se_dev);
	if (err) {
		dev_err(se_dev->dev, "request_irq failed - irq[%d] err[%d]\n",
			se_dev->irq, err);
		return err;
	}

	se_dev->c = devm_clk_get(se_dev->dev, "se");
	if (IS_ERR(se_dev->c)) {
		dev_err(se_dev->dev, "se clk_get_sys failed: %ld\n",
			PTR_ERR(se_dev->c));
		return PTR_ERR(se_dev->c);
	}

	err = clk_prepare_enable(se_dev->c);
	if (err) {
		dev_err(se_dev->dev, "clk enable failed for se\n");
		return err;
	}

	if (se_dev->chipdata->rng1_supported) {
		se_dev->rdata = devm_kzalloc(se_dev->dev,
					     sizeof(u32) * 4, GFP_KERNEL);
		if (!se_dev->rdata) {
			err = -ENOMEM;
			goto clk_dis;
		}
	}

	elp_dev = se_dev;

	err = tegra_se_pka1_init_key_slot(se_dev);
	if (err) {
		dev_err(se_dev->dev, "tegra_se_pka_init_key_slot failed\n");
		goto clk_dis;
	}

	init_completion(&se_dev->complete);

	err = tegra_se_check_trng_op(se_dev);
	if (err)
		err = tegra_se_set_trng_op(se_dev);
	if (err) {
		dev_err(se_dev->dev, "set_trng_op Failed\n");
		goto clk_dis;
	}

	err = crypto_register_kpp(&ecdh_algs[0]);
	if (err) {
		dev_err(se_dev->dev, "kpp registeration failed for ECDH\n");
		goto clk_dis;
	}

	node = of_node_get(se_dev->dev->of_node);
	err = of_property_read_u32(node, "pka1-rsa-priority",
				   &pka1_rsa_algs[0].base.cra_priority);
	if (err) {
		dev_err(se_dev->dev, "Missing rsa-pka-source property\n");
		goto prop_fail;
	}

	err = crypto_register_akcipher(&pka1_rsa_algs[0]);
	if (err) {
		dev_err(se_dev->dev, "crypto_register_akcipher "
			"alg failed index[0]\n");
		goto prop_fail;
	}

	err = crypto_register_akcipher(&ecdsa_alg);
	if (err) {
		dev_err(se_dev->dev,
			"crypto_register_alg ecdsa failed\n");
		goto ecdsa_fail;
	}

	clk_disable_unprepare(se_dev->c);
	dev_info(se_dev->dev, "%s: complete", __func__);
	return 0;

ecdsa_fail:
	crypto_unregister_akcipher(&pka1_rsa_algs[0]);
prop_fail:
	crypto_unregister_kpp(&ecdh_algs[0]);
clk_dis:
	clk_disable_unprepare(se_dev->c);

	return err;
}

#ifdef CONFIG_PM_SLEEP
static int tegra_se_elp_suspend(struct device *dev)
{
	struct tegra_se_elp_dev *se_dev = dev_get_drvdata(dev);

	/* This is needed as ATF (ARM Trusted Firmware) needs SE clk in SC7
	 * cycle and ATF does not have access to BPMP to enable the clk by
	 * itself. So, currently this is handled in linux driver.
	 */
	clk_prepare_enable(se_dev->c);

	return 0;
}

static int tegra_se_elp_resume(struct device *dev)
{
	struct tegra_se_elp_dev *se_dev = dev_get_drvdata(dev);

	clk_disable_unprepare(se_dev->c);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops tegra_se_elp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tegra_se_elp_suspend, tegra_se_elp_resume)
};

static const struct of_device_id tegra_se_elp_of_match[] = {
	{
		.compatible = "nvidia,tegra186-se-elp",
		.data = &tegra18_se_chipdata,
	}, {
		.compatible = "nvidia,tegra210b01-se-elp",
		.data = &tegra21_se_chipdata,
	}, {
	},
};
MODULE_DEVICE_TABLE(of, tegra_se_elp_of_match);

static struct platform_driver tegra_se_elp_driver = {
	.probe  = tegra_se_elp_probe,
	.driver = {
		.name   = "tegra-se-elp",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_se_elp_of_match),
		.pm = &tegra_se_elp_pm_ops,
	},
};
module_platform_driver(tegra_se_elp_driver);

MODULE_DESCRIPTION("Tegra Elliptic Crypto algorithm support");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPL");
MODULE_ALIAS("tegra-se-elp");

