// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using Microsoft.Win32;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


namespace nDisplayLauncher.Settings
{
	public static class RegistrySaver
	{
		const string RegistryPath = "SOFTWARE\\Epic Games\\nDisplay";

		public const string RegAppList = "appList";
		public const string RegConfigList = "configList";
		public const string RegConfigName = "configName";
		public const string RegParamsList = "parameters";
		public const string RegIsStereoName = "isStereo";
		public const string RegIsNoSoundName = "isNoSound";
		public const string RegIsAllCoresName = "isAllCores";
		public const string RegIsFixedSeedName = "isFixedSeed";
		public const string RegIsNoTextureStreamingName = "isNoTextureStreaming";
		public const string RegIsFullscreen = "isFullscreen";
		public const string RegRenderApiName = "renderApi";
		public const string RegRenderModeName = "renderMode";
		public const string RegAdditionalParamsName = "additionalParams";



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
