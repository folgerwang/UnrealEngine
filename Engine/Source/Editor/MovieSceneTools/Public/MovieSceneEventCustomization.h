// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "IStructureDetailsView.h"

struct FAssetData;

class FMenuBuilder;
class IPropertyHandle;
class FDetailWidgetRow;
class UMovieSceneSection;
class UMovieSceneSequence;
class UMovieSceneEventTrack;
class UK2Node_FunctionEntry;
class IDetailChildrenBuilder;

/**
 * Customization for FMovieSceneEvent structs.
 * Will deduce the event's section either from the outer objects on the details customization, or use the one provided on construction (for instanced property type customizations)
 */
class FMovieSceneEventCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	static TSharedRef<IPropertyTypeCustomization> MakeInstance(UMovieSceneSection* InSection);

	virtual void CustomizeHeader( TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils ) override;

private:

	/**
	 * Clear the endpoint for the event(s) represented by this property handle.
	 * @note: Does not delete the endpoint in the blueprint itself.
	 */
	void ClearEventEndpoint();


	/**
	 * Creates a single new endpoint for the event(s) represented by this property handle.
	 */
	void CreateEventEndpoint();


	/**
	 * Creates a single new endpoint for the event(s) represented by this property handle using the specified function as a quick binding.
	 */
	void CreateEventEndpointFromFunction(UFunction* QuickBindFunction, UClass* PinClassType);


	/**
	 * Assigns the specified function entry to the event(s) represented by this property handle.
	 */
	void SetEventEndpoint(UK2Node_FunctionEntry* NewEndpoint);

	/**
	 * Compare the currently assigned endpoint with the specified entry. Used as the check-state for the radio buttons on the menu.
	 */
	bool CompareCurrentEventEndpoint(UK2Node_FunctionEntry* NewEndpoint);


	/**
	 * Navigate to the definition of the endpoint specified by the event(s) represented by this property handle.
	 */
	void NavigateToDefinition();


	/**
	 * Generate the content of the main combo button menu dropdown
	 */
	TSharedRef<SWidget> GetMenuContent();


	/**
	 * Generate the content of the creation shortcut sub-menu dropdown
	 */
	void PopulateQuickBindSubMenu(FMenuBuilder& MenuBuilder, UClass* TemplateClass);

private:

	/**
	 * Get the sequence that is common to all the events represented by this property handle, or nullptr if they are not all the same.
	 */
	UMovieSceneSequence* GetCommonSequence() const;


	/**
	 * Get the track that is common to all the events represented by this property handle, or nullptr if they are not all the same.
	 */
	UMovieSceneEventTrack* GetCommonTrack() const;


	/**
	 * Get the endpoint that is common to all the events represented by this property handle, or nullptr if they are not all the same.
	 */
	UK2Node_FunctionEntry* GetCommonEndpoint() const;


	/**
	 * Get all the objects that the events reside within.
	 */
	void GetEditObjects(TArray<UObject*>& OutObjects) const;

private:

	/** Get the name of the event to display on the main combo button */
	FText GetEventName() const;
	/** Get the icon of the event to display on the main combo button */
	const FSlateBrush* GetEventIcon() const;

	/** Get the visibility of the error icon */
	EVisibility GetErrorVisibility() const;
	/** Get the tooltip text for the error icon */
	FText GetErrorTooltip() const;

	UClass* FindObjectBindingClass();

private:

	/** Externally supplied section that the event(s) we're reflecting reside within */
	TWeakObjectPtr<UMovieSceneSection> WeakExternalSection;

	/** A cache of the common endpoint that is only used when the menu is open to avoid re-computing it every frame. */
	TWeakObjectPtr<UK2Node_FunctionEntry> CachedCommonEndpoint;

	/** The property handle we're reflecting */
	TSharedPtr<IPropertyHandle> PropertyHandle;
};
