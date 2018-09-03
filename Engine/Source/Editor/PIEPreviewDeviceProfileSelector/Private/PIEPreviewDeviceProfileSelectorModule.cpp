// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PIEPreviewDeviceProfileSelectorModule.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "PIEPreviewDeviceSpecification.h"
#include "JsonObjectConverter.h"
#include "MaterialShaderQualitySettings.h"
#include "RHI.h"
#include "Framework/Docking/TabManager.h"
#include "CoreGlobals.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Culture.h"
#include "PIEPreviewWindowStyle.h"
#include "PIEPreviewDevice.h"
#include "PIEPreviewWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "UnrealEngine.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPIEPreviewDevice, Log, All); 
DEFINE_LOG_CATEGORY(LogPIEPreviewDevice);
IMPLEMENT_MODULE(FPIEPreviewDeviceModule, PIEPreviewDeviceProfileSelector);

void FPIEPreviewDeviceModule::StartupModule()
{
}

void FPIEPreviewDeviceModule::ShutdownModule()
{
}

FString const FPIEPreviewDeviceModule::GetRuntimeDeviceProfileName()
{
	if (!bInitialized)
	{
		InitPreviewDevice();
	}

	return DeviceProfile;
}

void FPIEPreviewDeviceModule::InitPreviewDevice()
{
	bInitialized = true;

	if (ReadDeviceSpecification())
	{
		Device->ApplyRHIPrerequisitesOverrides();
		DeviceProfile = Device->GetProfile();
	}
}

TSharedRef<SWindow> FPIEPreviewDeviceModule::CreatePIEPreviewDeviceWindow(FVector2D ClientSize, FText WindowTitle, EAutoCenter AutoCenterType, FVector2D ScreenPosition, TOptional<float> MaxWindowWidth, TOptional<float> MaxWindowHeight)
{
	if (ScreenPosition.IsNearlyZero())
	{
		int32 WinX, WinY;
		bool bFoundX = GConfig->GetInt(TEXT("/Script/Engine.MobilePIE"), TEXT("WindowPosX"), WinX, GEngineIni);
		bool bFoundY = GConfig->GetInt(TEXT("/Script/Engine.MobilePIE"), TEXT("WindowPosY"), WinY, GEngineIni);

		if (bFoundX && bFoundY)
		{
			ScreenPosition.X = WinX;
			ScreenPosition.Y = WinY;

			AutoCenterType = EAutoCenter::None;
		}
	}

	FPIEPreviewWindowCoreStyle::InitializePIECoreStyle();

	static FWindowStyle BackgroundlessStyle = FCoreStyle::Get().GetWidgetStyle<FWindowStyle>("Window");
	BackgroundlessStyle.SetBackgroundBrush(FSlateNoResource());
	TSharedRef<SPIEPreviewWindow> Window = SNew(SPIEPreviewWindow, Device)
		.Type(EWindowType::GameWindow)
		.Style(&BackgroundlessStyle)
		.ClientSize(ClientSize)
		.Title(WindowTitle)
		.AutoCenter(AutoCenterType)
		.ScreenPosition(ScreenPosition)
		.MaxWidth(MaxWindowWidth)
		.MaxHeight(MaxWindowHeight)
		.FocusWhenFirstShown(true)
		.SaneWindowPlacement(AutoCenterType == EAutoCenter::None)
		.UseOSWindowBorder(false)
		.CreateTitleBar(true)
		.ShouldPreserveAspectRatio(true)
		.LayoutBorder(FMargin(0))
		.SizingRule(ESizingRule::FixedSize)
		.HasCloseButton(true)
		.SupportsMinimize(true)
		.SupportsMaximize(false)
		.bManualManageDPI(false);

 	WindowWPtr = Window;

	return Window;
}

void FPIEPreviewDeviceModule::UpdateDisplayResolution()
{
	if (!Device.IsValid())
	{
		return;
	}

	const int32 ClientWidth = Device->GetWindowClientWidth();
	const int32 ClientHeight = Device->GetWindowClientHeight();

	FSystemResolution::RequestResolutionChange(ClientWidth, ClientHeight, EWindowMode::Windowed);
	IConsoleManager::Get().CallAllConsoleVariableSinks();
}

