// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;


namespace nDisplayLauncher
{
	public class AppLogger : INotifyPropertyChanged
	{
		private AppLogger()
		{

		}

		//Implementation of INotifyPropertyChanged method for TwoWay binding
		public event PropertyChangedEventHandler PropertyChanged;

		protected void OnNotifyPropertyChanged(string propertyName)
		{
			if (PropertyChanged != null)
				PropertyChanged(this, new PropertyChangedEventArgs(propertyName));
		}

		//Set property with OnNotifyPropertyChanged call
		protected void Set<T>(ref T field, T newValue, string propertyName)
		{
			field = newValue;
			OnNotifyPropertyChanged(propertyName);
		}

		private static AppLogger _Instance;
		public static AppLogger Instance
		{
			get
			{
				if (_Instance == null)
				{
					_Instance = new AppLogger();
				}
				return _Instance;
			}
		}

		private string _Log;
		public string Log
		{
			get
			{
				if (_Log == null)
				{
					_Log = string.Empty;
				}
				return _Log;
			}
			set { Set(ref _Log, value, "Log"); }
		}

		public static void CleanLog()
		{
			Instance.Log = DateTime.Now.ToString() + System.Environment.NewLine;
		}

		public static void Add(string text)
		{
			Instance.Log = Instance.Log + DateTime.Now.ToString() + ":  " + text + System.Environment.NewLine;
		}
	}
}
