// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VirtualCamera.h"


#include "ConcertVirtualCamera.h"

DEFINE_LOG_CATEGORY(LogVirtualCamera);

const IVirtualCameraModule& IVirtualCameraModule::Get()
{
	static const FName ModuleName = TEXT("VirtualCamera");
	return FModuleManager::Get().GetModuleChecked<IVirtualCameraModule>(ModuleName);
}


/**
 *
 */
class FVirtualCameraModuleImpl : public IVirtualCameraModule
{
public:
	virtual FConcertVirtualCameraManager* GetConcertVirtualCameraManager() const override
	{
		return ConcertManager.Get();
	}

private:
	virtual void StartupModule() override
	{
		ConcertManager = MakeUnique<FConcertVirtualCameraManager>();
	}

	virtual void ShutdownModule() override
	{
		ConcertManager.Reset();
	}

	TUniquePtr<FConcertVirtualCameraManager> ConcertManager;
};


IMPLEMENT_MODULE(FVirtualCameraModuleImpl, VirtualCamera)