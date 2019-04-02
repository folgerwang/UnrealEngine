// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include <stdint.h>

// thread-safe
class AllocatorStats
{
public:
	AllocatorStats(void);

	void RegisterAllocation(size_t size);
	void UnregisterAllocation(size_t size);

	void Merge(const AllocatorStats& stats);

	void Print(const char* name) const;

	uint64_t GetAllocationCount(void) const;
	uint64_t GetMemorySize(void) const;

private:
	uint64_t m_allocationCount;
	uint64_t m_memorySize;
};
