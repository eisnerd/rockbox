/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: $
 *
 * Copyright (C) 2009 by Karl Kurbjun
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
#ifndef LCD_REMOTE_TARGET_H
#define LCD_REMOTE_TARGET_H

#define REMOTE_INIT_LCD   1
#define REMOTE_DEINIT_LCD 2

void lcd_remote_powersave(bool on);
void lcd_remote_set_invert_display(bool yesno);
//void lcd_remote_set_flip(bool yesno);

bool remote_detect(void);
void lcd_remote_init_device(void);
void lcd_remote_on(void);
void lcd_remote_off(void);
void lcd_remote_update(void);
void lcd_remote_update_rect(int, int, int, int);

#ifndef SIMULATOR
void _remote_backlight_on(void);
void _remote_backlight_off(void);
#endif

extern bool remote_initialized;

void lcd_remote_sleep(void);

int remote_read_device(void);
bool remote_button_hold(void);

#endif
