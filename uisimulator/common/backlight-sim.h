/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id:$
 *
 * Copyright (C) 2009 by Thomas Martitz
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

#ifndef BACKLIGHT_SIM_H
#define BACKLIGHT_SIM_H

#include "config.h"

bool _backlight_init(void);
void _backlight_on(void);
void _backlight_off(void);

#ifdef HAVE_BACKLIGHT_BRIGHTNESS
void _backlight_set_brightness(int val);
#endif /* HAVE_BACKLIGHT_BRIGHTNESS */
#ifdef HAVE_BUTTON_LIGHT
void _buttonlight_on(void);
void _buttonlight_off(void);
#ifdef HAVE_BUTTONLIGHT_BRIGHTNESS
void _buttonlight_set_brightness(int val);
#endif /* HAVE_BUTTONLIGHT_BRIGHTNESS */
#endif /* HAVE_BUTTON_LIGHT */

#ifdef HAVE_REMOTE_LCD
void _remote_backlight_on(void);
void _remote_backlight_off(void);
#endif /* HAVE_REMOTE_LCD */


#endif /* BACKLIGHT_SIM_H */

