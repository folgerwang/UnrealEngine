// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Reflection;
using System.IO;
using System.Diagnostics;

namespace DeploymentServerLauncher
{
	class Launcher
	{
		static int Main()
		{

			string CommandLine;
			string UATExecutable;
			string WorkingDirectory = Path.GetDirectoryName(AppDomain.CurrentDomain.BaseDirectory);
			var ExitCode = 193;
			if (Environment.OSVersion.Platform == PlatformID.MacOSX || Environment.OSVersion.Platform == PlatformID.Unix)
			{
				Process NewProcess = new Process();
				

				NewProcess.StartInfo.WorkingDirectory = WorkingDirectory;
				NewProcess.StartInfo.WorkingDirectory.TrimEnd('/');
				NewProcess.StartInfo.FileName = NewProcess.StartInfo.WorkingDirectory + "/../../../Build/BatchFiles/Mac/RunMono.sh";
				NewProcess.StartInfo.Arguments = "\"" + NewProcess.StartInfo.WorkingDirectory + "/DeploymentServer.exe\" server " + Process.GetCurrentProcess().Id.ToString() + " \"" + NewProcess.StartInfo.WorkingDirectory + "\"";

				NewProcess.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;
				NewProcess.StartInfo.UseShellExecute = true;

				try
				{
					NewProcess.Start();
				}
				catch (System.Exception ex)
				{
					Console.WriteLine("Failed to create deployment server process ({0})", ex.Message);
				}
			}
			else
			{
				var Domaininfo = new AppDomainSetup();
				Domaininfo.ApplicationBase = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
				Domaininfo.ConfigurationFile = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location) + "\\DeploymentServer.exe.config";
				WorkingDirectory.TrimEnd('\\');
				UATExecutable = WorkingDirectory + "\\DeploymentServer.exe";
				CommandLine = "server " + Process.GetCurrentProcess().Id.ToString() + " \"" + WorkingDirectory + "\"";
				Domaininfo.ShadowCopyFiles = "true";

				// Create application domain setup information.
				// Create the application domain.			
				var Domain = AppDomain.CreateDomain("DeploymentServer", AppDomain.CurrentDomain.Evidence, Domaininfo);
				// Default exit code in case UAT does not even start, otherwise we always return UAT's exit code.
				

				try
				{
					string[] Commands = CommandLine.Split(' ');
					Console.WriteLine("Executing assembly");
					ExitCode = Domain.ExecuteAssembly(UATExecutable, Commands);
					Console.WriteLine("Assembly exited:" + ExitCode.ToString());
					// Unload the application domain.
					AppDomain.Unload(Domain);
				}
				catch (Exception Ex)
				{
					Console.WriteLine(Ex.Message);
					Console.WriteLine(Ex.StackTrace);
				}
			}
			Console.WriteLine("Deployment launcher exited.");

			return ExitCode;
		}
	}
}
