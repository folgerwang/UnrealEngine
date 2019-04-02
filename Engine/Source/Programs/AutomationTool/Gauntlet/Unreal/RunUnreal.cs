// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using AutomationTool;
using AutomationTool.DeviceReservation;
using UnrealBuildTool;
using Gauntlet;
using System.IO;
using Newtonsoft.Json;
using System.Reflection;
using System.Text.RegularExpressions;

namespace Gauntlet
{
	/*
	 	public class RunUnreal<SourceType, OptionType> : BuildCommand
		where SourceType : UnrealBuildSource, 
		where OptionType : UnrealTestOptions
	 */

	/// <summary>
	/// Base class for executing Unreal tests.
	/// 
	/// For a full list of options see UnrealTestContextOption
	/// 
	/// </summary>
	public class RunUnreal : BuildCommand
	{
		/// <summary>
		/// Main UAT entrance point. Custom games can derive from RunUnrealTests to run custom setup steps or
		/// directly set params on ContextOptions to remove the need for certain command line params (e.g.
		/// -project=FooGame
		/// </summary>
		/// <returns></returns>
		public override ExitCode Execute()
		{
			Globals.Params = new Gauntlet.Params(this.Params);

			UnrealTestOptions ContextOptions = new UnrealTestOptions();

			AutoParam.ApplyParamsAndDefaults(ContextOptions, Globals.Params.AllArguments);

			if (string.IsNullOrEmpty(ContextOptions.Project))
			{
				throw new AutomationException("No project specified. Use -project=ShooterGame etc");
			}

			ContextOptions.Namespaces = "Gauntlet.UnrealTest,UE4Game";
			ContextOptions.UsesSharedBuildType = true;

			return RunTests(ContextOptions);
		}

