// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Interfaces/IBuildStatistics.h"
#include "BuildPatchInstaller.h"

namespace BuildPatchServices
{
	/**
	 * A factory for creating an IBuildStatistics instance.
	 */
	class FBuildStatisticsFactory
	{
	public:
		static IBuildStatistics* Create(FBuildPatchInstallerRef Installer);
	};
}