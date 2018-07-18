// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Slate/SRetainerWidget.h"
#include "Misc/App.h"
#include "UObject/Package.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "Layout/WidgetCaching.h"


DECLARE_CYCLE_STAT(TEXT("Retainer Widget Tick"), STAT_SlateRetainerWidgetTick, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Retainer Widget Paint"), STAT_SlateRetainerWidgetPaint, STATGROUP_Slate);

#if !UE_BUILD_SHIPPING

/** True if we should allow widgets to be cached in the UI at all. */
TAutoConsoleVariable<int32> EnableRetainedRendering(
	TEXT("Slate.EnableRetainedRendering"),
	true,
	TEXT("Whether to attempt to render things in SRetainerWidgets to render targets first."));

static bool IsRetainedRenderingEnabled()
{
	return EnableRetainedRendering.GetValueOnGameThread() == 1;
}

FOnRetainedModeChanged SRetainerWidget::OnRetainerModeChangedDelegate;
#else

static bool IsRetainedRenderingEnabled()
{
	return true;
}

#endif

/** Whether or not the platform should have deferred retainer widget render target updating enabled by default */
#define PLATFORM_REQUIRES_DEFERRED_RETAINER_UPDATE PLATFORM_IOS || PLATFORM_ANDROID;

/**
 * If this is true the retained rendering render thread work will happen during normal slate render thread rendering after the back buffer has been presented
 * in order to avoid extra render target switching in the middle of the frame. The downside is that the UI update will be a frame late
 */
int32 GDeferRetainedRenderingRenderThread = PLATFORM_REQUIRES_DEFERRED_RETAINER_UPDATE;
FAutoConsoleVariableRef DeferRetainedRenderingRT(
	TEXT("Slate.DeferRetainedRenderingRenderThread"),
	GDeferRetainedRenderingRenderThread,
	TEXT("Whether or not to defer retained rendering to happen at the same time as the rest of slate render thread work"));


class FRetainerWidgetRenderingResources : public FDeferredCleanupInterface, public FGCObject
{
public:
	FRetainerWidgetRenderingResources()
		: WidgetRenderer(nullptr)
		, RenderTarget(nullptr)
		, DynamicEffect(nullptr)
	{}

	~FRetainerWidgetRenderingResources()
	{
		// Note not using deferred cleanup for widget renderer here as it is already in deferred cleanup
		if (WidgetRenderer)
		{
			delete WidgetRenderer;
		}
	}

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(RenderTarget);
		Collector.AddReferencedObject(DynamicEffect);
	}
public:
	FWidgetRenderer* WidgetRenderer;
	UTextureRenderTarget2D* RenderTarget;
	UMaterialInstanceDynamic* DynamicEffect;
};

TArray<SRetainerWidget*, TInlineAllocator<3>> SRetainerWidget::Shared_WaitingToRender;
int32 SRetainerWidget::Shared_MaxRetainerWorkPerFrame(0);
TFrameValue<int32> SRetainerWidget::Shared_RetainerWorkThisFrame(0);


SRetainerWidget::SRetainerWidget()
	: EmptyChildSlot(this)
	, RenderingResources(new FRetainerWidgetRenderingResources)
{
}

SRetainerWidget::~SRetainerWidget()
{
	if( FSlateApplication::IsInitialized() )
	{
#if !UE_BUILD_SHIPPING
		OnRetainerModeChangedDelegate.RemoveAll( this );
#endif
	}

	// Begin deferred cleanup of rendering resources.  DO NOT delete here.  Will be deleted when safe
	BeginCleanup(RenderingResources);

	Shared_WaitingToRender.Remove(this);
}

