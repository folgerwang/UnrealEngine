// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Gauntlet;

namespace UE4Game
{
	/// <summary>
	/// Default set of options for testing "DefaultGame". Options that tests can configure
	/// should be public, external command-line driven options should be protected/private
	/// </summary>
	public class UE4TestConfig : UnrealTestConfiguration
	{
		/// <summary>
		/// Map to use
		/// </summary>
		[AutoParam]
		public string Map = "";

		/// <summary>
		/// Applies these options to the provided app config
		/// </summary>
		/// <param name="AppConfig"></param>
		public override void ApplyToConfig(UnrealAppConfig AppConfig)
		{
			base.ApplyToConfig(AppConfig);

			if (string.IsNullOrEmpty(Map) == false)
			{
				// If map is specified, pass it to the server, or just the client if there is no server
				if (AppConfig.ProcessType.IsServer()
				|| (AppConfig.ProcessType.IsClient() && RoleCount(UnrealTargetRole.Server) == 0))
				{
					// must be the first argument!
					AppConfig.CommandLine = Map + " " + AppConfig.CommandLine;
				}
			}			
		}
	}
}
