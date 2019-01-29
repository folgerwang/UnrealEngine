// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DeviceBrowserDefaultPlatformWidgetCreator.h"
#include "Internationalization/Text.h"
#include "Misc/MessageDialog.h"
#include "Misc/CoreMisc.h"
#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "EditorStyleSet.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "PlatformInfo.h"
#include "SDeviceBrowserDefaultPlatformAddWidget.h"

#define LOCTEXT_NAMESPACE "FDeviceBrowserDefaultPlatformWidgetCreator"

bool FDeviceBrowserDefaultPlatformWidgetCreator::IsAddDeviceInputValid(const FString& InPlatformName, const TSharedPtr<SWidget>& UserData)
{
	SDeviceBrowserDefaultPlatformAddWidget& CustomWidget = static_cast<SDeviceBrowserDefaultPlatformAddWidget&>(*UserData);

	ITargetPlatform* Platform = GetTargetPlatformManager()->FindTargetPlatform(*InPlatformName);
	check(Platform);

	FString TextCheck = CustomWidget.DeviceNameTextBox->GetText().ToString();
	TextCheck.TrimStartAndEndInline();

	if (!TextCheck.IsEmpty())
	{
		if (!Platform->RequiresUserCredentials())
		{
			return true;
		}

		// check user/password as well
		TextCheck = CustomWidget.UserNameTextBox->GetText().ToString();
		TextCheck.TrimStartAndEndInline();

		if (!TextCheck.IsEmpty())
		{
			// do not trim the password
			return !CustomWidget.UserPasswordTextBox->GetText().ToString().IsEmpty();
		}
	}

	return false;
}

void FDeviceBrowserDefaultPlatformWidgetCreator::AddDevice(const FString& InPlatformName, const TSharedPtr<SWidget>& UserData)
{
	SDeviceBrowserDefaultPlatformAddWidget& CustomWidget = static_cast<SDeviceBrowserDefaultPlatformAddWidget&>(*UserData);

	ITargetPlatform* Platform = GetTargetPlatformManager()->FindTargetPlatform(*InPlatformName);
	check(Platform);

	FString DeviceIdString = CustomWidget.DeviceIdTextBox->GetText().ToString();
	bool bAdded = Platform->AddDevice(DeviceIdString, false);
	if (bAdded)
	{
		// pass credentials to the newly added device
		if (Platform->RequiresUserCredentials())
		{
			// We cannot guess the device id, so we have to look it up by name
			TArray<ITargetDevicePtr> Devices;
			Platform->GetAllDevices(Devices);
			for (ITargetDevicePtr Device : Devices)
			{
				if (Device.IsValid() && Device->GetId().GetDeviceName() == DeviceIdString)
				{
					FString UserNameString = CustomWidget.UserNameTextBox->GetText().ToString();
					FString UserPassString = CustomWidget.UserPasswordTextBox->GetText().ToString();

					Device->SetUserCredentials(UserNameString, UserPassString);
				}
			}
		}

		CustomWidget.DeviceIdTextBox->SetText(FText::GetEmpty());
		CustomWidget.DeviceNameTextBox->SetText(FText::GetEmpty());
		CustomWidget.UserNameTextBox->SetText(FText::GetEmpty());
		CustomWidget.UserPasswordTextBox->SetText(FText::GetEmpty());
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DeviceAdderFailedToAddDeviceMessage", "Failed to add the device!"));
	}
}

TSharedPtr<SWidget> FDeviceBrowserDefaultPlatformWidgetCreator::CreateAddDeviceWidget(const FString& InPlatformName)
{
	return SNew(SDeviceBrowserDefaultPlatformAddWidget, InPlatformName);
}

TSharedPtr<SWidget> FDeviceBrowserDefaultPlatformWidgetCreator::CreateDeviceInfoWidget(const FString& InPlatformName, const ITargetDevicePtr& InDevice)
{
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
