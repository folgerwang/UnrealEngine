// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsPlatformHttp.h"
#include "HttpWinInet.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"
#include "Curl/CurlHttp.h"
#include "Curl/CurlHttpManager.h"

#include "Windows/AllowWindowsPlatformTypes.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpragma-pack"
#endif

// recreating parts of winhttp.h in here because winhttp.h and wininet.h do not play well with each other.
#if defined(_WIN64)
#include <pshpack8.h>
#else
#include <pshpack4.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(_WINHTTP_INTERNAL_)
#define WINHTTPAPI DECLSPEC_IMPORT
#else
#define WINHTTPAPI

#endif

// WinHttpOpen dwAccessType values (also for WINHTTP_PROXY_INFO::dwAccessType)
#define WINHTTP_ACCESS_TYPE_DEFAULT_PROXY               0
#define WINHTTP_ACCESS_TYPE_NO_PROXY                    1
#define WINHTTP_ACCESS_TYPE_NAMED_PROXY                 3
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY             4

	typedef struct
	{
		DWORD  dwAccessType;      // see WINHTTP_ACCESS_* types below
		LPWSTR lpszProxy;         // proxy server list
		LPWSTR lpszProxyBypass;   // proxy bypass list
	}
	WINHTTP_PROXY_INFO, *LPWINHTTP_PROXY_INFO;
	WINHTTPAPI BOOL WINAPI WinHttpGetDefaultProxyConfiguration(WINHTTP_PROXY_INFO* pProxyInfo);

	typedef struct
	{
		BOOL    fAutoDetect;
		LPWSTR  lpszAutoConfigUrl;
		LPWSTR  lpszProxy;
		LPWSTR  lpszProxyBypass;
	} WINHTTP_CURRENT_USER_IE_PROXY_CONFIG;
	WINHTTPAPI BOOL WINAPI WinHttpGetIEProxyConfigForCurrentUser(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* pProxyConfig);

#if defined(__cplusplus)
}
#endif

#include <poppack.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "Windows/HideWindowsPlatformTypes.h"


bool bUseCurl = true;

static bool IsUnsignedInteger(const FString& InString)
{
	bool bResult = true;
	for (auto CharacterIter : InString)
	{
		if (!FChar::IsDigit(CharacterIter))
		{
			bResult = false;
			break;
		}
	}
	return bResult;
}

static bool IsValidIPv4Address(const FString& InString)
{
	bool bResult = false;

	FString Temp;
	FString AStr, BStr, CStr, DStr, PortStr;

	bool bWasPatternMatched = false;
	if (InString.Split(TEXT("."), &AStr, &Temp))
	{
		if (Temp.Split(TEXT("."), &BStr, &Temp))
		{
			if (Temp.Split(TEXT("."), &CStr, &Temp))
			{
				if (Temp.Split(TEXT(":"), &DStr, &PortStr))
				{
					bWasPatternMatched = true;
				}
			}
		}
	}

	if (bWasPatternMatched)
	{
		if (IsUnsignedInteger(AStr) && IsUnsignedInteger(BStr) && IsUnsignedInteger(CStr) && IsUnsignedInteger(DStr) && IsUnsignedInteger(PortStr))
		{
			uint32 A, B, C, D, Port;
			LexFromString(A, *AStr);
			LexFromString(B, *BStr);
			LexFromString(C, *CStr);
			LexFromString(D, *DStr);
			LexFromString(Port, *PortStr);

			if (A < 256 && B < 256 && C < 256 && D < 256 && Port < 65536)
			{
				bResult = true;
			}
		}
	}

	return bResult;
}

void FWindowsPlatformHttp::Init()
{
	if (GConfig)
	{
		bool bUseCurlConfigValue = false;
		if (GConfig->GetBool(TEXT("Networking"), TEXT("UseLibCurl"), bUseCurlConfigValue, GEngineIni))
		{
			bUseCurl = bUseCurlConfigValue;
		}
	}

	// allow override on command line
	FString HttpMode;
	if (FParse::Value(FCommandLine::Get(), TEXT("HTTP="), HttpMode) &&
		(HttpMode.Equals(TEXT("WinInet"), ESearchCase::IgnoreCase)))
	{
		bUseCurl = false;
	}

#if WITH_LIBCURL
	if (bUseCurl)
	{
		FCurlHttpManager::InitCurl();
	}
#endif
}

