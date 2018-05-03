// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "UObject/GCObject.h"
#include "Misc/NotifyHook.h"
#include "Widgets/SCompoundWidget.h"
#include "MovieSceneSequenceID.h"
#include "ITimeSlider.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Widgets/Input/SSpinBox.h"
#include "Sequencer.h"

class FActorDragDropGraphEdOp;
class FAssetDragDropOp;
class FClassDragDropOp;
class FMovieSceneClipboard;
class FSequencerTimeSliderController;
class FVirtualTrackArea;
class ISequencerEditTool;
class SSequencerGotoBox;
class SSequencerLabelBrowser;
class SSequencerTrackArea;
class SSequencerTrackOutliner;
class SSequencerTransformBox;
class SSequencerTreeView;
class USequencerSettings;
struct FPaintPlaybackRangeArgs;
struct FSectionHandle;

namespace SequencerLayoutConstants
{
	/** The amount to indent child nodes of the layout tree */
	const float IndentAmount = 10.0f;

	/** Height of each folder node */
	const float FolderNodeHeight = 20.0f;

	/** Height of each object node */
	const float ObjectNodeHeight = 20.0f;

	/** Height of each section area if there are no sections (note: section areas may be larger than this if they have children. This is the height of a section area with no children or all children hidden) */
	const float SectionAreaDefaultHeight = 15.0f;

	/** Height of each key area */
	const float KeyAreaHeight = 15.0f;

	/** Height of each category node */
	const float CategoryNodeHeight = 15.0f;
}


/**
 * The kind of breadcrumbs that sequencer uses
 */
struct FSequencerBreadcrumb
{
	enum Type
	{
		ShotType,
		MovieSceneType,
	};

	/** The type of breadcrumb this is */
	FSequencerBreadcrumb::Type BreadcrumbType;

	/** The movie scene this may point to */
	FMovieSceneSequenceID SequenceID;

	FSequencerBreadcrumb(FMovieSceneSequenceIDRef InSequenceID)
		: BreadcrumbType(FSequencerBreadcrumb::MovieSceneType)
		, SequenceID(InSequenceID)
	{ }

	FSequencerBreadcrumb()
		: BreadcrumbType(FSequencerBreadcrumb::ShotType)
	{ }
};


/**
 * Main sequencer UI widget
 */
