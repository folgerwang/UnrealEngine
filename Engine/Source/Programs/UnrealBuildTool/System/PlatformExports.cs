// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// General platform support functions, exported for UAT
	/// </summary>
	public class PlatformExports
	{
		/// <summary>
		/// Checks whether the given platform is available
		/// </summary>
		/// <param name="Platform">Platform to check</param>
		/// <returns>True if the platform is available, false otherwise</returns>
		public static bool IsPlatformAvailable(UnrealTargetPlatform Platform)
		{
			return UEBuildPlatform.IsPlatformAvailable(Platform);
		}

		/// <summary>
		/// Gets a list of the registered platforms
		/// </summary>
		/// <returns>List of platforms</returns>
		public static IEnumerable<UnrealTargetPlatform> GetRegisteredPlatforms()
		{
			return UEBuildPlatform.GetRegisteredPlatforms();
		}

		/// <summary>
		/// Gets the default architecture for a given platform
		/// </summary>
		/// <param name="Platform">The platform to get the default architecture for</param>
		/// <param name="ProjectFile">Project file to read settings from</param>
		/// <returns>The default architecture</returns>
		public static string GetDefaultArchitecture(UnrealTargetPlatform Platform, FileReference ProjectFile)
		{
			return UEBuildPlatform.GetBuildPlatform(Platform).GetDefaultArchitecture(ProjectFile);
		}

		/// <summary>
		/// Checks whether the given project has a default build configuration
		/// </summary>
		/// <param name="ProjectFile">The project file</param>
		/// <param name="Platform">Platform to check settings for</param>
		/// <returns>True if the project uses the default build configuration</returns>
		public static bool HasDefaultBuildConfig(FileReference ProjectFile, UnrealTargetPlatform Platform)
		{
			UEBuildPlatform BuildPlat = UEBuildPlatform.GetBuildPlatform(Platform, true);
			return (BuildPlat == null)? true : BuildPlat.HasDefaultBuildConfig(Platform, ProjectFile.Directory);
		}

		/// <summary>
		/// Checks whether the given project requires a build because of platform needs
		/// </summary>
		/// <param name="ProjectFile">The project file</param>
		/// <param name="Platform">Platform to check settings for</param>
		/// <returns>True if the project requires a build for the platform</returns>
		public static bool RequiresBuild(FileReference ProjectFile, UnrealTargetPlatform Platform)
		{
			UEBuildPlatform BuildPlat = UEBuildPlatform.GetBuildPlatform(Platform, true);
			return (BuildPlat == null) ? false : BuildPlat.RequiresBuild(Platform, ProjectFile.Directory);
		}

		/// <summary>
		/// Returns an array of all platform folder names
		/// </summary>
		/// <returns>All platform folder names</returns>
		public static string[] GetPlatformFolderNames()
		{
			return UEBuildPlatform.GetPlatformFolderNames();
		}

		/// <summary>
		/// Returns an array of all platform folder names
		/// </summary>
		/// <param name="Platform">The platform to get the included folder names for</param>
		/// <returns>All platform folder names</returns>
		public static string[] GetIncludedFolderNames(UnrealTargetPlatform Platform)
		{
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform, false);
			return BuildPlatform.GetIncludedFolderNames();
		}

		/// <summary>
		/// Returns all the excluded folder names for a given platform
		/// </summary>
		/// <param name="Platform">The platform to get the excluded folder names for</param>
		/// <returns>Array of folder names</returns>
		public static string[] GetExcludedFolderNames(UnrealTargetPlatform Platform)
		{
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform, false);
			return BuildPlatform.GetExcludedFolderNames();
		}

		/// <summary>
		/// Check whether the given platform supports XGE
		/// </summary>
		/// <param name="Platform">Platform to check</param>
		/// <returns>True if the platform supports XGE</returns>
		public static bool CanUseXGE(UnrealTargetPlatform Platform)
		{
			return UEBuildPlatform.IsPlatformAvailable(Platform) && UEBuildPlatform.GetBuildPlatform(Platform).CanUseXGE();
		}

		/// <summary>
		/// Check whether the given platform supports the parallel executor in UAT
		/// </summary>
		/// <param name="Platform">Platform to check</param>
		/// <returns>True if the platform supports the parallel executor in UAT</returns>
		public static bool CanUseParallelExecutor(UnrealTargetPlatform Platform)
		{
			return UEBuildPlatform.IsPlatformAvailable(Platform) && UEBuildPlatform.GetBuildPlatform(Platform).CanUseParallelExecutor();
		}

		/// <summary>
		///
		/// </summary>
		public static void PreventAutoSDKSwitching()
		{
			UEBuildPlatformSDK.bAllowAutoSDKSwitching = false;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="Path"></param>
		public static void SetRemoteIniPath(string Path)
		{
			UnrealBuildTool.SetRemoteIniPath(Path);
		}

		/// <summary>
		/// Initialize UBT in the context of another host process (presumably UAT)
		/// </summary>
		/// <param name="bIsEngineInstalled">Whether the engine is installed</param>
		/// <returns>True if initialization was successful</returns>
		public static bool Initialize(bool bIsEngineInstalled)
		{
			UnrealBuildTool.SetIsEngineInstalled(bIsEngineInstalled);

			// Read the XML configuration files
			if(!XmlConfig.ReadConfigFiles(false))
			{
				return false;
			}

			// Register all the platform classes
			UnrealBuildTool.RegisterAllUBTClasses(SDKOutputLevel.Quiet, false);
			return true;
		}
	}
}
