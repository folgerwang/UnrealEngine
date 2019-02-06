// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneLiveLinkTrackRecorder.h"
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
#include "MovieSceneFolder.h"
#include "Engine/TimecodeProvider.h"
#include "Engine/Engine.h"
#include "TakeRecorderSource/TakeRecorderLiveLinkSource.h"

//recording
#include "HAL/FileManagerGeneric.h"

DEFINE_LOG_CATEGORY(LiveLinkSerialization);

void UMovieSceneLiveLinkTrackRecorder::CreateTrack(UMovieScene* InMovieScene, const FName& InSubjectName, UMovieSceneTrackRecorderSettings* InSettingsObject)
{
	MovieScene = InMovieScene;
	SubjectName = InSubjectName;
	CreateTracks();
}

UMovieSceneLiveLinkTrack* UMovieSceneLiveLinkTrackRecorder::DoesLiveLinkMasterTrackExist(const FName& MasterTrackName)
{
	for (UMovieSceneTrack* MasterTrack : MovieScene->GetMasterTracks())
	{
		if (MasterTrack->IsA(UMovieSceneLiveLinkTrack::StaticClass()))
		{
			UMovieSceneLiveLinkTrack* TestLiveLinkTrack = CastChecked<UMovieSceneLiveLinkTrack>(MasterTrack);
			if (TestLiveLinkTrack && TestLiveLinkTrack->GetPropertyName() == MasterTrackName)
			{
				return TestLiveLinkTrack;
			}
		}
	}
	return nullptr;
}

void UMovieSceneLiveLinkTrackRecorder::CreateTracks()
{

	LiveLinkTrack = nullptr;
	MovieSceneSection.Reset();

	CachedFramesArray.SetNum(0);

	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	FText Error;
	
	if (LiveLinkClient && SubjectName != NAME_None)
	{

		FName SerializedType("LiveLink");
		FLiveLinkManifestHeader  ManifestHeader(SerializedType);

		HandlerGuid = LiveLinkClient->StartRecordingLiveLink(SubjectName);

		LiveLinkTrack = DoesLiveLinkMasterTrackExist(SubjectName);
		if (!LiveLinkTrack.IsValid())
		{
			LiveLinkTrack = MovieScene->AddMasterTrack<UMovieSceneLiveLinkTrack>();
		}
		else
		{
			LiveLinkTrack->RemoveAllAnimationData();
		}

		LiveLinkTrack->SetPropertyNameAndPath(SubjectName, SubjectName.ToString());

		MovieSceneSection = Cast<UMovieSceneLiveLinkSection>(LiveLinkTrack->CreateNewSection());
		MovieSceneSection->SetIsActive(false);
		LiveLinkTrack->AddSection(*MovieSceneSection);

		MovieSceneSection->SetSubjectName(SubjectName);

		//MZ todo best place to save this out, make new option for this...
		FString Temp = SubjectName.ToString();
		FString FileName = FString::Printf(TEXT("%s_%s"), *(SerializedType.ToString()), *Temp);

		const FLiveLinkSubjectFrame *CurrentSubjectData = LiveLinkClient->GetSubjectData(SubjectName);
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

			FLiveLinkFileHeader Header(SubjectName, SecondsDiff, CurrentSubjectData->RefSkeleton, CurrentSubjectData->CurveKeyData.CurveNames,
				SerializedType, ObjectGuid);
			LiveLinkSerializer.SetLocalCaptureDir(Directory);

			if (!LiveLinkSerializer.OpenForWrite(FileName, Header, Error))
			{
				UE_LOG(LiveLinkSerialization, Warning, TEXT("Error Opening LiveLink Sequencer File: Subject '%s' Error '%s'"), *(SubjectName.ToString()), *(Error.ToString()));
			}
			else
			{
				ManifestHeader.SubjectNames.Add(SubjectName);
			}
		}
		else
		{
			UE_LOG(LiveLinkSerialization, Warning, TEXT("Error Getting LiveLink Subject Data File: Subject '%s' Error '%s'"), *(SubjectName.ToString()), *(Error.ToString()));
		}
	
		FString ManifestFileName = FString::Printf(TEXT("%s"), *(SerializedType.ToString()));

		Serializer.SetLocalCaptureDir(Directory);

		if (!Serializer.OpenForWrite(ManifestFileName, ManifestHeader, Error))
		{
			UE_LOG(LiveLinkSerialization, Warning, TEXT("Error Opening Live Link Manifest file Error '%s'"), *(Error.ToString()));
		}
		else
		{
			Serializer.Close(); //just read header.
		}
	}
}

