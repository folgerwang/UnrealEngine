// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_MemoryBlock.h"
#include "LC_Logging.h"
#include "Windows/WindowsHWrapper.h"


MemoryBlock::MemoryBlock(size_t capacity)
	: m_capacity(capacity)
	, m_size(0u)
	, m_data(new uint8_t[capacity])
{
}


MemoryBlock::~MemoryBlock(void)
{
	delete[] m_data;
}


void MemoryBlock::Insert(const void* data, size_t size)
{
	LC_ASSERT(m_size + size <= m_capacity, "Not enough space to insert data.");

	memcpy(m_data + m_size, data, size);
	m_size += size;
}
