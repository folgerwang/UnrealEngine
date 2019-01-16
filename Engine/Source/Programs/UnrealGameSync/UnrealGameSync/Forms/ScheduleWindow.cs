// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	partial class ScheduleWindow : Form
	{
		public ScheduleWindow(bool InEnabled, LatestChangeType InChange, TimeSpan InTime, bool AnyOpenProject, IEnumerable<UserSelectedProjectSettings> ScheduledProjects, IEnumerable<UserSelectedProjectSettings> OpenProjects)
		{
			InitializeComponent();

			EnableCheckBox.Checked = InEnabled;
			ChangeComboBox.SelectedIndex = (int)InChange;

			DateTime CurrentTime = DateTime.Now;
			TimePicker.Value = new DateTime(CurrentTime.Year, CurrentTime.Month, CurrentTime.Day, InTime.Hours, InTime.Minutes, InTime.Seconds);

			ProjectListBox.Items.Add("Any open projects", AnyOpenProject);

			Dictionary<string, UserSelectedProjectSettings> LocalFileToProject = new Dictionary<string, UserSelectedProjectSettings>(StringComparer.InvariantCultureIgnoreCase);
			AddProjects(ScheduledProjects, LocalFileToProject);
			AddProjects(OpenProjects, LocalFileToProject);

			foreach(UserSelectedProjectSettings Project in LocalFileToProject.Values.OrderBy(x => x.ToString()))
			{
				bool Enabled = ScheduledProjects.Any(x => x.LocalPath == Project.LocalPath);
				ProjectListBox.Items.Add(Project, Enabled);
			}

			UpdateEnabledControls();
		}

		private void AddProjects(IEnumerable<UserSelectedProjectSettings> Projects, Dictionary<string, UserSelectedProjectSettings> LocalFileToProject)
		{
			foreach(UserSelectedProjectSettings Project in Projects)
			{
				if(Project.LocalPath != null)
				{
					LocalFileToProject[Project.LocalPath] = Project;
				}
			}
		}

		public void CopySettings(out bool OutEnabled, out LatestChangeType OutChange, out TimeSpan OutTime, out bool OutAnyOpenProject, out List<UserSelectedProjectSettings> OutScheduledProjects)
		{
			OutEnabled = EnableCheckBox.Checked;
			OutChange = (LatestChangeType)ChangeComboBox.SelectedIndex;
			OutTime = TimePicker.Value.TimeOfDay;

			OutAnyOpenProject = false;

			List<UserSelectedProjectSettings> ScheduledProjects = new List<UserSelectedProjectSettings>();
			foreach(int Index in ProjectListBox.CheckedIndices)
			{
				if(Index == 0)
				{
					OutAnyOpenProject = true;
				}
				else
				{
					ScheduledProjects.Add((UserSelectedProjectSettings)ProjectListBox.Items[Index]);
				}
			}
			OutScheduledProjects = ScheduledProjects;
		}

		private void EnableCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledControls();
		}

		private void UpdateEnabledControls()
		{
			ChangeComboBox.Enabled = EnableCheckBox.Checked;
			TimePicker.Enabled = EnableCheckBox.Checked;
			ProjectListBox.Enabled = EnableCheckBox.Checked;
		}
	}
}
