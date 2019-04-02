// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"

class FHttpManager;
class IHttpRequest;

/**
 * Platform specific Http implementations
 * Intended usage is to use FPlatformHttp instead of FGenericPlatformHttp
 */
class HTTP_API FGenericPlatformHttp
{
public:

	/**
	 * Platform initialization step
	 */
	static void Init();

	/**
	 * Creates a platform-specific HTTP manager.
	 *
	 * @return nullptr if default implementation is to be used
	 */
	static FHttpManager* CreatePlatformHttpManager()
	{
		return nullptr;
	}

	/**
	 * Platform shutdown step
	 */
	static void Shutdown();

	/**
	 * Creates a new Http request instance for the current platform
	 *
	 * @return request object
	 */
	static IHttpRequest* ConstructRequest();

	/**
	 * Check if a platform uses the HTTP thread
	 *
	 * @return true if the platform uses threaded HTTP, false if not
	 */
	static bool UsesThreadedHttp();

	/**
	 * Returns a percent-encoded version of the passed in string
	 *
	 * @param UnencodedString The unencoded string to convert to percent-encoding
	 * @return The percent-encoded string
	 */
	static FString UrlEncode(const FString& UnencodedString);

	/**
	 * Returns a decoded version of the percent-encoded passed in string
	 *
	 * @param EncodedString The percent encoded string to convert to string
	 * @return The decoded string
	 */
	static FString UrlDecode(const FString& EncodedString);

	/**
	 * Returns the &lt; &gt...etc encoding for strings between HTML elements.
	 *
	 * @param UnencodedString The unencoded string to convert to html encoding.
	 * @return The html encoded string
	 */
	static FString HtmlEncode(const FString& UnencodedString);

	/** 
	 * Returns the domain portion of the URL, e.g., "a.b.c" of "http://a.b.c/d"
	 * @param Url the URL to return the domain of
	 * @return the domain of the specified URL
	 */
	static FString GetUrlDomain(const FString& Url);

	/**
	 * Get the mime type for the file
	 * @return the mime type for the file.
	 */
	static FString GetMimeType(const FString& FilePath);

	/**
	 * Returns the default User-Agent string to use in HTTP requests.
	 * Requests that explicitly set the User-Agent header will not use this value.
	 *
	 * @return the default User-Agent string that requests should use.
	 */
	static FString GetDefaultUserAgent();
	static FString EscapeUserAgentString(const FString& UnescapedString);

	/**
	 * Get the proxy address specified by the operating system
	 *
	 * @return optional FString: If unset: we are unable to get information from the operating system. If set: the proxy address set by the operating system (may be blank)
	 */
	static TOptional<FString> GetOperatingSystemProxyAddress();

	/**
	 * Check if getting proxy information from the current operating system is supported
	 * Useful for "Network Settings" type pages.  GetProxyAddress may return an empty or populated string but that does not imply
	 * the operating system does or does not support proxies (or that it has been implemented here)
	 * 
	 * @return true if we are able to get proxy information from the current operating system, false if not
	 */
	static bool IsOperatingSystemProxyInformationSupported();

	/**
	 * Helper function for checking if a byte array is in URL encoded format.
	 */
	static bool IsURLEncoded(const TArray<uint8>& Payload);
};