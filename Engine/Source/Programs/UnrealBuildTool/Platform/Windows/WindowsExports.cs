// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Public Linux functions exposed to UAT
	/// </summary>
	public static class WindowsExports
	{
		/// <summary>
		/// Tries to get the directory for an installed Visual Studio version
		/// </summary>
		/// <param name="Compiler">The compiler version</param>
		/// <param name="InstallDir">Receives the install directory on success</param>
		/// <returns>True if successful</returns>
		public static bool TryGetVSInstallDir(WindowsCompiler Compiler, out DirectoryReference InstallDir)
		{
			return WindowsPlatform.TryGetVSInstallDir(Compiler, out InstallDir);
		}

		/// <summary>
		/// Gets the path to MSBuild.exe
		/// </summary>
		/// <returns>Path to MSBuild.exe</returns>
		public static string GetMSBuildToolPath()
		{
			return WindowsPlatform.GetMsBuildToolPath().FullName;
		}
	}
}
