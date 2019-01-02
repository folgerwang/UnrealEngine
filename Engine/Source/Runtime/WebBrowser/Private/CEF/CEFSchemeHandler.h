// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IWebBrowserSchemeHandler.h"

#if WITH_CEF3

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"
#endif

#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
THIRD_PARTY_INCLUDES_START
#if PLATFORM_APPLE
PRAGMA_DISABLE_DEPRECATION_WARNINGS
#endif
#include "include/cef_resource_handler.h"
#include "include/cef_scheme.h"
#if PLATFORM_APPLE
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
THIRD_PARTY_INCLUDES_END
#pragma pop_macro("OVERRIDE")

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

/**
 * Implementation for managing CEF custom scheme handlers.
 */
class FCefSchemeHandlerFactories
{
public:
	/**
	 * Adds a custom scheme handler factory, for a given scheme and domain. The domain is ignored if the scheme is not a browser built in scheme,
	 * and all requests will go through this factory.
	 * @param Scheme                            The scheme name to handle.
	 * @param Domain                            The domain name to handle on the scheme. Ignored if scheme is not a built in scheme.
	 * @param WebBrowserSchemeHandlerFactory    The factory implementation for creating request handlers for this scheme.
	 */
	void AddSchemeHandlerFactory(FString Scheme, FString Domain, IWebBrowserSchemeHandlerFactory* WebBrowserSchemeHandlerFactory);

	/**
	 * Remove a custom scheme handler factory. The factory may still be used by existing open browser windows, but will no longer be provided for new ones.
	 * @param WebBrowserSchemeHandlerFactory    The factory implementation to remove.
	 */
	void RemoveSchemeHandlerFactory(IWebBrowserSchemeHandlerFactory* WebBrowserSchemeHandlerFactory);

	/**
	 * Register all scheme handler factories with the provided request context.
	 * @param Context   The context.
	 */
	void RegisterFactoriesWith(CefRefPtr<CefRequestContext>& Context);

private:
	/**
	 * A struct to wrap storage of a factory with it's provided scheme and domain, inc ref counting for the cef representation.
	 */
	struct FFactory
	{
	public:
		FFactory(FString Scheme, FString Domain, CefRefPtr<CefSchemeHandlerFactory> Factory);
		FString Scheme;
		FString Domain;
		CefRefPtr<CefSchemeHandlerFactory> Factory;
	};

	// Array of registered handler factories.
	TArray<FFactory> SchemeHandlerFactories;
};


#endif
