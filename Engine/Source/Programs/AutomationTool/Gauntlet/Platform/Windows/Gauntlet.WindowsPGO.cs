// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

namespace Gauntlet
{
	/// <summary>
	/// Skeleton Windows PGO platform implementation (primarily used to test PGO locally with editor builds)
	/// </summary>
	internal class WindowsPGOPlatform : IPGOPlatform
	{

		public UnrealTargetPlatform GetPlatform()
		{
			return UnrealTargetPlatform.Win64;
		}

		public void GatherResults(string ArtifactPath)
		{

		}

		public void ApplyConfiguration(PGOConfig Config)
		{

		}

		public bool TakeScreenshot(ITargetDevice Device, string ScreenshotDirectory, out string ImageFilename)
		{			
			ImageFilename = string.Empty;
			return false;
		}
	}
}