void SRetainerWidget::UpdateWidgetRenderer()
{
	// We can't write out linear.  If we write out linear, then we end up with premultiplied alpha
	// in linear space, which blending with gamma space later is difficult...impossible? to get right
	// since the rest of slate does blending in gamma space.
	const bool bWriteContentInGammaSpace = true;

	if (!RenderingResources->WidgetRenderer)
	{
		RenderingResources->WidgetRenderer = new FWidgetRenderer(bWriteContentInGammaSpace);
	}

	UTextureRenderTarget2D* RenderTarget = RenderingResources->RenderTarget;
	FWidgetRenderer* WidgetRenderer = RenderingResources->WidgetRenderer;

	WidgetRenderer->SetUseGammaCorrection(bWriteContentInGammaSpace);
	WidgetRenderer->SetIsPrepassNeeded(false);
	WidgetRenderer->SetClearHitTestGrid(false);

	// Update the render target to match the current gamma rendering preferences.
	if (RenderTarget && RenderTarget->SRGB != !bWriteContentInGammaSpace)
	{
		// Note, we do the opposite here of whatever write is, if we we're writing out gamma,
		// then sRGB writes were not supported, so it won't be an sRGB texture.
		RenderTarget->TargetGamma = !bWriteContentInGammaSpace ? 0.0f : 1.0;
		RenderTarget->SRGB = !bWriteContentInGammaSpace;

		RenderTarget->UpdateResource();
	}
}

void SRetainerWidget::Construct(const FArguments& InArgs)
{
	FSlateApplicationBase::Get().OnGlobalInvalidate().AddSP(this, &SRetainerWidget::OnGlobalInvalidate);

	STAT(MyStatId = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_Slate>(InArgs._StatId);)

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
	RenderTarget->ClearColor = FLinearColor::Transparent;
	RenderTarget->OverrideFormat = PF_B8G8R8A8;
	RenderTarget->bForceLinearGamma = false;

	RenderingResources->RenderTarget = RenderTarget;
	SurfaceBrush.SetResourceObject(RenderTarget);

	Window = SNew(SVirtualWindow)
    .Visibility(EVisibility::SelfHitTestInvisible);  // deubanks: We don't want Retainer Widgets blocking hit testing for tooltips

	Window->SetShouldResolveDeferred(false);
	
	UpdateWidgetRenderer();

	MyWidget = InArgs._Content.Widget;

	RenderOnPhase = InArgs._RenderOnPhase;
	RenderOnInvalidation = InArgs._RenderOnInvalidation;

	Phase = InArgs._Phase;
	PhaseCount = InArgs._PhaseCount;

	LastDrawTime = FApp::GetCurrentTime();
	LastTickedFrame = 0;

	bEnableRetainedRenderingDesire = true;
	bEnableRetainedRendering = false;

	bRenderRequested = true;

	RootCacheNode = nullptr;
	LastUsedCachedNodeIndex = 0;

	Window->SetContent(MyWidget.ToSharedRef());

	ChildSlot
	[
		Window.ToSharedRef()
	];

	if ( FSlateApplication::IsInitialized() )
	{
#if !UE_BUILD_SHIPPING
		OnRetainerModeChangedDelegate.AddRaw(this, &SRetainerWidget::OnRetainerModeChanged);

		static bool bStaticInit = false;

		if ( !bStaticInit )
		{
			bStaticInit = true;
			EnableRetainedRendering.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&SRetainerWidget::OnRetainerModeCVarChanged));
		}
#endif
	}
}

bool SRetainerWidget::ShouldBeRenderingOffscreen() const
{
	return bEnableRetainedRenderingDesire && IsRetainedRenderingEnabled();
}

bool SRetainerWidget::IsAnythingVisibleToRender() const
{
	return MyWidget.IsValid() && MyWidget->GetVisibility().IsVisible();
}

void SRetainerWidget::OnRetainerModeChanged()
{
	RefreshRenderingMode();
	Invalidate(EInvalidateWidget::Layout);
}

void SRetainerWidget::OnGlobalInvalidate()
{
	RequestRender();
}

#if !UE_BUILD_SHIPPING

void SRetainerWidget::OnRetainerModeCVarChanged( IConsoleVariable* CVar )
{
	OnRetainerModeChangedDelegate.Broadcast();
}

