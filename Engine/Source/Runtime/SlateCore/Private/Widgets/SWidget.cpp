// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SWidget.h"
#include "Types/PaintArgs.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/Children.h"
#include "SlateGlobals.h"
#include "Rendering/DrawElements.h"
#include "Widgets/IToolTip.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "Types/NavigationMetaData.h"
#include "Application/SlateApplicationBase.h"
#include "Styling/CoreStyle.h"
#include "Application/ActiveTimerHandle.h"
#include "Input/HittestGrid.h"
#include "Debugging/SlateDebugging.h"

DECLARE_DWORD_COUNTER_STAT(TEXT("Widgets Created (Per Frame)"), STAT_SlateTotalWidgetsPerFrame, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("SWidget::Paint (Count)"), STAT_SlateNumPaintedWidgets, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("SWidget::Tick (Count)"), STAT_SlateNumTickedWidgets, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("TickWidgets"), STAT_SlateTickWidgets, STATGROUP_Slate);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Widgets"), STAT_SlateTotalWidgets, STATGROUP_SlateMemory);
DECLARE_MEMORY_STAT(TEXT("SWidget Total Allocated Size"), STAT_SlateSWidgetAllocSize, STATGROUP_SlateMemory);



#if SLATE_CULL_WIDGETS

float GCullingSlackFillPercent = 0.25f;
static FAutoConsoleVariableRef CVarCullingSlackFillPercent(TEXT("Slate.CullingSlackFillPercent"), GCullingSlackFillPercent, TEXT("Scales the culling rect by the amount to provide extra slack/wiggle room for widgets that have a true bounds larger than the root child widget in a container."), ECVF_Default);

#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

int32 GShowClipping = 0;
static FAutoConsoleVariableRef CVarSlateShowClipRects(TEXT("Slate.ShowClipping"), GShowClipping, TEXT("Controls whether we should render a clipping zone outline.  Yellow = Axis Scissor Rect Clipping (cheap).  Red = Stencil Clipping (expensive)."), ECVF_Default);

int32 GDebugCulling = 0;
static FAutoConsoleVariableRef CVarSlateDebugCulling(TEXT("Slate.DebugCulling"), GDebugCulling, TEXT("Controls whether we should ignore clip rects, and just use culling."), ECVF_Default);

#endif

#if STATS

struct FScopeCycleCounterSWidget : public FCycleCounter
{
	/**
	 * Constructor, starts timing
	 */
	FORCEINLINE_STATS FScopeCycleCounterSWidget(const SWidget* Widget)
	{
		if (Widget)
		{
			TStatId WidgetStatId = Widget->GetStatID();
			if (FThreadStats::IsCollectingData(WidgetStatId))
			{
				Start(WidgetStatId);
			}
		}
	}

	/**
	 * Updates the stat with the time spent
	 */
	FORCEINLINE_STATS ~FScopeCycleCounterSWidget()
	{
		Stop();
	}
};

#else

struct FScopeCycleCounterSWidget
{
	FScopeCycleCounterSWidget(const SWidget* Widget)
	{
	}
	~FScopeCycleCounterSWidget()
	{;
	}
};

#endif

DEFINE_STAT(STAT_SlateVeryVerboseStatGroupTester);

void SWidget::CreateStatID() const
{
#if STATS
	StatID = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_SlateVeryVerbose>( ToString() );
#endif
}

FName NAME_MouseButtonDown(TEXT("MouseButtonDown"));
FName NAME_MouseButtonUp(TEXT("MouseButtonUp"));
FName NAME_MouseMove(TEXT("MouseMove"));
FName NAME_MouseDoubleClick(TEXT("MouseDoubleClick"));

SWidget::SWidget()
	: bIsHovered(false)
	, bCanSupportFocus(true)
	, bCanHaveChildren(true)
	, bClippingProxy(false)
	, bToolTipForceFieldEnabled(false)
	, bForceVolatile(false)
	, bCachedVolatile(false)
	, bInheritedVolatility(false)
	, bNeedsPrepass(true)
	, bNeedsDesiredSize(true)
	, bUpdatingDesiredSize(false)
	, Clipping(EWidgetClipping::Inherit)
	, UpdateFlags(EWidgetUpdateFlags::NeedsTick)
	, DesiredSize()
	, PrepassLayoutScaleMultiplier(1.0f)
	, CullingBoundsExtension()
	, EnabledState(true)
	, Visibility(EVisibility::Visible)
	, RenderOpacity(1.0f)
	, RenderTransform()
	, RenderTransformPivot(FVector2D::ZeroVector)
	, Cursor( TOptional<EMouseCursor::Type>() )
	, ToolTip()
	, LayoutCache(nullptr)	
{
	if (GIsRunning)
	{
		INC_DWORD_STAT(STAT_SlateTotalWidgets);
		INC_DWORD_STAT(STAT_SlateTotalWidgetsPerFrame);
	}
}

SWidget::~SWidget()
{
	// Unregister all ActiveTimers so they aren't left stranded in the Application's list.
	if ( FSlateApplicationBase::IsInitialized() )
	{
		for ( const auto& ActiveTimerHandle : ActiveTimers )
		{
			FSlateApplicationBase::Get().UnRegisterActiveTimer(ActiveTimerHandle);
		}
	}

	DEC_DWORD_STAT(STAT_SlateTotalWidgets);
	DEC_MEMORY_STAT_BY(STAT_SlateSWidgetAllocSize, AllocSize);
}

