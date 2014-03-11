/*
 * SSM4329 driver
 *
 * Copyright 2014 Analog Devices Inc.
 *  Author: Lars-Peter Clausen <lars@metafoo.de>
 *
 * Licensed under the GPL-2.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
//#include <linux/platform_data/ssm4329.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/gcd.h>

#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "ssm4329.h"

/* ADI Vendor ID */
#define SSM4329_REG_VENDOR_ID		0x4000
/* SSM4329 Device ID */
#define SSM4329_REG_DEVICE_ID1		0x4001
/* SSM4329 Device ID */
#define SSM4329_REG_DEVICE_ID2		0x4002
/* Revision Code */
#define SSM4329_REG_REVISION		0x4003
/* Analog blocks power control */
#define SSM4329_REG_ANA_PWR		0x4004
/* Digital blocks power control */
#define SSM4329_REG_DIG_PWR1		0x4005
/* Digital blocks power control */
#define SSM4329_REG_DIG_PWR2		0x4006
/* Master power control */
#define SSM4329_REG_CHIP_PWR		0x4007
/* Clock Control */
#define SSM4329_REG_CLK_CTRL1		0x4008
/* PLL Input Divider */
#define SSM4329_REG_CLK_CTRL2		0x4009
/* PLL Feedback Integer Divider (LSBs) */
#define SSM4329_REG_CLK_CTRL3		0x400A
/* PLL Feedback Integer Divider (MSBs) */
#define SSM4329_REG_CLK_CTRL4		0x400B
/* PLL Fractional numerator value (LSBs) */
#define SSM4329_REG_CLK_CTRL5		0x400C
/* PLL Fractional numerator value (MSBs) */
#define SSM4329_REG_CLK_CTRL6		0x400D
/* PLL Fractional denominator (LSBs) */
#define SSM4329_REG_CLK_CTRL7		0x400E
/* PLL Fractional denominator (MSBs) */
#define SSM4329_REG_CLK_CTRL8		0x400F
/* PLL Update */
#define SSM4329_REG_CLK_CTRL9		0x4010
/* Serial Port 1 Output Routing */
#define SSM4329_REG_ROUTE_SP1_1		0x4011
/* Serial Port 1 Output Routing */
#define SSM4329_REG_ROUTE_SP1_2		0x4012
/* Serial Port 2 Output Routing */
#define SSM4329_REG_ROUTE_SP2_1		0x4013
/* Serial Port 2 Output Routing */
#define SSM4329_REG_ROUTE_SP2_2		0x4014
/* DAC and Interpolation path Output Routing */
#define SSM4329_REG_ROUTE_DAC_INT	0x4015
/* Input SRC Routing */
#define SSM4329_REG_ROUTE_SRC_IN	0x4016
/* Output SRC Routing */
#define SSM4329_REG_ROUTE_SRC_OUT	0x4017
/* Serial Port X Control */
#define SSM4329_REG_SPT_CTRL1(x)	(0x401A + (x))
/* Serial Port X Contro */
#define SSM4329_REG_SPT_CTRL2(x)	(0x401B + (x))
/* Serial Port X Clock ontrol */
#define SSM4329_REG_SPT_CLOCKING(x)	(0x401C + (x))
/* Serial Port X Input lot to Channel Mapping */
#define SSM4329_REG_SPT_INPUTS1(x)	(0x401D + (x))
/* Serial Port X Input lot to Channel Mapping */
#define SSM4329_REG_SPT_INPUTS2(x)	(0x401E + (x))
/* Serial Port X OutputChannel to Slot Mapping */
#define SSM4329_REG_SPT_OUTPUTS1(x)	(0x401F + (x))
/* Serial Port X OutputChannel to Slot Mapping */
#define SSM4329_REG_SPT_OUTPUTS2(x)	(0x4020 + (x))
/* Serial Port X OutputChannel to Slot Mapping */
#define SSM4329_REG_SPT_OUTPUTS3(x)	(0x4021 + (x))
/* Serial Port X OutputChannel to Slot Mapping */
#define SSM4329_REG_SPT_OUTPUTS4(x)	(0x4022 + (x))
/* Class-D Amp and Output Sense Control */
#define SSM4329_REG_AMP_SNS_CTRL	0x402C
/* DAC Control */
#define SSM4329_REG_DAC_CTRL		0x402D
/* DAC Volume Control */
#define SSM4329_REG_DAC_VOLUME		0x402E
/* DAC High Rate Clip Point */
#define SSM4329_REG_DAC_CLIP		0x402F
/* DAC and Interp path Control */
#define SSM4329_REG_DAC_INTERP_CTRL	0x4030
/* Interp path Volume Control */
#define SSM4329_REG_INTERP_VOLUME	0x4031
/* Interp High Rate Clip Point */
#define SSM4329_REG_INTERP_CLIP		0x4032
/* ADC Control */
#define SSM4329_REG_ADC_CTRL		0x4033
/* ADC Volume Control */
#define SSM4329_REG_ADC_VOLUME		0x4034
/* SRC rate control */
#define SSM4329_REG_SRC_CTRL		0x4035
/* SigmaDSP Program Initialization */
#define SSM4329_REG_SDSP_CTRL1		0x4036
/* SimgaDSP Go Control */
#define SSM4329_REG_SDSP_CTRL2		0x4037
/* SigmaDSP Run Control */
#define SSM4329_REG_SDSP_CTRL3		0x4038
/* SigmaDSP Watchdog Control */
#define SSM4329_REG_SDSP_CTRL4		0x4039
/* SigmaDSP Watchdog Value */
#define SSM4329_REG_SDSP_CTRL5		0x403A
/* SigmaDSP Watchdog Value */
#define SSM4329_REG_SDSP_CTRL6		0x403B
/* SigmaDSP Watchdog Value */
#define SSM4329_REG_SDSP_CTRL7		0x403C
/* SigmaDSP Modulo Data Memory Start Location */
#define SSM4329_REG_SDSP_CTRL8		0x403D
/* SigmaDSP Modulo Data Memory Start Location */
#define SSM4329_REG_SDSP_CTRL9		0x403E
/* SigmaDSP Go divide rate. */
#define SSM4329_REG_SDSP_CTRL10		0x403F
/* SigmaDSP Go divide rate. */
#define SSM4329_REG_SDSP_CTRL11		0x4040
/* GPIO, IRQ, MCLKO pin control */
#define SSM4329_REG_PIN_FUNC		0x4041
/* GPIO output setting */
#define SSM4329_REG_GPIO_OUT_SETTING	0x4042
/* Mask causes of IRQ1 */
#define SSM4329_REG_IRQ1_MASK1		0x4043
/* Mask causes of IRQ1 */
#define SSM4329_REG_IRQ1_MASK2		0x4044
/* Mask causes of IRQ2 */
#define SSM4329_REG_IRQ2_MASK1		0x4045
/* Mask causes of IRQ2 */
#define SSM4329_REG_IRQ2_MASK2		0x4046
/* Clear Interrupts */
#define SSM4329_REG_IRQ_CLEAR		0x4047
/* MCLK Output Control */
#define SSM4329_REG_MCLKO_CTRL		0x4048
/* External Amplifier Control Enable */
#define SSM4329_REG_EAC1		0x404A
/* External Amplifier Power Control */
#define SSM4329_REG_EAC2		0x404B
/* External Amplifier Sense and Class-D Control */
#define SSM4329_REG_EAC3		0x404C
/* External Amplifier DAC Control */
#define SSM4329_REG_EAC4		0x404D
/* External Amp Control Address */
#define SSM4329_REG_EAC_ADDR		0x404E
/* External Amp Control Write Data */
#define SSM4329_REG_EAC_WR_DATA		0x404F
/* External Amp Read/Write Control */
#define SSM4329_REG_EAC_RW_CTRL		0x4050
/* Pad Control 1 */
#define SSM4329_REG_PAD_CTRL1		0x4051
/* Pad Control 2 */
#define SSM4329_REG_PAD_CTRL2		0x4052
/* Fault Auto Recovery */
#define SSM4329_REG_FAULT_RECOV		0x4053
/* VBAT Warning Level */
#define SSM4329_REG_VBAT_WARN_LEVEL	0x4054
/* Boost DC/DC converter control registers */
#define SSM4329_REG_BST_CTRL		0x4055
/* Software reset, not including control registers */
#define SSM4329_REG_SOFT_RESET		0x4056
/* Software reset of entire IC. */
#define SSM4329_REG_SOFT_FULL_RESET	0x4057
/* Read 8-bit VBAT sense value */
#define SSM4329_REG_VBAT		0x4058
/* Fault Status */
#define SSM4329_REG_FAULT_STATUS	0x4059
/* SigmaDSP and PLL Lock Status */
#define SSM4329_REG_PLL_SDSP_STATUS	0x405A
/* External Amplifier Control Status */
#define SSM4329_REG_EAC_STATUS		0x405B
/* External Amplifier Control Read Data */
#define SSM4329_REG_EAC_RD_DATA		0x405C
/* IRQ1 Status */
#define SSM4329_REG_IRQ1_STATUS1	0x405D
/* IRQ1 Status */
#define SSM4329_REG_IRQ1_STATUS2	0x405E
/* IRQ2 Status */
#define SSM4329_REG_IRQ2_STATUS1	0x405F
/* IRQ2 Status */
#define SSM4329_REG_IRQ2_STATUS2	0x4060
/* GPIO input reading */
#define SSM4329_REG_GPIO_IN_READING	0x4061


