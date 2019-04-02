// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Slate/SGameLayerManager.h"
#include "Widgets/SOverlay.h"
#include "Engine/LocalPlayer.h"
#include "Slate/SceneViewport.h"
#include "EngineGlobals.h"
#include "SceneView.h"
#include "Engine/Engine.h"
#include "Types/NavigationMetaData.h"
#include "Engine/GameEngine.h"
#include "Engine/UserInterfaceSettings.h"
#include "GeneralProjectSettings.h"
#include "Widgets/LayerManager/STooltipPresenter.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SPopup.h"
#include "Widgets/Layout/SWindowTitleBarArea.h"
#include "DebugCanvas.h"

/* SGameLayerManager interface
 *****************************************************************************/

SGameLayerManager::SGameLayerManager()
:	DefaultWindowTitleBarHeight(64.0f)
,	bIsGameUsingBorderlessWindow(false)
,	bUseScaledDPI(false)
{
}

void SGameLayerManager::Construct(const SGameLayerManager::FArguments& InArgs)
{
	SceneViewport = InArgs._SceneViewport;

	TSharedRef<SDPIScaler> DPIScaler =
		SNew(SDPIScaler)
		.DPIScale(this, &SGameLayerManager::GetGameViewportDPIScale)
		[
			// All user widgets live inside this vertical box.
			SAssignNew(WidgetHost, SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(TitleBarAreaVerticalBox, SWindowTitleBarArea)
				[
					SAssignNew(WindowTitleBarVerticalBox, SBox)
				]
			]

			+ SVerticalBox::Slot()
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				[
					SAssignNew(PlayerCanvas, SCanvas)
				]

				+ SOverlay::Slot()
				[
					InArgs._Content.Widget
				]

				+ SOverlay::Slot()
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(TitleBarAreaOverlay, SWindowTitleBarArea)
						[
							SAssignNew(WindowTitleBarOverlay, SBox)
						]
					]
				]

				+ SOverlay::Slot()
				[
					SNew(SPopup)
					[
						SAssignNew(TooltipPresenter, STooltipPresenter)
					]
				]
				+ SOverlay::Slot()
				[
					SAssignNew(DebugCanvas, SDebugCanvas)
					.SceneViewport(InArgs._SceneViewport)

				]
			]
		];

	ChildSlot
	[
		DPIScaler
	];

	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (GameEngine != nullptr)
	{
		TSharedPtr<SWindow> GameViewportWindow = GameEngine->GameViewportWindow.Pin();
		if (GameViewportWindow.IsValid())
		{
			TitleBarAreaOverlay->SetGameWindow(GameViewportWindow);
			TitleBarAreaVerticalBox->SetGameWindow(GameViewportWindow);
		}
	}

	DefaultTitleBarContentWidget =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox).HeightOverride(this, &SGameLayerManager::GetDefaultWindowTitleBarHeight)
		];

	TitleBarAreaOverlay->SetRequestToggleFullscreenCallback(FSimpleDelegate::CreateSP(this, &SGameLayerManager::RequestToggleFullscreen));
	TitleBarAreaVerticalBox->SetRequestToggleFullscreenCallback(FSimpleDelegate::CreateSP(this, &SGameLayerManager::RequestToggleFullscreen));

	SetWindowTitleBarState(nullptr, EWindowTitleBarMode::Overlay, false, false, false);

	bIsGameUsingBorderlessWindow = GetDefault<UGeneralProjectSettings>()->bUseBorderlessWindow && PLATFORM_WINDOWS;
}

void SGameLayerManager::SetSceneViewport(FSceneViewport* InSceneViewport)
{
	SceneViewport = InSceneViewport;
	DebugCanvas->SetSceneViewport(InSceneViewport);
}

const FGeometry& SGameLayerManager::GetViewportWidgetHostGeometry() const
{
	return WidgetHost->GetCachedGeometry();
}

const FGeometry& SGameLayerManager::GetPlayerWidgetHostGeometry(ULocalPlayer* Player) const
{
	TSharedPtr<FPlayerLayer> PlayerLayer = PlayerLayers.FindRef(Player);
	if ( PlayerLayer.IsValid() )
	{
		return PlayerLayer->Widget->GetCachedGeometry();
	}

	static FGeometry Identity;
	return Identity;
}