void SWidget::Construct(
	const TAttribute<FText>& InToolTipText,
	const TSharedPtr<IToolTip>& InToolTip,
	const TAttribute< TOptional<EMouseCursor::Type> >& InCursor,
	const TAttribute<bool>& InEnabledState,
	const TAttribute<EVisibility>& InVisibility,
	const float InRenderOpacity,
	const TAttribute<TOptional<FSlateRenderTransform>>& InTransform,
	const TAttribute<FVector2D>& InTransformPivot,
	const FName& InTag,
	const bool InForceVolatile,
	const EWidgetClipping InClipping,
	const TArray<TSharedRef<ISlateMetaData>>& InMetaData
)
{
	if ( InToolTip.IsValid() )
	{
		// If someone specified a fancy widget tooltip, use it.
		ToolTip = InToolTip;
	}
	else if ( InToolTipText.IsSet() )
	{
		// If someone specified a text binding, make a tooltip out of it
		ToolTip = FSlateApplicationBase::Get().MakeToolTip(InToolTipText);
	}
	else if( !ToolTip.IsValid() || (ToolTip.IsValid() && ToolTip->IsEmpty()) )
	{	
		// We don't have a tooltip.
		ToolTip.Reset();
	}

	Cursor = InCursor;
	EnabledState = InEnabledState;
	Visibility = InVisibility;
	RenderOpacity = InRenderOpacity;
	RenderTransform = InTransform;
	RenderTransformPivot = InTransformPivot;
	Tag = InTag;
	bForceVolatile = InForceVolatile;
	Clipping = InClipping;
	MetaData = InMetaData;
}

void SWidget::SWidgetConstruct(const TAttribute<FText>& InToolTipText, const TSharedPtr<IToolTip>& InToolTip, const TAttribute< TOptional<EMouseCursor::Type> >& InCursor, const TAttribute<bool>& InEnabledState, const TAttribute<EVisibility>& InVisibility, const float InRenderOpacity, const TAttribute<TOptional<FSlateRenderTransform>>& InTransform, const TAttribute<FVector2D>& InTransformPivot, const FName& InTag, const bool InForceVolatile, const EWidgetClipping InClipping, const TArray<TSharedRef<ISlateMetaData>>& InMetaData)
{
	Construct(InToolTipText, InToolTip, InCursor, InEnabledState, InVisibility, InRenderOpacity, InTransform, InTransformPivot, InTag, InForceVolatile, InClipping, InMetaData);
}

FReply SWidget::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	return FReply::Unhandled();
}

void SWidget::OnFocusLost(const FFocusEvent& InFocusEvent)
{
}

void SWidget::OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath)
{
}

void SWidget::OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OnFocusChanging(PreviousFocusPath, NewWidgetPath);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FReply SWidget::OnKeyChar( const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnPreviewKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if (bCanSupportFocus && SupportsKeyboardFocus())
	{
		EUINavigation Direction = FSlateApplicationBase::Get().GetNavigationDirectionFromKey(InKeyEvent);
		// It's the left stick return a navigation request of the correct direction
		if (Direction != EUINavigation::Invalid)
		{
			const ENavigationGenesis Genesis = InKeyEvent.GetKey().IsGamepadKey() ? ENavigationGenesis::Controller : ENavigationGenesis::Keyboard;
			return FReply::Handled().SetNavigation(Direction, Genesis);
		}
	}
	return FReply::Unhandled();
}

FReply SWidget::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnAnalogValueChanged( const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent )
{
	if (bCanSupportFocus && SupportsKeyboardFocus())
	{
		EUINavigation Direction = FSlateApplicationBase::Get().GetNavigationDirectionFromAnalog(InAnalogInputEvent);
		// It's the left stick return a navigation request of the correct direction
		if (Direction != EUINavigation::Invalid)
		{
			return FReply::Handled().SetNavigation(Direction, ENavigationGenesis::Controller);
		}
	}
	return FReply::Unhandled();
}

FReply SWidget::OnPreviewMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( FPointerEventHandler* Event = PointerEvents.Find(NAME_MouseButtonDown) )
	{
		if ( Event->IsBound() )
		{
			return Event->Execute(MyGeometry, MouseEvent);
		}
	}
	return FReply::Unhandled();
}

FReply SWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( FPointerEventHandler* Event = PointerEvents.Find(NAME_MouseButtonUp) )
	{
		if ( Event->IsBound() )
		{
			return Event->Execute(MyGeometry, MouseEvent);
		}
	}
	return FReply::Unhandled();
}

FReply SWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( FPointerEventHandler* Event = PointerEvents.Find(NAME_MouseMove) )
	{
		if ( Event->IsBound() )
		{
			return Event->Execute(MyGeometry, MouseEvent);
		}
	}
	return FReply::Unhandled();
}

FReply SWidget::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( FPointerEventHandler* Event = PointerEvents.Find(NAME_MouseDoubleClick) )
	{
		if ( Event->IsBound() )
		{
			return Event->Execute(MyGeometry, MouseEvent);
		}
	}
	return FReply::Unhandled();
}

