// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Microsoft.Win32;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	class LinuxCommon
	{
		public static string Which(string name)
		{
			Process proc = new Process();
			proc.StartInfo.FileName = "/bin/sh";
			proc.StartInfo.Arguments = String.Format("-c 'which {0}'", name);
			proc.StartInfo.UseShellExecute = false;
			proc.StartInfo.CreateNoWindow = true;
			proc.StartInfo.RedirectStandardOutput = true;
			proc.StartInfo.RedirectStandardError = true;

			proc.Start();
			proc.WaitForExit();

			string path = proc.StandardOutput.ReadLine();
			Log.TraceVerbose(String.Format("which {0} result: ({1}) {2}", name, proc.ExitCode, path));

			if (proc.ExitCode == 0 && String.IsNullOrEmpty(proc.StandardError.ReadToEnd()))
			{
				return path;
			}
			return null;
		}

		public static string WhichClang()
		{
			string[] ClangNames = { "clang++", "clang++-7.0", "clang++-6.0" };
			string ClangPath;
			foreach (string ClangName in ClangNames)
			{
				ClangPath = Which(ClangName);
				if (!String.IsNullOrEmpty(ClangPath))
				{
					return ClangPath;
				}
			}
			return null;
		}

		public static string WhichGcc()
		{
			return Which("g++");
		}
	}
}

