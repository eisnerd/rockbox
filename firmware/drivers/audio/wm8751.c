/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Driver for WM8751 audio codec
 *
 * Based on code from the ipodlinux project - http://ipodlinux.org/
 * Adapted for Rockbox in December 2005
 *
 * Original file: linux/arch/armnommu/mach-ipod/audio.c
 *
 * Copyright (c) 2003-2005 Bernard Leach (leachbj@bouncycastle.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "kernel.h"
#include "wmcodec.h"
#include "audio.h"
#include "audiohw.h"
#include "system.h"

const struct sound_settings_info audiohw_settings[] = {
    [SOUND_VOLUME]        = {"dB", 0,  1, -74,   6, -25},
#ifdef USE_ADAPTIVE_BASS
    [SOUND_BASS]          = {"",   0,  1,   0,  15,   0},
#else
    [SOUND_BASS]          = {"dB", 1, 15, -60,  90,   0},
#endif
    [SOUND_TREBLE]        = {"dB", 1, 15, -60,  90,   0},
    [SOUND_BALANCE]       = {"%",  0,  1,-100, 100,   0},
    [SOUND_CHANNELS]      = {"",   0,  1,   0,   5,   0},
    [SOUND_STEREO_WIDTH]  = {"%",  0,  5,   0, 250, 100},
};

/* Flags used in combination with settings */

/* use zero crossing to reduce clicks during volume changes */
#define LOUT1_BITS      (LOUT1_LO1ZC)
/* latch left volume first then update left+right together */
#define ROUT1_BITS      (ROUT1_RO1ZC | ROUT1_RO1VU)
#define LOUT2_BITS      (LOUT2_LO2ZC)
#define ROUT2_BITS      (ROUT2_RO2ZC | ROUT2_RO2VU)
/* We use linear bass control with 200 Hz cutoff */
#ifdef USE_ADAPTIVE_BASE
#define BASSCTRL_BITS   (BASSCTRL_BC | BASSCTRL_BB)
#else
#define BASSCTRL_BITS   (BASSCTRL_BC)
#endif
/* We use linear treble control with 4 kHz cutoff */
#define TREBCTRL_BITS   (TREBCTRL_TC)

static int prescaler = 0;

/* convert tenth of dB volume (-730..60) to master volume register value */
int tenthdb2master(int db)
{
    /* +6 to -73dB 1dB steps (plus mute == 80levels) 7bits */
    /* 1111111 ==  +6dB  (0x7f)                            */
    /* 1111001 ==   0dB  (0x79)                            */
    /* 0110000 == -73dB  (0x30)                            */
    /* 0101111..0000000 == mute  (<= 0x2f)                 */
    if (db < VOLUME_MIN)
        return 0x0;
    else
        return (db / 10) + 73 + 0x30;
}

static int tone_tenthdb2hw(int value)
{
    /* -6.0db..+0db..+9.0db step 1.5db - translate -60..+0..+90 step 15
        to 10..6..0 step -1.
    */
    value = 10 - (value + 60) / 15;

    if (value == 6)
        value = 0xf; /* 0db -> off */

    return value;
}


#ifdef USE_ADAPTIVE_BASS
static int adaptivebass2hw(int value)
{
    /* 0 to 15 step 1 - step -1  0 = off is a 15 in the register */
    value = 15 - value;

    return value;
}
#endif

#if defined(HAVE_WM8750)
static int recvol2hw(int value)
{
/* convert tenth of dB of input volume (-172...300) to input register value */
    /* +30dB to -17.25 0.75dB step 6 bits */
    /* 111111 == +30dB  (0x3f)            */
    /* 010111 ==   0dB  (0x17)            */
    /* 000000 == -17.25dB                 */

    return (3*(value/10 - 0x17))/4;
}
#endif
static void audiohw_mute(bool mute)
{
    /* Mute:   Set DACMU = 1 to soft-mute the audio DACs. */
    /* Unmute: Set DACMU = 0 to soft-un-mute the audio DACs. */
    wmcodec_write(DACCTRL, mute ? DACCTRL_DACMU : 0);
}

/* Reset and power up the WM8751 */
void audiohw_preinit(void)
{
#ifdef MROBE_100
    /* controls headphone ouput */
    GPIOL_ENABLE     |= 0x10;
    GPIOL_OUTPUT_EN  |= 0x10;
    GPIOL_OUTPUT_VAL |= 0x10; /* disable */
#endif

    /*
     * 1. Switch on power supplies.
     *    By default the WM8751 is in Standby Mode, the DAC is
     *    digitally muted and the Audio Interface, Line outputs
     *    and Headphone outputs are all OFF (DACMU = 1 Power
     *    Management registers 1 and 2 are all zeros).
     */

    wmcodec_write(RESET, RESET_RESET);    /*Reset*/

     /* 2. Enable Vmid and VREF. */
    wmcodec_write(PWRMGMT1, PWRMGMT1_VREF | PWRMGMT1_VMIDSEL_5K);

#ifdef CODEC_SLAVE
    wmcodec_write(AINTFCE,AINTFCE_WL_16|AINTFCE_FORMAT_I2S);
#else
    /* BCLKINV=0(Dont invert BCLK) MS=1(Enable Master) LRSWAP=0 LRP=0 */
    /* IWL=00(16 bit) FORMAT=10(I2S format) */
    wmcodec_write(AINTFCE, AINTFCE_MS | AINTFCE_WL_16 |
                  AINTFCE_FORMAT_I2S);
#endif
    /* Set default samplerate */

    audiohw_set_frequency(HW_FREQ_DEFAULT);
}

/* Enable DACs and audio output after a short delay */
void audiohw_postinit(void)
{
    /* From app notes: allow Vref to stabilize to reduce clicks */
    sleep(HZ);

     /* 3. Enable DACs as required. */
    wmcodec_write(PWRMGMT2, PWRMGMT2_DACL | PWRMGMT2_DACR);

     /* 4. Enable line and / or headphone output buffers as required. */
#if defined(MROBE_100) || defined(MPIO_HD200)
    wmcodec_write(PWRMGMT2, PWRMGMT2_DACL | PWRMGMT2_DACR |
                  PWRMGMT2_LOUT1 | PWRMGMT2_ROUT1);
#else
    wmcodec_write(PWRMGMT2, PWRMGMT2_DACL | PWRMGMT2_DACR |
                  PWRMGMT2_LOUT1 | PWRMGMT2_ROUT1 | PWRMGMT2_LOUT2 |
                  PWRMGMT2_ROUT2);
#endif

    /* Full -0dB on the DACS */
    wmcodec_write(LEFTGAIN, 0xff);
    wmcodec_write(RIGHTGAIN, RIGHTGAIN_RDVU | 0xff);

    wmcodec_write(ADDITIONAL1, ADDITIONAL1_TSDEN | ADDITIONAL1_TOEN |
                    ADDITIONAL1_DMONOMIX_LLRR | ADDITIONAL1_VSEL_DEFAULT);

    wmcodec_write(LEFTMIX1, LEFTMIX1_LD2LO | LEFTMIX1_LI2LO_DEFAULT);
    wmcodec_write(RIGHTMIX2, RIGHTMIX2_RD2RO | RIGHTMIX2_RI2RO_DEFAULT);

#ifdef TOSHIBA_GIGABEAT_F
#ifdef HAVE_HARDWARE_BEEP
    /* Single-ended mono input */
    wmcodec_write(MONOMIX1, 0);

    /* Route mono input to both outputs at 0dB */
    wmcodec_write(LEFTMIX2, LEFTMIX2_MI2LO | LEFTMIX2_MI2LOVOL(2));
    wmcodec_write(RIGHTMIX1, RIGHTMIX1_MI2RO | RIGHTMIX1_MI2ROVOL(2));
#endif
#endif

#ifdef MPIO_HD200
    /* Crude fix for high pitch noise at startup
     * I should find out what realy causes this
     */
    wmcodec_write(LOUT1, LOUT1_BITS|0x7f);
    wmcodec_write(ROUT1, ROUT1_BITS|0x7f);
    wmcodec_write(LOUT1, LOUT1_BITS);
    wmcodec_write(ROUT1, ROUT1_BITS);
#endif

    /* lower power consumption */
    wmcodec_write(PWRMGMT1, PWRMGMT1_VREF | PWRMGMT1_VMIDSEL_50K);

    audiohw_mute(false);

#ifdef MROBE_100
    /* enable headphone output */
    GPIOL_OUTPUT_VAL &= ~0x10;
    GPIOL_OUTPUT_EN  |=  0x10;
#endif
}

void audiohw_set_master_vol(int vol_l, int vol_r)
{
    /* +6 to -73dB 1dB steps (plus mute == 80levels) 7bits */
    /* 1111111 ==  +6dB                                    */
    /* 1111001 ==   0dB                                    */
    /* 0110000 == -73dB                                    */
    /* 0101111 == mute (0x2f)                              */

    wmcodec_write(LOUT1, LOUT1_BITS | LOUT1_LOUT1VOL(vol_l));
    wmcodec_write(ROUT1, ROUT1_BITS | ROUT1_ROUT1VOL(vol_r));
}

#ifndef MROBE_100
void audiohw_set_lineout_vol(int vol_l, int vol_r)
{
    wmcodec_write(LOUT2, LOUT2_BITS | LOUT2_LOUT2VOL(vol_l));
    wmcodec_write(ROUT2, ROUT2_BITS | ROUT2_ROUT2VOL(vol_r));
}
#endif

void audiohw_set_bass(int value)
{
    wmcodec_write(BASSCTRL, BASSCTRL_BITS |

#ifdef USE_ADAPTIVE_BASS
        BASSCTRL_BASS(adaptivebass2hw(value)));
#else
        BASSCTRL_BASS(tone_tenthdb2hw(value)));
#endif
}

