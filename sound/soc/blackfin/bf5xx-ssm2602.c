/*
 * File:         sound/soc/blackfin/bf5xx-ssm4567.c
 * Author:       Cliff Cai <Cliff.Cai@analog.com>
 *
 * Created:      Tue June 06 2008
 * Description:  board driver for SSM4567 sound chip
 *
 * Modified:
 *               Copyright 2008 Analog Devices Inc.
 *
 * Bugs:         Enter bugs at http://blackfin.uclinux.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <asm/dma.h>
#include <asm/portmux.h>
#include <linux/gpio.h>
#include "bf5xx-sport.h"

static struct snd_soc_card bf5xx_ssm4567;

/* CODEC is master for BCLK and LRC in this configuration. */
#define BF5XX_SSM4567_DAIFMT (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | \
				SND_SOC_DAIFMT_CBM_CFM)

static struct snd_soc_dai_link bf5xx_ssm4567_dai[] = {
	{
		.name = "ssm4567",
		.stream_name = "SSM4567",
		.cpu_dai_name = "bfin-i2s.0",
		.codec_dai_name = "ssm4567-hifi",
		.platform_name = "bfin-i2s-pcm-audio",
		.codec_name = "ssm4567.0-0034",
		.dai_fmt = BF5XX_SSM4567_DAIFMT,
	},
	{
		.name = "ssm4567",
		.stream_name = "SSM4567",
		.cpu_dai_name = "bfin-i2s.1",
		.codec_dai_name = "ssm4567-hifi",
		.platform_name = "bfin-i2s-pcm-audio",
		.codec_name = "ssm4567.0-0034",
		.dai_fmt = BF5XX_SSM4567_DAIFMT,
	},
};

static struct snd_soc_card bf5xx_ssm4567 = {
	.name = "bfin-ssm4567",
	.owner = THIS_MODULE,
	.dai_link = &bf5xx_ssm4567_dai[CONFIG_SND_BF5XX_SPORT_NUM],
	.num_links = 1,
};

static struct platform_device *bf5xx_ssm4567_snd_device;

static int __init bf5xx_ssm4567_init(void)
{
	int ret;

	pr_debug("%s enter\n", __func__);
	bf5xx_ssm4567_snd_device = platform_device_alloc("soc-audio", -1);
	if (!bf5xx_ssm4567_snd_device)
		return -ENOMEM;

	platform_set_drvdata(bf5xx_ssm4567_snd_device, &bf5xx_ssm4567);
	ret = platform_device_add(bf5xx_ssm4567_snd_device);

	if (ret)
		platform_device_put(bf5xx_ssm4567_snd_device);

	return ret;
}

static void __exit bf5xx_ssm4567_exit(void)
{
	pr_debug("%s enter\n", __func__);
	platform_device_unregister(bf5xx_ssm4567_snd_device);
}

module_init(bf5xx_ssm4567_init);
module_exit(bf5xx_ssm4567_exit);

/* Module information */
MODULE_AUTHOR("Cliff Cai");
MODULE_DESCRIPTION("ALSA SoC SSM4567 BF527-EZKIT");
MODULE_LICENSE("GPL");

