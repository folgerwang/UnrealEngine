// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionExampleResponse.h"

namespace GeometryCollectionExample
{

	void ExampleResponse::ExpectTrue(bool Condition, FString Reason) 
	{
		ErrorFlag |= !Condition;
		if (!Condition)
		{
			Reasons.Add(Reason);
		}
	}
}