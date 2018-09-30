using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Register all platforms (and in the process, configure all autosdks)
	/// </summary>
	[ToolMode("SetupPlatforms")]
	class SetupPlatforms : ToolMode
	{
		/// <summary>
		/// Execute the tool mode
		/// </summary>
		/// <param name="Arguments">Command line arguments</param>
		/// <returns>Exit code</returns>
		public override int Execute(CommandLineArguments Arguments)
		{
			using(SingleInstanceMutex.Acquire(SingleInstanceMutexType.Global, Arguments))
			{
				// Output a warning if there are any arguments that are still unused
				Arguments.CheckAllArgumentsUsed();

				// Read the XML configuration files
				XmlConfig.ReadConfigFiles(false);

				// Find and register all tool chains, build platforms, etc. that are present
				UnrealBuildTool.RegisterAllUBTClasses(false);
			}
			return 0;
		}
	}
}