void FWindowsPlatformHttp::Shutdown()
{
#if WITH_LIBCURL
	if (bUseCurl)
	{
		FCurlHttpManager::ShutdownCurl();
	}
	else
#endif
	{
		FWinInetConnection::Get().ShutdownConnection();
	}
}

FHttpManager * FWindowsPlatformHttp::CreatePlatformHttpManager()
{
#if WITH_LIBCURL
	if (bUseCurl)
	{
		return new FCurlHttpManager();
	}
#endif
	// allow default to be used
	return NULL;
}

IHttpRequest* FWindowsPlatformHttp::ConstructRequest()
{
#if WITH_LIBCURL
	if (bUseCurl)
	{
		return new FCurlHttpRequest();
	}
	else
#endif
	{
		return new FHttpRequestWinInet();
	}
}

FString FWindowsPlatformHttp::GetMimeType(const FString& FilePath)
{
	FString MimeType = TEXT("application/unknown");
	const FString FileExtension = FPaths::GetExtension(FilePath, true);

	HKEY hKey;
	if ( ::RegOpenKeyEx(HKEY_CLASSES_ROOT, *FileExtension, 0, KEY_READ, &hKey) == ERROR_SUCCESS )
	{
		TCHAR MimeTypeBuffer[128];
		DWORD MimeTypeBufferSize = sizeof(MimeTypeBuffer);
		DWORD KeyType = 0;

		if ( ::RegQueryValueEx(hKey, TEXT("Content Type"), NULL, &KeyType, (BYTE*)MimeTypeBuffer, &MimeTypeBufferSize) == ERROR_SUCCESS && KeyType == REG_SZ )
		{
			MimeType = MimeTypeBuffer;
		}

		::RegCloseKey(hKey);
	}

	return MimeType;
}

TOptional<FString> FWindowsPlatformHttp::GetOperatingSystemProxyAddress()
{
	FString ProxyAddress;

	// Retrieve the default proxy configuration.
	WINHTTP_PROXY_INFO DefaultProxyInfo;
	memset(&DefaultProxyInfo, 0, sizeof(DefaultProxyInfo));
	WinHttpGetDefaultProxyConfiguration(&DefaultProxyInfo);

	if (DefaultProxyInfo.lpszProxy != nullptr)
	{
		FString TempProxy(DefaultProxyInfo.lpszProxy);
		if (IsValidIPv4Address(TempProxy))
		{
			ProxyAddress = MoveTemp(TempProxy);
		}
		else
		{
			if (TempProxy.Split(TEXT("https="), nullptr, &TempProxy))
			{
				TempProxy.Split(TEXT(";"), &TempProxy, nullptr);
				if (IsValidIPv4Address(TempProxy))
				{
					ProxyAddress = MoveTemp(TempProxy);
				}
			}
		}
	}

	// Look for the proxy setting for the current user. Charles proxies count in here.
	if (ProxyAddress.IsEmpty())
	{
		WINHTTP_CURRENT_USER_IE_PROXY_CONFIG IeProxyInfo;
		memset(&IeProxyInfo, 0, sizeof(IeProxyInfo));
		WinHttpGetIEProxyConfigForCurrentUser(&IeProxyInfo);

		if (IeProxyInfo.lpszProxy != nullptr)
		{
			FString TempProxy(IeProxyInfo.lpszProxy);
			if (IsValidIPv4Address(TempProxy))
			{
				ProxyAddress = MoveTemp(TempProxy);
			}
			else
			{
				if (TempProxy.Split(TEXT("https="), nullptr, &TempProxy))
				{
					TempProxy.Split(TEXT(";"), &TempProxy, nullptr);
					if (IsValidIPv4Address(TempProxy))
					{
						ProxyAddress = MoveTemp(TempProxy);
					}
				}
			}
		}
	}
	return ProxyAddress;
}

bool FWindowsPlatformHttp::IsOperatingSystemProxyInformationSupported()
{
	return true;
}
