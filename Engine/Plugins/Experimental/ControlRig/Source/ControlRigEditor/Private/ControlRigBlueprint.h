// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Engine/Blueprint.h"
#include "Misc/Crc.h"
#include "ControlRigDefines.h"
#include "Hierarchy.h"
#include "ControlRigPickerWidget.h"
#include "Interfaces/Interface_PreviewMeshProvider.h"
#include "ControlRigBlueprint.generated.h"

class UControlRigBlueprintGeneratedClass;
class USkeletalMesh;

/** A link between two properties. Links become copies between property data at runtime. */
USTRUCT()
struct FControlRigBlueprintPropertyLink
{
	GENERATED_BODY()

	FControlRigBlueprintPropertyLink() 
		: SourcePropertyHash(0)
		, DestPropertyHash(0)
	{}

	FControlRigBlueprintPropertyLink(const FString& InSourcePropertyPath, const FString& InDestPropertyPath)
		: SourcePropertyPath(InSourcePropertyPath)
		, DestPropertyPath(InDestPropertyPath)
		, SourcePropertyHash(FCrc::StrCrc32<TCHAR>(*SourcePropertyPath))
		, DestPropertyHash(FCrc::StrCrc32<TCHAR>(*DestPropertyPath))
	{}

	friend bool operator==(const FControlRigBlueprintPropertyLink& A, const FControlRigBlueprintPropertyLink& B)
	{
		return A.SourcePropertyHash == B.SourcePropertyHash && A.DestPropertyHash == B.DestPropertyHash;
	}

	const FString& GetSourcePropertyPath() const { return SourcePropertyPath; }
	const FString& GetDestPropertyPath() const { return DestPropertyPath; }

	uint32 GetSourcePropertyHash() const { return SourcePropertyHash; }
	uint32 GetDestPropertyHash() const { return DestPropertyHash; }

private:
	/** Path to the property we are linking from */
	UPROPERTY(VisibleAnywhere, Category="Links")
	FString SourcePropertyPath;

	/** Path to the property we are linking to */
	UPROPERTY(VisibleAnywhere, Category="Links")
	FString DestPropertyPath;

	// Hashed strings for faster comparisons
	UPROPERTY(VisibleAnywhere, Category="Links")
	uint32 SourcePropertyHash;

	UPROPERTY(VisibleAnywhere, Category="Links")
	uint32 DestPropertyHash;
};

UCLASS(BlueprintType)
class UControlRigBlueprint : public UBlueprint, public IInterface_PreviewMeshProvider
{
	GENERATED_BODY()

public:
	UControlRigBlueprint();

	/** Get the (full) generated class for this control rig blueprint */
	UControlRigBlueprintGeneratedClass* GetControlRigBlueprintGeneratedClass() const;

	/** Get the (skeleton) generated class for this control rig blueprint */
	UControlRigBlueprintGeneratedClass* GetControlRigBlueprintSkeletonClass() const;

#if WITH_EDITOR
	// UBlueprint interface
	virtual UClass* GetBlueprintClass() const override;
	virtual bool SupportedByDefaultBlueprintFactory() const override { return false; }
	virtual bool IsValidForBytecodeOnlyRecompile() const override { return false; }
	virtual void LoadModulesRequiredForCompilation() override;
	virtual void GetTypeActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void GetInstanceActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;	
#endif	// #if WITH_EDITOR

	/** Make a property link between the specified properties - used by the compiler */
	void MakePropertyLink(const FString& InSourcePropertyPath, const FString& InDestPropertyPath);

	/** Get the picker widget class for this rig */
	TSubclassOf<UControlRigPickerWidget> GetPickerWidgetClass() const { return PickerWidgetClass; }

	/** IInterface_PreviewMeshProvider interface */
	virtual void SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty = true) override;
	virtual USkeletalMesh* GetPreviewMesh() const override;

private:
	/** Links between the various properties we have */
	UPROPERTY(EditAnywhere, Category="Links")
	TArray<FControlRigBlueprintPropertyLink> PropertyLinks;

	/** list of operators. Visible for debug purpose for now */
	UPROPERTY(VisibleAnywhere, Category = "Links")
	TArray<FControlRigOperator> Operators;

	// need list of "allow query property" to "source" - whether rig unit or property itself
	// this will allow it to copy data to target
	UPROPERTY(VisibleAnywhere, Category = "Links")
	TMap<FName, FString> AllowSourceAccessProperties;

	UPROPERTY(VisibleAnywhere, Category = "Hierarchy")
	FRigHierarchy Hierarchy;

	/** The picker widget class */
	UPROPERTY(EditAnywhere, Category="Picker")
	TSubclassOf<UControlRigPickerWidget> PickerWidgetClass;

	/** The default skeletal mesh to use when previewing this asset */
	UPROPERTY(DuplicateTransient, AssetRegistrySearchable)
	TSoftObjectPtr<USkeletalMesh> PreviewSkeletalMesh;

	friend class FControlRigBlueprintCompilerContext;
	friend class SRigHierarchy;
	friend class FControlRigEditor;
};
