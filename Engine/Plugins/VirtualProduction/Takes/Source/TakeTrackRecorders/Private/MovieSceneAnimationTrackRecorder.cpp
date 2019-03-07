// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TrackRecorders/MovieSceneAnimationTrackRecorder.h"
#include "TrackRecorders/MovieSceneAnimationTrackRecorderSettings.h"
#include "TakesUtils.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "AnimationRecorder.h"
#include "MovieScene.h"
#include "AssetRegistryModule.h"
#include "SequenceRecorderSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"

DEFINE_LOG_CATEGORY(AnimationSerialization);

bool FMovieSceneAnimationTrackRecorderFactory::CanRecordObject(UObject* InObjectToRecord) const
{
	if (InObjectToRecord->IsA<USkeletalMeshComponent>())
	{
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InObjectToRecord);
		if (SkeletalMeshComponent && SkeletalMeshComponent->SkeletalMesh)
		{
			return true;
		}
	}
	return false;
}

UMovieSceneTrackRecorder* FMovieSceneAnimationTrackRecorderFactory::CreateTrackRecorderForObject() const
{
	return NewObject<UMovieSceneAnimationTrackRecorder>();
}

void UMovieSceneAnimationTrackRecorder::CreateAnimationAssetAndSequence(const AActor* Actor, const FDirectoryPath& AnimationDirectory)
{
	SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;
	if (SkeletalMesh.IsValid())
	{
		ComponentTransform = SkeletalMeshComponent->GetComponentToWorld().GetRelativeTransform(Actor->GetTransform());
		FString AnimationAssetName = Actor->GetActorLabel();
		AnimSequence = TakesUtils::MakeNewAsset<UAnimSequence>(AnimationDirectory.Path, AnimationAssetName);
		if (AnimSequence.IsValid())
		{
			FAssetRegistryModule::AssetCreated(AnimSequence.Get());

			// Assign the skeleton we're recording to the newly created Animation Sequence.
			AnimSequence->SetSkeleton(SkeletalMeshComponent->SkeletalMesh->Skeleton);
		}
	}

}
// todo move to FTakeUtils?
static FGuid GetActorInSequence(AActor* InActor, UMovieScene* MovieScene)
{
	FString ActorTargetName = InActor->GetActorLabel();


	for (int32 SpawnableCount = 0; SpawnableCount < MovieScene->GetSpawnableCount(); ++SpawnableCount)
	{
		const FMovieSceneSpawnable& Spawnable = MovieScene->GetSpawnable(SpawnableCount);
		if (Spawnable.GetName() == ActorTargetName || Spawnable.Tags.Contains(*ActorTargetName))
		{
			return Spawnable.GetGuid();
		}
	}

	for (int32 PossessableCount = 0; PossessableCount < MovieScene->GetPossessableCount(); ++PossessableCount)
	{
		const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(PossessableCount);
		if (Possessable.GetName() == ActorTargetName || Possessable.Tags.Contains(*ActorTargetName))
		{
			return Possessable.GetGuid();
		}
	}
	return FGuid();
}

