// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaViewportOutput.h"
#include "AjaMediaViewportOutputImpl.h"
#include "IAjaMediaOutputModule.h"

#include "Engine/GameEngine.h"
#include "Engine/RendererSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Timecode.h"
#include "Misc/FrameRate.h"

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
		Implementation->Tick(FApp::GetTimecode());
	}
}

void UAjaMediaViewportOutput::ActivateOutput(UAjaMediaOutput* MediaOutput)
{
	DeactivateOutput();

	if (MediaOutput == nullptr)
	{
		UE_LOG(LogAjaMediaOutput, Error, TEXT("Couldn't start the capture. No Media Output was provided."));
		return;
	}

	if (MediaOutput->OutputType == EAjaMediaOutputType::FillAndKey)
	{
		{
			static const auto CVarDefaultBackBufferPixelFormat = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultBackBufferPixelFormat"));
			if (EDefaultBackBufferPixelFormat::NumberOfBitForAlpha(EDefaultBackBufferPixelFormat::FromInt(CVarDefaultBackBufferPixelFormat->GetValueOnGameThread())) >= 8)
			{
				UE_LOG(LogAjaMediaOutput, Error, TEXT("Can't output Key. The 'Frame Buffer Pixel Format' must be set to at least 8 bits of alpha."));
				return;
			}
		}

		{
			static const auto CVarPropagateAlpha = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessing.PropagateAlpha"));
			EAlphaChannelMode::Type PropagateAlpha = EAlphaChannelMode::FromInt(CVarPropagateAlpha->GetValueOnGameThread());
			if (PropagateAlpha != EAlphaChannelMode::AllowThroughTonemapper)
			{

				UE_LOG(LogAjaMediaOutput, Error, TEXT("Can't output Key. 'Enable alpha channel support in post-processing' must be set to 'Allow through tonemapper'"));
				return;
			}
		}
	}

	TSharedPtr<FSceneViewport> FoundSceneViewport;
	ULevel* Level = nullptr;
	if (AjaMediaOutputDevice::FindSceneViewportAndLevel(FoundSceneViewport, Level) && FoundSceneViewport.IsValid())
	{
		Implementation = FAjaMediaViewportOutputImpl::CreateShared(MediaOutput, FoundSceneViewport);
		if (!Implementation.IsValid())
		{
			UE_LOG(LogAjaMediaOutput, Error, TEXT("Could not initialize the Output interface."));
			DeactivateOutput();
		}
	}
	else
	{
		UE_LOG(LogAjaMediaOutput, Warning, TEXT("No viewport could be found. Play in 'Standalone' or in 'New Editor Window PIE'."));
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
