// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Threading;
using System.Text;
using System.Text.RegularExpressions;
using System.Drawing;
using System.Linq;
using System.Collections;
using System.Diagnostics;

namespace Gauntlet
{
	public enum ERoleModifier
	{
		None,
		Dummy,
		Null
	};

	/// <summary>
	/// Represents a role that will be performed in an Unreal Session
	/// </summary>
	public class UnrealSessionRole
	{

		/// <summary>
		/// Type of role
		/// </summary>
		public UnrealTargetRole RoleType;

		/// <summary>
		/// Platform this role uses
		/// </summary>
		public UnrealTargetPlatform Platform;

		/// <summary>
		/// Configuration this role runs in
		/// </summary>
		public UnrealTargetConfiguration Configuration;

		/// <summary>
		/// Constraints this role runs under
		/// </summary>
		public UnrealTargetConstraint Constraint;

		/// <summary>
		/// Options that this role needs
		/// </summary>
		public IConfigOption<UnrealAppConfig> Options;

		/// <summary>
		/// Command line that this role will use
		/// </summary>
		public string CommandLine;

		/// <summary>
		/// List of files to copy to the device.
		/// </summary>
		public List<UnrealFileToCopy> FilesToCopy;

		/// <summary>
		/// Properties we require our build to have
		/// </summary>
		public BuildFlags RequiredBuildFlags;

		/// <summary>
		/// Should be represented by a null device?
		/// </summary>
		public ERoleModifier RoleModifier;

		/// <summary>
		/// Is this a dummy executable? 
		/// </summary>
		public bool IsDummy() { return RoleModifier == ERoleModifier.Dummy; }

		/// <summary>
		/// Is this role Null? 
		/// </summary>
		public bool IsNullRole() { return RoleModifier == ERoleModifier.Null; }


		/// <summary>
		/// Constructor taking limited params
		/// </summary>
		/// <param name="InType"></param>
		/// <param name="InPlatform"></param>
		/// <param name="InConfiguration"></param>
		/// <param name="InOptions"></param>
		public UnrealSessionRole(UnrealTargetRole InType, UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, IConfigOption<UnrealAppConfig> InOptions)
			: this(InType, InPlatform, InConfiguration, null, InOptions)
		{
		}

		/// <summary>
		/// Constructor taking optional params
		/// </summary>
		/// <param name="InType"></param>
		/// <param name="InPlatform"></param>
		/// <param name="InConfiguration"></param>
		/// <param name="InCommandLine"></param>
		/// <param name="InOptions"></param>
		public UnrealSessionRole(UnrealTargetRole InType, UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, string InCommandLine = null, IConfigOption<UnrealAppConfig> InOptions = null)
		{
			RoleType = InType;

			Platform = InPlatform;
			Configuration = InConfiguration;

			if (string.IsNullOrEmpty(InCommandLine))
			{
				CommandLine = "";
			}
			else
			{
				CommandLine = InCommandLine;
			}

			RequiredBuildFlags = BuildFlags.None;

			if (Globals.Params.ParseParam("dev") && !RoleType.UsesEditor())
			{
				RequiredBuildFlags |= BuildFlags.CanReplaceExecutable;
			}

            if (Globals.Params.ParseParam("bulk") && InPlatform == UnrealTargetPlatform.Android)
            {
                RequiredBuildFlags |= BuildFlags.Bulk;
            }

            Options = InOptions;
            FilesToCopy = new List<UnrealFileToCopy>();
			RoleModifier = ERoleModifier.None;
        }

        /// <summary>
        /// Debugging aid
        /// </summary>
        /// <returns></returns>
        public override string ToString()
		{
			return string.Format("{0} {1} {2}", Platform, Configuration, RoleType);
		}
	}

	/// <summary>
	/// Represents an instance of a running an Unreal session. Basically an aggregate of all processes for
	/// all roles (clients, server, etc
	/// 
	/// TODO - combine this into UnrealSession
	/// </summary>
	public class UnrealSessionInstance : IDisposable
	{
		/// <summary>
		/// Represents an running role in our session
		/// </summary>
		public class RoleInstance
		{
			public RoleInstance(UnrealSessionRole InRole, IAppInstance InInstance)
			{
				Role = InRole;
				AppInstance = InInstance;
			}

			/// <summary>
			/// Role that is being performed in this session
			/// </summary>
			public UnrealSessionRole Role { get; protected set; }

