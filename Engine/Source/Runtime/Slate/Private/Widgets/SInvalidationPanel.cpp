// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SInvalidationPanel.h"
#include "Rendering/DrawElements.h"
#include "Misc/App.h"
#include "Application/SlateApplicationBase.h"
#include "Styling/CoreStyle.h"
#include "Layout/WidgetPath.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetCaching.h"
#include "Types/ReflectionMetadata.h"
#include "Rendering/SlateObjectReferenceCollector.h"

//DECLARE_CYCLE_STAT(TEXT("Invalidation Time"), STAT_InvalidationTime, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Cached Elements"), STAT_SlateNumCachedElements, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Invalidated Elements"), STAT_SlateNumInvalidatedElements, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Volatile Widgets"), STAT_SlateNumVolatileWidgets, STATGROUP_Slate);

DECLARE_CYCLE_STAT(TEXT("SInvalidationPanel::Tick"), STAT_SlateInvalidationTick, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("SInvalidationPanel::Paint"), STAT_SlateInvalidationPaint, STATGROUP_Slate);

DEFINE_LOG_CATEGORY_STATIC(LogSlateInvalidationPanel, Log, All);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

/** True if we should allow widgets to be cached in the UI at all. */
TAutoConsoleVariable<int32> InvalidationDebugging(
	TEXT("Slate.InvalidationDebugging"),
	false,
	TEXT("Whether to show invalidation debugging visualization"));

bool SInvalidationPanel::IsInvalidationDebuggingEnabled()
{
	return InvalidationDebugging.GetValueOnGameThread() == 1;
}

void SInvalidationPanel::EnableInvalidationDebugging(bool bEnable)
{
	InvalidationDebugging.AsVariable()->Set(bEnable);
}

/** True if we should allow widgets to be cached in the UI at all. */
TAutoConsoleVariable<int32> EnableWidgetCaching(
	TEXT("Slate.EnableWidgetCaching"),
	true,
	TEXT("Whether to attempt to cache any widgets through invalidation panels."));

bool SInvalidationPanel::GetEnableWidgetCaching()
{
	return EnableWidgetCaching.GetValueOnGameThread() == 1;
}

void SInvalidationPanel::SetEnableWidgetCaching(bool bEnable)
{
	EnableWidgetCaching.AsVariable()->Set(bEnable);
}

TAutoConsoleVariable<int32> AlwaysInvalidate(
	TEXT("Slate.AlwaysInvalidate"),
	false,
	TEXT("Forces invalidation panels to cache, but to always invalidate."));

#endif

static int32 CacheRenderData = 1;
static FAutoConsoleVariableRef CVarCacheRenderData(
	TEXT("Slate.CacheRenderData"),
	CacheRenderData,
	TEXT("Invalidation panels will cache render data, otherwise cache only widget draw elements."));

static bool ShouldCacheRenderData()
{
	return WITH_ENGINE && CacheRenderData != 0;
}

SInvalidationPanel::SInvalidationPanel()
	: EmptyChildSlot(this)
{
}

void SInvalidationPanel::Construct( const FArguments& InArgs )
{
	FSlateApplicationBase::Get().OnGlobalInvalidate().AddSP( this, &SInvalidationPanel::OnGlobalInvalidate );

	ChildSlot
	[
		InArgs._Content.Widget
	];

	bNeedsCaching = true;
	bNeedsCachePrepass = true;
	bIsInvalidating = false;
	bCanCache = true;
	RootCacheNode = nullptr;
	LastUsedCachedNodeIndex = 0;
	LastHitTestIndex = 0;
	MaximumLayerIdCachedAt = 0;
	LastClippingIntersectionSize = FVector2D::ZeroVector;

	bCacheRelativeTransforms = InArgs._CacheRelativeTransforms;

	bCacheRenderData = ShouldCacheRenderData();

#if SLATE_VERBOSE_NAMED_EVENTS
	DebugName = InArgs._DebugName;
	DebugTickName = InArgs._DebugName + TEXT("_Tick");
	DebugPaintName = InArgs._DebugName + TEXT("_Paint");
#endif
}

