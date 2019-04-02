// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_Process.h"
#include "LC_Memory.h"
#include "LC_PointerUtil.h"
#include "LC_VirtualMemory.h"
#include "LC_Logging.h"
#include <Psapi.h>

// BEGIN EPIC MODS
#pragma warning(push)
#pragma warning(disable:6011) // warning C6011: Dereferencing NULL pointer 'processInfo'.
#pragma warning(disable:6335) // warning C6335: Leaking process information handle 'context->pi.hProcess'.
// END EPIC MODS

// internal ntdll.dll definitions
typedef LONG NTSTATUS;
typedef LONG KPRIORITY;

enum NT_SYSTEM_INFORMATION_CLASS
{
	SystemBasicInformation = 0,
	SystemPerformanceInformation = 2,
	SystemTimeOfDayInformation = 3,
	SystemProcessInformation = 5,
	SystemProcessorPerformanceInformation = 8,
	SystemHandleInformation = 16,
	SystemInterruptInformation = 23,
	SystemExceptionInformation = 33,
	SystemRegistryQuotaInformation = 37,
	SystemLookasideInformation = 45,
	SystemProcessIdInformation = 0x58
};

enum NT_KWAIT_REASON
{
	Executive,
	FreePage,
	PageIn,
	PoolAllocation,
	DelayExecution,
	Suspended,
	UserRequest,
	WrExecutive,
	WrFreePage,
	WrPageIn,
	WrPoolAllocation,
	WrDelayExecution,
	WrSuspended,
	WrUserRequest,
	WrEventPair,
	WrQueue,
	WrLpcReceive,
	WrLpcReply,
	WrVirtualMemory,
	WrPageOut,
	WrRendezvous,
	Spare2,
	Spare3,
	Spare4,
	Spare5,
	Spare6,
	WrKernel,
	MaximumWaitReason
};

struct NT_CLIENT_ID
{
	HANDLE UniqueProcess;
	HANDLE UniqueThread;
};

struct NT_SYSTEM_THREAD_INFORMATION
{
	LARGE_INTEGER KernelTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER CreateTime;
	ULONG WaitTime;
	PVOID StartAddress;
	NT_CLIENT_ID ClientId;
	KPRIORITY Priority;
	LONG BasePriority;
	ULONG ContextSwitches;
	ULONG ThreadState;
	NT_KWAIT_REASON WaitReason;
};

struct NT_UNICODE_STRING
{
	USHORT Length;
	USHORT MaximumLength;
	PWSTR Buffer;
};

struct NT_SYSTEM_PROCESS_INFORMATION
{
	ULONG uNext;
	ULONG uThreadCount;
	LARGE_INTEGER WorkingSetPrivateSize; // since VISTA
	ULONG HardFaultCount; // since WIN7
	ULONG NumberOfThreadsHighWatermark; // since WIN7
	ULONGLONG CycleTime; // since WIN7
	LARGE_INTEGER CreateTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER KernelTime;
	NT_UNICODE_STRING ImageName;
	KPRIORITY BasePriority;
	HANDLE uUniqueProcessId;
	HANDLE InheritedFromUniqueProcessId;
	ULONG HandleCount;
	ULONG SessionId;
	ULONG_PTR UniqueProcessKey; // since VISTA (requires SystemExtendedProcessInformation)
	SIZE_T PeakVirtualSize;
	SIZE_T VirtualSize;
	ULONG PageFaultCount;
	SIZE_T PeakWorkingSetSize;
	SIZE_T WorkingSetSize;
	SIZE_T QuotaPeakPagedPoolUsage;
	SIZE_T QuotaPagedPoolUsage;
	SIZE_T QuotaPeakNonPagedPoolUsage;
	SIZE_T QuotaNonPagedPoolUsage;
	SIZE_T PagefileUsage;
	SIZE_T PeakPagefileUsage;
	SIZE_T PrivatePageCount;
	LARGE_INTEGER ReadOperationCount;
	LARGE_INTEGER WriteOperationCount;
	LARGE_INTEGER OtherOperationCount;
	LARGE_INTEGER ReadTransferCount;
	LARGE_INTEGER WriteTransferCount;
	LARGE_INTEGER OtherTransferCount;
	NT_SYSTEM_THREAD_INFORMATION Threads[1];
};