#define SSM4329_DAC_CTRL_DAC_MUTE BIT(6)
#define SSM4329_ADC_CTRL_ADC_MUTE BIT(6)

#define SSM4329_REG_CLK_CTRL9_PLL_UPDATE BIT(0)

#define SSM4329_REG_CHIP_PWR_CHIP_PWDN BIT(0)

#define SSM4329_REG_SPT_CTRL1_DATA_FORMAT_MASK		(0x7 << 4)
#define SSM4329_REG_SPT_CTRL1_DATA_FORMAT_I2S		(0x0 << 4)
#define SSM4329_REG_SPT_CTRL1_DATA_FORMAT_LJ		(0x1 << 4)
#define SSM4329_REG_SPT_CTRL1_DATA_FORMAT_DLEAY8	(0x2 << 4)
#define SSM4329_REG_SPT_CTRL1_DATA_FORMAT_DELAY12	(0x3 << 4)
#define SSM4329_REG_SPT_CTRL1_DATA_FORMAT_DELAY16	(0x4 << 4)
#define SSM4329_REG_SPT_CTRL1_SLOT_WIDTH_MASK		(0x3 << 2)
#define SSM4329_REG_SPT_CTRL1_SLOT_WIDTH_32		(0x0 << 2)
#define SSM4329_REG_SPT_CTRL1_SLOT_WIDTH_16		(0x1 << 2)
#define SSM4329_REG_SPT_CTRL1_SLOT_WIDTH_24		(0x2 << 2)
#define SSM4329_REG_SPT_CTRL1_MODE_MASK			(0x3 << 0)
#define SSM4329_REG_SPT_CTRL1_MODE_STEREO		(0x0 << 0)
#define SSM4329_REG_SPT_CTRL1_MODE_TDM			(0x1 << 0)
#define SSM4329_REG_SPT_CTRL1_MODE_MONO			(0x2 << 0)


