// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Widgets/SWindow.h"
#include "PIEPreviewWindowTitleBar.h"
#include "PIEPreviewDevice.h"

class PIEPREVIEWDEVICEPROFILESELECTOR_API SPIEPreviewWindow
	: public SWindow
{
public:
	/**
	* Default constructor. Use SNew(SPIEPreviewWindow) instead.
	*/
	SPIEPreviewWindow();

	void Construct(const FArguments& InArgs, TSharedPtr<FPIEPreviewDevice> InDevice);

	// call this to 
	void SetSceneViewportPadding(const FMargin& Margin);

	// sets the device displayed by this window
	void SetDevice(TSharedPtr<class FPIEPreviewDevice> InDevice);

	// retrieves the display device
	TSharedPtr<class FPIEPreviewDevice> GetDevice();

	// call to perform resolution scaling on the device
	void ScaleWindow(const float ScaleFactor, const float BezelFactor = 1.0f);

	// call to retrieve device's current resolution scale factor
	float GetWindowScaleFactor() const;

	// call to rotate the window emulating a rotation of the physical device
	void RotateWindow();

	// wrapper function that will query the same functionality on the device
	bool IsRotationAllowed() const;

	// call to enable/disable bezel visibility
	void FlipBezelVisibility();

	// call to determine whether or not the phone bezel is displayed 
	bool GetBezelVisibility() const;

	static int32 GetDefaultTitleBarSize();

	// call with a true argument to resize the window so it matches the device physical size
	void SetScaleWindowToDeviceSize(const bool bScale);

	// call with a true argument to restrict the window size to the desktop size
	void SetClampWindowSize(const bool bClamp);
	bool IsClampingWindowSize() const;

private:
	virtual EHorizontalAlignment GetTitleAlignment() override;
	virtual TSharedRef<SWidget> MakeWindowTitleBar(const TSharedRef<SWindow>& Window, const TSharedPtr<SWidget>& CenterContent, EHorizontalAlignment CenterContentAlignment) override;

	// this will properly scale and rotate the bezel to match the orientation of the device
	void ComputeBezelOrientation();

	// function that adds a widget to display the device's bezel
	void CreatePIEPreviewBezelOverlay(UTexture2D* pBezelImage);

	// function used create a menu toolbar
	void CreateMenuToolBar();

	/** creates and returns the settings menu */
	TSharedRef<SWidget> BuildSettingsMenu();

	/** helper function used to create a menu item formed from a description text and a checkbox */
	/** @ IsCheckedFunction - computes the state of the checkbox */
	/** @ ExecuteActionFunction - function to be called when this menu item is clicked */
	void CreateMenuEntry(class FMenuBuilder& MenuBuilder, FText&& TextEntry, TFunction<ECheckBoxState()>&& IsCheckedFunction, TFunction<void()>&& ExecuteActionFunction);

	// call to compute window size, position on screen and bezel orientation
	void UpdateWindow();

	// call function to make the size of this window match the physical device size
	void ScaleDeviceToPhisicalSize();

	// utility function that will compute a screen and DPI scale factor needed to scale the display window to the physical device size
	void ComputeScaleToDeviceSizeFactor(float& OutScreenFactor, float& OutDPIScaleFactor) const;

	// callback used when DPI value varies, useful when we need to constrain the window to the physical device size
	void OnWindowMoved(const TSharedRef<SWindow>& Window);

	// callback used when DPI value varies, useful when we need to constrain the window to the physical device size
	void OnDisplayDPIChanged(TSharedRef<SWindow> Window);

protected:
	// brush created to display the bezel
	FSlateBrush BezelBrush;

	// pointer to the actual bezel image
	TSharedPtr<SImage> BezelImage;

	// pointer to the device that this window will display
	TSharedPtr<class FPIEPreviewDevice> Device;

	float CachedScaleToDeviceFactor = 0.0f;
	float CachedDPIScaleFactor = 0.0f;

	/** when true the window size will be restricted to the desktop size  */
	bool bClampWindowSizeState = true;

	FDelegateHandle HandleDPIChange;
};

FORCEINLINE bool SPIEPreviewWindow::IsRotationAllowed() const
{
	if (!Device.IsValid())
	{
		return false;
	}

	return Device->IsRotationAllowed();
}

FORCEINLINE TSharedPtr<class FPIEPreviewDevice> SPIEPreviewWindow::GetDevice()
{
	return Device;
}

FORCEINLINE void SPIEPreviewWindow::SetClampWindowSize(const bool bClamp)
{
	bClampWindowSizeState = bClamp;
}

FORCEINLINE bool SPIEPreviewWindow::IsClampingWindowSize() const
{
	return bClampWindowSizeState;
}

#endif