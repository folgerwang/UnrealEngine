// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SEditorUserWidgetHost : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SEditorUserWidgetHost) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UWorld* InWorld);

	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

private:
	/** The world we create widgets with */
	TWeakObjectPtr<UWorld> World;
};