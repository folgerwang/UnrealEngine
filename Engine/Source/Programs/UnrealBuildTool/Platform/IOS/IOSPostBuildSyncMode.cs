// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	[Serializable]
	class IOSPostBuildSyncTarget
	{
		public UnrealTargetPlatform Platform;
		public UnrealTargetConfiguration Configuration;
		public FileReference ProjectFile;
		public string TargetName;
		public TargetType TargetType;
		public FileReference OutputPath;
		public List<string> UPLScripts;
		public VersionNumber SdkVersion;
		public bool bCreateStubIPA;
		public bool bSkipCrashlytics;
		public DirectoryReference ProjectDirectory;
		public DirectoryReference ProjectIntermediateDirectory;
		public string ImportProvision;
		public string ImportCertificate;
		public string ImportCertificatePassword;
		public Dictionary<string, DirectoryReference> FrameworkNameToSourceDir;
		public bool bForDistribution = false;

		public IOSPostBuildSyncTarget(ReadOnlyTargetRules Target, FileReference OutputPath, DirectoryReference ProjectIntermediateDirectory, List<string> UPLScripts, VersionNumber SdkVersion, Dictionary<string, DirectoryReference> FrameworkNameToSourceDir)
		{
			this.Platform = Target.Platform;
			this.Configuration = Target.Configuration;
			this.ProjectFile = Target.ProjectFile;
			this.TargetName = Target.Name;
			this.TargetType = Target.Type;
			this.OutputPath = OutputPath;
			this.UPLScripts = UPLScripts;
			this.SdkVersion = SdkVersion;
			this.bCreateStubIPA = Target.IOSPlatform.bCreateStubIPA;
			this.bSkipCrashlytics = Target.IOSPlatform.bSkipCrashlytics;
			this.ProjectDirectory = DirectoryReference.FromFile(Target.ProjectFile) ?? UnrealBuildTool.EngineDirectory;
			this.ProjectIntermediateDirectory = ProjectIntermediateDirectory;
			this.ImportProvision = Target.IOSPlatform.ImportProvision;
			this.ImportCertificate = Target.IOSPlatform.ImportCertificate;
			this.ImportCertificatePassword = Target.IOSPlatform.ImportCertificatePassword;
			this.FrameworkNameToSourceDir = FrameworkNameToSourceDir;
			this.bForDistribution = Target.IOSPlatform.bForDistribution;
		}
	}

	[ToolMode("IOSPostBuildSync", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms)]
	class IOSPostBuildSyncMode : ToolMode
	{
		[CommandLine("-Input=", Required = true)]
		public FileReference InputFile = null;

		[CommandLine("-XmlConfigCache=")]
		public FileReference XmlConfigCache = null;

		public override int Execute(CommandLineArguments Arguments)
		{
			Arguments.ApplyTo(this);
			Arguments.CheckAllArgumentsUsed();

			// Run the PostBuildSync command
			IOSPostBuildSyncTarget Target = BinaryFormatterUtils.Load<IOSPostBuildSyncTarget>(InputFile);
			IOSToolChain.PostBuildSync(Target);

			return 0;
		}
	}
}