			/// <summary>
			/// Underlying AppInstance that us running the role
			/// </summary>
			public IAppInstance AppInstance { get; protected set; }

			/// <summary>
			/// Debugging aid
			/// </summary>
			/// <returns></returns>
			public override string ToString()
			{
				return Role.ToString();
			}
		};

		/// <summary>
		/// All running roles
		/// </summary>
		public RoleInstance[] RunningRoles { get; protected set; }

		/// <summary>
		/// Helper for accessing all client processes. May return an empty array if no clients are involved
		/// </summary>
		public IAppInstance[] ClientApps
		{
			get
			{
				return RunningRoles.Where(R => R.Role.RoleType.IsClient()).Select(R => R.AppInstance).ToArray();
			}
		}

		/// <summary>
		/// Helper for accessing server process. May return null if no serer is invlved
		/// </summary>
		public IAppInstance ServerApp
		{
			get
			{
				return RunningRoles.Where(R => R.Role.RoleType.IsServer()).Select(R => R.AppInstance).FirstOrDefault();
			}
		}

		/// <summary>
		/// Helper for accessing server process. May return null if no serer is invlved
		/// </summary>
		public IAppInstance EditorApp
		{
			get
			{
				return RunningRoles.Where(R => R.Role.RoleType.IsEditor()).Select(R => R.AppInstance).FirstOrDefault();
			}
		}

		/// <summary>
		/// Helper that returns true if clients are currently running
		/// </summary>
		public bool ClientsRunning
		{
			get
			{
				return ClientApps != null && ClientApps.Where(C => C.HasExited).Count() == 0;
			}
		}

		/// <summary>
		/// Helper that returns true if there's a running server
		/// </summary>
		public bool ServerRunning
		{
			get
			{
				return ServerApp != null && ServerApp.HasExited == false;
			}
		}

		/// <summary>
		/// Returns true if any of our roles are still running
		/// </summary>
		public bool IsRunningRoles
		{
			get
			{
				return RunningRoles.Any(R => R.AppInstance.HasExited == false);
			}
		}

		/// <summary>
		/// COnstructor. Roles must be passed in
		/// </summary>
		/// <param name="InRunningRoles"></param>
		public UnrealSessionInstance(RoleInstance[] InRunningRoles)
		{
			RunningRoles = InRunningRoles;
		}

		~UnrealSessionInstance()
		{
			Dispose(false);
		}

		#region IDisposable Support
		private bool disposedValue = false; // To detect redundant calls

		protected virtual void Dispose(bool disposing)
		{
			if (!disposedValue)
			{
				if (disposing)
				{
					// TODO: dispose managed state (managed objects).
				}

				Shutdown();
				disposedValue = true;
			}
		}

		// This code added to correctly implement the disposable pattern.
		public void Dispose()
		{
			// Do not change this code. Put cleanup code in Dispose(bool disposing) above.
			Dispose(true);
		}
		#endregion

		/// <summary>
		/// Shutdown the session by killing any remaining processes.
		/// </summary>
		/// <returns></returns>
		public void Shutdown()
		{
			// Kill any remaining client processes
			if (ClientApps != null)
			{
				List<IAppInstance> RunningApps = ClientApps.Where(App => App.HasExited == false).ToList();

				if (RunningApps.Count > 0)
				{
					Log.Info("Shutting down {0} clients", RunningApps.Count);
					RunningApps.ForEach(App => App.Kill());
				}
			}

			if (ServerApp != null)
			{
				if (ServerApp.HasExited == false)
				{
					Log.Info("Shutting down server");
					ServerApp.Kill();
				}
			}

			// kill anything that's left
			RunningRoles.Where(R => R.AppInstance.HasExited == false).ToList().ForEach(R => R.AppInstance.Kill());

			// Wait for it all to end
			RunningRoles.ToList().ForEach(R => R.AppInstance.WaitForExit());

			Thread.Sleep(3000);
		}
	}

	/// <summary>
	/// Represents the set of available artifacts available after an UnrealSessionRole has completed
	/// </summary>
	public class UnrealRoleArtifacts
	{
		/// <summary>
		/// Session role info that created these artifacts
		/// </summary>
		public UnrealSessionRole SessionRole { get; protected set; }