void UMovieSceneLiveLinkTrackRecorder::SetSectionStartTimecodeImpl(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame) 
{
	float Time = 0.0f;
	SecondsDiff = FPlatformTime::Seconds() - Time;

	if (MovieSceneSection.IsValid())
	{
		MovieSceneSection->TimecodeSource = FMovieSceneTimecodeSource(InSectionStartTimecode);
	}
	LastRotationValues.Reset();
}

void UMovieSceneLiveLinkTrackRecorder::StopRecordingImpl()
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	if (LiveLinkClient && MovieSceneSection.IsValid())
	{
		LiveLinkClient->StopRecordingLiveLinkData(HandlerGuid, SubjectName);
	}
}

void UMovieSceneLiveLinkTrackRecorder::FinalizeTrackImpl()
{
	if (MovieSceneSection.IsValid())
	{
		TArray<FMovieSceneFloatChannel>& FloatChannels = MovieSceneSection->GetFloatChannels();
		if (bReduceKeys)
		{
			FKeyDataOptimizationParams Params;
			Params.bAutoSetInterpolation = true;
			for (FMovieSceneFloatChannel& Channel : FloatChannels)
			{
				Channel.Optimize(Params);
			}
		}
		else
		{
			for (FMovieSceneFloatChannel& Channel : FloatChannels)
			{
				Channel.AutoSetTangents();
			}
		}
		LiveLinkSerializer.Close();
		MovieSceneSection->SetIsActive(true);
	}
}

