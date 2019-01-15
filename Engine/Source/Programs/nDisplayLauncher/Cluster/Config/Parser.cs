// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

using nDisplayLauncher.Cluster.Config.Entity;
using nDisplayLauncher.Log;


namespace nDisplayLauncher.Cluster.Config
{
	public static class Parser
	{
		// Returns version of specified config file
		public static ConfigurationVersion GetVersion(string filePath)
		{
			try
			{
				foreach (string DirtyLine in File.ReadLines(filePath))
				{
					string Line = PreprocessConfigLine(DirtyLine);

					if (Line.ToLower().StartsWith("[info]"))
					{
						EntityInfo Info = new EntityInfo(Line);
						return Info.Version;
					}
				}
			}
			catch (Exception ex)
			{
				AppLogger.Log("ERROR! " + ex.Message);
			}

			return ConfigurationVersion.Ver21;
		}

		// Config file parser
		public static Configuration Parse(string filePath)
		{
			Configuration ParsedConfig = new Configuration();

			try
			{
				foreach (string DirtyLine in File.ReadLines(filePath))
				{
					string Line = PreprocessConfigLine(DirtyLine);

					if (string.IsNullOrEmpty(Line) || Line.First() == '#')
					{
						//Do nothing
					}
					else
					{
						if (Line.ToLower().StartsWith("[cluster_node]"))
						{
							EntityClusterNode ClusterNode = new EntityClusterNode(Line);
							if (!string.IsNullOrEmpty(ClusterNode.Id))
							{
								ParsedConfig.ClusterNodes.Add(ClusterNode.Id, ClusterNode);
							}
						}
						if (Line.ToLower().StartsWith("[window]"))
						{
							EntityWindow Window = new EntityWindow(Line);
							if (!string.IsNullOrEmpty(Window.Id))
							{
								ParsedConfig.Windows.Add(Window.Id, Window);
							}
						}
					}
				}
			}
			catch (Exception ex)
			{
				AppLogger.Log("ERROR! " + ex.Message);
				return null;
			}

			return ParsedConfig;
		}

		public static string PreprocessConfigLine(string text)
		{
			string CleanString = text;
			int len = 0;

			// Remove all spaces before '='
			do
			{
				len = CleanString.Length;
				CleanString = text.Replace(" =", "=");
			}
			while (len != CleanString.Length);

			// Remove all spaces after '='
			do
			{
				len = CleanString.Length;
				CleanString = text.Replace("= ", "=");
			}
			while (len != CleanString.Length);

			// Remove all spaces before ','
			do
			{
				len = CleanString.Length;
				CleanString = text.Replace(" ,", ",");
			}
			while (len != CleanString.Length);

			// Remove all spaces after ','
			do
			{
				len = CleanString.Length;
				CleanString = text.Replace(", ", ",");
			}
			while (len != CleanString.Length);

			// Convert everything to lower case
			CleanString = CleanString.ToLower();

			return CleanString;
		}

		public static string GetStringValue(string ArgName, string text)
		{
			// Break the string to separate key-value substrings
			string[] ArgValPairs = text.Split(' ');

			// Parse each key-value substring to find the requested argument (key)
			foreach (string Pair in ArgValPairs)
			{
				string[] KeyValue = Pair.Trim().Split('=');
				if (KeyValue[0].CompareTo(ArgName) == 0)
				{
					return KeyValue[1];
				}
			}

			return string.Empty;
		}

		public static bool GetBoolValue(string ArgName, string text)
		{
			string Value = GetStringValue(ArgName, text);

			if (Value.CompareTo("true") == 0 || Value.CompareTo("1") == 0)
			{
				return true;
			}
			else if (Value.CompareTo("false") == 0 || Value.CompareTo("0") == 0)
			{
				return false;
			}
			else
			{
				// some logs
				return false;
			}
		}

		public static int GetIntValue(string ArgName, string text)
		{
			string StringValue = GetStringValue(ArgName, text);
			int IntValue = 0;

			try
			{
				IntValue = int.Parse(StringValue);
			}
			catch (Exception)
			{
				// some logs
			}

			return IntValue;
		}

		public static List<string> GetStringArrayValue(string ArgName, string text)
		{
			string[] Values = null;
			string StringValue = GetStringValue(ArgName, text);

			try
			{
				// Remove leading quotes
				if (StringValue.StartsWith("\""))
				{
					StringValue = StringValue.Substring(1);
				}

				// Remove trailing quotes
				if (StringValue.EndsWith("\""))
				{
					StringValue = StringValue.Substring(0, StringValue.Length - 1);
				}

				// Split value parameters
				Values = StringValue.Split(',');
			}
			catch (Exception)
			{
				// some logs
			}

			return Values.ToList();
		}

		public static string RemoveArgument(string ArgName, string text)
		{
			string Result = text.ToLower().Trim();
			int Idx1 = Result.IndexOf(ArgName.ToLower().Trim());
			if (Idx1 >= 0)
			{
				int Idx2 = Result.IndexOf(' ', Idx1 + 1);
				if (Idx2 > Idx1)
				{
					Result = Result.Remove(Idx1, Idx2 - Idx1 + 1);
				}
				else
				{
					Result = Result.Remove(Idx1);
				}
			}

			return Result;
		}
	}
}
