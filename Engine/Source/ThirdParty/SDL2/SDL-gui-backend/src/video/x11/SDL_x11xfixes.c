/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "../../SDL_internal.h"

/* EG BEGIN */
#ifdef SDL_WITH_EPIC_EXTENSIONS

#if SDL_VIDEO_DRIVER_X11

#include "SDL_x11video.h"
#include "SDL_x11xfixes.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_touch_c.h"

#if SDL_VIDEO_DRIVER_X11_XFIXES
static int xfixes_initialized = 0;

static int
query_xfixes_version(Display *display, int major, int minor)
{
    /* We don't care if this fails, so long as it sets major/minor on it's way out the door. */
    X11_XFixesQueryVersion(display, &major, &minor);
    return ((major * 1000) + minor);
}

static SDL_bool
xfixes_version_atleast(const int version, const int wantmajor, const int wantminor)
{
    return (version >= ((wantmajor * 1000) + wantminor));
}

#endif /* SDL_VIDEO_DRIVER_X11_XFIXES */

void
X11_InitXFixes(_THIS)
{
#if SDL_VIDEO_DRIVER_X11_XFIXES
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    int version = 0;
    int event, error;
    int fixes_opcode;

    if (!X11_XQueryExtension(data->display, "XFIXES", &fixes_opcode, &event, &error)) {
        return;
    }

    /* We need at least 5.0 for barriers. */
    version = query_xfixes_version(data->display, 5, 0);
    if (!xfixes_version_atleast(version, 5, 0)) {
        return; /* X server does not support the version we want at all. */
    }

    xfixes_initialized = 1;

#endif
}

int
X11_XFixesIsInitialized()
{
#if SDL_VIDEO_DRIVER_X11_XFIXES
    return xfixes_initialized;
#else
    return 0;
#endif
}

SDL_bool
X11_ConfineCursor(_THIS, SDL_Window * window, const SDL_Rect * rect, int flags)
{
#if SDL_VIDEO_DRIVER_X11_XFIXES

    /* Yaakuro: For some reason Xfixes when confining inside a rect where the
     * edges exactly match, a rectangle the cursor 'slips' out of the barrier.
     * To prevent that the lines for the barriers will span the whole screen.
     */
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    if (!X11_XFixesIsInitialized()) {
        SDL_Unsupported();
        return SDL_FALSE;
    }

    /* If there is already a set of barriers active, disable them. */
    if (data->active_cursor_confined_window) {
         X11_DestroyPointerBarrier(_this, data->active_cursor_confined_window);
    }

    /* If the user specified a valid window, use it to calculate the barrier rectangle else destroy
     * all barriers for all windows */
    if (window) {
        SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;

        /* If user did not specify an area to confine, destroy the barrier that was/is assigned to
         * this window it was assigned*/
        if (rect) {
            int x1, y1, x2, y2;
            SDL_Rect bounds;
            /* Find the windows's monitor.*/
            int displayIndex = SDL_GetWindowDisplayIndex(window);
            SDL_zero(bounds);
            SDL_GetDisplayBounds(displayIndex, &bounds);

            /** Negative values are not allowed. Clip values relative to the specified window. */
            x1 = rect->x >= 0 ? rect->x : 0;
            y1 = rect->y >= 0 ? rect->y : 0;
            x2 = (x1 + rect->w) <= bounds.w ? (x1 + rect->w) : bounds.w; /* Do we have to do this? */
            y2 = (y1 + rect->h) <= bounds.h ? (y1 + rect->h) : bounds.h; /* Do we have to do this? */

            if ((wdata->barrier_rect.x != rect->x) ||
                (wdata->barrier_rect.y != rect->y) ||
                (wdata->barrier_rect.w != rect->w) ||
                (wdata->barrier_rect.h != rect->h)) {
                wdata->barrier_rect = *rect;
            }
            /** Create the left barrier */
            wdata->barrier[0] = X11_XFixesCreatePointerBarrier(data->display, wdata->xwindow,
                                                 x1, 0,
                                                 x1, bounds.h,
                                                 BarrierPositiveX,
                                                 0, NULL);
            /** Create the right barrier */
            wdata->barrier[1] = X11_XFixesCreatePointerBarrier(data->display, wdata->xwindow,
                                                 x2, 0,
                                                 x2, bounds.h,
                                                 BarrierNegativeX,
                                                 0, NULL);
            /** Create the top barrier */
            wdata->barrier[2] = X11_XFixesCreatePointerBarrier(data->display, wdata->xwindow,
                                                 0, y1,
                                                 bounds.w, y1,
                                                 BarrierPositiveY,
                                                 0, NULL);
            /** Create the bottom barrier */
            wdata->barrier[3] = X11_XFixesCreatePointerBarrier(data->display, wdata->xwindow,
                                                 0, y2,
                                                 bounds.w, y2,
                                                 BarrierNegativeY,
                                                 0, NULL);
            X11_XFlush(data->display);

            /* Lets remember current active confined window. */
            data->active_cursor_confined_window = window;

            /* User activated the confinement for this window. We use this later to reactivate
             * the confinement if it got deactivated by FocusOut or UnmapNotify */
            wdata->pointer_barrier_active = SDL_TRUE;

            return SDL_TRUE;
      } else {
          X11_DestroyPointerBarrier(_this, window);

          /* Only set barrier inactive when user specified NULL and not handled by focus out. */
          if (flags != X11_BARRIER_HANDLED_BY_EVENT) {
              wdata->pointer_barrier_active = SDL_FALSE;
          }
          return SDL_TRUE;
      }
    } else {
        X11_DestroyAllPointerBarrier(_this);
    }
#endif
    return SDL_FALSE;
}

void
X11_DestroyPointerBarrier(_THIS, SDL_Window * window)
{
#if SDL_VIDEO_DRIVER_X11_XFIXES
    int i;
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    if (window) {
        SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;

        for (i = 0; i < 4; i++) {
            if (wdata->barrier[i] > 0) {
                X11_XFixesDestroyPointerBarrier(data->display, wdata->barrier[i]);
                wdata->barrier[i] = 0;
            }
        }
        X11_XFlush(data->display);
    }
    data->active_cursor_confined_window = NULL;
#endif
}

void
X11_DestroyAllPointerBarrier(_THIS)
{
#if SDL_VIDEO_DRIVER_X11_XFIXES
    int i, windowidx;
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    int numwindows = data->numwindows;
    SDL_WindowData **windowlist = data->windowlist;

    if (windowlist) {
        for (windowidx = 0; windowidx < numwindows; ++windowidx) {
            for (i = 0; i < 4; i++) {
                if (windowlist[windowidx]->barrier[i] > 0) {
                    X11_XFixesDestroyPointerBarrier(data->display, windowlist[windowidx]->barrier[i]);
                    windowlist[windowidx]->barrier[i] = 0;
                }
            }
        }
        X11_XFlush(data->display);
    }
    data->active_cursor_confined_window = NULL;
#endif
}

#endif /* SDL_VIDEO_DRIVER_X11 */

#endif /* SDL_WITH_EPIC_EXTENSIONS */
/* EG END */

/* vi: set ts=4 sw=4 expandtab: */
