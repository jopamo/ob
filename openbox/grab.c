/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   grab.c for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
   Copyright (c) 2003-2007   Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include "grab.h"
#include "openbox.h"
#include "event.h"
#include "screen.h"
#include "debug.h"
#include "obt/display.h"
#include "obt/keyboard.h"

#include <glib.h>
#include <X11/Xlib.h>

/*!
 * \brief Bitmask for pointer grabs: listen for button presses/releases and pointer motion.
 */
#define GRAB_PTR_MASK (ButtonPressMask | ButtonReleaseMask | PointerMotionMask)

/*!
 * \brief Bitmask for keyboard grabs: listen for key presses/releases.
 */
#define GRAB_KEY_MASK (KeyPressMask | KeyReleaseMask)

/*!
 * \brief Maximum number of lock-mask combinations (num/caps/scroll).
 */
#define MASK_LIST_SIZE 8

/*!
 * \brief A list of all possible combinations of keyboard lock masks:
 *        No locks, NumLock, CapsLock, ScrollLock, or any combination of these.
 */
static guint mask_list[MASK_LIST_SIZE];

/*!
 * \brief Count of how many times the keyboard has been grabbed.
 */
static guint kgrabs = 0;

/*!
 * \brief Count of how many times the pointer has been grabbed.
 */
static guint pgrabs = 0;

/*!
 * \brief The timestamp of the most recent grab operation.
 *        Defaults to CurrentTime until a successful grab updates it.
 */
static Time  grab_time = CurrentTime;

/*!
 * \brief Tracks how many "passive" keyboard grabs are active (see \c grab_key_passive_count).
 */
static gint passive_count = 0;

/*!
 * \brief The input context used when grabbing keyboard input.
 */
static ObtIC *ic = NULL;

/*!
 * \brief Computes the appropriate X timestamp to use when ungrabbing.
 * \return An X timestamp, either CurrentTime or \c event_time().
 *
 * If the clock has moved backward, the last known \c grab_time could be
 * in the future relative to the server time, so we fallback to CurrentTime.
 */
static Time
ungrab_time(void)
{
    Time t = event_time();

    /* If last grab_time is invalid or t is behind it, use CurrentTime. */
    if (grab_time == CurrentTime ||
        !(t == CurrentTime || event_time_after(t, grab_time)))
    {
        t = CurrentTime;
    }
    return t;
}

/*!
 * \brief Returns the Window used for pointer/keyboard grabbing.
 *        Typically the \c screen_support_win from Openbox's screen code.
 */
static Window
grab_window(void)
{
    return screen_support_win;
}

/*!
 * \brief Checks if any keyboard grabs are active.
 */
gboolean
grab_on_keyboard(void)
{
    return (kgrabs > 0);
}

/*!
 * \brief Checks if any pointer grabs are active.
 */
gboolean
grab_on_pointer(void)
{
    return (pgrabs > 0);
}

/*!
 * \brief Returns the \c ObtIC (input context) used for grabbing input events.
 */
ObtIC *
grab_input_context(void)
{
    return ic;
}

/*!
 * \brief Grabs (or ungrabs) the keyboard in "stacked" fashion.
 *
 * \param grab TRUE to grab, FALSE to ungrab.
 * \return TRUE if the (un)grab was successful, FALSE otherwise.
 *
 * If the keyboard is already grabbed (kgrabs > 0), subsequent calls increment
 * and simply return TRUE. On the first successful grab, we store \c grab_time
 * from \c event_time().
 */
gboolean
grab_keyboard_full(gboolean grab)
{
    gboolean ret = FALSE;

    if (grab) {
        if (kgrabs++ == 0) {
            /* attempt the actual XGrabKeyboard */
            ret = (XGrabKeyboard(obt_display, grab_window(),
                                 False, GrabModeAsync, GrabModeAsync,
                                 event_time()) == Success);
            if (!ret) {
                --kgrabs;  /* revert increment if it failed */
            } else {
                /* reset passive_count and store new grab_time */
                passive_count = 0;
                grab_time = event_time();
            }
        } else {
            /* already grabbed, so success by definition */
            ret = TRUE;
        }
    } else if (kgrabs > 0) {
        /* ungrab scenario */
        if (--kgrabs == 0) {
            XUngrabKeyboard(obt_display, ungrab_time());
        }
        ret = TRUE;
    }

    return ret;
}

