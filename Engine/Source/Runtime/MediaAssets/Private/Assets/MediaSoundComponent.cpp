// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaSoundComponent.h"
#include "MediaAssetsPrivate.h"

#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "IMediaAudioSample.h"
#include "IMediaPlayer.h"
#include "MediaAudioResampler.h"
#include "Misc/ScopeLock.h"
#include "Sound/AudioSettings.h"
#include "UObject/UObjectGlobals.h"

#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"

#if PLATFORM_HTML5
	#include "AudioDevice.h"
	#include "Engine/Engine.h"
#endif


#define MEDIASOUNDCOMPONENT_TRACE_RATEADJUSTMENT 0


/* Static initialization
 *****************************************************************************/

USoundClass* UMediaSoundComponent::DefaultMediaSoundClassObject = nullptr;


/* UMediaSoundComponent structors
 *****************************************************************************/

UMediaSoundComponent::UMediaSoundComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Channels(EMediaSoundChannels::Stereo)
	, DynamicRateAdjustment(false)
	, RateAdjustmentFactor(0.00000001f)
	, RateAdjustmentRange(FFloatRange(0.995f, 1.005f))
	, CachedRate(0.0f)
	, CachedTime(FTimespan::Zero())
	, RateAdjustment(1.0f)
	, Resampler(new FMediaAudioResampler)
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;

#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif

}


UMediaSoundComponent::~UMediaSoundComponent()
{
	delete Resampler;
}


/* UMediaSoundComponent interface
 *****************************************************************************/

bool UMediaSoundComponent::BP_GetAttenuationSettingsToApply(FSoundAttenuationSettings& OutAttenuationSettings)
{
	const FSoundAttenuationSettings* SelectedAttenuationSettings = GetSelectedAttenuationSettings();

	if (SelectedAttenuationSettings == nullptr)
	{
		return false;
	}

	OutAttenuationSettings = *SelectedAttenuationSettings;

	return true;
}


UMediaPlayer* UMediaSoundComponent::GetMediaPlayer() const
{
	return CurrentPlayer.Get();
}


void UMediaSoundComponent::SetMediaPlayer(UMediaPlayer* NewMediaPlayer)
{
	CurrentPlayer = NewMediaPlayer;
}

#if WITH_EDITOR

void UMediaSoundComponent::SetDefaultMediaPlayer(UMediaPlayer* NewMediaPlayer)
{
	MediaPlayer = NewMediaPlayer;
	CurrentPlayer = MediaPlayer;
}

#endif


void UMediaSoundComponent::UpdatePlayer()
{
	if (!CurrentPlayer.IsValid())
	{
		CachedRate = 0.0f;
		CachedTime = FTimespan::Zero();

		FScopeLock Lock(&CriticalSection);
		SampleQueue.Reset();

		return;
	}

	// create a new sample queue if the player changed
	TSharedRef<FMediaPlayerFacade, ESPMode::ThreadSafe> PlayerFacade = CurrentPlayer->GetPlayerFacade();

	if (PlayerFacade != CurrentPlayerFacade)
	{
		const auto NewSampleQueue = MakeShared<FMediaAudioSampleQueue, ESPMode::ThreadSafe>();
		PlayerFacade->AddAudioSampleSink(NewSampleQueue);
		{
			FScopeLock Lock(&CriticalSection);
			SampleQueue = NewSampleQueue;
		}

		CurrentPlayerFacade = PlayerFacade;
	}

	// caching play rate and time for audio thread (eventual consistency is sufficient)
	CachedRate = PlayerFacade->GetRate();
	CachedTime = PlayerFacade->GetTime();
}


/* TAttenuatedComponentVisualizer interface
 *****************************************************************************/

void UMediaSoundComponent::CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, FBaseAttenuationSettings::AttenuationShapeDetails>& ShapeDetailsMap) const
{
	const FSoundAttenuationSettings* SelectedAttenuationSettings = GetSelectedAttenuationSettings();

	if (SelectedAttenuationSettings != nullptr)
	{
		SelectedAttenuationSettings->CollectAttenuationShapesForVisualization(ShapeDetailsMap);
	}
}


/* UActorComponent interface
 *****************************************************************************/

void UMediaSoundComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITORONLY_DATA
	if (SpriteComponent != nullptr)
	{
		SpriteComponent->SpriteInfo.Category = TEXT("Sounds");
		SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT("SpriteCategory", "Sounds", "Sounds");

		if (bAutoActivate)
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorResources/AudioIcons/S_AudioComponent_AutoActivate.S_AudioComponent_AutoActivate")));
		}
		else
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorResources/AudioIcons/S_AudioComponent.S_AudioComponent")));
		}
	}
#endif
}


void UMediaSoundComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdatePlayer();
}


/* USceneComponent interface
 *****************************************************************************/

void UMediaSoundComponent::Activate(bool bReset)
{
	if (bReset || ShouldActivate())
	{
		SetComponentTickEnabled(true);
	}

	Super::Activate(bReset);
}


