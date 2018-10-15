// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using AutomationTool;
using UnrealBuildTool;

namespace Gauntlet
{ 
	/// <summary>
	/// Represents the configuration needed to run an instance of an Unreal app
	/// </summary>
	public class UnrealAppConfig : IAppConfig
    {
		/// <summary>
		/// Reference name
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Name of this unreal project
		/// </summary>
		public string ProjectName { get; set; }

		/// <summary>
		/// Type of role this instance performs
		/// </summary>
		public UnrealTargetRole ProcessType { get; set; }

		/// <summary>
		/// Platform this role runs on
		/// </summary>
		public UnrealTargetPlatform Platform { get; set; }

		/// <summary>
		/// Configuration for this role
		/// </summary>
		public UnrealTargetConfiguration Configuration { get; set; }

        /// <summary>
        /// Files to copy over to the device, plus what type of files they are.
        /// </summary>
        public List<UnrealFileToCopy> FilesToCopy { get; set; }
        
        /// <summary>
        /// Arguments for this instance
        /// </summary>
        public string CommandLine { get; set; }

		/// <summary>
		/// Sandbox that we'd like to install this instance in
		/// </summary>
		public string Sandbox { get; set; }

		// new system
		public IBuild Build { get; set; }
	}
}