		/// <summary>
		/// AppInstance that was used to run this role
		/// </summary>
		public IAppInstance AppInstance { get; protected set; }

		/// <summary>
		/// Path to artifacts from this role (these are local and were retried from the device).
		/// </summary>
		public string ArtifactPath { get; protected set; }

		/// <summary>
		/// Out log file wrapped in a parser
		/// </summary>
		public UnrealLogParser LogParser { get; protected set; }

		/// <summary>
		/// A preprocessed summary of our log file
		/// </summary>
		public UnrealLogParser.LogSummary LogSummary { get { return LogParser.GetSummary(); } }

		/// <summary>
		/// Path to Log from this role
		/// </summary>
		public string LogPath { get; protected set; }

		/// <summary>
		/// Constructor, all values must be provided
		/// </summary>
		/// <param name="InSessionRole"></param>
		/// <param name="InAppInstance"></param>
		/// <param name="InArtifactPath"></param>
		/// <param name="InLogSummary"></param>
		public UnrealRoleArtifacts(UnrealSessionRole InSessionRole, IAppInstance InAppInstance, string InArtifactPath, string InLogPath, UnrealLogParser InLog)
		{
			SessionRole = InSessionRole;
			AppInstance = InAppInstance;
			ArtifactPath = InArtifactPath;
			LogPath = InLogPath;
			LogParser = InLog;
		}
	}


	/// <summary>
	/// Helper class that understands how to launch/monitor/stop an an Unreal test (clients + server) based on params contained in the test context and config
	/// </summary>
	public class UnrealSession : IDisposable
	{
		/// <summary>
		/// Source of the build that will be launched
		/// </summary>
		protected UnrealBuildSource BuildSource { get; set; }

		/// <summary>
		/// Roles that will be performed by this session
		/// </summary>
		protected UnrealSessionRole[] SessionRoles { get; set; }

		/// <summary>
		/// Running instance of this session
		/// </summary>
		public UnrealSessionInstance SessionInstance { get; protected set; }

		/// <summary>
		/// Sandbox for installed apps
		/// </summary>
		public string Sandbox { get; set; }

		/// <summary>
		/// Devices that experienced problems
		/// </summary>
		internal List<ProblemDevice> ProblemDevices { get; set; }

		/// <summary>
		/// Devices that were reserved for our session
		/// </summary>
		protected List<ITargetDevice> ReservedDevices { get; set; }
		
		/// <summary>
		/// Constructor that takes a build source and a number of roles
		/// </summary>
		/// <param name="InSource"></param>
		/// <param name="InSessionRoles"></param>
		public UnrealSession(UnrealBuildSource InSource, IEnumerable<UnrealSessionRole> InSessionRoles)
		{
			BuildSource = InSource;
			SessionRoles = InSessionRoles.ToArray();

			if (SessionRoles.Length == 0)
			{
				throw new AutomationException("No roles specified for Unreal session");
			}

			List<string> ValidationIssues = new List<string>();
            ProblemDevices = new List<ProblemDevice>();
            ReservedDevices = new List<ITargetDevice>();

			if (!CheckRolesArePossible(ref ValidationIssues))
			{
				ValidationIssues.ForEach(S => Log.Error("{0}", S));
				throw new AutomationException("One or more issues occurred when validating build {0} against requested roles", InSource.BuildName);
			}
		}

		/// <summary>
		/// Destructor, terminates any running session
		/// </summary>
		~UnrealSession()
		{
			Dispose(false);
		}

		#region IDisposable Support
		private bool disposedValue = false; // To detect redundant calls

		protected virtual void Dispose(bool disposing)
		{
			if (!disposedValue)
			{
				if (disposing)
				{
					// TODO: dispose managed state (managed objects).
				}

				ShutdownSession();
				disposedValue = true;
			}
		}

		// This code added to correctly implement the disposable pattern.
		public void Dispose()
		{
			// Do not change this code. Put cleanup code in Dispose(bool disposing) above.
			Dispose(true);
		}
		#endregion

		void ReleaseDevices()
		{
			if (ReservedDevices.Count() > 0)
			{
				DevicePool.Instance.ReleaseDevices(ReservedDevices);
				ReservedDevices.Clear();
			}
		}

