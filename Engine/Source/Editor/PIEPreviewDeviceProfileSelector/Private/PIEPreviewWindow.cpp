// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PIEPreviewWindow.h"

#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "SEditorViewportToolBarMenu.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SCheckBox.h"
#include "Misc/ConfigCacheIni.h"


#if WITH_EDITOR

//***********************************************************************************
//SPIEToolbar Implementation
//***********************************************************************************

/** Toolbar class used to add some menus to configure various device display settings */
class SPIEToolbar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SPIEToolbar) {}
		SLATE_EVENT(FOnGetContent, OnGetMenuContent)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	FReply OnMenuClicked();

protected:
	TSharedPtr<SMenuAnchor> MenuAnchor;
};

void SPIEToolbar::Construct(const FArguments& InArgs)
{
	SViewportToolBar::Construct(SViewportToolBar::FArguments());

	const FSlateBrush* ImageBrush = FPIEPreviewWindowCoreStyle::Get().GetBrush("ComboButton.Arrow");

	const float MenuIconSize = 16.0f;

	TSharedPtr<SWidget> ButtonContent =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(MenuIconSize)
				.HeightOverride(MenuIconSize)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image(ImageBrush)
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		];

	ChildSlot
	[
		SAssignNew(MenuAnchor, SMenuAnchor)
		.Placement(MenuPlacement_BelowAnchor)
		[
			SNew(SButton)
			// Allows users to drag with the mouse to select options after opening the menu */
			.ClickMethod(EButtonClickMethod::MouseDown)
			.ContentPadding(FMargin(2.0f, 2.0f))
			.VAlign(VAlign_Center)
			.ButtonStyle(FPIEPreviewWindowCoreStyle::Get(), "PIEWindow.MenuButton")
			.OnClicked(this, &SPIEToolbar::OnMenuClicked)
			[
				ButtonContent.ToSharedRef()
			]
		]
		.OnGetMenuContent(InArgs._OnGetMenuContent)
	];
}

FReply SPIEToolbar::OnMenuClicked()
{
	// If the menu button is clicked toggle the state of the menu anchor which will open or close the menu
	if (MenuAnchor->ShouldOpenDueToClick())
	{
		MenuAnchor->SetIsOpen(true);
		SetOpenMenu(MenuAnchor);
	}
	else
	{
		MenuAnchor->SetIsOpen(false);
		TSharedPtr<SMenuAnchor> NullAnchor;
		SetOpenMenu(NullAnchor);
	}

	return FReply::Handled();
}

//***********************************************************************************
//SPIEPreviewWindow Implementation
//***********************************************************************************
SPIEPreviewWindow::SPIEPreviewWindow()
{

}

void SPIEPreviewWindow::Construct(const FArguments& InArgs, TSharedPtr<FPIEPreviewDevice> InDevice)
{
	SWindow::Construct(InArgs);

	SetDevice(InDevice);
}

TSharedRef<SWidget> SPIEPreviewWindow::MakeWindowTitleBar(const TSharedRef<SWindow>& Window, const TSharedPtr<SWidget>& CenterContent, EHorizontalAlignment CenterContentAlignment)
{
	TSharedRef<SPIEPreviewWindowTitleBar> WindowTitleBar = SNew(SPIEPreviewWindowTitleBar, Window, CenterContent, CenterContentAlignment)
		.Visibility(EVisibility::SelfHitTestInvisible);
	return WindowTitleBar;
}

EHorizontalAlignment SPIEPreviewWindow::GetTitleAlignment()
{
	return EHorizontalAlignment::HAlign_Left;
}

void SPIEPreviewWindow::SetSceneViewportPadding(const FMargin& Margin)
{
	ContentSlot->SlotPadding = Margin;
}

void SPIEPreviewWindow::ComputeBezelOrientation()
{
	if (BezelImage.IsValid())
	{
		FVector2D WindowSize = GetClientSizeInScreen();

		bool bBezelRotated = Device->IsDeviceFlipped();

		float ScaleX = bBezelRotated ? WindowSize.X / WindowSize.Y : 1.0f;
		float ScaleY = bBezelRotated ? Inverse(ScaleX) : 1.0f;

		FScale2D Scale = FScale2D(ScaleX, ScaleY);
		FQuat2D Rotation = FQuat2D(bBezelRotated ? -PI / 2 : 0);
		FMatrix2x2 ImageTransformationMatrix = Concatenate(Rotation, Scale);

		BezelImage->SetRenderTransform(FSlateRenderTransform(ImageTransformationMatrix));
	}
}

void SPIEPreviewWindow::CreateMenuToolBar()
{
	AddOverlaySlot()
		.Padding(5.0f, 3.0f + SWindowDefs::DefaultTitleBarSize, 0, 0)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SNew(SPIEToolbar)
			.OnGetMenuContent(this, &SPIEPreviewWindow::BuildSettingsMenu)
		];
}