void UMediaSoundComponent::Deactivate()
{
	if (!ShouldActivate())
	{
		SetComponentTickEnabled(false);
	}

	Super::Deactivate();
}


/* UObject interface
 *****************************************************************************/

void UMediaSoundComponent::PostInitProperties()
{
	Super::PostInitProperties();

	if (UMediaSoundComponent::DefaultMediaSoundClassObject == nullptr)
	{
		const FSoftObjectPath DefaultMediaSoundClassName = GetDefault<UAudioSettings>()->DefaultMediaSoundClassName;

		if (DefaultMediaSoundClassName.IsValid())
		{
			UMediaSoundComponent::DefaultMediaSoundClassObject = LoadObject<USoundClass>(nullptr, *DefaultMediaSoundClassName.ToString());
		}
	}

	// We have a different default sound class object for media sound components
	if (SoundClass == USoundBase::DefaultSoundClassObject || SoundClass == nullptr)
	{
		SoundClass = UMediaSoundComponent::DefaultMediaSoundClassObject;
	}
}


void UMediaSoundComponent::PostLoad()
{
	Super::PostLoad();

	CurrentPlayer = MediaPlayer;
}


#if WITH_EDITOR

void UMediaSoundComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName MediaPlayerName = GET_MEMBER_NAME_CHECKED(UMediaSoundComponent, MediaPlayer);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	if (PropertyThatChanged != nullptr)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();

		if (PropertyName == MediaPlayerName)
		{
			CurrentPlayer = MediaPlayer;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif //WITH_EDITOR


/* USynthComponent interface
 *****************************************************************************/

bool UMediaSoundComponent::Init(int32& SampleRate)
{
	Super::Init(SampleRate);

	if (Channels == EMediaSoundChannels::Mono)
	{
		NumChannels = 1;
	}
	else //if (Channels == EMediaSoundChannels::Stereo)
	{
		NumChannels = 2;
	}/*
	else
	{
		NumChannels = 8;
	}*/

	// increase buffer callback size for media decoding. Media doesn't need fast response time so can decode more per callback.
	PreferredBufferLength = NumChannels * 8196;

	Resampler->Initialize(NumChannels, SampleRate);

	return true;
}


int32 UMediaSoundComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	TSharedPtr<FMediaAudioSampleQueue, ESPMode::ThreadSafe> PinnedSampleQueue;
	{
		FScopeLock Lock(&CriticalSection);
		PinnedSampleQueue = SampleQueue;
	}

	if (PinnedSampleQueue.IsValid() && (CachedRate != 0.0f))
	{
		const float Rate = CachedRate.Load();
		const FTimespan Time = CachedTime.Load();

		FTimespan OutTime = FTimespan::Zero();

		while (true)
		{
			const uint32 FramesRequested = NumSamples / NumChannels;
			const uint32 FramesWritten = Resampler->Generate(OutAudio, OutTime, FramesRequested, Rate * RateAdjustment, Time, *PinnedSampleQueue);

			if (FramesWritten == 0)
			{
				return 0; // no samples available
			}

			if (DynamicRateAdjustment)
			{
				RateAdjustment = 1.0f + (CachedTime.Load().GetTicks() - OutTime.GetTicks()) * RateAdjustmentFactor;
			}

			if (RateAdjustmentRange.IsEmpty() || RateAdjustmentRange.Contains(RateAdjustment))
			{
				break; // valid sample
			}

			// drop sample (clocks are too out of sync)
			RateAdjustment = 1.0f;

			#if MEDIASOUNDCOMPONENT_TRACE_RATEADJUSTMENT
				UE_LOG(LogMediaAssets, Verbose, TEXT("MediaSoundComponent %p: Sample dropped, Rate %f, Time %s, OutTime %s, Queue %i"),
					this,
					Rate,
					*Time.ToString(TEXT("%h:%m:%s.%t")),
					*OutTime.ToString(TEXT("%h:%m:%s.%t")),
					PinnedSampleQueue->Num()
				);
			#endif
		}

		#if MEDIASOUNDCOMPONENT_TRACE_RATEADJUSTMENT
			UE_LOG(LogMediaAssets, Verbose, TEXT("MediaSoundComponent %p: Sample rendered, Rate %f, Time %s, OutTime %s, RateAdjustment %f, Queue %i"),
				this,
				Rate,
				*Time.ToString(TEXT("%h:%m:%s.%t")),
				*OutTime.ToString(TEXT("%h:%m:%s.%t")),
				RateAdjustment,
				PinnedSampleQueue->Num()
			);
		#endif
	}
	else
	{
		Resampler->Flush();
	}
	return NumSamples;
}


/* UMediaSoundComponent implementation
 *****************************************************************************/

const FSoundAttenuationSettings* UMediaSoundComponent::GetSelectedAttenuationSettings() const
{
	if (bOverrideAttenuation)
	{
		return &AttenuationOverrides;
	}
	
	if (AttenuationSettings != nullptr)
	{
		return &AttenuationSettings->Attenuation;
	}

	return nullptr;
}