void SGameLayerManager::NotifyPlayerAdded(int32 PlayerIndex, ULocalPlayer* AddedPlayer)
{
	UpdateLayout();
}

void SGameLayerManager::NotifyPlayerRemoved(int32 PlayerIndex, ULocalPlayer* RemovedPlayer)
{
	UpdateLayout();
}

void SGameLayerManager::AddWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent, const int32 ZOrder)
{
	TSharedPtr<FPlayerLayer> PlayerLayer = FindOrCreatePlayerLayer(Player);
	
	// NOTE: Returns FSimpleSlot but we're ignoring here.  Could be used for alignment though.
	PlayerLayer->Widget->AddSlot(ZOrder)
	[
		ViewportContent
	];
}

void SGameLayerManager::RemoveWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent)
{
	TSharedPtr<FPlayerLayer>* PlayerLayerPtr = PlayerLayers.Find(Player);
	if ( PlayerLayerPtr )
	{
		TSharedPtr<FPlayerLayer> PlayerLayer = *PlayerLayerPtr;
		PlayerLayer->Widget->RemoveSlot(ViewportContent);
	}
}

void SGameLayerManager::ClearWidgetsForPlayer(ULocalPlayer* Player)
{
	TSharedPtr<FPlayerLayer> PlayerLayer = PlayerLayers.FindRef(Player);
	if ( PlayerLayer.IsValid() )
	{
		PlayerLayer->Widget->ClearChildren();
	}
}

TSharedPtr<IGameLayer> SGameLayerManager::FindLayerForPlayer(ULocalPlayer* Player, const FName& LayerName)
{
	TSharedPtr<FPlayerLayer> PlayerLayer = PlayerLayers.FindRef(Player);
	if ( PlayerLayer.IsValid() )
	{
		return PlayerLayer->Layers.FindRef(LayerName);
	}

	return TSharedPtr<IGameLayer>();
}

bool SGameLayerManager::AddLayerForPlayer(ULocalPlayer* Player, const FName& LayerName, TSharedRef<IGameLayer> Layer, int32 ZOrder)
{
	TSharedPtr<FPlayerLayer> PlayerLayer = FindOrCreatePlayerLayer(Player);
	if ( PlayerLayer.IsValid() )
	{
		TSharedPtr<IGameLayer> ExistingLayer = PlayerLayer->Layers.FindRef(LayerName);
		if ( ExistingLayer.IsValid() )
		{
			return false;
		}

		PlayerLayer->Layers.Add(LayerName, Layer);

		PlayerLayer->Widget->AddSlot(ZOrder)
		[
			Layer->AsWidget()
		];

		return true;
	}

	return false;
}

void SGameLayerManager::ClearWidgets()
{
	PlayerCanvas->ClearChildren();

	// Potential for removed layers to impact the map, so need to
	// remove & delete as separate steps
	while (PlayerLayers.Num())
	{
		const auto LayerIt = PlayerLayers.CreateIterator();
		const TSharedPtr<FPlayerLayer> Layer = LayerIt.Value();

		if (Layer.IsValid())
		{
			Layer->Slot = nullptr;
		}

		PlayerLayers.Remove(LayerIt.Key());
	}

	SetWindowTitleBarState(nullptr, EWindowTitleBarMode::Overlay, false, false, false);
}

void SGameLayerManager::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	CachedGeometry = AllottedGeometry;

	UpdateLayout();
}

int32 SGameLayerManager::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	SCOPED_NAMED_EVENT_TEXT("Paint: Game UI", FColor::Green);
	const int32 ResultLayer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	return ResultLayer;
}

bool SGameLayerManager::OnVisualizeTooltip(const TSharedPtr<SWidget>& TooltipContent)
{
	TooltipPresenter->SetContent(TooltipContent);

	return true;
}

void SGameLayerManager::SetUseFixedDPIValue(const bool bInUseFixedDPI, const FIntPoint ViewportSize /*= FIntPoint()*/)
{
	bUseScaledDPI = bInUseFixedDPI;
	ScaledDPIViewportReference = ViewportSize;
}

