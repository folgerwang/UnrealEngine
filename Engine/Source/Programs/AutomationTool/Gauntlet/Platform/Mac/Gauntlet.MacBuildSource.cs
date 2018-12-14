// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Threading;
using System.Text.RegularExpressions;
using System.Linq;

namespace Gauntlet
{
	public class MacBuildSource : StagedBuildSource<StagedBuild>
	{
		public override string BuildName {  get { return "MacBuild";  } }

		public override UnrealTargetPlatform Platform { get { return UnrealTargetPlatform.Mac; }}

		public override string PlatformFolderPrefix { get { return "Mac"; }}
	}
}
