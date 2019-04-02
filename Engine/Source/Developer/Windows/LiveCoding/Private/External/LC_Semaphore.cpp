// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_Semaphore.h"
#include "LC_Logging.h"
#include "Windows/WindowsHWrapper.h"


Semaphore::Semaphore(unsigned int initialValue, unsigned int maximumValue) :
	m_sema(::CreateSemaphore(nullptr, static_cast<LONG>(initialValue), static_cast<LONG>(maximumValue), nullptr))
{
}


Semaphore::~Semaphore(void)
{
	::CloseHandle(m_sema);
}


void Semaphore::Signal(void)
{
	::ReleaseSemaphore(m_sema, 1, nullptr);
}


void Semaphore::Wait(void)
{
	const DWORD result = ::WaitForSingleObject(m_sema, INFINITE);
	switch (result)
	{
		case WAIT_OBJECT_0:
			// semaphore was successfully signaled
			break;

		case WAIT_TIMEOUT:
			// the operation timed out, which should never happen with a timeout of INFINITE
			LC_ERROR_DEV("Semaphore timed out.");
			break;

		case WAIT_ABANDONED:
			LC_ERROR_DEV("Wait() was called on a stale semaphore which was not released by the owning thread.");
			break;

		case WAIT_FAILED:
			LC_ERROR_DEV("Failed to Wait() on a semaphore.");
			break;

		default:
			break;
	}
}


bool Semaphore::TryWait(void)
{
	const DWORD result = ::WaitForSingleObject(m_sema, 0);
	switch (result)
	{
		case WAIT_OBJECT_0:
			// semaphore was successfully signaled
			return true;

		case WAIT_TIMEOUT:
			return false;

		case WAIT_ABANDONED:
			LC_ERROR_DEV("Wait() was called on a stale semaphore which was not released by the owning thread.");
			return false;

		case WAIT_FAILED:
			LC_ERROR_DEV("Failed to Wait() on a semaphore.");
			return false;

		default:
			return false;
	}
}
