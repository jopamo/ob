#ifndef __grab_h
#define __grab_h

/*!
 * \file grab.h
 * \brief Provides functions for grabbing the X server, pointer, and keyboard
 *        in a stacked manner, as well as utility functions for grabbing keys
 *        and buttons with various modifiers.
 */

#include "misc.h"
#include "obt/keyboard.h"

#include <glib.h>
#include <X11/Xlib.h>

/*!
 * \brief Initializes the grab system.
 * \param reconfig TRUE if this is being called due to a reconfiguration,
 *                 FALSE if it's a cold startup.
 *
 * Sets up data structures for keyboard lock masks, creates the global input
 * context (\c ObtIC) used when grabbing input, etc.
 */
void grab_startup(gboolean reconfig);

/*!
 * \brief Shuts down the grab system, ungrabbing pointer/keyboard/server if needed.
 * \param reconfig TRUE if this is due to a reconfiguration,
 *                 FALSE if it's a full shutdown.
 *
 * On a full shutdown (\c reconfig = FALSE), repeatedly ungrabs everything
 * until counts go to zero. Also unrefs the global \c ObtIC.
 */
void grab_shutdown(gboolean reconfig);

/*!
 * \brief Retrieves the global input context (\c ObtIC) used for grabbed keyboard input.
 * \return A pointer to the \c ObtIC, or NULL if not yet initialized.
 */
ObtIC *grab_input_context(void);

/*!
 * \brief Grabs or ungrabs the keyboard in a "stacked" manner.
 * \param grab TRUE to grab, FALSE to ungrab.
 * \return TRUE if the operation was successful, FALSE otherwise.
 *
 * Internally maintains a count (\c kgrabs). If \c kgrabs is already nonzero
 * and you call \c grab_keyboard_full(TRUE) again, it increments and returns TRUE
 * immediately. If you call \c grab_keyboard_full(FALSE), it decrements and only
 * truly ungrabs when the count reaches zero.
 */
gboolean grab_keyboard_full(gboolean grab);

/*!
 * \brief Grabs or ungrabs the pointer in a "stacked" manner.
 * \param grab         TRUE to grab, FALSE to ungrab.
 * \param owner_events If TRUE, pointer events go to the grabbing window if it
 *                     contains the pointer. If FALSE, all pointer events are
 *                     reported with respect to the grab window.
 * \param confine      If TRUE, the pointer is confined to the screen root window.
 * \param cur          The \c ObCursor shape to show while pointer is grabbed.
 * \return TRUE if successful, FALSE otherwise.
 *
 * Similar to \c grab_keyboard_full, but for pointer grabs. A global count
 * (\c pgrabs) tracks how many times the pointer is "stacked grabbed." If the
 * pointer is already grabbed, subsequent calls to grab again just increment
 * and return TRUE. Ungrab only occurs when the count goes from 1 to 0.
 */
gboolean grab_pointer_full(gboolean grab, gboolean owner_events,
                           gboolean confine, ObCursor cur);

/*!
 * \brief Grabs or ungrabs the X server (stacked).
 * \param grab TRUE to grab, FALSE to ungrab.
 * \return The updated server grab count (how many times we've grabbed).
 *
 * When this count goes from 0 to 1, we call \c XGrabServer().
 * Going from 1 to 0 triggers \c XUngrabServer().
 */
gint grab_server(gboolean grab);

/*!
 * \brief Convenience macros for grabbing/ungrabbing the keyboard and pointer.
 *
 * These are stack-based: if you call \c grab_keyboard() multiple times,
 * you must call \c ungrab_keyboard() the same number of times to release.
 */
#define grab_keyboard()         grab_keyboard_full(TRUE)
#define ungrab_keyboard()       grab_keyboard_full(FALSE)
#define grab_pointer(o,c,u)     grab_pointer_full(TRUE, (o), (c), (u))
#define ungrab_pointer()        grab_pointer_full(FALSE, FALSE, FALSE, OB_CURSOR_NONE)

/*!
 * \brief Checks if the keyboard is currently grabbed (kgrabs > 0).
 */
gboolean grab_on_keyboard(void);

/*!
 * \brief Checks if the pointer is currently grabbed (pgrabs > 0).
 */
gboolean grab_on_pointer(void);

/*!
 * \brief Grabs a mouse button on a window, including all lock-mask combinations.
 * \param button       The mouse button (1=left,2=middle,3=right, etc.).
 * \param state        Modifier state (e.g., Mod4Mask).
 * \param win          The X Window to grab on.
 * \param mask         The event mask (e.g. ButtonPressMask).
 * \param pointer_mode Usually \c GrabModeAsync or \c GrabModeSync.
 * \param cursor       The \c ObCursor to show while pointer is in that window.
 *
 * Internally grabs the button once for each combination of lock states (NumLock,
 * CapsLock, ScrollLock, etc.).
 */
void grab_button_full(guint button, guint state, Window win, guint mask,
                      gint pointer_mode, ObCursor cursor);

/*!
 * \brief Ungrabs a mouse button for all lock-mask combos.
 * \param button The mouse button.
 * \param state  The base modifier state.
 * \param win    The window to ungrab on.
 */
void ungrab_button(guint button, guint state, Window win);

/*!
 * \brief Grabs a key with various lock-mask combos.
 * \param keycode       The keycode (not keysym).
 * \param state         Base modifier state (e.g., Mod4Mask).
 * \param win           The X Window to grab on.
 * \param keyboard_mode \c GrabModeAsync or \c GrabModeSync for the keyboard.
 */
void grab_key(guint keycode, guint state, Window win, gint keyboard_mode);

/*!
 * \brief Ungrabs all keys on a given window, ignoring lock masks.
 */
void ungrab_all_keys(Window win);

/*!
 * \brief Adjusts the "passive" grab counter by \p change, if the keyboard
 *        isn't actively grabbed.
 * \param change A signed integer to add to the \c passive_count.
 *
 * If \c grab_on_keyboard() is active, this function does nothing.
 */
void grab_key_passive_count(int change);

/*!
 * \brief If a "passive" keyboard grab is in effect, ungrab it now.
 *
 * Resets the internal \c passive_count to zero after ungrabbing.
 */
void ungrab_passive_key(void);

#endif /* __grab_h */
