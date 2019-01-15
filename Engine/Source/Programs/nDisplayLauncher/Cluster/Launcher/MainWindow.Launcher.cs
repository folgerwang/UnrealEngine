// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Forms;

using nDisplayLauncher.Cluster;
using nDisplayLauncher.Cluster.Config;
using nDisplayLauncher.Cluster.Config.Conversion;
using nDisplayLauncher.Log;


namespace nDisplayLauncher
{
	public partial class MainWindow
	{
		static readonly string CfgFileExtention = "nDisplay config file (*.cfg)|*.cfg";
		static readonly string AppFileExtention = "nDisplay application (*.exe)|*.exe|Script (*.bat;*.cmd)|*.bat;*.cmd";


		private void InitializeLauncher()
		{
			CtrlLauncherTab.DataContext = TheLauncher;
		}

		private void ctrlComboConfigs_DropDownOpened(object sender, EventArgs e)
		{
			ctrlComboConfigs.Items.Refresh();
		}

		private void ctrlBtnRun_Click(object sender, RoutedEventArgs e)
		{
			if (ctrlListApps.SelectedIndex < 0 || ctrlComboConfigs.SelectedIndex < 0)
			{
				AppLogger.Log("No application/config selected");
				return;
			}

			TheLauncher.ProcessCommand(Launcher.ClusterCommandType.RunApp);
		}

		private void ctrlBtnKill_Click(object sender, RoutedEventArgs e)
		{
			if (ctrlComboConfigs.SelectedIndex < 0)
			{
				AppLogger.Log("No config selected");
				return;
			}

			TheLauncher.ProcessCommand(Launcher.ClusterCommandType.KillApp);
		}

		private void ctrlBtnRestartComputers_Click(object sender, RoutedEventArgs e)
		{
			if (ctrlComboConfigs.SelectedIndex < 0)
			{
				AppLogger.Log("No config selected");
				return;
			}

			DialogResult dialogResult = System.Windows.Forms.MessageBox.Show("Are you sure?", "Restart cluster nodes", MessageBoxButtons.YesNo);
			if (dialogResult == System.Windows.Forms.DialogResult.Yes)
			{
				TheLauncher.ProcessCommand(Launcher.ClusterCommandType.RestartComputers);
			}
		}

		private void ctrlBtnAddApplication_Click(object sender, RoutedEventArgs e)
		{
			System.Windows.Forms.OpenFileDialog openFileDialog = new System.Windows.Forms.OpenFileDialog();
			openFileDialog.Filter = AppFileExtention;
			if (openFileDialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
			{
				string appPath = openFileDialog.FileName;
				TheLauncher.AddApplication(appPath);
				ctrlListApps.Items.Refresh();
			}
		}

		private void ctrlDelApplicationBtn_Click(object sender, RoutedEventArgs e)
		{
			if (ctrlListApps.SelectedItem != null)
			{
				TheLauncher.DeleteApplication();
				ctrlListApps.Items.Refresh();
			}
		}

		private void ctrlComboConfigs_SelectionChanged(object sender, SelectionChangedEventArgs e)
		{
			TheLauncher.ChangeConfigSelection(TheLauncher.SelectedConfig);

			// Check if need to upgrade the selected file
			if (File.Exists(TheLauncher.SelectedConfig))
			{
				UpgradeConfigIfNeeded(TheLauncher.SelectedConfig);
			}
		}

		private void ctrlBtnAddConfig_Click(object sender, RoutedEventArgs e)
		{
			System.Windows.Forms.OpenFileDialog openFileDialog = new System.Windows.Forms.OpenFileDialog();
			openFileDialog.Filter = CfgFileExtention;
			if (openFileDialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
			{
				string configPath = openFileDialog.FileName;
				if (!TheLauncher.Configs.Exists(x => x == configPath))
				{
					TheLauncher.AddConfig(configPath);
					ctrlComboConfigs.Items.Refresh();
				}
			}
		}

		private void ctrlBtnDeleteConfig_Click(object sender, RoutedEventArgs e)
		{
			if (ctrlComboConfigs.SelectedItem != null)
			{
				TheLauncher.DeleteConfig();
				ctrlComboConfigs.Items.Refresh();
			}
		}

		private void UpgradeConfigIfNeeded(string file)
		{
			string Text = string.Empty;

			ConfigurationVersion Ver = Parser.GetVersion(TheLauncher.SelectedConfig);
			if (Ver < Launcher.CurrentVersion)
			{
				AppLogger.Log("An outdated config file selected.");
				Text += "Selected config file doesn't match the current version of the nDisplayLauncher. ";
				Text += "Would you like to convert this file automatically? A new file will be created and added to the list.";

				System.Windows.Forms.DialogResult dialogResult = System.Windows.Forms.MessageBox.Show(Text, "Wrong config file format", MessageBoxButtons.YesNo);
				if (dialogResult == System.Windows.Forms.DialogResult.Yes)
				{
					string NewFile = string.Empty;
					if (PerformConfigUpgrade(TheLauncher.SelectedConfig, ref NewFile))
					{
						Text = "Conversion successful. The newly created file was added to the list.\n";
						Text += NewFile;
						Text += "\nDon't forget to deploy the new config file to your remote machines.";
						System.Windows.Forms.MessageBox.Show(Text, "Success", MessageBoxButtons.OK);
					}
					else
					{
						Text = "Conversion failed. Please try to upgrade the configuration file manually.";
						System.Windows.Forms.MessageBox.Show(Text, "Failed", MessageBoxButtons.OK);
					}
				}
				else
				{
					AppLogger.Log("Config file upgrade skipped.");
				}
			}
			else if (Ver > Launcher.CurrentVersion)
			{
				Text = "Incompatible configuration file";
				System.Windows.Forms.MessageBox.Show(Text, "Failed", MessageBoxButtons.OK);
			}
		}

		private bool PerformConfigUpgrade(string OldFile, ref string NewFile)
		{
			AppLogger.Log("Upgrading the config file...");
			ConfigConversion.ConversionResult result = ConfigConversion.Convert(TheLauncher.SelectedConfig, Launcher.CurrentVersion);
			if (result.Success == true)
			{
				AppLogger.Log("Upgraded successfully. Auto-generated file location: " + result.NewConfigFile);
				if (!TheLauncher.Configs.Exists(x => x == result.NewConfigFile))
				{
					TheLauncher.AddConfig(result.NewConfigFile);
					ctrlComboConfigs.Items.Refresh();
				}

				// Make this new file selected in the GUI
				TheLauncher.SelectedConfig = result.NewConfigFile;
			}
			else
			{
				AppLogger.Log("Upgrade failed.");
			}

			NewFile = result.NewConfigFile;
			return result.Success;
		}

	}
}
