/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Felix Arends
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

#include <math.h>
#include <stdlib.h>         /* EXIT_SUCCESS */
#include "sim-ui-defines.h"
#include "lcd-charcells.h"
#include "lcd-remote.h"
#include "config.h"
#include "button.h"
#include "kernel.h"
#include "backlight.h"
#include "misc.h"
#include "button-sdl.h"
#include "backlight.h"
#ifdef SIMULATOR
#include "sim_tasks.h"
#include "buttonmap.h"
#endif
#include "debug.h"

#ifdef HAVE_TOUCHSCREEN
#include "touchscreen.h"
static int mouse_coords = 0;
#endif
/* how long until repeat kicks in */
#define REPEAT_START      6

/* the speed repeat starts at */
#define REPEAT_INTERVAL_START   4

/* speed repeat finishes at */
#define REPEAT_INTERVAL_FINISH  2

#ifdef HAVE_TOUCHSCREEN
#define USB_KEY SDLK_c /* SDLK_u is taken by BUTTON_MIDLEFT */
#else
#define USB_KEY SDLK_u
#endif

#if defined(IRIVER_H100_SERIES) || defined (IRIVER_H300_SERIES)
int _remote_type=REMOTETYPE_H100_LCD;

int remote_type(void)
{
    return _remote_type;
}
#endif

struct event_queue button_queue;

static int btn = 0;    /* Hopefully keeps track of currently pressed keys... */

#ifdef HAS_BUTTON_HOLD
bool hold_button_state = false;
bool button_hold(void) {
    return hold_button_state;
}
#endif

#ifdef HAS_REMOTE_BUTTON_HOLD
bool remote_hold_button_state = false;
bool remote_button_hold(void) {
    return remote_hold_button_state;
}
#endif
static void button_event(int key, bool pressed);
extern bool debug_wps;
extern bool mapping;

#ifdef HAVE_TOUCHSCREEN
static void touchscreen_event(int x, int y)
{
    if(background) {
        x -= UI_LCD_POSX;
        y -= UI_LCD_POSY;
    }

    if(x >= 0 && y >= 0 && x < SIM_LCD_WIDTH && y < SIM_LCD_HEIGHT) {
        mouse_coords = (x << 16) | y;
        button_event(BUTTON_TOUCHSCREEN, true);
        if (debug_wps)
            printf("Mouse at: (%d, %d)\n", x, y);
    }
}
#endif

bool gui_message_loop(void)
{
    SDL_Event event;
    static int x,y,xybutton = 0;

    while (SDL_WaitEvent(&event))
    {
        sim_enter_irq_handler();
        switch(event.type)
        {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                button_event(event.key.keysym.sym, event.type == SDL_KEYDOWN);
                break;
#ifdef HAVE_TOUCHSCREEN
            case SDL_MOUSEMOTION:
                if (event.motion.state & SDL_BUTTON(1))
                    touchscreen_event(event.motion.x, event.motion.y);
                break;
#endif
            case SDL_MOUSEBUTTONDOWN:
                switch ( event.button.button ) {
#ifdef HAVE_SCROLLWHEEL
                    case SDL_BUTTON_WHEELUP:
                        button_event( SDLK_UP, true );
                        break;
                    case SDL_BUTTON_WHEELDOWN:
                        button_event( SDLK_DOWN, true );
                        break;
#endif
                    case SDL_BUTTON_LEFT:
                    case SDL_BUTTON_MIDDLE:
                        if ( mapping && background ) {
                            x = event.button.x;
                            y = event.button.y;
                        }
                        if ( background ) {
                            xybutton = xy2button( event.button.x, event.button.y );
                            if( xybutton ) {
                                button_event( xybutton, true );
                                break;
                            }
                        }
#ifdef HAVE_TOUCHSCREEN
                        touchscreen_event(event.button.x, event.button.y);
#endif
                        break;

                    default:
                        break;
                }

                if (debug_wps && event.button.button == 1)
                {
                    if ( background ) 
#ifdef HAVE_REMOTE
                        if ( event.button.y < UI_REMOTE_POSY ) /* Main Screen */
                            printf("Mouse at: (%d, %d)\n", event.button.x - UI_LCD_POSX -1 , event.button.y - UI_LCD_POSY - 1 );
                        else 
                            printf("Mouse at: (%d, %d)\n", event.button.x - UI_REMOTE_POSX -1 , event.button.y - UI_REMOTE_POSY - 1 );
#else
                        printf("Mouse at: (%d, %d)\n", event.button.x - UI_LCD_POSX -1 , event.button.y - UI_LCD_POSY - 1 );
#endif
                    else 
                        if ( event.button.y/display_zoom < LCD_HEIGHT ) /* Main Screen */
                            printf("Mouse at: (%d, %d)\n", event.button.x/display_zoom, event.button.y/display_zoom );
#ifdef HAVE_REMOTE
                        else
                            printf("Mouse at: (%d, %d)\n", event.button.x/display_zoom, event.button.y/display_zoom - LCD_HEIGHT );
#endif
                }
                break;
            case SDL_MOUSEBUTTONUP:
                switch ( event.button.button ) {
                    /* The scrollwheel button up events are ignored as they are queued immediately */
                    case SDL_BUTTON_LEFT:
                    case SDL_BUTTON_MIDDLE:
                        if ( mapping && background ) {
                            printf("    { SDLK_,     %d, %d, %d, \"\" },\n", x,
#define SQUARE(x) ((x)*(x))
                            y, (int)sqrt( SQUARE(x-(int)event.button.x)
                                    + SQUARE(y-(int)event.button.y))  );
                        }
                        if ( background && xybutton ) {
                                button_event( xybutton, false );
                                xybutton = 0;
                            }
#ifdef HAVE_TOUCHSCREEN
                            else
                                button_event(BUTTON_TOUCHSCREEN, false);
#endif
                        break;
                    default:
                        break;
                }
                break;
                

            case SDL_QUIT:
            {
                sim_exit_irq_handler();
                return false;
            }
            default:
                /*printf("Unhandled event\n"); */
                break;
        }
        sim_exit_irq_handler();
    }

    return true;
}