class SSequencer
	: public SCompoundWidget
	, public FGCObject
	, public FNotifyHook
{
public:

	DECLARE_DELEGATE_OneParam( FOnToggleBoolOption, bool )
	SLATE_BEGIN_ARGS( SSequencer )
	{ }
		/** The current view range (seconds) */
		SLATE_ATTRIBUTE( FAnimatedRange, ViewRange )

		/** The current clamp range (seconds) */
		SLATE_ATTRIBUTE( FAnimatedRange, ClampRange )

		/** The playback range */
		SLATE_ATTRIBUTE( TRange<FFrameNumber>, PlaybackRange )

		/** The selection range */
		SLATE_ATTRIBUTE( TRange<FFrameNumber>, SelectionRange)

		/** The current sub sequence range */
		SLATE_ATTRIBUTE( TOptional<TRange<FFrameNumber>>, SubSequenceRange)

		/** The playback status */
		SLATE_ATTRIBUTE( EMovieScenePlayerStatus::Type, PlaybackStatus )

		/** Called when the user changes the playback range */
		SLATE_EVENT( FOnFrameRangeChanged, OnPlaybackRangeChanged )

		/** Called when the user has begun dragging the playback range */
		SLATE_EVENT( FSimpleDelegate, OnPlaybackRangeBeginDrag )

		/** Called when the user has finished dragging the playback range */
		SLATE_EVENT( FSimpleDelegate, OnPlaybackRangeEndDrag )

		/** Called when the user changes the selection range */
		SLATE_EVENT( FOnFrameRangeChanged, OnSelectionRangeChanged )

		/** Called when the user has begun dragging the selection range */
		SLATE_EVENT( FSimpleDelegate, OnSelectionRangeBeginDrag )

		/** Called when the user has finished dragging the selection range */
		SLATE_EVENT( FSimpleDelegate, OnSelectionRangeEndDrag )

		/** Whether the playback range is locked */
		SLATE_ATTRIBUTE( bool, IsPlaybackRangeLocked )

		/** Called when the user toggles the play back range lock */
		SLATE_EVENT( FSimpleDelegate, OnTogglePlaybackRangeLocked )

		/** The current scrub position in (seconds) */
		SLATE_ATTRIBUTE( FFrameTime, ScrubPosition )

		/** Called when the user changes the view range */
		SLATE_EVENT( FOnViewRangeChanged, OnViewRangeChanged )

		/** Called when the user changes the clamp range */
		SLATE_EVENT( FOnTimeRangeChanged, OnClampRangeChanged )

		/** Called to get the nearest key */
		SLATE_EVENT( FOnGetNearestKey, OnGetNearestKey )

		/** Called when the user has begun scrubbing */
		SLATE_EVENT( FSimpleDelegate, OnBeginScrubbing )

		/** Called when the user has finished scrubbing */
		SLATE_EVENT( FSimpleDelegate, OnEndScrubbing )

		/** Called when the user changes the scrub position */
		SLATE_EVENT( FOnScrubPositionChanged, OnScrubPositionChanged )

		/** Called to populate the add combo button in the toolbar. */
		SLATE_EVENT( FOnGetAddMenuContent, OnGetAddMenuContent )

		/** Called when object is clicked. */
		SLATE_EVENT(FOnBuildCustomContextMenuForGuid, OnBuildCustomContextMenuForGuid)
			
		/** Called when any widget contained within sequencer has received focus */
		SLATE_EVENT( FSimpleDelegate, OnReceivedFocus )

		/** Extender to use for the add menu. */
		SLATE_ARGUMENT( TSharedPtr<FExtender>, AddMenuExtender )

		/** Extender to use for the toolbar. */
		SLATE_ARGUMENT(TSharedPtr<FExtender>, ToolbarExtender)
	SLATE_END_ARGS()


	void Construct(const FArguments& InArgs, TSharedRef<FSequencer> InSequencer);

	void BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings);
	
	~SSequencer();
	
	virtual void AddReferencedObjects( FReferenceCollector& Collector )
	{
		Collector.AddReferencedObject( Settings );
	}

	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	/** Updates the layout node tree from movie scene data */
	void UpdateLayoutTree();

	/** Causes the widget to register an empty active timer that persists until Sequencer playback stops */
	void RegisterActiveTimerForPlayback();

	/** Updates the breadcrumbs from a change in the shot filter state. */
	void UpdateBreadcrumbs();
	void ResetBreadcrumbs();
	void PopBreadcrumb();

	/** Step to next and previous keyframes */
	void StepToNextKey();
	void StepToPreviousKey();
	void StepToNextCameraKey();
	void StepToPreviousCameraKey();
	void StepToKey(bool bStepToNextKey, bool bCameraOnly);

	/** Called when the save button is clicked */
	void OnSaveMovieSceneClicked();

	/** Called when the save-as button is clicked */
	void OnSaveMovieSceneAsClicked();

	/** Called when the curve editor is shown or hidden */
	void OnCurveEditorVisibilityChanged();

	/** Access the tree view for this sequencer */
	TSharedPtr<SSequencerTreeView> GetTreeView() const;

	/** Generate a helper structure that can be used to transform between phsyical space and virtual space in the track area */
	FVirtualTrackArea GetVirtualTrackArea() const;

	/** Get an array of section handles for the given set of movie scene sections */
	TArray<FSectionHandle> GetSectionHandles(const TSet<TWeakObjectPtr<UMovieSceneSection>>& DesiredSections) const;

	/** @return a numeric type interface that will parse and display numbers as frames and times correctly */
	TSharedRef<INumericTypeInterface<double>> GetNumericTypeInterface() const;
	
	/** Access the currently active track area edit tool */
	const ISequencerEditTool* GetEditTool() const;

	void ShowTickResolutionOverlay();
	void HideTickResolutionOverlay();

	/** Sets the play time for the sequence but clamped by the working range. This is useful for cases where we can't clamp via the UI control. */
	void SetPlayTimeClampedByWorkingRange(double Frame);
public:

	// FNotifyHook overrides

	void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FEditPropertyChain* PropertyThatChanged);

protected:

	// SWidget interface

	// @todo Sequencer Basic drag and drop support. Doesn't belong here most likely.
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual void OnFocusChanging( const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent ) override;

