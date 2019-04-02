// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
class ITargetPlatform;

enum class ECheckBoxState : uint8;

/* FShaderFormatsPropertyDetails
 *****************************************************************************/

/**
 * Helper which implements details panel customizations for a device profiles parent property
 */
class SHAREDSETTINGSWIDGETS_API FShaderFormatsPropertyDetails
: public TSharedFromThis<FShaderFormatsPropertyDetails>
{
	
public:
	typedef FText (*GetFriendlyNameFromRHINameFnc)(const FString&);
	static FText GetFriendlyNameFromRHINameMac(const FString& InRHIName);
	
	/**
	 * Constructor for the parent property details view
	 *
	 * @param InDetailsBuilder - Where we are adding our property view to
	 * @param InProperty - The category name to override
	 * @param InTitle - Title for display
	 */
	FShaderFormatsPropertyDetails(IDetailLayoutBuilder* InDetailBuilder, FString InProperty = TEXT("TargetedRHIs"), FString InTitle = TEXT("Targeted RHIs"));
	
	/** Simple delegate for updating shader version warning */
	void SetOnUpdateShaderWarning(FSimpleDelegate const& Delegate);
	
	/**
	 * Create the UI to select which windows shader formats we are targetting
	 */
	void CreateTargetShaderFormatsPropertyView(ITargetPlatform* TargetPlatform, GetFriendlyNameFromRHINameFnc FriendlyNameFnc);
	
	
	/**
	 * @param InRHIName - The input RHI to check
	 * @returns Whether this RHI is currently enabled
	 */
	ECheckBoxState IsTargetedRHIChecked(FName InRHIName) const;
	
private:
	
	// Supported/Targeted RHI check boxes
	void OnTargetedRHIChanged(ECheckBoxState InNewValue, FName InRHIName);
	
private:
	
	/** A handle to the detail view builder */
	IDetailLayoutBuilder* DetailBuilder;
	
	/** Access to the Parent Property */
	TSharedPtr<IPropertyHandle> ShaderFormatsPropertyHandle;

	/** The category name to override */
	FString Property;

	/** Title for display */
	FString Title;

	/** Preserve shader format order when writing to property */
	TMap<FName, int> ShaderFormatOrder;
};