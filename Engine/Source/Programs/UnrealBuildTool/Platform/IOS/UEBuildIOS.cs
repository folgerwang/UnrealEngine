// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Xml;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// IOS-specific target settings
	/// </summary>
	public class IOSTargetRules
	{
		/// <summary>
		/// Don't generate crashlytics data
		/// </summary>
		[CommandLine("-skipcrashlytics")]
		public bool bSkipCrashlytics = false;

		/// <summary>
		/// Manual override for the provision to use. Should be a full path.
		/// </summary>
		[CommandLine("-ImportProvision=")]
		public string ImportProvision = null;

		/// <summary>
		/// Imports the given certificate (inc private key) into a temporary keychain before signing.
		/// </summary>
		[CommandLine("-ImportCertificate=")]
		public string ImportCertificate = null;

		/// <summary>
		/// Password for the imported certificate
		/// </summary>
		[CommandLine("-ImportCertificatePassword=")]
		public string ImportCertificatePassword = null;
	}

	/// <summary>
	/// Read-only wrapper for IOS-specific target settings
	/// </summary>
	public class ReadOnlyIOSTargetRules
	{
		/// <summary>
		/// The private mutable settings object
		/// </summary>
		private IOSTargetRules Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The settings object to wrap</param>
		public ReadOnlyIOSTargetRules(IOSTargetRules Inner)
		{
			this.Inner = Inner;
		}

		/// <summary>
		/// Accessors for fields on the inner TargetRules instance
		/// </summary>
		#region Read-only accessor properties 
#if !__MonoCS__
#pragma warning disable CS1591
#endif
		public bool bSkipCrashlytics
		{
			get { return Inner.bSkipCrashlytics; }
		}

		public string ImportProvision
		{
			get { return Inner.ImportProvision; }
		}

		public string ImportCertificate
		{
			get { return Inner.ImportCertificate; }
		}

		public string ImportCertificatePassword
		{
			get { return Inner.ImportCertificatePassword; }
		}

#if !__MonoCS__
#pragma warning restore CS1591
#endif
		#endregion
	}

	/// <summary>
	/// Stores project-specific IOS settings. Instances of this object are cached by IOSPlatform.
	/// </summary>
	class IOSProjectSettings
	{
		/// <summary>
		/// The cached project file location
		/// </summary>
		public readonly FileReference ProjectFile;

		/// <summary>
		/// Whether to generate a dSYM file or not.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGeneratedSYMFile")]
		[CommandLine("-skipgeneratedsymfile", Value="false")]
		public readonly bool bGeneratedSYMFile = true;

		/// <summary>
		/// Whether to generate a dSYM bundle or not.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGeneratedSYMBundle")]
		[CommandLine("-skipgeneratedsymbundle", Value = "false")]
		public readonly bool bGeneratedSYMBundle = false;

        /// <summary>
        /// Whether to generate a dSYM file or not.
        /// </summary>
        [ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGenerateCrashReportSymbols")]
        public readonly bool bGenerateCrashReportSymbols = false;

        /// <summary>
        /// The minimum supported version
        /// </summary>
        [ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "MinimumiOSVersion")]
		private readonly string MinimumIOSVersion = null;

		/// <summary>
		/// Whether to support iPhone
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsIPhone")]
		private readonly bool bSupportsIPhone = true;

		/// <summary>
		/// Whether to support iPad
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsIPad")]
		private readonly bool bSupportsIPad = true;

		/// <summary>
		/// Whether to target ArmV7
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bDevForArmV7")]
		private readonly bool bDevForArmV7 = false;

		/// <summary>
		/// Whether to target Arm64
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bDevForArm64")]
		private readonly bool bDevForArm64 = false;

		/// <summary>
		/// Whether to target ArmV7S
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bDevForArmV7S")]
		private readonly bool bDevForArmV7S = false;

		/// <summary>
		/// Whether to target ArmV7 for shipping configurations
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bShipForArmV7")]
		private readonly bool bShipForArmV7 = false;

		/// <summary>
		/// Whether to target Arm64 for shipping configurations
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bShipForArm64")]
		private readonly bool bShipForArm64 = false;

		/// <summary>
		/// Whether to target ArmV7S for shipping configurations
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bShipForArmV7S")]
		private readonly bool bShipForArmV7S = false;

		/// <summary>
		/// additional linker flags for shipping
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "AdditionalShippingLinkerFlags")]
		public readonly string AdditionalShippingLinkerFlags = "";

		/// <summary>
		/// additional linker flags for non-shipping
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "AdditionalLinkerFlags")]
		public readonly string AdditionalLinkerFlags = "";

		/// <summary>
		/// mobile provision to use for code signing
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "MobileProvision")]
		public readonly string MobileProvision = "";

        /// <summary>
        /// signing certificate to use for code signing
        /// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "SigningCertificate")]
        public readonly string SigningCertificate = "";


		/// <summary>
		/// true if bit code should be embedded
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bShipForBitcode")]
		public readonly bool bShipForBitcode = false;

        /// <summary>
        /// true if notifications are enabled
        /// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bEnableRemoteNotificationsSupport")]
        public readonly bool bNotificationsEnabled = false;

		/// <summary>
		/// The bundle identifier
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "BundleIdentifier")]
		public readonly string BundleIdentifier = "";

		/// <summary>
		/// true if using Xcode managed provisioning, else false
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bAutomaticSigning")]
		public readonly bool bAutomaticSigning = false;

		/// <summary>
		/// The IOS Team ID
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "IOSTeamID")]
		public readonly string TeamID = "";

		/// <summary>
		/// true to change FORCEINLINE to a regular INLINE.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bDisableForceInline")]
		public readonly bool bDisableForceInline = false;
		
		/// <summary>
		/// Returns a list of all the non-shipping architectures which are supported
		/// </summary>
		public IEnumerable<string> NonShippingArchitectures
		{
			get
			{
				if(bDevForArmV7)
				{
					yield return "armv7";
				}
				if(bDevForArm64 || (!bDevForArmV7 && !bDevForArmV7S))
				{
					yield return "arm64";
				}
				if(bDevForArmV7S)
				{
					yield return "armv7s";
				}
			}
		}

		/// <summary>
		/// Returns a list of all the shipping architectures which are supported
		/// </summary>
		public IEnumerable<string> ShippingArchitectures
		{
			get
			{
				if(bShipForArmV7)
				{
					yield return "armv7";
				}
				if(bShipForArm64 || (!bShipForArmV7 && !bShipForArmV7S))
				{
					yield return "arm64";
				}
				if(bShipForArmV7S)
				{
					yield return "armv7s";
				}
			}
		}

		/// <summary>
		/// Which version of the iOS to allow at run time
		/// </summary>
		public virtual string RuntimeVersion
		{
			get
			{
				switch (MinimumIOSVersion)
				{
					case "IOS_10":
						return "10.0";
					case "IOS_11":
						return "11.0";
					default:
						return "9.0";
				}
			}
		}

		/// <summary>
		/// which devices the game is allowed to run on
		/// </summary>
		public virtual string RuntimeDevices
		{
			get
			{
				if (bSupportsIPad && !bSupportsIPhone)
				{
					return "2";
				}
				else if (!bSupportsIPad && bSupportsIPhone)
				{
					return "1";
				}
				else
				{
					return "1,2";
				}
			}
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="ProjectFile">The project file to read settings for</param>
		public IOSProjectSettings(FileReference ProjectFile) 
			: this(ProjectFile, UnrealTargetPlatform.IOS)
		{
		}

		/// <summary>
		/// Protected constructor. Used by TVOSProjectSettings.
		/// </summary>
		/// <param name="ProjectFile">The project file to read settings for</param>
		/// <param name="Platform">The platform to read settings for</param>
		protected IOSProjectSettings(FileReference ProjectFile, UnrealTargetPlatform Platform)
		{
			this.ProjectFile = ProjectFile;
			ConfigCache.ReadSettings(DirectoryReference.FromFile(ProjectFile), Platform, this);
            BundleIdentifier = BundleIdentifier.Replace("[PROJECT_NAME]", ((ProjectFile != null) ? ProjectFile.GetFileNameWithoutAnyExtensions() : "UE4Game")).Replace("_", "");
		}
	}

	/// <summary>
	/// IOS provisioning data
	/// </summary>
    class IOSProvisioningData
    {
		public string SigningCertificate;
		public FileReference MobileProvisionFile;
        public string MobileProvisionUUID;
        public string MobileProvisionName;
        public string TeamUUID;
		public bool bHaveCertificate = false;

		public string MobileProvision
		{
			get { return (MobileProvisionFile == null)? null : MobileProvisionFile.GetFileName(); }
		}

		public IOSProvisioningData(IOSProjectSettings ProjectSettings, bool bForDistribution)
			: this(ProjectSettings, false, bForDistribution)
		{
		}

		protected IOSProvisioningData(IOSProjectSettings ProjectSettings, bool bIsTVOS, bool bForDistribtion)
		{
            SigningCertificate = ProjectSettings.SigningCertificate;
            string MobileProvision = ProjectSettings.MobileProvision;

			FileReference ProjectFile = ProjectSettings.ProjectFile;
            if (!string.IsNullOrEmpty(SigningCertificate))
            {
                // verify the certificate
                Process IPPProcess = new Process();
                if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
                {
                    string IPPCmd = "\"" + UnrealBuildTool.EngineDirectory + "/Binaries/DotNET/IOS/IPhonePackager.exe\" certificates " + ((ProjectFile != null) ? ("\"" + ProjectFile.ToString() + "\"") : "Engine") + " -bundlename " + ProjectSettings.BundleIdentifier + (bForDistribtion ? " -distribution" : "");
                    IPPProcess.StartInfo.WorkingDirectory = UnrealBuildTool.EngineDirectory.ToString();
                    IPPProcess.StartInfo.FileName = UnrealBuildTool.EngineDirectory + "/Build/BatchFiles/Mac/RunMono.sh";
                    IPPProcess.StartInfo.Arguments = IPPCmd;
                    IPPProcess.OutputDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                    IPPProcess.ErrorDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                }
                else
                {
					string IPPCmd = "certificates " + ((ProjectFile != null) ? ("\"" + ProjectFile.ToString() + "\"") : "Engine") + " -bundlename " + ProjectSettings.BundleIdentifier + (bForDistribtion ? " -distribution" : "");
                    IPPProcess.StartInfo.WorkingDirectory = UnrealBuildTool.EngineDirectory.ToString();
                    IPPProcess.StartInfo.FileName = UnrealBuildTool.EngineDirectory + "\\Binaries\\DotNET\\IOS\\IPhonePackager.exe";
                    IPPProcess.StartInfo.Arguments = IPPCmd;
                    IPPProcess.OutputDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                    IPPProcess.ErrorDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                }
                Utils.RunLocalProcess(IPPProcess);
            }
            else
            {
                SigningCertificate = bForDistribtion ? "iPhone Distribution" : "iPhone Developer";
                bHaveCertificate = true;
            }

			if(!string.IsNullOrEmpty(MobileProvision))
			{
				DirectoryReference MobileProvisionDir;
				if(BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					MobileProvisionDir = DirectoryReference.Combine(new DirectoryReference(Environment.GetEnvironmentVariable("HOME")), "Library", "MobileDevice", "Provisioning Profiles");
				}
				else
				{
					MobileProvisionDir = DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.LocalApplicationData), "Apple Computer", "MobileDevice", "Provisioning Profiles");
				}

				FileReference PossibleMobileProvisionFile = FileReference.Combine(MobileProvisionDir, MobileProvision);
				if(FileReference.Exists(PossibleMobileProvisionFile))
				{
					MobileProvisionFile = PossibleMobileProvisionFile;
				}
			}

            if (MobileProvisionFile == null || !bHaveCertificate)
            {

                SigningCertificate = "";
                MobileProvision = "";
				MobileProvisionFile = null;
                Log.TraceLog("Provision not specified or not found for " + ((ProjectFile != null) ? ProjectFile.GetFileNameWithoutAnyExtensions() : "UE4Game") + ", searching for compatible match...");
                Process IPPProcess = new Process();
                if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
                {
                    string IPPCmd = "\"" + UnrealBuildTool.EngineDirectory + "/Binaries/DotNET/IOS/IPhonePackager.exe\" signing_match " + ((ProjectFile != null) ? ("\"" + ProjectFile.ToString() + "\"") : "Engine") + " -bundlename " + ProjectSettings.BundleIdentifier + (bIsTVOS ? " -tvos" : "") + (bForDistribtion ? " -distribution" : "");
                    IPPProcess.StartInfo.WorkingDirectory = UnrealBuildTool.EngineDirectory.ToString();
                    IPPProcess.StartInfo.FileName = UnrealBuildTool.EngineDirectory + "/Build/BatchFiles/Mac/RunMono.sh";
                    IPPProcess.StartInfo.Arguments = IPPCmd;
                    IPPProcess.OutputDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                    IPPProcess.ErrorDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                }
                else
                {
                    string IPPCmd = "signing_match " + ((ProjectFile != null) ? ("\"" + ProjectFile.ToString() + "\"") : "Engine") + " -bundlename " + ProjectSettings.BundleIdentifier + (bIsTVOS ? " -tvos" : "") + (bForDistribtion ? " -distribution" : "");
                    IPPProcess.StartInfo.WorkingDirectory = UnrealBuildTool.EngineDirectory.ToString();
                    IPPProcess.StartInfo.FileName = UnrealBuildTool.EngineDirectory + "\\Binaries\\DotNET\\IOS\\IPhonePackager.exe";
                    IPPProcess.StartInfo.Arguments = IPPCmd;
                    IPPProcess.OutputDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                    IPPProcess.ErrorDataReceived += new DataReceivedEventHandler(IPPDataReceivedHandler);
                }
                Utils.RunLocalProcess(IPPProcess);
				if(MobileProvisionFile != null)
				{
					Log.TraceLog("Provision found for " + ((ProjectFile != null) ? ProjectFile.GetFileNameWithoutAnyExtensions() : "UE4Game") + ", Provision: " + MobileProvisionFile + " Certificate: " + SigningCertificate);
				}
            }

            // add to the dictionary
            SigningCertificate = SigningCertificate.Replace("\"", "");

            // read the provision to get the UUID
			if(MobileProvisionFile == null)
			{
				Log.TraceLog("No matching provision file was discovered for {0}. Please ensure you have a compatible provision installed.", ProjectFile);
			}
			else if(!FileReference.Exists(MobileProvisionFile))
			{
				Log.TraceLog("Selected mobile provision for {0} ({1}) was not found. Please ensure you have a compatible provision installed.", ProjectFile, MobileProvisionFile);
			}
			else
            {
				byte[] AllBytes = FileReference.ReadAllBytes(MobileProvisionFile);

				uint StartIndex = (uint)AllBytes.Length;
				uint EndIndex = (uint)AllBytes.Length;

				for (uint i = 0; i + 4 < AllBytes.Length; i++)
				{
					if (AllBytes[i] == '<' && AllBytes[i+1] == '?' && AllBytes[i+ 2] == 'x' && AllBytes[i+ 3] == 'm' && AllBytes[i+ 4] == 'l')
					{
						StartIndex = i;
						break;
					}
				}

				if (StartIndex < AllBytes.Length)
				{
					for (uint i = StartIndex; i + 7 < AllBytes.Length; i++)
					{
						if(AllBytes[i] == '<' && AllBytes[i + 1] == '/' && AllBytes[i + 2] == 'p' && AllBytes[i + 3] == 'l' && AllBytes[i + 4] == 'i' && AllBytes[i + 5] == 's' && AllBytes[i + 6] == 't' && AllBytes[i + 7] == '>')
						{
							EndIndex = i+7;
							break;
						}
					}
				}

				if (StartIndex < AllBytes.Length && EndIndex < AllBytes.Length)
				{
					byte[] TextBytes = new byte[EndIndex - StartIndex];
					Buffer.BlockCopy(AllBytes, (int)StartIndex, TextBytes, 0, (int)(EndIndex - StartIndex));

					string AllText = Encoding.UTF8.GetString(TextBytes);
					int idx = AllText.IndexOf("<key>UUID</key>");
					if (idx > 0)
					{
						idx = AllText.IndexOf("<string>", idx);
						if (idx > 0)
						{
							idx += "<string>".Length;
							MobileProvisionUUID = AllText.Substring(idx, AllText.IndexOf("</string>", idx) - idx);
						}
					}
					idx = AllText.IndexOf("<key>com.apple.developer.team-identifier</key>");
					if (idx > 0)
					{
						idx = AllText.IndexOf("<string>", idx);
						if (idx > 0)
						{
							idx += "<string>".Length;
							TeamUUID = AllText.Substring(idx, AllText.IndexOf("</string>", idx) - idx);
						}
					}
                    idx = AllText.IndexOf("<key>Name</key>");
                    if (idx > 0)
                    {
                        idx = AllText.IndexOf("<string>", idx);
                        if (idx > 0)
                        {
                            idx += "<string>".Length;
                            MobileProvisionName = AllText.Substring(idx, AllText.IndexOf("</string>", idx) - idx);
                        }
                    }
				}

				if (string.IsNullOrEmpty(MobileProvisionUUID) || string.IsNullOrEmpty(TeamUUID))
				{
					MobileProvision = null;
					SigningCertificate = null;
					Log.TraceLog("Failed to parse the mobile provisioning profile.");
				}
            }
		}

        void IPPDataReceivedHandler(Object Sender, DataReceivedEventArgs Line)
        {
            if ((Line != null) && (Line.Data != null))
            {
				Log.TraceLog("{0}", Line.Data);
                if (!string.IsNullOrEmpty(SigningCertificate))
                {
                    if (Line.Data.Contains("CERTIFICATE-") && Line.Data.Contains(SigningCertificate))
                    {
                        bHaveCertificate = true;
                    }
                }
                else
                {
                    int cindex = Line.Data.IndexOf("CERTIFICATE-");
                    int pindex = Line.Data.IndexOf("PROVISION-");
                    if (cindex > -1 && pindex > -1)
                    {
                        cindex += "CERTIFICATE-".Length;
                        SigningCertificate = Line.Data.Substring(cindex, pindex - cindex - 1);
                        pindex += "PROVISION-".Length;
						if(pindex < Line.Data.Length)
						{
							MobileProvisionFile = new FileReference(Line.Data.Substring(pindex));
						}
                    }
                }
            }
        }
    }

	class IOSPlatform : UEBuildPlatform
	{
		IOSPlatformSDK SDK;
		List<IOSProjectSettings> CachedProjectSettings = new List<IOSProjectSettings>();
        Dictionary<string, IOSProvisioningData> ProvisionCache = new Dictionary<string, IOSProvisioningData>();

		// by default, use an empty architecture (which is really just a modifer to the platform for some paths/names)
		public static string IOSArchitecture = "";

		public IOSPlatform(IOSPlatformSDK InSDK)
			: this(InSDK, UnrealTargetPlatform.IOS, CppPlatform.IOS)
		{
		}

		protected IOSPlatform(IOSPlatformSDK InSDK, UnrealTargetPlatform TargetPlatform, CppPlatform CPPPlatform)
			: base(TargetPlatform, CPPPlatform)
		{
			SDK = InSDK;
		}

        // The current architecture - affects everything about how UBT operates on IOS
        public override string GetDefaultArchitecture(FileReference ProjectFile)
		{
			return IOSArchitecture;
		}

		public override void ResetTarget(TargetRules Target)
		{
			// we currently don't have any simulator libs for PhysX
			if (Target.Architecture == "-simulator")
			{
				Target.bCompilePhysX = false;
			}

			Target.bBuildEditor = false;
			Target.bBuildDeveloperTools = false;
			Target.bCompileAPEX = false;
            Target.bCompileNvCloth = false;
            Target.bCompileSimplygon = false;
            Target.bCompileSimplygonSSF = false;
			Target.bBuildDeveloperTools = false;

			Target.bDeployAfterCompile = true;
		}

		public override void ValidateTarget(TargetRules Target)
		{
			// we assume now we are building with IOS8 or later
			if (Target.bCompileAgainstEngine)
			{
				Target.GlobalDefinitions.Add("HAS_METAL=1");
				Target.ExtraModuleNames.Add("MetalRHI");
			}
			else
			{
				Target.GlobalDefinitions.Add("HAS_METAL=0");
			}

			Target.bCheckSystemHeadersForModification = false;
		}

		public override SDKStatus HasRequiredSDKsInstalled()
		{
			return SDK.HasRequiredSDKsInstalled();
		}

		/// <summary>
		/// Determines if the given name is a build product for a target.
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UE4Editor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <returns>True if the string matches the name of a build product, false otherwise</returns>
		public override bool IsBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes)
		{
			return IsBuildProductName(FileName, NamePrefixes, NameSuffixes, "")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".stub")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dylib")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dSYM")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dSYM.zip")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".o");
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The binary extenstion (ie 'exe' or 'dll')</returns>
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".dylib";
				case UEBuildBinaryType.Executable:
					return "";
				case UEBuildBinaryType.StaticLibrary:
					return ".a";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		public IOSProjectSettings ReadProjectSettings(FileReference ProjectFile)
		{
			IOSProjectSettings ProjectSettings = CachedProjectSettings.FirstOrDefault(x => x.ProjectFile == ProjectFile);
			if(ProjectSettings == null)
			{
				ProjectSettings = CreateProjectSettings(ProjectFile);
				CachedProjectSettings.Add(ProjectSettings);
			}
			return ProjectSettings;
		}

		protected virtual IOSProjectSettings CreateProjectSettings(FileReference ProjectFile)
		{
			return new IOSProjectSettings(ProjectFile);
		}

		public IOSProvisioningData ReadProvisioningData(FileReference ProjectFile, bool bForDistribution = false)
		{
			IOSProjectSettings ProjectSettings = ReadProjectSettings(ProjectFile);
			return ReadProvisioningData(ProjectSettings, bForDistribution);
		}

		public IOSProvisioningData ReadProvisioningData(IOSProjectSettings ProjectSettings, bool bForDistribution = false)
        {
			string ProvisionKey = ProjectSettings.BundleIdentifier + " " + bForDistribution.ToString();

            IOSProvisioningData ProvisioningData;
			if(!ProvisionCache.TryGetValue(ProvisionKey, out ProvisioningData))
            {
				ProvisioningData = CreateProvisioningData(ProjectSettings, bForDistribution);
                ProvisionCache.Add(ProvisionKey, ProvisioningData);
            }
			return ProvisioningData;
        }

		protected virtual IOSProvisioningData CreateProvisioningData(IOSProjectSettings ProjectSettings, bool bForDistribution)
		{
			return new IOSProvisioningData(ProjectSettings, bForDistribution);
		}

		public override string[] GetDebugInfoExtensions(ReadOnlyTargetRules InTarget, UEBuildBinaryType InBinaryType)
		{
			IOSProjectSettings ProjectSettings = ReadProjectSettings(InTarget.ProjectFile);

			if(ProjectSettings.bGeneratedSYMBundle)
			{
				return new string[] {".dSYM.zip"};
			}
			else if (ProjectSettings.bGeneratedSYMFile)
            {
                return new string[] {".dSYM"};
            }

            return new string [] {};
		}

		public override bool CanUseXGE()
		{
			return false;
		}

		public override bool CanUseDistcc()
		{
			return true;
		}

		public override void PreBuildSync()
		{
			IOSToolChain.PreBuildSync();
		}

		public override void PostBuildSync(UEBuildTarget Target)
		{
			IOSToolChain.PostBuildSync(Target);
		}

		public bool HasCustomIcons(DirectoryReference ProjectDirectoryName)
		{
			string IconDir = Path.Combine(ProjectDirectoryName.FullName, "Build", "IOS", "Resources", "Graphics");
			if(Directory.Exists(IconDir))
			{
				foreach (string f in Directory.EnumerateFiles(IconDir))
				{
					if (f.Contains("Icon") && Path.GetExtension(f).Contains(".png"))
					{
						return true;
					}
				}
			}
			return false;
		}

		/// <summary>
		/// Check for the default configuration
		/// return true if the project uses the default build config
		/// </summary>
		public override bool HasDefaultBuildConfig(UnrealTargetPlatform Platform, DirectoryReference ProjectDirectoryName)
		{
			string[] BoolKeys = new string[] {
				"bDevForArmV7", "bDevForArm64", "bDevForArmV7S", "bShipForArmV7", 
				"bShipForArm64", "bShipForArmV7S", "bShipForBitcode", "bGeneratedSYMFile",
				"bGeneratedSYMBundle", "bEnableRemoteNotificationsSupport", "bEnableCloudKitSupport",
                "bGenerateCrashReportSymbols"
            };
			string[] StringKeys = new string[] {
				"MinimumiOSVersion", 
				"AdditionalLinkerFlags",
				"AdditionalShippingLinkerFlags"
			};

			// check for custom icons
			if (HasCustomIcons(ProjectDirectoryName))
			{
				return false;
			}

			// look up iOS specific settings
			if (!DoProjectSettingsMatchDefault(Platform, ProjectDirectoryName, "/Script/IOSRuntimeSettings.IOSRuntimeSettings",
					BoolKeys, null, StringKeys))
			{
				return false;
			}

			// check the base settings
			return base.HasDefaultBuildConfig(Platform, ProjectDirectoryName);
		}

		/// <summary>
		/// Check for the build requirement due to platform requirements
		/// return true if the project requires a build
		/// </summary>
		public override bool RequiresBuild(UnrealTargetPlatform Platform, DirectoryReference ProjectDirectoryName)
		{
			// check for custom icons
			return HasCustomIcons(ProjectDirectoryName);
		}

		public override bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true; // for iOS can only run cooked. this is mostly for testing console code paths.
		}

		public override bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
		{
			// This platform currently always compiles monolithic
			return true;
		}

		/// <summary>
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Mac))
			{
				bool bBuildShaderFormats = Target.bForceBuildShaderFormats;
				if (!Target.bBuildRequiresCookedData)
				{
					if (ModuleName == "Engine")
					{
						if (Target.bBuildDeveloperTools)
						{
							Rules.DynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
							Rules.DynamicallyLoadedModuleNames.Add("TVOSTargetPlatform");
						}
					}
					else if (ModuleName == "TargetPlatform")
					{
						bBuildShaderFormats = true;
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatPVR");
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatASTC");
						if (Target.bBuildDeveloperTools && Target.bCompileAgainstEngine)
						{
							Rules.DynamicallyLoadedModuleNames.Add("AudioFormatADPCM");
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (ModuleName == "TargetPlatform")
				{
					if (Target.bForceBuildTargetPlatforms)
					{
						Rules.DynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
						Rules.DynamicallyLoadedModuleNames.Add("TVOSTargetPlatform");
					}

					if (bBuildShaderFormats)
					{
						Rules.DynamicallyLoadedModuleNames.Add("MetalShaderFormat");
					}
				}
			}
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>bool    true if debug info should be generated, false if not</returns>
		public override bool ShouldCreateDebugInfo(ReadOnlyTargetRules Target)
		{
			return true;
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="Target">Settings for the target being compiled</param>
		/// <param name="CompileEnvironment">The compile environment for this target</param>
		/// <param name="LinkEnvironment">The link environment for this target</param>
		public override void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			CompileEnvironment.Definitions.Add("PLATFORM_IOS=1");
			CompileEnvironment.Definitions.Add("PLATFORM_APPLE=1");

			CompileEnvironment.Definitions.Add("WITH_TTS=0");
			CompileEnvironment.Definitions.Add("WITH_SPEECH_RECOGNITION=0");
			CompileEnvironment.Definitions.Add("WITH_DATABASE_SUPPORT=0");
			CompileEnvironment.Definitions.Add("WITH_EDITOR=0");
			CompileEnvironment.Definitions.Add("USE_NULL_RHI=0");

			IOSProjectSettings ProjectSettings = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(Target.Platform)).ReadProjectSettings(Target.ProjectFile);
			if (ProjectSettings.bNotificationsEnabled)
			{
				CompileEnvironment.Definitions.Add("NOTIFICATIONS_ENABLED=1");
			}
			else
			{
				CompileEnvironment.Definitions.Add("NOTIFICATIONS_ENABLED=0");
			}

			CompileEnvironment.Definitions.Add("UE_DISABLE_FORCE_INLINE=" + (ProjectSettings.bDisableForceInline ? "1" : "0"));

			if (Target.Architecture == "-simulator")
			{
				CompileEnvironment.Definitions.Add("WITH_SIMULATOR=1");
			}
			else
			{
				CompileEnvironment.Definitions.Add("WITH_SIMULATOR=0");
			}

			LinkEnvironment.AdditionalFrameworks.Add(new UEBuildFramework("GameKit"));
			LinkEnvironment.AdditionalFrameworks.Add(new UEBuildFramework("StoreKit"));
			LinkEnvironment.AdditionalFrameworks.Add(new UEBuildFramework("DeviceCheck"));
		}

		/// <summary>
		/// Modify the rules for a newly created module, in a target that's being built for this platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForActivePlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
		}

		/// <summary>
		/// Creates a toolchain instance for the given platform.
		/// </summary>
		/// <param name="CppPlatform">The platform to create a toolchain for</param>
		/// <param name="Target">The target being built</param>
		/// <returns>New toolchain instance.</returns>
		public override UEToolChain CreateToolChain(CppPlatform CppPlatform, ReadOnlyTargetRules Target)
		{
			IOSProjectSettings ProjectSettings = ReadProjectSettings(Target.ProjectFile);
			return new IOSToolChain(Target.ProjectFile, ProjectSettings);
		}

		/// <summary>
		/// Deploys the given target
		/// </summary>
		/// <param name="Target">Information about the target being deployed</param>
		public override void Deploy(UEBuildDeployTarget Target)
		{
			new UEDeployIOS().PrepTargetForDeployment(Target);
		}
	}

	class IOSPlatformSDK : UEBuildPlatformSDK
	{
		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			if (!Utils.IsRunningOnMono)
			{
				// check to see if iTunes is installed
				string dllPath = Microsoft.Win32.Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared", "iTunesMobileDeviceDLL", null) as string;
				if (String.IsNullOrEmpty(dllPath) || !File.Exists(dllPath))
				{
					dllPath = Microsoft.Win32.Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared", "MobileDeviceDLL", null) as string;
					if (String.IsNullOrEmpty(dllPath) || !File.Exists(dllPath))
					{
						return SDKStatus.Invalid;
					}
				}
			}
			return SDKStatus.Valid;
		}
	}

	class IOSPlatformFactory : UEBuildPlatformFactory
	{
		protected override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.IOS; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		protected override void RegisterBuildPlatforms(SDKOutputLevel OutputLevel)
		{
			IOSPlatformSDK SDK = new IOSPlatformSDK();
			SDK.ManageAndValidateSDK(OutputLevel);

			// Register this build platform for IOS
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.IOS.ToString());
			UEBuildPlatform.RegisterBuildPlatform(new IOSPlatform(SDK));
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.Apple);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.IOS);
		}
	}
}

