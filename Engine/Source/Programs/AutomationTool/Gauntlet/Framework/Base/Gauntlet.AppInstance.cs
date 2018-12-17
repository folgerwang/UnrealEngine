// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;


namespace Gauntlet
{
	/// <summary>
	/// Interface that represents an instance of an app running on a device
	/// </summary>
	public interface IAppInstance
 	{
		/// <summary>
		/// Returns true/false if the process has exited for any reason
		/// </summary>
		bool HasExited { get; }

		/// <summary>
		/// Current StdOut of the process
		/// </summary>
		string StdOut { get; }

		/// <summary>
		/// Exit code of the process.
		/// </summary>
		int ExitCode { get; }

		/// <summary>
		/// Returns true if the process exited due to Kill() being called
		/// </summary>
		bool WasKilled { get; }

		/// <summary>
		/// Path to commandline used to start the process
		/// </summary>
		string CommandLine { get; }

		/// <summary>
		/// Path to artifacts from the process
		/// </summary>
		string ArtifactPath { get; }

		/// <summary>
		/// Device that the app was run on
		/// </summary>
		ITargetDevice Device { get; }

		/// <summary>
		/// Kills the process if its running (no need to call WaitForExit)
		/// </summary>
		void Kill();

		/// <summary>
		/// Waits for the process to exit normally
		/// </summary>
		/// <returns></returns>
		int WaitForExit();

	}
}
