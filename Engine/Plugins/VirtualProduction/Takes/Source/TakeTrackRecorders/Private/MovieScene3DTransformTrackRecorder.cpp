// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TrackRecorders/MovieScene3DTransformTrackRecorder.h"
#include "TrackRecorders/MovieSceneAnimationTrackRecorder.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Misc/ScopedSlowTask.h"
#include "GameFramework/Character.h"
#include "KeyParams.h"
#include "Algo/Transform.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"

DEFINE_LOG_CATEGORY(TransformSerialization);

bool FMovieScene3DTransformTrackRecorderFactory::CanRecordObject(UObject* InObjectToRecord) const
{
	if(USceneComponent* SceneComponent = Cast<USceneComponent>(InObjectToRecord))
	{
		// Dont record the root component transforms as this will be taken into account by the actor transform track
		// Also dont record transforms of skeletal mesh components as they will be taken into account in the actor transform
		bool bIsCharacterSkelMesh = false;
		if (SceneComponent->IsA<USkeletalMeshComponent>() && SceneComponent->GetOwner()->IsA<ACharacter>())
		{
			ACharacter* Character = CastChecked<ACharacter>(SceneComponent->GetOwner());
			bIsCharacterSkelMesh = SceneComponent == Character->GetMesh();
		}
		return (SceneComponent != SceneComponent->GetOwner()->GetRootComponent() && !bIsCharacterSkelMesh);
	}
	else 
	{
		return InObjectToRecord->IsA<AActor>();
	}
}

UMovieSceneTrackRecorder* FMovieScene3DTransformTrackRecorderFactory::CreateTrackRecorderForObject() const
{
	return NewObject<UMovieScene3DTransformTrackRecorder>();
}

void UMovieScene3DTransformTrackRecorder::CreateTrackImpl()
{
	bWasAttached = false;
	bSetFirstKey = true;
	static const FName Transform("Transform");
	MovieSceneTrack = MovieScene->FindTrack<UMovieScene3DTransformTrack>(ObjectGuid, Transform);
	if (!MovieSceneTrack.IsValid())
	{
		MovieSceneTrack = MovieScene->AddTrack<UMovieScene3DTransformTrack>(ObjectGuid);
	}
	else
	{
		MovieSceneTrack->RemoveAllAnimationData();
	}

	if (MovieSceneTrack.IsValid())
	{
		MovieSceneSection = Cast<UMovieScene3DTransformSection>(MovieSceneTrack->CreateNewSection());
		
		// Disable the section after creation so that the track can't be evaluated by Sequencer while recording.
		MovieSceneSection->SetIsActive(false);

		MovieSceneTrack->AddSection(*MovieSceneSection);

		FFrameRate   TickResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();

		FText Error;
		FString Name = ObjectToRecord.IsValid() ? ObjectToRecord->GetName() : TEXT("Unnamed_Actor");
		FName SerializedType("Transform");
		FString FileName = FString::Printf(TEXT("%s_%s"), *(SerializedType.ToString()), *Name);

		FTransformFileHeader Header(TickResolution, SerializedType, ObjectGuid);
		if (!TransformSerializer.OpenForWrite(FileName, Header, Error))
		{
			//UE_LOG(LogFrameTransport, Error, TEXT("Cannot open frame debugger cache %s. Failed to create archive."), *InFilename);
			UE_LOG(TransformSerialization, Warning, TEXT("Error Opening Transform Sequencer File: Object '%s' Error '%s'"), *(Name), *(Error.ToString()));
		}
	}
}

bool UMovieScene3DTransformTrackRecorder::ShouldAddNewKey(const FTransform& TransformToRecord)
{
	return !FTransform::AreTranslationsEqual(TransformToRecord, PreviousValue) || !FTransform::AreRotationsEqual(TransformToRecord, PreviousValue) || !FTransform::AreScale3DsEqual(TransformToRecord, PreviousValue);
}