TSharedRef<SWidget> SPIEPreviewWindow::BuildSettingsMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	auto ScaleDescriptionWidget = SNew(STextBlock)
		.Text(FText::FromString(TEXT("Window Scale")))
		.Justification(ETextJustify::Center);

	MenuBuilder.AddWidget(ScaleDescriptionWidget, FText());
	MenuBuilder.AddMenuSeparator();

	// create a scaling checkboxes for each scaling factor needed by the emulated device
	TArray<float>& ArrScaleFactors = Device->GetDeviceSpecs()->ScaleFactors;
	for (int32 i = 0; i < ArrScaleFactors.Num(); ++i)
	{
		const float ScaleFactor = ArrScaleFactors[i];

		FText EntryText = FText::FromString(FString::SanitizeFloat(ArrScaleFactors[i]) + TEXT("x"));
		auto IsCheckedFunction = [this, ScaleFactor]()
		{
			float WindowScaleFactor = GetWindowScaleFactor();

			return FMath::IsNearlyEqual(ScaleFactor, WindowScaleFactor) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		};

		auto ExecuteActionFunction = [this, ScaleFactor]()
		{
			SetScaleWindowToDeviceSize(false);
			ScaleWindow(ScaleFactor);
		};

		CreateMenuEntry(MenuBuilder, MoveTemp(EntryText), MoveTemp(IsCheckedFunction), MoveTemp(ExecuteActionFunction));
	}

	// scale to device size checkbox
	if (Device->GetDeviceSpecs()->PPI != 0)
	{
		FText EntryText = FText::FromString(TEXT("Scale to device size"));
		auto IsCheckedFunction = [this]()
		{
			bool bScaleToDeviceSize = IsManualManageDPIChanges();
			return bScaleToDeviceSize ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		};

		auto ExecuteActionFunction = [this]()
		{
			SetScaleWindowToDeviceSize(true);
		};

		CreateMenuEntry(MenuBuilder, MoveTemp(EntryText), MoveTemp(IsCheckedFunction), MoveTemp(ExecuteActionFunction));

		MenuBuilder.AddMenuSeparator();
	}

	// add bClampWindowSizeState checkbox
	{
		FText EntryText = FText::FromString(TEXT("Restrict to desktop size"));
		auto IsCheckedFunction = [this]()
		{
			bool bChecked = IsClampingWindowSize();
			return bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		};

		auto ExecuteActionFunction = [this]()
		{
			bool bClamp = IsClampingWindowSize();
			SetClampWindowSize(!bClamp);
		};

		CreateMenuEntry(MenuBuilder, MoveTemp(EntryText), MoveTemp(IsCheckedFunction), MoveTemp(ExecuteActionFunction));
	}
	// add checkbox to handle bezel visibility
	{
		FText EntryText = FText::FromString(TEXT("Show phone bezel"));
		auto IsCheckedFunction = [this]()
		{
			bool bBezelVisibility = GetBezelVisibility();
			return bBezelVisibility ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		};

		auto ExecuteActionFunction = [this]()
		{
			FlipBezelVisibility();
		};

		CreateMenuEntry(MenuBuilder, MoveTemp(EntryText), MoveTemp(IsCheckedFunction), MoveTemp(ExecuteActionFunction));
	}

	return MenuBuilder.MakeWidget();
}

void SPIEPreviewWindow::CreateMenuEntry(FMenuBuilder& MenuBuilder, FText&& TextEntry, TFunction<ECheckBoxState()>&& IsCheckedFunction, TFunction<void()>&& ExecuteActionFunction)
{
	auto Box =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		[
			SNew(STextBlock)
			.Visibility(EVisibility::HitTestInvisible)
			.Text(MoveTemp(TextEntry))
		]
		+ SHorizontalBox::Slot()
		.Padding(FMargin(10.0f, 0.0f))
		.HAlign(EHorizontalAlignment::HAlign_Right)
		[
			SNew(SCheckBox)
			.IsFocusable(false)
			.IsEnabled(false)
			.IsChecked_Lambda(MoveTemp(IsCheckedFunction))
		];

	MenuBuilder.AddMenuEntry(FUIAction(FExecuteAction::CreateLambda(MoveTemp(ExecuteActionFunction))), Box);
}

