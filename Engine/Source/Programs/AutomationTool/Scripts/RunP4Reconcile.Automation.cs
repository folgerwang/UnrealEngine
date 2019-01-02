// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AutomationTool
{
	/// <summary>
	/// Runs a P4 reconcile against a file or path and submits the result
	/// </summary>
	[RequireP4]
	class RunP4Reconcile : BuildCommand
	{
		public override void ExecuteBuild()
		{
			string Paths = ParseParamValue("Paths", null);
			if(string.IsNullOrWhiteSpace(Paths))
			{
				throw new AutomationException("-Paths must be defined! Usage: -Paths=Path1/...;Path2.txt");
			}

			string Description = ParseParamValue("Description", null);
			if(string.IsNullOrWhiteSpace(Description))
			{
				throw new AutomationException("-Description must be defined!");
			}

			string FileType = ParseParamValue("FileType", null);

			if (!CommandUtils.AllowSubmit)
			{
				LogWarning("Submitting to Perforce is disabled by default. Run with the -submit argument to allow.");
			}
			else
			{
				// Get the connection that we're going to submit with
				P4Connection SubmitP4 = CommandUtils.P4;
				// Reconcile the path against the depot
				int NewCL = SubmitP4.CreateChange(Description: Description.Replace("\\n", "\n"));
				try
				{
					foreach (string Path in Paths.Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries))
					{
						SubmitP4.Sync(String.Format("-k \"{0}\"", Path), AllowSpew: false);
						SubmitP4.Reconcile(NewCL, Path);
						if (FileType != null)
						{
							SubmitP4.P4(String.Format("reopen -t \"{0}\" \"{1}\"", FileType, Path), AllowSpew: false);
						}
					}

					if (SubmitP4.TryDeleteEmptyChange(NewCL))
					{
						CommandUtils.LogInformation("No files to submit; ignored.");
						return;
					}

					// Submit it
					int SubmittedCL;
					SubmitP4.Submit(NewCL, out SubmittedCL, true);
					if (SubmittedCL <= 0)
					{
						throw new AutomationException("Submit failed.");
					}
					CommandUtils.LogInformation("Submitted in changelist {0}", SubmittedCL);
				}
				catch(Exception Ex)
				{
					LogError("Failed to reconcile and submit files to P4, reason: {0}, reverting and deleting change.", Ex.ToString());
					SubmitP4.DeleteChange(NewCL);
				}
			}
		}

	}
}