void SWidget::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bIsHovered = true;

	if (MouseEnterHandler.IsBound())
	{
		// A valid handler is assigned; let it handle the event.
		MouseEnterHandler.Execute(MyGeometry, MouseEvent);
	}
}

void SWidget::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	bIsHovered = false;

	if (MouseLeaveHandler.IsBound())
	{
		// A valid handler is assigned; let it handle the event.
		MouseLeaveHandler.Execute(MouseEvent);
	}
}

FReply SWidget::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Unhandled();
}

FCursorReply SWidget::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	TOptional<EMouseCursor::Type> TheCursor = Cursor.Get();
	return ( TheCursor.IsSet() )
		? FCursorReply::Cursor( TheCursor.GetValue() )
		: FCursorReply::Unhandled();
}

TOptional<TSharedRef<SWidget>> SWidget::OnMapCursor(const FCursorReply& CursorReply) const
{
	return TOptional<TSharedRef<SWidget>>();
}

bool SWidget::OnVisualizeTooltip( const TSharedPtr<SWidget>& TooltipContent )
{
	return false;
}

TSharedPtr<FPopupLayer> SWidget::OnVisualizePopup(const TSharedRef<SWidget>& PopupContent)
{
	return TSharedPtr<FPopupLayer>();
}

FReply SWidget::OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Unhandled();
}

void SWidget::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
}

void SWidget::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
}

FReply SWidget::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnTouchGesture( const FGeometry& MyGeometry, const FPointerEvent& GestureEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnTouchMoved( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnTouchForceChanged(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return FReply::Unhandled();
}

FReply SWidget::OnTouchFirstMove(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return FReply::Unhandled();
}

FReply SWidget::OnMotionDetected( const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent )
{
	return FReply::Unhandled();
}

TOptional<bool> SWidget::OnQueryShowFocus(const EFocusCause InFocusCause) const
{
	return TOptional<bool>();
}

FPopupMethodReply SWidget::OnQueryPopupMethod() const
{
	return FPopupMethodReply::Unhandled();
}

TSharedPtr<struct FVirtualPointerPosition> SWidget::TranslateMouseCoordinateFor3DChild(const TSharedRef<SWidget>& ChildWidget, const FGeometry& MyGeometry, const FVector2D& ScreenSpaceMouseCoordinate, const FVector2D& LastScreenSpaceMouseCoordinate) const
{
	return nullptr;
}

void SWidget::OnFinishedPointerInput()
{

}

void SWidget::OnFinishedKeyInput()
{

}

FNavigationReply SWidget::OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent)
{
	EUINavigation Type = InNavigationEvent.GetNavigationType();
	TSharedPtr<FNavigationMetaData> NavigationMetaData = GetMetaData<FNavigationMetaData>();
	if (NavigationMetaData.IsValid())
	{
		TSharedPtr<SWidget> Widget = NavigationMetaData->GetFocusRecipient(Type).Pin();
		return FNavigationReply(NavigationMetaData->GetBoundaryRule(Type), Widget, NavigationMetaData->GetFocusDelegate(Type));
	}
	return FNavigationReply::Escape();
}

EWindowZone::Type SWidget::GetWindowZoneOverride() const
{
	// No special behavior.  Override this in derived widgets, if needed.
	return EWindowZone::Unspecified;
}

void SWidget::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
}

void SWidget::SlatePrepass()
{
	if (!GSlateLayoutCaching)
	{
		SlatePrepass(FSlateApplicationBase::Get().GetApplicationScale());
	}
}

void SWidget::SlatePrepass(float InLayoutScaleMultiplier)
{
#if SLATE_VERBOSE_NAMED_EVENTS
	SCOPED_NAMED_EVENT(SWidget_Prepass, FColor::Silver);
#endif

	if(GSlateLayoutCaching)
	{
		if (!bNeedsPrepass && PrepassLayoutScaleMultiplier == InLayoutScaleMultiplier)
		{
			return;
		}

		PrepassLayoutScaleMultiplier = InLayoutScaleMultiplier;
		bNeedsPrepass = false;
	}

	if ( bCanHaveChildren )
	{
		// Cache child desired sizes first. This widget's desired size is
		// a function of its children's sizes.
		FChildren* MyChildren = this->GetChildren();
		const int32 NumChildren = MyChildren->Num();
		for ( int32 ChildIndex=0; ChildIndex < NumChildren; ++ChildIndex )
		{
			const TSharedRef<SWidget>& Child = MyChildren->GetChildAt(ChildIndex);

			if ( GSlateLayoutCaching || Child->Visibility.Get() != EVisibility::Collapsed )
			{
				const float ChildLayoutScaleMultiplier = GetRelativeLayoutScale(MyChildren->GetSlotAt(ChildIndex), InLayoutScaleMultiplier);
				// Recur: Descend down the widget tree.
				Child->SlatePrepass(InLayoutScaleMultiplier * ChildLayoutScaleMultiplier);
			}
		}
	}

	if(!GSlateLayoutCaching)
	{
		// Cache this widget's desired size.
		CacheDesiredSize(InLayoutScaleMultiplier);
	}
}

void SWidget::InvalidatePrepass()
{
	SCOPED_NAMED_EVENT(SWidget_InvalidatePrepass, FColor::Orange);

	bNeedsPrepass = true;
	LayoutChanged(EInvalidateWidget::LayoutAndVolatility);
}

