using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
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
		/// Constructor
		/// </summary>
		/// <param name="Name">Name of the mode</param>
		public ToolModeAttribute(string Name)
		{
			this.Name = Name;
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
		public abstract int Execute(List<string> Arguments);
	}
}