void audiohw_set_treble(int value)
{
    wmcodec_write(TREBCTRL, TREBCTRL_BITS |
        TREBCTRL_TREB(tone_tenthdb2hw(value)));
}

void audiohw_set_prescaler(int value)
{
    prescaler = 3 * value / 15;
    wmcodec_write(LEFTGAIN, 0xff - (prescaler & LEFTGAIN_LDACVOL));
    wmcodec_write(RIGHTGAIN, RIGHTGAIN_RDVU |
                  (0xff - (prescaler & RIGHTGAIN_RDACVOL)));
}

/* Nice shutdown of WM8751 codec */
void audiohw_close(void)
{
    /* 1. Set DACMU = 1 to soft-mute the audio DACs. */
    audiohw_mute(true);

    /* 2. Disable all output buffers. */
    wmcodec_write(PWRMGMT2, 0x0);

    /* 3. Switch off the power supplies. */
    wmcodec_write(PWRMGMT1, 0x0);
}

void audiohw_set_frequency(int fsel)
{
    (void)fsel;
#ifndef CODEC_SLAVE
    static const unsigned char srctrl_table[HW_NUM_FREQ] =
    {
        HW_HAVE_11_([HW_FREQ_11] = CODEC_SRCTRL_11025HZ,)
        HW_HAVE_22_([HW_FREQ_22] = CODEC_SRCTRL_22050HZ,)
        HW_HAVE_44_([HW_FREQ_44] = CODEC_SRCTRL_44100HZ,)
        HW_HAVE_88_([HW_FREQ_88] = CODEC_SRCTRL_88200HZ,)
    };

    if ((unsigned)fsel >= HW_NUM_FREQ)
        fsel = HW_FREQ_DEFAULT;

    wmcodec_write(CLOCKING, srctrl_table[fsel]);
#endif
}