		void MarkProblemDevice(ITargetDevice Device)
		{
			if (ProblemDevices.Where(D => D.Name == Device.Name && D.Platform == Device.Platform).Count() > 0)
			{
				return;
			}

			// @todo Notify service of problem (reboot device, notify email, etc)
			// also, pass problem devices to service when asking for reservation, so don't get that device back
			ProblemDevices.Add(new ProblemDevice(Device.Name, Device.Platform));
		}

		/// <summary>
		/// Helper that reserves and returns a list of available devices based on the passed in roles
		/// </summary>
		/// <param name="Configs"></param>
		/// <returns></returns>
		public bool TryReserveDevices()
		{
			List<ITargetDevice> AcquiredDevices = new List<ITargetDevice>();
			List<ITargetDevice> SkippedDevices = new List<ITargetDevice>();

			ReleaseDevices();

			// figure out how many of each device we need
			Dictionary<UnrealTargetConstraint, int> RequiredDeviceTypes = new Dictionary<UnrealTargetConstraint, int>();
			IEnumerable<UnrealSessionRole> RolesNeedingDevices = SessionRoles.Where(R => !R.IsNullRole());

			// Get a count of the number of devices required for each platform
			RolesNeedingDevices.ToList().ForEach(C =>
			{
				if (!RequiredDeviceTypes.ContainsKey(C.Constraint))
				{
					RequiredDeviceTypes[C.Constraint] = 0;
				}
				RequiredDeviceTypes[C.Constraint]++;
			});

			// check whether pool can accommodate devices
			if (!DevicePool.Instance.CheckAvailableDevices(RequiredDeviceTypes, ProblemDevices))
			{
				return false;
			}

			// AG - this needs more thought as it can either be good (lots of devices under contention, temp failure) or
			// just burn cycles spinning on something that won't ever work...

			// If we had problem devices last time see if there are enough of that type that we can exclude them
			//if (ProblemDevices.Count > 0)
			//{
			//	Dictionary<UnrealTargetPlatform, int> ProblemPlatforms = new Dictionary<UnrealTargetPlatform, int>();

			//	// count how many problems for each platform
			//	ProblemDevices.ForEach(Device =>
			//	{
			//		if (ProblemPlatforms.ContainsKey(Device.Platform) == false)
			//		{
			//			ProblemPlatforms[Device.Platform] = 0;
			//		}

			//		ProblemPlatforms[Device.Platform]++;
			//	});

			//	List<ITargetDevice> ProblemsDevicesToRelease = new List<ITargetDevice>();

			//	// foreach device, see if we have enough others that we can ignore it
			//	ProblemDevices.ForEach(Device =>
			//	{
			//		if (ProblemPlatforms[Device.Platform] < AvailableDeviceTypes[Device.Platform])
			//		{
			//			Log.Verbose("Had problem with device {0} last time, will now ignore", Device.Name);
			//		}
			//		else
			//		{
			//			Log.Verbose("Would like to ignore device {0} but not enough of this type in pool", Device.Name);
			//			ProblemsDevicesToRelease.Add(Device);
			//		}
			//	});

			//	// remove any 
			//	ProblemDevices = ProblemDevices.Where(D => ProblemsDevicesToRelease.Contains(D) == false).ToList();
			//}

			// nothing acquired yet...
			AcquiredDevices.Clear();

			// for each platform, enumerate and select from the available devices
			foreach (var PlatformReqKP in RequiredDeviceTypes)
			{
				UnrealTargetConstraint Constraint = PlatformReqKP.Key;
				UnrealTargetPlatform Platform = Constraint.Platform;

				int NeedOfThisType = RequiredDeviceTypes[Constraint];

				DevicePool.Instance.EnumerateDevices(Constraint, Device =>
				{
					int HaveOfThisType = AcquiredDevices.Where(D => D.Platform == Device.Platform && Constraint.Check(Device)).Count();

					bool WeWant = NeedOfThisType > HaveOfThisType;


					if (WeWant)
					{
						bool Available = Device.IsAvailable;
						bool Have = AcquiredDevices.Contains(Device);
						
						bool Problem = ProblemDevices.Where(D => D.Name == Device.Name && D.Platform == Device.Platform).Count() > 0;

						Log.Verbose("Device {0}: Available:{1}, Have:{2}, HasProblem:{3}", Device.Name, Available, Have, Problem);

						if (Available
							&& Have == false
							&& Problem == false)
						{
							Log.Info("Acquiring device {0}", Device.Name);
							AcquiredDevices.Add(Device);
							HaveOfThisType++;
						}
						else
						{
							Log.Info("Skipping device {0}", Device.Name);
							SkippedDevices.Add(Device);
						}
					}

					// continue if we need more of this platform type
					return HaveOfThisType < NeedOfThisType;
				});
			}

			// If we got enough devices, go to step2 where we provision and try to connect them
			if (AcquiredDevices.Count == RolesNeedingDevices.Count())
			{
				// actually acquire them
				DevicePool.Instance.ReserveDevices(AcquiredDevices);

				Log.Info("Selected devices {0} for client(s). Prepping", string.Join(", ", AcquiredDevices));

				foreach (ITargetDevice Device in AcquiredDevices)
				{
					if (Device.IsOn == false)
					{
						Log.Info("Powering on {0}", Device);
						Device.PowerOn();
					}
					else if (Globals.Params.ParseParam("reboot"))
					{
						Log.Info("Rebooting {0}", Device);
						Device.Reboot();
					}

					if (Device.IsConnected == false)
					{
						Log.Verbose("Connecting to {0}", Device);
						Device.Connect();
					}
				}

				// Step 3: Verify we actually connected to them
				var LostDevices = AcquiredDevices.Where(D => !D.IsConnected);

				if (LostDevices.Count() > 0)
				{
					Log.Info("Lost connection to devices {0} for client(s). ", string.Join(", ", LostDevices));

					// mark these as problems. Could be something grabbed them before us, could be that they are
					// unresponsive in some way
					LostDevices.ToList().ForEach(D => MarkProblemDevice(D));
					AcquiredDevices.ToList().ForEach(D => D.Disconnect());

					DevicePool.Instance.ReleaseDevices(AcquiredDevices);
					AcquiredDevices.Clear();
				}
			}

			if (AcquiredDevices.Count() != RolesNeedingDevices.Count())
			{
				Log.Info("Failed to resolve all devices. Releasing the ones we have ");
				DevicePool.Instance.ReleaseDevices(AcquiredDevices);
			}
			else
			{
				ReservedDevices = AcquiredDevices;
			}

			// release devices that were skipped
			DevicePool.Instance.ReleaseDevices(SkippedDevices);			

			return ReservedDevices.Count() == RolesNeedingDevices.Count();
		}

