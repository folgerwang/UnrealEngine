// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

#include "AjaMediaFinder.h"
#include "AjaMediaSource.h"
#include "Input/Reply.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/SWidget.h"

/** AJA Media Source detail customization */
class FAjaMediaSourceDetailCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShareable(new FAjaMediaSourceDetailCustomization());  }

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	TSharedRef<SWidget> HandleSourceComboButtonMenuContent();
	void OnSelectionChanged(FAjaMediaConfiguration SelectedItem);
	FReply OnButtonClicked() const;

	TArray<TWeakObjectPtr<UAjaMediaSource>> MediaSources;
	TWeakPtr<SWidget> PermutationSelector;
	FAjaMediaConfiguration SelectedConfiguration;
};
