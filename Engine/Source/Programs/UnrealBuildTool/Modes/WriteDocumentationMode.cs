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
	/// Generates documentation from reflection data
	/// </summary>
	[ToolMode("WriteDocumentation", ToolModeOptions.None)]
	class WriteDocumentationMode : ToolMode
	{
		/// <summary>
		/// Enum for the type of documentation to generate
		/// </summary>
		enum DocumentationType
		{
			BuildConfiguration,
			ModuleRules,
			TargetRules,
		}

		/// <summary>
		/// Type of documentation to generate
		/// </summary>
		[CommandLine(Required = true)]
		DocumentationType Type = DocumentationType.BuildConfiguration;

		/// <summary>
		/// The HTML file to write to
		/// </summary>
		[CommandLine(Required = true)]
		FileReference OutputFile = null;

		/// <summary>
		/// Entry point for this command
		/// </summary>
		/// <returns></returns>
		public override int Execute(CommandLineArguments Arguments)
		{
			Arguments.ApplyTo(this);
			Arguments.CheckAllArgumentsUsed();

			switch(Type)
			{
				case DocumentationType.BuildConfiguration:
					XmlConfig.WriteDocumentation(OutputFile);
					break;
				case DocumentationType.ModuleRules:
					RulesDocumentation.WriteDocumentation(typeof(ModuleRules), OutputFile);
					break;
				case DocumentationType.TargetRules:
					RulesDocumentation.WriteDocumentation(typeof(TargetRules), OutputFile);
					break;
				default:
					throw new BuildException("Invalid documentation type: {0}", Type);
			}
			return 0;
		}
	}
}
