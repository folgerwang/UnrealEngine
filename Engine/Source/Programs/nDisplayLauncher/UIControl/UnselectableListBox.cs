// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.Collections;
using System.Windows;
using System.Windows.Controls;


namespace nDisplayLauncher.UIControl
{
	public class UnselectableListBox : ListBox
	{
		public UnselectableListBox() : base()
		{
			SelectionChanged += new SelectionChangedEventHandler((sender, e) =>
			{
				if (e.AddedItems.Count > 0)
				{
					var last = e.AddedItems[0];
					foreach (var item in new ArrayList(SelectedItems))
						if (item != last) SelectedItems.Remove(item);
				}
			});
		}

		static UnselectableListBox()
		{
			SelectionModeProperty.OverrideMetadata(typeof(UnselectableListBox),
				new FrameworkPropertyMetadata(SelectionMode.Multiple));
		}
	}
}
