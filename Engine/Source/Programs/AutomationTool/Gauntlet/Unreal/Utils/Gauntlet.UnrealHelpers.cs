// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using AutomationTool;
using UnrealBuildTool;
using System.Linq;
using System.Net.NetworkInformation;
using System.Text.RegularExpressions;

namespace Gauntlet
{
	/// <summary>
	/// Defines roles that can be performed for Unreal
	/// 
	/// TODO - Editor variants should be a tag, not an enum?
	/// </summary>
	public enum UnrealTargetRole
	{
		Unknown,
		Editor,
		EditorGame,
		EditorServer,
		Client,
		Server,
	};

    /// <summary>
    /// Base class of a file to copy as part of a test.
    /// </summary>
    public class UnrealFileToCopy
    {
        /// <summary>
        /// Base file to copy, including filename and extension.
        /// </summary>
        public string SourceFileLocation;
        /// <summary>
        /// The high-level base directory we want to copy to.
        /// </summary>
        public EIntendedBaseCopyDirectory TargetBaseDirectory;
        /// <summary>
        /// Where to copy the file to, including filename and extension, relative to the intended base directory.
        /// Should be any additional subdirectories, plus file name.
        /// </summary>
        public string TargetRelativeLocation;
        public UnrealFileToCopy(string SourceLoc, EIntendedBaseCopyDirectory TargetDirType, string TargetLoc)
        {
            SourceFileLocation = SourceLoc;
            TargetBaseDirectory = TargetDirType;
            TargetRelativeLocation = TargetLoc;
        }
    }

    /// <summary>
    /// Helper extensions for our enums
    /// </summary>
    public static class UnrealRoleTypeExtensions
	{
		public static bool UsesEditor(this UnrealTargetRole Type)
		{
			return Type == UnrealTargetRole.Editor || Type == UnrealTargetRole.EditorGame || Type == UnrealTargetRole.EditorServer;
		}

		public static bool IsServer(this UnrealTargetRole Type)
		{
			return Type == UnrealTargetRole.EditorServer || Type == UnrealTargetRole.Server;
		}

		public static bool IsClient(this UnrealTargetRole Type)
		{
			return Type == UnrealTargetRole.EditorGame || Type == UnrealTargetRole.Client;
		}

		public static bool IsEditor(this UnrealTargetRole Type)
		{
			return Type == UnrealTargetRole.Editor;
		}

		public static bool RunsLocally(this UnrealTargetRole Type)
		{
			return UsesEditor(Type) || IsServer(Type);
		}
	}

	/// <summary>
	/// Utility functions for Unreal tests
	/// </summary>
	public static class UnrealHelpers
	{
		public static UnrealTargetPlatform GetPlatformFromString(string PlatformName)
		{
			UnrealTargetPlatform UnrealPlatform = UnrealTargetPlatform.Unknown;

			if (Enum.TryParse<UnrealTargetPlatform>(PlatformName, true, out UnrealPlatform))
			{
				return UnrealPlatform;
			}
			
			throw new AutomationException("Unable convert platform {0} into a valid Unreal Platform", PlatformName);
		}

		/// <summary>
		/// Given a platform and a client/server flag, returns the name Unreal refers to it as. E.g. "WindowsClient", "LinuxServer".
		/// </summary>
		/// <param name="InTargetPlatform"></param>
		/// <param name="InTargetType"></param>
		/// <returns></returns>
		public static string GetPlatformName(UnrealTargetPlatform TargetPlatform, UnrealTargetRole ProcessType, bool UsesSharedBuildType)
		{
			string Platform = "";

			bool IsDesktop = UnrealBuildTool.Utils.GetPlatformsInClass(UnrealPlatformClass.Desktop).Contains(TargetPlatform);

			// These platforms can be built as either game, server or client
			if (IsDesktop)
			{
				Platform = (TargetPlatform == UnrealTargetPlatform.Win32 || TargetPlatform == UnrealTargetPlatform.Win64) ? "Windows" : TargetPlatform.ToString();

				if (ProcessType == UnrealTargetRole.Client)
				{
					if (UsesSharedBuildType)
					{
						Platform += "NoEditor";
					}
					else
					{
						Platform += "Client";
					}
				}
				else if (ProcessType == UnrealTargetRole.Server)
				{
					if (UsesSharedBuildType)
					{
						Platform += "NoEditor";
					}
					else
					{
						Platform += "Server";
					}
				}
			}
			else if (TargetPlatform == UnrealTargetPlatform.Android)
			{
				// TODO: hardcoded ETC2 for now, need to determine cook flavor used.
				// actual flavour required may depend on the target HW...
				if (UsesSharedBuildType)
				{
					Platform = TargetPlatform.ToString() + "_ETC2";
				}
				else
				{
					Platform = TargetPlatform.ToString() + "_ETC2Client";
				}
			}
			else
			{
				Platform = TargetPlatform.ToString();
			}

			return Platform;
		}

		/// <summary>
		/// Gets the filehost IP to provide to devkits by examining our local adapters and
		/// returning the one that's active and on the local LAN (based on DNS assignment)
		/// </summary>
		/// <returns></returns>
		public static string GetHostIpAddress()
		{
			NetworkInterface[] Interfaces = NetworkInterface.GetAllNetworkInterfaces();
			foreach (NetworkInterface adapter in Interfaces)
			{
				if (adapter.OperationalStatus == OperationalStatus.Up)
				{
					IPInterfaceProperties IP = adapter.GetIPProperties();
					for (int Index = 0; Index < IP.UnicastAddresses.Count; ++Index)
					{
						if (IP.UnicastAddresses[Index].IsDnsEligible)
						{
							// use default port
							return IP.UnicastAddresses[Index].Address.ToString();
						}
					}
				}
			}
			return "";
		}