FVector2D SWidget::GetDesiredSize() const
{
	if(GSlateLayoutCaching)
	{
		if (bNeedsDesiredSize && ensureMsgf(!bUpdatingDesiredSize, TEXT("The layout is cyclically dependent.  A child widget can not ask the desired size of a parent while the parent is asking the desired size of its children.")))
		{
			bUpdatingDesiredSize = true;

			// Cache this widget's desired size.
			const_cast<SWidget*>(this)->CacheDesiredSize(PrepassLayoutScaleMultiplier);

			bUpdatingDesiredSize = false;
		}

		return DesiredSize.GetValue();
	}
	else
	{
		return DesiredSize.Get(FVector2D::ZeroVector);
	}
}

#if SLATE_PARENT_POINTERS

void SWidget::AssignParentWidget(TSharedPtr<SWidget> InParent)
{
#if !UE_BUILD_SHIPPING
	ensureMsgf(InParent != SNullWidget::NullWidget, TEXT("The Null Widget can't be anyone's parent."));
	ensureMsgf(this != &SNullWidget::NullWidget.Get(), TEXT("The Null Widget can't have a parent, because a single instance is shared everywhere."));
	ensureMsgf(InParent.IsValid(), TEXT("Are you trying to detatch the parent of a widget?  Use ConditionallyDetatchParentWidget()."));
#endif

	ParentWidgetPtr = InParent;
	if (InParent.IsValid())
	{
		InParent->Invalidate(EInvalidateWidget::Layout);
	}
}

bool SWidget::ConditionallyDetatchParentWidget(SWidget* InExpectedParent)
{
#if !UE_BUILD_SHIPPING
	ensureMsgf(this != &SNullWidget::NullWidget.Get(), TEXT("The Null Widget can't have a parent, because a single instance is shared everywhere."));
#endif

	TSharedPtr<SWidget> Parent = ParentWidgetPtr.Pin();
	if (Parent.Get() == InExpectedParent)
	{
		ParentWidgetPtr.Reset();

		if (Parent.IsValid())
		{
			Parent->Invalidate(EInvalidateWidget::Layout);
		}

		return true;
	}

	return false;
}

#endif

void SWidget::LayoutChanged(EInvalidateWidget InvalidateReason)
{
	if(EnumHasAnyFlags(InvalidateReason, EInvalidateWidget::Layout))
	{
		bNeedsDesiredSize = true;

#if SLATE_PARENT_POINTERS
		TSharedPtr<SWidget> ParentWidget = ParentWidgetPtr.Pin();
		if (ParentWidget.IsValid())
		{
			ParentWidget->ChildLayoutChanged(InvalidateReason);
		}
#endif
	}
}

void SWidget::ChildLayoutChanged(EInvalidateWidget InvalidateReason)
{
	if (!bNeedsDesiredSize || EnumHasAllFlags(InvalidateReason, EInvalidateWidget::Visibility) )
	{
		LayoutChanged(InvalidateReason);
	}
}

void SWidget::CacheDesiredSize(float InLayoutScaleMultiplier)
{
#if SLATE_VERBOSE_NAMED_EVENTS
	SCOPED_NAMED_EVENT(SWidget_CacheDesiredSize, FColor::Red);
#endif
	// Cache this widget's desired size.
	Advanced_SetDesiredSize(ComputeDesiredSize(InLayoutScaleMultiplier));
}

void SWidget::CachePrepass(const TWeakPtr<ILayoutCache>& InLayoutCache)
{
	if ( bCanHaveChildren )
	{
		FChildren* MyChildren = this->GetChildren();
		const int32 NumChildren = MyChildren->Num();
		for ( int32 ChildIndex=0; ChildIndex < NumChildren; ++ChildIndex )
		{
			const TSharedRef<SWidget>& Child = MyChildren->GetChildAt(ChildIndex);
			if ( Child->GetVisibility().IsVisible() == false )
			{
				Child->LayoutCache = InLayoutCache;
			}
			else
			{
				Child->CachePrepass(InLayoutCache);
			}
		}
	}
}

bool SWidget::SupportsKeyboardFocus() const
{
	return false;
}

bool SWidget::HasKeyboardFocus() const
{
	return (FSlateApplicationBase::Get().GetKeyboardFocusedWidget().Get() == this);
}

TOptional<EFocusCause> SWidget::HasUserFocus(int32 UserIndex) const
{
	return FSlateApplicationBase::Get().HasUserFocus(SharedThis(this), UserIndex);
}

TOptional<EFocusCause> SWidget::HasAnyUserFocus() const
{
	return FSlateApplicationBase::Get().HasAnyUserFocus(SharedThis(this));
}

bool SWidget::HasUserFocusedDescendants(int32 UserIndex) const
{
	return FSlateApplicationBase::Get().HasUserFocusedDescendants(SharedThis(this), UserIndex);
}

bool SWidget::HasFocusedDescendants() const
{
	return FSlateApplicationBase::Get().HasFocusedDescendants(SharedThis(this));
}

bool SWidget::HasAnyUserFocusOrFocusedDescendants() const
{
	return HasAnyUserFocus().IsSet() || HasFocusedDescendants();
}

const FSlateBrush* SWidget::GetFocusBrush() const
{
	return FCoreStyle::Get().GetBrush("FocusRectangle");
}