void UMovieScene3DTransformTrackRecorder::StopRecordingImpl()
{
	TransformSerializer.Close();
}

void UMovieScene3DTransformTrackRecorder::FinalizeTrackImpl()
{
 	if (!MovieSceneSection.IsValid())
 	{
 		return;
 	}
 
 	FScopedSlowTask SlowTask(4.0f, NSLOCTEXT("TakeRecorder", "ProcessingTransforms", "Processing Transforms"));
 
 	check (	BufferedTransforms.Times.Num() == BufferedTransforms.LocationX.Num());	
 	check (	BufferedTransforms.Times.Num() == BufferedTransforms.LocationY.Num());
 	check (	BufferedTransforms.Times.Num() == BufferedTransforms.LocationZ.Num());
 	check (	BufferedTransforms.Times.Num() == BufferedTransforms.RotationX.Num());
 	check (	BufferedTransforms.Times.Num() == BufferedTransforms.RotationY.Num());
 	check (	BufferedTransforms.Times.Num() == BufferedTransforms.RotationZ.Num());
 	check (	BufferedTransforms.Times.Num() == BufferedTransforms.ScaleX.Num());
 	check (	BufferedTransforms.Times.Num() == BufferedTransforms.ScaleY.Num());
 	check (	BufferedTransforms.Times.Num() == BufferedTransforms.ScaleZ.Num());
 
 	SlowTask.EnterProgressFrame();
 
 	// Try to 're-wind' rotations that look like axis flips
 	// We need to do this as a post-process because the recorder cant reliably access 'wound' rotations:
 	// - Net quantize may use quaternions.
 	// - Scene components cache transforms as quaternions.
 	// - Gameplay is free to clamp/fmod rotations as it sees fit.
 	int32 TransformCount = BufferedTransforms.Times.Num();
 	for(int32 TransformIndex = 0; TransformIndex < TransformCount - 1; TransformIndex++)
 	{
 		FMath::WindRelativeAnglesDegrees(BufferedTransforms.RotationZ[TransformIndex], BufferedTransforms.RotationZ[TransformIndex+1]);
 		FMath::WindRelativeAnglesDegrees(BufferedTransforms.RotationY[TransformIndex], BufferedTransforms.RotationY[TransformIndex+1]);
 		FMath::WindRelativeAnglesDegrees(BufferedTransforms.RotationX[TransformIndex], BufferedTransforms.RotationX[TransformIndex+1]);
 	}
 
 	SlowTask.EnterProgressFrame();
  
 	// add buffered transforms
 	TArrayView<FMovieSceneFloatChannel*> FloatChannels = MovieSceneSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
 
	ERichCurveInterpMode LocalInterpMode = InterpolationMode;
 	auto Transformation = [LocalInterpMode](float In)
 	{
 		FMovieSceneFloatValue NewValue(In);
 		NewValue.InterpMode = LocalInterpMode;
 		return NewValue;
 	};
 	TArray<FMovieSceneFloatValue> FloatValues;
 
 	FloatValues.Reset(BufferedTransforms.LocationX.Num());
 	Algo::Transform(BufferedTransforms.LocationX, FloatValues, Transformation);
 	FloatChannels[0]->Set(BufferedTransforms.Times, MoveTemp(FloatValues));
 
 	FloatValues.Reset(BufferedTransforms.LocationY.Num());
 	Algo::Transform(BufferedTransforms.LocationY, FloatValues, Transformation);
 	FloatChannels[1]->Set(BufferedTransforms.Times, MoveTemp(FloatValues));
 
 	FloatValues.Reset(BufferedTransforms.LocationZ.Num());
 	Algo::Transform(BufferedTransforms.LocationZ, FloatValues, Transformation);
 	FloatChannels[2]->Set(BufferedTransforms.Times, MoveTemp(FloatValues));
 
 	FloatValues.Reset(BufferedTransforms.RotationX.Num());
 	Algo::Transform(BufferedTransforms.RotationX, FloatValues, Transformation);
 	FloatChannels[3]->Set(BufferedTransforms.Times, MoveTemp(FloatValues));
 
 	FloatValues.Reset(BufferedTransforms.RotationY.Num());
 	Algo::Transform(BufferedTransforms.RotationY, FloatValues, Transformation);
 	FloatChannels[4]->Set(BufferedTransforms.Times, MoveTemp(FloatValues));
 
 	FloatValues.Reset(BufferedTransforms.RotationZ.Num());
 	Algo::Transform(BufferedTransforms.RotationZ, FloatValues, Transformation);
 	FloatChannels[5]->Set(BufferedTransforms.Times, MoveTemp(FloatValues));
 
 	FloatValues.Reset(BufferedTransforms.ScaleX.Num());
 	Algo::Transform(BufferedTransforms.ScaleX, FloatValues, Transformation);
 	FloatChannels[6]->Set(BufferedTransforms.Times, MoveTemp(FloatValues));
 
 	FloatValues.Reset(BufferedTransforms.ScaleY.Num());
 	Algo::Transform(BufferedTransforms.ScaleY, FloatValues, Transformation);
 	FloatChannels[7]->Set(BufferedTransforms.Times, MoveTemp(FloatValues));
 
 	FloatValues.Reset(BufferedTransforms.ScaleZ.Num());
 	Algo::Transform(BufferedTransforms.ScaleZ, FloatValues, Transformation);
 	FloatChannels[8]->Set(BufferedTransforms.Times, MoveTemp(FloatValues));
 
 	FTransform FirstTransform = FTransform::Identity;
	if (DefaultTransform.IsSet())
	{
		FirstTransform = DefaultTransform.GetValue();
	}
	else if (BufferedTransforms.Times.Num())
 	{
 		FirstTransform.SetTranslation(FVector(BufferedTransforms.LocationX[0], BufferedTransforms.LocationY[0], BufferedTransforms.LocationZ[0]));
 		FirstTransform.SetRotation(FQuat(FRotator(BufferedTransforms.RotationY[0], BufferedTransforms.RotationZ[0], BufferedTransforms.RotationX[0])));
 		FirstTransform.SetScale3D(FVector(BufferedTransforms.ScaleX[0], BufferedTransforms.ScaleY[0], BufferedTransforms.ScaleZ[0]));
 	}
 
 	BufferedTransforms = FBufferedTransformKeys();
 
 	SlowTask.EnterProgressFrame();
 
	FTrackRecorderSettings TrackRecorderSettings = OwningTakeRecorderSource->GetTrackRecorderSettings();

	if (TrackRecorderSettings.bReduceKeys)
	{
		FKeyDataOptimizationParams Params;
		Params.bAutoSetInterpolation = true;
		for (FMovieSceneFloatChannel* Channel : FloatChannels)
		{
			Channel->Optimize(Params);
		}
	}
	else
	{
		for (FMovieSceneFloatChannel* Channel : FloatChannels)
		{
			Channel->AutoSetTangents();
		}
	}

	if (TrackRecorderSettings.bRemoveRedundantTracks)
	{
 		// we cant remove redundant tracks if we were attached as the playback relies on update order of
 		// transform tracks. Without this track, relative transforms would accumulate.
 		if(!bWasAttached)
 		{
 			bool bCanReset = true;
			for (FMovieSceneFloatChannel* Channel : MovieSceneSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>())
			{
				if (Channel->GetNumKeys() > 1)
				{
					bCanReset = false;
					break;
				}
			}

			if (bCanReset)
			{
				for (FMovieSceneFloatChannel* Channel : MovieSceneSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>())
				{
					Channel->Reset();
				}

				FloatChannels[0]->SetDefault(FirstTransform.GetTranslation()[0]);
				FloatChannels[1]->SetDefault(FirstTransform.GetTranslation()[1]);
				FloatChannels[2]->SetDefault(FirstTransform.GetTranslation()[2]);
				FloatChannels[3]->SetDefault(FirstTransform.GetRotation().Euler().X);
				FloatChannels[4]->SetDefault(FirstTransform.GetRotation().Euler().Y);
				FloatChannels[5]->SetDefault(FirstTransform.GetRotation().Euler().Z);
				FloatChannels[6]->SetDefault(FirstTransform.GetScale3D()[0]);
				FloatChannels[7]->SetDefault(FirstTransform.GetScale3D()[1]);
				FloatChannels[8]->SetDefault(FirstTransform.GetScale3D()[2]);

				// The section can be removed if this is a spawnable since the spawnable template should have the same default values
				if (!TrackRecorderSettings.bRecordToPossessable && MovieSceneTrack.IsValid())
				{
					FString ObjectToRecordName = ObjectToRecord.IsValid() ? ObjectToRecord->GetName() : TEXT("Unnamed_Actor");
					UE_LOG(LogTakesCore, Log, TEXT("Removed unused track (%s) for (%s)"), *MovieSceneTrack->GetTrackName().ToString(), *ObjectToRecordName);

					MovieSceneTrack->RemoveSection(*MovieSceneSection);
					MovieScene->RemoveTrack(*MovieSceneTrack);
				}
			}
 		}
 	}
 
 	SlowTask.EnterProgressFrame();
 
 	// If recording a spawnable, update the spawnable object template to the first keyframe
 	if (MovieScene.IsValid() && ObjectGuid.IsValid())
 	{
 		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectGuid);
 		if (Spawnable)
 		{
 			Spawnable->SpawnTransform = FirstTransform;
 		}
 	}
}

