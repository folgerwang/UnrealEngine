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
	/// Public IOS functions exposed to UAT
	/// </summary>
	public static class IOSExports
	{
		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		public static bool UseRPCUtil()
		{
			XmlConfig.ReadConfigFiles();
			return RemoteToolChain.bUseRPCUtil;
		}

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

			IOSProvisioningData Data = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS)).ReadProvisioningData(ProjectSettings, Distribution);
			if(Data == null)
			{
				MobileProvision = null;
				SigningCertificate = null;
				TeamUUID = null;
				bAutomaticSigning = true;
			}
			else
			{
				MobileProvision = Data.MobileProvision;
				SigningCertificate = Data.SigningCertificate;
				TeamUUID = Data.TeamUUID;
                if (Data.MobileProvisionName.Contains("*") || ProjectSettings.bAutomaticSigning)
                {
                    bAutomaticSigning = true;
                }
                else
                {
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
