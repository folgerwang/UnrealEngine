// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlackmagicMediaViewportOutput.h"
#include "BlackmagicMediaViewportOutputImpl.h"
#include "IBlackmagicMediaOutputModule.h"

#include "Engine/GameEngine.h"
#include "Misc/Timecode.h"
#include "Misc/FrameRate.h"
#include "Widgets/SViewport.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif //WITH_EDITOR

/* namespace BlackmagicMediaOutputDevice definition
*****************************************************************************/

namespace BlackmagicMediaOutputDevice
{
	bool FindSceneViewportAndLevel(TSharedPtr<FSceneViewport>& OutSceneViewport, ULevel*& OutLevel);
}

/* UBlackmagicMediaViewportOutput
*****************************************************************************/

UBlackmagicMediaViewportOutput::UBlackmagicMediaViewportOutput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Implementation(nullptr)
{
}

void UBlackmagicMediaViewportOutput::BeginDestroy()
{
	DeactivateOutput();
	Super::BeginDestroy();
}

ETickableTickType UBlackmagicMediaViewportOutput::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UBlackmagicMediaViewportOutput::IsTickable() const
{
	return Implementation.IsValid();
}

void UBlackmagicMediaViewportOutput::Tick(float DeltatTime)
{
	if (Implementation.IsValid())
	{
		Implementation->Tick(FApp::GetTimecode());
	}
}

void UBlackmagicMediaViewportOutput::ActivateOutput(UBlackmagicMediaOutput* MediaOutput)
{
	DeactivateOutput();

	if (MediaOutput != nullptr)
	{
		TSharedPtr<FSceneViewport> FoundSceneViewport;
		ULevel* Level = nullptr;
		if (BlackmagicMediaOutputDevice::FindSceneViewportAndLevel(FoundSceneViewport, Level) && FoundSceneViewport.IsValid())
		{
			Implementation = FBlackmagicMediaViewportOutputImpl::CreateShared(MediaOutput, FoundSceneViewport);
			if (!Implementation.IsValid())
			{
				UE_LOG(LogBlackmagicMediaOutput, Error, TEXT("Could not initialized the Output interface."));
				DeactivateOutput();
			}
		}
		else
		{
			UE_LOG(LogBlackmagicMediaOutput, Warning, TEXT("No viewport could be found. Play in 'Standalone' or in 'New Editor Window PIE'."));
		}
	}
	else
	{
		UE_LOG(LogBlackmagicMediaOutput, Error, TEXT("Couldn't start the capture. No Media Output was provided."));
	}
}

void UBlackmagicMediaViewportOutput::DeactivateOutput()
{
	if (Implementation.IsValid())
	{
		Implementation->Shutdown();
		Implementation.Reset();
	}
}

/* namespace AjaMediaOutputDevice implementation
*****************************************************************************/
namespace BlackmagicMediaOutputDevice
{
	bool FindSceneViewportAndLevel(TSharedPtr<FSceneViewport>& OutSceneViewport, ULevel*& OutLevel)
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE)
				{
					UEditorEngine* EditorEngine = CastChecked<UEditorEngine>(GEngine);
					FSlatePlayInEditorInfo& Info = EditorEngine->SlatePlayInEditorMap.FindChecked(Context.ContextHandle);
					if (Info.SlatePlayInEditorWindowViewport.IsValid())
					{
						OutLevel = Context.World()->GetCurrentLevel();
						OutSceneViewport = Info.SlatePlayInEditorWindowViewport;
						return true;
					}
				}
			}
			return false;
		}
		else
#endif
		{
			UGameEngine* GameEngine = CastChecked<UGameEngine>(GEngine);
			OutLevel = GameEngine->GetGameWorld()->GetCurrentLevel();
			OutSceneViewport = GameEngine->SceneViewport;
			return true;
		}
	}
}
