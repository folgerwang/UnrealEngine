// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "HeaderProvider.h"
#include "UnrealHeaderTool.h"
#include "UnrealTypeDefinitionInfo.h"
#include "ClassMaps.h"

FHeaderProvider::FHeaderProvider(EHeaderProviderSourceType InType, FString&& InId)//, bool bInAutoInclude/* = false*/)
	: Type(InType)
	, Id(MoveTemp(InId))
	, Cache(nullptr)
{

}

FUnrealSourceFile* FHeaderProvider::Resolve()
{
	if (Type != EHeaderProviderSourceType::Resolved)
	{
		if (Type == EHeaderProviderSourceType::ClassName)
		{
			FName IdName(*Id);
			for (const auto& Pair : GTypeDefinitionInfoMap)
			{
				if (Pair.Key->GetFName() == IdName)
				{
					Cache = &Pair.Value->GetUnrealSourceFile();
					break;
				}
			}
		}
		else if (const TSharedRef<FUnrealSourceFile>* Source = GUnrealSourceFilesMap.Find(Id))
		{
			Cache = &Source->Get();
		}

		Type = EHeaderProviderSourceType::Resolved;
	}

	return Cache;
}

FString FHeaderProvider::ToString() const
{
	return FString::Printf(TEXT("%s %s"), Type == EHeaderProviderSourceType::ClassName ? TEXT("class") : TEXT("file"), *Id);
}

const FString& FHeaderProvider::GetId() const
{
	return Id;
}

bool operator==(const FHeaderProvider& A, const FHeaderProvider& B)
{
	return A.Id == B.Id && A.Type == B.Type;
}
