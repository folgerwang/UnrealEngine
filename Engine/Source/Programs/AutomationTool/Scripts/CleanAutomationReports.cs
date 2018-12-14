// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Commandlet to clean up all afolders under a temp storage root that are older than a given number of days
	/// </summary>
	[Help("Removes folders in an automation report directory that are older than a certain time.")]
	[Help("ReportDir=<Directory>", "Path to the root report directory")]
	[Help("Days=<N>", "Number of days to keep reports for")]
	class CleanAutomationReports : BuildCommand
	{
		/// <summary>
		/// Entry point for the commandlet
		/// </summary>
		public override void ExecuteBuild()
		{
			string ReportDir = ParseParamValue("ReportDir", null);
			if (ReportDir == null)
			{
				throw new AutomationException("Missing -ReportDir parameter");
			}

			string Days = ParseParamValue("Days", null);
			if (Days == null)
			{
				throw new AutomationException("Missing -Days parameter");
			}

			double DaysValue;
			if (!Double.TryParse(Days, out DaysValue))
			{
				throw new AutomationException("'{0}' is not a valid value for the -Days parameter", Days);
			}

			DateTime RetainTime = DateTime.UtcNow - TimeSpan.FromDays(DaysValue);

			// Enumerate all the build directories
			CommandUtils.LogInformation("Scanning {0}...", ReportDir);
			int NumFolders = 0;
			List<DirectoryInfo> FoldersToDelete = new List<DirectoryInfo>();
			foreach (DirectoryInfo BuildDirectory in new DirectoryInfo(ReportDir).EnumerateDirectories())
			{
				try
				{
					if(!BuildDirectory.EnumerateFiles("*", SearchOption.AllDirectories).Any(x => x.LastWriteTimeUtc > RetainTime))
					{
						FoldersToDelete.Add(BuildDirectory);
					}
					NumFolders++;
				}
				catch(Exception Ex)
				{
					CommandUtils.LogWarning("Unable to enumerate {0}: {1}", BuildDirectory.FullName, Ex.ToString());
				}
			}
			CommandUtils.LogInformation("Found {0} builds; {1} to delete.", NumFolders, FoldersToDelete.Count);

			// Delete them all
			for (int Idx = 0; Idx < FoldersToDelete.Count; Idx++)
			{
				try
				{
					CommandUtils.LogInformation("[{0}/{1}] Deleting {2}...", Idx + 1, FoldersToDelete.Count, FoldersToDelete[Idx].FullName);
					FoldersToDelete[Idx].Delete(true);
				}
				catch (Exception Ex)
				{
					CommandUtils.LogWarning("Failed to delete folder; will try one file at a time: {0}", Ex);
					CommandUtils.DeleteDirectory_NoExceptions(true, FoldersToDelete[Idx].FullName);
				}
			}
		}
	}
}