#if defined(HAVE_WM8750)
void audiohw_set_recsrc(int source, bool recording)
{
    /* INPUT1 - FM radio
     * INPUT2 - Line-in
     * INPUT3 - MIC
     *
     * if recording == false we use analog bypass from input
     * turn off ADC, PGA to save power
     * turn on output buffer(s)
     * 
     * if recording == true we route input signal to PGA
     * and monitoring picks up signal after PGA in analog domain
     * turn on ADC, PGA, DAC, output buffer(s)
     */
    
    switch(source)
    {
    case AUDIO_SRC_PLAYBACK:
        /* mute PGA, disable all audio paths but DAC and output stage*/
        wmcodec_write(LINVOL, LINVOL_LINMUTE | LINVOL_LINVOL(23)); /* 0dB */
        wmcodec_write(RINVOL, RINVOL_RINMUTE | RINVOL_RINVOL(23)); /* 0dB */
        wmcodec_write(PWRMGMT1, PWRMGMT1_VREF | PWRMGMT1_VMIDSEL_50K);
        wmcodec_write(PWRMGMT2, PWRMGMT2_DACL | PWRMGMT2_DACR |
                      PWRMGMT2_LOUT1 | PWRMGMT2_ROUT1);

        /* route DAC signal to output mixer */
        wmcodec_write(LEFTMIX1, LEFTMIX1_LD2LO);
        wmcodec_write(RIGHTMIX2, RIGHTMIX2_RD2RO);

        /* unmute DAC */
        audiohw_mute(false);
        break;

    case AUDIO_SRC_FMRADIO:
        if(recording)
        {
            /* Set input volume to PGA */
            wmcodec_write(LINVOL, LINVOL_LINVOL(23));
            wmcodec_write(RINVOL, RINVOL_RINVOL(23));

            /* Turn on PGA and ADC */
            wmcodec_write(PWRMGMT1, PWRMGMT1_VREF | PWRMGMT1_VMIDSEL_50K |
                          PWRMGMT1_AINL | PWRMGMT1_AINR | 
                          PWRMGMT1_ADCL | PWRMGMT1_ADCR);

            /* Setup input source for PGA as INPUT1 
             * MICBOOST disabled
             */
            wmcodec_write(ADCL, ADCL_LINSEL_LINPUT1 | ADCL_LMICBOOST_DISABLED);
            wmcodec_write(ADCR, ADCR_RINSEL_RINPUT1 | ADCR_RMICBOOST_DISABLED);

            /* setup output digital data
             * default is LADC -> LDATA, RADC -> RDATA
             * so we don't touch this
             */

            /* power up DAC and output stage */
            wmcodec_write(PWRMGMT2, PWRMGMT2_DACL | PWRMGMT2_DACR |
                          PWRMGMT2_LOUT1 | PWRMGMT2_ROUT1);

            /* analog monitor */
            wmcodec_write(LEFTMIX1, LEFTMIX1_LMIXSEL_ADCLIN |
                          LEFTMIX1_LD2LO);
            wmcodec_write(RIGHTMIX2, RIGHTMIX2_RMIXSEL_ADCRIN |
                          RIGHTMIX2_RD2RO);
        }
        else
        {

            /* turn off ADC, PGA */
            wmcodec_write(PWRMGMT1, PWRMGMT1_VREF | PWRMGMT1_VMIDSEL_50K);

           /* turn on DAC and output stage */
            wmcodec_write(PWRMGMT2, PWRMGMT2_DACL | PWRMGMT2_DACR |
                          PWRMGMT2_LOUT1 | PWRMGMT2_ROUT1);

           /* setup monitor mode by routing input signal to outmix 
             * at 0dB volume
             */
            wmcodec_write(LEFTMIX1, LEFTMIX1_LI2LO | LEFTMIX1_LMIXSEL_LINPUT1 |
                          LEFTMIX1_LI2LOVOL(0x20) | LEFTMIX1_LD2LO);
            wmcodec_write(RIGHTMIX2, RIGHTMIX2_RI2RO | RIGHTMIX2_RMIXSEL_RINPUT1 |
                          RIGHTMIX2_RI2ROVOL(0x20) | RIGHTMIX2_RD2RO);
        }
        break;

    case AUDIO_SRC_LINEIN:
        /* Set input volume to PGA */
        wmcodec_write(LINVOL, LINVOL_LINVOL(23));
        wmcodec_write(RINVOL, RINVOL_RINVOL(23));

        /* Turn on PGA, ADC, DAC */
        wmcodec_write(PWRMGMT1, PWRMGMT1_VREF | PWRMGMT1_VMIDSEL_50K |
                      PWRMGMT1_AINL | PWRMGMT1_AINR | 
                      PWRMGMT1_ADCL | PWRMGMT1_ADCR);

        /* turn on DAC and output stage */
        wmcodec_write(PWRMGMT2, PWRMGMT2_DACL | PWRMGMT2_DACR |
                      PWRMGMT2_LOUT1 | PWRMGMT2_ROUT1);

        /* Setup input source for PGA as INPUT2 
         * MICBOOST disabled
         */
        wmcodec_write(ADCL, ADCL_LINSEL_LINPUT2 | ADCL_LMICBOOST_DISABLED);
        wmcodec_write(ADCR, ADCR_RINSEL_RINPUT2 | ADCR_RMICBOOST_DISABLED);

        /* setup output digital data
         * default is LADC -> LDATA, RADC -> RDATA
         * so we don't touch this
         */

        /* digital monitor */
        wmcodec_write(LEFTMIX1, LEFTMIX1_LMIXSEL_ADCLIN |
                      LEFTMIX1_LD2LO);
        wmcodec_write(RIGHTMIX2, RIGHTMIX2_RMIXSEL_ADCRIN |
                      RIGHTMIX2_RD2RO);
        break;

    case AUDIO_SRC_MIC:
        /* Set input volume to PGA */
        wmcodec_write(LINVOL, LINVOL_LINVOL(23));
        wmcodec_write(RINVOL, RINVOL_RINVOL(23));

        /* Turn on PGA and ADC, turn off DAC */
        wmcodec_write(PWRMGMT1, PWRMGMT1_VREF | PWRMGMT1_VMIDSEL_50K |
                      PWRMGMT1_AINL | PWRMGMT1_AINR | 
                      PWRMGMT1_ADCL | PWRMGMT1_ADCR);

        /* turn on DAC and output stage */
        wmcodec_write(PWRMGMT2, PWRMGMT2_DACL | PWRMGMT2_DACR |
                      PWRMGMT2_LOUT1 | PWRMGMT2_ROUT1);

        /* Setup input source for PGA as INPUT3 
         * MICBOOST disabled
         */
        wmcodec_write(ADCL, ADCL_LINSEL_LINPUT3 | ADCL_LMICBOOST_DISABLED);
        wmcodec_write(ADCR, ADCR_RINSEL_RINPUT3 | ADCR_RMICBOOST_DISABLED);

        /* setup output digital data
         * default is LADC -> LDATA, RADC -> RDATA
         * so we don't touch this
         */

        /* analog monitor */
        wmcodec_write(LEFTMIX1, LEFTMIX1_LMIXSEL_ADCLIN |
                      LEFTMIX1_LD2LO);
        wmcodec_write(RIGHTMIX2, RIGHTMIX2_RMIXSEL_ADCRIN |
                      RIGHTMIX2_RD2RO);
        break;

    } /* switch(source) */
}

/* Setup PGA gain */
void audiohw_set_recvol(int left, int right, int type)
{
    (void)type;
    wmcodec_write(LINVOL, LINVOL_LINVOL(recvol2hw(left)));
    wmcodec_write(RINVOL, RINVOL_RINVOL(recvol2hw(right)));
}
#endif