enum NT_PROCESS_INFORMATION_CLASS
{
	ProcessBasicInformation,
	ProcessQuotaLimits,
	ProcessIoCounters,
	ProcessVmCounters,
	ProcessTimes,
	ProcessBasePriority,
	ProcessRaisePriority,
	ProcessDebugPort,
	ProcessExceptionPort,
	ProcessAccessToken,
	ProcessLdtInformation,
	ProcessLdtSize,
	ProcessDefaultHardErrorMode,
	ProcessIoPortHandlers,          // Note: this is kernel mode only
	ProcessPooledUsageAndLimits,
	ProcessWorkingSetWatch,
	ProcessUserModeIOPL,
	ProcessEnableAlignmentFaultFixup,
	ProcessPriorityClass,
	ProcessWx86Information,
	ProcessHandleCount,
	ProcessAffinityMask,
	ProcessPriorityBoost,
	ProcessDeviceMap,
	ProcessSessionInformation,
	ProcessForegroundInformation,
	ProcessWow64Information,
	ProcessImageFileName,
	ProcessLUIDDeviceMapsEnabled,
	ProcessBreakOnTermination,
	ProcessDebugObjectHandle,
	ProcessDebugFlags,
	ProcessHandleTracing,
	ProcessIoPriority,
	ProcessExecuteFlags,
	ProcessTlsInformation,
	ProcessCookie,
	ProcessImageInformation,
	ProcessCycleTime,
	ProcessPagePriority,
	ProcessInstrumentationCallback,
	ProcessThreadStackAllocation,
	ProcessWorkingSetWatchEx,
	ProcessImageFileNameWin32,
	ProcessImageFileMapping,
	ProcessAffinityUpdateMode,
	ProcessMemoryAllocationMode,
	ProcessGroupInformation,
	ProcessTokenVirtualizationEnabled,
	ProcessConsoleHostProcess,
	ProcessWindowInformation,
	MaxProcessInfoClass
};

struct RTL_USER_PROCESS_PARAMETERS
{
	BYTE Reserved1[16];
	PVOID Reserved2[10];
	NT_UNICODE_STRING ImagePathName;
	NT_UNICODE_STRING CommandLine;
};

struct NT_LDR_DATA_TABLE_ENTRY
{
	LIST_ENTRY InLoadOrderLinks;
	LIST_ENTRY InMemoryOrderLinks;
	LIST_ENTRY InInitializationOrderLinks;
	PVOID DllBase;
	PVOID EntryPoint;
	ULONG SizeOfImage;
	NT_UNICODE_STRING FullDllName;
	NT_UNICODE_STRING BaseDllName;
	ULONG Flags;
	USHORT LoadCount;
	USHORT ObsoleteLoadCount;
	USHORT TlsIndex;
	LIST_ENTRY HashLinks;
	ULONG TimeDateStamp;
};

struct NT_PEB_LDR_DATA
{
	ULONG Length;
	BOOLEAN Initialized;
	PVOID SsHandle;
	LIST_ENTRY InLoadOrderModuleList;
	LIST_ENTRY InMemoryOrderModuleList;
	LIST_ENTRY InInitializationOrderModuleList;
	PVOID EntryInProgress;
	BOOLEAN ShutdownInProgress;
	HANDLE ShutdownThreadId;
};

typedef VOID (NTAPI *NT_PS_POST_PROCESS_INIT_ROUTINE)(VOID);

// not the real full definition, but enough for our purposes
struct NT_PEB
{
	BYTE Reserved1[2];
	BYTE BeingDebugged;
	BYTE Reserved2[1];
	PVOID Reserved3[2];
	NT_PEB_LDR_DATA* Ldr;
	RTL_USER_PROCESS_PARAMETERS* ProcessParameters;
	PVOID Reserved4[3];
	PVOID AtlThunkSListPtr;
	PVOID Reserved5;
	ULONG Reserved6;
	PVOID Reserved7;
	ULONG Reserved8;
	ULONG AtlThunkSListPtr32;
	PVOID Reserved9[45];
	BYTE Reserved10[96];
	NT_PS_POST_PROCESS_INIT_ROUTINE* PostProcessInitRoutine;
	BYTE Reserved11[128];
	PVOID Reserved12[1];
	ULONG SessionId;
};