SInvalidationPanel::~SInvalidationPanel()
{
	for ( int32 i = 0; i < NodePool.Num(); i++ )
	{
		delete NodePool[i];
	}

	if ( FSlateApplication::IsInitialized() )
	{
		FSlateApplication::Get().ReleaseResourcesForLayoutCache(this);
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
bool SInvalidationPanel::GetCanCache() const
{
	return bCanCache && EnableWidgetCaching.GetValueOnGameThread() == 1;
}
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
bool SInvalidationPanel::IsCachingNeeded() const
{
	return bNeedsCaching || AlwaysInvalidate.GetValueOnGameThread() == 1;
}
#endif

bool SInvalidationPanel::IsCachingNeeded(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, int32 LayerId) const
{
	// We only need to re-cache if the incoming layer is higher than the maximum layer Id we cached at,
	// we do this so that widgets that appear and live behind your invalidated UI don't constantly invalidate
	// everything above it.
	if (LayerId > MaximumLayerIdCachedAt)
	{
		return true;
	}
	
	if (LastClippingIndex != OutDrawElements.GetClippingIndex())
	{
		return true;
	}

	int32 ClippingStateCount = OutDrawElements.GetClippingManager().GetClippingStates().Num();
	if (LastClippingStateOffset != ClippingStateCount)
	{
		return true;
	}
	
	if (bCacheRelativeTransforms)
	{
		bool bOverlapping;
		FVector2D IntersectionSize = AllottedGeometry.GetLayoutBoundingRect().IntersectionWith(MyCullingRect, bOverlapping).GetSize();
		if (!LastClippingIntersectionSize.Equals(IntersectionSize, 1.0f))
		{
			return true;
		}

		// If the container we're in has changed in either scale or the rotation matrix has changed, 
		if ( AllottedGeometry.GetAccumulatedLayoutTransform().GetScale() != LastAllottedGeometry.GetAccumulatedLayoutTransform().GetScale() ||
			 AllottedGeometry.GetAccumulatedRenderTransform().GetMatrix() != LastAllottedGeometry.GetAccumulatedRenderTransform().GetMatrix() )
		{
			return true;
		}
	}
	else
	{
		// If the container we're in has changed in any way we need to invalidate for sure.
		if ( AllottedGeometry.GetAccumulatedLayoutTransform() != LastAllottedGeometry.GetAccumulatedLayoutTransform() ||
			AllottedGeometry.GetAccumulatedRenderTransform() != LastAllottedGeometry.GetAccumulatedRenderTransform() )
		{
			return true;
		}
	}

	if ( AllottedGeometry.GetLocalSize() != LastAllottedGeometry.GetLocalSize() )
	{
		return true;
	}

	// If our clip rect changes size, we've definitely got to invalidate.
	const FVector2D ClipRectSize = MyCullingRect.GetSize().RoundToVector();
	if ( ClipRectSize != LastClipRectSize )
	{
		return true;
	}

	TOptional<FSlateClippingState> ClippingState = OutDrawElements.GetClippingState();
	if (LastClippingState != ClippingState)
	{
		return true;
	}
	
	return false;
}

void SInvalidationPanel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(CachedResources);

#if SLATE_VERBOSE_NAMED_EVENTS
	UE_LOG(LogSlateInvalidationPanel, Verbose, TEXT("SInvalidationPanel(%s): %d References"), *DebugName, CachedResources.Num());
#endif
}

void SInvalidationPanel::SetCanCache(bool InCanCache)
{
	bCanCache = InCanCache;

	InvalidateCache();
}

void SInvalidationPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
#if SLATE_VERBOSE_NAMED_EVENTS
	SCOPED_NAMED_EVENT_FSTRING(DebugTickName, FColor::Blue);
#endif

	SCOPE_CYCLE_COUNTER(STAT_SlateInvalidationTick);

	if ( GetCanCache() )
	{
		//SCOPE_CYCLE_COUNTER(STAT_InvalidationTime);

		bool bShouldCacheRenderData = ShouldCacheRenderData();
		if (bCacheRenderData != bShouldCacheRenderData)
		{
			bCacheRenderData = bShouldCacheRenderData;
			InvalidateCache();
		}
		
		if (bNeedsCachePrepass)
		{
			CachePrepass(SharedThis(this));
			bNeedsCachePrepass = false;
		}
	}
}

FChildren* SInvalidationPanel::GetChildren()
{
	if ( GetCanCache() == false || IsCachingNeeded() )
	{
		return SCompoundWidget::GetChildren();
	}
	else
	{
		return &EmptyChildSlot;
	}
}

void SInvalidationPanel::InvalidateWidget(SWidget* InvalidateWidget)
{
	bNeedsCaching = true;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ( InvalidateWidget != nullptr && IsInvalidationDebuggingEnabled() )
	{
		InvalidatorWidgets.Add(InvalidateWidget->AsShared(), 1);
	}
#endif
}

