// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;
using System.Threading;
using System.Reflection;
using AutomationTool;
using UnrealBuildTool;
using Tools.DotNETCommon;
using System.Linq;

public class ExtractPaks : BuildCommand
{
	public override void ExecuteBuild()
	{
		var bLayered = ParseParam("layered");
		var SourceDirectory = ParseParamValue("sourcedirectory", null);
		var TargetDirectory = ParseParamValue("targetdirectory", null);
		var CryptoKeysFile = ParseParamValue("cryptokeysjson", null);
		var Compressor = ParseParamValue("customcompressor", null);

		string ExtraArgs = "";

		if (!string.IsNullOrEmpty(Compressor))
		{
			ExtraArgs += "-customcompressor=" + Compressor;
		}

		if (Params.Contains("passargs"))
		{
			IEnumerable<string> ExtraArgArray = Params.Where((arg, index) => index > Array.IndexOf(Params, "passargs"));

			if (ExtraArgArray.Count() > 0)
			{
				ExtraArgs += " -" + string.Join(" -", ExtraArgArray);
			}
		}

		if (String.IsNullOrEmpty(SourceDirectory))
		{
			throw new AutomationException("SourceDirectory is a required parameter");
		}
		if (String.IsNullOrEmpty(TargetDirectory))
		{
			throw new AutomationException("TargetDirectory is a required parameter");
		}
		if (String.IsNullOrEmpty(CryptoKeysFile))
		{
			throw new AutomationException("CryptoKeysJson is a required parameter");
		}

		DirectoryInfo SourceDirectoryInfo = new DirectoryInfo(SourceDirectory);

		LogInformation("Extracting paks from {0} to {1}", SourceDirectory, TargetDirectory);

		PackageUtils.ExtractPakFiles(SourceDirectoryInfo, TargetDirectory, CryptoKeysFile, ExtraArgs, bLayered);
	}
}