bool SWidget::HasMouseCapture() const
{
	return FSlateApplicationBase::Get().DoesWidgetHaveMouseCapture(SharedThis(this));
}

bool SWidget::HasMouseCaptureByUser(int32 UserIndex, TOptional<int32> PointerIndex) const
{
	return FSlateApplicationBase::Get().DoesWidgetHaveMouseCaptureByUser(SharedThis(this), UserIndex, PointerIndex);
}

void SWidget::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
}

bool SWidget::FindChildGeometries( const FGeometry& MyGeometry, const TSet< TSharedRef<SWidget> >& WidgetsToFind, TMap<TSharedRef<SWidget>, FArrangedWidget>& OutResult ) const
{
	FindChildGeometries_Helper(MyGeometry, WidgetsToFind, OutResult);
	return OutResult.Num() == WidgetsToFind.Num();
}


void SWidget::FindChildGeometries_Helper( const FGeometry& MyGeometry, const TSet< TSharedRef<SWidget> >& WidgetsToFind, TMap<TSharedRef<SWidget>, FArrangedWidget>& OutResult ) const
{
	// Perform a breadth first search!

	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	this->ArrangeChildren(MyGeometry, ArrangedChildren);
	const int32 NumChildren = ArrangedChildren.Num();

	// See if we found any of the widgets on this level.
	for(int32 ChildIndex=0; ChildIndex < NumChildren; ++ChildIndex )
	{
		const FArrangedWidget& CurChild = ArrangedChildren[ ChildIndex ];
		
		if ( WidgetsToFind.Contains(CurChild.Widget) )
		{
			// We found one of the widgets for which we need geometry!
			OutResult.Add( CurChild.Widget, CurChild );
		}
	}

	// If we have not found all the widgets that we were looking for, descend.
	if ( OutResult.Num() != WidgetsToFind.Num() )
	{
		// Look for widgets among the children.
		for( int32 ChildIndex=0; ChildIndex < NumChildren; ++ChildIndex )
		{
			const FArrangedWidget& CurChild = ArrangedChildren[ ChildIndex ];
			CurChild.Widget->FindChildGeometries_Helper( CurChild.Geometry, WidgetsToFind, OutResult );
		}	
	}	
}

FGeometry SWidget::FindChildGeometry( const FGeometry& MyGeometry, TSharedRef<SWidget> WidgetToFind ) const
{
	// We just need to find the one WidgetToFind among our descendants.
	TSet< TSharedRef<SWidget> > WidgetsToFind;
	{
		WidgetsToFind.Add( WidgetToFind );
	}
	TMap<TSharedRef<SWidget>, FArrangedWidget> Result;

	FindChildGeometries( MyGeometry, WidgetsToFind, Result );

	return Result.FindChecked( WidgetToFind ).Geometry;
}

int32 SWidget::FindChildUnderMouse( const FArrangedChildren& Children, const FPointerEvent& MouseEvent )
{
	const FVector2D& AbsoluteCursorLocation = MouseEvent.GetScreenSpacePosition();
	return SWidget::FindChildUnderPosition( Children, AbsoluteCursorLocation );
}

int32 SWidget::FindChildUnderPosition( const FArrangedChildren& Children, const FVector2D& ArrangedSpacePosition )
{
	const int32 NumChildren = Children.Num();
	for( int32 ChildIndex=NumChildren-1; ChildIndex >= 0; --ChildIndex )
	{
		const FArrangedWidget& Candidate = Children[ChildIndex];
		const bool bCandidateUnderCursor = 
			// Candidate is physically under the cursor
			Candidate.Geometry.IsUnderLocation( ArrangedSpacePosition );

		if (bCandidateUnderCursor)
		{
			return ChildIndex;
		}
	}

	return INDEX_NONE;
}

FString SWidget::ToString() const
{
	return FString::Printf(TEXT("%s [%s]"), *this->TypeOfWidget.ToString(), *this->GetReadableLocation() );
}

FString SWidget::GetTypeAsString() const
{
	return this->TypeOfWidget.ToString();
}

FName SWidget::GetType() const
{
	return TypeOfWidget;
}

FString SWidget::GetReadableLocation() const
{
#if !UE_BUILD_SHIPPING
	return FString::Printf(TEXT("%s(%d)"), *FPaths::GetCleanFilename(this->CreatedInLocation.GetPlainNameString()), this->CreatedInLocation.GetNumber());
#else
	return FString();
#endif
}

FName SWidget::GetCreatedInLocation() const
{
#if !UE_BUILD_SHIPPING
	return CreatedInLocation;
#else
	return NAME_None;
#endif
}

FName SWidget::GetTag() const
{
	return Tag;
}

FSlateColor SWidget::GetForegroundColor() const
{
	static FSlateColor NoColor = FSlateColor::UseForeground();
	return NoColor;
}

void SWidget::SetToolTipText(const TAttribute<FText>& ToolTipText)
{
	ToolTip = FSlateApplicationBase::Get().MakeToolTip(ToolTipText);
}

void SWidget::SetToolTipText( const FText& ToolTipText )
{
	ToolTip = FSlateApplicationBase::Get().MakeToolTip(ToolTipText);
}

void SWidget::SetToolTip( const TSharedPtr<IToolTip> & InToolTip )
{
	ToolTip = InToolTip;
}

