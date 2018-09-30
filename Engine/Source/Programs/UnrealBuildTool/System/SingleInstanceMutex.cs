using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// The type of mutex to aquire
	/// </summary>
	enum SingleInstanceMutexType
	{
		/// <summary>
		/// Prevent any instance running in the system
		/// </summary>
		Global,

		/// <summary>
		/// Prevents any instance running in the current branch
		/// </summary>
		PerBranch,
	}

	/// <summary>
	/// System-wide mutex allowing only one instance of the program to run at a time
	/// </summary>
	class SingleInstanceMutex : IDisposable
	{
		/// <summary>
		/// The global mutex instance
		/// </summary>
		Mutex GlobalMutex;

		/// <summary>
		/// Constructor. Attempts to acquire the global mutex
		/// </summary>
		/// <param name="Type">The type of mutex to acquire</param>
		/// <param name="bWaitMutex"></param>
		public SingleInstanceMutex(SingleInstanceMutexType Type, bool bWaitMutex)
		{
			// Get the mutex name
			string MutexName;
			if (Type == SingleInstanceMutexType.Global)
			{
				MutexName = "Global\\UnrealBuildTool_Mutex_AutoSDKS";
			}
			else
			{
				MutexName = "Global\\UnrealBuildTool_Mutex_" + Assembly.GetExecutingAssembly().CodeBase.GetHashCode().ToString();
			}

			// Try to create the mutex, with it initially locked
			bool bCreatedMutex;
			GlobalMutex = new Mutex(true, MutexName, out bCreatedMutex);

			// If we didn't create the mutex, we can wait for it or fail immediately
			if (!bCreatedMutex)
			{
				if (bWaitMutex)
				{
					try
					{
						GlobalMutex.WaitOne();
					}
					catch (AbandonedMutexException)
					{
					}
				}
				else if (Type == SingleInstanceMutexType.Global)
				{
					throw new BuildException("A conflicting instance of UnrealBuildTool is already running.");
				}
				else
				{
					throw new BuildException("A conflicting instance of UnrealBuildTool is already running at {0}.", Assembly.GetExecutingAssembly().CodeBase);
				}
			}
		}

		/// <summary>
		/// Attempt to aquire the single instance mutex, using settings determined from the command line
		/// </summary>
		/// <param name="Type">The type of mutex to aquire</param>
		/// <param name="Arguments">Command line arguments</param>
		/// <returns>The single instance mutex, or null if the -NoMutex argument is specified on the command line</returns>
		public static SingleInstanceMutex Acquire(SingleInstanceMutexType Type, CommandLineArguments Arguments)
		{
			if(Arguments.HasOption("-NoMutex"))
			{
				return null;
			}
			else
			{
				return new SingleInstanceMutex(Type, Arguments.HasOption("-WaitMutex"));
			}
		}

		/// <summary>
		/// Release the mutex and dispose of the object
		/// </summary>
		public void Dispose()
		{
			if (GlobalMutex != null)
			{
				GlobalMutex.ReleaseMutex();
				GlobalMutex.Dispose();
				GlobalMutex = null;
			}
		}
	}
}
