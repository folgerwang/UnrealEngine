// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Curl/CurlHttpManager.h"

#if WITH_LIBCURL

#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/LocalTimestampDirectoryVisitor.h"
#include "Misc/Paths.h"

#include "Curl/CurlHttpThread.h"
#include "Curl/CurlHttp.h"
#include "Misc/OutputDeviceRedirector.h"
#include "HttpModule.h"

#if WITH_SSL
#include "Modules/ModuleManager.h"
#include "Ssl.h"
#endif

#include "SocketSubsystem.h"
#include "IPAddress.h"

CURLM* FCurlHttpManager::GMultiHandle = NULL;
CURLSH* FCurlHttpManager::GShareHandle = NULL;

FCurlHttpManager::FCurlRequestOptions FCurlHttpManager::CurlRequestOptions;

#if PLATFORM_UNIX	// known to be available for Linux libcurl+libcrypto bundle at least
extern "C"
{
void CRYPTO_get_mem_functions(
		void *(**m)(size_t, const char *, int),
		void *(**r)(void *, size_t, const char *, int),
		void (**f)(void *, const char *, int));
int CRYPTO_set_mem_functions(
		void *(*m)(size_t, const char *, int),
		void *(*r)(void *, size_t, const char *, int),
		void (*f)(void *, const char *, int));
}
#endif // PLATFORM_UNIX

// set functions that will init the memory
namespace LibCryptoMemHooks
{
	void* (*ChainedMalloc)(size_t Size, const char* Src, int Line) = nullptr;
	void* (*ChainedRealloc)(void* Ptr, const size_t Size, const char* Src, int Line) = nullptr;
	void (*ChainedFree)(void* Ptr, const char* Src, int Line) = nullptr;
	bool bMemoryHooksSet = false;

	/** This malloc will init the memory, keeping valgrind happy */
	void* MallocWithInit(size_t Size, const char* Src, int Line)
	{
		void* Result = FMemory::Malloc(Size);
		if (LIKELY(Result))
		{
			FMemory::Memzero(Result, Size);
		}

		return Result;
	}

	/** This realloc will init the memory, keeping valgrind happy */
	void* ReallocWithInit(void* Ptr, const size_t Size, const char* Src, int Line)
	{
		size_t CurrentUsableSize = FMemory::GetAllocSize(Ptr);
		void* Result = FMemory::Realloc(Ptr, Size);
		if (LIKELY(Result) && CurrentUsableSize < Size)
		{
			FMemory::Memzero(reinterpret_cast<uint8 *>(Result) + CurrentUsableSize, Size - CurrentUsableSize);
		}

		return Result;
	}

	/** This realloc will init the memory, keeping valgrind happy */
	void Free(void* Ptr, const char* Src, int Line)
	{
		return FMemory::Free(Ptr);
	}

	void SetMemoryHooks()
	{
		// do not set this in Shipping until we prove that the change in OpenSSL behavior is safe
#if PLATFORM_UNIX && !UE_BUILD_SHIPPING
		CRYPTO_get_mem_functions(&ChainedMalloc, &ChainedRealloc, &ChainedFree);
		CRYPTO_set_mem_functions(MallocWithInit, ReallocWithInit, Free);
#endif // PLATFORM_UNIX && !UE_BUILD_SHIPPING

		bMemoryHooksSet = true;
	}

	void UnsetMemoryHooks()
	{
		// remove our overrides
		if (LibCryptoMemHooks::bMemoryHooksSet)
		{
			// do not set this in Shipping until we prove that the change in OpenSSL behavior is safe
#if PLATFORM_UNIX && !UE_BUILD_SHIPPING
			CRYPTO_set_mem_functions(LibCryptoMemHooks::ChainedMalloc, LibCryptoMemHooks::ChainedRealloc, LibCryptoMemHooks::ChainedFree);
#endif // PLATFORM_UNIX && !UE_BUILD_SHIPPING

			bMemoryHooksSet = false;
			ChainedMalloc = nullptr;
			ChainedRealloc = nullptr;
			ChainedFree = nullptr;
		}
	}
}