void UMovieSceneAnimationTrackRecorder::CreateTrackImpl()
{
	if (MovieScene.IsValid())
	{
		AActor* Actor = nullptr;
		SkeletalMeshComponent = CastChecked<USkeletalMeshComponent>(ObjectToRecord.Get());
		Actor = SkeletalMeshComponent->GetOwner();

		// Build an asset path to record our new animation asset to.
		FString PathToRecordTo = FPackageName::GetLongPackagePath(MovieScene->GetOutermost()->GetPathName());
		FString BaseName = MovieScene->GetName();
		
		FDirectoryPath AnimationDirectory;
		AnimationDirectory.Path = PathToRecordTo;

		UMovieSceneAnimationTrackRecorderSettings* AnimSettings = CastChecked<UMovieSceneAnimationTrackRecorderSettings>(Settings.Get());
		if (AnimSettings->AnimationSubDirectory.Len())
		{
			AnimationDirectory.Path /= AnimSettings->AnimationSubDirectory;
		}

		CreateAnimationAssetAndSequence(Actor, AnimationDirectory);

		if (AnimSequence.IsValid())
		{
			FFrameRate SampleRate = MovieScene->GetDisplayRate();

			FText Error;
			FString Name = SkeletalMeshComponent->GetName();
			FName SerializedType("Animation");
			FString FileName = FString::Printf(TEXT("%s_%s"), *(SerializedType.ToString()), *Name);
			float IntervalTime = SampleRate.AsDecimal() > 0.0f ? 1.0f / SampleRate.AsDecimal() : 1.0f / FAnimationRecordingSettings::DefaultSampleRate;
			FAnimationFileHeader Header(SerializedType, ObjectGuid, IntervalTime);

			USkeleton* AnimSkeleton = AnimSequence->GetSkeleton();
			// add all frames

			const USkinnedMeshComponent* const MasterPoseComponentInst = SkeletalMeshComponent->MasterPoseComponent.Get();
			const TArray<FTransform>* SpaceBases;
			if (MasterPoseComponentInst)
			{
				SpaceBases = &MasterPoseComponentInst->GetComponentSpaceTransforms();
			}
			else
			{
				SpaceBases = &SkeletalMeshComponent->GetComponentSpaceTransforms();
			}
			for (int32 BoneIndex = 0; BoneIndex < SpaceBases->Num(); ++BoneIndex)
			{
				// verify if this bone exists in skeleton
				const int32 BoneTreeIndex = AnimSkeleton->GetSkeletonBoneIndexFromMeshBoneIndex(SkeletalMeshComponent->MasterPoseComponent != nullptr ? SkeletalMeshComponent->MasterPoseComponent->SkeletalMesh : SkeletalMeshComponent->SkeletalMesh, BoneIndex);
				if (BoneTreeIndex != INDEX_NONE)
				{
					// add tracks for the bone existing
					FName BoneTreeName = AnimSkeleton->GetReferenceSkeleton().GetBoneName(BoneTreeIndex);
					Header.AddNewRawTrack(BoneTreeName);
				}
			}
			Header.ActorGuid = GetActorInSequence(Actor, MovieScene.Get());
			Header.StartTime = 0.f; // ToDo: This should be assigned after the recording actually starts.

			if (!AnimationSerializer.OpenForWrite(FileName, Header, Error))
			{
				//UE_LOG(LogFrameTransport, Error, TEXT("Cannot open frame debugger cache %s. Failed to create archive."), *InFilename);
				UE_LOG(AnimationSerialization, Warning, TEXT("Error Opening Animation Sequencer File: Object '%s' Error '%s'"), *(Name), *(Error.ToString()));
			}
			bAnimationRecorderCreated = false;

			UMovieSceneSkeletalAnimationTrack* AnimTrack = MovieScene->FindTrack<UMovieSceneSkeletalAnimationTrack>(ObjectGuid);
			if (!AnimTrack)
			{
				AnimTrack = MovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>(ObjectGuid);
			}
			else
			{
				AnimTrack->RemoveAllAnimationData();
			}

			if (AnimTrack)
			{
				AnimTrack->AddNewAnimation(FFrameNumber(0), AnimSequence.Get());
				MovieSceneSection = Cast<UMovieSceneSkeletalAnimationSection>(AnimTrack->GetAllSections()[0]);
				MovieSceneSection->Params.bForceCustomMode = true;
			}
		}
	}

}

void UMovieSceneAnimationTrackRecorder::StopRecordingImpl()
{
	AnimationSerializer.Close();

	if (SkeletalMeshComponent.IsValid())
	{
		// Legacy Animation Recorder allowed recording into an animation asset directly and not creating an movie section
		const bool bShowAnimationAssetCreatedToast = false;
		FAnimationRecorderManager::Get().StopRecordingAnimation(SkeletalMeshComponent.Get(), bShowAnimationAssetCreatedToast);
	}
}


void UMovieSceneAnimationTrackRecorder::FinalizeTrackImpl()
{ 
 	if(MovieSceneSection.IsValid() && AnimSequence.IsValid() && MovieSceneSection->HasStartFrame())
 	{
 		FFrameRate   TickResolution  = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
 		FFrameNumber SequenceLength  = (AnimSequence->GetPlayLength() * TickResolution).FloorToFrame();
 		
 		MovieSceneSection->SetEndFrame(TRangeBound<FFrameNumber>::Exclusive(MovieSceneSection->GetInclusiveStartFrame() + SequenceLength));
 	}

	FTrackRecorderSettings TrackRecorderSettings = OwningTakeRecorderSource->GetTrackRecorderSettings();
	if (TrackRecorderSettings.bSaveRecordedAssets)
	{
		TakesUtils::SaveAsset(GetAnimSequence());
	}
}

