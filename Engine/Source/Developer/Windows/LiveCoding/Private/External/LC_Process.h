// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Thread.h"
#include <string>
#include <vector>

namespace process
{
	struct Context
	{
		uint32_t flags;
		HANDLE pipeReadEnd;
		PROCESS_INFORMATION pi;
		thread::Handle threadId;
		std::wstring stdoutData;
	};

	struct Module
	{
		std::wstring fullPath;
		void* baseAddress;
		uint32_t sizeOfImage;
	};

	typedef HANDLE Handle;

	// returns the process ID for the calling process
	unsigned int GetId(void);


	struct SpawnFlags
	{
		enum Enum : uint32_t
		{
			NONE = 0u,
			REDIRECT_STDOUT = 1u << 0u
		};
	};

	// spawns a new process
	Context* Spawn(const wchar_t* exePath, const wchar_t* workingDirectory, const wchar_t* commandLine, const void* environmentBlock, uint32_t flags);

	// waits until a spawned process has exited
	unsigned int Wait(Context* context);

	// destroys a spawned process
	void Destroy(Context*& context);

	// terminates a spawned process
	void Terminate(Handle processHandle);



	// opens a process
	Handle Open(unsigned int processId);

	// closes a process
	void Close(Handle& handle);

	// returns the full path for a process' image
	std::wstring GetImagePath(Handle handle);

	// returns the base address of the calling process
	void* GetBase(void);

	// returns the path to the executable of the calling process
	std::wstring GetImagePath(void);

	// returns the size of a module loaded into the virtual address space of a given process
	uint32_t GetImageSize(Handle handle, void* moduleBase);

	// returns whether the process with the given handle is still active
	bool IsActive(Handle handle);



	// reads from process memory
	void ReadProcessMemory(Handle handle, const void* srcAddress, void* destBuffer, size_t size);

	template <typename T>
	T ReadProcessMemory(Handle handle, const void* srcAddress)
	{
		T value = {};
		ReadProcessMemory(handle, srcAddress, &value, sizeof(T));

		return value;
	}

	// writes to process memory
	void WriteProcessMemory(Handle handle, void* destAddress, const void* srcBuffer, size_t size);

	template <typename T>
	void WriteProcessMemory(Handle handle, void* destAddress, const T& value)
	{
		WriteProcessMemory(handle, destAddress, &value, sizeof(T));
	}

	// scans a region of memory in the given process until a free block of a given size is found.
	// will only consider blocks at addresses with a certain alignment.
	void* ScanMemoryRange(Handle handle, const void* lowerBound, const void* upperBound, size_t size, size_t alignment);

	// makes the memory pages in the given region executable (in case they aren't already) while keeping other protection flags intact
	void MakePagesExecutable(Handle handle, void* address, size_t size);



	// flushes the process' instruction cache
	void FlushInstructionCache(Handle handle, void* address, size_t size);



	// suspends a process
	void Suspend(Handle handle);

	// resumes a suspended process
	void Resume(Handle handle);

	// continues the calling thread of a process with the given thread context
	void Continue(CONTEXT* threadContext);


	// enumerates all threads of a process, returning their thread IDs.
	// NOTE: only call on suspended processes!
	std::vector<unsigned int> EnumerateThreads(unsigned int processId);

	// enumerates all modules of a process, returning their info.
	// NOTE: only call on suspended processes!
	std::vector<Module> EnumerateModules(Handle handle);


	// dumps raw memory for a given process
	void DumpMemory(Handle handle, const void* address, size_t size);
}