		/// <summary>
		/// Execute all tests according to the provided context
		/// </summary>
		/// <param name="Context"></param>
		/// <returns></returns>
		public ExitCode RunTests(UnrealTestOptions ContextOptions)
		{
			if (ContextOptions.Verbose)
			{
				Gauntlet.Log.Level = Gauntlet.LogLevel.Verbose;
			}

			if (ContextOptions.VeryVerbose)
			{
				Gauntlet.Log.Level = Gauntlet.LogLevel.VeryVerbose;
			}

			if (ParseParam("log"))
			{
				if (!Directory.Exists(ContextOptions.LogDir))
				{
					Directory.CreateDirectory(ContextOptions.LogDir);
				}

				// include test names and timestamp in log filename as multiple (parallel or sequential) Gauntlet tests may be outputting to same directory
				string LogPath = Path.Combine(ContextOptions.LogDir, string.Format("GauntletLog{0}-{1}.txt", ContextOptions.TestList.Aggregate(new StringBuilder(), (SB, T) => SB.AppendFormat("-{0}", T.ToString())).ToString(), DateTime.Now.ToString(@"yyyy.MM.dd.HH.mm.ss")));
				Gauntlet.Log.Verbose("Writing Gauntlet log to {0}", LogPath);
				Gauntlet.Log.SaveToFile(LogPath);
			}

			// prune our temp folder
			Utils.SystemHelpers.CleanupMarkedDirectories(ContextOptions.TempDir, 7);

			if (string.IsNullOrEmpty(ContextOptions.Build))
			{
				throw new AutomationException("No builds specified. Use -builds=p:\\path\\to\\build");
			}

			if (typeof(UnrealBuildSource).IsAssignableFrom(ContextOptions.BuildSourceType) == false)
			{
				throw new AutomationException("Provided BuildSource type does not inherit from UnrealBuildSource");
			}

			// make -test=none implicit if no test is supplied

			if (ContextOptions.TestList.Count == 0)
			{
				Gauntlet.Log.Info("No test specified, creating default test node");
				ContextOptions.TestList.Add(TestRequest.CreateRequest("DefaultTest"));
			}

			bool EditorForAllRoles = Globals.Params.ParseParam("editor") || string.Equals(Globals.Params.ParseValue("build", ""), "editor", StringComparison.OrdinalIgnoreCase);

			if (EditorForAllRoles)
			{
				Gauntlet.Log.Verbose("Will use Editor for all roles");
			}

			Dictionary<UnrealTargetRole, UnrealTestRoleContext> RoleContexts = new Dictionary<UnrealTargetRole, UnrealTestRoleContext>();

			// Default platform to the current os
			UnrealTargetPlatform DefaultPlatform = BuildHostPlatform.Current.Platform;
			UnrealTargetConfiguration DefaultConfiguration = UnrealTargetConfiguration.Development;

			// todo, pass this in as a BuildSource and remove the COntextOption params specific to finding builds
			UnrealBuildSource BuildInfo = (UnrealBuildSource)Activator.CreateInstance(ContextOptions.BuildSourceType, new object[] { ContextOptions.Project, ContextOptions.UsesSharedBuildType, Environment.CurrentDirectory, ContextOptions.Build, ContextOptions.SearchPaths });

			// Setup accounts
			SetupAccounts();

			List<ITestNode> AllTestNodes = new List<ITestNode>();

			bool InitializedDevices = false;

			HashSet<UnrealTargetPlatform> UsedPlatforms = new HashSet<UnrealTargetPlatform>();
			
			// for all platforms we want to test...
			foreach (ArgumentWithParams PlatformWithParams in ContextOptions.PlatformList)
			{
				string PlatformString = PlatformWithParams.Argument;

				// combine global and platform-specific params
				Params CombinedParams = new Params(ContextOptions.Params.AllArguments.Concat(PlatformWithParams.AllArguments).ToArray());

				UnrealTargetPlatform PlatformType;

				if (!Enum.TryParse<UnrealTargetPlatform>(PlatformString, true, out PlatformType))
				{
					throw new AutomationException("Unable to convert platform '{0}' into an UnrealTargetPlatform", PlatformString);
				}

				if (!InitializedDevices)
				{
					// Setup the devices and assign them to the executor
					SetupDevices(PlatformType, ContextOptions);
					InitializedDevices = true;
				}

				//  Create a context for each process type to operate as
				foreach (UnrealTargetRole Type in Enum.GetValues(typeof(UnrealTargetRole)))
				{
					UnrealTestRoleContext Role = new UnrealTestRoleContext();

					// Default to these
					Role.Type = Type;
					Role.Platform = DefaultPlatform;
					Role.Configuration = DefaultConfiguration;

					// globally, what was requested (e.g -platform=PS4 -configuration=Shipping)
					UnrealTargetPlatform RequestedPlatform = PlatformType;
					UnrealTargetConfiguration RequestedConfiguration = ContextOptions.Configuration;

					// look for FooConfiguration, FooPlatform overrides.
					// e.g. ServerConfiguration, ServerPlatform
					string PlatformRoleString = Globals.Params.ParseValue(Type.ToString() + "Platform", null);
					string ConfigString = Globals.Params.ParseValue(Type.ToString() + "Configuration", null);

					if (string.IsNullOrEmpty(PlatformRoleString) == false)
					{
						RequestedPlatform = (UnrealTargetPlatform)Enum.Parse(typeof(UnrealTargetPlatform), PlatformRoleString, true);
					}

					if (string.IsNullOrEmpty(ConfigString) == false)
					{
						RequestedConfiguration = (UnrealTargetConfiguration)Enum.Parse(typeof(UnrealTargetConfiguration), ConfigString, true);
					}

					// look for -clientargs= and -editorclient etc
					Role.ExtraArgs = Globals.Params.ParseValue(Type.ToString() + "Args", "");
					bool UsesEditor = EditorForAllRoles || Globals.Params.ParseParam("Editor" + Type.ToString());

					if (UsesEditor)
					{
						Gauntlet.Log.Verbose("Will use Editor for role {0}", Type);
					}

					Role.Skip = Globals.Params.ParseParam("Skip" + Type.ToString());

					if (Role.Skip)
					{
						Gauntlet.Log.Verbose("Will use NullPlatform to skip role {0}", Type);
					}

					// TODO - the below is a bit rigid, but maybe that's good enough since the "actually use the editor.." option
					// is specific to clients and servers

					// client can override platform and config
					if (Type.IsClient())
					{
						Role.Platform = RequestedPlatform;
						Role.Configuration = RequestedConfiguration;

						if (UsesEditor)
						{
							Role.Type = UnrealTargetRole.EditorGame;
							Role.Platform = DefaultPlatform;
							Role.Configuration = UnrealTargetConfiguration.Development;
						}
					}
					else if (Type.IsServer())
					{
						// server can only override config
						Role.Configuration = RequestedConfiguration;

						if (UsesEditor)
						{
							Role.Type = UnrealTargetRole.EditorServer;
							Role.Platform = DefaultPlatform;
							Role.Configuration = UnrealTargetConfiguration.Development;
						}
					}

					Gauntlet.Log.Verbose("Mapped Role {0} to RoleContext {1}", Type, Role);

					RoleContexts[Type] = Role;

					UsedPlatforms.Add(Role.Platform);
				}

				UnrealTestContext Context = new UnrealTestContext(BuildInfo, RoleContexts, ContextOptions);

				IEnumerable<ITestNode> TestNodes = CreateTestList(Context, CombinedParams, PlatformWithParams);

				AllTestNodes.AddRange(TestNodes);
			}

			bool AllTestsPassed = ExecuteTests(ContextOptions, AllTestNodes);

			// dispose now, not during shutdown gc, because this runs commands...
			DevicePool.Instance.Dispose();

			DoCleanup(UsedPlatforms);

			return AllTestsPassed ? ExitCode.Success : ExitCode.Error_TestFailure;
		}