void FPIEPreviewDeviceModule::PrepareDeviceDisplay()
{
	TSharedPtr<SPIEPreviewWindow> WindowPtr = WindowWPtr.Pin();

	if (!WindowPtr.IsValid() && !Device.IsValid())
	{
		return;
	}

	FSlateApplication::Get().SetGameIsFakingTouchEvents(true);

	WindowPtr->SetScaleWindowToDeviceSize(true);

	UpdateDisplayResolution();
}

void FPIEPreviewDeviceModule::ApplyPreviewDeviceState()
{
	if (!Device.IsValid())
	{
		return;
	}

	// TODO: Localization
	FString AppTitle = FGlobalTabmanager::Get()->GetApplicationTitle().ToString() + "Previewing: "+ PreviewDevice;
	FGlobalTabmanager::Get()->SetApplicationTitle(FText::FromString(AppTitle));

	int32 TitleBarSize = SPIEPreviewWindow::GetDefaultTitleBarSize();
	Device->SetupDevice(TitleBarSize);

	// need to call this before the actual window is created in order override the window mode to EWindowMode::Windowed
	UpdateDisplayResolution();
}

const FPIEPreviewDeviceContainer& FPIEPreviewDeviceModule::GetPreviewDeviceContainer()
{
	if (!EnumeratedDevices.GetRootCategory().IsValid())
	{
		EnumeratedDevices.EnumerateDeviceSpecifications(GetDeviceSpecificationContentDir());
	}
	return EnumeratedDevices;
}

FString FPIEPreviewDeviceModule::GetDeviceSpecificationContentDir()
{
	return FPaths::EngineContentDir() / TEXT("Editor") / TEXT("PIEPreviewDeviceSpecs");
}

FString FPIEPreviewDeviceModule::FindDeviceSpecificationFilePath(const FString& SearchDevice)
{
	const FPIEPreviewDeviceContainer& PIEPreviewDeviceContainer = GetPreviewDeviceContainer();
	FString FoundPath;

	int32 FoundIndex;
	if (PIEPreviewDeviceContainer.GetDeviceSpecifications().Find(SearchDevice, FoundIndex))
	{
		TSharedPtr<FPIEPreviewDeviceContainerCategory> SubCategory = PIEPreviewDeviceContainer.FindDeviceContainingCategory(FoundIndex);
		if(SubCategory.IsValid())
		{
			FoundPath = SubCategory->GetSubDirectoryPath() / SearchDevice + ".json";
		}
	}
	return FoundPath;

}

bool FPIEPreviewDeviceModule::ReadDeviceSpecification()
{
	Device = nullptr;

	if (!FParse::Value(FCommandLine::Get(), GetPreviewDeviceCommandSwitch(), PreviewDevice))
	{
		return false;
	}

	const FString Filename = FindDeviceSpecificationFilePath(PreviewDevice);

	FString Json;
	if (FFileHelper::LoadFileToString(Json, *Filename))
	{
		TSharedPtr<FJsonObject> RootObject;
		TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(Json);
		if (FJsonSerializer::Deserialize(JsonReader, RootObject) && RootObject.IsValid())
		{
			// We need to initialize FPIEPreviewDeviceSpecifications early as device profiles need to be evaluated before ProcessNewlyLoadedUObjects can be called.
			CreatePackage(nullptr, TEXT("/Script/PIEPreviewDeviceProfileSelector"));

			Device = MakeShareable(new FPIEPreviewDevice());

			if (!FJsonObjectConverter::JsonAttributesToUStruct(RootObject->Values, FPIEPreviewDeviceSpecifications::StaticStruct(), Device->GetDeviceSpecs().Get(), 0, 0))
			{
				Device = nullptr;
			}
		}
	}
	bool bValidDeviceSpec = Device.IsValid();
	if (!bValidDeviceSpec)
	{
		UE_LOG(LogPIEPreviewDevice, Warning, TEXT("Could not load device specifications for preview target device '%s'"), *PreviewDevice);
	}

	return bValidDeviceSpec;
}