void UMovieScene3DTransformTrackRecorder::RecordSampleImpl(const FQualifiedFrameTime& CurrentTime)
{
 	if (!MovieSceneTrack.IsValid())
 	{
 		return;
 	}
 
 	if(ObjectToRecord.IsValid() && MovieSceneSection.IsValid())
 	{
 		FFrameRate   TickResolution  = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
 		FFrameNumber CurrentFrame    = CurrentTime.ConvertTo(TickResolution).FloorToFrame();
		
		MovieSceneSection->ExpandToFrame(CurrentFrame);

 		if(USceneComponent* SceneComponent = Cast<USceneComponent>(ObjectToRecord.Get()))
 		{
 			// dont record non-registered scene components
 			if(!SceneComponent->IsRegistered())
 			{
 				return;
 			}
 		}
		if (!DefaultTransform.IsSet())
		{
			SetUpDefaultTransform();
		}

		FTransform TransformThisFrame = FTransform::Identity;
		if (ResolveTransformToRecord(TransformThisFrame))
		{
			if (ShouldAddNewKey(TransformThisFrame))
			{
				if (PreviousFrame.IsSet())
				{
					BufferedTransforms.Add(PreviousValue, PreviousFrame.GetValue());
					FSerializedTransform SerializedTransform(PreviousValue, PreviousFrame.GetValue());
					TransformSerializer.WriteFrameData(TransformSerializer.FramesWritten, SerializedTransform);
				}
				BufferedTransforms.Add(TransformThisFrame, CurrentFrame);
				FSerializedTransform SerializedTransform(TransformThisFrame, CurrentFrame);
				TransformSerializer.WriteFrameData(TransformSerializer.FramesWritten, SerializedTransform);

				PreviousValue = TransformThisFrame;
				PreviousFrame.Reset();
			}
			else
			{
				if (bSetFirstKey)
				{
					FSerializedTransform SerializedTransform(PreviousValue, CurrentFrame);
					TransformSerializer.WriteFrameData(TransformSerializer.FramesWritten, SerializedTransform);
					bSetFirstKey = false;
				}
				PreviousFrame = CurrentFrame;
			}
		}
 	}
}

