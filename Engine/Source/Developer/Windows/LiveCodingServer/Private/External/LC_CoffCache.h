// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Coff.h"
#include "LC_CriticalSection.h"


template <typename T>
class CoffCache
{
public:
	CoffCache(void);
	~CoffCache(void);

	// updates an entry in the cache. if it already exists, the existing one will be deleted.
	// takes ownership over the database.
	// thread-safe.
	void Update(const ImmutableString& coffIdentifier, T* database);

	// returns a database associated with a certain COFF id.
	const T* Lookup(const ImmutableString& coffIdentifier) const;

	bool Contains(const ImmutableString& coffIdentifier) const;

private:
	mutable CriticalSection m_cs;

	typedef types::StringMap<T*> Cache;
	Cache m_cache;
};
