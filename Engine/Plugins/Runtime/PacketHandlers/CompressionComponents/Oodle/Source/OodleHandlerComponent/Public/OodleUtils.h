// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if HAS_OODLE_SDK

namespace OodleUtils
{
	/**
	* Decompress replay data using Oodle, for use by INetworkReplayStreamer streamers
	*
	* @param InCompressed	The compressed replay source data (prefixed with size and uncompressed size)
	* @param OutBuffer		The destination buffer for uncompressed data
	* @return				Whether or not decompression succeeded
	*/
	OODLEHANDLERCOMPONENT_API bool DecompressReplayData(const TArray<uint8>& InCompressed, TArray< uint8 >& OutBuffer);

	/**
	* Compress replay data using Oodle, for use by INetworkReplayStreamer streamers
	*
	* @param InBuffer		The uncompressed replay source data 
	* @param OutCompressed	The destination buffer for compressed data (prefixed with size and uncompressed size)
	* @return				Whether or not compression succeeded
	*/
	OODLEHANDLERCOMPONENT_API bool CompressReplayData(const TArray<uint8>& InBuffer, TArray< uint8 >& OutCompressed);
};

#endif