FCachedWidgetNode* SInvalidationPanel::CreateCacheNode() const
{
	// If the node pool is empty, allocate a few
	if ( LastUsedCachedNodeIndex >= NodePool.Num() )
	{
		for ( int32 i = 0; i < 10; i++ )
		{
			NodePool.Add(new FCachedWidgetNode());
		}
	}

	// Return one of the preallocated nodes and increment the next node index.
	FCachedWidgetNode* NewNode = NodePool[LastUsedCachedNodeIndex];
	++LastUsedCachedNodeIndex;

	return NewNode;
}

void SInvalidationPanel::OnGlobalInvalidate()
{
	InvalidateCache();
}

template<typename ElementType>
TFunction<void(ElementType&)> UpdateClipIndexFn(int32 LastClippingIndex, int32 ClippingStateOffset)
{
	return [=](ElementType& Element) {
		if (Element.GetClippingIndex() == -1)
		{
			Element.SetClippingIndex(LastClippingIndex);
		}
		else
		{
			Element.SetClippingIndex(ClippingStateOffset + Element.GetClippingIndex());
		}
	};
}

int32 SInvalidationPanel::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
#if SLATE_VERBOSE_NAMED_EVENTS
	SCOPED_NAMED_EVENT_FSTRING(DebugPaintName, FColor::Purple);
