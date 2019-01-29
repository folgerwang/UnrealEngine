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
	/// Implements a default node that should boot most projects and supports n number of clients
	/// via -numclients=<n> and a server via -server
	/// </summary>
	public class DefaultTest : UnrealTestNode<UE4TestConfig>
	{
		public DefaultTest(UnrealTestContext InContext) : base(InContext)
		{
		}

		public override UE4TestConfig GetConfiguration()
		{
			// just need a single client
			UE4TestConfig Config = base.GetConfiguration();

			int ClientCount = Context.TestParams.ParseValue("numclients", 1);
			bool WithServer = Context.TestParams.ParseParam("server");

			if (ClientCount > 0)
			{
				Config.RequireRoles(UnrealTargetRole.Client, ClientCount);
			}

			if (WithServer)
			{
				Config.RequireRoles(UnrealTargetRole.Server, 1);
			}

			return Config;
		}
	}
}
