// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Exports a target as a JSON file
	/// </summary>
	[ToolMode("JsonExport", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms | ToolModeOptions.SingleInstance)]
	class JsonExportMode : ToolMode
	{
		/// <summary>
		/// Execute this command
		/// </summary>
		/// <param name="Arguments">Command line arguments</param>
		/// <returns>Exit code (always zero)</returns>
		public override int Execute(CommandLineArguments Arguments)
		{
			List<TargetDescriptor> TargetDescriptors = TargetDescriptor.ParseCommandLine(Arguments, false, false);
			foreach(TargetDescriptor TargetDescriptor in TargetDescriptors)
			{
				// Create the target
				UEBuildTarget Target = UEBuildTarget.Create(TargetDescriptor, false, false);

				// Get the output file
				FileReference OutputFile = TargetDescriptor.AdditionalArguments.GetFileReferenceOrDefault("-OutputFile=", null);
				if(OutputFile == null)
				{
					OutputFile = Target.ReceiptFileName.ChangeExtension(".json");
				}

				// Write the output file
				Log.TraceInformation("Writing {0}...", OutputFile);
				Target.ExportJson(OutputFile);
			}
			return 0;
		}
	}
}