		/// <summary>
		/// Check that all the current roles can be performed by our build source
		/// </summary>
		/// <param name="Issues"></param>
		/// <returns></returns>
		bool CheckRolesArePossible(ref List<string> Issues)
		{
			bool Success = true;
			foreach (var Role in SessionRoles)
			{
				if (!BuildSource.CanSupportRole(Role, ref Issues))
				{
					Success = false;
				}
			}

			return Success;
		}

		/// <summary>
		/// Installs and launches all of our roles and returns an UnrealSessonInstance that represents the aggregate
		/// of all of these processes. Will perform retries if errors with devices are encountered so this is a "best
		/// attempt" at running with the devices provided
		/// </summary>
		/// <returns></returns>
		public UnrealSessionInstance LaunchSession()
		{
			SessionInstance = null;

			// tries to find devices and launch our session. Will loop until we succeed, we run out of devices/retries, or
			// something fatal occurs..
			while (SessionInstance == null && Globals.CancelSignalled == false)
			{
				int Retries = 5;
				int RetryWait = 120;

				IEnumerable<UnrealSessionRole> RolesNeedingDevices = SessionRoles.Where(R => R.IsNullRole() == false);

				while (ReservedDevices.Count() < RolesNeedingDevices.Count())
				{
					// get devices
					TryReserveDevices();

					if (Globals.CancelSignalled)
					{
						break;
					}

					// if we failed to get enough devices, show a message and wait
					if (ReservedDevices.Count() != SessionRoles.Count())
					{
						if (Retries == 0)
						{
							throw new AutomationException("Unable to acquire all devices for test.");
						}
						Log.Info("\nUnable to find enough device(s). Waiting {0} secs (retries left={1})\n", RetryWait, --Retries);
						Thread.Sleep(RetryWait * 1000);
					}
				}

				if (Globals.CancelSignalled)
				{
					return null;
				}

				Dictionary<IAppInstall, UnrealSessionRole> InstallsToRoles = new Dictionary<IAppInstall, UnrealSessionRole>();

				// create a copy of our list
				IEnumerable<ITargetDevice> DevicesToInstallOn = ReservedDevices.ToArray();

				bool InstallSuccess = true;

				// sort by constraints, so that we pick constrained devices first
				List<UnrealSessionRole> SortedRoles = SessionRoles.OrderBy(R => R.Constraint.IsIdentity() ? 1 : 0).ToList();				

				// first install all roles on these devices
				foreach (var Role in SortedRoles)
				{
					ITargetDevice Device = null;

					if (Role.IsNullRole() == false)
					{
						Device = DevicesToInstallOn.Where(D => D.IsConnected && D.Platform == Role.Platform
															&& ( Role.Constraint.IsIdentity() || DevicePool.Instance.GetConstraint(D) == Role.Constraint)).First();

						DevicesToInstallOn = DevicesToInstallOn.Where(D => D != Device);
					}
					else
					{
						Device = new TargetDeviceNull(string.Format("Null{0}", Role.RoleType));
					}

					// create a config from the build source (this also applies the role options)
					UnrealAppConfig AppConfig = BuildSource.CreateConfiguration(Role);

					// todo - should this be elsewhere?
					AppConfig.Sandbox = Sandbox;

					IAppInstall Install = null;

					try
					{
						Install = Device.InstallApplication(AppConfig);
					}
					catch (System.Exception Ex)
					{
						// Warn, ignore the device, and do not continue
						Log.Info("Failed to install app onto device {0} for role {1}. {2}. Will retry with new device", Device, Role, Ex);
						MarkProblemDevice(Device);
						InstallSuccess = false;
						break;
					}
										

					if (Globals.CancelSignalled)
					{
						break;
					}

					InstallsToRoles[Install] = Role;
				}

				if (InstallSuccess == false)
				{
					// release all devices
					ReleaseDevices();
				}
				

				if (InstallSuccess && Globals.CancelSignalled == false)
				{
					List<UnrealSessionInstance.RoleInstance> RunningRoles = new List<UnrealSessionInstance.RoleInstance>();

					// Now try to run all installs on their devices
					foreach (var InstallRoleKV in InstallsToRoles)
					{
						IAppInstall CurrentInstall = InstallRoleKV.Key;

						bool Success = false;

						try
						{
							IAppInstance Instance = CurrentInstall.Run();

							if (Instance != null || Globals.CancelSignalled)
							{
								RunningRoles.Add(new UnrealSessionInstance.RoleInstance(InstallRoleKV.Value, Instance));
							}

							Success = true;
						}
						catch (DeviceException Ex)
						{
							// shutdown all 
							Log.Warning("Device {0} threw an exception during launch. \nException={1}", CurrentInstall.Device, Ex.Message);
							Success = false;
						}

						if (Success == false)
						{
							Log.Warning("Failed to start build on {0}. Marking as problem device and retrying with new set", CurrentInstall.Device);

							// terminate anything that's running
							foreach (UnrealSessionInstance.RoleInstance RunningRole in RunningRoles)
							{
								Log.Info("Shutting down {0}", RunningRole.AppInstance.Device);
								RunningRole.AppInstance.Kill();
								RunningRole.AppInstance.Device.Disconnect();
							}

							// mark that device as a problem
							MarkProblemDevice(CurrentInstall.Device);

							// release all devices
							ReleaseDevices();

							break; // do not continue loop
						}
					}

					if (RunningRoles.Count() == SessionRoles.Count())
					{
						SessionInstance = new UnrealSessionInstance(RunningRoles.ToArray());
					}
				}
			}

			return SessionInstance;
		}