void UMovieSceneAnimationTrackRecorder::RecordSampleImpl(const FQualifiedFrameTime& CurrentTime)
{
	// The animation recorder does most of the work here
	//  Note we wait for first tick so that we can make sure all of the attach tracks are set up .
	if (!bAnimationRecorderCreated)
	{
		
		bAnimationRecorderCreated = true;
		AActor* Actor = nullptr;
		SkeletalMeshComponent = CastChecked<USkeletalMeshComponent>(ObjectToRecord.Get());
		Actor = SkeletalMeshComponent->GetOwner();
		USceneComponent* RootComponent = Actor->GetRootComponent();
		USceneComponent* AttachParent = RootComponent ? RootComponent->GetAttachParent() : nullptr;
		
		//In Sequence Recorder this would be done via checking if the component was dynamically created, due to changes in how the take recorder handles this, it no longer 
		//possible so it seems if it's native do root, otherwise, don't.  
		bool bRemoveRootAnimation = SkeletalMeshComponent->CreationMethod != EComponentCreationMethod::Native ? false : true;

		// Need to pass this up to the settings since it's used later to force root lock and transfer root from animation to transform
		UMovieSceneAnimationTrackRecorderSettings* AnimSettings = CastChecked<UMovieSceneAnimationTrackRecorderSettings>(Settings.Get());
		AnimSettings->bRemoveRootAnimation = bRemoveRootAnimation;

		//If not removing root we also don't record in world space ( not totally sure if it matters but matching up with Sequence Recorder)
		bool bRecordInWorldSpace = bRemoveRootAnimation == false ? false : true;
		
		if (bRecordInWorldSpace && AttachParent && OwningTakeRecorderSource)
		{
			// We capture world space transforms for actors if they're attached, but we're not recording the attachment parent
			bRecordInWorldSpace = !OwningTakeRecorderSource->IsOtherActorBeingRecorded(AttachParent->GetOwner());
		}

		FFrameRate SampleRate = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetDisplayRate();

		//Set this up here so we know that it's parent sources have also been added so we record in the correct space
		FAnimationRecordingSettings RecordingSettings;
		RecordingSettings.SampleRate = SampleRate.AsDecimal();
		RecordingSettings.InterpMode = AnimSettings->InterpMode;
		RecordingSettings.TangentMode = AnimSettings->TangentMode;
		RecordingSettings.Length = 0;
		RecordingSettings.bRecordInWorldSpace = bRecordInWorldSpace;
		RecordingSettings.bRemoveRootAnimation = bRemoveRootAnimation;
		FAnimationRecorderManager::Get().RecordAnimation(SkeletalMeshComponent.Get(), AnimSequence.Get(), &AnimationSerializer, RecordingSettings);
	}
	if (SkeletalMeshComponent.IsValid())
	{
		// re-force updates on as gameplay can sometimes turn these back off!
		SkeletalMeshComponent->bEnableUpdateRateOptimizations = false;
		SkeletalMeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
}

void UMovieSceneAnimationTrackRecorder::RemoveRootMotion()
{
	 if(AnimSequence.IsValid())
	 {
		 UMovieSceneAnimationTrackRecorderSettings* AnimSettings = CastChecked<UMovieSceneAnimationTrackRecorderSettings>(Settings.Get());
		 if (AnimSettings->bRemoveRootAnimation) 
		 {
			 // Remove Root Motion by forcing the root lock on for now (which prevents the motion at evaluation time)
			 // In addition to set it to root lock we need to make sure it's to be zero'd since 
			 //	in all cases we expect the transform track to store either the absolute or relative transform for that skelmesh.

			 AnimSequence->bForceRootLock = true;
			 AnimSequence->RootMotionRootLock = ERootMotionRootLock::Zero;
		 }
	 }
}

bool UMovieSceneAnimationTrackRecorder::LoadRecordedFile(const FString& FileName, UMovieScene *InMovieScene, TMap<FGuid, AActor*>& ActorGuidToActorMap,  TFunction<void()> InCompletionCallback)
{
	
	bool bFileExists = AnimationSerializer.DoesFileExist(FileName);
	if (bFileExists)
	{
		FText Error;
		FAnimationFileHeader Header;

		if (AnimationSerializer.OpenForRead(FileName, Header, Error))
		{
			AnimationSerializer.GetDataRanges([this, InMovieScene, FileName, Header, ActorGuidToActorMap, InCompletionCallback](uint64 InMinFrameId, uint64 InMaxFrameId)
			{
				auto OnReadComplete = [this, InMovieScene, FileName, Header, ActorGuidToActorMap, InCompletionCallback]()
				{
					TArray<FAnimationSerializedFrame> &InFrames = AnimationSerializer.ResultData;
					if (InFrames.Num() > 0)
					{

						UMovieSceneSkeletalAnimationTrack* AnimTrack = InMovieScene->FindTrack<UMovieSceneSkeletalAnimationTrack>(Header.Guid);
						if (!AnimTrack)
						{
							AnimTrack = InMovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>(Header.Guid);
						}
						else
						{
							AnimTrack->RemoveAllAnimationData();
						}
						if (AnimTrack)
						{
							AActor*const*  Actors = ActorGuidToActorMap.Find(Header.ActorGuid);
							if (Actors &&  Actors[0]->FindComponentByClass<USkeletalMeshComponent>())
							{
								const AActor* Actor = Actors[0];
								ObjectToRecord = Actor->FindComponentByClass<USkeletalMeshComponent>();
								MovieScene = InMovieScene;
								SkeletalMeshComponent = CastChecked<USkeletalMeshComponent>(ObjectToRecord.Get());

								FString PathToRecordTo = FPackageName::GetLongPackagePath(MovieScene->GetOutermost()->GetPathName());
								FString BaseName = MovieScene->GetName();
								FDirectoryPath AnimationDirectory;
								AnimationDirectory.Path = PathToRecordTo;

								CreateAnimationAssetAndSequence(Actor,AnimationDirectory);

								AnimSequence->RecycleAnimSequence();
								AnimSequence->SequenceLength = 0.f;
								AnimSequence->SetRawNumberOfFrame(0);
								for (int32 TrackIndex = 0; TrackIndex < Header.AnimationTrackNames.Num(); ++TrackIndex)
								{
									AnimSequence->AddNewRawTrack(Header.AnimationTrackNames[TrackIndex]);

								}
								AnimSequence->InitializeNotifyTrack();

								for (const FAnimationSerializedFrame& SerializedFrame : InFrames)
								{
									const FSerializedAnimation &Frame = SerializedFrame.Frame;
									for (int32 TrackIndex = 0; TrackIndex < Frame.AnimationData.Num(); ++TrackIndex)
									{
										FRawAnimSequenceTrack& RawTrack = AnimSequence->GetRawAnimationTrack(TrackIndex);
										RawTrack.PosKeys.Add(Frame.AnimationData[TrackIndex].PosKey);
										RawTrack.RotKeys.Add(Frame.AnimationData[TrackIndex].RotKey);
										RawTrack.ScaleKeys.Add(Frame.AnimationData[TrackIndex].ScaleKey);
									}
								}
								AnimSequence->SetRawNumberOfFrame( InFrames.Num() );
								AnimSequence->SequenceLength = (AnimSequence->GetRawNumberOfFrames() > 1) ? (AnimSequence->GetRawNumberOfFrames() - 1) * Header.IntervalTime : MINIMUM_ANIMATION_LENGTH;

								//fix up notifies todo notifies mz
								AnimSequence->PostProcessSequence();
								AnimSequence->MarkPackageDirty();
								FFrameRate TickResolution = InMovieScene->GetTickResolution();;

								// save the package to disk, for convenience and so we can run this in standalone mod
								UPackage* const Package = AnimSequence->GetOutermost();
								FString const PackageName = Package->GetName();
								FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
								UPackage::SavePackage(Package, NULL, RF_Standalone, *PackageFileName, GError, nullptr, false, true, SAVE_NoError);


								FFrameNumber SequenceLength = (AnimSequence->GetPlayLength() * TickResolution).FloorToFrame();
								FFrameNumber StartFrame = (Header.StartTime * TickResolution).FloorToFrame();
								AnimTrack->AddNewAnimation(StartFrame, AnimSequence.Get());
								MovieSceneSection = Cast<UMovieSceneSkeletalAnimationSection>(AnimTrack->GetAllSections()[0]);
								MovieSceneSection->SetEndFrame(TRangeBound<FFrameNumber>::Exclusive(MovieSceneSection->GetInclusiveStartFrame() + SequenceLength));

							}

						}
					}
					AnimationSerializer.Close();
					InCompletionCallback();
				}; //callback

				AnimationSerializer.ReadFramesAtFrameRange(InMinFrameId, InMaxFrameId, OnReadComplete);

			});
			return true;
		}
		else
		{
			AnimationSerializer.Close();
		}
	}
	
	return false;
}