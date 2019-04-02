// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Swap synchronization messages
 */
//@todo: encapsulate strings below in message classes
struct FDisplayClusterSwapSyncMsg
{
	constexpr static auto ProtocolName = "SwapSync";
	
	constexpr static auto TypeRequest  = "request";
	constexpr static auto TypeResponse = "response";

	struct WaitForSwapSync
	{
		constexpr static auto name = "WaitForSwapSync";
		constexpr static auto argThreadTime  = "ThreadTime";
		constexpr static auto argBarrierTime = "BarrierTime";
	};
};
