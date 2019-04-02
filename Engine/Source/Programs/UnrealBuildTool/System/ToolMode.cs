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
	/// Systems that need to be configured to execute a tool mode
	/// </summary>
	[Flags]
	enum ToolModeOptions
	{
		/// <summary>
		/// Do not initialize anything
		/// </summary>
		None = 0,

		/// <summary>
		/// Start prefetching metadata for the engine folder as early as possible
		/// </summary>
		StartPrefetchingEngine = 1,

		/// <summary>
		/// Initializes the XmlConfig system
		/// </summary>
		XmlConfig = 2,

		/// <summary>
		/// Registers build platforms
		/// </summary>
		BuildPlatforms = 4,

		/// <summary>
		/// Registers build platforms for validation
		/// </summary>
		BuildPlatformsForValidation = 8,

		/// <summary>
		/// Only allow a single instance running in the branch at once
		/// </summary>
		SingleInstance = 16,

		/// <summary>
		/// Print out the total time taken to execute
		/// </summary>
		ShowExecutionTime = 32,
	}

	/// <summary>
	/// Attribute used to specify options for a UBT mode.
	/// </summary>
	class ToolModeAttribute : Attribute
	{
		/// <summary>
		/// Name of this mode
		/// </summary>
		public string Name;

		/// <summary>
		/// Options for executing this mode
		/// </summary>
		public ToolModeOptions Options;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Name">Name of the mode</param>
		/// <param name="Options">Options for this mode</param>
		public ToolModeAttribute(string Name, ToolModeOptions Options)
		{
			this.Name = Name;
			this.Options = Options;
		}
	}

	/// <summary>
	/// Base class for standalone UBT modes. Different modes can be invoked using the -Mode=[Name] argument on the command line, where [Name] is determined by 
	/// the ToolModeAttribute on a ToolMode derived class. The log system will be initialized before calling the mode, but little else.
	/// </summary>
	abstract class ToolMode
	{
		/// <summary>
		/// Entry point for this command.
		/// </summary>
		/// <param name="Arguments">List of command line arguments</param>
		/// <returns>Exit code for the process</returns>
		public abstract int Execute(CommandLineArguments Arguments);
	}
}
