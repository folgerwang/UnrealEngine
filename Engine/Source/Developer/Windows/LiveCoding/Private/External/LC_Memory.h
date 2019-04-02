// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"

namespace memory
{
	template <typename T>
	inline void ReleaseAndNull(T*& instance)
	{
		if (instance)
		{
			instance->Release();
			instance = nullptr;
		}
	}


	template <typename T>
	inline void DeleteAndNull(T*& instance)
	{
		delete instance;
		instance = nullptr;
	}
}