#define SSM4329_REG_SPT_CTRL2_TRI_STATE BIT(4)

#define SSM4329_REG_SPT_CLOCKING_LRCLK_POL	BIT(7)
#define SSM4329_REG_SPT_CLOCKING_LRCLK_SRC_MASK	(0x7 << 4)
#define SSM4329_REG_SPT_CLOCKING_LRCLK_SRC_EXT	(0x0 << 4)
#define SSM4329_REG_SPT_CLOCKING_LRCLK_SRC_48	(0x1 << 4)
#define SSM4329_REG_SPT_CLOCKING_LRCLK_SRC_96	(0x2 << 4)
#define SSM4329_REG_SPT_CLOCKING_LRCLK_SRC_192	(0x3 << 4)
#define SSM4329_REG_SPT_CLOCKING_LRCLK_SRC_12	(0x4 << 4)
#define SSM4329_REG_SPT_CLOCKING_LRCLK_SRC_24	(0x5 << 4)
#define SSM4329_REG_SPT_CLOCKING_LRCLK_SRC_8	(0x6 << 4)
#define SSM4329_REG_SPT_CLOCKING_LRCLK_SRC_16	(0x7 << 4)
#define SSM4329_REG_SPT_CLOCKING_BCLK_POL	BIT(3)
#define SSM4329_REG_SPT_CLOCKING_BCLK_SRC_MASK	(0x7 << 0)
#define SSM4329_REG_SPT_CLOCKING_BCLK_SRC_EXT	(0x0 << 0)
#define SSM4329_REG_SPT_CLOCKING_BCLK_SRC_64	(0x1 << 0)
#define SSM4329_REG_SPT_CLOCKING_BCLK_SRC_128	(0x2 << 0)
#define SSM4329_REG_SPT_CLOCKING_BCLK_SRC_256	(0x3 << 0)
#define SSM4329_REG_SPT_CLOCKING_BCLK_SRC_512	(0x4 << 0)

