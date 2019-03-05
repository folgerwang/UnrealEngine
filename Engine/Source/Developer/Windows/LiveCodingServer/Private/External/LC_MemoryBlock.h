// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Platform.h"
#include <stdint.h>


class MemoryBlock
{
public:
	explicit MemoryBlock(size_t capacity);
	~MemoryBlock(void);

	void Insert(const void* data, size_t size);

	template <typename T>
	inline void Insert(const T& data)
	{
		Insert(&data, sizeof(T));
	}

	inline const void* GetData(void) const
	{
		return m_data;
	}

	inline size_t GetSize(void) const
	{
		return m_size;
	}

private:
	size_t m_capacity;
	size_t m_size;
	uint8_t* m_data;

	LC_DISABLE_COPY(MemoryBlock);
	LC_DISABLE_MOVE(MemoryBlock);
	LC_DISABLE_ASSIGNMENT(MemoryBlock);
	LC_DISABLE_MOVE_ASSIGNMENT(MemoryBlock);
};
