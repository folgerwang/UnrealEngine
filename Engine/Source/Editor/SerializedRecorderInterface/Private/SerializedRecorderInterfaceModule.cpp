// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ISerializedRecorderInterfaceModule.h"
#include "ISerializedRecorder.h"

FName ISerializedRecorder::ModularFeatureName(TEXT("ModularFeature_SerialzedRecorder"));

class FSerializedRecorderInterfaceModule : public ISerializedRecorderInterfaceModule
{
public:
	
	// IModuleInterface interface
	virtual void StartupModule() override
	{
		
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

private:

};

IMPLEMENT_MODULE(FSerializedRecorderInterfaceModule, SerializedRecorderInterface);
