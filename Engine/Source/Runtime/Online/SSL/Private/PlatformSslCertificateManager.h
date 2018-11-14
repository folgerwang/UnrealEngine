// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if WITH_SSL

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformSslCertificateManager.h"
#elif PLATFORM_PS4
#include "SslCertificateManager.h"
using FPlatformSslCertificateManager = FSslCertificateManager;
#elif PLATFORM_XBOXONE
#include "SslCertificateManager.h"
using FPlatformSslCertificateManager = FSslCertificateManager;
#elif PLATFORM_MAC
#include "SslCertificateManager.h"
using FPlatformSslCertificateManager = FSslCertificateManager;
#elif PLATFORM_IOS
#include "SslCertificateManager.h"
using FPlatformSslCertificateManager = FSslCertificateManager;
#elif PLATFORM_ANDROID
#include "Android/AndroidPlatformSslCertificateManager.h"
#elif PLATFORM_HTML5
#include "SslCertificateManager.h"
using FPlatformSslCertificateManager = FSslCertificateManager;
#elif PLATFORM_UNIX
#include "Unix/UnixPlatformSslCertificateManager.h"
#elif PLATFORM_SWITCH
#include "SslCertificateManager.h"
using FPlatformSslCertificateManager = FSslCertificateManager;
#else
#error Unknown platform
#endif

#endif // WITH_SSL
