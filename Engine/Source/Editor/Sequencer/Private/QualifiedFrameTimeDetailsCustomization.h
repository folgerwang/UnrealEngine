// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class IDetailLayoutBuilder;
class ISequencer;

/**
 *  Customize the qualified frame time to support conversion from seconds/frames/timecode formats.
 */
class FQualifiedFrameTimeDetailsCustomization : public IPropertyTypeCustomization
{
public:
	FQualifiedFrameTimeDetailsCustomization(TWeakPtr<ISequencer> InSequencer)
	{
		Sequencer = InSequencer;
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override {};
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	FText OnGetTimeText() const;
	void OnTimeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);

	TWeakPtr<ISequencer> Sequencer;
};
