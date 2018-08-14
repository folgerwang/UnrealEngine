// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneLiveLinkSectionRecorder.h"
#include "Modules/ModuleManager.h"
#include "MovieScene/MovieSceneLiveLinkSection.h"
#include "MovieScene/MovieSceneLiveLinkTrack.h"
#include "SequenceRecorderUtils.h"
#include "ISequenceRecorder.h"
#include "MotionControllerComponent.h"
#include "IMotionController.h"
#include "Features/IModularFeatures.h"
#include "LiveLinkClient.h"
#include "LiveLinkComponent.h"
#include "Misc/ScopedSlowTask.h"
#include "Channels/MovieSceneChannelProxy.h"

TSharedPtr<IMovieSceneSectionRecorder> FMovieSceneLiveLinkSectionRecorderFactory::CreateSectionRecorder(const struct FActorRecordingSettings& InActorRecordingSettings) const
{
	return MakeShareable(new FMovieSceneLiveLinkSectionRecorder);
}

bool FMovieSceneLiveLinkSectionRecorderFactory::CanRecordObject(UObject* InObjectToRecord) const
{
	if (Cast<UMotionControllerComponent>(InObjectToRecord) || Cast<ULiveLinkComponent>(InObjectToRecord))
	{
		return true;
	}
	return false;
}

void FMovieSceneLiveLinkSectionRecorder::SetLiveLinkSubjects(ULiveLinkComponent* MovieSceneLiveComp)
{
	SubjectNames.Reset();
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

		if (LiveLinkClient)
		{
			LiveLinkClient->GetSubjectNames(SubjectNames);
		}
	}
}

void FMovieSceneLiveLinkSectionRecorder::SetLiveLinkSubject(UMotionControllerComponent* MotionControllerComp)
{
	SubjectNames.Reset(0);

	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

		if (LiveLinkClient)
		{
			TArray<FName> InSubjectNames;
			LiveLinkClient->GetSubjectNames(InSubjectNames);

			TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
			IMotionController *CurrentController = nullptr;
			for (auto MotionController : MotionControllers)
			{
				if (MotionController == nullptr)
				{
					continue;
				}
				if (ETrackingStatus::Tracked == MotionController->GetControllerTrackingStatus(MotionControllerComp->PlayerIndex, MotionControllerComp->MotionSource))
				{
					FString MotionSourceString = MotionControllerComp->MotionSource.ToString();
					for (FName Name : InSubjectNames)
					{
						FString StringName = Name.ToString();
						int32 Index = MotionSourceString.Find(StringName);
						if (Index == 0)
						{
							SubjectNames.Add(Name);
							break;
						}
					}
				}
			}
		}
	}
}

void FMovieSceneLiveLinkSectionRecorder::CreateSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& Guid, float Time)
{
	ObjectGuid = Guid;
	ObjectToRecord = InObjectToRecord;
	MovieScene = InMovieScene;
	TimecodeSource = SequenceRecorderUtils::GetTimecodeSource();
	SecondsDiff = FPlatformTime::Seconds() - Time;

	UMotionControllerComponent* MotionControllerComp = Cast<UMotionControllerComponent>(ObjectToRecord.Get());
	if (MotionControllerComp)
	{
		SetLiveLinkSubject(MotionControllerComp);

	}
	else
	{
		ULiveLinkComponent* LiveLinkComponent = Cast<ULiveLinkComponent>(ObjectToRecord.Get());
		if (LiveLinkComponent)
		{
			SetLiveLinkSubjects(LiveLinkComponent);
		}
	}
	CreateTracks(InMovieScene, Guid, Time);
}

void FMovieSceneLiveLinkSectionRecorder::CreateTracks(UMovieScene* InMovieScene, const FGuid& Guid, float Time)
{
	MovieSceneSections.Reset(SubjectNames.Num());
	for (const FName& Name : SubjectNames)
	{
		TWeakObjectPtr<class UMovieSceneLiveLinkSection> MovieSceneSection;
		TWeakObjectPtr<class UMovieSceneLiveLinkTrack> MovieSceneTrack;

		MovieSceneTrack = InMovieScene->FindTrack<UMovieSceneLiveLinkTrack>(Guid, Name);
		if (!MovieSceneTrack.IsValid())
		{
			MovieSceneTrack = InMovieScene->AddTrack<UMovieSceneLiveLinkTrack>(Guid);
			MovieSceneTrack->SetPropertyNameAndPath(Name, Name.ToString());

		}
		else
		{
			MovieSceneTrack->RemoveAllAnimationData();
		}

		if (MovieSceneTrack.IsValid())
		{
			MovieSceneSection = Cast<UMovieSceneLiveLinkSection>(MovieSceneTrack->CreateNewSection());

			MovieSceneTrack->AddSection(*MovieSceneSection);

			MovieSceneSection->SetSubjectName(Name);

			FFrameRate   TickResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
			FFrameNumber CurrentFrame = (Time * TickResolution).FloorToFrame();

			MovieSceneSection->SetRange(TRange<FFrameNumber>::Inclusive(CurrentFrame, CurrentFrame));

			MovieSceneSection->TimecodeSource = TimecodeSource;
		}
		MovieSceneSections.Add(MovieSceneSection);
	}
}

