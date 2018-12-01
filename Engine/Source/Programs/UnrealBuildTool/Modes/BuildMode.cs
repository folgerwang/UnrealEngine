using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Builds a target
	/// </summary>
	[ToolMode("Build")]
	class BuildMode : ToolMode
	{
		/// <summary>
		/// Main entry point
		/// </summary>
		/// <param name="CmdLine">Command-line arguments</param>
		/// <returns>One of the values of ECompilationResult</returns>
		public override int Execute(CommandLineArguments CmdLine)
		{
			return UnrealBuildTool.GuardedMain(CmdLine);
		}
	}
}