		/// <summary>
		/// Restarts the current session (if any)
		/// </summary>
		/// <returns></returns>
		public UnrealSessionInstance RestartSession()
		{
			ShutdownSession();

			// AG-TODO - want to preserve device reservations here...

			return LaunchSession();
		}

		/// <summary>
		/// Shuts down the current session (if any)
		/// </summary>
		/// <returns></returns>
		public void ShutdownSession()
		{
			if (SessionInstance != null)
			{
				SessionInstance.Dispose();
				SessionInstance = null;
			}

			ReleaseDevices();
		}

		/// <summary>
		/// Retrieves and saves all artifacts from the provided session role. Artifacts are saved to the destination path 
		/// </summary>
		/// <param name="InContext"></param>
		/// <param name="InRunningRole"></param>
		/// <param name="InDestArtifactPath"></param>
		/// <returns></returns>
		public UnrealRoleArtifacts SaveRoleArtifacts(UnrealTestContext InContext, UnrealSessionInstance.RoleInstance InRunningRole, string InDestArtifactPath)
		{

			bool IsServer = InRunningRole.Role.RoleType.IsServer();
			string RoleName = (InRunningRole.Role.IsDummy() ? "Dummy" : "") + InRunningRole.Role.RoleType.ToString();
			UnrealTargetPlatform Platform = InRunningRole.Role.Platform;
			string RoleConfig = InRunningRole.Role.Configuration.ToString();

			Directory.CreateDirectory(InDestArtifactPath);

			// Don't archive editor data, there can be a *lot* of stuff in that saved folder!
			bool IsEditor = InRunningRole.Role.RoleType.UsesEditor();

			bool IsDevBuild = InContext.TestParams.ParseParam("dev");

			string DestSavedDir = Path.Combine(InDestArtifactPath, "Saved");
			string SourceSavedDir = "";

			// save the contents of the saved directory
			SourceSavedDir = InRunningRole.AppInstance.ArtifactPath;

			// save the output from TTY
			string ArtifactLogPath = Path.Combine(InDestArtifactPath, RoleName + "Output.log");

			// Write a brief Gauntlet header to aid debugging
			StringBuilder LogOut = new StringBuilder();
			LogOut.AppendLine("------ Gauntlet Test ------");
			LogOut.AppendFormat("Role: {0}\r\n", InRunningRole.Role);
			LogOut.AppendFormat("Automation Command: {0}\r\n", Environment.CommandLine);
			LogOut.AppendLine("---------------------------");

			// Write instance stdout stream
			LogOut.Append(InRunningRole.AppInstance.StdOut);

			File.WriteAllText(ArtifactLogPath, LogOut.ToString());
			Log.Info("Wrote Log to {0}", ArtifactLogPath);

			if (IsServer == false)
			{
				// gif-ify and jpeg-ify any screenshots
				try
				{

					string ScreenshotPath = Path.Combine(SourceSavedDir, "Screenshots", Platform.ToString()).ToLower();

					if (Directory.Exists(ScreenshotPath) && Directory.GetFiles(ScreenshotPath).Length > 0)
					{
						Log.Info("Downsizing and gifying session images at {0}", ScreenshotPath);

						// downsize first so gif-step is quicker and takes less resoruces.
						Utils.Image.ConvertImages(ScreenshotPath, ScreenshotPath, "jpg", true);

						string GifPath = Path.Combine(InDestArtifactPath, RoleName + "Test.gif");
						if (Utils.Image.SaveImagesAsGif(ScreenshotPath, GifPath))
						{
							Log.Info("Saved gif to {0}", GifPath);
						}
					}
				}
				catch (Exception Ex)
				{
					Log.Info("Failed to downsize and gif-ify images! {0}", Ex);
				}
			}

			// don't archive data in dev mode, because peoples saved data could be huuuuuuuge!
			if (IsEditor == false)
			{
				LogLevel OldLevel = Log.Level;
				Log.Level = LogLevel.Normal;

				if (Directory.Exists(SourceSavedDir))
				{
					Utils.SystemHelpers.CopyDirectory(SourceSavedDir, DestSavedDir);
					Log.Info("Archived artifacts to to {0}", DestSavedDir);
				}
				else
				{
					Log.Info("Archive path '{0}' was not found!", SourceSavedDir);
				}

				Log.Level = OldLevel;
			}
			else
			{
				if (IsEditor)
				{
					Log.Info("Skipping archival of assets for editor {0}", RoleName);
				}
				else if (IsDevBuild)
				{
					Log.Info("Skipping archival of assets for dev build");
				}
			}			


			// TODO REMOVEME- this should go elsewhere, likely a util that can be called or inserted by relevant test nodes.
			if (IsServer == false)
			{
				// Copy over PSOs
				try
				{
					if (InContext.Options.LogPSO)
					{
						foreach (var ThisFile in CommandUtils.FindFiles_NoExceptions(true, "*.rec.upipelinecache", true, DestSavedDir))
						{
							bool Copied = false;
							var JustFile = Path.GetFileName(ThisFile);
							if (JustFile.StartsWith("++"))
							{

								var Parts = JustFile.Split(new Char[] { '+', '-' }).Where(A => A != "").ToArray();
								if (Parts.Count() >= 2)
								{
									string ProjectName = Parts[0].ToString();
									string BuildRoot = CommandUtils.CombinePaths(CommandUtils.RootBuildStorageDirectory());

									string SrcBuildPath = CommandUtils.CombinePaths(BuildRoot, ProjectName);
									string SrcBuildPath2 = CommandUtils.CombinePaths(BuildRoot, ProjectName.Replace("Game", "").Replace("game", ""));

									if (!CommandUtils.DirectoryExists(SrcBuildPath))
									{
										SrcBuildPath = SrcBuildPath2;
									}
									if (CommandUtils.DirectoryExists(SrcBuildPath))
									{
										var JustBuildFolder = JustFile.Replace("-" + Parts.Last(), "");

										string PlatformStr = Platform.ToString();
										string SrcCLMetaPath = CommandUtils.CombinePaths(SrcBuildPath, JustBuildFolder, PlatformStr, "MetaData");
										if (CommandUtils.DirectoryExists(SrcCLMetaPath))
										{
											string SrcCLMetaPathCollected = CommandUtils.CombinePaths(SrcCLMetaPath, "CollectedPSOs");
											if (!CommandUtils.DirectoryExists(SrcCLMetaPathCollected))
											{
												Log.Info("Creating Directory {0}", SrcCLMetaPathCollected);
												CommandUtils.CreateDirectory(SrcCLMetaPathCollected);
											}
											if (CommandUtils.DirectoryExists(SrcCLMetaPathCollected))
											{
												string DestFile = CommandUtils.CombinePaths(SrcCLMetaPathCollected, JustFile);
												CommandUtils.CopyFile_NoExceptions(ThisFile, DestFile, true);
												if (CommandUtils.FileExists(true, DestFile))
												{
													Log.Info("Deleting local file, copied to {0}", DestFile);
													CommandUtils.DeleteFile_NoExceptions(ThisFile, true);
													Copied = true;
												}
											}
										}
									}
								}
							}
							if (!Copied)
							{
								Log.Warning("Could not find anywhere to put this file {0}", JustFile);
							}
						}
					}
				}
				catch (Exception Ex)
				{
					Log.Info("Failed to copy upipelinecaches to the network {0}", Ex);
				}

			}
			// END REMOVEME


			UnrealLogParser LogParser = new UnrealLogParser(InRunningRole.AppInstance.StdOut);

			int ExitCode = InRunningRole.AppInstance.ExitCode;
			LogParser.GetTestExitCode(out ExitCode);

			UnrealRoleArtifacts Artifacts = new UnrealRoleArtifacts(InRunningRole.Role, InRunningRole.AppInstance, InDestArtifactPath, ArtifactLogPath, LogParser);

			return Artifacts;
		}