/** Structure used to buffer up individual curve keys. Keys are inserted currently in Finalize Section and then moved to the channel */
struct FLiveLinkCurveKeys
{
	void Add(float Val, FFrameNumber FrameNumber)
	{
		FMovieSceneFloatValue NewValue(Val);
		NewValue.InterpMode = RCIM_Cubic;
		Curve.Add(NewValue);
		Times.Add(FrameNumber);
	}

	void Reserve(int32 Num)
	{
		Curve.Reserve(Num);
		Times.Reserve(Num);
	}

	void AddToFloatChannels(int32 StartIndex, TArray<FMovieSceneFloatChannel> &FloatChannels)
	{
		FloatChannels[StartIndex].Set(Times, MoveTemp(Curve));
		FloatChannels[StartIndex].AutoSetTangents();
	}


	TArray<FMovieSceneFloatValue> Curve;
	/*Unlike Transforms that will always have a key per frame, curves are optional and so miss frames, so we need to keep track of each curve. */
	TArray<FFrameNumber> Times;

};

/** Structure used to buffer up transform keys. Keys are inserted currently in Finalize Section and then moved to the channel */
struct FLiveLinkTransformKeys
{
	void Add(const FTransform& InTransform)
	{
		FMovieSceneFloatValue NewValue(InTransform.GetTranslation().X);
		NewValue.InterpMode = RCIM_Cubic;
		LocationX.Add(NewValue);
		NewValue = FMovieSceneFloatValue(InTransform.GetTranslation().Y);
		NewValue.InterpMode = RCIM_Cubic;
		LocationY.Add(NewValue);
		NewValue = FMovieSceneFloatValue(InTransform.GetTranslation().Z);
		NewValue.InterpMode = RCIM_Cubic;
		LocationZ.Add(NewValue);

		FRotator WoundRotation = InTransform.Rotator();
		NewValue = FMovieSceneFloatValue(WoundRotation.Roll);
		NewValue.InterpMode = RCIM_Cubic;
		RotationX.Add(NewValue);

		NewValue = FMovieSceneFloatValue(WoundRotation.Pitch);
		NewValue.InterpMode = RCIM_Cubic;
		RotationY.Add(NewValue);

		NewValue = FMovieSceneFloatValue(WoundRotation.Yaw);
		NewValue.InterpMode = RCIM_Cubic;
		RotationZ.Add(NewValue);

		NewValue = FMovieSceneFloatValue(InTransform.GetScale3D().X);
		NewValue.InterpMode = RCIM_Cubic;
		ScaleX.Add(NewValue);

		NewValue = FMovieSceneFloatValue(InTransform.GetScale3D().Y);
		NewValue.InterpMode = RCIM_Cubic;
		ScaleY.Add(NewValue);

		NewValue = FMovieSceneFloatValue(InTransform.GetScale3D().Z);
		NewValue.InterpMode = RCIM_Cubic;
		ScaleZ.Add(NewValue);

	}

	void Reserve(int32 Num)
	{
		LocationX.Reserve(Num);
		LocationY.Reserve(Num);
		LocationZ.Reserve(Num);

		RotationX.Reserve(Num);
		RotationY.Reserve(Num);
		RotationZ.Reserve(Num);

		ScaleX.Reserve(Num);
		ScaleY.Reserve(Num);
		ScaleZ.Reserve(Num);
	}

	TArray<FMovieSceneFloatValue> LocationX, LocationY, LocationZ;
	TArray<FMovieSceneFloatValue> RotationX, RotationY, RotationZ;
	TArray<FMovieSceneFloatValue> ScaleX, ScaleY, ScaleZ;

	void AddToFloatChannels(int32 StartIndex, TArray<FMovieSceneFloatChannel> &FloatChannels, const TArray<FFrameNumber> &Times)
	{
		FloatChannels[StartIndex].Set(Times, MoveTemp(LocationX));
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex].Set(Times, MoveTemp(LocationY));
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex].Set(Times, MoveTemp(LocationZ));
		FloatChannels[StartIndex++].AutoSetTangents();

		FloatChannels[StartIndex].Set(Times, MoveTemp(RotationX));
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex].Set(Times, MoveTemp(RotationY));
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex].Set(Times, MoveTemp(RotationZ));
		FloatChannels[StartIndex++].AutoSetTangents();

		FloatChannels[StartIndex].Set(Times, MoveTemp(ScaleX));
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex].Set(Times, MoveTemp(ScaleY));
		FloatChannels[StartIndex++].AutoSetTangents();
		FloatChannels[StartIndex].Set(Times, MoveTemp(ScaleZ));
		FloatChannels[StartIndex++].AutoSetTangents();
	}
};


