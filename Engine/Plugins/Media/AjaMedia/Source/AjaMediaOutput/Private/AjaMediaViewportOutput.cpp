// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaViewportOutput.h"
#include "AjaMediaViewportOutputImpl.h"
#include "IAjaMediaOutputModule.h"

#include "Engine/GameEngine.h"
#include "ITimecodeProvider.h"
#include "ITimeManagementModule.h"
#include "Timecode.h"
#include "Framerate.h"

#include "Widgets/SViewport.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif //WITH_EDITOR

/* namespace AjaMediaOutputDevice definition
*****************************************************************************/

namespace AjaMediaOutputDevice
{
	bool FindSceneViewportAndLevel(TSharedPtr<FSceneViewport>& OutSceneViewport, ULevel*& OutLevel);
}

/* UAjaMediaViewportOutput
*****************************************************************************/

UAjaMediaViewportOutput::UAjaMediaViewportOutput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Implementation(nullptr)
{
}

void UAjaMediaViewportOutput::BeginDestroy()
{
	DeactivateOutput();
	Super::BeginDestroy();
}

ETickableTickType UAjaMediaViewportOutput::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UAjaMediaViewportOutput::IsTickable() const
{
	return Implementation.IsValid();
}

void UAjaMediaViewportOutput::Tick(float DeltaTime)
{
	if (Implementation.IsValid())
	{
		//By default, we send the engine frame number directly to the output. A sequential Timecode will be generated from the output's FrameRate
		int32 FrameNumber = (int32)GFrameNumber;

		if (const ITimecodeProvider* Provider = ITimeManagementModule::Get().GetTimecodeProvider())
		{
			if (Provider->IsSynchronized())
			{
				FTimecode Timecode = Provider->GetCurrentTimecode();
				const FFrameRate TimeManagerFrameRate = Provider->GetFrameRate();
				const FFrameTime CurrentFrameTimeManagerTime = Timecode.ToFrameNumber(TimeManagerFrameRate);
				const FFrameTime CurrentFrameOutputTime = FFrameRate::TransformTime(CurrentFrameTimeManagerTime, TimeManagerFrameRate, Implementation->GetOutputFrameRate());
				FrameNumber = CurrentFrameOutputTime.GetFrame().Value;
			}
		}

		Implementation->Tick(FrameNumber);
	}
}

void UAjaMediaViewportOutput::ActivateOutput(UAjaMediaOutput* MediaOutput)
{
	DeactivateOutput();

	if (MediaOutput != nullptr)
	{
		TSharedPtr<FSceneViewport> FoundSceneViewport;
		ULevel* Level = nullptr;
		if (AjaMediaOutputDevice::FindSceneViewportAndLevel(FoundSceneViewport, Level) || !FoundSceneViewport.IsValid())
		{
			Implementation = FAjaMediaViewportOutputImpl::CreateShared(MediaOutput, FoundSceneViewport);
			if (!Implementation.IsValid())
			{
				UE_LOG(LogAjaMediaOutput, Error, TEXT("Could not initialized the Output interface."));
				DeactivateOutput();
			}
		}
		else
		{
			UE_LOG(LogAjaMediaOutput, Warning, TEXT("No viewport could be found. Play in 'Standalone' or in 'New Editor Window PIE'."));
		}
	}
	else
	{
		UE_LOG(LogAjaMediaOutput, Error, TEXT("Couldn't start the capture. No Media Output was provided."));
	}
}

void UAjaMediaViewportOutput::DeactivateOutput()
{
	if (Implementation.IsValid())
	{
		Implementation->Shutdown();
		Implementation.Reset();
	}
}

/* namespace AjaMediaOutputDevice implementation
*****************************************************************************/
namespace AjaMediaOutputDevice
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