bool SGameLayerManager::IsUsingFixedDPIValue() const
{
	return bUseScaledDPI;
}

float SGameLayerManager::GetGameViewportDPIScale() const
{
	const FSceneViewport* Viewport = SceneViewport.Get();

	if (Viewport == nullptr)
	{
		return 1;
	}

	const auto UserInterfaceSettings = GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());

	if (UserInterfaceSettings == nullptr)
	{
		return 1;
	}

	FIntPoint ViewportSize = Viewport->GetSize();
	float GameUIScale;

	if (bUseScaledDPI)
	{
		float DPIValue = UserInterfaceSettings->GetDPIScaleBasedOnSize(ScaledDPIViewportReference);
		float ViewportScale = FMath::Min((float)ViewportSize.X / (float)ScaledDPIViewportReference.X, (float)ViewportSize.Y / (float)ScaledDPIViewportReference.Y);

		GameUIScale = DPIValue * ViewportScale;
	}
	else
	{
		GameUIScale = UserInterfaceSettings->GetDPIScaleBasedOnSize(ViewportSize);
	}

	// Remove the platform DPI scale from the incoming size.  Since the platform DPI is already
	// attempt to normalize the UI for a high DPI, and the DPI scale curve is based on raw resolution
	// for what a assumed platform scale of 1, extract that scale the calculated scale, since that will
	// already be applied by slate.
	const float FinalUIScale = GameUIScale / Viewport->GetCachedGeometry().Scale;

	return FinalUIScale;
}

FOptionalSize SGameLayerManager::GetDefaultWindowTitleBarHeight() const
{
	return DefaultWindowTitleBarHeight;
}

void SGameLayerManager::UpdateLayout()
{
	if ( const FSceneViewport* Viewport = SceneViewport.Get() )
	{
		if ( UWorld* World = Viewport->GetClient()->GetWorld() )
		{
			if ( World->IsGameWorld() == false )
			{
				PlayerLayers.Reset();
				return;
			}

			if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
			{
				const TArray<ULocalPlayer*>& GamePlayers = GEngine->GetGamePlayers(World);

				RemoveMissingPlayerLayers(GamePlayers);
				AddOrUpdatePlayerLayers(CachedGeometry, ViewportClient, GamePlayers);
			}
		}
	}
}

TSharedPtr<SGameLayerManager::FPlayerLayer> SGameLayerManager::FindOrCreatePlayerLayer(ULocalPlayer* LocalPlayer)
{
	TSharedPtr<FPlayerLayer>* PlayerLayerPtr = PlayerLayers.Find(LocalPlayer);
	if ( PlayerLayerPtr == nullptr )
	{
		// Prevent any navigation outside of a player's layer once focus has been placed there.
		TSharedRef<FNavigationMetaData> StopNavigation = MakeShareable(new FNavigationMetaData());
		StopNavigation->SetNavigationStop(EUINavigation::Up);
		StopNavigation->SetNavigationStop(EUINavigation::Down);
		StopNavigation->SetNavigationStop(EUINavigation::Left);
		StopNavigation->SetNavigationStop(EUINavigation::Right);
		StopNavigation->SetNavigationStop(EUINavigation::Previous);
		StopNavigation->SetNavigationStop(EUINavigation::Next);

		// Create a new entry for the player
		TSharedPtr<FPlayerLayer> NewLayer = MakeShareable(new FPlayerLayer());

		// Create a new overlay widget to house any widgets we want to display for the player.
		NewLayer->Widget = SNew(SOverlay)
			.AddMetaData(StopNavigation)
			.Clipping(EWidgetClipping::ClipToBoundsAlways);
		
		// Add the overlay to the player canvas, which we'll update every frame to match
		// the dimensions of the player's split screen rect.
		PlayerCanvas->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Expose(NewLayer->Slot)
			[
				NewLayer->Widget.ToSharedRef()
			];

		PlayerLayerPtr = &PlayerLayers.Add(LocalPlayer, NewLayer);
	}

	return *PlayerLayerPtr;
}

