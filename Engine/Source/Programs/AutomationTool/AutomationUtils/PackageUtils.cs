// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Threading;
using AutomationTool;
using EpicGames.MCP.Automation;
using UnrealBuildTool;
using System.Diagnostics;
using Tools.DotNETCommon;
using System.Reflection;
using System.Threading.Tasks;

namespace AutomationTool
{
	public partial class PackageUtils
	{
		private static void UnpakBuild(string SourceDirectory, List<string> PakFiles, string TargetDirectory, string CryptoFilename, string AdditionalArgs)
		{
			if (!CommandUtils.DirectoryExists(SourceDirectory))
			{
				CommandUtils.LogError("Pak file directory {0} doesn't exist.", SourceDirectory);
				return;
			}

			string UnrealPakExe = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine/Binaries/Win64/UnrealPak.exe");
			CommandUtils.CreateDirectory(TargetDirectory);

			Parallel.ForEach(PakFiles, pakFile =>
			{
				string PathFileFullPath = CommandUtils.CombinePaths(SourceDirectory, pakFile);
				string UnrealPakCommandLine = string.Format("{0} -Extract {1} -ExtractToMountPoint -cryptokeys=\"{2}\" {3}",
															PathFileFullPath, TargetDirectory, CryptoFilename, AdditionalArgs);
				CommandUtils.RunAndLog(CommandUtils.CmdEnv, UnrealPakExe, UnrealPakCommandLine, Options: CommandUtils.ERunOptions.Default | CommandUtils.ERunOptions.UTF8Output | CommandUtils.ERunOptions.SpewIsVerbose);
				//CommandUtils.Log(UnrealPakCommandLine);
			});
		}

		private static List<FileInfo>[] SortFilesByPatchLayers(FileInfo[] FileInfoList)
		{
			int NumLevels = 0;
			bool FoundFilesInLevel = true;

			while (FoundFilesInLevel)
			{
				FoundFilesInLevel = false;
				string PatchPakSuffix = String.Format("_{0}_P", NumLevels);
				if (FileInfoList.Any(fileInfo => Path.GetFileNameWithoutExtension(fileInfo.Name).EndsWith(PatchPakSuffix)))
				{
					NumLevels++;
					FoundFilesInLevel = true;
				}
			}

			List<FileInfo>[] FileList = new List<FileInfo>[NumLevels + 1];

			FileList[0] = FileInfoList.Where(fileInfo => !Path.GetFileNameWithoutExtension(fileInfo.Name).EndsWith("_P")).ToList();
			
			for (int PatchLevel = 0; PatchLevel < NumLevels; PatchLevel++)
			{
				string PatchPakSuffix = String.Format("_{0}_P", NumLevels);
				FileList[PatchLevel + 1] = FileInfoList.Where(fileInfo => Path.GetFileNameWithoutExtension(fileInfo.Name).EndsWith(PatchPakSuffix)).ToList();
			}

			return FileList;
		}

		private static void GetFileListByPatchLayers(FileInfo[] PakFiles, out List<string>[] PatchLayers)
		{
			var SortedLayers = SortFilesByPatchLayers(PakFiles);

			PatchLayers = SortedLayers.Select(patchLayer => patchLayer.Select(fileInfo => fileInfo.Name).ToList()).ToArray();
		}

		public static void ExtractPakFiles(DirectoryInfo SourceDirectory, string TargetDirectory, string CryptoKeysFilename, string AdditionalArgs, bool bExtractByLayers)
		{
			var PakFiles = SourceDirectory.GetFiles("*.pak", SearchOption.TopDirectoryOnly);

			if (bExtractByLayers)
			{
				List<string>[] PatchLayers;

				GetFileListByPatchLayers(PakFiles, out PatchLayers);

				int NumLayers = PatchLayers.Length;

				for (int layerIndex = 0; layerIndex < NumLayers; layerIndex++)
				{
					UnpakBuild(SourceDirectory.FullName, PatchLayers[layerIndex], TargetDirectory, CryptoKeysFilename, AdditionalArgs);
				}
			}
			else
			{
				UnpakBuild(SourceDirectory.FullName, PakFiles.Select(file => file.Name).ToList(), TargetDirectory, CryptoKeysFilename, AdditionalArgs);
			}
		}
	}
}
