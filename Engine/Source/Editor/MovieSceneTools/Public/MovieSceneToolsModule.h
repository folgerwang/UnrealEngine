// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "IMovieSceneTools.h"

class UMovieSceneSection;

class IMovieSceneToolsTakeData
{
public:
	virtual bool GatherTakes(const UMovieSceneSection* Section, TArray<uint32>& TakeNumbers, uint32& CurrentTakeNumber) = 0;
	virtual UObject* GetTake(const UMovieSceneSection* Section, uint32 TakeNumber) = 0;
};

/**
* Implements the MovieSceneTools module.
*/
class MOVIESCENETOOLS_API FMovieSceneToolsModule
	: public IMovieSceneTools
{
public:

	static inline FMovieSceneToolsModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FMovieSceneToolsModule >("MovieSceneTools");
	}

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void RegisterTakeData(IMovieSceneToolsTakeData*);
	void UnregisterTakeData(IMovieSceneToolsTakeData*);

	bool GatherTakes(const UMovieSceneSection* Section, TArray<uint32>& TakeNumbers, uint32& CurrentTakeNumber);
	UObject* GetTake(const UMovieSceneSection* Section, uint32 TakeNumber);
		
private:

	void RegisterClipboardConversions();

private:

	/** Registered delegate handles */
	FDelegateHandle BoolPropertyTrackCreateEditorHandle;
	FDelegateHandle BytePropertyTrackCreateEditorHandle;
	FDelegateHandle ColorPropertyTrackCreateEditorHandle;
	FDelegateHandle FloatPropertyTrackCreateEditorHandle;
	FDelegateHandle IntegerPropertyTrackCreateEditorHandle;
	FDelegateHandle VectorPropertyTrackCreateEditorHandle;
	FDelegateHandle TransformPropertyTrackCreateEditorHandle;
	FDelegateHandle EulerTransformPropertyTrackCreateEditorHandle;
	FDelegateHandle VisibilityPropertyTrackCreateEditorHandle;
	FDelegateHandle ActorReferencePropertyTrackCreateEditorHandle;
	FDelegateHandle StringPropertyTrackCreateEditorHandle;
	FDelegateHandle ObjectTrackCreateEditorHandle;

	FDelegateHandle AnimationTrackCreateEditorHandle;
	FDelegateHandle AttachTrackCreateEditorHandle;
	FDelegateHandle AudioTrackCreateEditorHandle;
	FDelegateHandle EventTrackCreateEditorHandle;
	FDelegateHandle ParticleTrackCreateEditorHandle;
	FDelegateHandle ParticleParameterTrackCreateEditorHandle;
	FDelegateHandle PathTrackCreateEditorHandle;
	FDelegateHandle CameraCutTrackCreateEditorHandle;
	FDelegateHandle CinematicShotTrackCreateEditorHandle;
	FDelegateHandle SlomoTrackCreateEditorHandle;
	FDelegateHandle SubTrackCreateEditorHandle;
	FDelegateHandle TransformTrackCreateEditorHandle;
	FDelegateHandle ComponentMaterialTrackCreateEditorHandle;
	FDelegateHandle FadeTrackCreateEditorHandle;
	FDelegateHandle SpawnTrackCreateEditorHandle;
	FDelegateHandle LevelVisibilityTrackCreateEditorHandle;
	FDelegateHandle CameraAnimTrackCreateEditorHandle;
	FDelegateHandle CameraShakeTrackCreateEditorHandle;
	FDelegateHandle MPCTrackCreateEditorHandle;
	FDelegateHandle PrimitiveMaterialCreateEditorHandle;

	TArray<IMovieSceneToolsTakeData*> TakeDatas;
};