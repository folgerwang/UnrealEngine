// Copyright (c) Microsoft Corporation. All rights reserved.

#include "WindowsMixedRealityDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "PropertyHandle.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SButton.h"

#include "WindowsMixedRealityRuntimeSettings.h"
#include "WindowsMixedRealityStatics.h"

#define LOCTEXT_NAMESPACE "FWindowsMixedRealityDetails"

TSharedRef<IDetailCustomization> FWindowsMixedRealityDetails::MakeInstance()
{
	return MakeShareable(new FWindowsMixedRealityDetails);
}

void FWindowsMixedRealityDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& remotingCategory = DetailBuilder.EditCategory(TEXT("Holographic Remoting"));
	remotingCategory.AddCustomRow(LOCTEXT("Connect Button", "Connect Button"))
		[
			SNew(SButton)
			.Text(LOCTEXT("Connect", "Connect"))
			.OnClicked_Raw(this, &FWindowsMixedRealityDetails::OnConnectButtonClicked)
		];
	
	remotingCategory.AddCustomRow(LOCTEXT("Disconnect Button", "Disconnect Button"))
		[
			SNew(SButton)
			.Text(LOCTEXT("Disconnect", "Disconnect"))
			.OnClicked_Raw(this, &FWindowsMixedRealityDetails::OnDisconnectButtonClicked)
		];
}

FReply FWindowsMixedRealityDetails::OnConnectButtonClicked()
{
	UWindowsMixedRealityRuntimeSettings* settings = UWindowsMixedRealityRuntimeSettings::Get();

	FString ip = settings->RemoteHoloLensIP;
	UE_LOG(LogTemp, Log, TEXT("Connecting to: %s"), *ip);

	unsigned int bitrate = settings->MaxBitrate;

	WindowsMixedReality::FWindowsMixedRealityStatics::ConnectToRemoteHoloLens(ip, bitrate);

	return FReply::Handled();
}

FReply FWindowsMixedRealityDetails::OnDisconnectButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("Disconnecting from remote HoloLens"));
	WindowsMixedReality::FWindowsMixedRealityStatics::DisconnectFromRemoteHoloLens();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