struct ssm4329 {
	struct regmap *regmap;
	bool right_j;
	unsigned int sysclk;
	void (*switch_mode)(struct device *dev);
};

static const struct reg_default ssm4329_reg_defaults[] = {
	{ SSM4329_REG_ANA_PWR, 0x04 },
	{ SSM4329_REG_DIG_PWR1, 0xf8 },
	{ SSM4329_REG_DIG_PWR2, 0x0f },
	{ SSM4329_REG_CHIP_PWR, 0x01 },
	{ SSM4329_REG_CLK_CTRL1, 0x01 },
	{ SSM4329_REG_CLK_CTRL2, 0x00 },
	{ SSM4329_REG_CLK_CTRL3, 0x00 },
	{ SSM4329_REG_CLK_CTRL4, 0x08 },
	{ SSM4329_REG_CLK_CTRL5, 0x00 },
	{ SSM4329_REG_CLK_CTRL6, 0x00 },
	{ SSM4329_REG_CLK_CTRL7, 0x00 },
	{ SSM4329_REG_CLK_CTRL8, 0x00 },
	{ SSM4329_REG_CLK_CTRL9, 0x00 },
	{ SSM4329_REG_ROUTE_SP1_1, 0x11 },
	{ SSM4329_REG_ROUTE_SP1_2, 0x00 },
	{ SSM4329_REG_ROUTE_SP2_1, 0x02 },
	{ SSM4329_REG_ROUTE_SP2_2, 0x00 },
	{ SSM4329_REG_ROUTE_DAC_INT, 0x00 },
	{ SSM4329_REG_ROUTE_SRC_IN, 0x00 },
	{ SSM4329_REG_ROUTE_SRC_OUT, 0x00 },
	{ SSM4329_REG_SPT_CTRL1(0), 0x00 },
	{ SSM4329_REG_SPT_CTRL2(1), 0x00 },
	{ SSM4329_REG_SPT_CLOCKING(2), 0x00 },
	{ SSM4329_REG_SPT_INPUTS1(3), 0x10 },
	{ SSM4329_REG_SPT_INPUTS2(4), 0x32 },
	{ SSM4329_REG_SPT_OUTPUTS1(5), 0x00 },
	{ SSM4329_REG_SPT_OUTPUTS2(6), 0x01 },
	{ SSM4329_REG_SPT_OUTPUTS3(7), 0x02 },
	{ SSM4329_REG_SPT_OUTPUTS4(8), 0x03 },
	{ SSM4329_REG_SPT_CTRL1(0), 0x00 },
	{ SSM4329_REG_SPT_CTRL2(1), 0x00 },
	{ SSM4329_REG_SPT_CLOCKING(2), 0x00 },
	{ SSM4329_REG_SPT_INPUTS1(3), 0x10 },
	{ SSM4329_REG_SPT_INPUTS2(4), 0x32 },
	{ SSM4329_REG_SPT_OUTPUTS1(5), 0x00 },
	{ SSM4329_REG_SPT_OUTPUTS2(6), 0x01 },
	{ SSM4329_REG_SPT_OUTPUTS3(7), 0x02 },
	{ SSM4329_REG_SPT_OUTPUTS4(8), 0x03 },
	{ SSM4329_REG_AMP_SNS_CTRL, 0x41 },
	{ SSM4329_REG_DAC_CTRL, 0x62 },
	{ SSM4329_REG_DAC_VOLUME, 0x40 },
	{ SSM4329_REG_DAC_CLIP, 0xff },
	{ SSM4329_REG_DAC_INTERP_CTRL, 0x00 },
	{ SSM4329_REG_INTERP_VOLUME, 0x40 },
	{ SSM4329_REG_INTERP_CLIP, 0xff },
	{ SSM4329_REG_ADC_CTRL, 0x04 },
	{ SSM4329_REG_ADC_VOLUME, 0x40 },
	{ SSM4329_REG_SRC_CTRL, 0x00 },
	{ SSM4329_REG_SDSP_CTRL1, 0x00 },
	{ SSM4329_REG_SDSP_CTRL2, 0x00 },
	{ SSM4329_REG_SDSP_CTRL3, 0x00 },
	{ SSM4329_REG_SDSP_CTRL4, 0x00 },
	{ SSM4329_REG_SDSP_CTRL5, 0x00 },
	{ SSM4329_REG_SDSP_CTRL6, 0x00 },
	{ SSM4329_REG_SDSP_CTRL7, 0x00 },
	{ SSM4329_REG_SDSP_CTRL8, 0x07 },
	{ SSM4329_REG_SDSP_CTRL9, 0xf4 },
	{ SSM4329_REG_SDSP_CTRL10, 0x08 },
	{ SSM4329_REG_SDSP_CTRL11, 0x00 },
	{ SSM4329_REG_PIN_FUNC, 0x00 },
	{ SSM4329_REG_GPIO_OUT_SETTING, 0x00 },
	{ SSM4329_REG_IRQ1_MASK1, 0x7f },
	{ SSM4329_REG_IRQ1_MASK2, 0x08 },
	{ SSM4329_REG_IRQ2_MASK1, 0x7f },
	{ SSM4329_REG_IRQ2_MASK2, 0x08 },
	{ SSM4329_REG_IRQ_CLEAR, 0x00 },
	{ SSM4329_REG_MCLKO_CTRL, 0x00 },
	{ SSM4329_REG_EAC1, 0x00 },
	{ SSM4329_REG_EAC2, 0x81 },
	{ SSM4329_REG_EAC3, 0x09 },
	{ SSM4329_REG_EAC4, 0x32 },
	{ SSM4329_REG_EAC_ADDR, 0x00 },
	{ SSM4329_REG_EAC_WR_DATA, 0x00 },
	{ SSM4329_REG_EAC_RW_CTRL, 0x00 },
	{ SSM4329_REG_PAD_CTRL1, 0x00 },
	{ SSM4329_REG_PAD_CTRL2, 0x00 },
	{ SSM4329_REG_FAULT_RECOV, 0x00 },
	{ SSM4329_REG_VBAT_WARN_LEVEL, 0x00 },
	{ SSM4329_REG_BST_CTRL, 0x50 },
};

