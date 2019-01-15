// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WmfMediaPrivate.h"

#if WMFMEDIA_SUPPORTED_PLATFORM

/**
 * Custom topology loader used to check if video will be decoded on GPU or CPU
 * even when hardware acceleration is enabled.
 */
class WmfMediaTopologyLoader
{
public:

	bool IsHardwareAccelerated(const TComPtr<IMFTopology>& InTopology) const;

private:

	bool ResolveActivationNode(const TComPtr<IMFTopology>& InTopology) const;
	bool CheckTopologyForHardwareDecoding(const TComPtr<IMFTopology>& InTopology) const;
};

#endif
