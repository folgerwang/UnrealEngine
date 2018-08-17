// Copyright (c) Microsoft Corporation. All rights reserved.

#include "WindowsMixedRealitySpatialInput.h"
#include "IWindowsMixedRealitySpatialInputPlugin.h"

//---------------------------------------------------
// Microsoft Windows MixedReality SpatialInput plugin
//---------------------------------------------------

class FWindowsMixedRealitySpatialInputPlugin :
	public IWindowsMixedRealitySpatialInputPlugin
{
public:
	FWindowsMixedRealitySpatialInputPlugin()
	{
	}

	virtual void StartupModule() override
	{
		IInputDeviceModule::StartupModule();
	}

	virtual TSharedPtr<class IInputDevice> CreateInputDevice(
		const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;
};

TSharedPtr<class IInputDevice> FWindowsMixedRealitySpatialInputPlugin::CreateInputDevice(
	const TSharedRef<FGenericApplicationMessageHandler> & InMessageHandler)
{
	TSharedPtr< WindowsMixedReality::FWindowsMixedRealitySpatialInput > WindowsMixedRealitySpatialInput(
		new WindowsMixedReality::FWindowsMixedRealitySpatialInput(InMessageHandler));

	return WindowsMixedRealitySpatialInput;
}

IMPLEMENT_MODULE(FWindowsMixedRealitySpatialInputPlugin, WindowsMixedRealitySpatialInput)
