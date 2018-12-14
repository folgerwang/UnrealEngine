// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace AutomationTool
{
	class CryptoKeys : BuildCommand
	{
		public override void ExecuteBuild()
		{
			var Params = new ProjectParams
			(
				Command: this,
				// Shared
				RawProjectPath: ProjectPath
			);

			LogInformation("********** CRYPTOKEYS COMMAND STARTED **********");

			string UE4EditorExe = HostPlatform.Current.GetUE4ExePath(Params.UE4Exe);
			if (!FileExists(UE4EditorExe))
			{
				throw new AutomationException("Missing " + UE4EditorExe + " executable. Needs to be built first.");
			}

			bool bCycleAllKeys = ParseParam("updateallkeys");
			bool bCycleEncryptionKey = bCycleAllKeys || ParseParam("updateencryptionkey");
			bool bCycleSigningKey = bCycleAllKeys || ParseParam("updatesigningkey");

			if (!bCycleAllKeys && !bCycleEncryptionKey && !bCycleSigningKey)
			{
				throw new Exception("A target for key cycling must be specified when using the cryptokeys automation script\n\t-updateallkeys: Update all keys\n\t-updateencryptionkey: Update encryption key\n\t-updatesigningkey: Update signing key");
			}

			FileReference OutputFile = FileReference.Combine(ProjectPath.Directory, "Config", "DefaultCrypto.ini");
			FileReference NoRedistOutputFile = FileReference.Combine(ProjectPath.Directory, "Config", "NoRedist", "DefaultCrypto.ini");
			FileReference DestinationFile = OutputFile;

			// If the project has a DefaultCrypto.ini in a NoRedist folder, we want to copy the newly generated file into that location
			if (FileReference.Exists(NoRedistOutputFile))
			{
				DestinationFile = NoRedistOutputFile;
			}

			string ChangeDescription = "Automated update of ";
			if (bCycleEncryptionKey)
			{
				ChangeDescription += "encryption";
			}

			if (bCycleSigningKey)
			{
				if (bCycleEncryptionKey)
				{
					ChangeDescription += " and ";
				}
				ChangeDescription += "signing";
			}

			ChangeDescription += " key";

			if (bCycleEncryptionKey && bCycleSigningKey)
			{
				ChangeDescription += "s";
			}

			ChangeDescription += " for project " + Params.ShortProjectName;

			P4Connection SubmitP4 = null;
			int NewCL = 0;
			if (CommandUtils.P4Enabled)
			{
				SubmitP4 = CommandUtils.P4;

				NewCL = SubmitP4.CreateChange(Description: ChangeDescription);
				SubmitP4.Revert(String.Format("-k \"{0}\"", DestinationFile.FullName));
				SubmitP4.Sync(String.Format("-k \"{0}\"", DestinationFile.FullName), AllowSpew: false);
				SubmitP4.Add(NewCL, String.Format("\"{0}\"", DestinationFile.FullName));
				SubmitP4.Edit(NewCL, String.Format("\"{0}\"", DestinationFile.FullName));
			}
			else
			{
				LogInformation(ChangeDescription);
				FileReference.MakeWriteable(OutputFile);
			}

			string CommandletParams = "";
			if (bCycleAllKeys) CommandletParams = "-updateallkeys";
			else if (bCycleEncryptionKey) CommandletParams = "-updateencryptionkey";
			else if (bCycleSigningKey) CommandletParams = "-updatesigningkey";

			RunCommandlet(ProjectPath, UE4EditorExe, "CryptoKeys", CommandletParams);

			if (DestinationFile != OutputFile)
			{
				File.Delete(DestinationFile.FullName);
				FileReference.Move(OutputFile, DestinationFile);
			}

			if (SubmitP4 != null)
			{
				int ActualCL;
				SubmitP4.Submit(NewCL, out ActualCL);
			}
		}

		public bool MakeNoRedist { get { return true; } }

		private FileReference ProjectFullPath;
		public virtual FileReference ProjectPath
		{
			get
			{
				if (ProjectFullPath == null)
				{
					var bForeign = ParseParam("foreign");
					var bForeignCode = ParseParam("foreigncode");
					if (bForeign)
					{
						var DestSample = ParseParamValue("DestSample", "CopiedHoverShip");
						var Dest = ParseParamValue("ForeignDest", CombinePaths(@"C:\testue4\foreign\", DestSample + "_ _Dir"));
						ProjectFullPath = new FileReference(CombinePaths(Dest, DestSample + ".uproject"));
					}
					else if (bForeignCode)
					{
						var DestSample = ParseParamValue("DestSample", "PlatformerGame");
						var Dest = ParseParamValue("ForeignDest", CombinePaths(@"C:\testue4\foreign\", DestSample + "_ _Dir"));
						ProjectFullPath = new FileReference(CombinePaths(Dest, DestSample + ".uproject"));
					}
					else
					{
						var OriginalProjectName = ParseParamValue("project", "");

						if (string.IsNullOrEmpty(OriginalProjectName))
						{
							throw new AutomationException("No project file specified. Use -project=<project>.");
						}

						var ProjectName = OriginalProjectName;
						ProjectName = ProjectName.Trim(new char[] { '\"' });
						if (ProjectName.IndexOfAny(new char[] { '\\', '/' }) < 0)
						{
							ProjectName = CombinePaths(CmdEnv.LocalRoot, ProjectName, ProjectName + ".uproject");
						}
						else if (!FileExists_NoExceptions(ProjectName))
						{
							ProjectName = CombinePaths(CmdEnv.LocalRoot, ProjectName);
						}
						if (FileExists_NoExceptions(ProjectName))
						{
							ProjectFullPath = new FileReference(ProjectName);
						}
						else
						{
							var Branch = new BranchInfo(new List<UnrealTargetPlatform> { UnrealBuildTool.BuildHostPlatform.Current.Platform });
							var GameProj = Branch.FindGame(OriginalProjectName);
							if (GameProj != null)
							{
								ProjectFullPath = GameProj.FilePath;
							}
							if (ProjectFullPath == null || !FileExists_NoExceptions(ProjectFullPath.FullName))
							{
								throw new AutomationException("Could not find a project file {0}.", ProjectName);
							}
						}
					}
				}
				return ProjectFullPath;
			}
		}
	}
}