#endif

void SRetainerWidget::SetRetainedRendering(bool bRetainRendering)
{
	bEnableRetainedRenderingDesire = bRetainRendering;
}

void SRetainerWidget::RefreshRenderingMode()
{
	const bool bShouldBeRenderingOffscreen = ShouldBeRenderingOffscreen();

	if ( bEnableRetainedRendering != bShouldBeRenderingOffscreen )
	{
		bEnableRetainedRendering = bShouldBeRenderingOffscreen;

		Window->SetContent(MyWidget.ToSharedRef());
	}
}

void SRetainerWidget::SetContent(const TSharedRef< SWidget >& InContent)
{
	MyWidget = InContent;
	Window->SetContent(InContent);
}

UMaterialInstanceDynamic* SRetainerWidget::GetEffectMaterial() const
{
	return RenderingResources->DynamicEffect;
}

void SRetainerWidget::SetEffectMaterial(UMaterialInterface* EffectMaterial)
{
	if ( EffectMaterial )
	{
		UMaterialInstanceDynamic* DynamicEffect = Cast<UMaterialInstanceDynamic>(EffectMaterial);
		if ( !DynamicEffect )
		{
			DynamicEffect = UMaterialInstanceDynamic::Create(EffectMaterial, GetTransientPackage());
		}
		RenderingResources->DynamicEffect = DynamicEffect;

		SurfaceBrush.SetResourceObject(RenderingResources->DynamicEffect);
	}
	else
	{
		RenderingResources->DynamicEffect = nullptr;
		SurfaceBrush.SetResourceObject(RenderingResources->RenderTarget);
	}

	UpdateWidgetRenderer();
}

void SRetainerWidget::SetTextureParameter(FName TextureParameter)
{
	DynamicEffectTextureParameter = TextureParameter;
}
 
void SRetainerWidget::SetWorld(UWorld* World)
{
	OuterWorld = World;
}

FChildren* SRetainerWidget::GetChildren()
{
	if ( bEnableRetainedRendering )
	{
		return &EmptyChildSlot;
	}
	else
	{
		return SCompoundWidget::GetChildren();
	}
}

bool SRetainerWidget::ComputeVolatility() const
{
	return true;
}

FCachedWidgetNode* SRetainerWidget::CreateCacheNode() const
{
	// If the node pool is empty, allocate a few
	if (LastUsedCachedNodeIndex >= NodePool.Num())
	{
		for (int32 i = 0; i < 10; i++)
		{
			NodePool.Add(new FCachedWidgetNode());
		}
	}

	// Return one of the preallocated nodes and increment the next node index.
	FCachedWidgetNode* NewNode = NodePool[LastUsedCachedNodeIndex];
	++LastUsedCachedNodeIndex;

	return NewNode;
}

void SRetainerWidget::InvalidateWidget(SWidget* InvalidateWidget)
{
	if (RenderOnInvalidation)
	{
		bRenderRequested = true;
	}
}

void SRetainerWidget::SetRenderingPhase(int32 InPhase, int32 InPhaseCount)
{
	Phase = InPhase;
	PhaseCount = InPhaseCount;
}

void SRetainerWidget::RequestRender()
{
	bRenderRequested = true;
}