void SPIEPreviewWindow::CreatePIEPreviewBezelOverlay(UTexture2D* pBezelImage)
{
	if (pBezelImage != nullptr)
	{
		BezelBrush.SetResourceObject(pBezelImage);
		BezelBrush.ImageSize = FVector2D(pBezelImage->GetSizeX(), pBezelImage->GetSizeY());

		auto GetBezelVisibility = [this]()
		{
			EVisibility Visibility = EVisibility::Collapsed;

			if (Device.IsValid())
			{
				bool bVisible = Device->GetBezelVisibility();
				Visibility = bVisible ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
			}

			return Visibility;
		};

		BezelImage = SNew(SImage)
			.Image(&BezelBrush)
			.Visibility_Lambda(GetBezelVisibility)
			.RenderTransformPivot(FVector2D(.5f, .5f));

		AddOverlaySlot()
			.Padding(0, SWindowDefs::DefaultTitleBarSize, 0, 0)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				BezelImage.ToSharedRef()
			];

		ComputeBezelOrientation();
	}
}

void SPIEPreviewWindow::ScaleWindow(const float ScreenFactor, const float DPIScaleFactor /*= 1.0f*/)
{
	if (!Device.IsValid())
	{
		return;
	}

	if (FMath::IsNearlyEqual(ScreenFactor, CachedScaleToDeviceFactor) && FMath::IsNearlyEqual(DPIScaleFactor, CachedDPIScaleFactor))
	{
		return;
	}

	CachedScaleToDeviceFactor = ScreenFactor;
	CachedDPIScaleFactor = DPIScaleFactor;

	if (IsManualManageDPIChanges())
	{
		FSlateApplication::Get().HandleDPIScaleChanged(GetNativeWindow().ToSharedRef());
	}

	Device->ScaleResolution(ScreenFactor, DPIScaleFactor, bClampWindowSizeState);

	UpdateWindow();
}

float SPIEPreviewWindow::GetWindowScaleFactor() const
{
	float ScaleFactor = 1.0f;

	if (Device.IsValid())
	{
		ScaleFactor = Device->GetResolutionScale();
	}

	return ScaleFactor;
}

void SPIEPreviewWindow::OnWindowMoved(const TSharedRef<SWindow>& Window)
{
	if (IsManualManageDPIChanges())
	{
		ScaleDeviceToPhisicalSize();
	}

	FVector2D WindowPos = GetPositionInScreen();
	GConfig->SetInt(TEXT("/Script/Engine.MobilePIE"), TEXT("WindowPosX"), FMath::CeilToInt(WindowPos.X), GEngineIni);
	GConfig->SetInt(TEXT("/Script/Engine.MobilePIE"), TEXT("WindowPosY"), FMath::CeilToInt(WindowPos.Y), GEngineIni);
}

void SPIEPreviewWindow::OnDisplayDPIChanged(TSharedRef<SWindow> Window)
{
	if (IsManualManageDPIChanges())
	{
		ScaleDeviceToPhisicalSize();
	}
}

void SPIEPreviewWindow::SetScaleWindowToDeviceSize(const bool bScale)
{
	if (bScale != IsManualManageDPIChanges())
	{
		SetManualManageDPIChanges(bScale);

		if (Device.IsValid())
		{
			Device->SetIgnoreMobileContentScaleFactor(bScale);
		}

		if (IsManualManageDPIChanges())
		{
			ScaleDeviceToPhisicalSize();

			SetOnWindowMoved(FOnWindowMoved::CreateSP(this, &SPIEPreviewWindow::OnWindowMoved));
			HandleDPIChange = FSlateApplication::Get().OnSystemSignalsDPIChanged().AddSP(this, &SPIEPreviewWindow::OnDisplayDPIChanged);
		}
		else
		{
			SetOnWindowMoved(nullptr);

			if (HandleDPIChange.IsValid())
			{
				FSlateApplication::Get().OnSystemSignalsDPIChanged().Remove(HandleDPIChange);
			}
		}
	}
}

void SPIEPreviewWindow::ScaleDeviceToPhisicalSize()
{
	if (Device.IsValid())
	{
		float ScreenFactor;
		float DPIScaleFactor;
		ComputeScaleToDeviceSizeFactor(ScreenFactor, DPIScaleFactor);

		SetDPIScaleFactor(DPIScaleFactor);
		ScaleWindow(ScreenFactor, DPIScaleFactor);
	}
}

