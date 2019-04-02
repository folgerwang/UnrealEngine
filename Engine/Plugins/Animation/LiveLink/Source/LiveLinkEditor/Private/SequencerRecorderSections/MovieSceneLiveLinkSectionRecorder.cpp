// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneLiveLinkSectionRecorder.h"
#include "Modules/ModuleManager.h"
#include "MovieScene/MovieSceneLiveLinkSection.h"
#include "MovieScene/MovieSceneLiveLinkTrack.h"
#include "MovieScene/MovieSceneLiveLinkBufferData.h"
#include "SequenceRecorderUtils.h"
#include "ISequenceRecorder.h"
#include "MotionControllerComponent.h"
#include "IMotionController.h"
#include "Features/IModularFeatures.h"
#include "LiveLinkClient.h"
#include "LiveLinkComponent.h"
#include "Misc/ScopedSlowTask.h"
//recording
#include "HAL/FileManagerGeneric.h"

//DEFINE_LOG_CATEGORY(LiveLinkSerialization);

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
	CachedFramesArray.SetNum(SubjectNames.Num());
	//LiveLinkSerializers.SetNum(SubjectNames.Num());

	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	SecondsDiff = FPlatformTime::Seconds() - Time;
	FText Error;

	if (LiveLinkClient)
	{
		int32 Index = 0;

		//FName SerializedType("LiveLink");

		//FLiveLinkManifestHeader  ManifestHeader(SerializedType);
		HandlerGuid = LiveLinkClient->StartRecordingLiveLink(SubjectNames);

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
				//MZ todo best place to save this out, make new option for this...

				//FString Temp = Name.ToString();
				//FString FileName = FString::Printf(TEXT("%s_%s"), *(SerializedType.ToString()), *Temp);

				const FLiveLinkSubjectFrame *CurrentSubjectData = LiveLinkClient->GetSubjectData(Name);
				if (CurrentSubjectData)
				{
					FLiveLinkRefSkeleton RefSkeleton;
					FLiveLinkCurveKey  CurveKey;
					if (CurrentSubjectData)
					{
						RefSkeleton = CurrentSubjectData->RefSkeleton;
						CurveKey = CurrentSubjectData->CurveKeyData;
					}
					int32 NumChannels = MovieSceneSection->CreateChannelProxy(RefSkeleton, CurveKey.CurveNames);
					/*
					FLiveLinkFileHeader Header(Name, SecondsDiff, CurrentSubjectData->RefSkeleton, CurrentSubjectData->CurveKeyData.CurveNames,
						SerializedType, Guid);
					LiveLinkSerializers[Index].SetLocalCaptureDir(Directory);
					
					if (!LiveLinkSerializers[Index].OpenForWrite(FileName, Header, Error))
					{
						UE_LOG(LiveLinkSerialization, Warning, TEXT("Error Opening LiveLink Sequencer File: Subject '%s' Error '%s'"), *(Name.ToString()), *(Error.ToString()));
					}
					else
					{
						ManifestHeader.SubjectNames.Add(Name);
					}
					*/
				}
				else
				{
					//UE_LOG(LiveLinkSerialization, Warning, TEXT("Error Getting LiveLink Subject Data File: Subject '%s' Error '%s'"), *(Name.ToString()), *(Error.ToString()));
				}
				++Index;
			}
			MovieSceneSections.Add(MovieSceneSection);
		}

		/*
		FString ManifestFileName = FString::Printf(TEXT("%s_%s"), *(SerializedType.ToString()), *ObjectToRecord->GetName());

		Serializer.SetLocalCaptureDir(Directory);

		if (!Serializer.OpenForWrite(ManifestFileName, ManifestHeader, Error))
		{
			UE_LOG(LiveLinkSerialization, Warning, TEXT("Error Opening Live Link Manifest file Error '%s'"), *(Error.ToString()));
		}
		else
		{
			Serializer.Close(); //just read header.
		}
		*/

		LastRotationValues.Reset();

	}
}


void FMovieSceneLiveLinkSectionRecorder::FinalizeSection(float CurrentTime)
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	if (LiveLinkClient)
	{
		LiveLinkClient->StopRecordingLiveLinkData(HandlerGuid, SubjectNames);
	}

	if (ObjectToRecord.IsValid())
	{
		if (SubjectNames.Num() > 0)
		{
			Record(CurrentTime); //
			int32 SectionCount = 0;
			for (const FName& SubjectName : SubjectNames)
			{
				TWeakObjectPtr<class UMovieSceneLiveLinkSection> MovieSceneSection = MovieSceneSections[SectionCount];
				TArray<FMovieSceneFloatChannel>& FloatChannels = MovieSceneSection->GetFloatChannels();
				for (FMovieSceneFloatChannel& Channel : FloatChannels)
				{
					Channel.AutoSetTangents();
				}
				//LiveLinkSerializers[SectionCount++].Close();
			}
		}
		return;
	}
}


