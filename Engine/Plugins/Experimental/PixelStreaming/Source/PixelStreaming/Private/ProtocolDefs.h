// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AssertionMacros.h"

namespace PixelStreamingProtocol
{
	enum class EToUE4Msg : uint8
	{
		/**********************************************************************/
		
		/*
		 * Control Messages. Range = 0..49.
		 */
		
		IFrameRequest = 0,
		RequestQualityControl = 1, // This one is intercepted and processed at the proxy
		MaxFpsRequest = 2,
		AverageBitrateRequest = 3,
		StartStreaming = 4,
		StopStreaming = 5,

		/**********************************************************************/

		/*
		 * Input Messages. Range = 50..89.
		 */

		// Generic Input Messages. Range = 50..59.
		UIInteraction = 50,
		Command = 51,

		// Keyboard Input Message. Range = 60..69.
		KeyDown = 60,
		KeyUp = 61,
		KeyPress = 62,

		// Mouse Input Messages. Range = 70..79.
		MouseEnter = 70,
		MouseLeave = 71,
		MouseDown = 72,
		MouseUp = 73,
		MouseMove = 74,
		MouseWheel = 75,

		// Touch Input Messages. Range = 80..89.
		TouchStart = 80,
		TouchEnd = 81,
		TouchMove = 82,
		
		/**********************************************************************/

		/*
		 * Ensure Count is the final entry.
		 */
		Count

		/**********************************************************************/
	};

	// !!! modifying this enum make sure to update the next function !!!
	enum class EToProxyMsg : uint8 { AudioPCM, SpsPps, VideoIDR, Video, ClientConfig, Response, Count };
	inline const TCHAR* PacketTypeStr(EToProxyMsg PktType) 
	{
		static const TCHAR* Str[static_cast<uint8>(EToProxyMsg::Count)] = { TEXT("AudioPCM"), TEXT("SpsPps"), TEXT("VideoIDR"), TEXT("Video"), TEXT("ClientConfig"), TEXT("Response") };
		check(PktType < EToProxyMsg::Count);
		return Str[static_cast<uint8>(PktType)];
	}

	//! Messages that can be sent to the webrtc clients
	enum class EToClientMsg : uint8 { QualityControlOwnership, Response };

	enum class ECirrusToProxyMsg : uint8 { offer, iceCandidate, clientDisconnected, config, count };
	enum class EProxyToCirrusMsg : uint8 { answer, iceCandidate, disconnectClient };
};