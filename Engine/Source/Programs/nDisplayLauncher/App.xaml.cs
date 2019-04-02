// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

using nDisplayLauncher.Cluster;


namespace nDisplayLauncher
{
	public partial class App : Application
	{
		protected override void OnStartup(StartupEventArgs e)
		{
			EventManager.RegisterClassHandler(typeof(TextBox), UIElement.PreviewMouseLeftButtonDownEvent, new MouseButtonEventHandler(SelectivelyHandleMouseButton), true);
			EventManager.RegisterClassHandler(typeof(TextBox), UIElement.GotKeyboardFocusEvent, new RoutedEventHandler(SelectAllText), true);

			ParseCommandLine(e.Args);

			base.OnStartup(e);
		}

		private static void SelectivelyHandleMouseButton(object sender, MouseButtonEventArgs e)
		{
			var textbox = (sender as TextBox);
			if (textbox != null && !textbox.IsKeyboardFocusWithin)
			{
				if (e.OriginalSource.GetType().Name == "TextBoxView")
				{
					e.Handled = true;
					textbox.Focus();
				}
			}
		}

		private static void SelectAllText(object sender, RoutedEventArgs e)
		{
			var textBox = e.OriginalSource as TextBox;
			if (textBox != null)
			{
				textBox.SelectAll();
			}
		}


		private void ParseCommandLine(string[] args)
		{
			foreach (string arg in args)
			{
				try
				{
					if (arg.StartsWith(Launcher.ArgListenerPort, StringComparison.OrdinalIgnoreCase))
					{
						Launcher.DefaultListenerPort = int.Parse(arg.Substring(Launcher.ArgListenerPort.Length));
						return;
					}
				}
				catch (Exception)
				{
				}
			}
		}
	}
}
