// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using AutomationTool.DeviceReservation;
using System.Text.RegularExpressions;
using System.Linq;
using Newtonsoft.Json;
using UnrealBuildTool;

namespace Gauntlet
{

	/// <summary>
	/// Information that defines a device
	/// </summary>
	public class BlacklistEntry
	{
		public string TestName;

		public UnrealTargetPlatform[] Platforms;

		public string BranchName;
		public BlacklistEntry()
		{
			TestName = "None";
			Platforms = new UnrealTargetPlatform [] { UnrealTargetPlatform.Unknown };
			BranchName = "None";
		}

		public override string ToString()
		{
			return string.Format("{0} Platforms={1} Branch={2}", TestName, string.Join(",",Platforms), BranchName);
		}
	}


	public class Blacklist
	{
		/// <summary>
		/// Static instance
		/// </summary>
		private static Blacklist _Instance;

		IEnumerable<BlacklistEntry> BlacklistEntries;

		/// <summary>
		/// Protected constructor - code should use DevicePool.Instance
		/// </summary>
		protected Blacklist()
		{
			if (_Instance == null)
			{
				_Instance = this;
			}

			BlacklistEntries = new BlacklistEntry[0] { };

			// temp, pass in
			LoadBlacklist(@"P:\Builds\Fortnite\Automation\Config\blacklist.json");
		}

		/// <summary>
		/// Access to our singleton
		/// </summary>
		public static Blacklist Instance
		{
			get
			{
				if (_Instance == null)
				{
					new Blacklist();
				}
				return _Instance;
			}
		}

		protected void LoadBlacklist(string InFilePath)
		{
			if (File.Exists(InFilePath))
			{
				try
				{ 
					Gauntlet.Log.Info("Loading blacklist from {0}", InFilePath);
					List<BlacklistEntry> NewEntries = JsonConvert.DeserializeObject<List<BlacklistEntry>>(File.ReadAllText(InFilePath));

					if (NewEntries != null)
					{
						BlacklistEntries = NewEntries;
					}

					// cannonical branch format is ++
					BlacklistEntries = BlacklistEntries.Select(E =>
					{
						E.BranchName = E.BranchName.Replace("/", "+");
						return E;
					});
				}
				catch (Exception Ex)
				{
				Log.Warning("Failed to load blacklist file {0}. {1}", InFilePath, Ex.Message);
				}
				
			}
		}
		public bool IsTestBlacklisted(string InNodeName, UnrealTargetPlatform InPlatform, string InBranchName)
		{
			// find any references to this test irrespective of platform & branch
			IEnumerable<BlacklistEntry> Entries = BlacklistEntries.Where(E => E.TestName == InNodeName);

			string NormalizedBranchName = InBranchName.Replace("/", "+");

			// Filter by branch
			Entries = Entries.Where(E => E.BranchName == "*" || string.Equals(E.BranchName, NormalizedBranchName, StringComparison.OrdinalIgnoreCase));

			// Filter by branch
			Entries = Entries.Where(E => E.Platforms.Contains(UnrealTargetPlatform.Unknown) || E.Platforms.Contains(InPlatform));

			return Entries.Count() > 0;
		}
	}
}
