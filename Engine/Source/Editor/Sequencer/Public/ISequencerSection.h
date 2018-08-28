// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SNullWidget.h"
#include "Layout/Margin.h"
#include "MovieSceneSection.h"

class FMenuBuilder;
class FSequencerSectionPainter;
struct FSlateBrush;

/** Enumerates which edge is being resized */
UENUM()
enum ESequencerSectionResizeMode
{
	SSRM_LeadingEdge,
	SSRM_TrailingEdge
};

namespace SequencerSectionConstants
{
	/** How far the user has to drag the mouse before we consider the action dragging rather than a click */
	const float SectionDragStartDistance = 5.0f;

	/** The size of each key */
	const FVector2D KeySize(12.0f, 12.0f);

	const float DefaultSectionGripSize = 7.0f;

	const float DefaultSectionHeight = 15.f;

	const FName SelectionColorName("SelectionColor");

	const FName SelectionInactiveColorName("SelectionColorInactive");
}

/**
 * Interface that should be implemented for the UI portion of a section
 */
class ISequencerSection
{
public:
	virtual ~ISequencerSection(){}
	/**
	 * The MovieSceneSection data being visualized
	 */
	virtual UMovieSceneSection* GetSectionObject() = 0;

	/**
	 * Called when the section should be painted
	 *
	 * @param Painter		Structure that affords common painting operations
	 * @return				The new LayerId
	 */
	virtual int32 OnPaintSection( FSequencerSectionPainter& InPainter ) const = 0;

	/**
	 * Allows each section to have it's own unique widget for advanced editing functionality
	 * OnPaintSection will still be called if a widget is provided.  OnPaintSection is still used for the background section display
	 * 
	 * @return The generated widget 
	 */
	virtual TSharedRef<SWidget> GenerateSectionWidget() { return SNullWidget::NullWidget; }

	DEPRECATED(4.20, "Please override Sequencer::DrawKeys instead")
	virtual const FSlateBrush* GetKeyBrush(FKeyHandle KeyHandle) const { return nullptr; }

	DEPRECATED(4.20, "Please override Sequencer::DrawKeys instead")
	virtual FVector2D GetKeyBrushOrigin( FKeyHandle KeyHandle ) const { return FVector2D(0.0f, 0.0f); }

	/**
	 * Called when the section is double clicked
	 *
	 * @param SectionGeometry	Geometry of the section
	 * @param MouseEvent		Event causing the double click
	 * @return A reply in response to double clicking the section
	 */
	virtual FReply OnSectionDoubleClicked( const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent ) { return FReply::Unhandled(); }

	/**
	 * Called when the section is double clicked
	 *
	 * @param SectionGeometry	Geometry of the section
	 * @param MouseEvent		Event causing the double click
	 * @param ObjectBinding		The object guid bound to this section
	 * @return A reply in response to double clicking the section
	 */
	virtual FReply OnSectionDoubleClicked( const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent, const FGuid& ObjectBinding) { return FReply::Unhandled(); }

	/**
	 * Called when a key on this section is double clicked
	 *
	 * @param KeyHandle			The key that was clicked
	 * @return A reply in response to double clicking the key
	 */
	virtual FReply OnKeyDoubleClicked( FKeyHandle KeyHandle ) { return FReply::Unhandled(); }

	/**
	 * @return The display name of the section in the section view
	 */
	virtual FText GetSectionTitle() const { return FText(); }

	/**
	 * @return The amount of padding to apply to non-interactive portions of the section interface (such as section text)
	 */
	virtual FMargin GetContentPadding() const { return FMargin(11.f, 6.f); }

	/**
	 * Generates the inner layout for this section
	 *
	 * @param LayoutBuilder	The builder utility for creating section layouts
	 */	
	SEQUENCER_API virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder );

	/**
	 * @return The height of the section
	 */
	virtual float GetSectionHeight() const { return SequencerSectionConstants::DefaultSectionHeight; }
	
	virtual float GetSectionGripSize() const { return SequencerSectionConstants::DefaultSectionGripSize; }

	/**
	 * @return Whether or not the user can resize this section.
	 */
	virtual bool SectionIsResizable() const {return true;}

	/**
	 * Ticks the section during the Slate tick
	 *
	 * @param  AllottedGeometry The space allotted for this widget
	 * @param  ClippedGeometry The space for this widget clipped against the parent widget
	 * @param  InCurrentTime  Current absolute real time
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	virtual void Tick( const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime ) {}

	/**
	 * Builds up the section context menu for the outliner
	 *
	 * @param MenuBuilder	The menu builder to change
	 * @param ObjectBinding The object guid bound to this section
	 */
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding) {}

	/**
	 * Called when the user requests that a category from this section be deleted. 
	 *
	 * @param CategoryNamePath An array of category names which is the path to the category to be deleted.
	 * @returns Whether or not the category was deleted.
	 */
	virtual bool RequestDeleteCategory( const TArray<FName>& CategoryNamePath ) { return false; }

	/**
	 * Called when the user requests that a key area from this section be deleted.
	 *
	 * @param KeyAreaNamePath An array of names representing the path of to the key area to delete, starting with any categories which contain the key area.
	 * @returns Whether or not the key area was deleted.
	 */
	virtual bool RequestDeleteKeyArea( const TArray<FName>& KeyAreaNamePath ) { return false; }

	/**
	 * Resize the section 
	 *
	 * @param ResizeMode Resize either the leading or the trailing edge of the section
	 * @param ResizeTime The time to resize to
	 */
	virtual void BeginResizeSection() {}
	SEQUENCER_API virtual void ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeFrameNumber);

	/**
	 * Slips the section by a specific factor
	 *
	 * @param SlipTime The amount to slip this section by
	 */
	virtual void BeginSlipSection() {}
	virtual void SlipSection(double SlipTime) {}
};

class FSequencerSection : public ISequencerSection
{
public:
	FSequencerSection(UMovieSceneSection& InSection)
		: WeakSection(&InSection)
	{}

	SEQUENCER_API virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;

	virtual UMovieSceneSection* GetSectionObject() override final
	{
		return WeakSection.Get();
	}

protected:
	TWeakObjectPtr<UMovieSceneSection> WeakSection;
};