bool SRetainerWidget::PaintRetainedContent(const FPaintArgs& Args, const FGeometry& AllottedGeometry)
{
	if (RenderOnPhase)
	{
		if (LastTickedFrame != GFrameCounter && (GFrameCounter % PhaseCount) == Phase)
		{
			bRenderRequested = true;
		}
	}

	if (Shared_MaxRetainerWorkPerFrame > 0)
	{
		if (Shared_RetainerWorkThisFrame.TryGetValue(0) > Shared_MaxRetainerWorkPerFrame)
		{
			Shared_WaitingToRender.AddUnique(this);
			return false;
		}
	}
	
	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
	const FVector2D RenderSize = PaintGeometry.GetLocalSize() * PaintGeometry.GetAccumulatedRenderTransform().GetMatrix().GetScale().GetVector();

	if (RenderSize != PreviousRenderSize)
	{
		PreviousRenderSize = RenderSize;
		bRenderRequested = true;
	}

	if ( bRenderRequested )
	{
		// In order to get material parameter collections to function properly, we need the current world's Scene
		// properly propagated through to any widgets that depend on that functionality. The SceneViewport and RetainerWidget the 
		// only location where this information exists in Slate, so we push the current scene onto the current
		// Slate application so that we can leverage it in later calls.
		UWorld* TickWorld = OuterWorld.Get();
		if (TickWorld && TickWorld->Scene && IsInGameThread())
		{
			FSlateApplication::Get().GetRenderer()->RegisterCurrentScene(TickWorld->Scene);
		}
		else if (IsInGameThread())
		{
			FSlateApplication::Get().GetRenderer()->RegisterCurrentScene(nullptr);
		}

		// Update the number of retainers we've drawn this frame.
		Shared_RetainerWorkThisFrame = Shared_RetainerWorkThisFrame.TryGetValue(0) + 1;

		LastTickedFrame = GFrameCounter;
		const double TimeSinceLastDraw = FApp::GetCurrentTime() - LastDrawTime;

		const uint32 RenderTargetWidth  = FMath::RoundToInt(RenderSize.X);
		const uint32 RenderTargetHeight = FMath::RoundToInt(RenderSize.Y);

		const FVector2D ViewOffset = PaintGeometry.DrawPosition.RoundToVector();

		// Keep the visibilities the same, the proxy window should maintain the same visible/non-visible hit-testing of the retainer.
		Window->SetVisibility(GetVisibility());

		// Need to prepass.
		Window->SlatePrepass(AllottedGeometry.Scale);

		// Reset the cached node pool index so that we effectively reset the pool.
		LastUsedCachedNodeIndex = 0;
		RootCacheNode = nullptr;

		UTextureRenderTarget2D* RenderTarget = RenderingResources->RenderTarget;
		FWidgetRenderer* WidgetRenderer = RenderingResources->WidgetRenderer;

		if ( RenderTargetWidth != 0 && RenderTargetHeight != 0 )
		{
			if ( MyWidget->GetVisibility().IsVisible() )
			{
				if ( RenderTarget->GetSurfaceWidth() != RenderTargetWidth ||
					 RenderTarget->GetSurfaceHeight() != RenderTargetHeight )
				{
					
					// If the render target resource already exists just resize it.  Calling InitCustomFormat flushes render commands which could result in a huge hitch
					if(RenderTarget->GameThread_GetRenderTargetResource() && RenderTarget->OverrideFormat == PF_B8G8R8A8)
					{
						RenderTarget->ResizeTarget(RenderTargetWidth, RenderTargetHeight);
					}
					else
					{
						const bool bForceLinearGamma = false;
						RenderTarget->InitCustomFormat(RenderTargetWidth, RenderTargetHeight, PF_B8G8R8A8, bForceLinearGamma);
						RenderTarget->UpdateResourceImmediate();
					}
				}

				const float Scale = AllottedGeometry.Scale;

				const FVector2D DrawSize = FVector2D(RenderTargetWidth, RenderTargetHeight);
				const FGeometry WindowGeometry = FGeometry::MakeRoot(DrawSize * ( 1 / Scale ), FSlateLayoutTransform(Scale, PaintGeometry.DrawPosition));

				// Update the surface brush to match the latest size.
				SurfaceBrush.ImageSize = DrawSize;

				WidgetRenderer->ViewOffset = -ViewOffset;

				SRetainerWidget* MutableThis = const_cast<SRetainerWidget*>(this);
				TSharedRef<SRetainerWidget> SharedMutableThis = SharedThis(MutableThis);
				
				FPaintArgs PaintArgs(*this, Args.GetGrid(), Args.GetWindowToDesktopTransform(), FApp::GetCurrentTime(), Args.GetDeltaTime());

				RootCacheNode = CreateCacheNode();
				RootCacheNode->Initialize(Args, SharedMutableThis, WindowGeometry);

				WidgetRenderer->DrawWindow(
					PaintArgs.EnableCaching(SharedMutableThis, RootCacheNode, true, true),
					RenderTarget,
					Window.ToSharedRef(),
					WindowGeometry,
					WindowGeometry.GetLayoutBoundingRect(),
					TimeSinceLastDraw,
					GDeferRetainedRenderingRenderThread!=0);

				bRenderRequested = false;
				Shared_WaitingToRender.Remove(this);

				LastDrawTime = FApp::GetCurrentTime();

				return true;
			}
		}
	}

	return false;
}

