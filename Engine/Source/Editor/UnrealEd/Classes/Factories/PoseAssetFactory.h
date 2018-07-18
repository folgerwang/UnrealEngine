// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/**
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "PoseAssetFactory.generated.h"

struct FAssetData;
class SWindow;
class SMultiLineEditableTextBox;

UCLASS(HideCategories=Object,MinimalAPI)
class UPoseAssetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	class USkeleton* TargetSkeleton;
	
	/** The preview mesh to use with this pose asset */
	UPROPERTY()
	class USkeletalMesh* PreviewSkeletalMesh;

	/* Used when creating a composite from an AnimSequence, becomes the only AnimSequence contained */
	UPROPERTY()
	class UAnimSequence* SourceAnimation;

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	

private:
	void OnWindowUserActionDelegate(bool bCreate, UAnimSequence* InSequence, const TArray<FString>& InPoseNames);

private:
	TArray<FString> PoseNames;
};

