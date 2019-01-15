// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/ISlateStyle.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"

struct FLevelSequenceActionExtender;

/**
 * Implements actions for ULevelSequence assets.
 */
class FLevelSequenceActions
	: public FAssetTypeActions_Base
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InStyle The style set to use for asset editor toolkits.
	 */
	FLevelSequenceActions(const TSharedRef<ISlateStyle>& InStyle);

	/**
	 * Register a new extender that can add new actions to level sequence assets
	 */
	void RegisterLevelSequenceActionExtender(const TSharedRef<FLevelSequenceActionExtender>& InExtender)
	{
		ActionExtenders.AddUnique(InExtender);
	}

	/**
	 * Unregister a previously registered action extender
	 */
	void UnregisterLevelSequenceActionExtender(const TSharedRef<FLevelSequenceActionExtender>& InExtender)
	{
		ActionExtenders.Remove(InExtender);
	}

public:
	
	// IAssetTypeActions interface

	virtual uint32 GetCategories() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual bool ShouldForceWorldCentric() override;
	virtual bool CanLocalize() const override { return false; }
	virtual bool HasActions( const TArray<UObject*>& InObjects ) const override;
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;

private:

	/** Pointer to the style set to use for toolkits. */
	TSharedRef<ISlateStyle> Style;

	/** Array of registered action extenders. */
	TArray<TSharedRef<FLevelSequenceActionExtender>> ActionExtenders;
};
