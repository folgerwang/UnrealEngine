/* Copyright (c) 2013-2018 by Mercer Road Corp
 *
 * Permission to use, copy, modify or distribute this software in binary or source form 
 * for any purpose is allowed only under explicit prior consent in writing from Mercer Road Corp
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND MERCER ROAD CORP DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL MERCER ROAD CORP
 * BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifndef __VXCPLATFORM_H
#define __VXCPLATFORM_H
/*
#ifdef _MSC_VER
#ifdef BUILD_SHARED
  #ifdef BUILDING_VIVOX_PLATFORM
    #define VXPLATFORM_DLLEXPORT __declspec(dllexport)
  #else
    #define VXPLATFORM_DLLEXPORT __declspec(dllimport)
  #endif
#else
    #define VXPLATFORM_DLLEXPORT
#endif
#else
  #define VXPLATFORM_DLLEXPORT __attribute__ ((visibility("default")))
#endif
*/
#define VXPLATFORM_DLLEXPORT

#include <stddef.h>
#include <string>

#define OS_E_SUCCESS 0
#define OS_E_TIMEOUT 0x40000

#if !defined(_WIN32)
# define E_FAIL ((os_error_t) - 1)
#endif

namespace vxplatform {
    std::string string_format(const char *fmt, ...);

    typedef unsigned long os_error_t;
    typedef unsigned long os_thread_id;

    typedef void * os_thread_handle;
    typedef void * os_event_handle;
    typedef os_error_t (*thread_start_function_t)(void*);

    VXPLATFORM_DLLEXPORT os_error_t create_thread(thread_start_function_t pf, void* pArg, os_thread_handle* pHandle, size_t stacksize = 0, int priority = -1);
    VXPLATFORM_DLLEXPORT os_error_t create_thread(thread_start_function_t pf, void *pArg, os_thread_handle *phThread, os_thread_id *pTid, size_t stacksize = 0, int priority = -1);
    VXPLATFORM_DLLEXPORT os_error_t delete_thread(os_thread_handle handle);
    VXPLATFORM_DLLEXPORT os_error_t join_thread(os_thread_handle handle, int timeout = -1);
    VXPLATFORM_DLLEXPORT os_error_t close_thread_handle(os_thread_handle handle);
    VXPLATFORM_DLLEXPORT os_thread_id get_current_thread_id();
    VXPLATFORM_DLLEXPORT void thread_sleep(unsigned long long ms);
    VXPLATFORM_DLLEXPORT void set_thread_name(const std::string& threadName);

    VXPLATFORM_DLLEXPORT os_error_t create_event(os_event_handle *pHandle);
    VXPLATFORM_DLLEXPORT os_error_t set_event(os_event_handle handle);
    VXPLATFORM_DLLEXPORT os_error_t wait_event(os_event_handle handle, int timeout = -1);
    VXPLATFORM_DLLEXPORT os_error_t delete_event(os_event_handle handle);
    VXPLATFORM_DLLEXPORT double get_millisecond_tick_counter();

    class VXPLATFORM_DLLEXPORT Lock
    {
    public:
        Lock();
        ~Lock();

        void Take();
        void Release();

    private:
        void* m_pImpl;

        Lock(const Lock&);
    };

    class VXPLATFORM_DLLEXPORT Locker
    {

    public:
        Locker(Lock* pLock);
        ~Locker();

    private:
        Lock* m_pLock;

        Locker(const Lock&);
    };
}

#endif
