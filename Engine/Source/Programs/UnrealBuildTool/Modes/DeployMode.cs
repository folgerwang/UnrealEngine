using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Invokes the deployment handler for a target.
	/// </summary>
	[ToolMode("Deploy")]
	class DeployMode : ToolMode
	{
		/// <summary>
		/// If we are just running the deployment step, specifies the path to the given deployment settings
		/// </summary>
		[CommandLine("-TargetInfo", Required=true)]
		public FileReference TargetInfoFile = null;

		/// <summary>
		/// Execute the tool mode
		/// </summary>
		/// <param name="Arguments">Command line arguments</param>
		/// <returns>Exit code</returns>
		public override int Execute(CommandLineArguments Arguments)
		{
			// Apply the arguments
			Arguments.ApplyTo(this);
			Arguments.CheckAllArgumentsUsed();

			// Change the working directory to be the Engine/Source folder. We are likely running from Engine/Binaries/DotNET
			// This is critical to be done early so any code that relies on the current directory being Engine/Source will work.
			DirectoryReference.SetCurrentDirectory(UnrealBuildTool.EngineSourceDirectory);

			// Find and register all tool chains, build platforms, etc. that are present
			UnrealBuildTool.RegisterAllUBTClasses(false);

			// Execute the deploy
			UEBuildDeployTarget DeployTarget = new UEBuildDeployTarget(TargetInfoFile);
			Log.WriteLine(LogEventType.Console, "Deploying {0} {1} {2}...", DeployTarget.TargetName, DeployTarget.Platform, DeployTarget.Configuration);
			UEBuildPlatform.GetBuildPlatform(DeployTarget.Platform).Deploy(DeployTarget);

			return (int)ECompilationResult.Succeeded;
		}
	}
}