		void DoCleanup(IEnumerable<UnrealTargetPlatform> UsedPlatforms)
		{
			if (!Globals.Params.ParseParam("removedevices"))
			{
				return;
			}

			if (UsedPlatforms.Contains(UnrealTargetPlatform.PS4))
			{
				String DevKitUtilPath = Path.Combine(Environment.CurrentDirectory, "Engine/Binaries/DotNET/PS4/PS4DevKitUtil.exe");
				Gauntlet.Log.Verbose("PS4DevkitUtil executing 'removeall'");
				IProcessResult BootResult = CommandUtils.Run(DevKitUtilPath, "removeall");
			}
		}


		bool ExecuteTests(UnrealTestOptions Options, IEnumerable<ITestNode> TestList)
		{
			// Create the test executor
			var Executor = new TextExecutor();
			
			try
			{
				bool Result = Executor.ExecuteTests(Options, TestList);

				return Result;
			}
			catch (System.Exception ex)
			{
				Gauntlet.Log.Info("");
				Gauntlet.Log.Error("{0}.\r\n\r\n{1}", ex.Message, ex.StackTrace);

				return false;
			}
			finally
			{
				Executor.Dispose();

				DevicePool.Instance.Dispose();

				if (ParseParam("clean"))
				{
					LogInformation("Deleting temp dir {0}", Options.TempDir);
					DirectoryInfo Di = new DirectoryInfo(Options.TempDir);
					if (Di.Exists)
					{
						Di.Delete(true);
					}
				}

				GC.Collect();
			}
		}

