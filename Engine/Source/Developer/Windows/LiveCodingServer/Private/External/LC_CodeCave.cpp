// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_CodeCave.h"
#include "LC_VirtualMemory.h"
#include "LC_Patch.h"
#include "LC_Thread.h"


CodeCave::CodeCave(process::Handle processHandle, unsigned int processId, unsigned int commandThreadId)
	: m_processHandle(processHandle)
	, m_processId(processId)
	, m_commandThreadId(commandThreadId)
	, m_cave(nullptr)
	, m_perThreadData()
{
	m_perThreadData.reserve(128u);
}


void CodeCave::Install(void)
{
	process::Suspend(m_processHandle);

	// prepare jump-to-self code cave
	const uint32_t pageSize = virtualMemory::GetPageSize();
	m_cave = virtualMemory::Allocate(m_processHandle, pageSize, virtualMemory::PageType::EXECUTE_READ_WRITE);
	patch::InstallJumpToSelf(m_processHandle, m_cave);

	// enumerate all threads of the process now that it's suspended
	const std::vector<unsigned int>& threadIds = process::EnumerateThreads(m_processId);
	
	const size_t threadCount = threadIds.size();
	m_perThreadData.resize(threadCount);

	// set all threads' instruction pointers into the code cave.
	// additionally, we set the threads' priority to IDLE so that they don't burn CPU cycles,
	// which could totally starve all CPUs and the OS, depending on how many threads
	// are currently running.
	for (size_t i = 0u; i < threadCount; ++i)
	{
		const unsigned id = threadIds[i];
		m_perThreadData[i].id = id;

		if (id == m_commandThreadId)
		{
			// this is the Live++ command thread, don't put it into the cave
			continue;
		}

		thread::Handle threadHandle = thread::Open(id);
		thread::Context context = thread::GetContext(threadHandle);
		m_perThreadData[i].priority = thread::GetPriority(threadHandle);
		m_perThreadData[i].originalIp = thread::ReadInstructionPointer(context);
		thread::SetPriority(threadHandle, THREAD_PRIORITY_IDLE);
		thread::WriteInstructionPointer(context, m_cave);
		thread::SetContext(threadHandle, context);
		thread::Close(threadHandle);
	}

	// let the process resume. all threads except the Live++ thread will be held in the code cave
	process::Resume(m_processHandle);
}


void CodeCave::Uninstall(void)
{
	process::Suspend(m_processHandle);

	// restore original thread instruction pointers
	const size_t threadCount = m_perThreadData.size();
	for (size_t i = 0u; i < threadCount; ++i)
	{
		const unsigned id = m_perThreadData[i].id;
		if (id == m_commandThreadId)
		{
			// this is the Live++ command thread
			continue;
		}

		thread::Handle threadHandle = thread::Open(id);
		thread::Context context = thread::GetContext(threadHandle);
		const void* currentIp = thread::ReadInstructionPointer(context);

		// only set the original instruction pointer if the thread is really being held in the cave.
		// in certain situations (e.g. after an exception), the debugger/OS already restored the context
		// of all threads, and it would be fatal to interfere with this.
		if (currentIp == m_cave)
		{
			thread::SetPriority(threadHandle, m_perThreadData[i].priority);
			thread::WriteInstructionPointer(context, m_perThreadData[i].originalIp);
			thread::SetContext(threadHandle, context);
		}
		thread::Close(threadHandle);
	}

	// get rid of the code cave
	virtualMemory::Free(m_processHandle, m_cave);

	process::Resume(m_processHandle);
}
