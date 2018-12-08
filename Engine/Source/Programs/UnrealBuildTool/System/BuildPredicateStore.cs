// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
	[Serializable]
	class BuildPredicateStore
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

		public BuildPredicateStore()
		{
		}

		public BuildPredicateStore(BinaryReader Reader, List<FileItem> UniqueFileItems)
		{
			SourceDirectories = new HashSet<DirectoryReference>(Reader.ReadArray(() => Reader.ReadDirectoryReference()));
			WorkingSet = new HashSet<FileItem>(Reader.ReadFileItemList(UniqueFileItems));
			CandidatesForWorkingSet = new HashSet<FileItem>(Reader.ReadFileItemList(UniqueFileItems));
			AdditionalDependencies = new HashSet<FileItem>(Reader.ReadFileItemList(UniqueFileItems));
			UObjectModuleHeaders = Reader.ReadList(() => new UHTModuleHeaderInfo(Reader, UniqueFileItems));
		}

		public void Write(BinaryWriter Writer, Dictionary<FileItem, int> UniqueFileItemToIndex)
		{
			Writer.Write(SourceDirectories.ToArray(), x => Writer.Write(x));
			Writer.Write(WorkingSet.ToList(), UniqueFileItemToIndex);
			Writer.Write(CandidatesForWorkingSet.ToList(), UniqueFileItemToIndex);
			Writer.Write(AdditionalDependencies.ToList(), UniqueFileItemToIndex);
			Writer.Write(UObjectModuleHeaders, x => x.Write(Writer, UniqueFileItemToIndex));
		}
	}
}
