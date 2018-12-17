// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Contains predicates required for the action graph generated for a build to be valid. Unlike individual actions within the graph, these conditions must hold for the graph itself to be valid.
	/// </summary>
	class BuildPrerequisites
	{
		/// <summary>
		/// Set of all source directories. Any files being added or removed from these directories will invalidate the makefile.
		/// </summary>
		public HashSet<DirectoryReference> SourceDirectories = new HashSet<DirectoryReference>();

		/// <summary>
		/// The set of source files that UnrealBuildTool determined to be part of the programmer's "working set". Used for adaptive non-unity builds.
		/// </summary>
		public HashSet<FileItem> WorkingSet = new HashSet<FileItem>();

		/// <summary>
		/// Set of files which are currently not part of the working set, but could be.
		/// </summary>
		public HashSet<FileItem> CandidatesForWorkingSet = new HashSet<FileItem>();

		/// <summary>
		/// Additional files which are required 
		/// </summary>
		public HashSet<FileItem> AdditionalDependencies = new HashSet<FileItem>(); 

		/// <summary>
		/// Used to map names of modules to their .Build.cs filename
		/// </summary>
		public List<UHTModuleHeaderInfo> UObjectModuleHeaders = new List<UHTModuleHeaderInfo>();

		public BuildPrerequisites()
		{
		}

		public BuildPrerequisites(BinaryArchiveReader Reader)
		{
			SourceDirectories = Reader.ReadHashSet(() => Reader.ReadDirectoryReference());
			WorkingSet = Reader.ReadHashSet(() => Reader.ReadFileItem());
			CandidatesForWorkingSet = Reader.ReadHashSet(() => Reader.ReadFileItem());
			AdditionalDependencies = Reader.ReadHashSet(() => Reader.ReadFileItem());
			UObjectModuleHeaders = Reader.ReadList(() => new UHTModuleHeaderInfo(Reader));
		}

		public void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteHashSet(SourceDirectories, x => Writer.WriteDirectoryReference(x));
			Writer.WriteHashSet(WorkingSet, x => Writer.WriteFileItem(x));
			Writer.WriteHashSet(CandidatesForWorkingSet, x => Writer.WriteFileItem(x));
			Writer.WriteHashSet(AdditionalDependencies, x => Writer.WriteFileItem(x));
			Writer.WriteList(UObjectModuleHeaders, x => x.Write(Writer));
		}
	}
}