struct NT_PROCESS_BASIC_INFORMATION
{
	NTSTATUS ExitStatus;
	NT_PEB* PebBaseAddress;
	ULONG_PTR AffinityMask;
	KPRIORITY BasePriority;
	HANDLE UniqueProcessId;
	HANDLE InheritedFromUniqueProcessId;
};

#define NT_SUCCESS(Status)				((NTSTATUS)(Status) >= 0)
#define STATUS_INFO_LENGTH_MISMATCH		((NTSTATUS)0xC0000004L)


// these are undocumented functions, found in ntdll.dll.
// we don't call them directly, but use them for extracting their signature.
extern "C" NTSTATUS NtSuspendProcess(HANDLE ProcessHandle);
extern "C" NTSTATUS NtResumeProcess(HANDLE ProcessHandle);
extern "C" NTSTATUS NtWriteVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, ULONG NumberOfBytesToWrite, PULONG NumberOfBytesWritten);
extern "C" NTSTATUS NtQuerySystemInformation(NT_SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);
extern "C" NTSTATUS NtQueryInformationProcess(HANDLE ProcessHandle, NT_PROCESS_INFORMATION_CLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength);
extern "C" NTSTATUS NtContinue(CONTEXT* ThreadContext, BOOLEAN RaiseAlert);


namespace
{
	// helper class that allows us to call an undocumented function in any Windows DLL, as long as it is exported and we know its signature
	template <typename T>
	class UndocumentedFunction {};

	template <typename R, typename... Args>
	class UndocumentedFunction<R(Args...)>
	{
		typedef R (NTAPI *Function)(Args...);

	public:
		UndocumentedFunction(const char* moduleName, const char* functionName)
			: m_moduleName(moduleName)
			, m_functionName(functionName)
			, m_function(nullptr)
		{
			HMODULE module = ::GetModuleHandleA(moduleName);
			if (!module)
			{
				LC_ERROR_USER("Cannot get handle for module %s", moduleName);
				return;
			}

			m_function = reinterpret_cast<Function>(reinterpret_cast<uintptr_t>(::GetProcAddress(module, functionName)));
			if (!m_function)
			{
				LC_ERROR_USER("Cannot get address of function %s in module %s", functionName, moduleName);
			}
		}

		R operator()(Args... args) const
		{
			const NTSTATUS status = m_function(std::forward<Args>(args)...);
			if (!NT_SUCCESS(status))
			{
				LC_ERROR_USER("Call to function %s in module %s failed. Error: 0x%X", m_functionName, m_moduleName, status);
			}

			return status;
		}

	private:
		const char* m_moduleName;
		const char* m_functionName;
		Function m_function;
	};


	static uint32_t ConvertToExecutableProtection(uint32_t currentProtection)
	{
		// cut off PAGE_GUARD, PAGE_NOCACHE, PAGE_WRITECOMBINE, and PAGE_REVERT_TO_FILE_MAP
		const uint32_t extraBits = currentProtection & 0xFFFFFF00u;
		const uint32_t pageProtection = currentProtection & 0x000000FFu;

		switch (pageProtection)
		{
			case PAGE_NOACCESS:
			case PAGE_READONLY:
			case PAGE_READWRITE:
			case PAGE_WRITECOPY:
				return (pageProtection << 4u) | extraBits;

			case PAGE_EXECUTE:
			case PAGE_EXECUTE_READ:
			case PAGE_EXECUTE_READWRITE:
			case PAGE_EXECUTE_WRITECOPY:
			default:
				return currentProtection;
		}
	}
}


namespace undocumentedFunctions
{
	static UndocumentedFunction<decltype(NtSuspendProcess)> NtSuspendProcess("ntdll.dll", "NtSuspendProcess");
	static UndocumentedFunction<decltype(NtResumeProcess)> NtResumeProcess("ntdll.dll", "NtResumeProcess");
	static UndocumentedFunction<decltype(NtWriteVirtualMemory)> NtWriteVirtualMemory("ntdll.dll", "NtWriteVirtualMemory");
	static UndocumentedFunction<decltype(NtQuerySystemInformation)> NtQuerySystemInformation("ntdll.dll", "NtQuerySystemInformation");
	static UndocumentedFunction<decltype(NtQueryInformationProcess)> NtQueryInformationProcess("ntdll.dll", "NtQueryInformationProcess");
	static UndocumentedFunction<decltype(NtContinue)> NtContinue("ntdll.dll", "NtContinue");
}


