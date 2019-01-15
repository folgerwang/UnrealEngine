// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "TakeRecorderSources.h"
#include "TakeRecorderSourceProperty.generated.h"

USTRUCT(BlueprintType)
struct TAKESCORE_API FActorRecordedProperty
{
	GENERATED_BODY()

	FActorRecordedProperty()
		: PropertyName(NAME_None)
		, bEnabled(false)
		, RecorderName()
	{
	}

	FActorRecordedProperty(const FName& InName, const bool bInEnabled, const FText& InRecorderName)
	{
		PropertyName = InName;
		bEnabled = bInEnabled;
		RecorderName = InRecorderName;
	}
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Property")
	FName PropertyName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Property")
	bool bEnabled;

	UPROPERTY(VisibleAnywhere, Category = "Property")
	FText RecorderName;
};

/**
* This represents a list of all possible properties and components on an actor
* which can be recorded by the Actor Recorder and whether or not the user wishes
* to record them. If you wish to expose a property to be recorded it needs to be marked
* as "Interp" (C++) or "Expose to Cinematics" in Blueprints.
*/
UCLASS(BlueprintType)
class TAKESCORE_API UActorRecorderPropertyMap : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, Category = "Property")
	TSoftObjectPtr<UObject> RecordedObject;

	/* Represents properties exposed to Cinematics that can possibly be recorded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	TArray<FActorRecordedProperty> Properties;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, meta=(ShowInnerProperties, EditFixedOrder), Category = "Property")
	TArray<UActorRecorderPropertyMap*> Children;
};
