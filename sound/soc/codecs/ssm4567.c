/*
 * SSM4567 amplifier audio driver
 *
 * Copyright 2013 Analog Devices Inc.
 *  Author: Lars-Peter Clausen <lars@metafoo.de>
 *
 * Licensed under the GPL-2.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#define SSM4567_REG_POWER		0x00
#define SSM4567_REG_SENSE_AMP		0x01
#define SSM4567_REG_DAC			0x02
#define SSM4567_REG_VOLUME		0x03
#define SSM4567_REG_SAI_CTRL1		0x04
#define SSM4567_REG_SAI_CTRL2		0x05
#define SSM4567_REG_PLACEMENT(x) (0x6 + (x))
#define SSM4567_REG_VBAT		0x0c
#define SSM4567_REG_LIMITER_CTRL1	0x0d
#define SSM4567_REG_LIMITER_CTRL2	0x0e
#define SSM4567_REG_LIMITER_CTRL3	0x0f
#define SSM4567_REG_STATUS1		0x10
#define SSM4567_REG_STATUS2		0x11
#define SSM4567_REG_FAULT		0x12
#define SSM4567_REG_PDM_CTRL		0x13
#define SSM4567_REG_CLOCK_CTRL		0x14
#define SSM4567_REG_BOOST_CTRL1		0x15
#define SSM4567_REG_BOOST_CTRL2		0x16
#define SSM4567_REG_SOFT_RESET		0xff

#define SSM4567_POWER_APWDN_EN		BIT(7)
#define SSM4567_POWER_BSNS_PWDN		BIT(6)
#define SSM4567_POWER_VSNS_PWDN		BIT(5)
#define SSM4567_POWER_ISNS_PWDN		BIT(4)
#define SSM4567_POWER_BOOTST_PWDN	BIT(3)
#define SSM4567_POWER_AMP_PWDN		BIT(2)
#define SSM4567_POWER_VBAT_ONLY		BIT(1)
#define SSM4567_POWER_SPWDN		BIT(0)

#define SSM4567_SAI_CTRL1_BCLK			BIT(6)
#define SSM4567_SAI_CTRL1_TDM_BLCKS_MASK	(0x3 << 4)
#define SSM4567_SAI_CTRL1_TDM_BLCKS_32		(0x0 << 4)
#define SSM4567_SAI_CTRL1_TDM_BLCKS_48		(0x1 << 4)
#define SSM4567_SAI_CTRL1_TDM_BLCKS_64		(0x2 << 4)
#define SSM4567_SAI_CTRL1_FSYNC			BIT(3)
#define SSM4567_SAI_CTRL1_LJ			BIT(2)
#define SSM4567_SAI_CTRL1_TDM			BIT(1)
#define SSM4567_SAI_CTRL1_PDM			BIT(0)

#define SSM4567_SAI_CTRL2_TDM_SLOT_MASK		0x7
#define SSM4567_SAI_CTRL2_TDM_SLOT(x)		(x)

#define SSM4567_DAC_MUTE		BIT(6)
#define SSM4567_DAC_FS_MASK		0x07
#define SSM4567_DAC_FS_8000		0x00
#define SSM4567_DAC_FS_16000		0x01
#define SSM4567_DAC_FS_32000		0x02
#define SSM4567_DAC_FS_64000		0x03
#define SSM4567_DAC_FS_128000		0x04

/* codec private data */
struct ssm4567_priv {
	struct regmap *regmap;
};

static const uint8_t ssm4567_reg[] = {
	0x81, 0x09, 0x32, 0x40, 0x00, 0x08, 0x01, 0x20,
	0x32, 0x07, 0x07, 0x07, 0x00, 0xa4, 0x73, 0x00,
	0x00, 0x00, 0x30, 0x40, 0x11, 0x02, 0x00,
};

static const DECLARE_TLV_DB_MINMAX_MUTE(ssm4567_vol_tlv, -7125, 2400);

static const struct snd_kcontrol_new ssm4567_snd_controls[] = {
	SOC_SINGLE("DAC High Pass Filter Switch", SSM4567_REG_DAC, 0, 1, 1),
	SOC_SINGLE_TLV("Master Playback Volume", SSM4567_REG_VOLUME, 0, 0xff, 1,
			ssm4567_vol_tlv),
};

