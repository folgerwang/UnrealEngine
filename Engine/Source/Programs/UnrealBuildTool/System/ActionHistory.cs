// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.Serialization.Formatters.Binary;
using System.Security.Cryptography;
using System.Text;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Caches include dependency information to speed up preprocessing on subsequent runs.
	/// </summary>
	class ActionHistory
	{
		/// <summary>
		/// Version number to check
		/// </summary>
		const int CurrentVersion = 2;

		/// <summary>
		/// Size of each hash value
		/// </summary>
		const int HashLength = 16;

		/// <summary>
		/// Path to store the cache data to.
		/// </summary>
		FileReference Location;

		/// <summary>
		/// Base directory for output files. Any files under this directory will have their command lines stored in this object, otherwise the parent will be used.
		/// </summary>
		DirectoryReference BaseDirectory;

		/// <summary>
		/// The parent action history. Any files not under this base directory will use this.
		/// </summary>
		ActionHistory Parent;

		/// <summary>
		/// The command lines used to produce files, keyed by the absolute file paths.
		/// </summary>
		Dictionary<FileItem, byte[]> OutputItemToCommandLineHash = new Dictionary<FileItem, byte[]>();

		/// <summary>
		/// Whether the dependency cache is dirty and needs to be saved.
		/// </summary>
		bool bModified;

		/// <summary>
		/// Object to use for guarding access to the OutputItemToCommandLine dictionary
		/// </summary>
		object LockObject = new object();

		/// <summary>
		/// Static cache of all loaded action history files
		/// </summary>
		static Dictionary<FileReference, ActionHistory> LoadedFiles = new Dictionary<FileReference, ActionHistory>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Location">File to store this history in</param>
		/// <param name="BaseDirectory">Base directory for files to track</param>
		/// <param name="Parent">The parent action history</param>
		public ActionHistory(FileReference Location, DirectoryReference BaseDirectory, ActionHistory Parent)
		{
			this.Location = Location;
			this.BaseDirectory = BaseDirectory;
			this.Parent = Parent;

			if(FileReference.Exists(Location))
			{
				Load();
			}
		}

		/// <summary>
		/// Attempts to load this action history from disk
		/// </summary>
		void Load()
		{
			try
			{
				using(BinaryArchiveReader Reader = new BinaryArchiveReader(Location))
				{
					int Version = Reader.ReadInt();
					if(Version != CurrentVersion)
					{
						Log.TraceLog("Unable to read action history from {0}; version {1} vs current {2}", Location, Version, CurrentVersion);
						return;
					}

					OutputItemToCommandLineHash = Reader.ReadDictionary(() => Reader.ReadFileItem(), () => Reader.ReadFixedSizeByteArray(HashLength));
				}
			}
			catch(Exception Ex)
			{
				Log.TraceWarning("Unable to read {0}. See log for additional information.", Location);
				Log.TraceLog("{0}", ExceptionUtils.FormatExceptionDetails(Ex));
			}
		}

		/// <summary>
		/// Saves this action history to disk
		/// </summary>
		void Save()
		{
			DirectoryReference.CreateDirectory(Location.Directory);
			using(BinaryArchiveWriter Writer = new BinaryArchiveWriter(Location))
			{
				Writer.WriteInt(CurrentVersion);
				Writer.WriteDictionary(OutputItemToCommandLineHash, Key => Writer.WriteFileItem(Key), Value => Writer.WriteFixedSizeByteArray(Value));
			}
			bModified = false;
		}

		/// <summary>
		/// Computes the case-invariant hash for a string
		/// </summary>
		/// <param name="Text">The text to hash</param>
		/// <returns>Hash of the string</returns>
		static byte[] ComputeHash(string Text)
		{
			string InvariantText = Text.ToUpperInvariant();
			byte[] InvariantBytes = Encoding.Unicode.GetBytes(InvariantText);
			return new MD5CryptoServiceProvider().ComputeHash(InvariantBytes);
		}

		/// <summary>
		/// Compares two hashes for equality
		/// </summary>
		/// <param name="A">The first hash value</param>
		/// <param name="B">The second hash value</param>
		/// <returns>True if the hashes are equal</returns>
		static bool CompareHashes(byte[] A, byte[] B)
		{
			for(int Idx = 0; Idx < HashLength; Idx++)
			{
				if(A[Idx] != B[Idx])
				{
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// Gets the producing command line for the given file
		/// </summary>
		/// <param name="File">The output file to look for</param>
		/// <param name="CommandLine">Receives the command line used to produce this file</param>
		/// <returns>True if the output item exists</returns>
		public bool UpdateProducingCommandLine(FileItem File, string CommandLine)
		{
			if(File.Location.IsUnderDirectory(BaseDirectory) || Parent == null)
			{
				byte[] NewHash = ComputeHash(CommandLine);
				lock (LockObject)
				{
					byte[] CurrentHash;
					if(!OutputItemToCommandLineHash.TryGetValue(File, out CurrentHash) || !CompareHashes(CurrentHash, NewHash))
					{
						OutputItemToCommandLineHash[File] = NewHash;
						bModified = true;
						return true;
					}
					return false;
				}
			}
			else
			{
				return Parent.UpdateProducingCommandLine(File, CommandLine);
			}
		}

		/// <summary>
		/// Gets the location for the engine action history
		/// </summary>
		/// <param name="TargetName">Target name being built</param>
		/// <param name="Platform">The platform being built</param>
		/// <param name="TargetType">Type of the target being built</param>
		/// <returns>Path to the engine action history for this target</returns>
		public static FileReference GetEngineLocation(string TargetName, UnrealTargetPlatform Platform, TargetType TargetType)
		{
			string AppName;
			if(TargetType == TargetType.Program)
			{
				AppName = TargetName;
			}
			else
			{
				AppName = UEBuildTarget.GetAppNameForTargetType(TargetType);
			}

			return FileReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", Platform.ToString(), AppName, "ActionHistory.bin");
		}

		/// <summary>
		/// Gets the location of the project action history
		/// </summary>
		/// <param name="ProjectFile">Path to the project file</param>
		/// <param name="Platform">Platform being built</param>
		/// <param name="TargetName">Name of the target being built</param>
		/// <returns>Path to the project action history</returns>
		public static FileReference GetProjectLocation(FileReference ProjectFile, string TargetName, UnrealTargetPlatform Platform)
		{
			return FileReference.Combine(ProjectFile.Directory, "Intermediate", "Build", Platform.ToString(), TargetName, "ActionHistory.bin");
		}

		/// <summary>
		/// Creates a hierarchy of action history stores for a particular target
		/// </summary>
		/// <param name="ProjectFile">Project file for the target being built</param>
		/// <param name="TargetName">Name of the target</param>
		/// <param name="Platform">Platform being built</param>
		/// <param name="TargetType">The target type</param>
		/// <returns>Dependency cache hierarchy for the given project</returns>
		public static ActionHistory CreateHierarchy(FileReference ProjectFile, string TargetName, UnrealTargetPlatform Platform, TargetType TargetType)
		{
			ActionHistory History = null;

			if(ProjectFile == null || !UnrealBuildTool.IsEngineInstalled())
			{
				FileReference EngineCacheLocation = GetEngineLocation(TargetName, Platform, TargetType);
				History = FindOrAddHistory(EngineCacheLocation, UnrealBuildTool.EngineDirectory, History);
			}

			if(ProjectFile != null)
			{
				FileReference ProjectCacheLocation = GetProjectLocation(ProjectFile, TargetName, Platform);
				History = FindOrAddHistory(ProjectCacheLocation, ProjectFile.Directory, History);
			}

			return History;
		}

		/// <summary>
		/// Enumerates all the locations of action history files for the given target
		/// </summary>
		/// <param name="ProjectFile">Project file for the target being built</param>
		/// <param name="TargetName">Name of the target</param>
		/// <param name="Platform">Platform being built</param>
		/// <param name="TargetType">The target type</param>
		/// <returns>Dependency cache hierarchy for the given project</returns>
		public static IEnumerable<FileReference> GetFilesToClean(FileReference ProjectFile, string TargetName, UnrealTargetPlatform Platform, TargetType TargetType)
		{
			if(ProjectFile == null || !UnrealBuildTool.IsEngineInstalled())
			{
				yield return GetEngineLocation(TargetName, Platform, TargetType);
			}
			if(ProjectFile != null)
			{
				yield return GetProjectLocation(ProjectFile, TargetName, Platform);
			}
		}

		/// <summary>
		/// Reads a cache from the given location, or creates it with the given settings
		/// </summary>
		/// <param name="Location">File to store the cache</param>
		/// <param name="BaseDirectory">Base directory for files that this cache should store data for</param>
		/// <param name="Parent">The parent cache to use</param>
		/// <returns>Reference to a dependency cache with the given settings</returns>
		static ActionHistory FindOrAddHistory(FileReference Location, DirectoryReference BaseDirectory, ActionHistory Parent)
		{
			lock(LoadedFiles)
			{
				ActionHistory History;
				if(LoadedFiles.TryGetValue(Location, out History))
				{
					Debug.Assert(History.BaseDirectory == BaseDirectory);
					Debug.Assert(History.Parent == Parent);
				}
				else
				{
					History = new ActionHistory(Location, BaseDirectory, Parent);
					LoadedFiles.Add(Location, History);
				}
				return History;
			}
		}

		/// <summary>
		/// Save all the loaded action histories
		/// </summary>
		public static void SaveAll()
		{
			lock(LoadedFiles)
			{
				foreach(ActionHistory History in LoadedFiles.Values)
				{
					if(History.bModified)
					{
						History.Save();
					}
				}
			}
		}
	}
}
