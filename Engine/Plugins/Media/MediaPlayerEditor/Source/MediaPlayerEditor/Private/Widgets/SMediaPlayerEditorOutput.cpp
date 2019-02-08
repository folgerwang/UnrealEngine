// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SMediaPlayerEditorOutput.h"

#include "AudioDevice.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "IMediaEventSink.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "MediaTexture.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SMediaImage.h"


/* SMediaPlayerEditorOutput structors
 *****************************************************************************/

SMediaPlayerEditorOutput::SMediaPlayerEditorOutput()
	: MediaPlayer(nullptr)
	, MediaTexture(nullptr)
	, SoundComponent(nullptr)
{ }


SMediaPlayerEditorOutput::~SMediaPlayerEditorOutput()
{
	if (MediaPlayer.IsValid())
	{
		MediaPlayer->OnMediaEvent().RemoveAll(this);
		MediaPlayer.Reset();
	}

	if (MediaTexture != nullptr)
	{
		MediaTexture->RemoveFromRoot();
		MediaTexture = nullptr;
	}

	if (SoundComponent != nullptr)
	{
		SoundComponent->Stop();
		SoundComponent->RemoveFromRoot();
		SoundComponent = nullptr;
	}
}


/* SMediaPlayerEditorOutput interface
 *****************************************************************************/

void SMediaPlayerEditorOutput::Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer)
{
	MediaPlayer = &InMediaPlayer;

	// create media sound component
	if ((GEngine != nullptr) && GEngine->UseSound())
	{
		SoundComponent = NewObject<UMediaSoundComponent>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public);

		if (SoundComponent != nullptr)
		{
			SoundComponent->bIsUISound = true;
			SoundComponent->bIsPreviewSound = true;
			SoundComponent->SetMediaPlayer(&InMediaPlayer);
			SoundComponent->Initialize();
			SoundComponent->AddToRoot();
		}
	}

	// create media texture
	MediaTexture = NewObject<UMediaTexture>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public);

	if (MediaTexture != nullptr)
	{
		MediaTexture->AutoClear = true;
		MediaTexture->SetMediaPlayer(&InMediaPlayer);
		MediaTexture->UpdateResource();
		MediaTexture->AddToRoot();
	}

	TSharedRef<SMediaImage> MediaImage = SNew(SMediaImage, MediaTexture)
		.BrushImageSize_Lambda([&]()
			{
				if (MediaTexture)
					return FVector2D(MediaTexture->GetSurfaceWidth(), MediaTexture->GetSurfaceHeight());
				else
					return FVector2D::ZeroVector;
			});

	ChildSlot
	[
		MediaImage
	];

	MediaPlayer->OnMediaEvent().AddRaw(this, &SMediaPlayerEditorOutput::HandleMediaPlayerMediaEvent);
}


/* SWidget interface
 *****************************************************************************/

void SMediaPlayerEditorOutput::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (SoundComponent != nullptr)
	{
		SoundComponent->UpdatePlayer();
	}
}


/* SMediaPlayerEditorOutput callbacks
 *****************************************************************************/

void SMediaPlayerEditorOutput::HandleMediaPlayerMediaEvent(EMediaEvent Event)
{
	if (SoundComponent == nullptr)
	{
		return;
	}

	if (Event == EMediaEvent::PlaybackSuspended)
	{
		SoundComponent->Stop();
	}
	else if (Event == EMediaEvent::PlaybackResumed)
	{
		if (GEditor->PlayWorld == nullptr)
		{
			SoundComponent->Start();
		}
	}
}
