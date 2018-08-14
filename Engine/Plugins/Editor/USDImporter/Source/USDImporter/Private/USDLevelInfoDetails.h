// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"

class AUSDLevelInfo;
class IDetailLayoutBuilder;

class FUSDLevelInfoDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	
	FReply OnSaveUSD();

private:
	/** The selected sky light */
	TWeakObjectPtr<AUSDLevelInfo> USDLevelInfo;
};