		/// <summary>
		/// Create the list of tests specified by the context. 
		/// </summary>
		/// <param name="Context"></param>
		/// <returns></returns>
		IEnumerable<ITestNode> CreateTestList(UnrealTestContext Context, Params DefaultParams, ArgumentWithParams PlatformParams = null)
		{
			List<ITestNode> NodeList = new List<ITestNode>();

			IEnumerable<string> Namespaces = Context.Options.Namespaces.Split(',').Select(S => S.Trim());
			
			List<string> BuildIssues = new List<string>();

			UnrealTargetPlatform UnrealPlatform = UnrealTargetPlatform.Unknown;
			if (!Enum.TryParse(PlatformParams.Argument, true, out UnrealPlatform))
			{
				throw new AutomationException("Could not convert platform {0} to a valid UnrealTargetPlatform", PlatformParams.Argument);
			}

			//List<string> Platforms = Globals.Params.ParseValue("platform")

			// Create an instance of each test and add it to the executor
			foreach (var Test in Context.Options.TestList)
			{
				// create a copy of the context for this test
				UnrealTestContext TestContext = (UnrealTestContext)Context.Clone();

				// if test specifies platforms, filter for this context
				if (Test.Platforms.Count() > 0 && Test.Platforms.Where(Plat => Plat.Argument == PlatformParams.Argument).Count() == 0)
				{
					continue;
				}

				if (Blacklist.Instance.IsTestBlacklisted(Test.TestName, UnrealPlatform, TestContext.BuildInfo.Branch))
				{
					Gauntlet.Log.Info("Test {0} is currently blacklisted on {1} in branch {2}", Test.TestName, UnrealPlatform, TestContext.BuildInfo.Branch);
					continue;
				}

				// combine global and test-specific params
				Params CombinedParams = new Params(DefaultParams.AllArguments.Concat(Test.TestParams.AllArguments).ToArray());

				// parse any target constraints
				List<string> PerfSpecArgs = CombinedParams.ParseValues("PerfSpec", false);
				string PerfSpecArg = PerfSpecArgs.Count > 0 ? PerfSpecArgs.Last() : "Unspecified";
				EPerfSpec PerfSpec;
				if (!Enum.TryParse<EPerfSpec>(PerfSpecArg, true, out PerfSpec))
				{
					throw new AutomationException("Unable to convert perfspec '{0}' into an EPerfSpec", PerfSpec);
				}

				TestContext.Constraint = new UnrealTargetConstraint(UnrealPlatform, PerfSpec);

				TestContext.TestParams = CombinedParams;

				// This will throw if the test cannot be created
				ITestNode NewTest = Utils.TestConstructor.ConstructTest<ITestNode, UnrealTestContext>(Test.TestName, TestContext, Namespaces);

				NodeList.Add(NewTest);
			}

			return NodeList;
		}

		/// <summary>
		/// Setup the account pool for tests that use it. Internally our derived classes (e.g.
		/// RunOrionTests) fill these with the usernames and passwords for all of our various
		/// test accounts. This is left here as an example.
		/// </summary>
		/// <returns></returns>
		protected virtual void SetupAccounts()
		{
			string Username = Globals.Params.ParseValue("username", null);
			string Password = Globals.Params.ParseValue("password", null);

			if (!string.IsNullOrEmpty(Username) && !string.IsNullOrEmpty(Password))
			{
				AccountPool.Instance.RegisterAccount(new EpicAccount(Username, Password));
			}
		}

		protected void SetupDevices(UnrealTargetPlatform DefaultPlatform, UnrealTestOptions Options)
		{

			Reservation.ReservationDetails = Options.JobDetails;

			DevicePool.Instance.SetLocalOptions(Options.TempDir, Options.Parallel > 1, Options.DeviceURL);
			DevicePool.Instance.AddLocalDevices(10);

			foreach (var DeviceWithParams in Options.DeviceList)
			{
				UnrealTargetPlatform Platform = DefaultPlatform;

				// see if one of the params is a platform
				foreach (var Param in DeviceWithParams.AllArguments)
				{
					if (Enum.TryParse(Param, true, out Platform))
					{
						break;
					}
				}

				DevicePool.Instance.AddDevices(Platform, DeviceWithParams.Argument);
			}
		}
	}
}
