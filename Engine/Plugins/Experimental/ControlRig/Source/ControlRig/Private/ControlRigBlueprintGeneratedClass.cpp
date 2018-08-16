// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintGeneratedClass.h"
#include "Units/RigUnit_Control.h"

UControlRigBlueprintGeneratedClass::UControlRigBlueprintGeneratedClass()
{
}

void UControlRigBlueprintGeneratedClass::Link(FArchive& Ar, bool bRelinkExistingProperties)
{
	Super::Link(Ar, bRelinkExistingProperties);

#if WITH_EDITORONLY_DATA
	ControlUnitProperties.Empty();
	RigUnitProperties.Empty();

	for (TFieldIterator<UProperty> It(this); It; ++It)
	{
		if (UStructProperty* StructProp = Cast<UStructProperty>(*It))
		{
			if (StructProp->Struct->IsChildOf(FRigUnit::StaticStruct()))
			{
				RigUnitProperties.Add(StructProp);

				if (StructProp->Struct->IsChildOf(FRigUnit_Control::StaticStruct()))
				{
					ControlUnitProperties.Add(StructProp);
				}
			}
		}
	}
#endif
}

void UControlRigBlueprintGeneratedClass::PurgeClass(bool bRecompilingOnLoad)
{
	Super::PurgeClass(bRecompilingOnLoad);
}