int32 SRetainerWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	STAT(FScopeCycleCounter PaintCycleCounter(MyStatId);)

	SRetainerWidget* MutableThis = const_cast<SRetainerWidget*>( this );

	MutableThis->RefreshRenderingMode();

	if ( bEnableRetainedRendering && IsAnythingVisibleToRender() )
	{
		SCOPE_CYCLE_COUNTER( STAT_SlateRetainerWidgetPaint );

		TSharedRef<SRetainerWidget> SharedMutableThis = SharedThis(MutableThis);

		const bool bNewFramePainted = MutableThis->PaintRetainedContent(Args, AllottedGeometry);

		UTextureRenderTarget2D* RenderTarget = RenderingResources->RenderTarget;

		if ( RenderTarget->GetSurfaceWidth() >= 1 && RenderTarget->GetSurfaceHeight() >= 1 )
		{
			const FLinearColor ComputedColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint() * ColorAndOpacity.Get() * SurfaceBrush.GetTint(InWidgetStyle));
			// Retainer widget uses pre-multiplied alpha, so pre-multiply the color by the alpha to respect opacity.
			const FLinearColor PremultipliedColorAndOpacity(ComputedColorAndOpacity * ComputedColorAndOpacity.A);

			FWidgetRenderer* WidgetRenderer = RenderingResources->WidgetRenderer;
			UMaterialInstanceDynamic* DynamicEffect = RenderingResources->DynamicEffect;
	
			const bool bDynamicMaterialInUse = (DynamicEffect != nullptr);
			if (bDynamicMaterialInUse)
			{
				DynamicEffect->SetTextureParameterValue(DynamicEffectTextureParameter, RenderTarget);
			}

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				&SurfaceBrush,
				// We always write out the content in gamma space, so when we render the final version we need to
				// render without gamma correction enabled.
				ESlateDrawEffect::PreMultipliedAlpha | ESlateDrawEffect::NoGamma,
				FLinearColor(PremultipliedColorAndOpacity.R, PremultipliedColorAndOpacity.G, PremultipliedColorAndOpacity.B, PremultipliedColorAndOpacity.A)
			);
			
			if (RootCacheNode)
			{
				RootCacheNode->RecordHittestGeometry(Args.GetGrid(), Args.GetLastHitTestIndex(), LayerId, FVector2D(0, 0));
			}

			// Any deferred painted elements of the retainer should be drawn directly by the main renderer, not rendered into the render target,
			// as most of those sorts of things will break the rendering rect, things like tooltips, and popup menus.
			for ( auto& DeferredPaint : WidgetRenderer->DeferredPaints )
			{
				OutDrawElements.QueueDeferredPainting(DeferredPaint->Copy(Args));
			}
		}

		return LayerId;
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

FVector2D SRetainerWidget::ComputeDesiredSize(float LayoutScaleMuliplier) const
{
	if ( bEnableRetainedRendering )
	{
		return MyWidget->GetDesiredSize();
	}
	else
	{
		return SCompoundWidget::ComputeDesiredSize(LayoutScaleMuliplier);
	}
}