void SGameLayerManager::RemoveMissingPlayerLayers(const TArray<ULocalPlayer*>& GamePlayers)
{
	TArray<ULocalPlayer*> ToRemove;

	// Find the player layers for players that no longer exist
	for ( TMap< ULocalPlayer*, TSharedPtr<FPlayerLayer> >::TIterator It(PlayerLayers); It; ++It )
	{
		ULocalPlayer* Key = ( *It ).Key;
		if ( !GamePlayers.Contains(Key) )
		{
			ToRemove.Add(Key);
		}
	}

	// Remove the missing players
	for ( ULocalPlayer* Player : ToRemove )
	{
		RemovePlayerWidgets(Player);
	}
}

void SGameLayerManager::RemovePlayerWidgets(ULocalPlayer* LocalPlayer)
{
	TSharedPtr<FPlayerLayer> Layer = PlayerLayers.FindRef(LocalPlayer);
	PlayerCanvas->RemoveSlot(Layer->Widget.ToSharedRef());

	PlayerLayers.Remove(LocalPlayer);
}

void SGameLayerManager::AddOrUpdatePlayerLayers(const FGeometry& AllottedGeometry, UGameViewportClient* ViewportClient, const TArray<ULocalPlayer*>& GamePlayers)
{
	if (GamePlayers.Num() == 0)
	{
		return;
	}

	ESplitScreenType::Type SplitType = ViewportClient->GetCurrentSplitscreenConfiguration();
	TArray<struct FSplitscreenData>& SplitInfo = ViewportClient->SplitscreenInfo;

	float InverseDPIScale = ViewportClient->Viewport ? 1.0f / GetGameViewportDPIScale() : 1.0f;

	// Add and Update Player Layers
	for ( int32 PlayerIndex = 0; PlayerIndex < GamePlayers.Num(); PlayerIndex++ )
	{
		ULocalPlayer* Player = GamePlayers[PlayerIndex];

		if ( SplitType < SplitInfo.Num() && PlayerIndex < SplitInfo[SplitType].PlayerData.Num() )
		{
			TSharedPtr<FPlayerLayer> PlayerLayer = FindOrCreatePlayerLayer(Player);
			FPerPlayerSplitscreenData& SplitData = SplitInfo[SplitType].PlayerData[PlayerIndex];

			// Viewport Sizes
			FVector2D Size, Position;
			Size.X = SplitData.SizeX;
			Size.Y = SplitData.SizeY;
			Position.X = SplitData.OriginX;
			Position.Y = SplitData.OriginY;

			FVector2D AspectRatioInset = GetAspectRatioInset(Player);

			Position += AspectRatioInset;
			Size -= (AspectRatioInset * 2.0f);

			Size = Size * AllottedGeometry.GetLocalSize() * InverseDPIScale;
			Position = Position * AllottedGeometry.GetLocalSize() * InverseDPIScale;

			if (WindowTitleBarState.Mode == EWindowTitleBarMode::VerticalBox && Size.Y > WindowTitleBarVerticalBox->GetDesiredSize().Y)
			{
				Size.Y -= WindowTitleBarVerticalBox->GetDesiredSize().Y;
			}

			PlayerLayer->Slot->Size(Size);
			PlayerLayer->Slot->Position(Position);
		}
	}
}

FVector2D SGameLayerManager::GetAspectRatioInset(ULocalPlayer* LocalPlayer) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SGameLayerManager_GetAspectRatioInset);
	FVector2D Offset(0.f, 0.f);
	if ( LocalPlayer )
	{
		FSceneViewProjectionData ProjectionData;
		if (LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, eSSP_FULL, ProjectionData))
		{
			const FIntRect ViewRect = ProjectionData.GetViewRect();
			const FIntRect ConstrainedViewRect = ProjectionData.GetConstrainedViewRect();

			// Return normalized coordinates.
			Offset.X = ( ConstrainedViewRect.Min.X - ViewRect.Min.X ) / (float)ViewRect.Width();
			Offset.Y = ( ConstrainedViewRect.Min.Y - ViewRect.Min.Y ) / (float)ViewRect.Height();
		}
	}

	return Offset;
}

void SGameLayerManager::SetDefaultWindowTitleBarHeight(float Height)
{
	DefaultWindowTitleBarHeight = Height;
}

