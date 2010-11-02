/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _// __ \_/ ___\|  |/ /| __ \ / __ \  \/  /
 *   Jukebox    |    |   ( (__) )  \___|    ( | \_\ ( (__) )    (
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2007 Vuong Minh Hiep (vmh)
 * Copyright (C) 2008 Thomas Martitz (kugel.)
 * Copyright (C) 2008 Alexander Papst
 * Copyright (C) 2008 Peter D'Hoye
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

#include "plugin.h"
#include "lib/helper.h"



/* variable button definitions - only targets with a colour display */
#if defined(HAVE_LCD_COLOR)
#if (CONFIG_KEYPAD == IRIVER_H300_PAD)
#   define LAMP_LEFT       BUTTON_LEFT
#   define LAMP_RIGHT      BUTTON_RIGHT

#elif (CONFIG_KEYPAD == IPOD_4G_PAD)
#   define LAMP_LEFT       BUTTON_LEFT
#   define LAMP_RIGHT      BUTTON_RIGHT
#   define LAMP_NEXT       BUTTON_SCROLL_FWD
#   define LAMP_PREV       BUTTON_SCROLL_BACK

#elif (CONFIG_KEYPAD == IAUDIO_X5M5_PAD)
#   define LAMP_LEFT       BUTTON_LEFT
#   define LAMP_RIGHT      BUTTON_RIGHT

#elif (CONFIG_KEYPAD == GIGABEAT_PAD)
#   define LAMP_LEFT       BUTTON_LEFT
#   define LAMP_RIGHT      BUTTON_RIGHT

#elif (CONFIG_KEYPAD == GIGABEAT_S_PAD)
#   define LAMP_LEFT       BUTTON_LEFT
#   define LAMP_RIGHT      BUTTON_RIGHT

#elif (CONFIG_KEYPAD == SANSA_E200_PAD) || \
      (CONFIG_KEYPAD == SANSA_FUZE_PAD)
#   define LAMP_LEFT       BUTTON_LEFT
#   define LAMP_RIGHT      BUTTON_RIGHT
#   define LAMP_NEXT       BUTTON_SCROLL_FWD
#   define LAMP_PREV       BUTTON_SCROLL_BACK

#elif (CONFIG_KEYPAD == SANSA_C200_PAD)
#   define LAMP_LEFT       BUTTON_LEFT
#   define LAMP_RIGHT      BUTTON_RIGHT

#elif (CONFIG_KEYPAD == IRIVER_H10_PAD)
#   define LAMP_LEFT       BUTTON_LEFT
#   define LAMP_RIGHT      BUTTON_RIGHT
#   define LAMP_NEXT       BUTTON_SCROLL_UP
#   define LAMP_PREV       BUTTON_SCROLL_DOWN

#elif CONFIG_KEYPAD == MROBE500_PAD
#   define LAMP_LEFT       BUTTON_LEFT
#   define LAMP_RIGHT      BUTTON_RIGHT

#elif CONFIG_KEYPAD == COWON_D2_PAD

#elif CONFIG_KEYPAD == IAUDIO67_PAD
#   define LAMP_LEFT       BUTTON_LEFT
#   define LAMP_RIGHT      BUTTON_RIGHT

#elif CONFIG_KEYPAD == PHILIPS_HDD1630_PAD
#   define LAMP_LEFT       BUTTON_LEFT
#   define LAMP_RIGHT      BUTTON_RIGHT

#elif CONFIG_KEYPAD == PHILIPS_HDD6330_PAD
#   define LAMP_LEFT       BUTTON_LEFT
#   define LAMP_RIGHT      BUTTON_RIGHT

#elif CONFIG_KEYPAD == PHILIPS_SA9200_PAD
#   define LAMP_LEFT       BUTTON_PREV
#   define LAMP_RIGHT      BUTTON_NEXT

#elif CONFIG_KEYPAD == ONDAVX747_PAD
#   define LAMP_LEFT       BUTTON_VOL_DOWN
#   define LAMP_RIGHT      BUTTON_VOL_UP

#elif CONFIG_KEYPAD == ONDAVX777_PAD

#elif CONFIG_KEYPAD == SAMSUNG_YH_PAD
#   define LAMP_LEFT       BUTTON_LEFT
#   define LAMP_RIGHT      BUTTON_RIGHT

#elif CONFIG_KEYPAD == PBELL_VIBE500_PAD
#   define LAMP_LEFT       BUTTON_PREV
#   define LAMP_RIGHT      BUTTON_NEXT

#else
#   error Missing key definitions for this keypad
#endif
#endif

#ifdef HAVE_TOUCHSCREEN
# ifndef LAMP_LEFT
#   define LAMP_LEFT       BUTTON_MIDLEFT
# endif
# ifndef LAMP_RIGHT
#   define LAMP_RIGHT      BUTTON_MIDRIGHT
# endif
# ifndef LAMP_NEXT
#   define LAMP_NEXT       BUTTON_TOPMIDDLE
# endif
# ifndef LAMP_PREV
#   define LAMP_PREV       BUTTON_BOTTOMMIDDLE
# endif
#endif

#ifdef HAVE_LCD_COLOR
/* RGB color sets */
#define NUM_COLORSETS   2
static unsigned colorset[NUM_COLORSETS] = {
    LCD_RGBPACK(255, 255, 255),    /* white */
    LCD_RGBPACK(255,   0,   0),    /* red */
};
#endif /* HAVE_LCD_COLOR */

/* this is the plugin entry point */
enum plugin_status plugin_start(const void* parameter)
{
    enum plugin_status status = PLUGIN_OK;
    long button;
    (void)parameter;

#ifdef HAVE_LCD_COLOR
    int cs = 0;
    bool quit = false;
    bool update = true;
#endif /* HAVE_LCD_COLOR */

#if LCD_DEPTH > 1
    unsigned bg_color = rb->lcd_get_background();
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_background(LCD_WHITE);
#endif

#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    backlight_brightness_set(MAX_BRIGHTNESS_SETTING);
#endif /* HAVE_BACKLIGHT_BRIGHTNESS */
#ifdef HAVE_BUTTONLIGHT_BRIGHTNESS
    buttonlight_brightness_set(MAX_BRIGHTNESS_SETTING);
#endif /* HAVE_BUTTONLIGHT_BRIGHTNESS */

#ifdef HAVE_LCD_INVERT
#ifdef HAVE_NEGATIVE_LCD
    rb->lcd_set_invert_display(true);
#else
    rb->lcd_set_invert_display(false);
#endif /* HAVE_NEGATIVE_LCD */
#endif /* HAVE_LCD_INVERT */

    backlight_force_on();
#ifdef HAVE_BUTTON_LIGHT
    buttonlight_force_on();
#endif /* HAVE_BUTTON_LIGHT */

#ifdef HAVE_LCD_COLOR
    do
    {
        if(update)
        {
            if(cs < 0)
                cs = NUM_COLORSETS-1;
            if(cs >= NUM_COLORSETS)
                cs = 0;
            rb->lcd_set_background(colorset[cs]);
            rb->lcd_clear_display();
            rb->lcd_update();
            update = false;
        }

        switch((button = rb->button_get_w_tmo(HZ*30)))
        {
            case LAMP_RIGHT:
#ifdef LAMP_NEXT
            case LAMP_NEXT:
#endif /* LAMP_NEXT */
                cs++;
                update = true;
                break;

            case LAMP_LEFT:
#ifdef LAMP_PREV
            case LAMP_PREV:
#endif /* LAMP_PREV */
                cs--;
                update = true;
                break;

            default:
                if(button)
                {
                    if(rb->default_event_handler(button) == SYS_USB_CONNECTED)
                    {
                        status = PLUGIN_USB_CONNECTED;
                        quit = true;
                    }
                    if(!(button & (BUTTON_REL|BUTTON_REPEAT))
                        && !IS_SYSEVENT(button))
                        quit = true;
                }
                break;
        }
        rb->reset_poweroff_timer();
    } while (!quit);

#else /* HAVE_LCD_COLOR */
    rb->lcd_clear_display();
    rb->lcd_update();
    /* wait */
    do
    {
        button = rb->button_get_w_tmo(HZ*30);
        if(button)
        {
            if(rb->default_event_handler(button) == SYS_USB_CONNECTED)
            {
                status = PLUGIN_USB_CONNECTED;
                break;
            }
            if(!IS_SYSEVENT(button))
                break;
        }
        rb->reset_poweroff_timer();
    } while (1);

#endif /*HAVE_LCD_COLOR */

    /* restore */
    backlight_use_settings();
#ifdef HAVE_BUTTON_LIGHT
    buttonlight_use_settings();
#endif /* HAVE_BUTTON_LIGHT */

#ifdef HAVE_LCD_INVERT
    rb->lcd_set_invert_display(rb->global_settings->invert);
#endif /* HAVE_LCD_INVERT */

#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    backlight_brightness_use_setting();
#endif /* HAVE_BACKLIGHT_BRIGHTNESS */
#ifdef HAVE_BUTTONLIGHT_BRIGHTNESS
    buttonlight_brightness_use_setting();
#endif /* HAVE_BUTTONLIGHT_BRIGHTNESS */

#if LCD_DEPTH > 1
    rb->lcd_set_background(bg_color);
#endif
    return status;
}
