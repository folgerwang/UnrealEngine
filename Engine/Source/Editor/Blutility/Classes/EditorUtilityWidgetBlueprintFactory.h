// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Factory for utility widget blueprints
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Factories/Factory.h"
#include "Engine/Blueprint.h"
#include "EditorUtilityWidgetBlueprintFactory.generated.h"


class SWindow;

UCLASS(HideCategories = Object, MinimalAPI)
class UEditorUtilityWidgetBlueprintFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// The type of blueprint that will be created
	UPROPERTY(EditAnywhere, Category = WidgetBlueprintFactory)
	TEnumAsByte<EBlueprintType> BlueprintType;

	// The parent class of the created blueprint
	UPROPERTY(EditAnywhere, Category = WidgetBlueprintFactory, meta = (AllowAbstract = ""))
	TSubclassOf<class UUserWidget> ParentClass;

	// UFactory interface
	virtual bool ConfigureProperties() override;
	virtual bool ShouldShowInNewMenu() const override { return true; }
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool CanCreateNew() const override;
	
	// End of UFactory interface

};
