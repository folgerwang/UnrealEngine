// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Runtime.InteropServices;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// The type of shell supported by this platform. Used to configure command line arguments.
	/// </summary>
	public enum ShellType
	{
		/// <summary>
		/// The Bourne shell
		/// </summary>
		Sh,

		/// <summary>
		/// Windows command interpreter
		/// </summary>
		Cmd,
	}

	/// <summary>
	/// Host platform abstraction
	/// </summary>
	public abstract class BuildHostPlatform
	{
		private static BuildHostPlatform CurrentPlatform;
		private static bool bIsMac = File.Exists("/System/Library/CoreServices/SystemVersion.plist");

		/// <summary>
		/// Returns the name of platform UBT is running on. Internal use only. If you need access this this enum, use BuildHostPlatform.Current.Platform */
		/// </summary>
		private static UnrealTargetPlatform GetRuntimePlatform()
		{
			PlatformID Platform = Environment.OSVersion.Platform;
			switch (Platform)
			{
				case PlatformID.Win32NT:
					return UnrealTargetPlatform.Win64;
				case PlatformID.Unix:
					return bIsMac ? UnrealTargetPlatform.Mac : UnrealTargetPlatform.Linux;
				case PlatformID.MacOSX:
					return UnrealTargetPlatform.Mac;
				default:
					throw new BuildException("Unhandled runtime platform " + Platform);
			}
		}

		/// <summary>
		/// Host platform singleton.
		/// </summary>
		static public BuildHostPlatform Current
		{
			get
			{
				if (CurrentPlatform == null)
				{
					switch (GetRuntimePlatform())
					{
						case UnrealTargetPlatform.Win64:
							CurrentPlatform = new WindowsBuildHostPlatform();
							break;
						case UnrealTargetPlatform.Mac:
							CurrentPlatform = new MacBuildHostPlatform();
							break;
						case UnrealTargetPlatform.Linux:
							CurrentPlatform = new LinuxBuildHostPlatform();
							break;
					}
				}
				return CurrentPlatform;
			}
		}

		/// <summary>
		/// Gets the current host platform type.
		/// </summary>
		abstract public UnrealTargetPlatform Platform { get; }

		/// <summary>
		/// Gets the path to the shell for this platform
		/// </summary>
		abstract public FileReference Shell { get; }

		/// <summary>
		/// The type of shell returned by the Shell parameter
		/// </summary>
		abstract public ShellType ShellType { get; }

		/// <summary>
		/// Class that holds information about a running process
		/// </summary>
		public class ProcessInfo
		{
			/// <summary>
			/// Process ID
			/// </summary>
			public int PID;

			/// <summary>
			/// Name of the process
			/// </summary>
			public string Name;

			/// <summary>
			/// Filename of the process binary
			/// </summary>
			public string Filename;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="InPID">The process ID</param>
			/// <param name="InName">The process name</param>
			/// <param name="InFilename">The process filename</param>
			public ProcessInfo(int InPID, string InName, string InFilename)
			{
				PID = InPID;
				Name = InName;
				Filename = InFilename;
			}

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="Proc">Process to take information from</param>
			public ProcessInfo(Process Proc)
			{
				PID = Proc.Id;
				Name = Proc.ProcessName;
				Filename = Path.GetFullPath(Proc.MainModule.FileName);
			}

			/// <summary>
			/// Format as a string for debugging
			/// </summary>
			/// <returns>String containing process info</returns>
			public override string ToString()
			{
				return String.Format("{0}, {1}", Name, Filename);
			}
		}

		/// <summary>
		/// Gets all currently running processes.
		/// </summary>
		/// <returns></returns>
		public virtual ProcessInfo[] GetProcesses()
		{
			Process[] AllProcesses = Process.GetProcesses();
			List<ProcessInfo> Result = new List<ProcessInfo>(AllProcesses.Length);
			foreach (Process Proc in AllProcesses)
			{
				try
				{
					if (!Proc.HasExited)
					{
						Result.Add(new ProcessInfo(Proc));
					}
				}
				catch { }
			}
			return Result.ToArray();
		}

		/// <summary>
		/// Gets a process by name.
		/// </summary>
		/// <param name="Name">Name of the process to get information for.</param>
		/// <returns></returns>
		public virtual ProcessInfo GetProcessByName(string Name)
		{
			ProcessInfo[] AllProcess = GetProcesses();
			foreach (ProcessInfo Info in AllProcess)
			{
				if (Info.Name == Name)
				{
					return Info;
				}
			}
			return null;
		}

		/// <summary>
		/// Gets processes by name.
		/// </summary>
		/// <param name="Name">Name of the process to get information for.</param>
		/// <returns></returns>
		public virtual ProcessInfo[] GetProcessesByName(string Name)
		{
			ProcessInfo[] AllProcess = GetProcesses();
			List<ProcessInfo> Result = new List<ProcessInfo>();
			foreach (ProcessInfo Info in AllProcess)
			{
				if (Info.Name == Name)
				{
					Result.Add(Info);
				}
			}
			return Result.ToArray();
		}

		/// <summary>
		/// Gets the filenames of all modules associated with a process
		/// </summary>
		/// <param name="PID">Process ID</param>
		/// <param name="Filename">Filename of the binary associated with the process.</param>
		/// <returns>An array of all module filenames associated with the process. Can be empty of the process is no longer running.</returns>
		public virtual string[] GetProcessModules(int PID, string Filename)
		{
			List<string> Modules = new List<string>();
			try
			{
				Process Proc = Process.GetProcessById(PID);
				if (Proc != null)
				{
					foreach (ProcessModule Module in Proc.Modules.Cast<System.Diagnostics.ProcessModule>())
					{
						Modules.Add(Path.GetFullPath(Module.FileName));
					}
				}
			}
			catch { }
			return Modules.ToArray();
		}

		/// <summary>
		/// Determines the default project file formats for this platform
		/// </summary>
		/// <returns>Sequence of project file formats</returns>
		internal abstract IEnumerable<ProjectFileFormat> GetDefaultProjectFileFormats();
	}

	class WindowsBuildHostPlatform : BuildHostPlatform
	{
		public override UnrealTargetPlatform Platform
		{
			get { return UnrealTargetPlatform.Win64; }
		}

		public override FileReference Shell
		{
			get { return new FileReference(Environment.GetEnvironmentVariable("COMSPEC")); }
		}

		public override ShellType ShellType
		{
			get { return ShellType.Cmd; }
		}
			
		internal override IEnumerable<ProjectFileFormat> GetDefaultProjectFileFormats()
		{
			yield return ProjectFileFormat.VisualStudio;
		}
	}

	class MacBuildHostPlatform : BuildHostPlatform
	{
		public override UnrealTargetPlatform Platform
		{
			get { return UnrealTargetPlatform.Mac; }
		}

		public override FileReference Shell
		{
			get { return new FileReference("/bin/sh"); }
		}

		public override ShellType ShellType
		{
			get { return ShellType.Sh; }
		}

		/// <summary>
		/// Currently Mono returns incomplete process names in Process.GetProcesses() so we need to parse 'ps' output.
		/// </summary>
		/// <returns></returns>
		public override ProcessInfo[] GetProcesses()
		{
			List<ProcessInfo> Result = new List<ProcessInfo>();

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = "ps";
			StartInfo.Arguments = "-eaw -o pid,comm";
			StartInfo.CreateNoWindow = true;
			StartInfo.UseShellExecute = false;
			StartInfo.RedirectStandardOutput = true;

			Process Proc = new Process();
			Proc.StartInfo = StartInfo;
			try
			{
				Proc.Start();
				Proc.WaitForExit();
				for (string Line = Proc.StandardOutput.ReadLine(); Line != null; Line = Proc.StandardOutput.ReadLine())
				{
					Line = Line.Trim();
					int PIDEnd = Line.IndexOf(' ');
					string PIDString = Line.Substring(0, PIDEnd);
					if (PIDString != "PID")
					{
						string Filename = Line.Substring(PIDEnd + 1);
						int Pid = Int32.Parse(PIDString);
						try
						{
							Process ExistingProc = Process.GetProcessById(Pid);
							if (ExistingProc != null && Pid != Process.GetCurrentProcess().Id && ExistingProc.HasExited == false)
							{
								ProcessInfo ProcInfo = new ProcessInfo(ExistingProc.Id, Path.GetFileName(Filename), Filename);
								Result.Add(ProcInfo);
							}
						}
						catch { }
					}
				}

			}
			catch { }
			return Result.ToArray();
		}

		/// <summary>
		/// Currently Mono returns incomplete list of modules for Process.Modules so we need to parse vmmap output.
		/// </summary>
		/// <param name="PID"></param>
		/// <param name="Filename"></param>
		/// <returns></returns>
		public override string[] GetProcessModules(int PID, string Filename)
		{
			HashSet<string> Modules = new HashSet<string>();
			// Add the process file name to the module list. This is to make it compatible with the results of Process.Modules on Windows.
			Modules.Add(Filename);

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = "vmmap";
			StartInfo.Arguments = String.Format("{0} -w", PID);
			StartInfo.CreateNoWindow = true;
			StartInfo.UseShellExecute = false;
			StartInfo.RedirectStandardOutput = true;

			Process Proc = new Process();
			Proc.StartInfo = StartInfo;
			try
			{
				Proc.Start();
				// Start processing output before vmmap exits otherwise it's going to hang
				while (!Proc.WaitForExit(1))
				{
					ProcessVMMapOutput(Proc, Modules);
				}
				ProcessVMMapOutput(Proc, Modules);
			}
			catch { }
			return Modules.ToArray();
		}
		private void ProcessVMMapOutput(Process Proc, HashSet<string> Modules)
		{
			for (string Line = Proc.StandardOutput.ReadLine(); Line != null; Line = Proc.StandardOutput.ReadLine())
			{
				Line = Line.Trim();
				if (Line.EndsWith(".dylib"))
				{
					const int SharingModeLength = 6;
					int SMStart = Line.IndexOf("SM=");
					int PathStart = SMStart + SharingModeLength;
					string Module = Line.Substring(PathStart).Trim();
					if (!Modules.Contains(Module))
					{
						Modules.Add(Module);
					}
				}
			}
		}

		internal override IEnumerable<ProjectFileFormat> GetDefaultProjectFileFormats()
		{
			yield return ProjectFileFormat.XCode;
		}
	}

	class LinuxBuildHostPlatform : BuildHostPlatform
	{
		public override UnrealTargetPlatform Platform
		{
			get { return UnrealTargetPlatform.Linux; }
		}

		public override FileReference Shell
		{
			get { return new FileReference("/bin/sh"); }
		}

		public override ShellType ShellType
		{
			get { return ShellType.Sh; }
		}

		/// <summary>
		/// Currently Mono returns incomplete process names in Process.GetProcesses() so we need to use /proc
		/// (also, Mono locks up during process traversal sometimes, trying to open /dev/snd/pcm*)
		/// </summary>
		/// <returns></returns>
		public override ProcessInfo[] GetProcesses()
		{
			// @TODO: Implement for Linux
			return new List<ProcessInfo>().ToArray();
		}

		/// <summary>
		/// Currently Mono returns incomplete list of modules for Process.Modules so we need to parse /proc/PID/maps.
		/// (also, Mono locks up during process traversal sometimes, trying to open /dev/snd/pcm*)
		/// </summary>
		/// <param name="PID"></param>
		/// <param name="Filename"></param>
		/// <returns></returns>
		public override string[] GetProcessModules(int PID, string Filename)
		{
			// @TODO: Implement for Linux
			return new List<string>().ToArray();
		}

		internal override IEnumerable<ProjectFileFormat> GetDefaultProjectFileFormats()
		{
			yield return ProjectFileFormat.Make;
			yield return ProjectFileFormat.VisualStudioCode;
			yield return ProjectFileFormat.KDevelop;
			yield return ProjectFileFormat.QMake;
			yield return ProjectFileFormat.CMake;
			yield return ProjectFileFormat.CodeLite;
		}
	}
}