static const DECLARE_TLV_DB_MINMAX_MUTE(ssm4329_volume_tlv, -7125, 2400);
static const DECLARE_TLV_DB_LINEAR(ssm4329_clip_tlv, -4816, 0);

static const struct snd_soc_dapm_widget ssm4329_dapm_widgets[] = {
	/*SND_SOC_DAPM_SUPPLY("Amplifier enable", SSM4329_REG_ANA_PWR, 0, 1,
		NULL, 0),
	SND_SOC_DAPM_SUPPLY("Boost Converter enable", SSM4329_REG_ANA_PWR, 1, 1,
		NULL, 0),
	SND_SOC_DAPM_SUPPLY("Serial Port ", SSM4329_REG_ANA_PWR, 1, 1,
		NULL, 0),
	SND_SOC_DAPM_SUPPLY("Boost Converter enable", SSM4329_REG_ANA_PWR, 1, 1,
		NULL, 0),*/

	SND_SOC_DAPM_SUPPLY("DSP", SSM4329_REG_DIG_PWR1, 0, 1, NULL, 0),
	SND_SOC_DAPM_AIF_IN("SP1 IN", NULL, 0, SSM4329_REG_DIG_PWR1, 1, 1),
	SND_SOC_DAPM_AIF_OUT("SP1 OUT", NULL, 0, SSM4329_REG_DIG_PWR1, 2, 1),
	SND_SOC_DAPM_AIF_IN("SP2 IN", NULL, 0, SSM4329_REG_DIG_PWR1, 3, 1),
	SND_SOC_DAPM_AIF_OUT("SP2 OUT", NULL, 0, SSM4329_REG_DIG_PWR1, 4, 1),
	SND_SOC_DAPM_SUPPLY("INTERP", SSM4329_REG_DIG_PWR1, 5, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY("DAC AEC", SSM4329_REG_DIG_PWR1, 6, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY("INTERP AEC", SSM4329_REG_DIG_PWR1, 7, 1, NULL, 0),

	SND_SOC_DAPM_OUTPUT("OUT"),
	SND_SOC_DAPM_INPUT("AIN"),
};

static const struct snd_soc_dapm_route ssm4329_dapm_routes[] = {
	{ "ADC", NULL, "AIN" },
	{ "OUT", NULL, "DAC" },

	{ "OUT", NULL, "Amplifier enable" },

	{ "Playback SP1", NULL, "SP1 IN" },
	{ "Capture SP1", NULL, "SP1 OUT" },
	{ "Playback SP2", NULL, "SP2 IN" },
	{ "Capture SP2", NULL, "SP2 OUT" },
};

static const struct snd_kcontrol_new ssm4329_snd_controls[] = {
	SOC_SINGLE_TLV("DAC Playback Volume", SSM4329_REG_DAC_VOLUME,
		0, 0xff, 1, ssm4329_volume_tlv),
	SOC_SINGLE_TLV("DAC Clip Point Volume", SSM4329_REG_DAC_CLIP,
		0, 0xff, 1, ssm4329_clip_tlv),
	SOC_SINGLE_TLV("Interpolator Playback Volume",
		SSM4329_REG_INTERP_VOLUME, 0, 0xff, 1, ssm4329_volume_tlv),
	SOC_SINGLE_TLV("Interpolator Clip Point Volume",
		SSM4329_REG_INTERP_CLIP, 0, 0xff, 1, ssm4329_clip_tlv),

	SOC_SINGLE_TLV("ADC Capture Volume", SSM4329_REG_ADC_VOLUME,
		0, 0xff, 1, ssm4329_volume_tlv),

	SOC_SINGLE("DAC High Pass Filter Switch", SSM4329_REG_DAC_CTRL,
		5, 1, 0),
	SOC_SINGLE("DAC Low-power Switch", SSM4329_REG_DAC_CTRL, 4, 1, 0),

	SOC_SINGLE("ADC High Pass Filter Switch", SSM4329_REG_ADC_CTRL,
		5, 1, 0),
	SOC_SINGLE("ADC Low-power Switch", SSM4329_REG_ADC_CTRL, 4, 1, 0),

	SOC_SINGLE("Interpolator Playback Switch", SSM4329_REG_DAC_INTERP_CTRL,
		0, 1, 1),
};

static int ssm4329_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	return 0;
}

static int ssm4329_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	struct ssm4329 *ssm4329 = snd_soc_codec_get_drvdata(codec);

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		regmap_update_bits(ssm4329->regmap, SSM4329_REG_CHIP_PWR,
			SSM4329_REG_CHIP_PWR_CHIP_PWDN, 0x00);
		break;
	case SND_SOC_BIAS_OFF:
		regmap_update_bits(ssm4329->regmap, SSM4329_REG_CHIP_PWR,
			SSM4329_REG_CHIP_PWR_CHIP_PWDN,
			SSM4329_REG_CHIP_PWR_CHIP_PWDN);
		break;
	default:
		break;
	}

	return 0;
}