namespace process
{
	static unsigned int __stdcall DrainPipe(void* data)
	{
		Context* context = static_cast<Context*>(data);

		std::vector<char> stdoutData;
		for (;;)
		{
			DWORD bytesRead = 0u;
			char buffer[256] = {};
			if (!::ReadFile(context->pipeReadEnd, buffer, sizeof(buffer) - 1u, &bytesRead, NULL))
			{
				// error while trying to read from the pipe, process has probably ended and closed its end of the pipe
				const DWORD error = ::GetLastError();
				if (error == ERROR_BROKEN_PIPE)
				{
					// this is expected
					break;
				}

				LC_ERROR_USER("Error 0x%X while reading from pipe", error);
				break;
			}

			stdoutData.insert(stdoutData.end(), buffer, buffer + bytesRead);
		}

		// convert stdout data to UTF16
		if (stdoutData.size() > 0u)
		{
			// cl.exe and link.exe write to stdout using the OEM codepage
			const int sizeNeeded = ::MultiByteToWideChar(CP_OEMCP, 0, stdoutData.data(), static_cast<int>(stdoutData.size()), NULL, 0);

			wchar_t* strTo = new wchar_t[static_cast<size_t>(sizeNeeded)];
			::MultiByteToWideChar(CP_OEMCP, 0, stdoutData.data(), static_cast<int>(stdoutData.size()), strTo, sizeNeeded);

			context->stdoutData.assign(strTo, static_cast<size_t>(sizeNeeded));
			delete[] strTo;
		}

		return 0u;
	}


	unsigned int GetId(void)
	{
		return ::GetCurrentProcessId();
	}


	Context* Spawn(const wchar_t* exePath, const wchar_t* workingDirectory, const wchar_t* commandLine, const void* environmentBlock, uint32_t flags)
	{
		Context* context = new Context { flags };

		::SECURITY_ATTRIBUTES saAttr;
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
		saAttr.bInheritHandle = Windows::TRUE;
		saAttr.lpSecurityDescriptor = NULL;

		::STARTUPINFOW startupInfo = {};
		startupInfo.cb = sizeof(startupInfo);

		HANDLE hProcessStdOutRead = NULL;
		HANDLE hProcessStdOutWrite = NULL;
		HANDLE hProcessStdErrWrite = NULL;

		if (flags & SpawnFlags::REDIRECT_STDOUT)
		{
			// create a STD_OUT pipe for the child process
			if (!CreatePipe(&hProcessStdOutRead, &hProcessStdOutWrite, &saAttr, 0))
			{
				LC_ERROR_USER("Cannot create stdout pipe. Error: 0x%X", ::GetLastError());
				delete context;
				return nullptr;
			}

			// create a duplicate of the STD_OUT write handle for the STD_ERR write handle. this is necessary in case the child
			// application closes one of its STD output handles.
			if (!::DuplicateHandle(::GetCurrentProcess(), hProcessStdOutWrite, ::GetCurrentProcess(),
				&hProcessStdErrWrite, 0, Windows::TRUE, DUPLICATE_SAME_ACCESS))
			{
				LC_ERROR_USER("Cannot duplicate stdout pipe. Error: 0x%X", ::GetLastError());
				::CloseHandle(hProcessStdOutRead);
				::CloseHandle(hProcessStdOutWrite);
				delete context;
				return nullptr;
			}

			// the spawned process will output data into the write-end of the pipe, and our process will read from the
			// read-end. because pipes can only do some buffering, we need to ensure that pipes never get clogged, otherwise
			// the spawned process could block due to the pipe being full.
			// therefore, we also create a new thread that continuously reads data from the pipe on our end.
			context->pipeReadEnd = hProcessStdOutRead;
			context->threadId = thread::Create(64u * 1024u, &DrainPipe, context);

			startupInfo.hStdOutput = hProcessStdOutWrite;
			startupInfo.hStdError = hProcessStdErrWrite;
			startupInfo.dwFlags = STARTF_USESTDHANDLES;
		}

		wchar_t* commandLineBuffer = nullptr;
		if (commandLine)
		{
			commandLineBuffer = new wchar_t[32768];
			wcscpy_s(commandLineBuffer, 32768u, commandLine);
		}

		LC_LOG_DEV("Spawning process:");
		{
			LC_LOG_INDENT_DEV;
			LC_LOG_DEV("Executable: %S", exePath);
			LC_LOG_DEV("Command line: %S", commandLineBuffer ? commandLineBuffer : L"none");
			LC_LOG_DEV("Working directory: %S", workingDirectory ? workingDirectory : L"none");
			LC_LOG_DEV("Custom environment block: %S", environmentBlock ? L"yes" : L"no");
		}

		// the environment block is not written to by CreateProcess, so it is safe to const_cast (it's a Win32 API mistake)
		const BOOL success = ::CreateProcessW(exePath, commandLineBuffer, NULL, NULL, Windows::TRUE, CREATE_NO_WINDOW, const_cast<void*>(environmentBlock), workingDirectory, &startupInfo, &context->pi);
		if (success == 0)
		{
			LC_ERROR_USER("Could not spawn process %S. Error: %d", exePath, ::GetLastError());
		}

		delete[] commandLineBuffer;

		if (flags & SpawnFlags::REDIRECT_STDOUT)
		{
			// we don't need those ends of the pipe
			::CloseHandle(hProcessStdOutWrite);
			::CloseHandle(hProcessStdErrWrite);
		}

		return context;
	}