void FCurlHttpManager::InitCurl()
{
	if (GMultiHandle != NULL)
	{
		UE_LOG(LogInit, Warning, TEXT("Already initialized multi handle"));
		return;
	}

	int32 CurlInitFlags = CURL_GLOBAL_ALL;
#if WITH_SSL
	// Make sure SSL is loaded so that we can use the shared cert pool, and to globally initialize OpenSSL if possible
	FSslModule& SslModule = FModuleManager::LoadModuleChecked<FSslModule>("SSL");
	if (SslModule.GetSslManager().InitializeSsl())
	{
		// Do not need Curl to initialize its own SSL
		CurlInitFlags = CurlInitFlags & ~(CURL_GLOBAL_SSL);
	}
#endif // #if WITH_SSL

	// Override libcrypt functions to initialize memory since OpenSSL triggers multiple valgrind warnings due to this.
	// Do this before libcurl/libopenssl/libcrypto has been inited.
	LibCryptoMemHooks::SetMemoryHooks();

	CURLcode InitResult = curl_global_init_mem(CurlInitFlags, CurlMalloc, CurlFree, CurlRealloc, CurlStrdup, CurlCalloc);
	if (InitResult == 0)
	{
		curl_version_info_data * VersionInfo = curl_version_info(CURLVERSION_NOW);
		if (VersionInfo)
		{
			UE_LOG(LogInit, Log, TEXT("Using libcurl %s"), ANSI_TO_TCHAR(VersionInfo->version));
			UE_LOG(LogInit, Log, TEXT(" - built for %s"), ANSI_TO_TCHAR(VersionInfo->host));

			if (VersionInfo->features & CURL_VERSION_SSL)
			{
				UE_LOG(LogInit, Log, TEXT(" - supports SSL with %s"), ANSI_TO_TCHAR(VersionInfo->ssl_version));
			}
			else
			{
				// No SSL
				UE_LOG(LogInit, Log, TEXT(" - NO SSL SUPPORT!"));
			}

			if (VersionInfo->features & CURL_VERSION_LIBZ)
			{
				UE_LOG(LogInit, Log, TEXT(" - supports HTTP deflate (compression) using libz %s"), ANSI_TO_TCHAR(VersionInfo->libz_version));
			}

			UE_LOG(LogInit, Log, TEXT(" - other features:"));

#define PrintCurlFeature(Feature)	\
			if (VersionInfo->features & Feature) \
			{ \
			UE_LOG(LogInit, Log, TEXT("     %s"), TEXT(#Feature));	\
			}

			PrintCurlFeature(CURL_VERSION_SSL);
			PrintCurlFeature(CURL_VERSION_LIBZ);

			PrintCurlFeature(CURL_VERSION_DEBUG);
			PrintCurlFeature(CURL_VERSION_IPV6);
			PrintCurlFeature(CURL_VERSION_ASYNCHDNS);
			PrintCurlFeature(CURL_VERSION_LARGEFILE);
			PrintCurlFeature(CURL_VERSION_IDN);
			PrintCurlFeature(CURL_VERSION_CONV);
			PrintCurlFeature(CURL_VERSION_TLSAUTH_SRP);
#undef PrintCurlFeature
		}

		GMultiHandle = curl_multi_init();
		if (NULL == GMultiHandle)
		{
			UE_LOG(LogInit, Fatal, TEXT("Could not initialize create libcurl multi handle! HTTP transfers will not function properly."));
		}

		int32 MaxTotalConnections = 0;
		if (GConfig->GetInt(TEXT("HTTP.Curl"), TEXT("MaxTotalConnections"), MaxTotalConnections, GEngineIni) && MaxTotalConnections > 0)
		{
			const CURLMcode SetOptResult = curl_multi_setopt(GMultiHandle, CURLMOPT_MAX_TOTAL_CONNECTIONS, static_cast<long>(MaxTotalConnections));
			if (SetOptResult != CURLM_OK)
			{
				UE_LOG(LogInit, Warning, TEXT("Failed to set libcurl max total connections options (%d), error %d ('%s')"),
					MaxTotalConnections, static_cast<int32>(SetOptResult), StringCast<TCHAR>(curl_multi_strerror(SetOptResult)).Get());
			}
		}

		GShareHandle = curl_share_init();
		if (NULL != GShareHandle)
		{
			curl_share_setopt(GShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
			curl_share_setopt(GShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
			curl_share_setopt(GShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
		}
		else
		{
			UE_LOG(LogInit, Fatal, TEXT("Could not initialize libcurl share handle!"));
		}
	}
	else
	{
		UE_LOG(LogInit, Fatal, TEXT("Could not initialize libcurl (result=%d), HTTP transfers will not function properly."), (int32)InitResult);
	}

	// Init curl request options
	if (FParse::Param(FCommandLine::Get(), TEXT("noreuseconn")))
	{
		CurlRequestOptions.bDontReuseConnections = true;
	}

	// discover cert location
	if (PLATFORM_UNIX)
	{
		static const char * KnownBundlePaths[] =
		{
			"/etc/pki/tls/certs/ca-bundle.crt",
			"/etc/ssl/certs/ca-certificates.crt",
			"/etc/ssl/ca-bundle.pem",
			nullptr
		};

		for (const char ** CurrentBundle = KnownBundlePaths; *CurrentBundle; ++CurrentBundle)
		{
			FString FileName(*CurrentBundle);
			UE_LOG(LogInit, Log, TEXT(" Libcurl: checking if '%s' exists"), *FileName);

			if (FPaths::FileExists(FileName))
			{
				CurlRequestOptions.CertBundlePath = *CurrentBundle;
				break;
			}
		}
		if (CurlRequestOptions.CertBundlePath == nullptr)
		{
			UE_LOG(LogInit, Log, TEXT(" Libcurl: did not find a cert bundle in any of known locations, TLS may not work"));
		}
	}
#if PLATFORM_ANDROID && USE_ANDROID_FILE
	// used #if here to protect against GExternalFilePath only available on Android
	else
	if (PLATFORM_ANDROID)
	{
		const int32 PathLength = 200;
		static ANSICHAR capath[PathLength] = { 0 };

		// if file does not already exist, create local PEM file with system trusted certificates
		extern FString GExternalFilePath;
		FString PEMFilename = GExternalFilePath / TEXT("ca-bundle.pem");
		if (!FPaths::FileExists(PEMFilename))
		{
			FString Contents;

			IFileManager* FileManager = &IFileManager::Get();
			auto Ar = TUniquePtr<FArchive>(FileManager->CreateFileWriter(*PEMFilename, 0));
			if (Ar)
			{
				// check for override ca-bundle.pem embedded in game content
				FString OverridePEMFilename = FPaths::ProjectContentDir() + TEXT("CurlCertificates/ca-bundle.pem");
				if (FFileHelper::LoadFileToString(Contents, *OverridePEMFilename))
				{
					const TCHAR* StrPtr = *Contents;
					auto Src = StringCast<ANSICHAR>(StrPtr, Contents.Len());
					Ar->Serialize((ANSICHAR*)Src.Get(), Src.Length() * sizeof(ANSICHAR));
				}
				else
				{
					// gather all the files in system certificates directory
					TArray<FString> directoriesToIgnoreAndNotRecurse;
					FLocalTimestampDirectoryVisitor Visitor(FPlatformFileManager::Get().GetPlatformFile(), directoriesToIgnoreAndNotRecurse, directoriesToIgnoreAndNotRecurse, false);
					FileManager->IterateDirectory(TEXT("/system/etc/security/cacerts"), Visitor);

					for (TMap<FString, FDateTime>::TIterator TimestampIt(Visitor.FileTimes); TimestampIt; ++TimestampIt)
					{
						// read and append the certificate file contents
						const FString CertFilename = TimestampIt.Key();
						if (FFileHelper::LoadFileToString(Contents, *CertFilename))
						{
							const TCHAR* StrPtr = *Contents;
							auto Src = StringCast<ANSICHAR>(StrPtr, Contents.Len());
							Ar->Serialize((ANSICHAR*)Src.Get(), Src.Length() * sizeof(ANSICHAR));
						}
					}

					// add optional additional certificates
					FString OptionalPEMFilename = FPaths::ProjectContentDir() + TEXT("CurlCertificates/ca-additions.pem");
					if (FFileHelper::LoadFileToString(Contents, *OptionalPEMFilename))
					{
						const TCHAR* StrPtr = *Contents;
						auto Src = StringCast<ANSICHAR>(StrPtr, Contents.Len());
						Ar->Serialize((ANSICHAR*)Src.Get(), Src.Length() * sizeof(ANSICHAR));
					}
				}

				FPlatformString::Strncpy(capath, TCHAR_TO_ANSI(*PEMFilename), PathLength);
				CurlRequestOptions.CertBundlePath = capath;
				UE_LOG(LogInit, Log, TEXT(" Libcurl: using generated PEM file: '%s'"), *PEMFilename);
			}
		}
		else
		{
			FPlatformString::Strncpy(capath, TCHAR_TO_ANSI(*PEMFilename), PathLength);
			CurlRequestOptions.CertBundlePath = capath;
			UE_LOG(LogInit, Log, TEXT(" Libcurl: using existing PEM file: '%s'"), *PEMFilename);
		}

		if (CurlRequestOptions.CertBundlePath == nullptr)
		{
			UE_LOG(LogInit, Log, TEXT(" Libcurl: failed to generate a PEM cert bundle, TLS may not work"));
		}
	}
#endif

	// set certificate verification (disable to allow self-signed certificates)
	if (CurlRequestOptions.CertBundlePath == nullptr)
	{
		CurlRequestOptions.bVerifyPeer = false;
	}
	bool bVerifyPeer = true;
	if (GConfig->GetBool(TEXT("/Script/Engine.NetworkSettings"), TEXT("n.VerifyPeer"), bVerifyPeer, GEngineIni))
	{
		CurlRequestOptions.bVerifyPeer = bVerifyPeer;
	}

	bool bAcceptCompressedContent = true;
	if (GConfig->GetBool(TEXT("HTTP"), TEXT("AcceptCompressedContent"), bAcceptCompressedContent, GEngineIni))
	{
		CurlRequestOptions.bAcceptCompressedContent = bAcceptCompressedContent;
	}

	int32 ConfigBufferSize = 0;
	if (GConfig->GetInt(TEXT("HTTP.Curl"), TEXT("BufferSize"), ConfigBufferSize, GEngineIni) && ConfigBufferSize > 0)
	{
		CurlRequestOptions.BufferSize = ConfigBufferSize;
	}

	CurlRequestOptions.MaxHostConnections = FHttpModule::Get().GetHttpMaxConnectionsPerServer();
	if (CurlRequestOptions.MaxHostConnections > 0)
	{
		const CURLMcode SetOptResult = curl_multi_setopt(GMultiHandle, CURLMOPT_MAX_HOST_CONNECTIONS, static_cast<long>(CurlRequestOptions.MaxHostConnections));
		if (SetOptResult != CURLM_OK)
		{
			FUTF8ToTCHAR Converter(curl_multi_strerror(SetOptResult));
			UE_LOG(LogInit, Warning, TEXT("Failed to set max host connections options (%d), error %d ('%s')"),
				CurlRequestOptions.MaxHostConnections, (int32)SetOptResult, Converter.Get());
			CurlRequestOptions.MaxHostConnections = 0;
		}
	}
	else
	{
		CurlRequestOptions.MaxHostConnections = 0;
	}

	TCHAR Home[256] = TEXT("");
	if (FParse::Value(FCommandLine::Get(), TEXT("MULTIHOMEHTTP="), Home, ARRAY_COUNT(Home)))
	{
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (SocketSubsystem)
		{
			TSharedRef<FInternetAddr> HostAddr = SocketSubsystem->CreateInternetAddr();
			HostAddr->SetAnyAddress();

			bool bIsValid = false;
			HostAddr->SetIp(Home, bIsValid);
			if (bIsValid)
			{
				CurlRequestOptions.LocalHostAddr = FString(Home);
			}
		}
	}

	// print for visibility
	CurlRequestOptions.Log();
}

void FCurlHttpManager::FCurlRequestOptions::Log()
{
	UE_LOG(LogInit, Log, TEXT(" CurlRequestOptions (configurable via config and command line):"));
		UE_LOG(LogInit, Log, TEXT(" - bVerifyPeer = %s  - Libcurl will %sverify peer certificate"),
		bVerifyPeer ? TEXT("true") : TEXT("false"),
		bVerifyPeer ? TEXT("") : TEXT("NOT ")
		);

	const FString& ProxyAddress = FHttpModule::Get().GetProxyAddress();
	const bool bUseHttpProxy = !ProxyAddress.IsEmpty();
	UE_LOG(LogInit, Log, TEXT(" - bUseHttpProxy = %s  - Libcurl will %suse HTTP proxy"),
		bUseHttpProxy ? TEXT("true") : TEXT("false"),
		bUseHttpProxy ? TEXT("") : TEXT("NOT ")
		);	
	if (bUseHttpProxy)
	{
		UE_LOG(LogInit, Log, TEXT(" - HttpProxyAddress = '%s'"), *ProxyAddress);
	}

	UE_LOG(LogInit, Log, TEXT(" - bDontReuseConnections = %s  - Libcurl will %sreuse connections"),
		bDontReuseConnections ? TEXT("true") : TEXT("false"),
		bDontReuseConnections ? TEXT("NOT ") : TEXT("")
		);

	UE_LOG(LogInit, Log, TEXT(" - CertBundlePath = %s  - Libcurl will %s"),
		(CertBundlePath != nullptr) ? *FString(CertBundlePath) : TEXT("nullptr"),
		(CertBundlePath != nullptr) ? TEXT("set CURLOPT_CAINFO to it") : TEXT("use whatever was configured at build time.")
		);

	UE_LOG(LogInit, Log, TEXT(" - MaxHostConnections = %d  - Libcurl will %slimit the number of connections to a host"),
		MaxHostConnections,
		(MaxHostConnections == 0) ? TEXT("NOT ") : TEXT("")
		);

	UE_LOG(LogInit, Log, TEXT(" - LocalHostAddr = %s"), LocalHostAddr.IsEmpty() ? TEXT("Default") : *LocalHostAddr);

	UE_LOG(LogInit, Log, TEXT(" - BufferSize = %d"), CurlRequestOptions.BufferSize);
}


void FCurlHttpManager::ShutdownCurl()
{
	if (NULL != GMultiHandle)
	{
		curl_multi_cleanup(GMultiHandle);
		GMultiHandle = NULL;
	}

	curl_global_cleanup();

	LibCryptoMemHooks::UnsetMemoryHooks();

#if WITH_SSL
	// Shutdown OpenSSL
	FSslModule& SslModule = FModuleManager::LoadModuleChecked<FSslModule>("SSL");
	SslModule.GetSslManager().ShutdownSsl();
#endif // #if WITH_SSL
}

FHttpThread* FCurlHttpManager::CreateHttpThread()
{
	return new FCurlHttpThread();
}

bool FCurlHttpManager::SupportsDynamicProxy() const
{
	return true;
}
#endif //WITH_LIBCURL
