// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkRefSkeleton.h"
#include "Features/IModularFeature.h"
#include "LiveLinkTypes.h"
#include "Misc/Guid.h"

class ILiveLinkSource;
struct FTimecode;

class LIVELINKINTERFACE_API ILiveLinkClient : public IModularFeature
{
public:
	virtual ~ILiveLinkClient() {}

	static FName ModularFeatureName;

	// Add a new live link source to the client
	virtual void AddSource(TSharedPtr<ILiveLinkSource> InSource) = 0;

	// Remove the specified source from the live link client
	virtual void RemoveSource(TSharedPtr<ILiveLinkSource> InSource) = 0;

	virtual void PushSubjectSkeleton(FGuid SourceGuid, FName SubjectName, const FLiveLinkRefSkeleton& RefSkeleton) = 0;
	virtual void PushSubjectData(FGuid SourceGuid, FName SubjectName, const FLiveLinkFrameData& FrameData) = 0;
	virtual void ClearSubject(FName SubjectName) = 0;

	// Populates an array with in-use subject names
	virtual void GetSubjectNames(TArray<FName>& SubjectNames) = 0;

	//Get Whether or not we are saving each frame or not or not
	virtual bool GetSaveFrames() const = 0;

	//Set Whether or not we are saving each frame
	//Returns whether or not we were previously saving.
	//If we were saving frames and now don't we remove the previously saved frames
	virtual bool SetSaveFrames(bool InSave) = 0;

	//Clear the stored frames associated with this subject
	virtual void ClearSubjectsFrames(FName SubjectName) = 0;

	//Clear All Subjects Frames
	virtual void ClearAllSubjectsFrames() = 0;

	virtual const FLiveLinkSubjectFrame* GetSubjectData(FName SubjectName) = 0;
	virtual const FLiveLinkSubjectFrame* GetSubjectDataAtWorldTime(FName SubjectName, double WorldTime) = 0;
	virtual const FLiveLinkSubjectFrame* GetSubjectDataAtSceneTime(FName SubjectName, const FTimecode& SceneTime) = 0;

	//Whether or not the subject's data is time syncrhonized or not
	virtual bool IsSubjectTimeSynchronized(FName SubjectName)  = 0;

	//Efficiently get data 
	virtual const TArray<FLiveLinkFrame>* GetSubjectRawFrames(FName SubjectName) = 0;

	//Functions to start recording live link data into a saved buffer that can that can queried dynamically for uses 
	//such as recording in the sequencer or for serialization.
	//Note that it's the responsibility of the client to handle any changes to the Subjects structure.
	//Note that it's the responsibility of the client to make sure that StopRecordingLiveLinkData is called on all subjects that it
	//started recording for or recording will continue unabated eventually causing the system to run of memory.

	//Start Recording the Live Link data for these Subjects.
	//Returns a GUID  that is used as an unique identifier to get the data or stop the recording.
	virtual FGuid StartRecordingLiveLink(const TArray<FName>& SubjectNames) = 0;
	virtual FGuid StartRecordingLiveLink(const FName& SubjectName) = 0;
	//Stop recording the live link data. This will free all memory. Must be called with correct Guid/SubjectNames used 
	//when starting recording.
	virtual void StopRecordingLiveLinkData(const FGuid &InGuid, const TArray<FName>& SubjectNames) = 0;
	virtual void StopRecordingLiveLinkData(const FGuid &InGuid, const FName& SubjectName) = 0;
	//Returns the copy of the recorded frames from the initial start or the last call to this.
	//Note that it also clears the frames so that the next time this function is called you get the next set of frames from 
	//the last time you called this method. If no new frames have come in OutFrames will be empty.
	virtual void GetAndFreeLastRecordedFrames(const FGuid& InHandlerGuid, FName SubjectName, TArray<FLiveLinkFrame> &OutFrames) = 0;

	//Specify that only this subject should accept frames from the specified source and any other source that has
	//been added to the whitelist. If no sources have been added then all sources can publish data on that subject,
	//but may cause interference issues between competing sources.
	virtual void AddSourceToSubjectWhiteList(FName SubjectName, FGuid SourceGuid) = 0;
	//Remove source from whitelist. If no sources are left in whitelest then all sources are active and publish.
	virtual void RemoveSourceFromSubjectWhiteList(FName SubjectName, FGuid SourceGuid) = 0;
	//Clear whitelist, and all sources are now active.
	virtual void ClearSourceWhiteLists() = 0;
	//Register for when subjects change
	virtual FDelegateHandle RegisterSubjectsChangedHandle(const FSimpleMulticastDelegate::FDelegate& SubjectsChanged) = 0;
	virtual void UnregisterSubjectsChangedHandle(FDelegateHandle Handle) = 0;

};