#endif
	SCOPE_CYCLE_COUNTER(STAT_SlateInvalidationPaint);

	if ( GetCanCache() )
	{
		const bool bWasCachingNeeded = IsCachingNeeded() || IsCachingNeeded(OutDrawElements, AllottedGeometry, MyCullingRect, LayerId);

		if ( bWasCachingNeeded )
		{
			{
#if SLATE_VERBOSE_NAMED_EVENTS
				SCOPED_NAMED_EVENT_F(TEXT("%s_Invalidation"), FColor::Red, *DebugPaintName);
#endif
				SInvalidationPanel* MutableThis = const_cast<SInvalidationPanel*>(this);
				TSharedRef<SInvalidationPanel> SharedMutableThis = SharedThis(MutableThis);

				// Always set the caching flag to false first, during the paint / tick pass we may change something
				// to volatile and need to re-cache.
				bNeedsCaching = false;

				bNeedsCachePrepass = true;

				// Mark that we're in the process of invalidating.
				bIsInvalidating = true;

				// Record a new maximum layer Id, throw in 10 so we have a bit of padding before we need to recache.
				MaximumLayerIdCachedAt = LayerId + 10;

				SWindow* Window = OutDrawElements.GetPaintWindow();
				CachedWindowElements = FSlateApplication::Get().GetCachableElementList(StaticCastSharedRef<SWindow>(Window->AsShared()), this);

				// Reset the render data handle in case it was in use, and we're not overriding it this frame.
				CachedRenderData.Reset();

				// Reset the cached node pool index so that we effectively reset the pool.
				LastUsedCachedNodeIndex = 0;

				RootCacheNode = CreateCacheNode();
				RootCacheNode->Initialize(Args, SharedMutableThis, AllottedGeometry);

				// TODO We may be double pre-passing here, if the invalidation happened at the end of last frame,
				// we'll have already done one pre-pass before getting here.
				ChildSlot.GetWidget()->SlatePrepass(AllottedGeometry.Scale);

				bool bPushedNewClip = false;
				const int32 CurrentClippingIndex = OutDrawElements.GetClippingManager().GetClippingIndex();

				if(CurrentClippingIndex != INDEX_NONE)
				{
					bPushedNewClip = true;
					FSlateClippingState CurrentClippingState = OutDrawElements.GetClippingManager().GetClippingStates()[CurrentClippingIndex];
					CachedWindowElements->GetClippingManager().PushClippingState(CurrentClippingState);
				}

				//TODO: When SWidget::Paint is called don't drag self if volatile, and we're doing a cache pass.
				CachedMaxChildLayer = SCompoundWidget::OnPaint(
					Args.EnableCaching(SharedMutableThis, RootCacheNode, true, false),
					AllottedGeometry,
					MyCullingRect,
					*CachedWindowElements.Get(),
					MaximumLayerIdCachedAt,
					InWidgetStyle,
					bParentEnabled);

				if (bPushedNewClip)
				{
					CachedWindowElements->GetClippingManager().PopClip();
				}

				{
					CachedResources.Reset();
					FSlateObjectReferenceCollector Collector(CachedResources);
					CachedWindowElements->AddReferencedObjects(Collector);
				}

				if (bCacheRelativeTransforms)
				{
					CachedAbsolutePosition = AllottedGeometry.GetAccumulatedRenderTransform().GetTranslation();
				}

				LastClippingStateOffset = OutDrawElements.GetClippingManager().GetClippingStates().Num();
				LastClippingIndex = OutDrawElements.GetClippingIndex();
				LastClippingState = OutDrawElements.GetClippingState();
				
				const int32 ClippingStateOffset = OutDrawElements.GetClippingManager().MergeClippingStates(CachedWindowElements->GetClippingManager().GetClippingStates());

				if (bCacheRenderData)
				{
					CachedRenderData = CachedWindowElements->CacheRenderData(this);
				}
				else
				{
					// offset clipping indices for cached elements, so they point to correct entry in merged clipping states
					CachedWindowElements->ForEachElement(UpdateClipIndexFn<FSlateDrawBase>(LastClippingIndex, ClippingStateOffset));
					CachedWindowElements->ForEachElement(UpdateClipIndexFn<FSlateDrawElement>(LastClippingIndex, ClippingStateOffset));
				}

				LastHitTestIndex = Args.GetLastHitTestIndex();

				LastAllottedGeometry = AllottedGeometry;
				LastClipRectSize = MyCullingRect.GetSize().RoundToVector();

				if (bCacheRelativeTransforms)
				{
					LastClippingIntersectionSize = AllottedGeometry.GetLayoutBoundingRect().IntersectionWith(MyCullingRect).GetSize();
				}

				bIsInvalidating = false;
			}
		}
		else
		{
#if SLATE_VERBOSE_NAMED_EVENTS
			SCOPED_NAMED_EVENT_TEXT("SInvalidationPanel::MergeClippingStates", FColor::Magenta);
#endif
			OutDrawElements.GetClippingManager().MergeClippingStates(CachedWindowElements->GetClippingManager().GetClippingStates());
		}

		FVector2D AbsoluteDeltaPosition = FVector2D::ZeroVector;
		if ( bCacheRelativeTransforms )
		{
			AbsoluteDeltaPosition = AllottedGeometry.GetAccumulatedRenderTransform().GetTranslation() - CachedAbsolutePosition;
		}

		// Record Hit Test Geometry
		{
			// The hit test grid is actually populated during the initial cache phase, so don't bother
			// recording the hit test geometry on the same frame that we regenerate the cache.
			if (bWasCachingNeeded == false)
			{
				INC_DWORD_STAT_BY(STAT_SlateNumCachedElements, CachedWindowElements->GetElementCount());

				RootCacheNode->RecordHittestGeometry(Args.GetGrid(), Args.GetLastHitTestIndex(), MaximumLayerIdCachedAt, AbsoluteDeltaPosition);
			}
			else
			{
				INC_DWORD_STAT_BY(STAT_SlateNumInvalidatedElements, CachedWindowElements->GetElementCount());
			}
		}

		int32 OutMaxChildLayer = CachedMaxChildLayer;

		if (bCacheRenderData)
		{
			FSlateDrawElement::MakeCachedBuffer(OutDrawElements, MaximumLayerIdCachedAt, CachedRenderData, AbsoluteDeltaPosition);
			// Merge the resources into the draw element list so it can keep UObjects alive.
			OutDrawElements.MergeResources(CachedResources);
		}
		else
		{
			OutDrawElements.MergeElementList(CachedWindowElements.Get(), AbsoluteDeltaPosition);
		}

		// Paint the volatile elements
		if ( CachedWindowElements.IsValid() )
		{
#if SLATE_VERBOSE_NAMED_EVENTS
			SCOPED_NAMED_EVENT_TEXT("Paint Volatile Widgets", FColor::Cyan);
#endif
			const TArray<TSharedPtr<FSlateWindowElementList::FVolatilePaint>>& VolatileElements = CachedWindowElements->GetVolatileElements();
			INC_DWORD_STAT_BY(STAT_SlateNumVolatileWidgets, VolatileElements.Num());
			
			int32 VolatileLayerId = 0;
			if (bCacheRenderData)
			{
				VolatileLayerId = CachedWindowElements->PaintVolatile(OutDrawElements, Args.GetCurrentTime(), Args.GetDeltaTime(), AbsoluteDeltaPosition);
			}
			else
			{
				VolatileLayerId = CachedWindowElements->PaintVolatileRootLayer(OutDrawElements, Args.GetCurrentTime(), Args.GetDeltaTime(), AbsoluteDeltaPosition);
			}
			
			OutMaxChildLayer = FMath::Max(OutMaxChildLayer, VolatileLayerId);
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		if ( IsInvalidationDebuggingEnabled() )
		{
			// Draw a green or red border depending on if we were invalidated this frame.
			{
				check(Args.IsCaching() == false);
				//const bool bShowOutlineAsCached = Args.IsCaching() || bWasCachingNeeded == false;
				const FLinearColor DebugTint = bWasCachingNeeded ? FLinearColor::Red : ( bCacheRelativeTransforms ? FLinearColor::Blue : FLinearColor::Green );

				FGeometry ScaledOutline = AllottedGeometry.MakeChild(FVector2D(0, 0), AllottedGeometry.GetLocalSize() * AllottedGeometry.Scale, Inverse(AllottedGeometry.Scale));

				FSlateDrawElement::MakeBox(
					OutDrawElements,
					++OutMaxChildLayer,
					ScaledOutline.ToPaintGeometry(),
					FCoreStyle::Get().GetBrush(TEXT("Debug.Border")),
					ESlateDrawEffect::None,
					DebugTint
				);
			}

			static const FName InvalidationPanelName(TEXT("SInvalidationPanel"));

			const FSlateBrush* VolatileBrush = FCoreStyle::Get().GetBrush(TEXT("FocusRectangle"));

			// Draw a yellow outline around any volatile elements.
			const TArray<TSharedPtr<FSlateWindowElementList::FVolatilePaint>>& VolatileElements = CachedWindowElements->GetVolatileElements();
			for ( const TSharedPtr<FSlateWindowElementList::FVolatilePaint>& VolatileElement : VolatileElements )
			{
				// Ignore drawing the volatility rect for child invalidation panels, that's not really important, since
				// they're always volatile and it will make it hard to see when they're invalidated.
				if ( const SWidget* Widget = VolatileElement->GetWidget() )
				{
					if ( Widget->GetType() == InvalidationPanelName )
					{
						continue;
					}
				}

				FGeometry VolatileGeometry = VolatileElement->GetGeometry();
				if (!AbsoluteDeltaPosition.IsZero())
				{
					// Account for relative translation delta
					VolatileGeometry.AppendTransform(FSlateLayoutTransform(AbsoluteDeltaPosition));
				}
				
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					++OutMaxChildLayer,
					VolatileGeometry.ToPaintGeometry(),
					VolatileBrush,
					ESlateDrawEffect::None,
					FLinearColor::Yellow
				);
			}

			// Draw a red flash for any widget that invalidated us recently, we slowly 
			// fade out the flashes over time, unless the widget invalidates us again.
			for ( TMap<TWeakPtr<SWidget>, double>::TIterator It(InvalidatorWidgets); It; ++It )
			{
				TSharedPtr<SWidget> SafeInvalidator = It.Key().Pin();
				if ( SafeInvalidator.IsValid() )
				{
					FWidgetPath WidgetPath;
					if ( FSlateApplication::Get().GeneratePathToWidgetUnchecked(SafeInvalidator.ToSharedRef(), WidgetPath, EVisibility::All) )
					{
						FArrangedWidget ArrangedWidget = WidgetPath.FindArrangedWidget(SafeInvalidator.ToSharedRef()).Get(FArrangedWidget::NullWidget);
						ArrangedWidget.Geometry.AppendTransform( FSlateLayoutTransform(Inverse(Args.GetWindowToDesktopTransform())) );

						FSlateDrawElement::MakeBox(
							OutDrawElements,
							++OutMaxChildLayer,
							ArrangedWidget.Geometry.ToPaintGeometry(),
							FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
							ESlateDrawEffect::None,
							FLinearColor::Red.CopyWithNewOpacity(0.75f * It.Value())
						);
					}

					It.Value() -= FApp::GetDeltaTime();

					if ( It.Value() <= 0 )
					{
						It.RemoveCurrent();
					}
				}
				else
				{
					It.RemoveCurrent();
				}
			}
		}

#endif

		return OutMaxChildLayer;
	}
	else
	{
#if SLATE_VERBOSE_NAMED_EVENTS
		SCOPED_NAMED_EVENT_TEXT("SInvalidationPanel Uncached", FColor::Emerald);
#endif
		return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}
}

void SInvalidationPanel::SetContent(const TSharedRef< SWidget >& InContent)
{
	InvalidateCache();

	ChildSlot
	[
		InContent
	];
}

bool SInvalidationPanel::ComputeVolatility() const
{
	return true;
}
