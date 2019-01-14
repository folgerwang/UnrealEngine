// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Xml;
using System.Reflection;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Global settings for building. Should not contain any target-specific settings.
	/// </summary>
	class BuildConfiguration
	{
		/// <summary>
		/// Whether to ignore import library files that are out of date when building targets.  Set this to true to improve iteration time.
		/// By default we don't bother relinking targets if only a dependent .lib has changed, as chances are that
		/// the import library wasn't actually different unless a dependent header file of this target was also changed,
		/// in which case the target would be rebuilt automatically.
		/// </summary>
		[XmlConfigFile]
		public bool bIgnoreOutdatedImportLibraries = true;

		/// <summary>
		/// Use existing static libraries for all engine modules in this target.
		/// </summary>
		[CommandLine("-UsePrecompiled")]
		public bool bUsePrecompiled = false;

		/// <summary>
		/// Whether debug info should be written to the console.
		/// </summary>
		[XmlConfigFile]
		public bool bPrintDebugInfo = false;

		/// <summary>
		/// Whether to log detailed action stats. This forces local execution.
		/// </summary>
		[XmlConfigFile]
		public bool bLogDetailedActionStats = false;

		/// <summary>
		/// Whether the hybrid executor will be used (a remote executor and local executor)
		/// </summary>
		[XmlConfigFile]
		public bool bAllowHybridExecutor = false;

		/// <summary>
		/// Whether XGE may be used.
		/// </summary>
		[XmlConfigFile]
		[CommandLine("-NoXGE", Value = "false")]
		public bool bAllowXGE = true;

		/// <summary>
		/// Whether SN-DBS may be used.
		/// </summary>
		[XmlConfigFile]
		public bool bAllowSNDBS = true;

		/// <summary>
		/// Enables support for very fast iterative builds by caching target data.  Turning this on causes Unreal Build Tool to emit
		/// 'UBT Makefiles' for targets when they're built the first time.  Subsequent builds will load these Makefiles and begin
		/// outdatedness checking and build invocation very quickly.  The caveat is that if source files are added or removed to
		/// the project, UBT will need to gather information about those in order for your build to complete successfully.  Currently,
		/// you must run the project file generator after adding/removing source files to tell UBT to re-gather this information.
		/// 
		/// Events that can invalidate the 'UBT Makefile':  
		///		- Adding/removing .cpp files
		///		- Adding/removing .h files with UObjects
		///		- Adding new UObject types to a file that didn't previously have any
		///		- Changing global build settings (most settings in this file qualify.)
		///		- Changed code that affects how Unreal Header Tool works
		///	
		///	You can force regeneration of the 'UBT Makefile' by passing the '-gather' argument, or simply regenerating project files
		///
		///	This also enables the fast include file dependency scanning and caching system that allows Unreal Build Tool to detect out 
		/// of date dependencies very quickly.  When enabled, a deep C++ include graph does not have to be generated, and instead
		/// we only scan and cache indirect includes for after a dependent build product was already found to be out of date.  During the
		/// next build, we'll load those cached indirect includes and check for outdatedness.
		/// </summary>
		[XmlConfigFile]
		[CommandLine("-NoUBTMakefiles", Value = "false")]
		public bool bUseUBTMakefiles = true;

		/// <summary>
		/// Whether DMUCS/Distcc may be used.
		/// Distcc requires some setup - so by default disable it so we don't break local or remote building
		/// </summary>
		[XmlConfigFile]
		public bool bAllowDistcc = false;

		/// <summary>
		/// Whether to allow using parallel executor on Windows.
		/// </summary>
		[XmlConfigFile]
		public bool bAllowParallelExecutor = true;

		/// <summary>
		/// If true, force header regeneration. Intended for the build machine
		/// </summary>
		[CommandLine("-ForceHeaderGeneration")]
		[XmlConfigFile(Category = "UEBuildConfiguration")]
		public bool bForceHeaderGeneration = false;

		/// <summary>
		/// If true, do not build UHT, assume it is already built
		/// </summary>
		[CommandLine("-NoBuildUHT")]
		[XmlConfigFile(Category = "UEBuildConfiguration")]
		public bool bDoNotBuildUHT = false;

		/// <summary>
		/// If true, fail if any of the generated header files is out of date.
		/// </summary>
		[CommandLine("-FailIfGeneratedCodeChanges")]
		[XmlConfigFile(Category = "UEBuildConfiguration")]
		public bool bFailIfGeneratedCodeChanges = false;

		/// <summary>
		/// True if hot-reload from IDE is allowed
		/// </summary>
		[CommandLine("-NoHotReloadFromIDE", Value="false")]
		[XmlConfigFile(Category = "UEBuildConfiguration")]
		public bool bAllowHotReloadFromIDE = true;

		/// <summary>
		/// If true, the Debug version of UnrealHeaderTool will be build and run instead of the Development version.
		/// </summary>
		[XmlConfigFile(Category = "UEBuildConfiguration")]
		public bool bForceDebugUnrealHeaderTool = false;

		/// <summary>
		/// Whether to skip compiling rules assemblies and just assume they are valid
		/// </summary>
		[CommandLine("-SkipRulesCompile")]
		public bool bSkipRulesCompile = false;
	}
}
