// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Cluster events messages
 */
struct FDisplayClusterClusterEventsMsg
{
	constexpr static auto ArgName       = "Name";
	constexpr static auto ArgType       = "Type";
	constexpr static auto ArgCategory   = "Category";
	constexpr static auto ArgParameters = "Parameters";
	constexpr static auto ArgError      = "Error";
};
