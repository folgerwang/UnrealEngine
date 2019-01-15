// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Represents a Mac/IOS framework
	/// </summary>
	class UEBuildFramework
	{
		/// <summary>
		/// The name of this framework
		/// </summary>
		public readonly string Name;

		/// <summary>
		/// Path to a zip file containing the framework. May be null.
		/// </summary>
		public readonly FileReference ZipFile;

		/// <summary>
		/// Path to the extracted framework directory.
		/// </summary>
		public readonly DirectoryReference OutputDirectory;

		/// <summary>
		/// 
		/// </summary>
		public readonly string CopyBundledAssets;

		/// <summary>
		/// File created after the framework has been extracted. Used to add dependencies into the action graph.
		/// </summary>
		public FileItem ExtractedTokenFile;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Name">The framework name</param>
		/// <param name="CopyBundledAssets"></param>
		public UEBuildFramework(string Name, string CopyBundledAssets = null)
		{
			this.Name = Name;
			this.CopyBundledAssets = CopyBundledAssets;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Name">The framework name</param>
		/// <param name="ZipFile">Path to the zip file for this framework</param>
		/// <param name="OutputDirectory">Path for the extracted zip file</param>
		/// <param name="CopyBundledAssets"></param>
		public UEBuildFramework(string Name, FileReference ZipFile, DirectoryReference OutputDirectory, string CopyBundledAssets)
		{
			this.Name = Name;
			this.ZipFile = ZipFile;
			this.OutputDirectory = OutputDirectory;
			this.CopyBundledAssets = CopyBundledAssets;
		}
	}
}
