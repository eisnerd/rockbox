/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 Gilles Roux
 *               2003 Garrett Derner
 *               2010 Yoshihisa Uchida
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
#ifndef PLUGIN_TEXT_VIEWER_PREFERENCES_H
#define PLUGIN_TEXT_VIEWER_PREFERENCES_H

/* scrollbar_mode */
enum {
    SB_OFF = 0,
    SB_ON,
};

/* word_mode */
enum {
    WRAP = 0,
    CHOP,
};

/* line_mode */
enum {
    NORMAL = 0,
    JOIN,
    EXPAND,
    REFLOW,
};

/* alignment */
enum {
    LEFT = 0,
    RIGHT,
};

/* page_mode */
enum {
    NO_OVERLAP = 0,
    OVERLAP,
};

/* header_mode */
enum {
    HD_NONE = 0,
    HD_PATH,
    HD_SBAR,
    HD_BOTH,
};

/* footer_mode */
enum {

    FT_NONE = 0,
    FT_PAGE,
    FT_SBAR,
    FT_BOTH,
};

/* horizontal_scroll_mode */
enum {
    SCREEN = 0,
    COLUMN,
};

/* vertical_scroll_mode */
enum {
    PAGE = 0,
    LINE,
};

/* narrow_mode */
enum {
    NM_PAGE = 0,
    NM_TOP_BOTTOM,
};

struct tv_preferences {
    unsigned word_mode;
    unsigned line_mode;
    unsigned alignment;

    unsigned encoding;

    unsigned horizontal_scrollbar;
    unsigned vertical_scrollbar;

    unsigned page_mode;
    unsigned header_mode;
    unsigned footer_mode;
    unsigned horizontal_scroll_mode;
    unsigned vertical_scroll_mode;

    int autoscroll_speed;

    int windows;

    unsigned narrow_mode;

    unsigned indent_spaces;

#ifdef HAVE_LCD_BITMAP
    unsigned char font_name[MAX_PATH];
    struct font *font;
#endif
    unsigned char file_name[MAX_PATH];
};

/*
 *     global pointer to the preferences
 */
extern struct tv_preferences *preferences;

/*
 * change the preferences
 *
 * [In] new_prefs
 *          new preferences
 */
void tv_set_preferences(const struct tv_preferences *new_prefs);

/*
 * copy the preferences
 *
 * [Out] copy_prefs
 *          the preferences in copy destination
 */
void tv_copy_preferences(struct tv_preferences *copy_prefs);

/*
 * set the default settings
 *
 * [Out] p
 *          the preferences which store the default settings
 */
void tv_set_default_preferences(struct tv_preferences *p);

/*
 * register the function to be executed when the current preferences is changed
 *
 * [In] listner
 *          the function to be executed when the current preferences is changed
 */
void tv_add_preferences_change_listner(void (*listner)(const struct tv_preferences *oldp));

#endif
