// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "EngineCustomTimeStep.generated.h"

/**
 * A CustomTimeStep control the Engine Framerate/Timestep.
 * This will update the FApp::CurrentTime/FApp::DeltaTime.
 * This is useful when you want the engine to be synchronized with an external clock (genlock).
 */
UCLASS(abstract)
class ENGINE_API UEngineCustomTimeStep : public UObject
{
	GENERATED_BODY()

public:
	/** This CustomTimeStep became the Engine's CustomTimeStep. */
	virtual bool Initialize(class UEngine* InEngine) PURE_VIRTUAL(UEngineCustomTimeStep::Initialize, return false;);

	/** This CustomTimeStep stop being the Engine's CustomTimeStep. */
	virtual void Shutdown(class UEngine* InEngine) PURE_VIRTUAL(UEngineCustomTimeStep::Shutdown, );

	/**
	 * Update FApp::CurrentTime/FApp::DeltaTime and optionnaly wait until the end of the frame.
	 * @return	true if the Engine's TimeStep should also be performed; false otherwise.
	 */
	virtual bool UpdateTimeStep(class UEngine* InEngine) PURE_VIRTUAL(UEngineCustomTimeStep::UpdateTimeStep, return true;);

public:
	/** Default behaviour of the engine. Update FApp::LastTime */
	static void UpdateApplicationLastTime();
};