TSharedPtr<IToolTip> SWidget::GetToolTip()
{
	return ToolTip;
}

void SWidget::OnToolTipClosing()
{
}

void SWidget::EnableToolTipForceField( const bool bEnableForceField )
{
	bToolTipForceFieldEnabled = bEnableForceField;
}

bool SWidget::IsDirectlyHovered() const
{
	return FSlateApplicationBase::Get().IsWidgetDirectlyHovered(SharedThis(this));
}

void SWidget::Invalidate(EInvalidateWidget InvalidateReason)
{
	SCOPED_NAMED_EVENT_TEXT("SWidget::Invalidate", FColor::Orange);

	const bool bWasVolatile = IsVolatileIndirectly() || IsVolatile();
	const bool bVolatilityChanged = EnumHasAnyFlags(InvalidateReason, EInvalidateWidget::Volatility) ? Advanced_InvalidateVolatility() : false;

	if (bWasVolatile == false || bVolatilityChanged)
	{
		Advanced_ForceInvalidateLayout();
	}

	LayoutChanged(InvalidateReason);

}

void SWidget::SetCursor( const TAttribute< TOptional<EMouseCursor::Type> >& InCursor )
{
	Cursor = InCursor;
}

void SWidget::SetDebugInfo( const ANSICHAR* InType, const ANSICHAR* InFile, int32 OnLine, size_t InAllocSize )
{
	TypeOfWidget = InType;

	STAT(AllocSize = InAllocSize);
	INC_MEMORY_STAT_BY(STAT_SlateSWidgetAllocSize, AllocSize);

#if !UE_BUILD_SHIPPING
	CreatedInLocation = FName( InFile );
	CreatedInLocation.SetNumber(OnLine);
#endif
}

void SWidget::OnClippingChanged()
{

}

FSlateRect SWidget::CalculateCullingAndClippingRules(const FGeometry& AllottedGeometry, const FSlateRect& IncomingCullingRect, bool& bClipToBounds, bool& bAlwaysClip, bool& bIntersectClipBounds) const
{
	bClipToBounds = false;
	bIntersectClipBounds = true;
	bAlwaysClip = false;

	if (!bClippingProxy)
	{
		switch (Clipping)
		{
		case EWidgetClipping::ClipToBounds:
			bClipToBounds = true;
			break;
		case EWidgetClipping::ClipToBoundsAlways:
			bClipToBounds = true;
			bAlwaysClip = true;
			break;
		case EWidgetClipping::ClipToBoundsWithoutIntersecting:
			bClipToBounds = true;
			bIntersectClipBounds = false;
			break;
		case EWidgetClipping::OnDemand:
			const float OverflowEpsilon = 1.0f;
			const FVector2D& CurrentSize = GetDesiredSize();
			const FVector2D& LocalSize = AllottedGeometry.GetLocalSize();
			bClipToBounds =
				(CurrentSize.X - OverflowEpsilon) > LocalSize.X ||
				(CurrentSize.Y - OverflowEpsilon) > LocalSize.Y;
			break;
		}
	}

	if (bClipToBounds)
	{
		FSlateRect MyCullingRect(AllottedGeometry.GetRenderBoundingRect(CullingBoundsExtension));

		if (bIntersectClipBounds)
		{
			bool bClipBoundsOverlapping;
			return IncomingCullingRect.IntersectionWith(MyCullingRect, bClipBoundsOverlapping);
		}
		
		return MyCullingRect;
	}

	return IncomingCullingRect;
}

int32 SWidget::Paint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
#if WITH_VERY_VERBOSE_SLATE_STATS
	FScopeCycleCounterSWidget WidgetScope(this);