static int ssm4329_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
	unsigned int rx_mask, int slots, int width)
{
	return 0;
}

static int ssm4329_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	struct ssm4329 *ssm4329 = snd_soc_dai_get_drvdata(dai);
	unsigned int val;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (mute)
			val = SSM4329_DAC_CTRL_DAC_MUTE;
		else
			val = 0;
		regmap_update_bits(ssm4329->regmap, SSM4329_REG_DAC_CTRL,
			SSM4329_DAC_CTRL_DAC_MUTE, val);
	} else {
		if (mute)
			val = SSM4329_ADC_CTRL_ADC_MUTE;
		else
			val = 0;
		regmap_update_bits(ssm4329->regmap, SSM4329_REG_ADC_CTRL,
			SSM4329_ADC_CTRL_ADC_MUTE, val);
	}

	return 0;
}

static int ssm4329_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	return 0;
}

static int ssm4329_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	return 0;
}

static int ssm4329_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct ssm4329 *ssm4329 = snd_soc_dai_get_drvdata(dai);
	unsigned int val;

	if (tristate)
		val = SSM4329_REG_SPT_CTRL2_TRI_STATE;
	else
		val = 0;

	return regmap_update_bits(ssm4329->regmap,
		SSM4329_REG_SPT_CTRL2(dai->id),
		SSM4329_REG_SPT_CTRL2_TRI_STATE, val);
}

