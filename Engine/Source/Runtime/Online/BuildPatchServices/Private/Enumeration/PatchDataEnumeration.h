// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "BuildPatchSettings.h"

namespace BuildPatchServices
{
	class IPatchDataEnumeration
	{
	public:
		virtual ~IPatchDataEnumeration() {}
		virtual bool Run() = 0;
		virtual bool Run(TArray<FString>& OutFiles) = 0;
	};

	class FPatchDataEnumerationFactory
	{
	public:
		static IPatchDataEnumeration* Create(const FPatchDataEnumerationConfiguration& Configuration);
	};
}
