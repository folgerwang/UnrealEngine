// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
/*
Based on ScopeGuard presented at:
http://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Andrei-Alexandrescu-Systematic-Error-Handling-in-C
*/

#pragma once

#include "WebRTCProxyCommon.h"

template<class Func>
class TScopeGuard
{
public:
	TScopeGuard(Func F)
		: F(std::move(F))
		, Active(true)
	{
	}

	~TScopeGuard()
	{
		if (Active)
			F();
	}

	void Dismiss()
	{
		Active = false;
	}

	TScopeGuard() = delete;
	TScopeGuard(const TScopeGuard&) = delete;
	TScopeGuard& operator=(const TScopeGuard&) = delete;
	TScopeGuard(TScopeGuard&& Other)
		: F(std::move(Other.F))
		, Active(Other.Active)
	{
		Other.Dismiss();
	}

private:
	Func F;
	bool Active;
};


/**
	Using a template function to create guards, to make for shorter code.
	e.g:
	auto g1 = ScopeGuard( [&] { SomeCleanupCode(); } );
*/
template< class Func>
TScopeGuard<Func> ScopeGuard(Func F)
{
	return TScopeGuard<Func>(std::move(F));
}

/**
	Macros to be able to set anonymous scope guards. E.g:

	// some code ...
	SCOPE_EXIT { some cleanup code };
	// more code ...
	SCOPE_EXIT { more cleanup code };
	// more code ...
 */
namespace detail
{
	enum class EScopeGuardOnExit {};
	template <typename Func>
	__forceinline TScopeGuard<Func> operator+(EScopeGuardOnExit, Func&& F) {
		return TScopeGuard<Func>(std::forward<Func>(F));
	}
}

#define CONCATENATE_IMPL(S1,S2) S1##S2
#define CONCATENATE(S1,S2) CONCATENATE_IMPL(S1,S2)

// Note: __COUNTER__ Expands to an integer starting with 0 and incrementing by 1 every time it is used in a source file or included headers of the source file.
#ifdef __COUNTER__
	#define ANONYMOUS_VARIABLE_BypassRedefinition(Str) \
		CONCATENATE(Str,__COUNTER__)
#else
	#define ANONYMOUS_VARIABLE(Str) \
		CONCATENATE(Str,__LINE__)
#endif

#define SCOPE_EXIT \
	auto ANONYMOUS_VARIABLE_BypassRedefinition(SCOPE_EXIT_STATE) \
	= ::detail::EScopeGuardOnExit() + [&]()