#endif

	INC_DWORD_STAT(STAT_SlateNumPaintedWidgets);

	// TODO, Maybe we should just make Paint non-const and keep OnPaint const.
	SWidget* MutableThis = const_cast<SWidget*>(this);

	if (GSlateLayoutCaching)
	{
		MutableThis->SlatePrepass(AllottedGeometry.Scale);
	}

	// Save the current layout cache we're associated with (if any)
	LayoutCache = Args.GetLayoutCache();

	// Record if we're part of a volatility pass, this is critical for ensuring we don't report a child
	// of a volatile widget as non-volatile, causing the invalidation panel to do work that's not required.
	//
	// Note: We only do this if we're not also caching.  The retainer panel takes advantage of the fact that
	// it can both send down it's caching & it's a volatile pass, implying everyone should render, everyone
	// is getting cached.  So we don't want volatile widgets to wait to be drawn later, they won't get another
	// chance.
	bInheritedVolatility = Args.IsVolatilityPass() && !Args.IsCaching();

	// If this widget clips to its bounds, then generate a new clipping rect representing the intersection of the bounding
	// rectangle of the widget's geometry, and the current clipping rectangle.
	bool bClipToBounds, bAlwaysClip, bIntersectClipBounds;
	FSlateRect CullingBounds = CalculateCullingAndClippingRules(AllottedGeometry, MyCullingRect, bClipToBounds, bAlwaysClip, bIntersectClipBounds);

	FWidgetStyle ContentWidgetStyle = FWidgetStyle(InWidgetStyle)
		.BlendOpacity(RenderOpacity);

	// If this paint pass is to cache off our geometry, but we're a volatile widget,
	// record this widget as volatile in the draw elements so that we get our own tick/paint 
	// pass later when the layout cache draws.
	if (IsVolatile() && Args.IsCaching() && !Args.IsVolatilityPass())
	{
		const int32 VolatileLayerId = LayerId + 1;
		OutDrawElements.QueueVolatilePainting(
			FSlateWindowElementList::FVolatilePaint(SharedThis(this), Args, AllottedGeometry, CullingBounds, OutDrawElements.GetClippingState(), VolatileLayerId, ContentWidgetStyle, bParentEnabled));

		return VolatileLayerId;
	}

	// Cache the geometry for tick to allow external users to get the last geometry that was used,
	// or would have been used to tick the Widget.
	CachedGeometry = AllottedGeometry;
	CachedGeometry.AppendTransform(FSlateLayoutTransform(Args.GetWindowToDesktopTransform()));

	MutableThis->ExecuteActiveTimers(Args.GetCurrentTime(), Args.GetDeltaTime());

	if (HasAnyUpdateFlags(EWidgetUpdateFlags::NeedsTick))
	{
		INC_DWORD_STAT(STAT_SlateNumTickedWidgets);

		SCOPE_CYCLE_COUNTER(STAT_SlateTickWidgets);
		MutableThis->Tick(CachedGeometry, Args.GetCurrentTime(), Args.GetDeltaTime());
	}

	// Record hit test geometry, but only if we're not caching.
	const FPaintArgs UpdatedArgs = Args.RecordHittestGeometry(this, AllottedGeometry, LayerId);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GDebugCulling)
	{
		// When we're debugging culling, don't actually clip, we'll just pretend to, so we can see the effects of
		// any widget doing culling to know if it's doing the right thing.
		bClipToBounds = false;
	}
#endif

	if ( bClipToBounds )
	{
		FSlateClippingZone ClippingZone(AllottedGeometry);
		ClippingZone.SetShouldIntersectParent(bIntersectClipBounds);
		ClippingZone.SetAlwaysClip(bAlwaysClip);
		OutDrawElements.PushClip(ClippingZone);

		// The hit test grid records things in desktop space, so we use the tick geometry instead of the paint geometry.
		FSlateClippingZone DesktopClippingZone(CachedGeometry);
		DesktopClippingZone.SetShouldIntersectParent(bIntersectClipBounds);
		DesktopClippingZone.SetAlwaysClip(bAlwaysClip);
		Args.GetGrid().PushClip(DesktopClippingZone);
	}

#if WITH_SLATE_DEBUGGING
	FSlateDebugging::BeginWidgetPaint.Broadcast(this, UpdatedArgs, AllottedGeometry, CullingBounds, OutDrawElements, LayerId);
#endif

	// Paint the geometry of this widget.
	int32 NewLayerID = OnPaint(UpdatedArgs, AllottedGeometry, CullingBounds, OutDrawElements, LayerId, ContentWidgetStyle, bParentEnabled);

#if WITH_SLATE_DEBUGGING
	FSlateDebugging::EndWidgetPaint.Broadcast(this, OutDrawElements, NewLayerID);
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GShowClipping && bClipToBounds)
	{
		FSlateClippingZone ClippingZone(AllottedGeometry);

		TArray<FVector2D> Points;
		Points.Add(ClippingZone.TopLeft);
		Points.Add(ClippingZone.TopRight);
		Points.Add(ClippingZone.BottomRight);
		Points.Add(ClippingZone.BottomLeft);
		Points.Add(ClippingZone.TopLeft);

		const bool bAntiAlias = true;
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			NewLayerID,
			FPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			ClippingZone.IsAxisAligned() ? FLinearColor::Yellow : FLinearColor::Red,
			bAntiAlias,
			2.0f);
	}
#endif

	if ( bClipToBounds )
	{
		OutDrawElements.PopClip();
		Args.GetGrid().PopClip();
	}

#if PLATFORM_UI_NEEDS_FOCUS_OUTLINES
	// Check if we need to show the keyboard focus ring, this is only necessary if the widget could be focused.
	if ( bCanSupportFocus && SupportsKeyboardFocus() )
	{
		bool bShowUserFocus = FSlateApplicationBase::Get().ShowUserFocus(SharedThis(this));
		if (bShowUserFocus)
		{
			const FSlateBrush* BrushResource = GetFocusBrush();

			if (BrushResource != nullptr)
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					NewLayerID,
					AllottedGeometry.ToPaintGeometry(),
					BrushResource,
					ESlateDrawEffect::None,
					BrushResource->GetTint(InWidgetStyle)
				);
			}
		}
	}
#endif

	if ( OutDrawElements.ShouldResolveDeferred() )
	{
		NewLayerID = OutDrawElements.PaintDeferred(NewLayerID, MyCullingRect);
	}

	return NewLayerID;
}

float SWidget::GetRelativeLayoutScale(const FSlotBase& Child, float LayoutScaleMultiplier) const
{
	return 1.0f;
}

void SWidget::ArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
#if SLATE_VERBOSE_NAMED_EVENTS
	SCOPED_NAMED_EVENT(SWidget_ArrangeChildren, FColor::Black);
#endif
	OnArrangeChildren(AllottedGeometry, ArrangedChildren);
}

