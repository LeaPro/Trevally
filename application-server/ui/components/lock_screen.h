/**
 * @file lock_screen.h
 * @brief Locking the panel to a role (see Roles.h, permissions.h).
 *
 * With roles configured the panel is locked until someone logs in with a role's passcode on the Enter PIN
 * page, which is shown on top of everything while locked (also after power-on). Its back arrow puts the
 * screen to sleep instead of letting anyone past it.
 *
 * The home screen's sleep icon (lock_screen_sleep()) logs the role out. It shows the screensaver (see
 * screensaver.h) when that is enabled, and otherwise switches the backlight off right away. The screensaver also
 * starts after its own inactivity timeout, and logs the role out then too. The backlight's inactivity timeout only
 * darkens the screen, without logging anyone out. While the screen is dark or the screensaver shows, a touch or
 * an encoder turn / press only wakes it: it never reaches what's under it. Waking a dark screen with the
 * screensaver up (or due) shows the screensaver, and only the next input dismisses it. Without roles, sleeping
 * just shows the screensaver or darkens the screen.
 */

#ifndef LOCK_SCREEN_H
#define LOCK_SCREEN_H

/** Call once the UI is built (after nav_init()). */
void lock_screen_init();

/** The sleep icon: log out, and show the screensaver or switch the screen off. */
void lock_screen_sleep();

/** Every UI tick: follows the backlight and the screensaver (guard) and the roles (locked: Enter PIN page). */
void lock_screen_tick();

#endif /* LOCK_SCREEN_H */
