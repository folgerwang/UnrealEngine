// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_Event.h"
#include "LC_Logging.h"
#include "Windows/WindowsHWrapper.h"


Event::Event(const wchar_t* name, Type::Enum type)
	: m_event(::CreateEventW(NULL, (type == Type::MANUAL_RESET) ? Windows::TRUE : Windows::FALSE, Windows::FALSE, name))
{
	const DWORD error = ::GetLastError();
	if (m_event == NULL)
	{
		LC_ERROR_USER("Cannot create event %S. Error: 0x%X", name ? name : L"(unnamed)", error);
	}
	else if (error == ERROR_ALREADY_EXISTS)
	{
		// another process already created this event, this is to be expected
	}
}


Event::~Event(void)
{
	::CloseHandle(m_event);
}


void Event::Signal(void)
{
	::SetEvent(m_event);
}


void Event::Reset(void)
{
	::ResetEvent(m_event);
}


void Event::Wait(void)
{
	const DWORD result = ::WaitForSingleObject(m_event, INFINITE);
	switch (result)
	{
		case WAIT_OBJECT_0:
			// event was successfully signaled
			break;

		case WAIT_TIMEOUT:
			// the operation timed out, which should never happen with a timeout of INFINITE
			LC_ERROR_DEV("Event timed out.");
			break;

		case WAIT_ABANDONED:
			LC_ERROR_DEV("Wait() was called on a stale event which was not released by the owning thread.");
			break;

		case WAIT_FAILED:
			LC_ERROR_DEV("Failed to Wait() on an event.");
			break;

		default:
			break;
	}
}


bool Event::WaitTimeout(unsigned int milliSeconds)
{
	const DWORD result = ::WaitForSingleObject(m_event, milliSeconds);
	switch (result)
	{
		case WAIT_OBJECT_0:
			return true;

		case WAIT_TIMEOUT:
			return false;

		case WAIT_ABANDONED:
			LC_ERROR_DEV("Wait() was called on a stale event which was not released by the owning thread.");
			return false;

		case WAIT_FAILED:
			LC_ERROR_DEV("Failed to Wait() on an event.");
			return false;

		default:
			return false;
	}
}
