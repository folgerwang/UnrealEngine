// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using Microsoft.Win32;

using System;
using System.Collections.Generic;


namespace nDisplayLauncher.Settings
{
	public static class RegistrySaver
	{
		const string RegistryPath = "SOFTWARE\\Epic Games\\nDisplay";

		// Categories
		public const string RegCategoryAppList              = "AppList";
		public const string RegCategoryConfigList           = "ConfigList";
		public const string RegCategoryParamsList           = "Parameters";
		public const string RegCategoryLogParams            = "LogParams";
		public const string RegCategoryClusterEvents        = "ClusterEvents";

		// Parameters
		public const string RegParamsIsStereoName           = "IsStereo";
		public const string RegParamsIsNoSoundName          = "IsNoSound";
		public const string RegParamsIsAllCoresName         = "IsAllCores";
		public const string RegParamsIsFixedSeedName        = "IsFixedSeed";
		public const string RegParamsIsNoTexStreamingName   = "IsNoTextureStreaming";
		public const string RegParamsIsFullscreen           = "IsFullscreen";
		public const string RegParamsRenderApiName          = "RenderApi";
		public const string RegParamsRenderModeName         = "RenderMode";
		public const string RegParamsAdditionalParamsName   = "AdditionalParams";
		public const string RegParamsAdditionalExecCmds     = "AdditionalExecCmds";

		// Log parameters
		public const string RegLogParamsUseCustomLogs       = "UseCustomLogs";
		public const string RegLogParamsVerbosityPlugin     = "VerbosityPlugin";
		public const string RegLogParamsVerbosityEngine     = "VerbosityEngine";
		public const string RegLogParamsVerbosityConfig     = "VerbosityConfig";
		public const string RegLogParamsVerbosityCluster    = "VerbosityCluster";
		public const string RegLogParamsVerbosityGame       = "VerbosityGame";
		public const string RegLogParamsVerbosityGameMode   = "VerbosityGameMode";
		public const string RegLogParamsVerbosityInput      = "VerbosityInput";
		public const string RegLogParamsVerbosityInputVrpn  = "VerbosityInputVrpn";
		public const string RegLogParamsVerbosityNetwork    = "VerbosityNetwork";
		public const string RegLogParamsVerbosityNetworkMsg = "VerbosityNetworkMsg";
		public const string RegLogParamsVerbosityBlueprint  = "VerbosityBlueprint";
		public const string RegLogParamsVerbosityRender     = "VerbosityRender";



		private static string[] ReadRegistry(string key)
		{
			RegistryKey MainKey = Registry.CurrentUser.OpenSubKey(RegistryPath, true);
			if (MainKey == null)
			{
				Registry.CurrentUser.CreateSubKey(RegistryPath);
			}
			RegistryKey RegKey = MainKey.OpenSubKey(key, true);
			if (RegKey == null)
			{
				RegKey = MainKey.CreateSubKey(key);
			}
			string[] valueNamesArray = null;
			if (RegKey != null)
			{
				valueNamesArray = RegKey.GetValueNames();
			}

			return valueNamesArray;
		}

		public static List<string> ReadStringsFromRegistry(string key)
		{
			string[] valueNamesArray = ReadRegistry(key);

			List<string> regKeys = null;
			if (valueNamesArray != null)
			{
				regKeys = new List<string>(valueNamesArray);
			}

			return regKeys;
		}

		public static string ReadStringFromRegistry(string key)
		{
			string[] valueNamesArray = ReadRegistry(key);

			string regKey = null;
			if (valueNamesArray != null && valueNamesArray.Length > 0)
			{
				regKey = valueNamesArray.GetValue(0) as string;
			}

			return regKey;
		}

		public static void AddRegistryValue(string key, string value)
		{
			UpdateRegistry(key, value, true);
		}

		public static string ReadStringValue(string key, string name)
		{
			return ReadValue(key, name) as string;
		}

		public static bool ReadBoolValue(string key, string name)
		{

			return Convert.ToBoolean(ReadValue(key, name));
		}

		private static object ReadValue(string key, string name)
		{
			try
			{
				RegistryKey regKey = Registry.CurrentUser.OpenSubKey(RegistryPath + "\\" + key, true);
				return regKey.GetValue(name);
			}
			catch (Exception /*exception*/)
			{
				//AppLogger.Add("Can't read value from registry. EXCEPTION: " + exception.Message);
				return null;
			}
		}

		public static void RemoveRegistryValue(string key, string value)
		{
			RegistryKey regKey = Registry.CurrentUser.OpenSubKey(RegistryPath + "\\" + key, true);
			regKey.DeleteValue(value);
		}

		public static string FindSelectedRegValue(string key)
		{
			string valueName = null;
			try
			{
				RegistryKey regKey = Registry.CurrentUser.OpenSubKey(RegistryPath + "\\" + key, true);
				if (regKey != null)
				{
					string[] valueNamesArray = ReadRegistry(key);
					foreach (string item in valueNamesArray)
					{
						if (Convert.ToBoolean(regKey.GetValue(item)))
						{
							return item;
						}
					}

				}
			}
			catch (Exception /*exception*/)
			{
				//AppLogger.Add("Can't find registry value. EXCEPTION: " + exception.Message);
			}
			return valueName;
		}

		public static void RemoveAllRegistryValues(string key)
		{
			RegistryKey regKey = Registry.CurrentUser.OpenSubKey(RegistryPath + "\\" + key, true);
			string[] values = regKey.GetValueNames();
			foreach (string value in values)
			{
				regKey.DeleteValue(value);
			}
		}

		public static void UpdateRegistry(string key, string name, object value)
		{
			try
			{
				RegistryKey regKey = Registry.CurrentUser.OpenSubKey(RegistryPath + "\\" + key, true);
				if (regKey == null)
				{
					Registry.CurrentUser.CreateSubKey(RegistryPath + "\\" + key);
					UpdateRegistry(key, name, value);
				}
				regKey.SetValue(name, value);
			}
			catch (Exception /*exception*/)
			{
				//AppLogger.Add("Can't update registry value. EXCEPTION: " + exception.Message);
			}
		}
	}
}
