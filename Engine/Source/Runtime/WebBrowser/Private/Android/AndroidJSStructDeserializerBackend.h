// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if USE_ANDROID_JNI

#include "CoreMinimal.h"
#include "AndroidJSScripting.h"
#include "Backends/JsonStructDeserializerBackend.h"
#include "Serialization/MemoryReader.h"

class FAndroidJSStructDeserializerBackend
	: public FJsonStructDeserializerBackend
{
public:
	FAndroidJSStructDeserializerBackend(FAndroidJSScriptingRef InScripting, const FString& JsonString);

	virtual bool ReadProperty( UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex ) override;

private:
	FAndroidJSScriptingRef Scripting;
	TArray<uint8> JsonData;
	FMemoryReader Reader;
};

#endif // USE_ANDROID_JNI