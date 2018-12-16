// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
	class CppIncludePaths
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


		public CPPHeaders(FileReference ProjectFile, FileReference DependencyCacheFile)
		{
			this.ProjectFile = ProjectFile;
			this.DependencyCacheFile = DependencyCacheFile;
		}

		public static TimeSpan DirectIncludeCacheMissesTotalTime = TimeSpan.Zero;
		public static int TotalDirectIncludeCacheMisses = 0;


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

			Stopwatch Timer = Stopwatch.StartNew();

			++CPPHeaders.TotalDirectIncludeCacheMisses;

			Result = GetUncachedDirectIncludeDependencies(CPPFile.Location, ProjectFile);

			// Populate cache with results.
			IncludeDependencyCache.SetDependencyInfo(CPPFile, Result);

			CPPHeaders.DirectIncludeCacheMissesTotalTime += Timer.Elapsed;

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