void FMovieSceneLiveLinkSectionRecorder::Record(float CurrentTime)
{
	if (ObjectToRecord.IsValid())
	{
		IModularFeatures& ModularFeatures = IModularFeatures::Get();
		ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		if (LiveLinkClient  && SubjectNames.Num() > 0)
		{
			int32 SectionCount = 0;
			for (const FName& SubjectName : SubjectNames)
			{
				TWeakObjectPtr<class UMovieSceneLiveLinkSection> MovieSceneSection = MovieSceneSections[SectionCount];

				FFrameRate TickResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
				FFrameNumber CurrentFrame = (CurrentTime * TickResolution).FloorToFrame();
				MovieSceneSection->ExpandToFrame(CurrentFrame);

				TArray<FLiveLinkFrame>& Frames = CachedFramesArray[SectionCount];
				LiveLinkClient->GetAndFreeLastRecordedFrames(HandlerGuid, SubjectName, Frames);
				if (Frames.Num() > 0)
				{
					//LiveLinkSerializers[SectionCount].WriteFrameData(LiveLinkSerializers[SectionCount].FramesWritten, Frames);

					bool InFirst = true;
					TArray<FFrameNumber> Times;
					Times.Reserve(Frames.Num());
					TArray<FLiveLinkTransformKeys> LinkTransformKeysArray;
					TArray<FLiveLinkCurveKeys>  LinkCurveKeysArray;

					for (const FLiveLinkFrame& Frame : Frames)
					{
						int32 TransformIndex = 0;
						int32 CurveIndex = 0;
						//FQualifiedFrameTime QualifiedFrameTime = LiveLinkClient->GetQualifiedFrameTimeAtIndex(SubjectName.GetValue(), FrameIndex++);
						double Second = Frame.WorldTime.Time - SecondsDiff;
						FFrameNumber FrameNumber = (Second * TickResolution).FloorToFrame();
						Times.Add(FrameNumber);

						if (InFirst)
						{
							LinkTransformKeysArray.SetNum(Frame.Transforms.Num());
							for (FLiveLinkTransformKeys& TransformKeys : LinkTransformKeysArray)
							{
								TransformKeys.Reserve(Frames.Num());
							}
							LinkCurveKeysArray.SetNum(Frame.Curves.Num());
							for (FLiveLinkCurveKeys& CurveKeys : LinkCurveKeysArray)
							{
								CurveKeys.Reserve(Frames.Num());
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
						TransformKeys.AppendToFloatChannelsAndReset(ChannelIndex, FloatChannels, Times, LastRotationValues);
						ChannelIndex += 9;
					}
					for (FLiveLinkCurveKeys CurveKeys : LinkCurveKeysArray)
					{
						CurveKeys.AppendToFloatChannelsAndReset(ChannelIndex, FloatChannels);
						ChannelIndex += 1;
					}
				} //if frames.num > 0
				++SectionCount;
			}//for each subject
		}
	}
}


/*
bool FMovieSceneLiveLinkSectionRecorder::LoadManifestFile(const FString& FileName, UMovieScene *InMovieScene, TFunction<void()> InCompletionCallback)
{

	bool bFileExists = Serializer.DoesFileExist(FileName);
	if (bFileExists)
	{
		FText Error;
		FLiveLinkManifestHeader Header;
		FString PathPart, FilenamePart, ExtensionPart;

		if (Serializer.OpenForRead(FileName, Header, Error))
		{
			for (const FName& SubjectName : Header.SubjectNames)
			{
				FPaths::Split(FileName, PathPart, FilenamePart, ExtensionPart);
				FilenamePart = FString("/") + Header.SerializedType.ToString() + FString("_") + SubjectName.ToString();
				LoadSubjectFile(PathPart + FilenamePart, InMovieScene, InCompletionCallback);
			}
			Serializer.Close();
		}
		else
		{
			Serializer.Close();
		}
		return true;
	}
	return false;

}

bool FMovieSceneLiveLinkSectionRecorder::LoadSubjectFile(const FString& FileName, UMovieScene *InMovieScene, TFunction<void()> InCompletionCallback)
{

	LiveLinkSerializers.SetNum(1);

	bool bFileExists = LiveLinkSerializers[0].DoesFileExist(FileName);
	if (bFileExists)
	{
		FText Error;
		FLiveLinkFileHeader Header;

		if (LiveLinkSerializers[0].OpenForRead(FileName, Header, Error))
		{
			LiveLinkSerializers[0].GetDataRanges([this, InMovieScene, FileName, Header, InCompletionCallback](uint64 InMinFrameId, uint64 InMaxFrameId)
			{
				auto OnReadComplete = [this, InMovieScene, Header, InCompletionCallback]()
				{
					TArray<FLiveLinkSerializedFrame> &InFrames = LiveLinkSerializers[0].ResultData;
					if (InFrames.Num() > 0)
					{

						TWeakObjectPtr<class UMovieSceneLiveLinkSection> MovieSceneSection;
						TWeakObjectPtr<class UMovieSceneLiveLinkTrack> MovieSceneTrack;

						MovieSceneTrack = InMovieScene->FindTrack<UMovieSceneLiveLinkTrack>(Header.Guid, Header.SubjectName);
						if (!MovieSceneTrack.IsValid())
						{
							MovieSceneTrack = InMovieScene->AddTrack<UMovieSceneLiveLinkTrack>(Header.Guid);
							MovieSceneTrack->SetPropertyNameAndPath(Header.SubjectName, Header.SubjectName.ToString());

						}
						else
						{
							MovieSceneTrack->RemoveAllAnimationData();
						}

						if (MovieSceneTrack.IsValid())
						{
							FName SubjectName;
							FLiveLinkFrameData FrameData;
							FLiveLinkRefSkeleton RefSkeleton;
							MovieSceneSection = Cast<UMovieSceneLiveLinkSection>(MovieSceneTrack->CreateNewSection());
							MovieSceneTrack->AddSection(*MovieSceneSection);

							MovieSceneSection->SetSubjectName(Header.SubjectName);
							FFrameRate   TickResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();

							MovieSceneSection->TimecodeSource = FMovieSceneTimecodeSource(FApp::GetTimecode());

							int32 NumChannels = MovieSceneSection->CreateChannelProxy(Header.RefSkeleton, Header.CurveNames);
							if (NumChannels > 0)
							{
								TArray<FFrameNumber> Times;
								Times.Reserve(InFrames.Num());
								TArray<FLiveLinkTransformKeys> LinkTransformKeysArray;
								TArray<FLiveLinkCurveKeys>  LinkCurveKeysArray;
								bool InFirst = true;
								for (const FLiveLinkSerializedFrame& SerializedFrame : InFrames)
								{
									const FLiveLinkFrame &Frame = SerializedFrame.Frame;

									int32 TransformIndex = 0;
									int32 CurveIndex = 0;
									//FQualifiedFrameTime QualifiedFrameTime = LiveLinkClient->GetQualifiedFrameTimeAtIndex(SubjectName.GetValue(), FrameIndex++);
									double Second = Frame.WorldTime.Time - Header.SecondsDiff;
									FFrameNumber FrameNumber = (Second * TickResolution).FloorToFrame();
									Times.Add(FrameNumber);
									MovieSceneSection->ExpandToFrame(FrameNumber);

									if (InFirst)
									{
										LinkTransformKeysArray.SetNum(Frame.Transforms.Num());
										for (FLiveLinkTransformKeys& TransformKeys : LinkTransformKeysArray)
										{
											TransformKeys.Reserve(InFrames.Num());
										}
										LinkCurveKeysArray.SetNum(Frame.Curves.Num());
										for (FLiveLinkCurveKeys& CurveKeys : LinkCurveKeysArray)
										{
											CurveKeys.Reserve(InFrames.Num());
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
					LiveLinkSerializers[0].Close();
					InCompletionCallback();

				}; //callback

				LiveLinkSerializers[0].ReadFramesAtFrameRange(InMinFrameId, InMaxFrameId, OnReadComplete);

			});
			return true;
		}
		else
		{
			LiveLinkSerializers[0].Close();

		}
	}
	return false;
}
bool FMovieSceneLiveLinkSectionRecorder::LoadRecordedFile(const FString& FileName, UMovieScene *InMovieScene, TFunction<void()> InCompletionCallback,  UObject *InObjectToLoad)
{
	FLiveLinkManifestHeader Header;
	TMovieSceneSerializer<FLiveLinkManifestHeader, FLiveLinkManifestHeader>  ManifestCheckSerializer;
	bool bFileExists = ManifestCheckSerializer.DoesFileExist(FileName);
	if (bFileExists)
	{
		FText Error;
		if (ManifestCheckSerializer.OpenForRead(FileName, Header, Error))
		{
			ManifestCheckSerializer.Close();
			//reuse LiveLinkSerializers Array
			if (Header.bIsManifest)
			{
				return LoadManifestFile(FileName, InMovieScene, InCompletionCallback);
			}
			else
			{
				return LoadSubjectFile(FileName, InMovieScene, InCompletionCallback);
			}
		}
		else
		{
			ManifestCheckSerializer.Close();
		}
	}
	return false;

}
*/