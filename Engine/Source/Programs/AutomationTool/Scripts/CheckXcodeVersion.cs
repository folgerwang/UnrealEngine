// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace AutomationTool
{
	[Help("Checks that the installed Xcode version is the version specified.")]
	[Help("-Version", "The expected version number")]
	class CheckXcodeVersion : BuildCommand
	{
		public override void ExecuteBuild()
		{
			string Version = ParseParamValue("Version");
			if(Version == null)
			{
				throw new AutomationException("Missing -Version=... parameter");
			}

			IProcessResult Result = Run("xcodebuild", "-version");
			if(Result.ExitCode != 0)
			{
				throw new AutomationException("Unable to query version number from xcodebuild (exit code={0})", Result.ExitCode);
			}

			Match Match = Regex.Match(Result.Output, "^Xcode ([0-9.]+)", RegexOptions.Multiline);
			if(!Match.Success)
			{
				throw new AutomationException("Missing version number from xcodebuild output:\n{0}", Result.Output);
			}

			if(Match.Groups[1].Value != Version)
			{
				LogWarning("Installed Xcode version is {0} - expected {1}", Match.Groups[1].Value, Version);
			}
		}
	}
}
