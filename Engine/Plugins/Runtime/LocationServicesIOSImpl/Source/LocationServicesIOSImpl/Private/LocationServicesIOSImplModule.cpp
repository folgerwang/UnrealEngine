// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LocationServicesIOSImplModule.h"
#include "LocationServicesIOSImpl.h"
#include "LocationServicesBPLibrary.h"

IMPLEMENT_MODULE(FLocationServicesIOSImplModule, LocationServicesIOSImpl)

void FLocationServicesIOSImplModule::StartupModule()
{
	ImplInstance = NewObject<ULocationServicesIOSImpl>();
	ULocationServices::SetLocationServicesImpl(ImplInstance);
}

void FLocationServicesIOSImplModule::ShutdownModule()
{
	ULocationServices::ClearLocationServicesImpl();
	
	ImplInstance = NULL;
}