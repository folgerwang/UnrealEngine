// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace GeometryCollectionExample
{
	class GEOMETRYCOLLECTIONEXAMPLECORE_API ExampleResponse
	{
	public:
		ExampleResponse() : ErrorFlag(false) {}
		virtual ~ExampleResponse() {}
		virtual void ExpectTrue(bool, FString Reason = "");
		virtual bool HasError() { return ErrorFlag; }
		TArray<FString> Reasons;
		bool ErrorFlag;
	};

}