// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LiveLinkBlueprintStructs.h"
#include "LiveLinkBlueprintLibrary.generated.h"

UCLASS()
class LIVELINK_API ULiveLinkBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

// FSubjectFrameHandle 

	// Returns the float curves stored in the Subject Frame as a map
	UFUNCTION(BlueprintPure, Category = "LiveLink")
	static void GetCurves(UPARAM(ref) FSubjectFrameHandle& SubjectFrameHandle, TMap<FName, float>& Curves);

	// Returns the number of Transforms stored in the Subject Frame
	UFUNCTION(BlueprintPure, Category = "LiveLink")
	static int NumberOfTransforms(UPARAM(ref) FSubjectFrameHandle& SubjectFrameHandle);

	// Returns an array of Transform Names stored in the Subject Frame
	UFUNCTION(BlueprintPure, Category = "LiveLink")
	static void TransformNames(UPARAM(ref) FSubjectFrameHandle& SubjectFrameHandle, TArray<FName>& TransformNames);

	// Returns the Root Transform for the Subject Frame as a LiveLink Transform or the Identity if there are no transforms.
	UFUNCTION(BlueprintPure, Category = "LiveLink")
	static void GetRootTransform(UPARAM(ref) FSubjectFrameHandle& SubjectFrameHandle, FLiveLinkTransform& LiveLinkTransform);

	// Returns the LiveLink Transform stored in a Subject Frame at a given index. Returns an Identity transform if Transform Index is invalid.
	UFUNCTION(BlueprintPure, Category = "LiveLink")
	static void GetTransformByIndex(UPARAM(ref) FSubjectFrameHandle& SubjectFrameHandle, int TransformIndex, FLiveLinkTransform& LiveLinkTransform);

	// Returns the LiveLink Transform stored in a Subject Frame with a given name. Returns an Identity transform if Transform Name is invalid.
	UFUNCTION(BlueprintPure, Category = "LiveLink")
	static void GetTransformByName(UPARAM(ref) FSubjectFrameHandle& SubjectFrameHandle, FName TransformName, FLiveLinkTransform& LiveLinkTransform);

	// Returns the Subject Metadata structure stored in the Subject Frame
	UFUNCTION(BlueprintPure, Category = "LiveLink")
	static void GetMetadata(UPARAM(ref) FSubjectFrameHandle& SubjectFrameHandle, FSubjectMetadata& Metadata);

// FLiveLinkTransform

	// Returns the Name of a given LiveLink Transform
	UFUNCTION(BlueprintPure, Category = "LiveLink")
	static void TransformName(UPARAM(ref) FLiveLinkTransform& LiveLinkTransform, FName& Name);

	// Returns the Transform value in Parent Space for a given LiveLink Transform
	UFUNCTION(BlueprintPure, Category = "LiveLink")
	static void ParentBoneSpaceTransform(UPARAM(ref) FLiveLinkTransform& LiveLinkTransform, FTransform& Transform);

	// Returns the Transform value in Root Space for a given LiveLink Transform
	UFUNCTION(BlueprintPure, Category = "LiveLink")
	static void ComponentSpaceTransform(UPARAM(ref) FLiveLinkTransform& LiveLinkTransform, FTransform& Transform);

	// Returns whether a given LiveLink Transform has a parent transform
	UFUNCTION(BlueprintPure, Category = "LiveLink")
	static bool HasParent(UPARAM(ref) FLiveLinkTransform& LiveLinkTransform);

	// Returns the Parent LiveLink Transform if one exists or an Identity transform if no parent exists
	UFUNCTION(BlueprintPure, Category = "LiveLink")
	static void GetParent(UPARAM(ref) FLiveLinkTransform& LiveLinkTransform, FLiveLinkTransform& Parent);

	// Returns the number of Children for a given LiveLink Transform
	UFUNCTION(BlueprintPure, Category = "LiveLink")
	static int ChildCount(UPARAM(ref) FLiveLinkTransform& LiveLinkTransform);

	// Returns an array of Child LiveLink Transforms for a given LiveLink Transform
	UFUNCTION(BlueprintPure, Category = "LiveLink")
	static void GetChildren(UPARAM(ref) FLiveLinkTransform& LiveLinkTransform, TArray<FLiveLinkTransform>& Children);

// FLiveLinkSourceHandle

	// Checks whether the LiveLink Source is valid via its handle
	UFUNCTION(BlueprintCallable, Category = "LiveLink")
	static bool IsSourceStillValid(UPARAM(ref) FLiveLinkSourceHandle& SourceHandle);

	// Requests the given LiveLink Source to shut down via its handle
	UFUNCTION(BlueprintCallable, Category = "LiveLink")
	static bool RequestShutdown(UPARAM(ref) FLiveLinkSourceHandle& SourceHandle);

	// Get the text status of a LiveLink Source via its handle
	UFUNCTION(BlueprintCallable, Category = "LiveLink")
	static FText GetSourceStatus(UPARAM(ref) FLiveLinkSourceHandle& SourceHandle);

	// Get the type of a LiveLink Source via its handle
	UFUNCTION(BlueprintCallable, Category = "LiveLink")
	static FText GetSourceType(UPARAM(ref) FLiveLinkSourceHandle& SourceHandle);

	// Get the machine name of a LiveLink Source via its handle
	UFUNCTION(BlueprintCallable, Category = "LiveLink")
	static FText GetSourceMachineName(UPARAM(ref) FLiveLinkSourceHandle& SourceHandle);
};