private:
	
	/** Handles key selection changes. */
	void HandleKeySelectionChanged();

	/** Handles selection changes in the label browser. */
	void HandleLabelBrowserSelectionChanged(FString NewLabel, ESelectInfo::Type SelectInfo);

	/** Handles determining the visibility of the label browser. */
	EVisibility HandleLabelBrowserVisibility() const;

	/** Handles section selection changes. */
	void HandleSectionSelectionChanged();

	/** Handles changes to the selected outliner nodes. */
	void HandleOutlinerNodeSelectionChanged();

	/** Syncs the current node selection to the curve editor. */
	void SyncCurveEditorToSelection();

	/** Empty active timer to ensure Slate ticks during Sequencer playback */
	EActiveTimerReturnType EnsureSlateTickDuringPlayback(double InCurrentTime, float InDeltaTime);	

	/** Get context menu contents. */
	void GetContextMenuContent(FMenuBuilder& MenuBuilder);

	/** Makes the toolbar. */
	TSharedRef<SWidget> MakeToolBar();

	/** Makes add button. */
	TSharedRef<SWidget> MakeAddButton();

	/** Makes the add menu for the toolbar. */
	TSharedRef<SWidget> MakeAddMenu();

	/** Makes the general menu for the toolbar. */
	TSharedRef<SWidget> MakeGeneralMenu();

	/** Makes the plabacky menu for the toolbar. */
	TSharedRef<SWidget> MakePlaybackMenu();

	/** Makes the select/edit menu for the toolbar. */
	TSharedRef<SWidget> MakeSelectEditMenu();

	/** Makes the snapping menu for the toolbar. */
	TSharedRef<SWidget> MakeSnapMenu();

	/** Makes the auto-change menu for the toolbar. */
	TSharedRef<SWidget> MakeAutoChangeMenu();

	/** Makes the allow edits menu for the toolbar. */
	TSharedRef<SWidget> MakeAllowEditsMenu();

	/** Makes the key group menu for the toolbar. */
	TSharedRef<SWidget> MakeKeyGroupMenu();

	/** Makes the playback speed menu for the toolbar. */
	void FillPlaybackSpeedMenu(FMenuBuilder& InMenuBuilder);

public:
	/** Makes the time display format menu for the toolbar and the play rate menu. */
	void FillTimeDisplayFormatMenu(FMenuBuilder& MenuBuilder);

public:	

	/** Makes a time range widget with the specified inner content */
	TSharedRef<SWidget> MakeTimeRange(const TSharedRef<SWidget>& InnerContent, bool bShowWorkingRange, bool bShowViewRange, bool bShowPlaybackRange);

	/** Gets the top time sliders widget. */
	TSharedPtr<ITimeSlider> GetTopTimeSliderWidget() const;

private:

	/**
	* Called when the time snap interval changes.
	*
	* @param InInterval	The new time snap interval
	*/
	void OnTimeSnapIntervalChanged(float InInterval);

	/**
	* @return The value of the current value snap interval.
	*/
	float OnGetValueSnapInterval() const;

	/**
	* Called when the value snap interval changes.
	*
	* @param InInterval	The new value snap interval
	*/
	void OnValueSnapIntervalChanged( float InInterval );

	/**
	 * Called when the outliner search terms change                                                              
	 */
	void OnOutlinerSearchChanged( const FText& Filter );

	/**
	 * @return The fill percentage of the animation outliner
	 */
	float GetColumnFillCoefficient(int32 ColumnIndex) const
	{
		return ColumnFillCoefficients[ColumnIndex];
	}

	/** Get the amount of space that the outliner spacer should fill */
	float GetOutlinerSpacerFill() const;

	/** Get the visibility of the curve area */
	EVisibility GetCurveEditorVisibility() const;

	/** Get the visibility of the track area */
	EVisibility GetTrackAreaVisibility() const;

	/**
	 * Called when one or more assets are dropped into the widget
	 *
	 * @param	DragDropOp	Information about the asset(s) that were dropped
	 */
	void OnAssetsDropped(const FAssetDragDropOp& DragDropOp);
	
	/**
	 * Called when one or more classes are dropped into the widget
	 *
	 * @param	DragDropOp	Information about the class(es) that were dropped
	 */
	void OnClassesDropped(const FClassDragDropOp& DragDropOp);
		
	/**
	 * Called when one or more actors are dropped into the widget
	 *
	 * @param	DragDropOp	Information about the actor(s) that was dropped
	 */
	void OnActorsDropped(FActorDragDropGraphEdOp& DragDropOp); 

	/** Called when a breadcrumb is clicked on in the sequencer */
	void OnCrumbClicked(const FSequencerBreadcrumb& Item);

	/** Gets the root movie scene name */
	FText GetRootAnimationName() const;

	/** Gets whether or not the breadcrumb trail should be visible. */
	EVisibility GetBreadcrumbTrailVisibility() const;

	/** Gets whether or not the curve editor toolbar should be visible. */
	EVisibility GetCurveEditorToolBarVisibility() const;

	/** Gets whether or not the bottom time slider should be visible. */
	EVisibility GetBottomTimeSliderVisibility() const;

	/** Gets whether or not the time range should be visible. */
	EVisibility GetTimeRangeVisibility() const;

	/** What is the preferred display format for time values. */
	EFrameNumberDisplayFormats GetTimeDisplayFormat() const;

	/** Called when a column fill percentage is changed by a splitter slot. */
	void OnColumnFillCoefficientChanged(float FillCoefficient, int32 ColumnIndex);

	/** Gets paint options for painting the playback range on sequencer */
	FPaintPlaybackRangeArgs GetSectionPlaybackRangeArgs() const;

	EVisibility GetDebugVisualizerVisibility() const;
	
	void SetPlaybackSpeed(float InPlaybackSpeed);
	float GetPlaybackSpeed() const;

	/** Controls how fast Spinboxes change values. */
	double GetSpinboxDelta() const;