		static public string GetExecutableName(string ProjectName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Config, UnrealTargetRole Role, string Extension)
		{
			string ExeName = ProjectName;

			if (Role == UnrealTargetRole.Server)
			{
				ExeName = Regex.Replace(ExeName, "Game", "Server", RegexOptions.IgnoreCase);
			}
			else if (Role == UnrealTargetRole.Client)
			{
				ExeName = Regex.Replace(ExeName, "Game", "Client", RegexOptions.IgnoreCase);
			}

			if (Config != UnrealTargetConfiguration.Development)
			{
				ExeName += string.Format("-{0}-{1}", Platform, Config);
			}

			// todo , how to find this?
			if (Platform == UnrealTargetPlatform.Android)
			{
				ExeName += "-arm64-es2";
			}

			// not all platforms use an extension
			if (!string.IsNullOrEmpty(Extension))
			{
				if (Extension.StartsWith(".") == false)
				{
					ExeName += ".";
				}

				ExeName += Extension;
			}

			return ExeName;
		}

		internal class ConfigInfo
		{
			public UnrealTargetRole 				RoleType;
			public UnrealTargetPlatform 		Platform;
			public UnrealTargetConfiguration 	Configuration;
			public bool							SharedBuild;

			public ConfigInfo()
			{
				RoleType = UnrealTargetRole.Unknown;
				Platform = UnrealTargetPlatform.Unknown;
				Configuration = UnrealTargetConfiguration.Unknown;
			}
		}

		static ConfigInfo GetUnrealConfigFromFileName(string InProjectName, string InName)
		{
			ConfigInfo Config = new ConfigInfo();

			string ShortName = Regex.Replace(InProjectName, "Game", "", RegexOptions.IgnoreCase);

			if (InName.StartsWith("UE4Game", StringComparison.OrdinalIgnoreCase))
			{
				ShortName = "UE4";
			}

			string AppName = Path.GetFileNameWithoutExtension(InName);

			// A project name may be either something like EngineTest or FortniteGame.
			// The Game, Client, Server executables will be called
			// EngineTest, EngineTestClient, EngineTestServer
			// FortniteGame, FortniteClient, FortniteServer
			// Or EngineTest-WIn64-Shipping, FortniteClient-Win64-Shipping etc
			// So we need to search for the project name minus 'Game', with the form, build-type, and platform all optional :(
			string RegExMatch = string.Format(@"{0}(Game|Client|Server|)(?:-(.+?)-(Test|Shipping))?", ShortName);

			// Format should be something like
			// FortniteClient
			// FortniteClient-Win64-Test
			// Match this and break it down to components
			var NameMatch = Regex.Match(AppName, RegExMatch, RegexOptions.IgnoreCase);

			if (NameMatch.Success)
			{
				string ModuleType = NameMatch.Groups[1].ToString().ToLower();
				string PlatformName = NameMatch.Groups[2].ToString();
				string ConfigType = NameMatch.Groups[3].ToString();

				if (ModuleType.Length == 0 || ModuleType == "game")
				{
					// how to express client&server?
					Config.RoleType = UnrealTargetRole.Client;
					Config.SharedBuild = true;
				}
				else if (ModuleType == "client")
				{
					Config.RoleType = UnrealTargetRole.Client;
				}
				else if (ModuleType == "server")
				{
					Config.RoleType = UnrealTargetRole.Server;
				}

				if (ConfigType.Length > 0)
				{
					Enum.TryParse(ConfigType, true, out Config.Configuration);
				}
				else
				{
					Config.Configuration = UnrealTargetConfiguration.Development;   // Development has no string
				}

				if (PlatformName.Length > 0)
				{
					Enum.TryParse(ConfigType, true, out Config.Platform);
				}
				else
				{
					Config.Platform = UnrealTargetPlatform.Unknown;
				}

				
			}

			return Config;
		}

		static public UnrealTargetConfiguration GetConfigurationFromExecutableName(string InProjectName, string InName)
		{
			return GetUnrealConfigFromFileName(InProjectName, InName).Configuration;
		}

		static public UnrealTargetRole GetRoleFromExecutableName(string InProjectName, string InName)
		{
			return GetUnrealConfigFromFileName(InProjectName, InName).RoleType;
		}

		static public string GetProjectPath(string InProjectName)
		{
			if (File.Exists(InProjectName))
			{
				return InProjectName;
			}

			string ShortName = Path.GetFileNameWithoutExtension(InProjectName);
		
			var RootDirs = Directory.EnumerateDirectories(Environment.CurrentDirectory);

			var ProjectDirs = "UE4Games.uprojectdirs";

			if (File.Exists(ProjectDirs))
			{
				var ExtraPaths = File.ReadAllLines(ProjectDirs).AsEnumerable();

				ExtraPaths = ExtraPaths.Where(P =>
				{
					string Trimmed = P.Trim();
					Trimmed = Trimmed.Replace('/', Path.DirectorySeparatorChar);
					Trimmed = Trimmed.Replace('\\', Path.DirectorySeparatorChar);
					return Trimmed.StartsWith(";") == false && Trimmed.StartsWith(".") == false;
				});

				RootDirs = RootDirs.Union(ExtraPaths.Select(P => Path.Combine(Environment.CurrentDirectory, P)));
			}

			foreach (var Dir in RootDirs)
			{
				// check both this dir and a subdir with the project name
				string ProjectName = Path.Combine(Dir, InProjectName) + ".uproject";
				if (File.Exists(ProjectName))
				{
					return ProjectName;
				}

				ProjectName = Path.Combine(Dir, InProjectName, InProjectName) + ".uproject";
				if (File.Exists(ProjectName))
				{
					return ProjectName;
				}
			}

			return "";
		}
	}
}