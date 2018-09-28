// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using Gauntlet;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace EditorTest
{
	class BootTest : UE4Game.DefaultNode
	{
		public BootTest(Gauntlet.UnrealTestContext InContext) 
			: base(InContext)
		{
		}

		public override UE4Game.UE4TestConfig GetConfiguration()
		{
			UE4Game.UE4TestConfig Config = base.GetConfiguration();

			Config.RequireRole(UnrealTargetRole.Editor);

			return Config;
		}
	}
}