	unsigned int Wait(Context* context)
	{
		// wait until process terminates
		::WaitForSingleObject(context->pi.hProcess, INFINITE);

		if (context->flags & SpawnFlags::REDIRECT_STDOUT)
		{
			// wait until all data is drained from the pipe
			thread::Join(context->threadId);
			thread::Close(context->threadId);

			// close remaining pipe handles
			::CloseHandle(context->pipeReadEnd);
		}

		DWORD exitCode = 0xFFFFFFFFu;
		::GetExitCodeProcess(context->pi.hProcess, &exitCode);

		return exitCode;
	}


	void Destroy(Context*& context)
	{
		::CloseHandle(context->pi.hProcess);
		::CloseHandle(context->pi.hThread);

		memory::DeleteAndNull(context);
	}


	void Terminate(Handle processHandle)
	{
		::TerminateProcess(processHandle, 0u);

		// termination is asynchronous, wait until the process is really gone
		::WaitForSingleObject(processHandle, INFINITE);
	}


	Handle Open(unsigned int processId)
	{
		return ::OpenProcess(PROCESS_ALL_ACCESS, Windows::FALSE, processId);
	}


	void Close(Handle& handle)
	{
		::CloseHandle(handle);
		handle = INVALID_HANDLE_VALUE;
	}


	std::wstring GetImagePath(Handle handle)
	{
		DWORD charCount = MAX_PATH + 1u;
		wchar_t processName[MAX_PATH + 1u] = {};
		::QueryFullProcessImageName(handle, 0u, processName, &charCount);

		return std::wstring(processName);
	}


	void* GetBase(void)
	{
		return ::GetModuleHandle(NULL);
	}


	std::wstring GetImagePath(void)
	{
		wchar_t filename[MAX_PATH + 1u] = {};
		::GetModuleFileNameW(NULL, filename, MAX_PATH + 1u);

		return std::wstring(filename);
	}


	uint32_t GetImageSize(Handle handle, void* moduleBase)
	{
		MODULEINFO info = {};
		::GetModuleInformation(handle, static_cast<HMODULE>(moduleBase), &info, sizeof(MODULEINFO));
		return info.SizeOfImage;
	}


	bool IsActive(Handle handle)
	{
		DWORD exitCode = 0u;
		const BOOL success = ::GetExitCodeProcess(handle, &exitCode);
		if ((success != 0) && (exitCode == STILL_ACTIVE))
		{
			return true;
		}

		// either the function has failed (because the process terminated unexpectedly) or the exit code
		// signals that the process exited already.
		return false;
	}

	
	void ReadProcessMemory(Handle handle, const void* srcAddress, void* destBuffer, size_t size)
	{
		const BOOL success = ::ReadProcessMemory(handle, srcAddress, destBuffer, size, NULL);
		if (success == 0)
		{
			LC_ERROR_USER("Cannot read %zu bytes from remote process at address 0x%p. Error: 0x%X", size, srcAddress, ::GetLastError());
		}
	}


