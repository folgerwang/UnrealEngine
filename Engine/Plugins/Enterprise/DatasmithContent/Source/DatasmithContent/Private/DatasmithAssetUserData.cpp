// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DatasmithAssetUserData.h"

#include "DatasmithContentModule.h"

#if WITH_EDITORONLY_DATA

bool UDatasmithAssetUserData::IsPostLoadThreadSafe() const
{
	return true;
}

void UDatasmithAssetUserData::PostLoad()
{
	Super::PostLoad();

	// RF_Transactional flag can cause a crash on save for Blueprint instances, and old data was flagged.
	ClearFlags(RF_Transactional);

	// A serialization issue caused nullptr to be serialized instead of valid UDatasmithObjectTemplate pointers.
	// This cleanup ensure values from this map can always be dereferenced
	for (auto It = ObjectTemplates.CreateIterator(); It; ++It)
	{
		if (!It->Value)
		{
			It.RemoveCurrent();
			UE_LOG(LogDatasmithContent, Warning, TEXT("Serialization issue: null value found in templates"))
		}
	}
}

#endif // WITH_EDITORONLY_DATA

