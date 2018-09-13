// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// 
	/// </summary>
	class UEBuildFramework
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="Name"></param>
		/// <param name="ZipPath"></param>
		/// <param name="CopyBundledAssets"></param>
		public UEBuildFramework(string Name, string ZipPath = null, string CopyBundledAssets = null)
		{
			this.FrameworkName = Name;
			this.FrameworkZipPath = ZipPath;
			this.CopyBundledAssets = CopyBundledAssets;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="Framework"></param>
		public UEBuildFramework(ModuleRules.Framework Framework)
		{
			FrameworkName = Framework.Name;
			FrameworkZipPath = Framework.ZipPath;
			CopyBundledAssets = Framework.CopyBundledAssets;
		}

		internal UEBuildModule OwningModule = null;

		/// <summary>
		/// 
		/// </summary>
		public string FrameworkName = null;

		/// <summary>
		/// 
		/// </summary>
		public string FrameworkZipPath = null;

		/// <summary>
		/// 
		/// </summary>
		public string CopyBundledAssets = null;
	}
}
