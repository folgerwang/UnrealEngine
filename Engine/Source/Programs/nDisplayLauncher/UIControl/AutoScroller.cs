// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Windows;
using System.Windows.Controls;


namespace nDisplayLauncher.UIControl
{
	public static class AutoScroller
	{
		public static readonly DependencyProperty AutoScrollProperty =
			DependencyProperty.RegisterAttached("AutoScroll", typeof(bool), typeof(AutoScroller), new PropertyMetadata(false, AutoScrollPropertyChanged));


		public static void AutoScrollPropertyChanged(DependencyObject obj, DependencyPropertyChangedEventArgs args)
		{
			var scrollViewer = obj as ScrollViewer;
			if (scrollViewer != null && (bool)args.NewValue)
			{
				scrollViewer.SizeChanged += ScrollViewer_SizeChanged;
				scrollViewer.ScrollToEnd();
			}
			else
			{
				scrollViewer.LayoutUpdated -= ScrollViewer_SizeChanged;
			}
		}

		private static void ScrollViewer_SizeChanged(object sender, EventArgs e)
		{
			var scrollViewer = sender as ScrollViewer;
			scrollViewer?.ScrollToEnd();
		}

		public static bool GetAutoScroll(DependencyObject obj)
		{
			return (bool)obj.GetValue(AutoScrollProperty);
		}

		public static void SetAutoScroll(DependencyObject obj, bool value)
		{
			obj.SetValue(AutoScrollProperty, value);
		}
	}
}
