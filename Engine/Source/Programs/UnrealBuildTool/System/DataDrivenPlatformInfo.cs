// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Tools.DotNETCommon;
using System.IO;

namespace UnrealBuildTool
{
	/// <summary>
	///  Class to manage looking up data driven platform information (loaded from .ini files instead of in code)
	/// </summary>
	public class DataDrivenPlatformInfo
	{
		/// <summary>
		/// All data driven information about a platform
		/// </summary>
		public class ConfigDataDrivenPlatformInfo
		{
			/// <summary>
			/// Is the platform a confidential ("console-style") platform
			/// </summary>
			public bool bIsConfidential;
			/// <summary>
			/// Does the ini need to inherit from another platform's inis?
			/// </summary>
			public string IniParent;

			/// <summary>
			/// Construct an info object from a config file
			/// </summary>
			/// <param name="Config"></param>
			public bool InitFromConfig(ConfigFile Config)
			{
				// we must have the key section 
				ConfigFileSection Section = null;
				if (Config.TryGetSection("DataDrivenPlatformInfo", out Section) == false)
				{
					return false;
				}

				ConfigHierarchySection ParsedSection = new ConfigHierarchySection(new List<ConfigFileSection>() { Section });

				// get string values
				if (ParsedSection.TryGetValue("IniParent", out IniParent) == false)
				{
					IniParent = "";
				}

				// slightly nasty bool parsing for bool values
				string Temp;
				if (ParsedSection.TryGetValue("bIsConfidential", out Temp) == false || ConfigHierarchy.TryParse(Temp, out bIsConfidential) == false)
				{
					bIsConfidential = false;
				}
				
				return true;
			}
		};

		static Dictionary<string, ConfigDataDrivenPlatformInfo> PlatformInfos = null;

		/// <summary>
		/// Return the data driven info for the given platform name 
		/// </summary>
		/// <param name="PlatformName"></param>
		/// <returns></returns>
		public static ConfigDataDrivenPlatformInfo GetDataDrivenInfoForPlatform(string PlatformName)
		{
			// need to init?
			if (PlatformInfos == null)
			{
				PlatformInfos = new Dictionary<string, ConfigDataDrivenPlatformInfo>();

				// look through all config dirs looking for the data driven ini file
				foreach (string FilePath in Directory.EnumerateFiles(Path.Combine(UnrealBuildTool.EngineDirectory.FullName, "Config"), "DataDrivenPlatformInfo.ini", SearchOption.AllDirectories))
				{
					// get the platform name from the path (Foo/Engine/Config/<Platform>/DataDrivenPlatformInfo.ini);
					string IniPlatformName = Path.GetFileName(Path.GetDirectoryName(FilePath));

					// load the DataDrivenPlatformInfo from the path
					ConfigFile Config = new ConfigFile(new FileReference(FilePath));
					ConfigDataDrivenPlatformInfo NewInfo = new ConfigDataDrivenPlatformInfo();

					// create the object using the config, and cache it
					if (NewInfo.InitFromConfig(Config))
					{
						PlatformInfos[IniPlatformName] = NewInfo;
					}
				}
			}

			// lookup the platform name (which is not guaranteed to be there)
			ConfigDataDrivenPlatformInfo Info = null;
			PlatformInfos.TryGetValue(PlatformName, out Info);

			// return what we found of null if nothing
			return Info;
		}
	}
}
