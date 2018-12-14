// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

namespace CryptoKeys
{
	CRYPTOKEYS_API void GenerateEncryptionKey(FString& OutBase64Key);
}