// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;

using nDisplayLauncher.Log;


namespace nDisplayLauncher.Cluster.Config.Entity
{
	public class EntityInfo : EntityBase
	{
		public ConfigurationVersion Version { get; set; } = ConfigurationVersion.Ver22;

		public EntityInfo()
		{
		}

		public EntityInfo(string text)
		{
			try
			{
				InitializeFromText(text);
			}
			catch (Exception ex)
			{
				AppLogger.Log(ex.Message);
			}
		}

		public override void InitializeFromText(string text)
		{
			string StrVersion = Parser.GetStringValue("version", text);
			Version = ConfigurationVersionHelpers.FromString(StrVersion);
		}
	}
}
