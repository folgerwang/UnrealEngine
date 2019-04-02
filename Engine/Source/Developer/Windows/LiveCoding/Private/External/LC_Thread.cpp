// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_Thread.h"
#include "LC_Platform.h"
#include "LC_Logging.h"
#include <process.h>

// BEGIN EPIC MODS
#pragma warning(push)
#pragma warning(disable:6322) // warning C6322: Empty _except block.
#pragma warning(disable:6258) // warning C6258: Using TerminateThread does not allow proper thread clean up.
// END EPIC MODS

namespace
{
	static const DWORD MS_VC_EXCEPTION = 0x406D1388;

	struct THREADNAME_INFO
	{
		ULONG_PTR dwType;		// Must be 0x1000.
		ULONG_PTR szName;		// Pointer to name (in user addr space).
		ULONG_PTR dwThreadID;	// Thread ID (-1 = caller thread).
		ULONG_PTR dwFlags;		// Reserved for future use, must be zero.
	};


	static void SetThreadName(const char* threadName)
	{
		// code for setting a thread's name taken from http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
		// note that the pragma directives in the code sample are wrong, and will not work for 32-bit builds.
		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = reinterpret_cast<ULONG_PTR>(threadName);
		info.dwThreadID = static_cast<ULONG_PTR>(-1);
		info.dwFlags = 0;

		__try
		{
			RaiseException(MS_VC_EXCEPTION, 0, 4u, reinterpret_cast<ULONG_PTR*>(&info));
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}
}


namespace thread
{
	unsigned int GetId(void)
	{
		return ::GetCurrentThreadId();
	}


	unsigned int GetId(Handle handle)
	{
		return ::GetThreadId(handle);
	}


	Handle Create(unsigned int stackSize, Function function, void* context)
	{
		const uintptr_t result = _beginthreadex(nullptr, stackSize, function, context, 0u, nullptr);
		if (result == 0u)
		{
			const DWORD error = ::GetLastError();
			LC_ERROR_USER("Error 0x%X while trying to create thread", error);
		}

		return reinterpret_cast<Handle>(result);
	}


	void Join(Handle handle)
	{
		::WaitForSingleObject(handle, INFINITE);
	}


	void Terminate(Handle handle)
	{
		::TerminateThread(handle, 0u);
	}


	void Yield(void)
	{
		::_mm_pause();
	}


	void Sleep(unsigned int milliSeconds)
	{
		::Sleep(milliSeconds);
	}


	void CancelIO(Handle handle)
	{
		::CancelSynchronousIo(handle);
	}


	Handle Open(unsigned int threadId)
	{
		return ::OpenThread(THREAD_ALL_ACCESS, Windows::FALSE, threadId);
	}


	void Close(Handle& handle)
	{
		::CloseHandle(handle);
		handle = INVALID_HANDLE_VALUE;
	}


	void Suspend(Handle handle)
	{
		::SuspendThread(handle);
	}


	void Resume(Handle handle)
	{
		::ResumeThread(handle);
	}


	int GetPriority(Handle handle)
	{
		return ::GetThreadPriority(handle);
	}


	void SetPriority(Handle handle, int priority)
	{
		::SetThreadPriority(handle, priority);
	}


	Context GetContext(Handle handle)
	{
		CONTEXT threadContext = {};
		threadContext.ContextFlags = CONTEXT_ALL;
		::GetThreadContext(handle, &threadContext);

		return threadContext;
	}


	void SetContext(Handle handle, const Context& context)
	{
		::SetThreadContext(handle, &context);
	}


	const void* ReadInstructionPointer(const Context& context)
	{
#if LC_64_BIT
		return reinterpret_cast<const void*>(context.Rip);
#else
		return reinterpret_cast<const void*>(context.Eip);
#endif
	}


	void WriteInstructionPointer(Context& context, const void* ip)
	{
#if LC_64_BIT
		context.Rip = reinterpret_cast<DWORD64>(ip);
#else
		context.Eip = reinterpret_cast<DWORD>(ip);
#endif
	}


	void SetName(const char* name)
	{
		SetThreadName(name);
	}
}

// BEGIN EPIC MODS
#pragma warning(pop)
// END EPIC MODS