static const struct snd_soc_dapm_widget ssm4567_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("DAC", "HiFi Playback", SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUTPUT("OUT"),
};

static const struct snd_soc_dapm_route ssm4567_routes[] = {
	{ "OUT", NULL, "DAC" },
};

static int ssm4567_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ssm4567_priv *ssm4567 = snd_soc_codec_get_drvdata(codec);
	unsigned int rate = params_rate(params);
	unsigned int fs;

	if (rate >= 8000 && rate <= 12000)
		fs = SSM4567_DAC_FS_8000;
	else if (rate >= 16000 && rate <= 24000)
		fs = SSM4567_DAC_FS_16000;
	else if (rate >= 32000 && rate <= 48000)
		fs = SSM4567_DAC_FS_32000;
	else if (rate >= 64000 && rate <= 96000)
		fs = SSM4567_DAC_FS_64000;
	else if (rate >= 128000 && rate <= 192000)
		fs = SSM4567_DAC_FS_128000;
	else
		return -EINVAL;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		break;
	default:
		return -EINVAL;
	}

	return regmap_update_bits(ssm4567->regmap, SSM4567_REG_DAC,
			SSM4567_DAC_FS_MASK, fs);
}

static int ssm4567_mute(struct snd_soc_dai *dai, int mute)
{
	struct ssm4567_priv *ssm4567 = snd_soc_codec_get_drvdata(dai->codec);
	unsigned int val;

	if (mute)
		val = SSM4567_DAC_MUTE;
	else
		val = 0;

	return regmap_update_bits(ssm4567->regmap, SSM4567_REG_DAC,
			SSM4567_DAC_MUTE, val);
}

static int ssm4567_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct ssm4567_priv *ssm4567 = snd_soc_codec_get_drvdata(codec_dai->codec);
	unsigned int ctrl1 = 0;
	bool invert_fclk;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		invert_fclk = false;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		ctrl1 |= SSM4567_SAI_CTRL1_BCLK;
		invert_fclk = false;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		ctrl1 |= SSM4567_SAI_CTRL1_FSYNC;
		invert_fclk = true;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		ctrl1 |= SSM4567_SAI_CTRL1_BCLK;
		invert_fclk = true;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		ctrl1 |= SSM4567_SAI_CTRL1_LJ;
		invert_fclk = !invert_fclk;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		ctrl1 |= SSM4567_SAI_CTRL1_TDM;
		break;
	case SND_SOC_DAIFMT_PDM:
		ctrl1 |= SSM4567_SAI_CTRL1_PDM;
		break;
	default:
		return -EINVAL;
	}

	if (invert_fclk)
		ctrl1 |= SSM4567_SAI_CTRL1_FSYNC;

	return regmap_write(ssm4567->regmap, SSM4567_REG_SAI_CTRL1, ctrl1);
}

static int ssm4567_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	struct ssm4567_priv *ssm4567 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) {
			ret = regmap_update_bits(ssm4567->regmap,
				SSM4567_REG_POWER, SSM4567_POWER_SPWDN, 0);
		}
		break;
	case SND_SOC_BIAS_OFF:
		ret = regmap_update_bits(ssm4567->regmap, SSM4567_REG_POWER,
			SSM4567_POWER_SPWDN, SSM4567_POWER_SPWDN);
		break;
	}

	if (ret)
		return ret;

	codec->dapm.bias_level = level;
	return 0;
}

static int ssm4567_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
	unsigned int rx_mask, int slots, int width)
{
	struct ssm4567_priv *ssm4567 = snd_soc_codec_get_drvdata(dai->codec);
	unsigned int blcks;
	int slot;
	int ret;

	if (tx_mask == 0)
		return -EINVAL;

	if (rx_mask && rx_mask != tx_mask)
		return -EINVAL;

	slot = ffs(tx_mask) - 1;
	if (tx_mask != BIT(slot))
		return -EINVAL;

	switch (width) {
	case 32:
		blcks = SSM4567_SAI_CTRL1_TDM_BLCKS_32;
		break;
	case 48:
		blcks = SSM4567_SAI_CTRL1_TDM_BLCKS_48;
		break;
	case 64:
		blcks = SSM4567_SAI_CTRL1_TDM_BLCKS_64;
		break;
	default:
		return -EINVAL;
	}

	ret = regmap_update_bits(ssm4567->regmap, SSM4567_REG_SAI_CTRL2,
		SSM4567_SAI_CTRL2_TDM_SLOT_MASK,
		SSM4567_SAI_CTRL2_TDM_SLOT(slot));
	if (ret)
		return ret;

	return regmap_update_bits(ssm4567->regmap, SSM4567_REG_SAI_CTRL1,
		SSM4567_SAI_CTRL1_TDM_BLCKS_MASK, blcks);
}

