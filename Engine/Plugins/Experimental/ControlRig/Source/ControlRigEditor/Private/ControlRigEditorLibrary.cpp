// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ControlRigEditorLibrary.h"
#include "Blueprint/BlueprintSupport.h"
#include "ControlRig.h"
#include "Units/RigUnitEditor_Base.h"
#include "ControlRigEditorModule.h"
#include "ControlRigBlueprintGeneratedClass.h"

#define LOCTEXT_NAMESPACE "UControlRigEditorLibrary"

//////////////////////////////////////////////////////////////////////////
// UControlRigEditorLibrary

const FName ControlRigEditorLibraryWarning = FName("ControlRig Editor Library");

UControlRigEditorLibrary::UControlRigEditorLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration(
			ControlRigEditorLibraryWarning,
			LOCTEXT("ControlRigEditorLibraryWarning", "ControlRig Library Warning")
		)
	);
}

URigUnitEditor_Base* UControlRigEditorLibrary::GetEditorObject( UControlRig* ControlRig, const FName& RigUnitName )
{
	FRigUnit* OwnerRigUnit = nullptr;
	FName ClassName = NAME_None;

	if (ControlRig)
	{
		// find the class from property name
		UControlRigBlueprintGeneratedClass* Class = Cast<UControlRigBlueprintGeneratedClass>(ControlRig->GetClass());
		for (UStructProperty* UnitProperty : Class->RigUnitProperties)
		{
			if (UnitProperty->GetFName() == RigUnitName)
			{
				OwnerRigUnit = UnitProperty->ContainerPtrToValuePtr<FRigUnit>(ControlRig);
				ClassName = UnitProperty->Struct->GetFName();
				break;
			}
		}

		if (OwnerRigUnit)
		{
			// if we find one, just return
			if (UObject** Found = ControlRig->RigUnitEditorObjects.Find(OwnerRigUnit))
			{
				return CastChecked<URigUnitEditor_Base>(*Found);
			}

			// else, we create one
			FControlRigEditorModule& ControlRigEditorModule = FModuleManager::LoadModuleChecked<FControlRigEditorModule>("ControlRigEditor");
			TSubclassOf<URigUnitEditor_Base> EditorClass = ControlRigEditorModule.GetEditorObjectByRigUnit(ClassName);

			// @later: should we skip default class ones?
			if (EditorClass)
			{
				URigUnitEditor_Base* NewEditorObject = NewObject<URigUnitEditor_Base>(ControlRig, EditorClass);
				NewEditorObject->SetSourceReference(ControlRig, OwnerRigUnit);
				ControlRig->RigUnitEditorObjects.Add(OwnerRigUnit, NewEditorObject);
				return NewEditorObject;
			}
		}

	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
