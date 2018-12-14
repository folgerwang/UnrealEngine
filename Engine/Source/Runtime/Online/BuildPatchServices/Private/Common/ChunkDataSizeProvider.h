// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Common/DataSizeProvider.h"
#include "BuildPatchManifest.h"

namespace BuildPatchServices
{
	class IChunkDataSizeProvider
		: public IDataSizeProvider
	{
	public:
		virtual void AddManifestData(const FBuildPatchAppManifest* Manifest) = 0;
	};

	class FChunkDataSizeProviderFactory
	{
	public:
		static IChunkDataSizeProvider* Create();
	};
}