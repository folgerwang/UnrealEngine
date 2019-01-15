// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.Windows;

using nDisplayLauncher.Log;


namespace nDisplayLauncher
{
	public partial class MainWindow
	{
		private void InitializeAppLogger()
		{
			ctrlTextLog.DataContext = AppLogger.Instance;
		}

		private void ctrlBtnCopyLog_Click(object sender, RoutedEventArgs e)
		{
			if (ctrlTextLog.Text != null)
			{
				System.Windows.Clipboard.SetText(ctrlTextLog.Text);
			}
		}

		private void ctrlBtnCleanLog_Click(object sender, RoutedEventArgs e)
		{
			AppLogger.CleanLog();
		}
	}
}
