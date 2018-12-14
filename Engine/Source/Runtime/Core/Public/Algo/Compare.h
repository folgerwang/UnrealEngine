// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"

namespace Algo
{
	/**
	* Compares entries in the container using a user-defined predicate container
	*
	* @param  InputA     Container used as baseline
	* @param  InputB     Container to compare against
	* @param  Predicate  Condition which returns true for elements which are deemed equal
	*/
	template <typename InT, typename PredicateT>
	FORCEINLINE bool CompareByPredicate(const InT& InputA, const InT& InputB, PredicateT Predicate)
	{
		if (InputA.Num() == InputB.Num())
		{
			uint32 Count = GetNum(InputA);

			auto* A = GetData(InputA);
			auto* B = GetData(InputB);

			while (Count)
			{
				if (!(Invoke(Predicate, *A, *B)))
				{
					return false;
				}

				++A;
				++B;
				--Count;
			}

			return true;
		}

		return false;
	}
}
