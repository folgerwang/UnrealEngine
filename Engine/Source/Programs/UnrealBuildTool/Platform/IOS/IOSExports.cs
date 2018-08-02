// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Public IOS functions exposed to UAT
	/// </summary>
	public static class IOSExports
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="InProject"></param>
		/// <param name="Distribution"></param>
		/// <param name="MobileProvision"></param>
		/// <param name="SigningCertificate"></param>
		/// <param name="TeamUUID"></param>
		/// <param name="bAutomaticSigning"></param>
		public static void GetProvisioningData(FileReference InProject, bool Distribution, out string MobileProvision, out string SigningCertificate, out string TeamUUID, out bool bAutomaticSigning)
		{
			IOSProjectSettings ProjectSettings = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS)).ReadProjectSettings(InProject);
			if (ProjectSettings == null)
			{
				MobileProvision = null;
				SigningCertificate = null;
				TeamUUID = null;
				bAutomaticSigning = false;
				return;
			}
			if (ProjectSettings.bAutomaticSigning)
			{
				MobileProvision = null;
				SigningCertificate = null;
				TeamUUID = ProjectSettings.TeamID;
				bAutomaticSigning = true;
			}
			else
			{
				IOSProvisioningData Data = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS)).ReadProvisioningData(ProjectSettings, Distribution);
				if (Data == null)
				{ // no provisioning, swith to automatic
					MobileProvision = null;
					SigningCertificate = null;
					TeamUUID = ProjectSettings.TeamID;
					bAutomaticSigning = true;
				}
				else
				{
					MobileProvision = Data.MobileProvision;
					SigningCertificate = Data.SigningCertificate;
					TeamUUID = Data.TeamUUID;
					bAutomaticSigning = false;
				}
			}
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="Config"></param>
		/// <param name="ProjectFile"></param>
		/// <param name="InProjectName"></param>
		/// <param name="InProjectDirectory"></param>
		/// <param name="InExecutablePath"></param>
		/// <param name="InEngineDir"></param>
		/// <param name="bForDistribution"></param>
		/// <param name="CookFlavor"></param>
		/// <param name="bIsDataDeploy"></param>
		/// <param name="bCreateStubIPA"></param>
		/// <param name="BuildReceiptFileName"></param>
		/// <returns></returns>
		public static bool PrepForUATPackageOrDeploy(UnrealTargetConfiguration Config, FileReference ProjectFile, string InProjectName, DirectoryReference InProjectDirectory, string InExecutablePath, DirectoryReference InEngineDir, bool bForDistribution, string CookFlavor, bool bIsDataDeploy, bool bCreateStubIPA, FileReference BuildReceiptFileName)
		{
			return new UEDeployIOS().PrepForUATPackageOrDeploy(Config, ProjectFile, InProjectName, InProjectDirectory.FullName, InExecutablePath, InEngineDir.FullName, bForDistribution, CookFlavor, bIsDataDeploy, bCreateStubIPA, BuildReceiptFileName);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="Config"></param>
		/// <param name="ProjectDirectory"></param>
		/// <param name="bIsUE4Game"></param>
		/// <param name="GameName"></param>
		/// <param name="ProjectName"></param>
		/// <param name="InEngineDir"></param>
		/// <param name="AppDirectory"></param>
		/// <param name="BuildReceiptFileName"></param>
		/// <param name="bSupportsPortrait"></param>
		/// <param name="bSupportsLandscape"></param>
		/// <param name="bSkipIcons"></param>
		/// <returns></returns>
		public static bool GeneratePList(FileReference ProjectFile, UnrealTargetConfiguration Config, DirectoryReference ProjectDirectory, bool bIsUE4Game, string GameName, string ProjectName, DirectoryReference InEngineDir, DirectoryReference AppDirectory, FileReference BuildReceiptFileName, out bool bSupportsPortrait, out bool bSupportsLandscape, out bool bSkipIcons)
		{
			return new UEDeployIOS().GeneratePList(ProjectFile, Config, ProjectDirectory.FullName, bIsUE4Game, GameName, ProjectName, InEngineDir.FullName, AppDirectory.FullName, BuildReceiptFileName, out bSupportsPortrait, out bSupportsLandscape, out bSkipIcons);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="PlatformType"></param>
		/// <param name="SourceFile"></param>
		/// <param name="TargetFile"></param>
		public static void StripSymbols(UnrealTargetPlatform PlatformType, FileReference SourceFile, FileReference TargetFile)
		{
			IOSProjectSettings ProjectSettings = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(PlatformType)).ReadProjectSettings(null);
			IOSToolChain ToolChain = new IOSToolChain(null, ProjectSettings);
			ToolChain.StripSymbols(SourceFile, TargetFile);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="Executable"></param>
		/// <param name="StageDirectory"></param>
		/// <param name="PlatformType"></param>
		public static void GenerateAssetCatalog(FileReference ProjectFile, FileReference Executable, DirectoryReference StageDirectory, UnrealTargetPlatform PlatformType)
		{
			CppPlatform Platform = PlatformType == UnrealTargetPlatform.IOS ? CppPlatform.IOS : CppPlatform.TVOS;

			// Determine whether the user has modified icons that require a remote Mac to build.
			bool bUserImagesExist = false;
			DirectoryReference ResourcesDir = IOSToolChain.GenerateAssetCatalog(ProjectFile, Platform, ref bUserImagesExist);

			// Don't attempt to do anything remotely if the user is using the default UE4 images.
			if (!bUserImagesExist)
			{
				return;
			}

            // Also don't attempt to use a remote Mac if packaging for TVOS on PC.
            if (Platform == CppPlatform.TVOS && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
            {
                return;
            }

			// Compile the asset catalog immediately
			if(BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				FileReference OutputFile = FileReference.Combine(StageDirectory, "Assets.car");

				RemoteMac Remote = new RemoteMac(ProjectFile);
				Remote.RunAssetCatalogTool(Platform, ResourcesDir, OutputFile);
			}
			else
			{
				// Get the output file
				FileReference OutputFile = IOSToolChain.GetAssetCatalogFile(Platform, Executable);

				// Delete the Assets.car file to force the asset catalog to build every time, because
				// removals of files or copies of icons (for instance) with a timestamp earlier than
				// the last generated Assets.car will result in nothing built.
				if (FileReference.Exists(OutputFile))
				{
					FileReference.Delete(OutputFile);
				}

				// Run the process locally
				using(Process Process = new Process())
				{
					Process.StartInfo.FileName = "/usr/bin/xcrun";
					Process.StartInfo.Arguments = IOSToolChain.GetAssetCatalogArgs(Platform, ResourcesDir.FullName, OutputFile.Directory.FullName);; 
					Process.StartInfo.UseShellExecute = false;
					Utils.RunLocalProcess(Process);
				}
			}
		}

        /// <summary>
        /// 
        /// </summary>
        public static bool SupportsIconCatalog(DirectoryReference ProjectDirectory, FileReference BuildRecieptFileName)
        {
            // get the receipt
            if (System.IO.File.Exists(BuildRecieptFileName.FullName))
            {
                TargetReceipt Receipt = TargetReceipt.Read(BuildRecieptFileName, UnrealBuildTool.EngineDirectory, ProjectDirectory);
                IEnumerable<ReceiptProperty> Results = Receipt.AdditionalProperties.Where(x => x.Name == "SDK");

                if (Results.Count() > 0)
                {
                    if (Single.Parse(Results.ElementAt(0).Value) >= 11.0f)
                    {
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }
            }
            return false;
        }
    }
}
