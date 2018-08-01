// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterEditorSettings.h"
#include "DisplayClusterEditorEngine.h"
#include "Misc/ConfigCacheIni.h"


UDisplayClusterEditorSettings::UDisplayClusterEditorSettings(class FObjectInitializer const & ObjectInitializer)
	: Super(ObjectInitializer) 
{
	GET_MEMBER_NAME_CHECKED(UDisplayClusterEditorSettings, bEnabled);
}

#if WITH_EDITOR
void UDisplayClusterEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		FName PropertyName(PropertyChangedEvent.Property->GetFName());
		FString DefaultEnginePath = FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir());

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UDisplayClusterEditorSettings, bEnabled))
		{
			if (bEnabled)
			{
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("GameEngine"), TEXT("/Script/DisplayCluster.DisplayClusterGameEngine"), DefaultEnginePath);
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("UnrealEdEngine"), TEXT("/Script/DisplayClusterEditor.DisplayClusterEditorEngine"), DefaultEnginePath);
			}
			else
			{
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("GameEngine"), TEXT("/Script/Engine.GameEngine"), DefaultEnginePath);
				GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("UnrealEdEngine"), TEXT("/Script/UnrealEd.UnrealEdEngine"), DefaultEnginePath);
			}

			GConfig->Flush(false, DefaultEnginePath);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
