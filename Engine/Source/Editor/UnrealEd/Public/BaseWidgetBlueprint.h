// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Blueprint.h"
#include "BaseWidgetBlueprint.generated.h"

UCLASS(Abstract)
class UNREALED_API UBaseWidgetBlueprint : public UBlueprint
{
	GENERATED_UCLASS_BODY()
public:
#if WITH_EDITORONLY_DATA
	/** A tree of the widget templates to be created */
	UPROPERTY()
	class UWidgetTree* WidgetTree;
#endif

	/**
	* Returns collection of widgets that represent the 'source' (user edited) widgets for this
	* blueprint - avoids calling virtual functions on instances and is therefore safe to use
	* throughout compilation.
	*/
	TArray<class UWidget*> GetAllSourceWidgets();
	TArray<const class UWidget*> GetAllSourceWidgets() const;

	/** Identical to GetAllSourceWidgets, but as an algorithm */
	void ForEachSourceWidget(TFunctionRef<void(class UWidget*)> Fn);
	void ForEachSourceWidget(TFunctionRef<void(class UWidget*)> Fn) const;

private:
	void ForEachSourceWidgetImpl(TFunctionRef<void(class UWidget*)> Fn) const;
};