void FMovieSceneLiveLinkSectionRecorder::FinalizeSection(float CurrentTime)
{
	if (ObjectToRecord.IsValid())
	{

		IModularFeatures& ModularFeatures = IModularFeatures::Get();
		ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

		if (LiveLinkClient  && SubjectNames.Num() > 0)
		{

			FScopedSlowTask SlowTask(SubjectNames.Num(), NSLOCTEXT("SequenceRecorder", "ProcessingLiveLink", "Processing LiveLink"));
			int32 SectionCount = 0;
			for (const FName& SubjectName : SubjectNames)
			{
				TWeakObjectPtr<class UMovieSceneLiveLinkSection> MovieSceneSection = MovieSceneSections[SectionCount++];

				FFrameRate TickResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
				FFrameNumber CurrentFrame = (CurrentTime * TickResolution).FloorToFrame();
				MovieSceneSection->ExpandToFrame(CurrentFrame);
				SlowTask.EnterProgressFrame();
				const TArray<FLiveLinkFrame>* Frames = LiveLinkClient->GetSubjectRawFrames(SubjectName);
				if (Frames)
				{
					bool InFirst = true;
					TArray<FFrameNumber> Times;
					Times.Reserve(Frames->Num());
					TArray<FLiveLinkTransformKeys> LinkTransformKeysArray;
					TArray<FLiveLinkCurveKeys>  LinkCurveKeysArray;

					for (const FLiveLinkFrame& Frame : *Frames)
					{
						int32 TransformIndex = 0;
						int32 CurveIndex = 0;
						//FQualifiedFrameTime QualifiedFrameTime = LiveLinkClient->GetQualifiedFrameTimeAtIndex(SubjectName.GetValue(), FrameIndex++);
						double Second = Frame.WorldTime.Time - SecondsDiff;
						FFrameNumber FrameNumber = (Second * TickResolution).FloorToFrame();
						Times.Add(FrameNumber);

						if (InFirst)
						{
							const FLiveLinkSubjectFrame *CurrentSubjectData = LiveLinkClient->GetSubjectData(SubjectName);
							FLiveLinkRefSkeleton RefSkeleton;
							FLiveLinkCurveKey  CurveKey;
							if (CurrentSubjectData)
							{
								RefSkeleton = CurrentSubjectData->RefSkeleton;
								CurveKey = CurrentSubjectData->CurveKeyData;
							}
							int32 NumChannels = MovieSceneSection->CreateChannelProxy(Frame,RefSkeleton, CurveKey);
							LinkTransformKeysArray.SetNum(Frame.Transforms.Num());
							for (FLiveLinkTransformKeys& TransformKeys : LinkTransformKeysArray)
							{
								TransformKeys.Reserve(Frames->Num());
							}
							LinkCurveKeysArray.SetNum(Frame.Curves.Num());
							for (FLiveLinkCurveKeys& CurveKeys : LinkCurveKeysArray)
							{
								CurveKeys.Reserve(Frames->Num());
							}
							InFirst = false;
						}

						for (const FTransform& Transform : Frame.Transforms)
						{
							LinkTransformKeysArray[TransformIndex++].Add(Transform);
						}
						for (const FOptionalCurveElement& Curve : Frame.Curves)
						{
							if (Curve.IsValid())
							{
								LinkCurveKeysArray[CurveIndex].Add(Curve.Value, FrameNumber);
							}
							CurveIndex += 1;
						}

					}

					TArray<FMovieSceneFloatChannel>& FloatChannels = MovieSceneSection->GetFloatChannels();
					int32 ChannelIndex = 0;
					for (FLiveLinkTransformKeys& TransformKeys : LinkTransformKeysArray)
					{
						TransformKeys.AddToFloatChannels(ChannelIndex, FloatChannels, Times);
						ChannelIndex += 9;
					}
					for (FLiveLinkCurveKeys CurveKeys : LinkCurveKeysArray)
					{
						CurveKeys.AddToFloatChannels(ChannelIndex, FloatChannels);
						ChannelIndex += 1;
					}
				}
			}
		}
	}
}

//MZ TODO STORE ITERATIVELY AND NOT JUST AT END SECTION
void FMovieSceneLiveLinkSectionRecorder::Record(float CurrentTime)
{

	/*

	if (ObjectToRecord.IsValid())
	{
		for (TWeakObjectPtr<class UMovieSceneLiveLinkSection> MovieSceneSection : MovieSceneSections)
		{
			if (MovieSceneSection.IsValid())
			{
				FFrameRate TickResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
				FFrameNumber CurrentFrame = (CurrentTime * TickResolution).FloorToFrame();

				MovieSceneSection->ExpandToFrame(CurrentFrame);
			}
		}
	}

	*/

}