void SPIEPreviewWindow::ComputeScaleToDeviceSizeFactor(float& OutScreenFactor, float& OutDPIScaleFactor) const
{
	OutScreenFactor = 1.0f;
	OutDPIScaleFactor = 1.0f;

	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);

	FVector2D WindowPos = GetPositionInScreen();
	int32 PointX = FMath::RoundToInt(WindowPos.X);
	int32 PointY = FMath::RoundToInt(WindowPos.Y);

	FPlatformRect& VirtualDisplayRect = DisplayMetrics.VirtualDisplayRect;
	PointX = FMath::Clamp(PointX, VirtualDisplayRect.Left, VirtualDisplayRect.Right);
	PointY = FMath::Clamp(PointY, VirtualDisplayRect.Top, VirtualDisplayRect.Bottom);

	float RatioMonitorResolution = 1.0f;
	int32 LocalPPI = 0;

	TArray<FMonitorInfo>& MonitorInfoArray = DisplayMetrics.MonitorInfo;
	for (int32 i = 0; i < MonitorInfoArray.Num(); ++i)
	{
		FMonitorInfo& MonitorInfo = MonitorInfoArray[i];

		const int32 PointOffset = 0;
		if (PointX >= MonitorInfo.DisplayRect.Left &&
			PointX <= MonitorInfo.DisplayRect.Right &&
			PointY >= MonitorInfo.DisplayRect.Top &&
			PointY <= MonitorInfo.DisplayRect.Bottom)
		{
			int32 MonitorWidth = MonitorInfo.DisplayRect.Right - MonitorInfo.DisplayRect.Left;
			int32 MonitorHeight = MonitorInfo.DisplayRect.Bottom - MonitorInfo.DisplayRect.Top;

			float MonitorResolutionScale = FMath::Min((float)MonitorWidth / (float)MonitorInfo.NativeWidth, (float)MonitorHeight / (float)MonitorInfo.NativeHeight);

			float NativeRatio = (float)MonitorInfo.NativeWidth / (float)MonitorInfo.NativeHeight;
			float CurrentRatio = (float)MonitorWidth / (float)MonitorHeight;
			float MonitorPixelRatio = NativeRatio / CurrentRatio;

			RatioMonitorResolution = MonitorResolutionScale * MonitorPixelRatio;

			LocalPPI = MonitorInfo.DPI;

			break;
		}
	}

	OutDPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(PointX, PointY);

	const int32 DevicePPI = Device->GetDeviceSpecs()->PPI;
	float PPIRatio = (!!DevicePPI && !!LocalPPI) ? (float)LocalPPI / (float)DevicePPI : 1.0f;
	OutScreenFactor = PPIRatio * RatioMonitorResolution / OutDPIScaleFactor;
}

void SPIEPreviewWindow::RotateWindow()
{
	if (!Device.IsValid())
	{
		return;
	}

	Device->SwitchOrientation(bClampWindowSizeState);

	UpdateWindow();
}

void SPIEPreviewWindow::FlipBezelVisibility()
{
	if (!Device.IsValid())
	{
		return;
	}

	bool bVisible = Device->GetBezelVisibility();
	Device->SetBezelVisibility(!bVisible, bClampWindowSizeState);

	UpdateWindow();
}

bool SPIEPreviewWindow::GetBezelVisibility() const
{
	bool bVisible = false;

	if (Device.IsValid())
	{
		bVisible = Device->GetBezelVisibility();
	}

	return bVisible;
}

void SPIEPreviewWindow::UpdateWindow()
{
	if (!Device.IsValid())
	{
		return;
	}

	// compute position window position
	// try to maintain its old top left corner position, but while keeping it inside the desktop area
	FVector2D WindowPos = GetPositionInScreen();
	int32 PosX = (int32)WindowPos.X;
	int32 PosY = (int32)WindowPos.Y;

	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);

	if (PosX + Device->GetWindowWidth() > DisplayMetrics.VirtualDisplayRect.Right)
	{
		PosX = DisplayMetrics.VirtualDisplayRect.Right - Device->GetWindowWidth();
	}
	PosX = FMath::Max(DisplayMetrics.VirtualDisplayRect.Left, PosX);

	if (PosY + Device->GetWindowHeight() > DisplayMetrics.VirtualDisplayRect.Bottom)
	{
		PosY = DisplayMetrics.VirtualDisplayRect.Bottom - Device->GetWindowHeight() - SWindowDefs::DefaultTitleBarSize;
	}
	PosY = FMath::Max(DisplayMetrics.VirtualDisplayRect.Top, PosY);

	ReshapeWindow(FVector2D(PosX, PosY), FVector2D(Device->GetWindowWidth(), Device->GetWindowHeight()));

	// offset the viewport widget into its correct location
	SetSceneViewportPadding(Device->GetViewportMargin());

	// bezel orientation depends on the window size so we need to call it after ReshapeWindow()
	ComputeBezelOrientation();
}

void SPIEPreviewWindow::SetDevice(TSharedPtr<FPIEPreviewDevice> InDevice)
{
	Device = InDevice;

	if (Device.IsValid())
	{
		FMargin ViewportOffset = Device->GetViewportMargin();
		SetSceneViewportPadding(ViewportOffset);

		CreatePIEPreviewBezelOverlay(InDevice->GetBezelTexture());

		CreateMenuToolBar();
	}
}

int32 SPIEPreviewWindow::GetDefaultTitleBarSize()
{
	return SWindowDefs::DefaultTitleBarSize;
}

#endif