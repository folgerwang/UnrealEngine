// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Units/RigUnit.h"
#include "RigUnitEditor_Base.generated.h"

class UControlRig;

/**
  * This is the base class for any Rig unit editor features. 
  * This class can be derived to your RigUnit if you want editor functionality for your rig unit in the editor module
  * You'll have to register the proper class for your RigUnit in your start up module. 
  * This class exists in ControlRig as ControlRig needs reference to it right now
  * @todo: we might be able to move this back to editor module if we create custom BP node that can create property class 
  * without declaring the type of the class. Right now ControlRig has to know about URigUnitEditor_Base to create/cache
  */
UCLASS(BlueprintType)
class CONTROLRIGEDITOR_API URigUnitEditor_Base : public UObject
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY(transient)
	UControlRig* ControlRig;

	FRigUnit* SourceRigUnit;
public:

	virtual void SetSourceReference(UControlRig* InControlRig, FRigUnit* InRigUnit)
	{
		ControlRig = InControlRig;
		SourceRigUnit = InRigUnit;
	}

	/* 
 	 * You can provide the interface function here that can interact with interface by default
	 * For now we only support these decoration

	 * UFUNCTION(BlueprintCallable, Category = "ControlRig | IKFK", meta = (NotBlueprintThreadSafe))
	 * void Snap();
	 * 
	 * @Note no parameter is supported yet
	 **/
	 
	virtual FString GetDisplayName() const
	{
		return SourceRigUnit->RigUnitName.ToString() + TEXT(" RigUnit");
	}

	virtual FString GetActionToolTip(const FName& ActionName) const
	{
		return SourceRigUnit->RigUnitName.ToString() + TEXT(" : ") + ActionName.ToString();
	}

	/** Function that lets you update source data. Triggered by Invoke Action  */
	bool UpdateSourceProperties(const FString& PropertyName) const;

	bool HasValidReference() const
	{
		return (ControlRig && SourceRigUnit);
	}
};

