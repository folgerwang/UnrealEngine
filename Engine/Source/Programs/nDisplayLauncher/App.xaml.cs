// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace nDisplayLauncher
{
	/// <summary>
	/// Interaction logic for App.xaml
	/// </summary>
	public partial class App : Application
	{
		protected override void OnStartup(StartupEventArgs e)
		{
			EventManager.RegisterClassHandler(typeof(TextBox), UIElement.PreviewMouseLeftButtonDownEvent,
				new MouseButtonEventHandler(SelectivelyHandleMouseButton), true);
			EventManager.RegisterClassHandler(typeof(TextBox), UIElement.GotKeyboardFocusEvent,
				new RoutedEventHandler(SelectAllText), true);

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
				textBox.SelectAll();
		}


		private void ParseCommandLine(string[] args)
		{
			foreach (string arg in args)
			{
				try
				{
					if (arg.StartsWith(Runner.ArgListenerPort, StringComparison.OrdinalIgnoreCase))
					{
						Runner.DefaultListenerPort = int.Parse(arg.Substring(Runner.ArgListenerPort.Length));
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
