// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.Windows;


namespace nDisplayLauncher
{
	public enum UE4LogVerbosity
	{
		All = 0,
		Verbose,
		Log,
		Display,
		Warning,
		Error,
		Fatal
	}

	public partial class MainWindow
	{
		private void InitializeLog()
		{
			CtrlLogTab.DataContext = TheLauncher;
		}

		private void ctrlBtnSetVerbosityAll_Click(object sender, RoutedEventArgs e)
		{
			UE4LogVerbosity Verbosity = UE4LogVerbosity.Log;

			if (sender == ctrlBtnSetVerbosityAll)
			{
				Verbosity = UE4LogVerbosity.All;
			}
			else if (sender == ctrlBtnSetVerbosityVerbose)
			{
				Verbosity = UE4LogVerbosity.Verbose;
			}
			else if (sender == ctrlBtnSetVerbosityLog)
			{
				Verbosity = UE4LogVerbosity.Log;
			}
			else if (sender == ctrlBtnSetVerbosityDisplay)
			{
				Verbosity = UE4LogVerbosity.Display;
			}
			else if (sender == ctrlBtnSetVerbosityWarning)
			{
				Verbosity = UE4LogVerbosity.Warning;
			}
			else if (sender == ctrlBtnSetVerbosityError)
			{
				Verbosity = UE4LogVerbosity.Error;
			}
			else if (sender == ctrlBtnSetVerbosityFatal)
			{
				Verbosity = UE4LogVerbosity.Fatal;
			}
			else
			{
				return;
			}

			TheLauncher.SelectedVerbocityPlugin     = Verbosity;
			TheLauncher.SelectedVerbocityEngine     = Verbosity;
			TheLauncher.SelectedVerbocityConfig     = Verbosity;
			TheLauncher.SelectedVerbocityCluster    = Verbosity;
			TheLauncher.SelectedVerbocityGame       = Verbosity;
			TheLauncher.SelectedVerbocityGameMode   = Verbosity;
			TheLauncher.SelectedVerbocityInput      = Verbosity;
			TheLauncher.SelectedVerbocityVrpn       = Verbosity;
			TheLauncher.SelectedVerbocityNetwork    = Verbosity;
			TheLauncher.SelectedVerbocityNetworkMsg = Verbosity;
			TheLauncher.SelectedVerbocityRender     = Verbosity;
			TheLauncher.SelectedVerbocityBlueprint  = Verbosity;
		}
	}
}
