// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AdvancedCopyCustomization.h"
#include "Containers/UnrealString.h"


#define LOCTEXT_NAMESPACE "AdvancedCopyCustomization"


UAdvancedCopyCustomization::UAdvancedCopyCustomization(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bShouldGenerateRelativePaths(true)
{
	FilterForExcludingDependencies.PackagePaths.Add(TEXT("/Engine"));
	FilterForExcludingDependencies.PackagePaths.Add(TEXT("/Script"));
	FilterForExcludingDependencies.bRecursivePaths = true;
	FilterForExcludingDependencies.bRecursiveClasses = true;
}

void UAdvancedCopyCustomization::SetPackageThatInitiatedCopy(const FString& InBasePackage)
{
	FString TempPackage = InBasePackage;
	if (!TempPackage.EndsWith(TEXT("/")))
	{
		TempPackage += TEXT("/");
	}
	PackageThatInitiatedCopy = TempPackage;
}

#undef LOCTEXT_NAMESPACE
