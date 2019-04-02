// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	/// <summary>
	/// This class contains settings for a site-specific deployment of UGS. Epic's internal implementation uses a static constructor in a NotForLicensees folder to initialize these values.
	/// </summary>
	static partial class DeploymentSettings
	{
		/// <summary>
		/// SQL connection string used to connect to the database for telemetry and review data. The 'Program' class is a partial class, to allow an
		/// opportunistically included C# source file in NotForLicensees/ProgramSettings.cs to override this value in a static constructor.
		/// </summary>
		public static readonly string ApiUrl = null;

		/// <summary>
		/// Specifies the depot path to sync down the stable version of UGS from, without a trailing slash (eg. //depot/UnrealGameSync/bin). This is a site-specific setting. 
		/// The UnrealGameSync executable should be located at Release/UnrealGameSync.exe under this path, with any dependent DLLs.
		/// </summary>
		public static readonly string DefaultDepotPath = null;

		/// <summary>
		/// Whether to send telemetry data by default. Can be useful for finding users in need of a hardware upgrade, but can bloat the DB for large teams.
		/// </summary>
		public static bool bSendTelemetry = false;
	}
}
