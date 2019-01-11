// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Windows;

namespace nDisplayLauncher.Cluster.Events
{
	public partial class ClusterEventWindow : Window, INotifyPropertyChanged
	{
		public HashSet<string> AvailableCategories { get; set; } = new HashSet<string>();
		public HashSet<string> AvailableTypes      { get; set; } = new HashSet<string>();
		public HashSet<string> AvailableNames      { get; set; } = new HashSet<string>();

		public string SelectedCategory { get; set; } = string.Empty;
		public string SelectedType     { get; set; } = string.Empty;
		public string SelectedName     { get; set; } = string.Empty;

		private string _TextArg = string.Empty;
		public string TextArg
		{
			get
			{
				return _TextArg;
			}

			set
			{
				if (value != _TextArg)
				{
					_TextArg = value;
					OnPropertyChanged("TextArg");
				}
			}
		}

		private string _TextVal = string.Empty;
		public string TextVal
		{
			get
			{
				return _TextVal;
			}

			set
			{
				if (value != _TextVal)
				{
					_TextVal = value;
					OnPropertyChanged("TextVal");
				}
			}
		}

		private class EventArgVal
		{
			public EventArgVal(string a, string v) { Arg = a; Val = v; }
			public string Arg { get; set; } = string.Empty;
			public string Val { get; set; } = string.Empty;
		}

		private ObservableCollection<EventArgVal> Arguments = new ObservableCollection<EventArgVal>();

		public event PropertyChangedEventHandler PropertyChanged;
		protected void OnPropertyChanged(string propertyName)
		{
			PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
		}


		public ClusterEventWindow()
		{
			InitializeComponent();

			this.DataContext = this;

			ctrlListClusterEventArgs.ItemsSource = Arguments;
		}

		private void Window_Initialized(object sender, System.EventArgs e)
		{
			ctrlComboEventCategory.Foreground = System.Windows.Media.Brushes.White;
			ctrlComboEventType.Foreground     = System.Windows.Media.Brushes.White;
			ctrlComboEventName.Foreground     = System.Windows.Media.Brushes.White;
		}

		public Dictionary<string, string> GetArgDictionary()
		{
			Dictionary<string, string> ArgMap = new Dictionary<string, string>();
			foreach (EventArgVal evt in Arguments)
			{
				if (!ArgMap.ContainsKey(evt.Arg))
				{
					ArgMap.Add(evt.Arg, evt.Val);
				}
				else
				{
					ArgMap[evt.Arg] = evt.Val;
				}
			}

			return ArgMap;
		}

		public void SetArgDictionary(Dictionary<string, string> ArgMap)
		{
			Arguments.Clear();
			foreach (KeyValuePair<string, string> ArgValPair in ArgMap)
			{
				Arguments.Add(new EventArgVal(ArgValPair.Key, ArgValPair.Value));
			}
		}

		private void ctrlBtnEventEditorApply_Click(object sender, RoutedEventArgs e)
		{
			DialogResult = true;
			Close();
		}

		private void ctrlBtnEventEditorCancel_Click(object sender, RoutedEventArgs e)
		{
			DialogResult = false;
			Close();
		}

		private void ctrlBtnEventEditorArgAdd_Click(object sender, RoutedEventArgs e)
		{
			Arguments.Add(new EventArgVal(ctrlTextEventArgument.Text, ctrlTextEventValue.Text));
		}

		private void ctrlBtnEventEditorArgDel_Click(object sender, RoutedEventArgs e)
		{
			if (Arguments.Count > 0)
			{
				Arguments.RemoveAt(Arguments.Count - 1);
			}
		}
	}
}
