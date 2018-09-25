// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using AutomationTool;
using System.Threading;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Reflection;
using ImageMagick;
using UnrealBuildTool;
using Tools.DotNETCommon;

namespace Gauntlet
{
	public class Globals
	{
		static Params InnerParams = new Params(Environment.GetCommandLineArgs());

		public static Params Params
		{
			get { return InnerParams; }
			set { InnerParams = value; }
		}

		static string InnerTempDir;
		static string InnerLogDir;
		static string InnerUE4RootDir;
		static object InnerLockObject = new object();
		static List<Action> InnerAbortHandlers;
		static List<Action> InnerPostAbortHandlers = new List<Action>();
		public static bool CancelSignalled { get; private set; }

		public static string TempDir
		{
			get
			{
				if (String.IsNullOrEmpty(InnerTempDir))
				{
					InnerTempDir = Path.Combine(Environment.CurrentDirectory, "GauntletTemp");
				}

				return InnerTempDir;
			}
			set
			{
				InnerTempDir = value;
			}
		}

		public static string LogDir
		{
			get
			{
				if (String.IsNullOrEmpty(InnerLogDir))
				{
					InnerLogDir = Path.Combine(TempDir, "Logs");
				}

				return InnerLogDir;
			}
			set
			{
				InnerLogDir = value;
			}
		}

		public static string UE4RootDir
		{
			get
			{
				if (String.IsNullOrEmpty(InnerUE4RootDir))
				{
					InnerUE4RootDir = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(Assembly.GetEntryAssembly().GetOriginalLocation()), "..", "..", ".."));
				}

