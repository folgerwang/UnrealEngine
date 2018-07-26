// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;
using System.Runtime.Serialization;
using System.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// For C++ source file items, this structure is used to cache data that will be used for include dependency scanning
	/// </summary>
	[Serializable]
	class CppIncludePaths : ISerializable
	{
		/// <summary>
		/// Ordered list of include paths for the module
		/// </summary>
		public HashSet<DirectoryReference> UserIncludePaths;

		/// <summary>
		/// The include paths where changes to contained files won't cause dependent C++ source files to
		/// be recompiled, unless BuildConfiguration.bCheckSystemHeadersForModification==true.
		/// </summary>
		public HashSet<DirectoryReference> SystemIncludePaths;

		/// <summary>
		/// Whether headers in system paths should be checked for modification when determining outdated actions.
		/// </summary>
		public bool bCheckSystemHeadersForModification;

		/// <summary>
		/// Contains a mapping from filename to the full path of the header in this environment.  This is used to optimized include path lookups at runtime for any given single module.
		/// </summary>
		public Dictionary<string, FileItem> IncludeFileSearchDictionary = new Dictionary<string, FileItem>();

		/// <summary>
		/// Construct an empty set of include paths
		/// </summary>
		public CppIncludePaths()
		{
			UserIncludePaths = new HashSet<DirectoryReference>();
			SystemIncludePaths = new HashSet<DirectoryReference>();
		}

		/// <summary>
		/// Copy constructor
		/// </summary>
		/// <param name="Other">Duplicate another instance's settings</param>
		public CppIncludePaths(CppIncludePaths Other)
		{
			UserIncludePaths = new HashSet<DirectoryReference>(Other.UserIncludePaths);
			SystemIncludePaths = new HashSet<DirectoryReference>(Other.SystemIncludePaths);
			bCheckSystemHeadersForModification = Other.bCheckSystemHeadersForModification;
		}

		/// <summary>
		/// Deserialize the include paths from the given context
		/// </summary>
		/// <param name="Info">Serialization info</param>
		/// <param name="Context">Serialization context</param>
		public CppIncludePaths(SerializationInfo Info, StreamingContext Context)
		{
			UserIncludePaths = new HashSet<DirectoryReference>((DirectoryReference[])Info.GetValue("ip", typeof(DirectoryReference[])));
			SystemIncludePaths = new HashSet<DirectoryReference>((DirectoryReference[])Info.GetValue("sp", typeof(DirectoryReference[])));
			bCheckSystemHeadersForModification = Info.GetBoolean("cs");
		}

		/// <summary>
		/// Serialize this instance
		/// </summary>
		/// <param name="Info">Serialization info</param>
		/// <param name="Context">Serialization context</param>
		public void GetObjectData(SerializationInfo Info, StreamingContext Context)
		{
			Info.AddValue("ip", UserIncludePaths.ToArray());
			Info.AddValue("sp", SystemIncludePaths.ToArray());
			Info.AddValue("cs", bCheckSystemHeadersForModification);
		}

		/// <summary>
		/// Given a C++ source file, returns a list of include paths we should search to resolve #includes for this path
		/// </summary>
		/// <param name="SourceFile">C++ source file we're going to check #includes for.</param>
		/// <returns>Ordered list of paths to search</returns>
		public List<DirectoryReference> GetPathsToSearch(FileReference SourceFile)
		{
			List<DirectoryReference> IncludePathsToSearch = new List<DirectoryReference>();
			IncludePathsToSearch.Add(SourceFile.Directory);
			IncludePathsToSearch.AddRange(UserIncludePaths);
			if (bCheckSystemHeadersForModification)
			{
				IncludePathsToSearch.AddRange(SystemIncludePaths);
			}
			return IncludePathsToSearch;
		}
	}

	class CPPHeaders
	{
		/// <summary>
		/// The project that we're caching headers for
		/// </summary>
		public FileReference ProjectFile;

		/// <summary>
		/// Path to the dependency cache for this target
		/// </summary>
		public FileReference DependencyCacheFile;

		/// <summary>
		/// Contains a cache of include dependencies (direct and indirect), one for each target we're building.
		/// </summary>
		public DependencyCache IncludeDependencyCache = null;

		/// <summary>
		/// Contains a cache of include dependencies (direct and indirect), one for each target we're building.
		/// </summary>
		public FlatCPPIncludeDependencyCache FlatCPPIncludeDependencyCache = null;

		/// <summary>
		/// 
		/// </summary>
		public static int TotalFindIncludedFileCalls = 0;

		/// <summary>
		/// 
		/// </summary>
		public static int IncludePathSearchAttempts = 0;

		/// <summary>
		/// A cache of the list of other files that are directly or indirectly included by a C++ file.
		/// </summary>
		Dictionary<FileItem, List<FileItem>> ExhaustiveIncludedFilesMap = new Dictionary<FileItem, List<FileItem>>();

		/// <summary>
		/// A cache of all files included by a C++ file, but only has files that we knew about from a previous session, loaded from a cache at startup
		/// </summary>
		Dictionary<FileItem, List<FileItem>> OnlyCachedIncludedFilesMap = new Dictionary<FileItem, List<FileItem>>();

		/// <summary>
		/// 
		/// </summary>
		public bool bUseUBTMakefiles;

		/// <summary>
		/// 
		/// </summary>
		public bool bUseFlatCPPIncludeDependencyCache;
		
		/// <summary>
		/// 
		/// </summary>
		public bool bUseIncludeDependencyResolveCache;

		/// <summary>
		/// 
		/// </summary>
		public bool bTestIncludeDependencyResolveCache;

		public CPPHeaders(FileReference ProjectFile, FileReference DependencyCacheFile, bool bUseUBTMakefiles, bool bUseFlatCPPIncludeDependencyCache, bool bUseIncludeDependencyResolveCache, bool bTestIncludeDependencyResolveCache)
		{
			this.ProjectFile = ProjectFile;
			this.DependencyCacheFile = DependencyCacheFile;
			this.bUseUBTMakefiles = bUseUBTMakefiles;
			this.bUseFlatCPPIncludeDependencyCache = bUseFlatCPPIncludeDependencyCache;
			this.bUseIncludeDependencyResolveCache = bUseIncludeDependencyResolveCache;
			this.bTestIncludeDependencyResolveCache = bTestIncludeDependencyResolveCache;
		}

		/// <summary>
		/// Finds the header file that is referred to by a partial include filename.
		/// </summary>
		/// <param name="FromFile">The file containing the include directory</param>
		/// <param name="RelativeIncludePath">path relative to the project</param>
		/// <param name="IncludePaths">Include paths to search</param>
		public static FileItem FindIncludedFile(FileReference FromFile, string RelativeIncludePath, CppIncludePaths IncludePaths)
		{
			FileItem Result = null;

			++TotalFindIncludedFileCalls;

			// Only search for the include file if the result hasn't been cached.
			string InvariantPath = RelativeIncludePath.ToLowerInvariant();
			if (!IncludePaths.IncludeFileSearchDictionary.TryGetValue(InvariantPath, out Result))
			{
				int SearchAttempts = 0;
				if (Path.IsPathRooted(RelativeIncludePath))
				{
					FileReference Reference = FileReference.Combine(UnrealBuildTool.EngineSourceDirectory, RelativeIncludePath);
					if (DirectoryLookupCache.FileExists(Reference))
					{
						Result = FileItem.GetItemByFileReference(Reference);
					}
					++SearchAttempts;
				}
				else
				{
					// Find the first include path that the included file exists in.
					List<DirectoryReference> IncludePathsToSearch = IncludePaths.GetPathsToSearch(FromFile);
					foreach (DirectoryReference IncludePath in IncludePathsToSearch)
					{
						++SearchAttempts;
						FileReference FullFilePath;
						try
						{
							FullFilePath = FileReference.Combine(IncludePath, RelativeIncludePath);
						}
						catch (ArgumentException Exception)
						{
							throw new BuildException(Exception, "Failed to combine null or invalid include paths.");
						}
						if (DirectoryLookupCache.FileExists(FullFilePath))
						{
							Result = FileItem.GetItemByFileReference(FullFilePath);
							break;
						}
					}
				}

				IncludePathSearchAttempts += SearchAttempts;

				if (UnrealBuildTool.bPrintPerformanceInfo)
				{
					// More than two search attempts indicates:
					//		- Include path was not relative to the directory that the including file was in
					//		- Include path was not relative to the project's base
					if (SearchAttempts > 2)
					{
						Log.TraceVerbose("   Cache miss: " + RelativeIncludePath + " found after " + SearchAttempts.ToString() + " attempts: " + (Result != null ? Result.AbsolutePath : "NOT FOUND!"));
					}
				}

				// Cache the result of the include path search.
				IncludePaths.IncludeFileSearchDictionary.Add(InvariantPath, Result);
			}

			// @todo ubtmake: The old UBT tried to skip 'external' (STABLE) headers here.  But it didn't work.  We might want to do this though!  Skip system headers and source/thirdparty headers!

			if (Result != null)
			{
				Log.TraceVerbose("Resolved included file \"{0}\" to: {1}", RelativeIncludePath, Result.AbsolutePath);
			}
			else
			{
				Log.TraceVerbose("Couldn't resolve included file \"{0}\"", RelativeIncludePath);
			}

			return Result;
		}

		public List<FileItem> FindAndCacheAllIncludedFiles(FileItem SourceFile, CppIncludePaths IncludePaths, bool bOnlyCachedDependencies)
		{
			List<FileItem> Result = null;

			if (IncludePaths.IncludeFileSearchDictionary == null)
			{
				IncludePaths.IncludeFileSearchDictionary = new Dictionary<string, FileItem>();
			}

			if (bOnlyCachedDependencies && bUseFlatCPPIncludeDependencyCache)
			{
				Result = FlatCPPIncludeDependencyCache.GetDependenciesForFile(SourceFile.Location);
				if (Result == null)
				{
					// Nothing cached for this file!  It is new to us.  This is the expected flow when our CPPIncludeDepencencyCache is missing.
				}
			}
			else
			{
				// If we're doing an exhaustive include scan, make sure that we have our include dependency cache loaded and ready
				if (!bOnlyCachedDependencies)
				{
					if (IncludeDependencyCache == null)
					{
						IncludeDependencyCache = DependencyCache.Create(DependencyCacheFile);
					}
				}

				// Get the headers
				Result = FindAndCacheIncludedFiles(SourceFile, IncludePaths, bOnlyCachedDependencies);

				// Update cache
				if (bUseFlatCPPIncludeDependencyCache && !bOnlyCachedDependencies)
				{
					List<FileReference> Dependencies = new List<FileReference>();
					foreach (FileItem IncludedFile in Result)
					{
						Dependencies.Add(IncludedFile.Location);
					}
					FlatCPPIncludeDependencyCache.SetDependenciesForFile(SourceFile.Location, Dependencies);
				}
			}

			return Result;
		}

		/// <summary>
		/// Add all the files included by a source file to a set, using a cache.
		/// </summary>
		/// <param name="SourceFile">The file to check.</param>
		/// <param name="IncludePaths">Include paths to search.</param>
		/// <param name="bOnlyCachedDependencies">Whether to just return cached dependencies, or update the cache with new results.</param>
		private List<FileItem> FindAndCacheIncludedFiles(FileItem SourceFile, CppIncludePaths IncludePaths, bool bOnlyCachedDependencies)
		{
			// Get the map of files to their list of includes
			Dictionary<FileItem, List<FileItem>> FileToIncludedFiles = bOnlyCachedDependencies ? OnlyCachedIncludedFilesMap : ExhaustiveIncludedFilesMap;

			// Check if the included files is in the cache. If not, we'll create it.
			List<FileItem> IncludedFiles;
			if(!FileToIncludedFiles.TryGetValue(SourceFile, out IncludedFiles))
			{
				HashSet<FileItem> VisitedFiles = new HashSet<FileItem>();
				VisitedFiles.Add(SourceFile);

				HashSet<FileItem> IncludedFilesSet = new HashSet<FileItem>();
				FindAndCacheIncludedFilesInner(SourceFile, IncludedFilesSet, IncludePaths, bOnlyCachedDependencies, VisitedFiles);

				IncludedFiles = IncludedFilesSet.ToList();
				FileToIncludedFiles.Add(SourceFile, IncludedFiles);
			}

			return IncludedFiles;
		}

		/// <summary>
		/// Add all the files included by a source file to a set, using a cache.
		/// </summary>
		/// <param name="SourceFile">The file to check.</param>
		/// <param name="IncludedFiles">Set of included files to add to</param>
		/// <param name="IncludePaths">Include paths to search.</param>
		/// <param name="bOnlyCachedDependencies">Whether to just return cached dependencies, or update the cache with new results.</param>
		/// <param name="VisitedFiles">Set of files that have already been visited. Used to prevent infinite loops between circularly dependent headers.</param>
		private void FindAndCacheIncludedFilesInner(FileItem SourceFile, HashSet<FileItem> IncludedFiles, CppIncludePaths IncludePaths, bool bOnlyCachedDependencies, HashSet<FileItem> VisitedFiles)
		{
			HashSet<FileItem> DirectlyIncludedFiles = GetDirectlyIncludedFiles(SourceFile, IncludePaths, bOnlyCachedDependencies);
			foreach (FileItem DirectlyIncludedFile in DirectlyIncludedFiles)
			{
				if(IncludedFiles.Add(DirectlyIncludedFile))
				{
					// Get the map of files to their list of includes
					Dictionary<FileItem, List<FileItem>> FileToIncludedFiles = bOnlyCachedDependencies ? OnlyCachedIncludedFilesMap : ExhaustiveIncludedFilesMap;

					// Recursively add the files included by this file
					List<FileItem> InnerFiles;
					if(FileToIncludedFiles.TryGetValue(DirectlyIncludedFile, out InnerFiles))
					{
						// We already have the include paths cached; just add them directly.
						IncludedFiles.UnionWith(InnerFiles);
					}
					else if(VisitedFiles.Add(DirectlyIncludedFile))
					{
						// We don't have include paths cached, and this isn't a recursive call. Create a new set and add it to the cache.
						HashSet<FileItem> InnerFilesSet = new HashSet<FileItem>();
						FindAndCacheIncludedFilesInner(DirectlyIncludedFile, InnerFilesSet, IncludePaths, bOnlyCachedDependencies, VisitedFiles);
						FileToIncludedFiles.Add(DirectlyIncludedFile, InnerFilesSet.ToList());
						IncludedFiles.UnionWith(InnerFilesSet);
					}
					else
					{
						// We're already building a list of include paths for this file up the stack. Just recurse through it this time.
						FindAndCacheIncludedFilesInner(DirectlyIncludedFile, IncludedFiles, IncludePaths, bOnlyCachedDependencies, VisitedFiles);
					}
				}
			}
		}

		/// <summary>
		/// Get a set of directly included files from the given source file, resolving their include paths to FileItem instances.
		/// </summary>
		/// <param name="SourceFile">The file to check.</param>
		/// <param name="IncludePaths">Include paths to search.</param>
		/// <param name="bOnlyCachedDependencies">Whether to just return cached dependencies, or update the cache with new results.</param>
		/// <returns>Set of files that are included</returns>
		private HashSet<FileItem> GetDirectlyIncludedFiles(FileItem SourceFile, CppIncludePaths IncludePaths, bool bOnlyCachedDependencies)
		{
			// Gather a list of names of files directly included by this C++ file.
			List<DependencyInclude> DirectIncludes = GetDirectIncludeDependencies(SourceFile, bOnlyCachedDependencies: bOnlyCachedDependencies);

			// Build a list of the unique set of files that are included by this file.
			HashSet<FileItem> DirectlyIncludedFiles = new HashSet<FileItem>();
			// require a for loop here because we need to keep track of the index in the list.
			for (int DirectlyIncludedFileNameIndex = 0; DirectlyIncludedFileNameIndex < DirectIncludes.Count; ++DirectlyIncludedFileNameIndex)
			{
				// Resolve the included file name to an actual file.
				DependencyInclude DirectInclude = DirectIncludes[DirectlyIncludedFileNameIndex];
				if (!DirectInclude.HasAttemptedResolve ||
					// ignore any preexisting resolve cache if we are not configured to use it.
					!bUseIncludeDependencyResolveCache ||
					// if we are testing the resolve cache, we force UBT to resolve every time to look for conflicts
					bTestIncludeDependencyResolveCache
					)
				{
					++TotalDirectIncludeResolveCacheMisses;

					// search the include paths to resolve the file
					FileItem DirectIncludeResolvedFile = CPPHeaders.FindIncludedFile(SourceFile.Location, DirectInclude.IncludeName, IncludePaths);
					if (DirectIncludeResolvedFile != null)
					{
						DirectlyIncludedFiles.Add(DirectIncludeResolvedFile);
					}
					IncludeDependencyCache.CacheResolvedIncludeFullPath(SourceFile, DirectlyIncludedFileNameIndex, DirectIncludeResolvedFile != null ? DirectIncludeResolvedFile.Location : null, bUseIncludeDependencyResolveCache, bTestIncludeDependencyResolveCache);
				}
				else
				{
					// we might have cached an attempt to resolve the file, but couldn't actually find the file (system headers, etc).
					if (DirectInclude.IncludeResolvedNameIfSuccessful != null)
					{
						DirectlyIncludedFiles.Add(FileItem.GetItemByFileReference(DirectInclude.IncludeResolvedNameIfSuccessful));
					}
				}
			}
			TotalDirectIncludeResolves += DirectIncludes.Count;

			return DirectlyIncludedFiles;
		}

		public static double TotalTimeSpentGettingIncludes = 0.0;
		public static int TotalIncludesRequested = 0;
		public static double DirectIncludeCacheMissesTotalTime = 0.0;
		public static int TotalDirectIncludeCacheMisses = 0;
		public static int TotalDirectIncludeResolveCacheMisses = 0;
		public static int TotalDirectIncludeResolves = 0;


		/// <summary>
		/// Regex that matches #include statements.
		/// </summary>
		static readonly Regex CPPHeaderRegex = new Regex("(([ \t]*#[ \t]*include[ \t]*[<\"](?<HeaderFile>[^\">]*)[\">][^\n]*\n*)|([^\n]*\n*))*",
													RegexOptions.Compiled | RegexOptions.Singleline | RegexOptions.ExplicitCapture);

		static readonly Regex MMHeaderRegex = new Regex("(([ \t]*#[ \t]*import[ \t]*[<\"](?<HeaderFile>[^\">]*)[\">][^\n]*\n*)|([^\n]*\n*))*",
													RegexOptions.Compiled | RegexOptions.Singleline | RegexOptions.ExplicitCapture);

		/// <summary>
		/// Regex that matches C++ code with UObject declarations which we will need to generated code for.
		/// </summary>
		static readonly Regex UObjectRegex = new Regex("^\\s*U(CLASS|STRUCT|ENUM|INTERFACE|DELEGATE)\\b", RegexOptions.Compiled | RegexOptions.Multiline);

		// Maintains a cache of file contents
		private static Dictionary<string, string> FileContentsCache = new Dictionary<string, string>();

		private static string GetFileContents(string Filename)
		{
			string Contents;
			if (FileContentsCache.TryGetValue(Filename, out Contents))
			{
				return Contents;
			}

			using (StreamReader Reader = new StreamReader(Filename, System.Text.Encoding.UTF8))
			{
				Contents = Reader.ReadToEnd();
				FileContentsCache.Add(Filename, Contents);
			}

			return Contents;
		}

		// Checks if a file contains UObjects
		public static bool DoesFileContainUObjects(string Filename)
		{
			string Contents = GetFileContents(Filename);
			return UObjectRegex.IsMatch(Contents);
		}

		/// <summary>
		/// Finds the names of files directly included by the given C++ file, and also whether the file contains any UObjects
		/// </summary>
		public List<DependencyInclude> GetDirectIncludeDependencies(FileItem CPPFile, bool bOnlyCachedDependencies)
		{
			// Try to fulfill request from cache first.
			List<DependencyInclude> Info = IncludeDependencyCache.GetCachedDependencyInfo(CPPFile);
			if (Info != null)
			{
				return Info;
			}

			List<DependencyInclude> Result = new List<DependencyInclude>();
			if (bOnlyCachedDependencies)
			{
				return Result;
			}

			DateTime TimerStartTime = DateTime.UtcNow;
			++CPPHeaders.TotalDirectIncludeCacheMisses;

			Result = GetUncachedDirectIncludeDependencies(CPPFile.Location, ProjectFile);

			// Populate cache with results.
			IncludeDependencyCache.SetDependencyInfo(CPPFile, Result);

			CPPHeaders.DirectIncludeCacheMissesTotalTime += (DateTime.UtcNow - TimerStartTime).TotalSeconds;

			return Result;
		}

		public static List<DependencyInclude> GetUncachedDirectIncludeDependencies(FileReference SourceFile, FileReference ProjectFile)
		{
			List<DependencyInclude> Result = new List<DependencyInclude>();

			// Get the adjusted filename
			string FileToRead = SourceFile.FullName;

			// Read lines from the C++ file.
			string FileContents = GetFileContents(FileToRead);
			if (string.IsNullOrEmpty(FileContents))
			{
				return Result;
			}

			// Note: This depends on UBT executing w/ a working directory of the Engine/Source folder!
			string EngineSourceFolder = Directory.GetCurrentDirectory();
			string InstalledFolder = EngineSourceFolder;
			Int32 EngineSourceIdx = EngineSourceFolder.IndexOf("\\Engine\\Source");
			if (EngineSourceIdx != -1)
			{
				InstalledFolder = EngineSourceFolder.Substring(0, EngineSourceIdx);
			}

			if (Utils.IsRunningOnMono)
			{
				// Mono crashes when running a regex on a string longer than about 5000 characters, so we parse the file in chunks
				int StartIndex = 0;
				const int SafeTextLength = 4000;
				while (StartIndex < FileContents.Length)
				{
					int EndIndex = StartIndex + SafeTextLength < FileContents.Length ? FileContents.IndexOf("\n", StartIndex + SafeTextLength) : FileContents.Length;
					if (EndIndex == -1)
					{
						EndIndex = FileContents.Length;
					}

					Result.AddRange(CollectHeaders(ProjectFile, SourceFile, FileToRead, FileContents, InstalledFolder, StartIndex, EndIndex));

					StartIndex = EndIndex + 1;
				}
			}
			else
			{
				Result = CollectHeaders(ProjectFile, SourceFile, FileToRead, FileContents, InstalledFolder, 0, FileContents.Length);
			}

			return Result;
		}

		/// <summary>
		/// Collects all header files included in a source file
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="SourceFile"></param>
		/// <param name="FileToRead"></param>
		/// <param name="FileContents"></param>
		/// <param name="InstalledFolder"></param>
		/// <param name="StartIndex"></param>
		/// <param name="EndIndex"></param>
		private static List<DependencyInclude> CollectHeaders(FileReference ProjectFile, FileReference SourceFile, string FileToRead, string FileContents, string InstalledFolder, int StartIndex, int EndIndex)
		{
			List<DependencyInclude> Result = new List<DependencyInclude>();

			Match M = CPPHeaderRegex.Match(FileContents, StartIndex, EndIndex - StartIndex);
			CaptureCollection Captures = M.Groups["HeaderFile"].Captures;
			Result.Capacity = Result.Count;
			foreach (Capture C in Captures)
			{
				string HeaderValue = C.Value;

				if (HeaderValue.IndexOfAny(Path.GetInvalidPathChars()) != -1)
				{
					throw new BuildException("In {0}: An #include statement contains invalid characters.  You might be missing a double-quote character. (\"{1}\")", FileToRead, C.Value);
				}

				//@TODO: The intermediate exclusion is to work around autogenerated absolute paths in Module.SomeGame.cpp style files
				bool bCheckForBackwardSlashes = FileToRead.StartsWith(InstalledFolder) || ((ProjectFile != null) && new FileReference(FileToRead).IsUnderDirectory(ProjectFile.Directory));
				if (bCheckForBackwardSlashes && !FileToRead.Contains("Intermediate") && !FileToRead.Contains("ThirdParty") && HeaderValue.IndexOf('\\', 0) >= 0)
				{
					throw new BuildException("In {0}: #include \"{1}\" contains backslashes ('\\'), please use forward slashes ('/') instead.", FileToRead, C.Value);
				}
				HeaderValue = Utils.CleanDirectorySeparators(HeaderValue);
				Result.Add(new DependencyInclude(HeaderValue));
			}

			// also look for #import in objective C files
			if (SourceFile.HasExtension(".MM") || SourceFile.HasExtension(".M"))
			{
				M = MMHeaderRegex.Match(FileContents, StartIndex, EndIndex - StartIndex);
				Captures = M.Groups["HeaderFile"].Captures;
				Result.Capacity += Captures.Count;
				foreach (Capture C in Captures)
				{
					Result.Add(new DependencyInclude(C.Value));
				}
			}

			return Result;
		}
	}
}