void UMovieScene3DTransformTrackRecorder::SetUpDefaultTransform()
{
	FTransform DT = FTransform::Identity;
	ResolveTransformToRecord(DT);

	FVector Translation = DT.GetTranslation();
	FVector EulerRotation = DT.GetRotation().Rotator().Euler();
	FVector Scale = DT.GetScale3D();
	
	DefaultTransform = DT;
	PreviousValue = DT;
	TArrayView<FMovieSceneFloatChannel*> FloatChannels = MovieSceneSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
	FloatChannels[0]->SetDefault(Translation.X);
	FloatChannels[1]->SetDefault(Translation.Y);
	FloatChannels[2]->SetDefault(Translation.Z);
	FloatChannels[3]->SetDefault(EulerRotation.X);
	FloatChannels[4]->SetDefault(EulerRotation.Y);
	FloatChannels[5]->SetDefault(EulerRotation.Z);
	FloatChannels[6]->SetDefault(Scale.X);
	FloatChannels[7]->SetDefault(Scale.Y);
	FloatChannels[8]->SetDefault(Scale.Z);
}
bool UMovieScene3DTransformTrackRecorder::ResolveTransformToRecord(FTransform& OutTransform)
{
 	if(USceneComponent* SceneComponent = Cast<USceneComponent>(ObjectToRecord.Get()))
 	{
 		OutTransform = SceneComponent->GetRelativeTransform();

 		return true;
 	}
 	else if(AActor* Actor = Cast<AActor>(ObjectToRecord.Get()))
 	{
 		bool bCaptureWorldSpaceTransform = false; //if not attached world = relative so we set this false so 
 
 		USceneComponent* RootComponent = Actor->GetRootComponent();
 		USceneComponent* AttachParent = RootComponent ? RootComponent->GetAttachParent() : nullptr;
 
 		bWasAttached = AttachParent != nullptr;
 		if (AttachParent && OwningTakeRecorderSource)
 		{
 			// We capture world space transforms for actors if they're attached, but we're not recording the attachment parent
			bCaptureWorldSpaceTransform = !OwningTakeRecorderSource->IsOtherActorBeingRecorded(AttachParent->GetOwner());
 		}
 
 		if (!RootComponent)
 		{
 			return false;
 		}
 
 		if (bCaptureWorldSpaceTransform)
 		{
 			OutTransform = Actor->ActorToWorld();
 		}
 		else
 		{
			/*  Need to fix this for weapon mesh recording needs to be local to what it's attached.
			FName SocketName;
			FName ComponentName;
			if (bCaptureWorldSpaceTransform)
			{
				OutTransform = Actor->ActorToWorld();
				FTransform ParentTransform = AttachedToActor->ActorToWorld();
				OutTransform = ParentTransform.GetRelativeTransform(OutTransform);
			}
			else
				*/
			{
				OutTransform = RootComponent->GetRelativeTransform();
			}
 		}

 		return true;
 	}

	return false;
}
//  Move root from animation from animation sequence to transform track IF we are removing root animation (which doesn't happen with dynamic spawned skelmeshes)
void UMovieScene3DTransformTrackRecorder::PostProcessAnimationData(UMovieSceneAnimationTrackRecorder* AnimTrackRecorder)
{
	ensure(AnimTrackRecorder);

	if (!MovieSceneSection.IsValid() || !AnimTrackRecorder)
	{
		return;
	}
	UMovieSceneAnimationTrackRecorderSettings* AnimSettings = CastChecked<UMovieSceneAnimationTrackRecorderSettings>(AnimTrackRecorder->GetTrackRecorderSettings());
	if (AnimSettings->bRemoveRootAnimation)
	{
		// Override the interpolation mode to use linear interpolation to avoid foot sliding
		InterpolationMode = RCIM_Linear;
		//Get All Animation Keys
		FBufferedTransformKeys  AnimationKeys;

		USkeletalMeshComponent* SkeletalMeshComponent = AnimTrackRecorder->GetSkeletalMeshComponent();
		bool bLayerCurrentBuffered = false;
		if (SkeletalMeshComponent)
		{
			FTransform Relative = FTransform::Identity;
			if (AActor* Actor = Cast<AActor>(ObjectToRecord.Get()))
			{
				USceneComponent* RootComponent = Actor->GetRootComponent();
				USceneComponent* AttachParent = RootComponent ? RootComponent->GetAttachParent() : nullptr;
				bWasAttached = AttachParent != nullptr;
				if (AttachParent && OwningTakeRecorderSource && DefaultTransform.IsSet())
				{
					if (BufferedTransforms.Num() == 0)
					{
						Relative = DefaultTransform.GetValue();
					}
					else
					{
						bLayerCurrentBuffered = true;
					}
				}

			}
			// Search for the Root Bone in the Skeleton
			const USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->MasterPoseComponent != nullptr ? SkeletalMeshComponent->MasterPoseComponent->SkeletalMesh : SkeletalMeshComponent->SkeletalMesh;
			const UAnimSequence* AnimSequence = AnimTrackRecorder->GetAnimSequence();
			if (AnimSequence && SkeletalMesh && AnimSequence->GetSkeleton())
			{
				// Find the root bone
				int32 RootIndex = INDEX_NONE;
				USkeleton* AnimSkeleton = AnimSequence->GetSkeleton();
				for (int32 TrackIndex = 0; TrackIndex < AnimSequence->GetRawAnimationData().Num(); ++TrackIndex)
				{
					// Vverify that this bone exists in skeleton
					int32 BoneTreeIndex = AnimSequence->GetSkeletonIndexFromRawDataTrackIndex(TrackIndex);
					if (BoneTreeIndex != INDEX_NONE)
					{
						int32 BoneIndex = AnimSkeleton->GetMeshBoneIndexFromSkeletonBoneIndex(SkeletalMesh, BoneTreeIndex);
						int32 ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
						if (ParentIndex == INDEX_NONE)
						{
							// We've found the root (root bones do not have a valid parent)
							RootIndex = BoneIndex;
							break;
						}
					}
				}

				if (RootIndex == INDEX_NONE)
				{
					FString ObjectToRecordName = ObjectToRecord.IsValid() ? ObjectToRecord->GetName() : TEXT("Unnamed_Actor");
					UE_LOG(LogTakesCore, Log, TEXT("No Root Found for (%s)"), *ObjectToRecordName);
					return;
				}

				const FFrameRate TickResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
				const FFrameNumber StartTime = MovieSceneSection->GetInclusiveStartFrame();

				// we may need to offset the transform here if the animation was not recorded on the root component
				FTransform InvComponentTransform = AnimTrackRecorder->GetComponentTransform().Inverse();

				const FRawAnimSequenceTrack& RawTrack = AnimSequence->GetRawAnimationData()[RootIndex];
				const int32 KeyCount = FMath::Max(FMath::Max(RawTrack.PosKeys.Num(), RawTrack.RotKeys.Num()), RawTrack.ScaleKeys.Num());
				for (int32 KeyIndex = 0; KeyIndex < KeyCount; KeyIndex++)
				{
					FTransform Transform;
					if (RawTrack.PosKeys.IsValidIndex(KeyIndex))
					{
						Transform.SetTranslation(RawTrack.PosKeys[KeyIndex]);
					}
					else if (RawTrack.PosKeys.Num() > 0)
					{
						Transform.SetTranslation(RawTrack.PosKeys[0]);
					}

					if (RawTrack.RotKeys.IsValidIndex(KeyIndex))
					{
						Transform.SetRotation(RawTrack.RotKeys[KeyIndex]);
					}
					else if (RawTrack.RotKeys.Num() > 0)
					{
						Transform.SetRotation(RawTrack.RotKeys[0]);
					}

					if (RawTrack.ScaleKeys.IsValidIndex(KeyIndex))
					{
						Transform.SetScale3D(RawTrack.ScaleKeys[KeyIndex]);
					}
					else if (RawTrack.ScaleKeys.Num() > 0)
					{
						Transform.SetScale3D(RawTrack.ScaleKeys[0]);
					}

					FFrameNumber AnimationFrame = (AnimSequence->GetTimeAtFrame(KeyIndex) * TickResolution).FloorToFrame();
					AnimationKeys.Add(InvComponentTransform * Transform * Relative, StartTime + AnimationFrame);
				}
			}
		}

		if (!bLayerCurrentBuffered)
		{
			BufferedTransforms = MoveTemp(AnimationKeys);
		}
		else
		{
			//The current BufferedTransforms need to layer on top of the animation keys
			BufferedTransforms = AnimationKeys.Collapse(BufferedTransforms);
		}
	}
}