				return InnerUE4RootDir;
			}

		}


		/// <summary>
		/// Acquired and released during the main Tick of the Gauntlet systems. Use this before touchung anything global scope from a 
		/// test thread.
		/// </summary>
		public static object MainLock
		{
			get { return InnerLockObject; }
		}

		/// <summary>
		/// Allows classes to register for notification of system-wide abort request. On an abort (e.g. ctrl+c) all handlers will 
		/// be called and then shutdown will continue
		/// </summary>
		public static List<Action> AbortHandlers
		{
			get
			{
				if (InnerAbortHandlers == null)
				{
					InnerAbortHandlers = new List<Action>();

					Console.CancelKeyPress += new ConsoleCancelEventHandler((obj, args) =>
					{
						CancelSignalled = true;

						// fire all abort handlers
						foreach (var Handler in AbortHandlers)
						{
							Handler();
						}

						// file all post-abort handlers
						foreach (var Handler in PostAbortHandlers)
						{
							Handler();
						}
					});
				}

				return InnerAbortHandlers;
			}
		}

		/// <summary>
		/// Allows classes to register for post-abort handlers. These are called after all abort handlers have returned
		/// so is the place to perform final cleanup.
		/// </summary>
		public static List<Action> PostAbortHandlers { get { return InnerPostAbortHandlers; } }


	}

	/// <summary>
	/// Enable/disable verbose logging
	/// </summary>
	public enum LogLevel
	{
		Normal,
		Verbose,
		VeryVerbose
	};

	/// <summary>
	/// Gauntlet Logging helper
	/// </summary>
	public class Log
	{
		public static LogLevel Level = LogLevel.Normal;

		public static bool IsVerbose
		{
			get
			{
				return Level >= LogLevel.Verbose;
			}
		}

		public static bool IsVeryVerbose
		{
			get
			{
				return Level >= LogLevel.VeryVerbose;
			}
		}

		static StreamWriter LogFile = null;

		static List<Action<string>> Callbacks;

		static int ECSuspendCount = 0;

		public static void AddCallback(Action<string> CB)
		{
			if (Callbacks == null)
			{
				Callbacks = new List<Action<string>>();
			}

			Callbacks.Add(CB);
		}

		public static void SaveToFile(string InPath)
		{
			int Attempts = 0;

			if (LogFile != null)
			{
				Console.WriteLine("Logfile already open for writing");
				return;
			}

			do
			{
				string Outpath = InPath;

				if (Attempts > 0)
				{
					string Ext = Path.GetExtension(InPath);
					string BaseName = Path.GetFileNameWithoutExtension(InPath);

					Outpath = Path.Combine(Path.GetDirectoryName(InPath), string.Format("{0}_{1}.{2}", BaseName, Attempts, Ext));
				}

				try
				{
					LogFile = new StreamWriter(Outpath);
				}
				catch (UnauthorizedAccessException Ex)
				{
					Console.WriteLine("Could not open {0} for writing. {1}", Outpath, Ex);
					Attempts++;
				}

			} while (LogFile == null && Attempts < 10);
		}

		static void Flush()
		{
			if (LogFile != null)
			{
				LogFile.Flush();
			}
		}

		public static void SuspendECErrorParsing()
		{
			if (ECSuspendCount++ == 0)
			{
				OutputMessage("<-- Suspend Log Parsing -->");
			}
		}

		public static void ResumeECErrorParsing()
		{
			if (--ECSuspendCount == 0)
			{
				OutputMessage("<-- Resume Log Parsing -->");
			}
		}

		/// <summary>
		/// Santi
		/// </summary>
		/// <param name="Input"></param>
		/// <param name="Sanitize"></param>
		/// <returns></returns>
		static private string SanitizeInput(string Input, string[] Sanitize)
		{
			foreach (string San in Sanitize)
			{
				string CleanSan = San;
				if (San.Length > 1)
				{
					CleanSan.Insert(San.Length - 1, "-");
				}
				Input = Regex.Replace(Input, "Warning", CleanSan, RegexOptions.IgnoreCase);
			}

			return Input;
		}

		/// <summary>
		/// Outputs the message to the console with an optional prefix and sanitization. Sanitizing
		/// allows errors and exceptions to be passed through to logs without triggering CIS warnings
		/// about out log
		/// </summary>
		/// <param name="Message"></param>
		/// <param name="Prefix"></param>
		/// <param name="Sanitize"></param>
		/// <returns></returns>
		static private void OutputMessage(string Message, string Prefix="", bool Sanitize=true)
		{
			// EC detects error statements in the log as a failure. Need to investigate best way of 
			// reporting errors, but not errors we've handled from tools.
			// Probably have Log.Error which does not sanitize?
			if (Sanitize)
			{
				string[] Triggers = { "Warning:", "Error:", "Exception:" };

				foreach (string Trigger in Triggers)
				{
					if (Message.IndexOf(Trigger, StringComparison.OrdinalIgnoreCase) != -1)
					{
						string SafeTrigger = Regex.Replace(Trigger, "i", "1", RegexOptions.IgnoreCase);
						SafeTrigger = Regex.Replace(SafeTrigger, "o", "0", RegexOptions.IgnoreCase);

						Message = Regex.Replace(Message, Trigger, SafeTrigger, RegexOptions.IgnoreCase);
					}
				}		
			}

			if (string.IsNullOrEmpty(Prefix) == false)
			{
				Message = Prefix + ": " + Message;
			}

			// TODO - Remove all Gauntlet logging and switch to UBT log?
			CommandUtils.LogInformation(Message);

			if (LogFile != null)
			{
				LogFile.WriteLine(Message);
			}

			if (Callbacks != null)
			{
				Callbacks.ForEach(A => A(Message));
			}
		}	

		static public void Verbose(string Format, params object[] Args)
		{
			if (IsVerbose)
			{
				Verbose(string.Format(Format, Args));
			}
		}

		static public void Verbose(string Message)
		{
			if (IsVerbose)
			{
				OutputMessage(Message);
			}
		}

		static public void VeryVerbose(string Format, params object[] Args)
		{
			if (IsVeryVerbose)
			{
				VeryVerbose(string.Format(Format, Args));
			}
		}

		static public void VeryVerbose(string Message)
		{
			if (IsVeryVerbose)
			{
				OutputMessage(Message);
			}
		}

		static public void Info(string Format, params object[] Args)
		{
			Info(string.Format(Format, Args));
		}

		static public void Info(string Message)
		{
			OutputMessage(Message);
		}

		static public void Warning(string Format, params object[] Args)
		{
			Warning(string.Format(Format, Args));
		}

		static public void Warning(string Message)
		{
			OutputMessage(Message, "Warning");
		}
		static public void Error(string Format, params object[] Args)
		{
			Error(string.Format(Format, Args));
		}
		static public void Error(string Message)
		{		
			OutputMessage(Message, "Error", false);
		}
	}

	/*
	 * Helper class that can be used with a using() statement to emit log entries that prevent EC triggering on Error/Warning statements
	*/
	public class ScopedSuspendECErrorParsing : IDisposable
	{
		public ScopedSuspendECErrorParsing()
		{
			Log.SuspendECErrorParsing();
		}

		~ScopedSuspendECErrorParsing()
		{
			Dispose(false);
		}

		#region IDisposable Support
		private bool disposedValue = false; // To detect redundant calls

		protected virtual void Dispose(bool disposing)
		{
			if (!disposedValue)
			{
				Log.ResumeECErrorParsing();

				disposedValue = true;
			}
		}

		// This code added to correctly implement the disposable pattern.
		public void Dispose()
		{
			Dispose(true);
		}
		#endregion
	}

	namespace Utils
	{
		
		public class TestConstructor
		{

			/// <summary>
			/// Helper function that returns the type of an object based on namespace and name
			/// </summary>
			/// <param name="Namespace"></param>
			/// <param name="TestName"></param>
			/// <returns></returns>
			private static Type GetTypeForTest(string TestName, IEnumerable<string> Namespaces)
			{
				var SearchAssemblies = AppDomain.CurrentDomain.GetAssemblies();

				// turn foo into [n1.foo, n2.foo, foo]
				IEnumerable<string> FullNames;

				if (Namespaces != null)
				{
					FullNames = Namespaces.Select(N => N + "." + TestName);
				}
				else
				{
					FullNames = new[] { TestName };
				}

				Log.VeryVerbose("Will search {0} for test {1}", string.Join(" ", FullNames), TestName);
				
				// find all types from loaded assemblies that implement testnode
					List < Type> CandidateTypes = new List<Type>();

				foreach (var Assembly in AppDomain.CurrentDomain.GetAssemblies())
				{
					foreach (var Type in Assembly.GetTypes())
					{
						if (typeof(ITestNode).IsAssignableFrom(Type))
						{
							CandidateTypes.Add(Type);
						}
					}
				}

				Log.VeryVerbose("Possible candidates for {0}: {1}", TestName, string.Join(" ", CandidateTypes));

				// check our expanded names.. need to search in namespace order
				foreach (string UserTypeName in FullNames)
				{
					// Even tho the user might have specified N1.Foo it still might be Other.N1.Foo so only
					// compare based on the number of namespaces that were specified.
					foreach (var Type in CandidateTypes)
					{
						string[] UserNameComponents = UserTypeName.Split('.');
						string[] TypeNameComponents = Type.FullName.Split('.');

						int MissingUserComponents = TypeNameComponents.Length - UserNameComponents.Length;

						if (MissingUserComponents > 0)
						{
							// 
							TypeNameComponents = TypeNameComponents.Skip(MissingUserComponents).ToArray();
						}

						var Difference = TypeNameComponents.Except(UserNameComponents, StringComparer.OrdinalIgnoreCase);

						if (Difference.Count() == 0)
						{
							Log.VeryVerbose("Considering {0} as best match for {1}", Type, TestName);
							return Type;
						}
					}
				}


				throw new AutomationException("Unable to find type {0} in assemblies. Namespaces= {1}.", TestName, Namespaces);
			}


			/// <summary>
			/// Helper function that returns all types within the specified namespace that are or derive from
			/// the specified type
			/// </summary>
			/// <param name="OfType"></param>
			/// <param name="TestName"></param>
			/// <returns></returns>
			public static IEnumerable<Type> GetTypesInNamespaces<BaseType>(IEnumerable<string> Namespaces)
				where BaseType : class
			{
				var AllTypes = Assembly.GetExecutingAssembly().GetTypes().Where(T => typeof(BaseType).IsAssignableFrom(T));

				if (Namespaces.Count() > 0)
				{
					AllTypes = AllTypes.Where(T => T.IsAbstract == false && Namespaces.Contains(T.Namespace.ToString()));
				}

				return AllTypes;
			}

			/// <summary>
			/// Constructs by name a new test of type "TestType" that takes no construction parameters
			/// </summary>
			/// <typeparam name="TestType"></typeparam>
			/// <typeparam name="ContextType"></typeparam>
			/// <param name="Namespace"></param>
			/// <param name="TestName"></param>
			/// <param name="Context"></param>
			/// <returns></returns>
			public static TestType ConstructTest<TestType, ParamType>(string TestName, ParamType Arg, IEnumerable<string> Namespaces)
					where TestType : class
			{
				Type NodeType = GetTypeForTest(TestName, Namespaces);

				ConstructorInfo NodeConstructor = null;
				TestType NewNode = null;

				if (Arg != null)
				{
					NodeConstructor = NodeType.GetConstructor(new Type[] { Arg.GetType() });

					if (NodeConstructor != null)
					{
						NewNode = NodeConstructor.Invoke(new object[] { Arg }) as TestType;
					}
				}

				if (NodeConstructor == null)
				{
					NodeConstructor = NodeType.GetConstructor(Type.EmptyTypes);
					if (NodeConstructor != null)
					{
						NewNode = NodeConstructor.Invoke(null) as TestType;
					}
				}

				if (NodeConstructor == null)
				{
					throw new AutomationException("Unable to find constructor for type {0} with our without params", typeof(TestType));
				}

				if (NewNode == null)
				{
					throw new AutomationException("Unable to construct node of type {0}", typeof(TestType));
				}

				return NewNode;
			}


			/// <summary>
			/// Constructs by name a list of tests of type "TestType" that take no construction params
			/// </summary>
			/// <typeparam name="TestType"></typeparam>
			/// <param name="Namespace"></param>
			/// <param name="TestNames"></param>
			/// <returns></returns>
			public static IEnumerable<TestType> ConstructTests<TestType, ParamType>(IEnumerable<string> TestNames, ParamType Arg, IEnumerable<string> Namespaces)
					where TestType : class
			{
				List<TestType> Tests = new List<TestType>();

				foreach (var Name in TestNames)
				{
					Tests.Add(ConstructTest<TestType, ParamType>(Name, Arg, Namespaces));
				}

				return Tests;
			}

			/// <summary>
			/// Constructs by name a list of tests of type "TestType" that take no construction params
			/// </summary>
			/// <typeparam name="TestType"></typeparam>
			/// <param name="Namespace"></param>
			/// <param name="TestNames"></param>
			/// <returns></returns>
			public static IEnumerable<TestType> ConstructTests<TestType>(IEnumerable<string> TestNames, IEnumerable<string> Namespaces)
					where TestType : class
			{
				List<TestType> Tests = new List<TestType>();

				foreach (var Name in TestNames)
				{
					Tests.Add(ConstructTest<TestType, object>(Name, null, Namespaces));
				}

				return Tests;
			}

			/// <summary>
			/// Constructs by name a list of tests of type "TestType" that take no construction params
			/// </summary>
			/// <typeparam name="TestType"></typeparam>
			/// <param name="Namespace"></param>
			/// <param name="TestNames"></param>
			/// <returns></returns>
			public static IEnumerable<string> GetTestNamesByGroup<TestType>(string Group, IEnumerable<string> Namespaces)
					where TestType : class
			{
				// Get all types in these namespaces
				IEnumerable<Type> TestTypes = GetTypesInNamespaces<TestType>(Namespaces);

				IEnumerable<string> SortedTests = new string[0];

				// If no group, just return a sorted list
				if (string.IsNullOrEmpty(Group))
				{
					SortedTests = TestTypes.Select(T => T.FullName).OrderBy(S => S);
				}
				else
				{
					Dictionary<string, int> TypesToPriority = new Dictionary<string, int>();

					// Find ones that have a group attribute
					foreach (Type T in TestTypes)
					{
						foreach (object Attrib in T.GetCustomAttributes(true))
						{
							TestGroup TestAttrib = Attrib as TestGroup;

							// Store the type name as a key with the priority as a value
							if (TestAttrib != null && Group.Equals(TestAttrib.GroupName, StringComparison.OrdinalIgnoreCase))
							{
								TypesToPriority[T.FullName] = TestAttrib.Priority;
							}
						}
					}

					// sort by priority then name
					SortedTests = TypesToPriority.Keys.OrderBy(K => TypesToPriority[K]).ThenBy(K => K);
				}
			
				return SortedTests;
			}
		
		}

		public static class InterfaceHelpers
		{

			public static IEnumerable<InterfaceType> FindImplementations<InterfaceType>()
				where InterfaceType : class
			{
				var AllTypes = Assembly.GetExecutingAssembly().GetTypes().Where(T => typeof(InterfaceType).IsAssignableFrom(T));

				List<InterfaceType> ConstructedTypes = new List<InterfaceType>();

				foreach (Type FoundType in AllTypes)
				{
					ConstructorInfo TypeConstructor = FoundType.GetConstructor(Type.EmptyTypes);

					if (TypeConstructor != null)
					{
						InterfaceType NewInstance = TypeConstructor.Invoke(null) as InterfaceType;

						ConstructedTypes.Add(NewInstance);
					}
				}
				
				return ConstructedTypes;
			}

		}


		public static class SystemHelpers
		{
			/// <summary>
			/// Options for copying directories
			/// </summary>
			public enum CopyOptions
			{
				Copy		= (1 << 0),		// Normal copy & combine/overwrite
				Mirror		= (1 << 1),		// copy + remove files from dest if not in src
				Default = Copy
			}
			
			/// <summary>
			/// Convenience function that removes some of the more esoteric options
			/// </summary>
			/// <param name="SourceDirPath"></param>
			/// <param name="DestDirPath"></param>
			/// <param name="Options"></param>
			/// <param name="RetryCount"></param>
			public static void CopyDirectory(string SourceDirPath, string DestDirPath, CopyOptions Options = CopyOptions.Default, int RetryCount = 5)
			{
				CopyDirectory(SourceDirPath, DestDirPath, Options, delegate (string s) { return s; }, RetryCount);
			}

			/// <summary>
			/// Copies src to dest by comparing files sizes and time stamps and only copying files that are different in src. Basically a more flexible
			/// robocopy
			/// </summary>
			/// <param name="SourcePath"></param>
			/// <param name="DestPath"></param>
			/// <param name="Verbose"></param>
			public static void CopyDirectory(string SourceDirPath, string DestDirPath, CopyOptions Options, Func<string, string> Transform, int RetryCount = 5)
			{
				DateTime StartTime = DateTime.Now;
				
				DirectoryInfo SourceDir = new DirectoryInfo(SourceDirPath);
				DirectoryInfo DestDir = new DirectoryInfo(DestDirPath);

				if (DestDir.Exists == false)
				{
					DestDir = Directory.CreateDirectory(DestDir.FullName);
				}

				System.IO.FileInfo[] SourceFiles = SourceDir.GetFiles("*", SearchOption.AllDirectories);
				System.IO.FileInfo[] DestFiles = DestDir.GetFiles("*", SearchOption.AllDirectories);

				// Convert dest into a map of relative paths to absolute
				Dictionary<string, System.IO.FileInfo> DestStructure = new Dictionary<string, System.IO.FileInfo>();

				foreach (FileInfo Info in DestFiles)
				{
					string RelativePath = Info.FullName.Replace(DestDir.FullName, "");

					// remove leading seperator
					if (RelativePath.First() == Path.DirectorySeparatorChar)
					{
						RelativePath = RelativePath.Substring(1);
					}

					DestStructure[RelativePath] = Info;
				}

				// List of relative-path files to copy to dest
				List<string> CopyList = new List<string>();

				// List of relative path files in dest to delete
				List<string> DeletionList = new List<string>();

				foreach (FileInfo SourceInfo in SourceFiles)
				{
					string SourceFilePath = SourceInfo.FullName.Replace(SourceDir.FullName, "");

					// remove leading seperator
					if (SourceFilePath.First() == Path.DirectorySeparatorChar)
					{
						SourceFilePath = SourceFilePath.Substring(1);
					}

					string DestFilePath = Transform(SourceFilePath);

					if (DestStructure.ContainsKey(DestFilePath) == false)
					{
						// No copy in dest, add it to the list
						CopyList.Add(SourceFilePath);
					}
					else
					{
						// Check the file is the same version
						FileInfo DestInfo = DestStructure[DestFilePath];

						// Difference in ticks. Even though we set the dest to the src there still appears to be minute
						// differences in ticks. 1ms is 10k ticks...
						Int64 TimeDelta = Math.Abs(DestInfo.LastWriteTime.Ticks - SourceInfo.LastWriteTime.Ticks);
						Int64 Threshhold = 100000;

						if (DestInfo.Length != SourceInfo.Length ||
							TimeDelta > Threshhold)
						{
							CopyList.Add(SourceFilePath);
						}

						// Remove it from the map
						DestStructure.Remove(DestFilePath);
					}
				}

				// If set to mirror, delete all the files that were not in source
				if ((Options & CopyOptions.Mirror) == CopyOptions.Mirror)
				{
					// Now go through the remaining map items and delete them
					foreach (var Pair in DestStructure)
					{
						DeletionList.Add(Pair.Key);
					}

					foreach (string RelativePath in DeletionList)
					{
						FileInfo DestInfo = new FileInfo(Path.Combine(DestDir.FullName, RelativePath));

						Log.Verbose("Deleting extra file {0}", DestInfo.FullName);

						try
						{
							DestInfo.Delete();
						}
						catch (Exception Ex)
						{
							Log.Warning("Failed to delete file {0}. {1}", DestInfo.FullName, Ex);
						}
					}

					// delete empty directories
					DirectoryInfo DestDirInfo = new DirectoryInfo(DestDirPath);

					DirectoryInfo[] AllSubDirs = DestDirInfo.GetDirectories("*", SearchOption.AllDirectories);

					foreach (DirectoryInfo SubDir in AllSubDirs)
					{
						try
						{
							if (SubDir.GetFiles().Length == 0 && SubDir.GetDirectories().Length == 0)
							{
								Log.Verbose("Deleting empty dir {0}", SubDir.FullName);

								SubDir.Delete(true);
							}
						}
						catch (Exception Ex)
						{
							// handle the case where a file is locked
							Log.Info("Failed to delete directory {0}. {1}", SubDir.FullName, Ex);
						}
					}
				}

				CancellationTokenSource CTS = new CancellationTokenSource();

				// todo - make param..
				var POptions = new ParallelOptions { MaxDegreeOfParallelism = 1, CancellationToken = CTS.Token  };

				// install a cancel handler so we can stop parallel-for gracefully
				Action CancelHandler = delegate()
				{
					CTS.Cancel();
				};

				Globals.AbortHandlers.Add(CancelHandler);

				// now do the work
				Parallel.ForEach(CopyList, POptions, RelativePath =>
				{ 
					// ensure path exists
					string DestPath = Path.Combine(DestDir.FullName, RelativePath);

					if (Transform != null)
					{
						DestPath = Transform(DestPath);
					}

					FileInfo DestInfo = new FileInfo(DestPath);
					FileInfo SrcInfo = new FileInfo(Path.Combine(SourceDir.FullName, RelativePath));

					// ensure directory exists
					DestInfo.Directory.Create();

					string DestFile = DestInfo.FullName;

					if (Transform != null)
					{
						DestFile = Transform(DestFile);
					}

					int Tries = 0;
					bool Copied = false;

					do
					{
						try
						{
							Log.Verbose("Copying to {0}", DestFile);

							SrcInfo.CopyTo(DestFile, true);

							// Clear and read-only attrib and set last write time
							FileInfo DestFileInfo = new FileInfo(DestFile);
							DestFileInfo.IsReadOnly = false;
							DestFileInfo.LastWriteTime = SrcInfo.LastWriteTime;
							Copied = true;
						}
						catch (Exception ex)
						{
							if (Tries++ < RetryCount)
							{
								Log.Info("Copy to {0} failed, retrying {1} of {2} in 30 secs..", DestFile, Tries, RetryCount);
								// todo - make param..
								Thread.Sleep(30000);
							}
							else
							{
								Log.Error("File Copy failed with {0}.", ex.Message);
								throw new Exception(string.Format("File Copy failed with {0}.", ex.Message));
							}
						}
					} while (Copied == false);
				});

				TimeSpan Duration = DateTime.Now - StartTime;
				if (Duration.TotalSeconds > 10)
				{
					Log.Verbose("Copied Directory in {0}", Duration.ToString(@"mm\m\:ss\s"));
				}

				// remove cancel handler
				Globals.AbortHandlers.Remove(CancelHandler);
			}

			public static string MakePathRelative(string FullPath, string BasePath)
			{
				// base path must be correctly formed!
				if (BasePath.Last() != Path.DirectorySeparatorChar)
				{
					BasePath += Path.DirectorySeparatorChar;
				}					

				var ReferenceUri = new Uri(BasePath);
				var FullUri = new Uri(FullPath);

				return ReferenceUri.MakeRelativeUri(FullUri).ToString();
			}

			public static string CorrectDirectorySeparators(string InPath)
			{
				if (Path.DirectorySeparatorChar == '/')
				{
					return InPath.Replace('\\', Path.DirectorySeparatorChar);
				}
				else
				{
					return InPath.Replace('/', Path.DirectorySeparatorChar);
				}
			}

			public static bool ApplicationExists(string InPath)
			{
				if (File.Exists(InPath))
				{
					return true;
				}

				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					if (InPath.EndsWith(".app", StringComparison.OrdinalIgnoreCase))
					{
						return Directory.Exists(InPath);
					}
				}

				return false;
			}
		}

		public class Image
		{
			protected static IEnumerable<FileInfo> GetSupportedFilesAtPath(string InPath)
			{
				string[] Extensions = new[] { ".jpg", ".jpeg", ".png", ".bmp" };

				DirectoryInfo Di = new DirectoryInfo(InPath);

				var Files = Di.GetFiles().Where(f => Extensions.Contains(f.Extension.ToLower()));

				return Files;
			}


			public static bool SaveImagesAsGif(IEnumerable<string> FilePaths, string OutPath, int Delay=100)
			{
				Log.Verbose("Turning {0} files into {1}", FilePaths.Count(), OutPath);
				try
				{
					using (MagickImageCollection Collection = new MagickImageCollection())
					{
						foreach (string File in FilePaths)
						{
							int Index = Collection.Count();
							Collection.Add(File);
							Collection[Index].AnimationDelay = Delay;
						}

						// Optionally reduce colors
						/*QuantizeSettings settings = new QuantizeSettings();
						settings.Colors = 256;
						Collection.Quantize(settings);*/

						foreach (MagickImage image in Collection)
						{
							image.Resize(640, 0);
						}

						// Optionally optimize the images (images should have the same size).
						Collection.Optimize();

						// Save gif
						Collection.Write(OutPath);

						Log.Verbose("Saved {0}", OutPath);
					}
				}
				catch (System.Exception Ex)
				{
					Log.Warning("SaveAsGif failed: {0}", Ex);
					return false;
				}

				return true;
			}

			public static bool SaveImagesAsGif(string InDirectory, string OutPath)
			{
				string[] Extensions = new[] { ".jpg", ".jpeg", ".png", ".bmp" };

				DirectoryInfo Di = new DirectoryInfo(InDirectory);

				var Files = GetSupportedFilesAtPath(InDirectory);

				// sort by creation time
				Files = Files.OrderBy(F => F.CreationTimeUtc);

				if (Files.Count() == 0)
				{
					Log.Warning("Could not find files at {0} to Gif-ify", InDirectory);
					return false;
				}

				return SaveImagesAsGif(Files.Select(F => F.FullName), OutPath);
			}

			public static bool ResizeImages(string InDirectory, int MaxWidth)
			{
				var Files = GetSupportedFilesAtPath(InDirectory);

				if (Files.Count() == 0)
				{
					Log.Warning("Could not find files at {0} to resize", InDirectory);
					return false;
				}

				Log.Verbose("Reizing {0} files at {1} to have a max width of {2}", Files.Count(), InDirectory, MaxWidth);

				try
				{
					foreach (FileInfo File in Files)
					{
						using (MagickImage Image = new MagickImage(File.FullName))
						{
							if (Image.Width > MaxWidth)
							{
								Image.Resize(MaxWidth, 0);
								Image.Write(File);
							}
						}
					}				
				}
				catch (System.Exception Ex)
				{
					Log.Warning("ResizeImages failed: {0}", Ex);
					return false;
				}

				return true;
			}


			public static bool ConvertImages(string InDirectory, string OutDirectory, string OutExtension, bool DeleteOriginals)
			{
				var Files = GetSupportedFilesAtPath(InDirectory);

				if (Files.Count() == 0)
				{
					Log.Warning("Could not find files at {0} to resize", InDirectory);
					return false;
				}

				Log.Verbose("Converting {0} files to {1}", Files.Count(), OutExtension);

				try
				{
					foreach (FileInfo File in Files)
					{
						using (MagickImage Image = new MagickImage(File.FullName))
						{
							string OutFile = Path.Combine(OutDirectory, File.Name);
							OutFile = Path.ChangeExtension(OutFile, OutExtension);	
							Image.Write(OutFile);
						}
					}

					if (DeleteOriginals)
					{
						Files.ToList().ForEach(F => F.Delete());
					}
				}
				catch (System.Exception Ex)
				{
					Log.Warning("ConvertImages failed: {0}", Ex);
					return false;
				}

				return true;
			}
		}
	}

	public static class RegexUtil
	{
		public static bool MatchAndApplyGroups(string InContent, string RegEx, Action<string[]> InFunc)
		{
			Match M = Regex.Match(InContent, RegEx, RegexOptions.IgnoreCase);

			IEnumerable<string> StringMatches = null;

			if (M.Success)
			{
				StringMatches = M.Groups.Cast<Capture>().Select(G => G.ToString());
				InFunc(StringMatches.ToArray());
			}

			return M.Success;
		}
	}
}