		/// <summary>
		/// Saves all artifacts from the provided session to the specified output path. 
		/// </summary>
		/// <param name="Context"></param>
		/// <param name="TestInstance"></param>
		/// <param name="OutputPath"></param>
		/// <returns></returns>
		public IEnumerable<UnrealRoleArtifacts> SaveRoleArtifacts(UnrealTestContext Context, UnrealSessionInstance TestInstance, string OutputPath)
		{
			int ClientCount = 1;
			int DummyClientCount = 1;

			List<UnrealRoleArtifacts> AllArtifacts = new List<UnrealRoleArtifacts>();

			foreach (UnrealSessionInstance.RoleInstance App in TestInstance.RunningRoles)
			{
				bool IsServer = App.Role.RoleType.IsServer();
				string RoleName = (App.Role.IsDummy() ? "Dummy" : "") + App.Role.RoleType.ToString();
				string FolderName = RoleName;

				if (IsServer == false)
				{
					if ((!App.Role.IsDummy() && ClientCount++ > 1))
					{
						FolderName += string.Format("_{0:00}", ClientCount);
					}
					else if (App.Role.IsDummy() && DummyClientCount++ > 1)
					{
						FolderName += string.Format("_{0:00}", DummyClientCount);
					}
				}

				string DestPath = Path.Combine(OutputPath, FolderName);

				var Artifacts = SaveRoleArtifacts(Context, App, DestPath);
				AllArtifacts.Add(Artifacts);
			}

			return AllArtifacts;
		}
	}
}