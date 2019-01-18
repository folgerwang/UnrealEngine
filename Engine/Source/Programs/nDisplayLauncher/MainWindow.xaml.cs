// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.Windows;

using nDisplayLauncher.Cluster;
using nDisplayLauncher.Cluster.Events;


namespace nDisplayLauncher
{
	public partial class MainWindow : Window
	{
		public Launcher TheLauncher;

		public MainWindow()
		{
			InitializeComponent();

			TheLauncher = new Launcher();

			InitializeAppLogger();
			InitializeLauncher();
			InitializeEvents();
			InitializeLog();
		}

		private void Window_Loaded(object sender, RoutedEventArgs e)
		{
			this.Title = "nDisplay Launcher";
		}

		private void Exit(object sender, RoutedEventArgs e)
		{
			this.Close();
		}
	}
}