TSharedRef<FActiveTimerHandle> SWidget::RegisterActiveTimer(float TickPeriod, FWidgetActiveTimerDelegate TickFunction)
{
	TSharedRef<FActiveTimerHandle> ActiveTimerHandle = MakeShareable(new FActiveTimerHandle(TickPeriod, TickFunction, FSlateApplicationBase::Get().GetCurrentTime() + TickPeriod));
	FSlateApplicationBase::Get().RegisterActiveTimer(ActiveTimerHandle);
	ActiveTimers.Add(ActiveTimerHandle);

	AddUpdateFlags(EWidgetUpdateFlags::NeedsActiveTimerUpdate);

	return ActiveTimerHandle;
}

void SWidget::UnRegisterActiveTimer(const TSharedRef<FActiveTimerHandle>& ActiveTimerHandle)
{
	if (FSlateApplicationBase::IsInitialized())
	{
		FSlateApplicationBase::Get().UnRegisterActiveTimer(ActiveTimerHandle);
		ActiveTimers.Remove(ActiveTimerHandle);

		if (ActiveTimers.Num() == 0)
		{
			RemoveUpdateFlags(EWidgetUpdateFlags::NeedsActiveTimerUpdate);
		}
	}
}

void SWidget::ExecuteActiveTimers(double CurrentTime, float DeltaTime)
{
	// loop over the registered tick handles and execute them, removing them if necessary.
	for (int32 i = 0; i < ActiveTimers.Num();)
	{
		EActiveTimerReturnType Result = ActiveTimers[i]->ExecuteIfPending(CurrentTime, DeltaTime);
		if (Result == EActiveTimerReturnType::Continue)
		{
			++i;
		}
		else
		{
			// Possible that execution unregistered the timer 
			if (ActiveTimers.IsValidIndex(i))
			{
				if (FSlateApplicationBase::IsInitialized())
				{
					FSlateApplicationBase::Get().UnRegisterActiveTimer(ActiveTimers[i]);
				}
				ActiveTimers.RemoveAt(i);
			}
		}
	}

	if (ActiveTimers.Num() == 0)
	{
		RemoveUpdateFlags(EWidgetUpdateFlags::NeedsActiveTimerUpdate);
	}
}

void SWidget::SetOnMouseButtonDown(FPointerEventHandler EventHandler)
{
	PointerEvents.Add(NAME_MouseButtonDown, EventHandler);
}

void SWidget::SetOnMouseButtonUp(FPointerEventHandler EventHandler)
{
	PointerEvents.Add(NAME_MouseButtonUp, EventHandler);
}

void SWidget::SetOnMouseMove(FPointerEventHandler EventHandler)
{
	PointerEvents.Add(NAME_MouseMove, EventHandler);
}

void SWidget::SetOnMouseDoubleClick(FPointerEventHandler EventHandler)
{
	PointerEvents.Add(NAME_MouseDoubleClick, EventHandler);
}

void SWidget::SetOnMouseEnter(FNoReplyPointerEventHandler EventHandler)
{
	MouseEnterHandler = EventHandler;
}

void SWidget::SetOnMouseLeave(FSimpleNoReplyPointerEventHandler EventHandler)
{
	MouseLeaveHandler = EventHandler;
}

#if SLATE_CULL_WIDGETS

bool SWidget::IsChildWidgetCulled(const FSlateRect& MyCullingRect, const FArrangedWidget& ArrangedChild) const
{
	// We add some slack fill to the culling rect to deal with the common occurrence
	// of widgets being larger than their root level widget is.  Happens when nested child widgets
	// inflate their rendering bounds to render beyond their parent (the child of this panel doing the culling), 
	// or using render transforms.  In either case, it introduces offsets to a bounding volume we don't 
	// actually know about or track in slate, so we have have two choices.
	//    1) Don't cull, set SLATE_CULL_WIDGETS to 0.
	//    2) Cull with a slack fill amount users can adjust.
	const FSlateRect CullingRectWithSlack = MyCullingRect.ScaleBy(GCullingSlackFillPercent);

	// 1) We check if the rendered bounding box overlaps with the culling rect.  Which is so that
	//    a render transformed element is never culled if it would have been visible to the user.
	if (FSlateRect::DoRectanglesIntersect(CullingRectWithSlack, ArrangedChild.Geometry.GetRenderBoundingRect()))
	{
		return false;
	}

	// 2) We also check the layout bounding box to see if it overlaps with the culling rect.  The
	//    reason for this is a bit more nuanced.  Suppose you dock a widget on the screen on the side
	//    and you want have it animate in and out of the screen.  Even though the layout transform 
	//    keeps the widget on the screen, the render transform alone would have caused it to be culled
	//    and therefore not ticked or painted.  The best way around this for now seems to be to simply
	//    check both rects to see if either one is overlapping the culling volume.
	if (FSlateRect::DoRectanglesIntersect(CullingRectWithSlack, ArrangedChild.Geometry.GetLayoutBoundingRect()))
	{
		return false;
	}

	// There's a special condition if the widget's clipping state is set does not intersect with clipping bounds, they in effect
	// will be setting a new culling rect, so let them pass being culling from this step.
	if (ArrangedChild.Widget->GetClipping() == EWidgetClipping::ClipToBoundsWithoutIntersecting)
	{
		return false;
	}

	return true;
}

#endif
