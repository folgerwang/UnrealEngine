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
		private void ProcessCommandDeployApp(List<ClusterNode> ClusterNodes)
		{
			HashSet<string> NodesSent = new HashSet<string>();

			// Add local IPs so we don't copy on current host
			IPAddress[] localIPs = Dns.GetHostAddresses(Dns.GetHostName());
			foreach (var LocalIP in localIPs)
			{
				NodesSent.Add(LocalIP.ToString());
			}

			foreach (ClusterNode Node in ClusterNodes)
			{
				if (!NodesSent.Contains(Node.address))
				{
					NodesSent.Add(Node.address);
					DeployDir(Path.GetDirectoryName(SelectedApplication), Path.GetDirectoryName(SelectedApplication), Node);
				}
			}
		}

		private string GenerateRemotePath(ClusterNode Node, string TargetPath)
		{
			string ResultPath = Path.GetFullPath(TargetPath);
			ResultPath = ResultPath.Replace(Path.VolumeSeparatorChar, '$');
			ResultPath = string.Format("\\\\{0}\\{1}", Node.address, ResultPath);
			return ResultPath;
		}

		// Copies SrcFile to the DestDir. If Node == null, local copying will be performed.
		private void DeployFile(string SrcFile, string DestDir, ClusterNode Node = null)
		{
			if (!File.Exists(SrcFile))
			{
				AppLogger.Add("File not found: " + SrcFile);
				return;
			}

			string appPath = "robocopy";
			string argList = string.Empty;

			if (Node != null)
			{
				// Build arguments with remote destination path
				argList = string.Format("{0} {1} {2}", Path.GetDirectoryName(SrcFile), GenerateRemotePath(Node, DestDir), Path.GetFileName(SrcFile));
			}
			else
			{
				// Build arguments with local destination path
				argList = string.Format("{0} {1} {2}", Path.GetDirectoryName(SrcFile), DestDir, Path.GetFileName(SrcFile));
			}

			Process proc = new Process();
			proc.StartInfo.FileName = appPath;
			proc.StartInfo.Arguments = argList;
			proc.Start();
		}

		// Copies SrcDir and its content to the DestDir as a new subdirectory on a remote machine. If Node == null, local copying will be performed.
		private void DeployDir(string SrcDir, string DestDir, ClusterNode Node = null)
		{
			if (!Directory.Exists(SrcDir))
			{
				AppLogger.Add("Directory not found: " + SrcDir);
				return;
			}

			string appPath = "robocopy";
			string argList = string.Empty;

			if (Node != null)
			{
				// Build arguments with remote destination path
				argList = string.Format("{0} {1} /e", SrcDir, GenerateRemotePath(Node, DestDir));
			}
			else
			{
				// Build arguments with local destination path
				argList = string.Format("{0} {1} /e", SrcDir, DestDir);
			}

			Process proc = new Process();
			proc.StartInfo.FileName = appPath;
			proc.StartInfo.Arguments = argList;
			proc.Start();
		}
	}
}
