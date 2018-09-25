// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UE4GameTest;
using EpicGame;
using Gauntlet;
using System;
using System.Text.RegularExpressions;

namespace EngineTest
{
	/// <summary>
	/// Runs automated tests on a platform
	/// </summary>
	public class BootTest : UE4GameTestNode
	{
		public BootTest(Gauntlet.UnrealTestContext InContext)
			: base(InContext)
		{
		}

		public override UE4GameTestConfig GetConfiguration()
		{
			UE4GameTestConfig Config = base.GetConfiguration();

			Config.RequireRole(UnrealRoleType.Client);

			if (Context.TestParams.ParseParam("server"))
			{
				Config.RequireRole(UnrealRoleType.Server);
			}

			return Config;
		}
	}
}