static int ssm4329_set_pll(struct snd_soc_codec *codec, int pll_id,
	int source, unsigned int freq_in, unsigned int freq_out)
{
	struct ssm4329 *ssm4329 = snd_soc_codec_get_drvdata(codec);
	unsigned int div;
	unsigned int r, n, m, i, j;

	if (pll_id != SSM4329_PLL)
	    return -EINVAL;
	
	switch (source) {
	case SSM4329_PLL_SRC_MCLKIN:
	case SSM4329_PLL_SRC_FSYNC1:
	case SSM4329_PLL_SRC_BCLK1:
	case SSM4329_PLL_SRC_FSYNC2:
	case SSM4329_PLL_SRC_BCLK2:
		break;
	default:
		return -EINVAL;
	}

	if (freq_in < 8000 || freq_in > 27000000)
		return -EINVAL;

	if (!freq_out) {
		r = 0;
		n = 0;
		m = 0;
		div = 0;
	} else {
		div = DIV_ROUND_UP(freq_in, 13500000);
		freq_in /= div;

		if (freq_out % freq_in != 0) {
			r = freq_out / freq_in;
			i = freq_out % freq_in;
			j = gcd(i, freq_in);
			n = i / j;
			m = freq_in / j;
		} else {
			r = freq_out / freq_in;
			n = 0;
			m = 0;
		}

		if (n > 0xffff || m > 0xffff || div == 0 || div > 7 || r > 0x3fff)
			return -EINVAL;
	}

	if (m != 0)
		source |= 1 << 4;

	regmap_write(ssm4329->regmap, SSM4329_REG_CLK_CTRL1, source);
	regmap_write(ssm4329->regmap, SSM4329_REG_CLK_CTRL2, div);
	regmap_write(ssm4329->regmap, SSM4329_REG_CLK_CTRL3, r & 0xff);
	regmap_write(ssm4329->regmap, SSM4329_REG_CLK_CTRL4, (r >> 8) & 0xff);
	regmap_write(ssm4329->regmap, SSM4329_REG_CLK_CTRL5, n & 0xff);
	regmap_write(ssm4329->regmap, SSM4329_REG_CLK_CTRL6, (n >> 8) & 0xff);
	regmap_write(ssm4329->regmap, SSM4329_REG_CLK_CTRL7, m & 0xff);
	regmap_write(ssm4329->regmap, SSM4329_REG_CLK_CTRL8, (m >> 8) & 0xff);

	/* Trigger PLL update */
	regmap_write(ssm4329->regmap, SSM4329_REG_CLK_CTRL9,
		SSM4329_REG_CLK_CTRL9_PLL_UPDATE);
	regmap_write(ssm4329->regmap, SSM4329_REG_CLK_CTRL9, 0);

	return 0;
}

static const struct snd_soc_dai_ops ssm4329_dai_ops = {
	.startup	= ssm4329_startup,
	.hw_params	= ssm4329_hw_params,
	.mute_stream	= ssm4329_mute,
	.set_fmt	= ssm4329_set_dai_fmt,
	.set_tdm_slot	= ssm4329_set_tdm_slot,
	.set_tristate	= ssm4329_set_tristate,
};

