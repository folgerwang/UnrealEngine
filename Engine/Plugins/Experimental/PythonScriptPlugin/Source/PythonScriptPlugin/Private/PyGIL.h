// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IncludePython.h"

#if WITH_PYTHON

/** Utility to handle taking a releasing the Python GIL within a scope */
class FPyScopedGIL
{
public:
	/** Constructor - take the GIL */
	FPyScopedGIL()
		: GILState(PyGILState_Ensure())
	{
	}

	/** Destructor - release the GIL */
	~FPyScopedGIL()
	{
		PyGILState_Release(GILState);
	}

	/** Non-copyable */
	FPyScopedGIL(const FPyScopedGIL&) = delete;
	FPyScopedGIL& operator=(const FPyScopedGIL&) = delete;

private:
	/** Internal GIL state */
	PyGILState_STATE GILState;
};

#endif	// WITH_PYTHON