#define SSM4567_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | \
	SNDRV_PCM_FMTBIT_S32)

static const struct snd_soc_dai_ops ssm4567_dai_ops = {
	.hw_params	= ssm4567_hw_params,
	.digital_mute	= ssm4567_mute,
	.set_fmt	= ssm4567_set_dai_fmt,
	.set_tdm_slot	= ssm4567_set_tdm_slot,
};

static struct snd_soc_dai_driver ssm4567_dai = {
	.name = "ssm4567-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SSM4567_FORMATS,
	},
	.ops = &ssm4567_dai_ops,
};

static int ssm4567_probe(struct snd_soc_codec *codec)
{
	struct ssm4567_priv *ssm4567 = snd_soc_codec_get_drvdata(codec);
	int ret;

	codec->control_data = ssm4567->regmap;
	ret = snd_soc_codec_set_cache_io(codec, 0, 0, SND_SOC_REGMAP);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	return regmap_update_bits(ssm4567->regmap, SSM4567_REG_POWER,
			SSM4567_POWER_APWDN_EN, 0);
}

static int ssm4567_remove(struct snd_soc_codec *codec)
{
	ssm4567_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_ssm4567 = {
	.probe = ssm4567_probe,
	.remove = ssm4567_remove,
	.set_bias_level = ssm4567_set_bias_level,
	.idle_bias_off = true,

	.controls = ssm4567_snd_controls,
	.num_controls = ARRAY_SIZE(ssm4567_snd_controls),
	.dapm_widgets = ssm4567_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(ssm4567_dapm_widgets),
	.dapm_routes = ssm4567_routes,
	.num_dapm_routes = ARRAY_SIZE(ssm4567_routes),
};

static bool ssm4567_register_volatile(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SSM4567_REG_SOFT_RESET:
	case SSM4567_REG_STATUS1:
	case SSM4567_REG_STATUS2:
	case SSM4567_REG_FAULT:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config ssm4567_regmap_config = {
	.val_bits = 8,
	.reg_bits = 8,

	.max_register = SSM4567_REG_BOOST_CTRL2,
	.volatile_reg = ssm4567_register_volatile,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults_raw = ssm4567_reg,
	.num_reg_defaults_raw = ARRAY_SIZE(ssm4567_reg),
};

static int ssm4567_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct ssm4567_priv *ssm4567;
	int ret;

	ssm4567 = devm_kzalloc(&i2c->dev, sizeof(struct ssm4567_priv),
				   GFP_KERNEL);
	if (ssm4567 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, ssm4567);

	ssm4567->regmap = devm_regmap_init_i2c(i2c, &ssm4567_regmap_config);
	if (IS_ERR(ssm4567->regmap))
		return PTR_ERR(ssm4567->regmap);

	regmap_write(ssm4567->regmap, SSM4567_REG_SOFT_RESET, 0x00);

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_ssm4567, &ssm4567_dai, 1);
	return ret;
}

static int ssm4567_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id ssm4567_i2c_ids[] = {
	{ "ssm4567", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ssm4567_i2c_id);

static struct i2c_driver ssm4567_driver = {
	.driver = {
		.name = "ssm4567",
		.owner = THIS_MODULE,
	},
	.probe = ssm4567_i2c_probe,
	.remove = ssm4567_i2c_remove,
	.id_table = ssm4567_i2c_ids,
};
module_i2c_driver(ssm4567_driver);

MODULE_DESCRIPTION("ASoC SSM4567 driver");
MODULE_AUTHOR("Lars-Peter Clausen <lars@metafoo.de>");
MODULE_LICENSE("GPL");