static struct snd_soc_dai_driver ssm4329_dais[] = {
	{
		.name = "ssm4329-sp1",
		.id = 0,
		.playback = {
			.stream_name = "SP1 Playback",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE,
			.sig_bits = 24,
		},
		.capture = {
			.stream_name = "SP1 Capture",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE,
			.sig_bits = 24,
		},
		.ops = &ssm4329_dai_ops,
	}, {
		.name = "ssm4329-sp2",
		.id = 1,
		.playback = {
			.stream_name = "SP2 Playback",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE,
			.sig_bits = 24,
		},
		.capture = {
			.stream_name = "SP2 Capture",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE,
			.sig_bits = 24,
		},
		.ops = &ssm4329_dai_ops,
	},
};

static const struct snd_soc_codec_driver ssm4329_codec_driver = {
	.set_bias_level = ssm4329_set_bias_level,
	.set_pll = ssm4329_set_pll,
	.idle_bias_off = true,

	.controls = ssm4329_snd_controls,
	.num_controls = ARRAY_SIZE(ssm4329_snd_controls),
	.dapm_widgets = ssm4329_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(ssm4329_dapm_widgets),
	.dapm_routes = ssm4329_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(ssm4329_dapm_routes),
};

int ssm4329_probe(struct device *dev, struct regmap *regmap,
	void (*switch_mode)(struct device *dev))
{
	unsigned int a, b, c, d;
	struct ssm4329 *ssm4329;

	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	regmap_read(regmap, SSM4329_REG_VENDOR_ID, &a);
	regmap_read(regmap, SSM4329_REG_DEVICE_ID1, &b);
	regmap_read(regmap, SSM4329_REG_DEVICE_ID2, &c);
	regmap_read(regmap, SSM4329_REG_REVISION, &d);
	dev_info(dev, "ssm4329 version: %.2x.%.2x%.2x.%2.x", a, b, c, d);

	ssm4329 = devm_kzalloc(dev, sizeof(*ssm4329), GFP_KERNEL);
	if (ssm4329 == NULL)
		return -ENOMEM;

	dev_set_drvdata(dev, ssm4329);

	ssm4329->regmap = regmap;
	ssm4329->switch_mode = switch_mode;

	return snd_soc_register_codec(dev, &ssm4329_codec_driver,
			ssm4329_dais, 2);
}
EXPORT_SYMBOL_GPL(ssm4329_probe);

static bool ssm4329_register_readable(struct device *dev, unsigned int reg)
{
	if (reg < 0x4000)
		return false;
	return true;
}

static bool ssm4329_register_volatile(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SSM4329_REG_VENDOR_ID:
	case SSM4329_REG_DEVICE_ID1:
	case SSM4329_REG_DEVICE_ID2:
	case SSM4329_REG_REVISION:
	case SSM4329_REG_VBAT:
	case SSM4329_REG_FAULT_STATUS:
	case SSM4329_REG_PLL_SDSP_STATUS:
	case SSM4329_REG_EAC_STATUS:
	case SSM4329_REG_EAC_RD_DATA:
	case SSM4329_REG_IRQ1_STATUS1:
	case SSM4329_REG_IRQ1_STATUS2:
	case SSM4329_REG_IRQ2_STATUS1:
	case SSM4329_REG_IRQ2_STATUS2:
	case SSM4329_REG_GPIO_IN_READING:
	    return true;
	}

	return false;
}

const struct regmap_config ssm4329_regmap_config = {
	.max_register = SSM4329_REG_GPIO_IN_READING,
	.volatile_reg = ssm4329_register_volatile,
	.readable_reg = ssm4329_register_readable,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = ssm4329_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(ssm4329_reg_defaults),
};
EXPORT_SYMBOL_GPL(ssm4329_regmap_config);

MODULE_DESCRIPTION("ASoC SSM4329 driver");
MODULE_AUTHOR("Lars-Peter Clausen <lars@metafoo.de>");
MODULE_LICENSE("GPL");