void UMovieSceneLiveLinkTrackRecorder::RecordSampleImpl(const FQualifiedFrameTime& CurrentTime)
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	if (LiveLinkClient && MovieSceneSection.IsValid())
	{
		//we know all section have same tick resoultion
		FFrameRate   TickResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
		
		FFrameNumber CurrentFrame = CurrentTime.ConvertTo(TickResolution).FloorToFrame();
		FFrameNumber FrameNumber;

		bool bSynced = LiveLinkClient->IsSubjectTimeSynchronized(SubjectName);

		MovieSceneSection->ExpandToFrame(MovieSceneSection->GetInclusiveStartFrame() + CurrentFrame);

		TArray<FLiveLinkFrame>& Frames = CachedFramesArray;
		LiveLinkClient->GetAndFreeLastRecordedFrames(HandlerGuid, SubjectName, Frames);
		if (Frames.Num() > 0)
		{
			LiveLinkSerializer.WriteFrameData(LiveLinkSerializer.FramesWritten, Frames);

			bool bInFirst = true;
			static TArray<FFrameNumber> Times;
			Times.Reset();
			static TArray<FLiveLinkTransformKeys> LinkTransformKeysArray;
			static TArray<FLiveLinkCurveKeys>  LinkCurveKeysArray;

			for (const FLiveLinkFrame& Frame : Frames)
			{
				int32 TransformIndex = 0;
				int32 CurveIndex = 0;
				if (bSynced && GEngine && GEngine->GetTimecodeProvider())
				{
					FQualifiedFrameTime QualifiedFrameTime = Frame.MetaData.SceneTime;
					const UTimecodeProvider* TimecodeProvider = GEngine->GetTimecodeProvider();
					FFrameNumber FrameNumberStart = MovieSceneSection->TimecodeSource.Timecode.ToFrameNumber(TimecodeProvider->GetFrameRate());
					QualifiedFrameTime.Time.FrameNumber -= FrameNumberStart;

					FFrameTime FrameTime = QualifiedFrameTime.ConvertTo(TickResolution);
					FrameNumber = FrameTime.FrameNumber;
				}
				else
				{
					double Second = Frame.WorldTime.Time + Frame.WorldTime.Offset - SecondsDiff;
					FrameNumber = (Second * TickResolution).FloorToFrame();
				}
				if (FrameNumber >= 0)
				{
					Times.Add(FrameNumber);
					MovieSceneSection->ExpandToFrame(MovieSceneSection->GetInclusiveStartFrame() + FrameNumber);
					if (bInFirst)
					{
						LinkTransformKeysArray.Reset(Frame.Transforms.Num());
						LinkTransformKeysArray.SetNum(Frame.Transforms.Num());
						for (FLiveLinkTransformKeys& TransformKeys : LinkTransformKeysArray)
						{
							TransformKeys.Reserve(Frames.Num());
						}
						LinkCurveKeysArray.Reset(Frame.Curves.Num());
						LinkCurveKeysArray.SetNum(Frame.Curves.Num());
						for (FLiveLinkCurveKeys& CurveKeys : LinkCurveKeysArray)
						{
							CurveKeys.Reserve(Frames.Num());
						}
						bInFirst = false;
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
			}
			if (bInFirst == false) //Never got a good frame if true
			{
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
			}

		} //if frames > 0
	}
}

void UMovieSceneLiveLinkTrackRecorder::AddContentsToFolder(UMovieSceneFolder* InFolder)
{
	if (LiveLinkTrack.IsValid())
	{
		InFolder->AddChildMasterTrack(LiveLinkTrack.Get());
	}
}

bool UMovieSceneLiveLinkTrackRecorder::LoadManifestFile(const FString& FileName, UMovieScene *InMovieScene, TFunction<void()> InCompletionCallback)
{
	bool bFileExists = Serializer.DoesFileExist(FileName);
	if (bFileExists)
	{
		FText Error;
		FLiveLinkManifestHeader Header;
		FString PathPart, FilenamePart, ExtensionPart;

		if (Serializer.OpenForRead(FileName, Header, Error))
		{
			for (const FName& SSubjectName : Header.SubjectNames)
			{
				FPaths::Split(FileName, PathPart, FilenamePart, ExtensionPart);
				FilenamePart = FString("/") + Header.SerializedType.ToString() + FString("_") + SSubjectName.ToString();
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

bool UMovieSceneLiveLinkTrackRecorder::LoadSubjectFile(const FString& FileName, UMovieScene *InMovieScene, TFunction<void()> InCompletionCallback)
{

	bool bFileExists = LiveLinkSerializer.DoesFileExist(FileName);
	if (bFileExists)
	{
		FText Error;
		FLiveLinkFileHeader Header;

		if (LiveLinkSerializer.OpenForRead(FileName, Header, Error))
		{
			LiveLinkSerializer.GetDataRanges([this, InMovieScene, FileName, Header, InCompletionCallback](uint64 InMinFrameId, uint64 InMaxFrameId)
			{
				auto OnReadComplete = [this, InMovieScene, Header, InCompletionCallback]()
				{
					TArray<FLiveLinkSerializedFrame> &InFrames = LiveLinkSerializer.ResultData;
					if (InFrames.Num() > 0)
					{
						LiveLinkTrack = MovieScene->FindMasterTrack<UMovieSceneLiveLinkTrack>();
						if (!LiveLinkTrack.IsValid())
						{
							LiveLinkTrack = MovieScene->AddMasterTrack<UMovieSceneLiveLinkTrack>();
							LiveLinkTrack->SetPropertyNameAndPath(Header.SubjectName, Header.SubjectName.ToString());
						}
						else
						{
							LiveLinkTrack->RemoveAllAnimationData();
						}

						if (LiveLinkTrack.IsValid())
						{

							MovieSceneSection = Cast<UMovieSceneLiveLinkSection>(LiveLinkTrack->CreateNewSection());
							LiveLinkTrack->AddSection(*MovieSceneSection);
							MovieSceneSection->SetSubjectName(Header.SubjectName);
							
							FLiveLinkFrameData FrameData;
							FLiveLinkRefSkeleton RefSkeleton;
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
									double Second = Frame.WorldTime.Time - Header.SecondsDiff;
									FFrameNumber FrameNumber = (Second * TickResolution).FloorToFrame();
									Times.Add(FrameNumber);
									MovieSceneSection->ExpandToFrame(MovieSceneSection->GetInclusiveStartFrame() + FrameNumber);

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
					LiveLinkSerializer.Close();
					InCompletionCallback();

				}; //callback

				LiveLinkSerializer.ReadFramesAtFrameRange(InMinFrameId, InMaxFrameId, OnReadComplete);

			}); 
			return true;
		}
		else
		{
			LiveLinkSerializer.Close();

		}
	}
	return false;
}

bool UMovieSceneLiveLinkTrackRecorder::LoadRecordedFile(const FString& FileName, UMovieScene *InMovieScene, TMap<FGuid, AActor*>& ActorGuidToActorMap, TFunction<void()> InCompletionCallback) 
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
