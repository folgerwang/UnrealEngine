// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	abstract class ActionExecutor
	{
		public abstract string Name
		{
			get;
		}

		public abstract bool ExecuteActions(List<Action> ActionsToExecute, bool bLogDetailedActionStats);
	}

}