static void button_event(int key, bool pressed)
{
    int new_btn = 0;
    static bool usb_connected = false;
    if (usb_connected && key != USB_KEY)
        return;
    switch (key)
    {
    case USB_KEY:
        if (!pressed)
        {
            usb_connected = !usb_connected;
            if (usb_connected)
                queue_post(&button_queue, SYS_USB_CONNECTED, 0);
            else
                queue_post(&button_queue, SYS_USB_DISCONNECTED, 0);
        }
        return;

#ifdef HAS_BUTTON_HOLD
    case SDLK_h:
        if(pressed)
        {
            hold_button_state = !hold_button_state;
            DEBUGF("Hold button is %s\n", hold_button_state?"ON":"OFF");
        }
        return;
#endif
        
#ifdef HAS_REMOTE_BUTTON_HOLD
    case SDLK_j:
        if(pressed)
        {
            remote_hold_button_state = !remote_hold_button_state;
            DEBUGF("Remote hold button is %s\n",
                   remote_hold_button_state?"ON":"OFF");
        }
        return;
#endif

#if defined(IRIVER_H100_SERIES) || defined (IRIVER_H300_SERIES)
    case SDLK_t:
        if(pressed)
            switch(_remote_type)
            {
                case REMOTETYPE_UNPLUGGED: 
                    _remote_type=REMOTETYPE_H100_LCD;
                    DEBUGF("Changed remote type to H100\n");
                    break;
                case REMOTETYPE_H100_LCD:
                    _remote_type=REMOTETYPE_H300_LCD;
                    DEBUGF("Changed remote type to H300\n");
                    break;
                case REMOTETYPE_H300_LCD:
                    _remote_type=REMOTETYPE_H300_NONLCD;
                    DEBUGF("Changed remote type to H300 NON-LCD\n");
                    break;
                case REMOTETYPE_H300_NONLCD:
                    _remote_type=REMOTETYPE_UNPLUGGED;
                    DEBUGF("Changed remote type to none\n");
                    break;
            }
        break;
#endif
    case SDLK_KP0:
    case SDLK_F5:
        if(pressed)
        {
            sim_trigger_screendump();
            return;
        }
        break;
#ifdef HAVE_TOUCHSCREEN
    case SDLK_F4:
        if(pressed)
        {
            touchscreen_set_mode(touchscreen_get_mode() == TOUCHSCREEN_POINT ? TOUCHSCREEN_BUTTON : TOUCHSCREEN_POINT);
            printf("Touchscreen mode: %s\n", touchscreen_get_mode() == TOUCHSCREEN_POINT ? "TOUCHSCREEN_POINT" : "TOUCHSCREEN_BUTTON");
        }
#endif
    default:
#ifdef HAVE_TOUCHSCREEN
        new_btn = key_to_touch(key, mouse_coords);
        if (!new_btn)
#endif
            new_btn = key_to_button(key);
        break;
    }
    /* Call to make up for scrollwheel target implementation.  This is
     * not handled in the main button.c driver, but on the target
     * implementation (look at button-e200.c for example if you are trying to 
     * figure out why using button_get_data needed a hack before).
     */
#if defined(BUTTON_SCROLL_FWD) && defined(BUTTON_SCROLL_BACK)
    if((new_btn == BUTTON_SCROLL_FWD || new_btn == BUTTON_SCROLL_BACK) && 
        pressed)
    {
        /* Clear these buttons from the data - adding them to the queue is
         *  handled in the scrollwheel drivers for the targets.  They do not
         *  store the scroll forward/back buttons in their button data for
         *  the button_read call.
         */
#ifdef HAVE_BACKLIGHT
        backlight_on();
#endif
#ifdef HAVE_BUTTON_LIGHT
        buttonlight_on();
#endif
        queue_post(&button_queue, new_btn, 1<<24);
        new_btn &= ~(BUTTON_SCROLL_FWD | BUTTON_SCROLL_BACK);
    }
#endif

    if (pressed)
        btn |= new_btn;
    else
        btn &= ~new_btn;
}
#if defined(HAVE_BUTTON_DATA) && defined(HAVE_TOUCHSCREEN)
int button_read_device(int* data)
{
    *data = mouse_coords;
#else
int button_read_device(void)
{
#endif
#ifdef HAS_BUTTON_HOLD
    int hold_button = button_hold();

#ifdef HAVE_BACKLIGHT
    /* light handling */
    static int hold_button_old = false;
    if (hold_button != hold_button_old)
    {
        hold_button_old = hold_button;
        backlight_hold_changed(hold_button);
    }
#endif

    if (hold_button)
        return BUTTON_NONE;
    else
#endif

    return btn;
}

void button_init_device(void)
{
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
}
