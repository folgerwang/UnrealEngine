// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Reusable 'Persona' features for asset editors concerned with USkeleton-related assets
 */
class IPersonaToolkit
{
public:
	/** Virtual destructor */
	virtual ~IPersonaToolkit() {}

	/** Get the skeleton that we are editing */
	virtual class USkeleton* GetSkeleton() const = 0;

	/** Get the editable skeleton that we are editing */
	virtual TSharedPtr<class IEditableSkeleton> GetEditableSkeleton() const = 0;

	/** Get the preview component that we are using */
	virtual class UDebugSkelMeshComponent* GetPreviewMeshComponent() const = 0;

	/** Get the skeletal mesh that we are editing */
	virtual class USkeletalMesh* GetMesh() const = 0;

	/** Set the skeletal mesh we are editing */
	virtual void SetMesh(class USkeletalMesh* InSkeletalMesh) = 0;

	/** Get the anim blueprint that we are editing */
	virtual class UAnimBlueprint* GetAnimBlueprint() const = 0;

	/** Get the animation asset that we are editing */
	virtual class UAnimationAsset* GetAnimationAsset() const = 0;

	/** Set the animation asset we are editing */
	virtual void SetAnimationAsset(class UAnimationAsset* InAnimationAsset) = 0;

	/** Get the preview scene that we are using */
	virtual TSharedRef<class IPersonaPreviewScene> GetPreviewScene() const = 0;

	/** Set the preview mesh, according to context (mesh, skeleton or animation etc.) */
	virtual class USkeletalMesh* GetPreviewMesh() const = 0;

	/** 
	 * Set the preview mesh, according to context (mesh, skeleton or animation etc.) 
	 * @param	InSkeletalMesh			The mesh to set
	 * @param	bSetPreviewMeshInAsset	If true, the mesh will be written to the asset so it can be permanently saved. 
	 *									Otherwise the change is merely transient and will reset next time the editor is opened.
	 */
	virtual void SetPreviewMesh(class USkeletalMesh* InSkeletalMesh, bool bSetPreviewMeshInAsset = true) = 0;

	/** Retrieve editor custom data. Return INDEX_NONE if the key is invalid */
	virtual int32 GetCustomData(const int32 Key) const { return INDEX_NONE; }
	
	/*
	 * Store the custom data using the key.
	 * Remark:
	 * The custom data memory should be clear when the editor is close by the user, this is not persistent data.
	 * Currently we use it to store the state of the editor UI to restore it properly when a refresh happen.
	 */
	virtual void SetCustomData(const int32 Key, const int32 CustomData) {}

	
	/** Get the context in which this toolkit is being used (usually the class name of the asset) */
	virtual FName GetContext() const = 0;
};
