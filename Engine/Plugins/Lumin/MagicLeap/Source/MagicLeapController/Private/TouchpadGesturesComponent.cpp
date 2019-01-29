// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TouchpadGesturesComponent.h"
#include "IMagicLeapControllerPlugin.h"
#include "MagicLeapController.h"

UTouchpadGesturesComponent::UTouchpadGesturesComponent()
{
	// Make sure this component ticks
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bAutoActivate = true;

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
		if (controller.IsValid())
		{
			controller->RegisterTouchpadGestureReceiver(this);
		}
	}
}

UTouchpadGesturesComponent::~UTouchpadGesturesComponent()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
		if (controller.IsValid())
		{
			controller->UnregisterTouchpadGestureReceiver(this);
		}
	}
}

void UTouchpadGesturesComponent::OnTouchpadGestureStartCallback(const FMagicLeapTouchpadGesture& GestureData)
{
	FScopeLock Lock(&CriticalSection);
	PendingTouchpadGestureStart.Add(GestureData);
}

void UTouchpadGesturesComponent::OnTouchpadGestureContinueCallback(const FMagicLeapTouchpadGesture& GestureData)
{
	FScopeLock Lock(&CriticalSection);
	PendingTouchpadGestureContinue.Add(GestureData);
}

void UTouchpadGesturesComponent::OnTouchpadGestureEndCallback(const FMagicLeapTouchpadGesture& GestureData)
{
	FScopeLock Lock(&CriticalSection);
	PendingTouchpadGestureEnd.Add(GestureData);
}

void UTouchpadGesturesComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	{
		FScopeLock Lock(&CriticalSection);
		for (const FMagicLeapTouchpadGesture& touchpad : PendingTouchpadGestureStart)
		{
			OnTouchpadGestureStart.Broadcast(touchpad);
		}
		for (const FMagicLeapTouchpadGesture& touchpad : PendingTouchpadGestureContinue)
		{
			OnTouchpadGestureContinue.Broadcast(touchpad);
		}
		for (const FMagicLeapTouchpadGesture& touchpad : PendingTouchpadGestureEnd)
		{
			OnTouchpadGestureEnd.Broadcast(touchpad);
		}

		PendingTouchpadGestureStart.Empty();
		PendingTouchpadGestureContinue.Empty();
		PendingTouchpadGestureEnd.Empty();
	}

}