public:
	/** On Paste Command */
	void OnPaste();
	bool CanPaste();

	/** Handle Track Paste */
	void PasteTracks();

	/** Open the paste menu */
	bool OpenPasteMenu();
	
	/** Open the paste from history menu */
	void PasteFromHistory();

	/** Generate a paste menu args structure */
	struct FPasteContextMenuArgs GeneratePasteArgs(FFrameNumber PasteAtTime, TSharedPtr<FMovieSceneClipboard> Clipboard = nullptr);

	/** Execute custom context menu if passed in the FSequencerViewParams  */
	void BuildCustomContextMenuForGuid(FMenuBuilder& MenuBuilder, FGuid ObjectBinding);

	/** This adds the specified path to the selection set to be restored the next time the tree view is refreshed. */
	void AddAdditionalPathToSelectionSet(const FString& Path) { AdditionalSelectionsToAdd.Add(Path); }
private:

	/** Goto box widget. */
	TSharedPtr<SSequencerGotoBox> GotoBox;

	/** Transform box widget. */
	TSharedPtr<SSequencerTransformBox> TransformBox;

	/** Section area widget */
	TSharedPtr<SSequencerTrackArea> TrackArea;

	/** Outliner widget */
	TSharedPtr<SSequencerTrackOutliner> TrackOutliner;

	/** The breadcrumb trail widget for this sequencer */
	TSharedPtr<SBreadcrumbTrail<FSequencerBreadcrumb>> BreadcrumbTrail;

	/** The label browser for filtering tracks. */
	TSharedPtr<SSequencerLabelBrowser> LabelBrowser;

	/** The search box for filtering tracks. */
	TSharedPtr<SSearchBox> SearchBox;

	/** The current playback time display.*/
	TSharedPtr<SSpinBox<double>> PlayTimeDisplay;

	/** The sequencer tree view responsible for the outliner and track areas */
	TSharedPtr<SSequencerTreeView> TreeView;

	/** The main sequencer interface */
	TWeakPtr<FSequencer> SequencerPtr;

	/** The top time slider widget */
	TSharedPtr<ITimeSlider> TopTimeSlider;

	/** Cached settings provided to the sequencer itself on creation */
	USequencerSettings* Settings;

	/** The fill coefficients of each column in the grid. */
	float ColumnFillCoefficients[2];

	/** Whether the active timer is currently registered */
	bool bIsActiveTimerRegistered;

	/** Whether the user is selecting. Ignore selection changes from the level when the user is selecting. */
	bool bUserIsSelecting;

	/** Extender to use for the 'add' menu */
	TSharedPtr<FExtender> AddMenuExtender;

	/** Extender to use for the toolbar */
	TSharedPtr<FExtender> ToolbarExtender;

	/** Numeric type interface used for converting parsing and generating strings from numbers */
	TSharedPtr<INumericTypeInterface<double>> NumericTypeInterface;

	/** Time slider controller for this sequencer */
	TSharedPtr<FSequencerTimeSliderController> TimeSliderController;

	FOnGetAddMenuContent OnGetAddMenuContent;

	/** Called when object is clicked in track list */
	FOnBuildCustomContextMenuForGuid OnBuildCustomContextMenuForGuid;

	/** Called when the user has begun dragging the selection selection range */
	FSimpleDelegate OnSelectionRangeBeginDrag;

	/** Called when the user has finished dragging the selection selection range */
	FSimpleDelegate OnSelectionRangeEndDrag;

	/** Called when the user has begun dragging the playback range */
	FSimpleDelegate OnPlaybackRangeBeginDrag;

	/** Called when the user has finished dragging the playback range */
	FSimpleDelegate OnPlaybackRangeEndDrag;

	/** Called when any widget contained within sequencer has received focus */
	FSimpleDelegate OnReceivedFocus;

	/** Cached clamp and view range for unlinking the curve editor time range */
	TRange<double> CachedClampRange;
	TRange<double> CachedViewRange;

	/**
	 * A list of additional paths to add to the selection set when it is restored after rebuilding the tree.
	 * This can be used to highlight nodes that may not exist until the rebuild. Cleared after the tree is rebuilt
	 * and the selection list is restored.
	*/
	TArray<FString> AdditionalSelectionsToAdd;

	TSharedPtr<SWidget> TickResolutionOverlay;
};