bool UMovieScene3DTransformTrackRecorder::LoadRecordedFile(const FString& FileName, UMovieScene *InMovieScene, TMap<FGuid, AActor*>& ActorGuidToActorMap,  TFunction<void()> InCompletionCallback)
{
	bool bFileExists = TransformSerializer.DoesFileExist(FileName);
	if (bFileExists)
	{
		FText Error;
		FTransformFileHeader Header;

		if (TransformSerializer.OpenForRead(FileName, Header, Error))
		{
			TransformSerializer.GetDataRanges([this, InMovieScene, FileName, Header, InCompletionCallback](uint64 InMinFrameId, uint64 InMaxFrameId)
			{
				auto OnReadComplete = [this, InMovieScene, Header, InCompletionCallback]()
				{
					TArray<FTransformSerializedFrame> &InFrames = TransformSerializer.ResultData;
					if (InFrames.Num() > 0)
					{
						static FName Transform("Transform");
						MovieSceneTrack = InMovieScene->FindTrack<UMovieScene3DTransformTrack>(Header.Guid, Transform);
						if (!MovieSceneTrack.IsValid())
						{
							MovieSceneTrack = InMovieScene->AddTrack<UMovieScene3DTransformTrack>(Header.Guid);
						}
						else
						{
							MovieSceneTrack->RemoveAllAnimationData();
						}

						if (MovieSceneTrack.IsValid())
						{

							FFrameRate FileTickResolution = Header.TickResolution;
							FFrameRate TickResolution = InMovieScene->GetTickResolution();
							FFrameTime FrameTime;
							FFrameNumber CurrentFrame;

							MovieSceneSection = Cast<UMovieScene3DTransformSection>(MovieSceneTrack->CreateNewSection());

							MovieSceneTrack->AddSection(*MovieSceneSection);
							TArrayView<FMovieSceneFloatChannel*> FloatChannels = MovieSceneSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
							//MovieSceneSection->TimecodeSource = SequenceRecorderUtils::GetTimecodeSource();

							bool InFirst = true;
							// @todo: sequencer-timecode: this was previously never actually used - should it be??
							const ERichCurveInterpMode Interpolation = RCIM_Cubic;//AnimRecorder.IsValid() ? RCIM_Linear : RCIM_Cubic;
							TArray<FMovieSceneFloatValue> TransX;
							TransX.SetNumUninitialized(InFrames.Num());
							TArray<FMovieSceneFloatValue> TransY;
							TransY.SetNumUninitialized(InFrames.Num());
							TArray<FMovieSceneFloatValue> TransZ;
							TransZ.SetNumUninitialized(InFrames.Num());
							TArray<FMovieSceneFloatValue> RotX;
							RotX.SetNumUninitialized(InFrames.Num());
							TArray<FMovieSceneFloatValue> RotY;
							RotY.SetNumUninitialized(InFrames.Num());
							TArray<FMovieSceneFloatValue> RotZ;
							RotZ.SetNumUninitialized(InFrames.Num());
							TArray<FMovieSceneFloatValue> ScaleX;
							ScaleX.SetNumUninitialized(InFrames.Num());
							TArray<FMovieSceneFloatValue> ScaleY;
							ScaleY.SetNumUninitialized(InFrames.Num());
							TArray<FMovieSceneFloatValue> ScaleZ;
							ScaleZ.SetNumUninitialized(InFrames.Num());
							TArray<FFrameNumber> Times;
							Times.SetNumUninitialized(InFrames.Num());

							int Index = 0;
							FMovieSceneFloatValue NewValue;
							NewValue.InterpMode = RCIM_Cubic; //Linear if animation ? mz

							for (const FTransformSerializedFrame& SerializedFrame : InFrames)
							{
								const FSerializedTransform &Frame = SerializedFrame.Frame;

								FrameTime = FFrameRate::TransformTime(Frame.Time, FileTickResolution, TickResolution);
								CurrentFrame = FrameTime.FrameNumber;
								Times[Index] = CurrentFrame;

								if (InFirst)
								{
									MovieSceneSection->SetRange(TRange<FFrameNumber>::Inclusive(CurrentFrame, CurrentFrame));

									FloatChannels[0]->SetDefault(Frame.Values[0]);
									FloatChannels[1]->SetDefault(Frame.Values[1]);
									FloatChannels[2]->SetDefault(Frame.Values[2]);
									FloatChannels[3]->SetDefault(Frame.Values[3]);
									FloatChannels[4]->SetDefault(Frame.Values[4]);
									FloatChannels[5]->SetDefault(Frame.Values[5]);
									FloatChannels[6]->SetDefault(Frame.Values[6]);
									FloatChannels[7]->SetDefault(Frame.Values[7]);
									FloatChannels[8]->SetDefault(Frame.Values[8]);
									InFirst = false;
								}

								NewValue.Value = Frame.Values[0];
								TransX[Index] = NewValue;
								NewValue.Value = Frame.Values[1];
								TransY[Index] = NewValue;
								NewValue.Value = Frame.Values[2];
								TransZ[Index] = NewValue;
								NewValue.Value = Frame.Values[3];
								RotX[Index] = NewValue;
								NewValue.Value = Frame.Values[4];
								RotY[Index] = NewValue;
								NewValue.Value = Frame.Values[5];
								RotZ[Index] = NewValue;
								NewValue.Value = Frame.Values[6];
								ScaleX[Index] = NewValue;
								NewValue.Value = Frame.Values[7];
								ScaleY[Index] = NewValue;
								NewValue.Value = Frame.Values[8];
								ScaleZ[Index] = NewValue;

								MovieSceneSection->ExpandToFrame(CurrentFrame);
								++Index;
							}
							FloatChannels[0]->Set(Times, MoveTemp(TransX));
							FloatChannels[1]->Set(Times, MoveTemp(TransY));
							FloatChannels[2]->Set(Times, MoveTemp(TransZ));
							FloatChannels[3]->Set(Times, MoveTemp(RotX));
							FloatChannels[4]->Set(Times, MoveTemp(RotY));
							FloatChannels[5]->Set(Times, MoveTemp(RotZ));
							FloatChannels[6]->Set(Times, MoveTemp(ScaleX));
							FloatChannels[7]->Set(Times, MoveTemp(ScaleY));
							FloatChannels[8]->Set(Times, MoveTemp(ScaleZ));

						}
					}
					TransformSerializer.Close();
					InCompletionCallback();
				}; //callback

				TransformSerializer.ReadFramesAtFrameRange(InMinFrameId, InMaxFrameId, OnReadComplete);

			});
			return true;
		}
		else
		{
			TransformSerializer.Close();
		}
	}
	return false;
}