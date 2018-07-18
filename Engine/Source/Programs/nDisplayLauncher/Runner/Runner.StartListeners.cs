// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using nDisplayLauncher.Config;
using nDisplayLauncher.Settings;


namespace nDisplayLauncher
{
	public partial class Runner
	{
		private void ProcessCommandStartListeners(List<ClusterNode> ClusterNodes)
		{
			if (String.IsNullOrEmpty(SelectedApplication))
			{
				AppLogger.Add("ERROR! No selected application");
				return;
			}

			// It supposed that listener application is exist alongside the application. Let's check it out.
			string ListenerFilePath = Path.Combine(Path.GetDirectoryName(SelectedApplication), ListenerAppName);
			if (!File.Exists(ListenerFilePath))
			{
				AppLogger.Add(string.Format("Listener application {0} not found", ListenerFilePath));
				return;
			}

			// Ok, we have listener application available on the local PC. It's possible that some remote machine has no listener available.
			// To make sure it's available everywhere we deploy listener application.
			HashSet<string> NodesSent = new HashSet<string>();

			// Add local IPs so we don't copy on current host
			IPAddress[] localIPs = Dns.GetHostAddresses(Dns.GetHostName());
			foreach (var LocalIP in localIPs)
			{
				NodesSent.Add(LocalIP.ToString());
			}

			// Upload listener
			foreach (ClusterNode Node in ClusterNodes)
			{
				if (!NodesSent.Contains(Node.address))
				{
					// Deploy the listener application if it doesn't exist on remote machine
					string RemoteListenerPath = GenerateRemotePath(Node, ListenerFilePath);
					if (!File.Exists(RemoteListenerPath))
					{
						DeployFile(ListenerFilePath, Path.GetDirectoryName(ListenerFilePath), Node);
					}
				}
			}

			// Now we're ready to start listeners remotely
			KeyValuePair<string, string>[] SchTaskCmds = new KeyValuePair<string, string>[]
			{
				new KeyValuePair<string, string>(
					String.Format("SCHTASKS /Create /TN StartClusterListener /TR \\\"{0}\\\" /SC ONEVENT /EC Application /MO *[System/EventID=777] /f", ListenerFilePath),
					"Task registration"),

				new KeyValuePair<string, string>(
					"SCHTASKS /RUN /TN StartClusterListener",
					"Task activation")
			};

			NodesSent.Clear();
			foreach (ClusterNode Node in ClusterNodes)
			{
				if (!NodesSent.Contains(Node.address))
				{
					foreach(var Cmd in SchTaskCmds)
					{ 
						NodesSent.Add(Node.address);

						string appPath = "wmic";
						string argList = string.Format("/node:\"{0}\" process call create \"{1}\"", Node.address, Cmd.Key);

						SpawnRemoteProcess(Cmd.Value, Node.address, appPath, argList);
					}
				}
			}

			// Finally, let's check if all listeners have been started successfully
			NodesSent.Clear();
			foreach (ClusterNode Node in ClusterNodes)
			{
				if (!NodesSent.Contains(Node.address))
				{
					NodesSent.Add(Node.address);

					int ResponseCode = SendDaemonCommand(Node.address, CommandStatus);
					if (ResponseCode != 0)
					{
						AppLogger.Add("Couldn't start the listener on " + Node.address);
					}
				}
			}
		}

		private bool SpawnRemoteProcess(string Name, string Dest, string Path, string Args, bool bQuiet = false)
		{
			bool bPassed = false;

			Process proc = new Process();
			proc.StartInfo.FileName = Path;
			proc.StartInfo.Arguments = Args;
			proc.StartInfo.CreateNoWindow = true;
			proc.StartInfo.UseShellExecute = false;
			proc.StartInfo.RedirectStandardOutput = true;
			proc.Start();
			bool failed = true;
			if (!bQuiet)
			{
				while (!proc.StandardOutput.EndOfStream)
				{
					string line = proc.StandardOutput.ReadLine();
					if (line.IndexOf("ReturnValue = ") >= 0)
					{
						int result = -1;
						string value = line.Split('=')[1].Replace(';', ' ');
						if (int.TryParse(value, out result) && result == 0)
						{
							AppLogger.Add(String.Format("{0} successfull on {1}", Name, Dest));
							failed = false;
							break;
						}
					}
				}
				if (failed)
				{
					AppLogger.Add(String.Format("ERROR! {0} failed on {1}", Name, Dest));
				}
			}

			return bPassed;
		}
	}
}
