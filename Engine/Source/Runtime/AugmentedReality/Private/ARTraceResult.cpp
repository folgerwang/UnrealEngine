// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ARTraceResult.h"
#include "ARSystem.h"


//
//
//
FARTraceResult::FARTraceResult()
: FARTraceResult(nullptr, 0.0f, EARLineTraceChannels::None, FTransform(), nullptr)
{
	
}


FARTraceResult::FARTraceResult( const TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe>& InARSystem, float InDistanceFromCamera, EARLineTraceChannels InTraceChannel, const FTransform& InLocalToTrackingTransform, UARTrackedGeometry* InTrackedGeometry )
: DistanceFromCamera(InDistanceFromCamera)
, TraceChannel(InTraceChannel)
, LocalToTrackingTransform(InLocalToTrackingTransform)
, TrackedGeometry(InTrackedGeometry)
, ARSystem(InARSystem)
{
	
}

float FARTraceResult::GetDistanceFromCamera() const
{
	return DistanceFromCamera;
}

void FARTraceResult::SetLocalToWorldTransform(const FTransform& LocalToWorldTransform)
{
	LocalToTrackingTransform = LocalToWorldTransform * ARSystem->GetXRTrackingSystem()->GetTrackingToWorldTransform().Inverse();
}

FTransform FARTraceResult::GetLocalToTrackingTransform() const
{
	return LocalToTrackingTransform;
}


FTransform FARTraceResult::GetLocalToWorldTransform() const
{
	return LocalToTrackingTransform * ARSystem->GetXRTrackingSystem()->GetTrackingToWorldTransform();
}


UARTrackedGeometry* FARTraceResult::GetTrackedGeometry() const
{
	return TrackedGeometry;
}

EARLineTraceChannels FARTraceResult::GetTraceChannel() const
{
	return TraceChannel;
}