	void WriteProcessMemory(Handle handle, void* destAddress, const void* srcBuffer, size_t size)
	{
		DWORD oldProtect = 0u;
		::VirtualProtectEx(handle, destAddress, size, PAGE_READWRITE, &oldProtect);
		{
			// instead of the regular WriteProcessMemory function, we use an undocumented function directly.
			// this is because Windows 10 introduced a performance regression that causes WriteProcessMemory to be 100 times slower (!)
			// than in previous versions of Windows.
			// this bug was reported here:
			// https://developercommunity.visualstudio.com/content/problem/228061/writeprocessmemory-slowdown-on-windows-10.html
			undocumentedFunctions::NtWriteVirtualMemory(handle, destAddress, const_cast<PVOID>(srcBuffer), static_cast<ULONG>(size), NULL);
		}
		::VirtualProtectEx(handle, destAddress, size, oldProtect, &oldProtect);
	}


	void* ScanMemoryRange(Handle handle, const void* lowerBound, const void* upperBound, size_t size, size_t alignment)
	{
		for (const void* scan = lowerBound; /* nothing */; /* nothing */)
		{
			// align address to be scanned
			scan = pointer::AlignTop<const void*>(scan, alignment);
			if (pointer::Offset<const void*>(scan, size) >= upperBound)
			{
				// outside of range to scan
				return nullptr;
			}
			else if (scan < lowerBound)
			{
				// outside of range (possible wrap-around)
				return nullptr;
			}

			MEMORY_BASIC_INFORMATION memoryInfo = {};
			::VirtualQueryEx(handle, scan, &memoryInfo, sizeof(MEMORY_BASIC_INFORMATION));

			if ((memoryInfo.RegionSize >= size) && (memoryInfo.State == MEM_FREE))
			{
				return memoryInfo.BaseAddress;
			}

			// keep on searching
			scan = pointer::Offset<const void*>(memoryInfo.BaseAddress, memoryInfo.RegionSize);
		}
	}


	void MakePagesExecutable(Handle handle, void* address, size_t size)
	{
		const uint32_t pageSize = virtualMemory::GetPageSize();
		const void* endOfRegion = pointer::Offset<const void*>(address, size);

		for (const void* scan = address; /* nothing */; /* nothing */)
		{
			MEMORY_BASIC_INFORMATION memoryInfo = {};
			const SIZE_T bytesInBuffer = ::VirtualQueryEx(handle, scan, &memoryInfo, sizeof(MEMORY_BASIC_INFORMATION));
			if (bytesInBuffer == 0u)
			{
				// could not query the protection, bail out
				break;
			}

			const uint32_t executableProtection = ConvertToExecutableProtection(memoryInfo.Protect);
			if (executableProtection != memoryInfo.Protect)
			{
				// change this page into an executable one
				DWORD oldProtection = 0u;
				::VirtualProtectEx(handle, memoryInfo.BaseAddress, pageSize, executableProtection, &oldProtection);
			}

			const void* endOfThisRegion = pointer::Offset<const void*>(memoryInfo.BaseAddress, pageSize);
			if (endOfThisRegion >= endOfRegion)
			{
				// we are done
				break;
			}

			// keep on walking pages
			scan = endOfThisRegion;
		}
	}


	void FlushInstructionCache(Handle handle, void* address, size_t size)
	{
		::FlushInstructionCache(handle, address, size);
	}


	void Suspend(Handle handle)
	{
		undocumentedFunctions::NtSuspendProcess(handle);
	}


	void Resume(Handle handle)
	{
		undocumentedFunctions::NtResumeProcess(handle);
	}


	void Continue(CONTEXT* threadContext)
	{
		undocumentedFunctions::NtContinue(threadContext, Windows::FALSE);
	}


