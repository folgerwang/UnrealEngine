// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.Serialization.Formatters.Binary;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Caches include dependency information to speed up preprocessing on subsequent runs.
	/// </summary>
	class ActionHistory
	{
		/// <summary>
		/// Path to store the cache data to.
		/// </summary>
		private string FilePath;

		/// <summary>
		/// The command lines used to produce files, keyed by the absolute file paths.
		/// </summary>
		private Dictionary<string, string> ProducedItemToPreviousActionCommandLine;

		/// <summary>
		/// Whether the dependency cache is dirty and needs to be saved.
		/// </summary>
		private bool bIsDirty;

		public ActionHistory(string InFilePath)
		{
			FilePath = Path.GetFullPath(InFilePath);

			bool bFoundCache = false;
			if (File.Exists(FilePath) == true)
			{
				try
				{
					// Deserialize the history from disk if the file exists.
					using (FileStream Stream = new FileStream(FilePath, FileMode.Open, FileAccess.Read))
					{
						BinaryFormatter Formatter = new BinaryFormatter();
						ProducedItemToPreviousActionCommandLine = Formatter.Deserialize(Stream) as Dictionary<string, string>;
					}

					bFoundCache = true;
				}
				catch (Exception)
				{
					// If this fails for any reason just reset. History will be created later.
					ProducedItemToPreviousActionCommandLine = null;
				}
			}

			if (!bFoundCache)
			{
				// Otherwise create a fresh history.
				ProducedItemToPreviousActionCommandLine = new Dictionary<string, string>();
			}

			bIsDirty = false;
		}

		public void Save()
		{
			// Only save if we've made changes to it since load.
			if (bIsDirty)
			{
				// Serialize the cache to disk.
				try
				{
					Directory.CreateDirectory(Path.GetDirectoryName(FilePath));
					using (FileStream Stream = new FileStream(FilePath, FileMode.Create, FileAccess.Write))
					{
						BinaryFormatter Formatter = new BinaryFormatter();
						Formatter.Serialize(Stream, ProducedItemToPreviousActionCommandLine);
					}
				}
				catch (Exception Ex)
				{
					throw new BuildException(Ex, "Failed to write dependency cache.");
				}
			}
		}

		public bool GetProducingCommandLine(FileItem File, out string Result)
		{
			return ProducedItemToPreviousActionCommandLine.TryGetValue(File.AbsolutePath.ToUpperInvariant(), out Result);
		}

		public void SetProducingCommandLine(FileItem File, string CommandLine)
		{
			ProducedItemToPreviousActionCommandLine[File.AbsolutePath.ToUpperInvariant()] = CommandLine;
			bIsDirty = true;
		}

		/// <summary>
		/// Generates a full path to action history file for the specified target.
		/// </summary>
		public static FileReference GeneratePathForTarget(FileReference ProjectFile, string TargetName, UnrealTargetPlatform Platform, string Architecture, bool bUseSharedBuildEnvironment)
		{
			if(bUseSharedBuildEnvironment)
			{
				if(UnrealBuildTool.IsEngineInstalled() && ProjectFile != null)
				{
					return FileReference.Combine(ProjectFile.Directory, "Intermediate", "Build", "ActionHistory.bin");
				}
				else
				{
					return FileReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", "ActionHistory.bin");
				}
			}
			else
			{
				string PlatformIntermediateFolder = UEBuildTarget.GetPlatformIntermediateFolder(Platform, Architecture);

				if(ProjectFile != null)
				{
					return FileReference.Combine(ProjectFile.Directory, PlatformIntermediateFolder, TargetName, "ActionHistory.bin");
				}
				else
				{
					return FileReference.Combine(UnrealBuildTool.EngineDirectory, PlatformIntermediateFolder, TargetName, "ActionHistory.bin");
				}
			}
		}
	}
}
