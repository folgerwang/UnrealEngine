// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaBundleActorBase.h"

#include "Components/PrimitiveComponent.h"
#include "MediaBundle.h"
#include "MediaSoundComponent.h"

void AMediaBundleActorBase::SetComponent(UPrimitiveComponent* InPrimitive, UMediaSoundComponent* InMediaSound)
{
	if (InPrimitive != PrimitiveCmp)
	{
		if (MediaBundle && PrimitiveCmp && PrimitiveCmp->GetMaterial(PrimitiveMaterialIndex) == MediaBundle->GetMaterial())
		{
			PrimitiveCmp->SetMaterial(PrimitiveMaterialIndex, nullptr);
		}
		PrimitiveCmp = InPrimitive;
		if (MediaBundle && PrimitiveCmp)
		{
			PrimitiveCmp->SetMaterial(PrimitiveMaterialIndex, MediaBundle->GetMaterial());
		}
	}

	if (InMediaSound != MediaSoundCmp)
	{
		if (MediaBundle && MediaSoundCmp && MediaSoundCmp->GetMediaPlayer() == MediaBundle->GetMediaPlayer())
		{
			MediaSoundCmp->SetMediaPlayer(nullptr);
		}
		MediaSoundCmp = InMediaSound;
		if (MediaBundle && MediaSoundCmp)
		{
			MediaSoundCmp->SetMediaPlayer(MediaBundle->GetMediaPlayer());
		}
	}
}

bool AMediaBundleActorBase::RequestOpenMediaSource()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return false;
	}
	if (bPlayingMedia)
	{
		return true;
	}

	bPlayingMedia = MediaBundle ? MediaBundle->OpenMediaSource() : false;
	return bPlayingMedia;
}

void AMediaBundleActorBase::RequestCloseMediaSource()
{
	if (MediaBundle && bPlayingMedia)
	{
		MediaBundle->CloseMediaSource();
		bPlayingMedia = false;
	}
}

void AMediaBundleActorBase::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoPlay)
	{
		RequestOpenMediaSource();
	}
}

void AMediaBundleActorBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RequestCloseMediaSource();

	Super::EndPlay(EndPlayReason);
}

void AMediaBundleActorBase::PostActorCreated()
{
	Super::PostActorCreated();

	if (!HasAnyFlags(RF_Transient))
	{
		if (bAutoPlay && bPlayWhileEditing)
		{
			RequestOpenMediaSource();
		}
	}
}

void AMediaBundleActorBase::PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph)
{
	Super::PostLoadSubobjects(OuterInstanceGraph);

	if (MediaBundle && PrimitiveCmp)
	{
		PrimitiveCmp->SetMaterial(PrimitiveMaterialIndex, MediaBundle->GetMaterial());
	}

	if (MediaBundle && MediaSoundCmp)
	{
		MediaSoundCmp->SetMediaPlayer(MediaBundle->GetMediaPlayer());
	}

	if (bAutoPlay && bPlayWhileEditing)
	{
		RequestOpenMediaSource();
	}
}

void AMediaBundleActorBase::Destroyed()
{
	RequestCloseMediaSource();

	Super::Destroyed();
}

#if WITH_EDITOR
void AMediaBundleActorBase::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange)
	{
		bool bResetComponent = false;
		if (PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(AMediaBundleActorBase, MediaBundle)
			|| PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(AMediaBundleActorBase, bAutoPlay)
			|| PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(AMediaBundleActorBase, bPlayWhileEditing))
		{
			bResetComponent = PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(AMediaBundleActorBase, MediaBundle);
			RequestCloseMediaSource();
		}
		else if (PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(AMediaBundleActorBase, PrimitiveCmp)
			|| PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(AMediaBundleActorBase, MediaSoundCmp))
		{
			bResetComponent = true;
		}


		if (bResetComponent && MediaBundle)
		{
			if (PrimitiveCmp && PrimitiveCmp->GetMaterial(PrimitiveMaterialIndex) == MediaBundle->GetMaterial())
			{
				PrimitiveCmp->SetMaterial(PrimitiveMaterialIndex, nullptr);
			}
			else if (MediaSoundCmp && MediaSoundCmp->GetMediaPlayer() == MediaBundle->GetMediaPlayer())
			{
				MediaSoundCmp->SetMediaPlayer(nullptr);
			}
		}
	}
}

void AMediaBundleActorBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	bool bSetComponent = false;
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AMediaBundleActorBase, MediaBundle)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(AMediaBundleActorBase, bAutoPlay)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(AMediaBundleActorBase, bPlayWhileEditing))
	{
		if ((HasActorBegunPlay() && bAutoPlay) || (bPlayWhileEditing && bAutoPlay))
		{
			bPlayingMedia = RequestOpenMediaSource();
		}
		bSetComponent = PropertyName == GET_MEMBER_NAME_CHECKED(AMediaBundleActorBase, MediaBundle);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AMediaBundleActorBase, PrimitiveCmp)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(AMediaBundleActorBase, MediaSoundCmp))
	{
		bSetComponent = true;
	}

	if (bSetComponent && MediaBundle)
	{
		if (PrimitiveCmp)
		{
			PrimitiveCmp->SetMaterial(PrimitiveMaterialIndex, MediaBundle->GetMaterial());
		}
		if (MediaSoundCmp)
		{
			MediaSoundCmp->SetMediaPlayer(MediaBundle->GetMediaPlayer());
		}
	}
}
#endif //WITH_EDITOR