/*!
 * \brief Grabs (or ungrabs) the pointer in "stacked" fashion.
 *
 * \param grab         TRUE to grab, FALSE to ungrab.
 * \param owner_events If TRUE, events go to the grabbing window if in it.
 * \param confine      If TRUE, pointer is confined to the root window.
 * \param cur          The \c ObCursor to show while pointer is grabbed.
 * \return TRUE if successful.
 *
 * Similar to \c grab_keyboard_full, but for pointer. If the pointer is
 * already grabbed (pgrabs > 0), we increment the count and succeed immediately.
 */
gboolean
grab_pointer_full(gboolean grab, gboolean owner_events,
                  gboolean confine, ObCursor cur)
{
    gboolean ret = FALSE;

    if (grab) {
        if (pgrabs++ == 0) {
            Window confine_to = (confine ? obt_root(ob_screen) : None);
            int status = XGrabPointer(obt_display, grab_window(), owner_events,
                                      GRAB_PTR_MASK,
                                      GrabModeAsync, GrabModeAsync,
                                      confine_to,
                                      ob_cursor(cur),
                                      event_time());
            ret = (status == Success);
            if (!ret) {
                --pgrabs;  /* revert increment if failed */
            } else {
                grab_time = event_time();
            }
        } else {
            ret = TRUE;
        }
    } else if (pgrabs > 0) {
        if (--pgrabs == 0) {
            XUngrabPointer(obt_display, ungrab_time());
        }
        ret = TRUE;
    }
    return ret;
}

/*!
 * \brief Grabs or ungrabs the X server in a "stacked" manner.
 * \param grab TRUE to grab, FALSE to ungrab.
 * \return The new server-grab count (how many times we've stacked grabs).
 *
 * When the count goes from 0 to 1, we call \c XGrabServer().
 * When it goes from 1 to 0, we call \c XUngrabServer().
 */
gint
grab_server(gboolean grab)
{
    static guint sgrabs = 0;

    if (grab) {
        if (sgrabs++ == 0) {
            XGrabServer(obt_display);
            XSync(obt_display, FALSE);
        }
    } else if (sgrabs > 0) {
        if (--sgrabs == 0) {
            XUngrabServer(obt_display);
            XFlush(obt_display);
        }
    }
    return (gint)sgrabs;
}

/*!
 * \brief Sets up the static mask_list[] with all combinations of lock masks, and
 *        creates the global \c ObtIC \c ic.
 *
 * \param reconfig If TRUE, this is a reconfiguration rather than a cold start.
 */
void
grab_startup(gboolean reconfig)
{
    guint i = 0;
    guint num, caps, scroll;

    num    = obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_NUMLOCK);
    caps   = obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_CAPSLOCK);
    scroll = obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_SCROLLLOCK);

    mask_list[i++] = 0;
    mask_list[i++] = num;
    mask_list[i++] = caps;
    mask_list[i++] = scroll;
    mask_list[i++] = num | caps;
    mask_list[i++] = num | scroll;
    mask_list[i++] = caps | scroll;
    mask_list[i++] = num | caps | scroll;
    g_assert(i == MASK_LIST_SIZE);

    /* Create the keyboard input context for grabs. */
    ic = obt_keyboard_context_new(obt_root(ob_screen), grab_window());
}

/*!
 * \brief Shuts down the grab system, unref'ing \c ic and ungrabbing pointer/keyboard/server.
 *
 * \param reconfig If TRUE, this is a reconfiguration rather than a full shutdown.
 */
