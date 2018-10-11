// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SocketSubsystemHTML5.h"
#include "SocketSubsystemModule.h"
#include "Modules/ModuleManager.h"


FSocketSubsystemHTML5* FSocketSubsystemHTML5::SocketSingleton = NULL;

FName CreateSocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
 	FName SubsystemName(TEXT("HTML5"));
	// Create and register our singleton factor with the main online subsystem for easy access
	FSocketSubsystemHTML5* SocketSubsystem = FSocketSubsystemHTML5::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		SocketSubsystemModule.RegisterSocketSubsystem(SubsystemName, SocketSubsystem);
		return SubsystemName;
	}
	else
	{
		FSocketSubsystemHTML5::Destroy();
		return NAME_None;
	}

}

void DestroySocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	SocketSubsystemModule.UnregisterSocketSubsystem(FName(TEXT("HTML5")));
	FSocketSubsystemHTML5::Destroy();
}

/** 
 * Singleton interface for the Android socket subsystem 
 * @return the only instance of the Android socket subsystem
 */
FSocketSubsystemHTML5* FSocketSubsystemHTML5::Create()
{
	if (SocketSingleton == NULL)
	{
		SocketSingleton = new FSocketSubsystemHTML5();
	}

	return SocketSingleton; 
}

/** 
 * Destroy the singleton Android socket subsystem
 */
void FSocketSubsystemHTML5::Destroy()
{
	if (SocketSingleton != NULL)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = NULL;
	}
}


bool FSocketSubsystemHTML5::Init(FString& Error)
{
	return true;
}

/**
 * Performs HTML5 specific socket clean up
 */
void FSocketSubsystemHTML5::Shutdown(void)
{
}


/**
 * @return Whether the device has a properly configured network device or not
 */
bool FSocketSubsystemHTML5::HasNetworkDevice()
{
	return true;
}

/**
 * Translates an ESocketAddressInfoFlags into a value usable by getaddrinfo
 */
int32 FSocketSubsystemHTML5::GetAddressInfoHintFlag(EAddressInfoFlags InFlags) const
{
	// As of writing, emscripten does not support AI_ADDRCONFIG. It is marked as an usable flag,
	// however if it is set, GAI will fail out with a bad name flag.
	// As such, remove the flag from any potential queries.
	EAddressInfoFlags ModifiedInFlags = (InFlags & ~EAddressInfoFlags::OnlyUsableAddresses);
	return FSocketSubsystemBSD::GetAddressInfoHintFlag(ModifiedInFlags);
}
