#pragma once
/* Copyright (c) 2014-2018 by Mercer Road Corp
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
#include <Windows.h>

namespace VivoxClientApi {

	///
	/// WindowsInvokeOnUIThread
	///
	/// This class is used to implement the InvokeOnUIThread method of IClientApiEventHandler
	///
    template<class T>
    class WindowsInvokeOnUIThread : public T
    {
    private:
        class RunOnUiThreadMessage {
        public:
            RunOnUiThreadMessage(void (* pf_func)(void *arg0), void *arg0) {
                m_pf_func = pf_func;
                m_arg0 = arg0;
            }
            void Execute() const
            {
                (*m_pf_func)(m_arg0);
            }
        private:

            void (* m_pf_func)(void *arg0);
            void *m_arg0;
        };

    public:
        WindowsInvokeOnUIThread(HINSTANCE hInst) : m_className("WindowsMarshaller") {
            m_hInstance = hInst;

            WNDCLASSEX wcex;

            memset(&wcex, 0, sizeof(wcex));
            wcex.cbSize = sizeof(WNDCLASSEX);

            wcex.lpfnWndProc = WndProc;
            wcex.hInstance = m_hInstance;
            wcex.lpszClassName = m_className;

            RegisterClassEx(&wcex);

            m_hWnd = CreateWindow(m_className, m_className, WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, HWND_MESSAGE, NULL, m_hInstance, NULL);

        }

        ~WindowsInvokeOnUIThread()
        {
            DestroyWindow(m_hWnd); 
            UnregisterClass(m_className, m_hInstance);
        }

        virtual void InvokeOnUIThread(void (pf_func)(void *arg0), void *arg0)
        {
            RunOnUiThreadMessage *message = new RunOnUiThreadMessage(pf_func, arg0);
            PostMessage(m_hWnd, WM_USER + 1, (WPARAM)message, NULL);
        }

        static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            switch (message)
            {
            case WM_USER + 1:
                {
                    RunOnUiThreadMessage *msg = (RunOnUiThreadMessage *)wParam;
                    msg->Execute();
                }
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
            return 0;
        }
    protected:
        const char *m_className;
        HWND m_hWnd;
        HINSTANCE m_hInstance;
    };

}