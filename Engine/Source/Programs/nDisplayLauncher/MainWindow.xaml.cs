// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Input;
using Microsoft.Win32;
using System.IO;
using System.Reflection;
using nDisplayLauncher.Config;
using nDisplayLauncher.Settings;

namespace nDisplayLauncher
{
	/// <summary>
	/// Interaction logic for MainWindow.xaml
	/// </summary>
	public partial class MainWindow : Window
	{
		static readonly string CfgFileExtention = "CAVE config file (*.cfg)|*.cfg";
		static readonly string AppFileExtention = "CAVE VR application (*.exe)|*.exe";

		public VRConfig CurrentConfig;
		public Runner CmdRunner;
		string WindowTitle = string.Empty;

		public MainWindow()
		{
			InitializeComponent();

			CmdRunner = new Runner();

			CtrlLauncherTab.DataContext = CmdRunner;
			appLogTextBox.DataContext = AppLogger.Instance;
			SetDefaultConfig();
		}

		private void Window_Loaded(object sender, RoutedEventArgs e)
		{
			UpdateTitle();
		}

		private void SetDefaultConfig()
		{
			string configPath = RegistrySaver.ReadStringFromRegistry(RegistrySaver.RegConfigName);
			if (string.IsNullOrEmpty(configPath))
			{
				CreateConfig();

			}
			else
			{
				ConfigFileParser(configPath);
			}
		}

		//Config file parser
		private void ConfigFileParser(string filePath)
		{
			CreateConfig();
			Parser.Parse(filePath, CurrentConfig);
			//Set first items in listboxes and treeview as default if existed
			CurrentConfig.SelectFirstItems();
			try
			{
				((CollectionViewSource)this.Resources["cvsInputTrackers"]).View.Refresh();
			}
			catch (NullReferenceException)
			{

			}
			//sceneNodeTrackerCb.SelectedIndex = -1;
			UpdateTitle();
			//SetViewportPreview();
		}

		//Setting title of widow.
		private void UpdateTitle()
		{
			WindowTitle = CurrentConfig.name + " - nDisplay Launcher ver. " + Assembly.GetExecutingAssembly().GetName().Version.ToString();
			this.Title = WindowTitle;
		}

		void CreateConfig()
		{
			RegistrySaver.RemoveAllRegistryValues(RegistrySaver.RegConfigName);
			CurrentConfig = new VRConfig();
			this.DataContext = CurrentConfig;
			//crutch. for refactoring
			CurrentConfig.selectedSceneNodeView = null;
			//AppLogger.Add("New config initialized");
			UpdateTitle();
		}

		public static void ConfigModifyIndicator()
		{
			if (!Application.Current.MainWindow.Title.StartsWith("*"))
			{
				Application.Current.MainWindow.Title = "*" + Application.Current.MainWindow.Title;
			}
		}

		//Exit app
		private void Exit(object sender, RoutedEventArgs e)
		{
			this.Close();
		}

		protected override void OnClosing(System.ComponentModel.CancelEventArgs e)
		{
			base.OnClosing(e);
		}

		private void CollectionViewSource_Filter(object sender, FilterEventArgs e)
		{

			if (e.Item is TrackerInput)
			{
				e.Accepted = true;
			}
			else
			{
				e.Accepted = false;
			}
		}

		private void configsCb_DropDownOpened(object sender, EventArgs e)
		{
			configsCb.Items.Refresh();
		}

		private void runBtn_Click(object sender, RoutedEventArgs e)
		{
			if (applicationsListBox.SelectedIndex < 0 || configsCb.SelectedIndex < 0)
			{
				AppLogger.Add("No application/config selected");
				return;
			}

			CmdRunner.ProcessCommand(Runner.ClusterCommandType.RunApp);
		}

		private void killBtn_Click(object sender, RoutedEventArgs e)
		{
			if (configsCb.SelectedIndex < 0)
			{
				AppLogger.Add("No config selected");
				return;
			}

			CmdRunner.ProcessCommand(Runner.ClusterCommandType.KillApp);
		}

		private void startDaemonsBtn_Click(object sender, RoutedEventArgs e)
		{
			if (configsCb.SelectedIndex < 0)
			{
				AppLogger.Add("No config selected");
				return;
			}

			CmdRunner.ProcessCommand(Runner.ClusterCommandType.StartListeners);
		}

		private void stopDaemonsBtn_Click(object sender, RoutedEventArgs e)
		{
			if (configsCb.SelectedIndex < 0)
			{
				AppLogger.Add("No config selected");
				return;
			}

			CmdRunner.ProcessCommand(Runner.ClusterCommandType.StopListeners);
		}

		private void deployAppBtn_Click(object sender, RoutedEventArgs e)
		{
			if (applicationsListBox.SelectedIndex < 0 || configsCb.SelectedIndex < 0)
			{
				AppLogger.Add("No application/config selected");
				return;
			}

			CmdRunner.ProcessCommand(Runner.ClusterCommandType.DeployApp);
		}

		private void CopyToClipboard(string text)
		{
			if (text != String.Empty)
			{
				Clipboard.SetText(text);
			}
		}

		private void addAppButton_Click(object sender, RoutedEventArgs e)
		{
			OpenFileDialog openFileDialog = new OpenFileDialog();
			openFileDialog.Filter = AppFileExtention;
			if (openFileDialog.ShowDialog() == true)
			{
				string appPath = openFileDialog.FileName;
				CmdRunner.AddApplication(appPath);
				applicationsListBox.Items.Refresh();
			}
		}

		private void deleteAppButton_Click(object sender, RoutedEventArgs e)
		{
			if (applicationsListBox.SelectedItem != null)
			{
				CmdRunner.DeleteApplication();
				applicationsListBox.Items.Refresh();
			}
		}

		private void addConfigButton_Click(object sender, RoutedEventArgs e)
		{
			OpenFileDialog openFileDialog = new OpenFileDialog();
			openFileDialog.Filter = CfgFileExtention;
			if (openFileDialog.ShowDialog() == true)
			{
				string configPath = openFileDialog.FileName;
				if (!CmdRunner.Configs.Exists(x => x == configPath))
				{
					CmdRunner.AddConfig(configPath);
					configsCb.Items.Refresh();
				}
			}
		}

		private void copyAppLogBtn_Click(object sender, RoutedEventArgs e)
		{
			CopyToClipboard(appLogTextBox.Text);
		}

		private void cleanAppLogBtn_Click(object sender, RoutedEventArgs e)
		{
			AppLogger.CleanLog();
		}


		private void configsCb_SelectionChanged(object sender, SelectionChangedEventArgs e)
		{
			CmdRunner.ChangeConfigSelection(CmdRunner.SelectedConfig);
		}

		private void deleteConfigButton_Click(object sender, RoutedEventArgs e)
		{
			if (configsCb.SelectedItem != null)
			{
				CmdRunner.DeleteConfig();
				configsCb.Items.Refresh();
			}
		}
	}
}