	std::vector<unsigned int> EnumerateThreads(unsigned int processId)
	{
		std::vector<unsigned int> threadIds;
		threadIds.reserve(256u);

		// 2MB should be enough for getting the process information, even on systems with high load
		ULONG bufferSize = 2048u * 1024u;
		void* processSnapshot = nullptr;
		NTSTATUS status = 0;

		do
		{
			processSnapshot = ::malloc(bufferSize);

			// try getting a process snapshot into the provided buffer
			status = undocumentedFunctions::NtQuerySystemInformation(SystemProcessInformation, processSnapshot, bufferSize, NULL);

			if (status == STATUS_INFO_LENGTH_MISMATCH)
			{
				// buffer is too small, try again
				::free(processSnapshot);
				bufferSize *= 2u;
			}
			else if (status < 0)
			{
				// something went wrong
				LC_ERROR_USER("Cannot enumerate threads in process (PID: %d)", processId);
				::free(processSnapshot);
				
				return threadIds;
			}
		}
		while (status == STATUS_INFO_LENGTH_MISMATCH);

		// find the process information for the given process ID
		{
			NT_SYSTEM_PROCESS_INFORMATION* processInfo = static_cast<NT_SYSTEM_PROCESS_INFORMATION*>(processSnapshot);

			while (processInfo != nullptr)
			{
				if (processInfo->uUniqueProcessId == reinterpret_cast<HANDLE>(static_cast<DWORD_PTR>(processId)))
				{
					// we found the process we're looking for
					break;
				}

				if (processInfo->uNext == 0u)
				{
					// we couldn't find our process
					LC_ERROR_USER("Cannot enumerate threads, process not found (PID: %d)", processId);
					::free(processSnapshot);

					return threadIds;
				}
				else
				{
					// walk to the next process info
					processInfo = pointer::Offset<NT_SYSTEM_PROCESS_INFORMATION*>(processInfo, processInfo->uNext);
				}
			}

			// record all threads belonging to the given process
			for (ULONG i = 0u; i < processInfo->uThreadCount; ++i)
			{
				const DWORD threadId = static_cast<DWORD>(reinterpret_cast<DWORD_PTR>(processInfo->Threads[i].ClientId.UniqueThread));
				threadIds.push_back(threadId);
			}
		}

		::free(processSnapshot);

		return threadIds;
	}


	std::vector<Module> EnumerateModules(Handle handle)
	{
		std::vector<Module> modules;
		modules.reserve(256u);

		NT_PROCESS_BASIC_INFORMATION pbi = {};
		undocumentedFunctions::NtQueryInformationProcess(handle, ProcessBasicInformation, &pbi, sizeof(pbi), NULL);

		NT_PEB processPEB = {};
		ReadProcessMemory(handle, pbi.PebBaseAddress, &processPEB, sizeof(NT_PEB));

		NT_PEB_LDR_DATA loaderData = {};
		ReadProcessMemory(handle, processPEB.Ldr, &loaderData, sizeof(NT_PEB_LDR_DATA));

		LIST_ENTRY* listHeader = loaderData.InLoadOrderModuleList.Flink;
		LIST_ENTRY* currentNode = listHeader;
		do
		{
			NT_LDR_DATA_TABLE_ENTRY entry = {};
			ReadProcessMemory(handle, currentNode, &entry, sizeof(NT_LDR_DATA_TABLE_ENTRY));

			currentNode = entry.InLoadOrderLinks.Flink;

			WCHAR fullDllName[MAX_PATH] = {};
			if (entry.FullDllName.Length > 0)
			{
				ReadProcessMemory(handle, entry.FullDllName.Buffer, fullDllName, entry.FullDllName.Length);
			}

			modules.emplace_back(Module { fullDllName, entry.DllBase, entry.SizeOfImage });
		}
		while (listHeader != currentNode);

		return modules;
	}


	void DumpMemory(Handle handle, const void* address, size_t size)
	{
		uint8_t* memory = new uint8_t[size];
		ReadProcessMemory(handle, address, memory, size);

		LC_LOG_DEV("Raw data:");
		LC_LOG_INDENT_DEV;
		for (size_t i = 0u; i < size; ++i)
		{
			LC_LOG_DEV("0x%02X", memory[i]);
		}

		delete[] memory;
	}
}

// BEGIN EPIC MODS
#pragma warning(pop)
// END EPIC MODS
