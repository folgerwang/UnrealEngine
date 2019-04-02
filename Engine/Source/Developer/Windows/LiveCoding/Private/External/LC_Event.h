// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "Windows/MinimalWindowsApi.h"

// named/unnamed event.
// acts process-wide if given a name.
class Event
{
public:
	struct Type
	{
		enum Enum
		{
			MANUAL_RESET,
			AUTO_RESET
		};
	};

	Event(const wchar_t* name, Type::Enum type);
	~Event(void);

	void Signal(void);
	void Reset(void);
	void Wait(void);

	// returns true if the event was signaled
	bool WaitTimeout(unsigned int milliSeconds);

private:
	Windows::HANDLE m_event;
};
