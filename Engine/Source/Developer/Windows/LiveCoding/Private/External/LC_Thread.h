// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "Windows/WindowsHWrapper.h"

namespace thread
{
	typedef CONTEXT Context;
	typedef HANDLE Handle;
	typedef unsigned int(__stdcall *Function)(void*);

	// returns the thread ID of the calling thread
	unsigned int GetId(void);

	// returns the thread ID of the given thread
	unsigned int GetId(Handle handle);

	Handle Create(unsigned int stackSize, Function function, void* context);
	void Join(Handle handle);
	void Terminate(Handle handle);

	void Yield(void);
	void Sleep(unsigned int milliSeconds);

	void CancelIO(Handle handle);


	// opens a thread
	Handle Open(unsigned int threadId);

	// closes a thread
	void Close(Handle& handle);

	// suspends a thread
	void Suspend(Handle handle);

	// resumes a thread
	void Resume(Handle handle);

	// returns a thread's priority
	int GetPriority(Handle handle);

	// sets a thread's priority
	void SetPriority(Handle handle, int priority);

	// returns a thread's context.
	// NOTE: only use on suspended threads!
	Context GetContext(Handle handle);

	// sets a thread's context.
	// NOTE: only use on suspended threads!
	void SetContext(Handle handle, const Context& context);

	// reads a context' instruction pointer
	const void* ReadInstructionPointer(const Context& context);

	// writes a context' instruction pointer
	void WriteInstructionPointer(Context& context, const void* ip);


	// sets the name of the calling thread
	void SetName(const char* name);
}
