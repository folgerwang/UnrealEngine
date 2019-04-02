// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneCameraCutSection.h"

#include "MovieScene.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneTransformTrack.h"
#include "MovieScene.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Evaluation/MovieSceneCameraCutTemplate.h"
#include "Compilation/MovieSceneTemplateInterrogation.h"
#include "IMovieScenePlayer.h"
#include "Camera/CameraComponent.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"

/* UMovieSceneCameraCutSection interface
 *****************************************************************************/

FMovieSceneEvalTemplatePtr UMovieSceneCameraCutSection::GenerateTemplate() const
{
	TOptional<FTransform> CutTransform;

	UMovieScene* MovieScene = GetTypedOuter<UMovieScene>();
	check(MovieScene);

	for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
	{
		if (Binding.GetObjectGuid() == CameraBindingID.GetGuid())
		{
			for (UMovieSceneTrack* Track : Binding.GetTracks())
			{
				UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(Track);
				if (TransformTrack)
				{
					// Extract the transform
					FMovieSceneEvaluationTrack TransformTrackTemplate = TransformTrack->GenerateTrackTemplate();
					FMovieSceneContext Context = FMovieSceneEvaluationRange(GetInclusiveStartFrame(), MovieScene->GetTickResolution());

					FMovieSceneInterrogationData Container;
					TransformTrackTemplate.Interrogate(Context, Container);

					for (auto It = Container.Iterate<FTransform>(UMovieScene3DTransformSection::GetInterrogationKey()); It; ++It)
					{
						CutTransform = *It;
						break;
					}
				}
			}
		}
	}

	return FMovieSceneCameraCutSectionTemplate(*this, CutTransform);
}

void UMovieSceneCameraCutSection::OnBindingsUpdated(const TMap<FGuid, FGuid>& OldGuidToNewGuidMap)
{
	if (OldGuidToNewGuidMap.Contains(CameraBindingID.GetGuid()))
	{
		CameraBindingID.SetGuid(OldGuidToNewGuidMap[CameraBindingID.GetGuid()]);
	}
}

void UMovieSceneCameraCutSection::GetReferencedBindings(TArray<FGuid>& OutBindings)
{
	OutBindings.Add(CameraBindingID.GetGuid());
}

void UMovieSceneCameraCutSection::PostLoad()
{
	Super::PostLoad();

	if (CameraGuid_DEPRECATED.IsValid())
	{
		if (!CameraBindingID.IsValid())
		{
			CameraBindingID = FMovieSceneObjectBindingID(CameraGuid_DEPRECATED, MovieSceneSequenceID::Root, EMovieSceneObjectBindingSpace::Local);
		}
		CameraGuid_DEPRECATED.Invalidate();
	}
}


UCameraComponent* UMovieSceneCameraCutSection::GetFirstCamera(IMovieScenePlayer& Player, FMovieSceneSequenceID SequenceID) const
{
	if (CameraBindingID.GetSequenceID().IsValid())
	{
		// Ensure that this ID is resolvable from the root, based on the current local sequence ID
		FMovieSceneObjectBindingID RootBindingID = CameraBindingID.ResolveLocalToRoot(SequenceID, Player.GetEvaluationTemplate().GetHierarchy());
		SequenceID = RootBindingID.GetSequenceID();
	}

	for (TWeakObjectPtr<>& WeakObject : Player.FindBoundObjects(CameraBindingID.GetGuid(), SequenceID))
	{
		if (UObject* Object = WeakObject .Get())
		{
			UCameraComponent* Camera = MovieSceneHelpers::CameraComponentFromRuntimeObject(Object);
			if (Camera)
			{
				return Camera;
			}
		}
	}

	return nullptr;
}