void SGameLayerManager::SetWindowTitleBarState(const TSharedPtr<SWidget>& TitleBarContent, EWindowTitleBarMode Mode, bool bTitleBarDragEnabled, bool bWindowButtonsVisible, bool bTitleBarVisible)
{
	UE_LOG(LogSlate, Log, TEXT("Updating window title bar state: %s mode, drag %s, window buttons %s, title bar %s"),
		Mode == EWindowTitleBarMode::Overlay ? TEXT("overlay") : TEXT("vertical box"),
		bTitleBarDragEnabled ? TEXT("enabled") : TEXT("disabled"),
		bWindowButtonsVisible ? TEXT("visible") : TEXT("hidden"),
		bTitleBarVisible ? TEXT("visible") : TEXT("hidden"));
	WindowTitleBarState.ContentWidget = TitleBarContent.IsValid() ? TitleBarContent : DefaultTitleBarContentWidget;
	WindowTitleBarState.Mode = Mode;
	WindowTitleBarState.bTitleBarDragEnabled = bTitleBarDragEnabled;
	WindowTitleBarState.bWindowButtonsVisible = bWindowButtonsVisible;
	WindowTitleBarState.bTitleBarVisible = bTitleBarVisible && bIsGameUsingBorderlessWindow;
	UpdateWindowTitleBar();
}

void SGameLayerManager::RestorePreviousWindowTitleBarState()
{
	// TODO: remove RestorePreviousWindowTitleBarState() and replace its usage in widget blueprints with SetWindowTitleBarState() calls
	SetWindowTitleBarState(nullptr, EWindowTitleBarMode::Overlay, false, false, false);
}

void SGameLayerManager::SetWindowTitleBarVisibility(bool bIsVisible)
{
	WindowTitleBarState.bTitleBarVisible = bIsVisible && bIsGameUsingBorderlessWindow;
	UpdateWindowTitleBarVisibility();
}

void SGameLayerManager::UpdateWindowTitleBar()
{
	if (WindowTitleBarState.ContentWidget.IsValid())
	{
		if (WindowTitleBarState.Mode == EWindowTitleBarMode::Overlay)
		{
			WindowTitleBarOverlay->SetContent(WindowTitleBarState.ContentWidget.ToSharedRef());
			TitleBarAreaOverlay->SetWindowButtonsVisibility(WindowTitleBarState.bWindowButtonsVisible);
		}
		else if (WindowTitleBarState.Mode == EWindowTitleBarMode::VerticalBox)
		{
			WindowTitleBarVerticalBox->SetContent(WindowTitleBarState.ContentWidget.ToSharedRef());
			TitleBarAreaVerticalBox->SetWindowButtonsVisibility(WindowTitleBarState.bWindowButtonsVisible);
		}
	}

	UpdateWindowTitleBarVisibility();
}

void SGameLayerManager::UpdateWindowTitleBarVisibility()
{
	const EVisibility VisibilityWhenEnabled = WindowTitleBarState.bTitleBarDragEnabled ? EVisibility::Visible : EVisibility::SelfHitTestInvisible;
	if (WindowTitleBarState.Mode == EWindowTitleBarMode::Overlay)
	{
		TitleBarAreaOverlay->SetVisibility(WindowTitleBarState.bTitleBarVisible ? VisibilityWhenEnabled : EVisibility::Collapsed);
		TitleBarAreaVerticalBox->SetVisibility(EVisibility::Collapsed);
	}
	else if (WindowTitleBarState.Mode == EWindowTitleBarMode::VerticalBox)
	{
		TitleBarAreaOverlay->SetVisibility(EVisibility::Collapsed);
		TitleBarAreaVerticalBox->SetVisibility(WindowTitleBarState.bTitleBarVisible ? VisibilityWhenEnabled : EVisibility::Collapsed);
	}
}

void SGameLayerManager::RequestToggleFullscreen()
{
	// SWindowTitleBarArea cannot access GEngine, so it'll call this when it needs to toggle fullscreen
	if (GEngine)
	{
		GEngine->DeferredCommands.Add(TEXT("TOGGLE_FULLSCREEN"));
	}
}
