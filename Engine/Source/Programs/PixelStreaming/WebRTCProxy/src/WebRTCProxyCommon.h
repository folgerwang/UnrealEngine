// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Windows/WindowsHWrapper.h"

// Directly use what is defined in UE4, to avoid duplication and bugs due
// to enums mismatches
#include "../../../../../Plugins/Experimental/PixelStreaming/Source//PixelStreaming/Private/ProtocolDefs.h"

#define EG_PLATFORM_WINDOWS 1
#define EG_PLATFORM_LINUX 2

#if defined(_WIN32)
	#define EG_PLATFORM EG_PLATFORM_WINDOWS
#elif __linux__
	#define EG_PLATFORM EG_PLATFORM_LINUX
#endif

#include "WebRTCProxyPCH.h"

/**
 * Gets the current process path
 * @param Filename	If specified, it will contain the name of the executable on return
 * @return The executable's directory
 */
std::string GetProcessPath(std::string* Filename = nullptr);

/**
 * Gets the extension of a file name
 * @param FullFilename	File name to get the extension from
 * @param Basename	If specified, it will contain the filename without extension
 */
std::string GetExtension(const std::string& FullFilename, std::string* Basename);

//
// Available parameters
//
extern std::pair<std::string, uint16_t> PARAM_Cirrus;
extern uint16_t PARAM_UE4Port;
extern bool PARAM_PlanB;
extern bool PARAM_LocalTime;

#if EG_PLATFORM == EG_PLATFORM_WINDOWS
	#pragma warning(disable: 26439) // This kind of function may not throw. Declare it 'noexcept' (f.6).
	#pragma warning(disable: 26444) // warning C26444: Avoid unnamed objects with custom construction and destruction (es.84).
	#pragma warning(disable: 6319) // Use of the comma-operator in a tested expression causes the left argument to be ignored when it has no side-effects.
#endif

using FClientId = uint32_t;

// Names used for a IceCandidate JSON object.
const char kCandidateSdpMidName[] = "sdpMid";
const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
const char kCandidateSdpName[] = "candidate";

// Names used for a SessionDescription JSON object.
const char kSessionDescriptionTypeName[] = "type";
const char kSessionDescriptionSdpName[] = "sdp";

// Names used for a CirrusConfig JSON object
const char kPeerConnectionConfigName[] = "peerConnectionConfig";
const char kIceServersName[] = "iceServers";
const char kUrlsName[] = "urls";
const char kUsernameName[] = "username";
const char kCredentialName[] = "credential";