void
grab_shutdown(gboolean reconfig)
{
    obt_keyboard_context_unref(ic);
    ic = NULL;

    if (reconfig)
        return;

    /* Force ungrab everything repeatedly until counts go to 0. */
    while (ungrab_keyboard()) ;
    while (ungrab_pointer()) ;
    while (grab_server(FALSE)) ;
}

/*!
 * \brief Grabs a mouse button with certain modifiers on a window.
 *
 * \param button       The mouse button number (1=left,2=middle,3=right).
 * \param state        The modifier state (e.g., Mod4Mask).
 * \param win          The X Window to grab on.
 * \param mask         Event mask (e.g., ButtonPressMask).
 * \param pointer_mode Usually GrabModeAsync or GrabModeSync.
 * \param cur          The \c ObCursor to show while pointer is in the grab window.
 *
 * Internally, it grabs the button with each possible lock mask combination (NumLock, CapsLock, etc.).
 */
void
grab_button_full(guint button, guint state, Window win, guint mask,
                 gint pointer_mode, ObCursor cur)
{
    guint i;

    /* We can get BadAccess errors if the server won't let us grab. */
    obt_display_ignore_errors(TRUE);
    for (i = 0; i < MASK_LIST_SIZE; ++i) {
        XGrabButton(obt_display, button,
                    state | mask_list[i],
                    win, False,
                    mask, pointer_mode, GrabModeAsync,
                    None, ob_cursor(cur));
    }
    obt_display_ignore_errors(FALSE);

    if (obt_display_error_occured) {
        ob_debug("Failed to grab button %d modifiers %d", button, state);
    }
}

/*!
 * \brief Ungrabs a mouse button on a window.
 *
 * \param button The mouse button number.
 * \param state  The modifier state.
 * \param win    The X Window from which to ungrab.
 */
void
ungrab_button(guint button, guint state, Window win)
{
    guint i;
    for (i = 0; i < MASK_LIST_SIZE; ++i) {
        XUngrabButton(obt_display, button, state | mask_list[i], win);
    }
}

/*!
 * \brief Grabs a key with certain modifiers on a window.
 *
 * \param keycode       The keycode (not keysym).
 * \param state         The base modifiers (e.g., Mod4Mask).
 * \param win           The X Window to grab on.
 * \param keyboard_mode Usually GrabModeAsync or GrabModeSync.
 *
 * We grab the key with each possible lock mask combination.
 */
void
grab_key(guint keycode, guint state, Window win, gint keyboard_mode)
{
    guint i;

    obt_display_ignore_errors(TRUE);
    for (i = 0; i < MASK_LIST_SIZE; ++i) {
        XGrabKey(obt_display,
                 keycode,
                 state | mask_list[i],
                 win,
                 FALSE,                /* owner_events */
                 GrabModeAsync,        /* pointer_mode */
                 keyboard_mode);       /* keyboard_mode */
    }
    obt_display_ignore_errors(FALSE);

    if (obt_display_error_occured) {
        ob_debug("Failed to grab keycode %d modifiers %d", keycode, state);
    }
}

/*!
 * \brief Ungrabs all keys on a window.
 *
 * \param win The window from which to ungrab keys.
 */
void
ungrab_all_keys(Window win)
{
    XUngrabKey(obt_display, AnyKey, AnyModifier, win);
}

/*!
 * \brief Adjusts the \c passive_count by \p change, except if the keyboard is fully grabbed.
 *
 * \param change How much to add to \c passive_count.
 *
 * The idea is that if \c grab_on_keyboard() is active, we ignore \c passive_count changes.
 */
void
grab_key_passive_count(int change)
{
    if (grab_on_keyboard()) return;
    passive_count += change;
    if (passive_count < 0) passive_count = 0;
}

/*!
 * \brief Ungrabs the keyboard if we have a "passive" grab in effect.
 *
 * Cancels any passive grab indicated by \c passive_count, then resets it to 0.
 */
void
ungrab_passive_key(void)
{
    /* ob_debug("ungrabbing %d passive grabs\n", passive_count); */
    if (passive_count) {
        XUngrabKeyboard(obt_display, event_time());
        passive_count = 0;
    }
}
