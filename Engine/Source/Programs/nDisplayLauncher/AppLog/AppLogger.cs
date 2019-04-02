// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;


namespace nDisplayLauncher.Log
{
	public class AppLogger : INotifyPropertyChanged
	{
		private AppLogger()
		{

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

		public event PropertyChangedEventHandler PropertyChanged;
		protected void OnNotifyPropertyChanged(string propertyName)
		{
			if (PropertyChanged != null)
			{
				PropertyChanged(this, new PropertyChangedEventArgs(propertyName));
			}
		}

		//Set property with OnNotifyPropertyChanged call
		protected void Set<T>(ref T field, T newValue, string propertyName)
		{
			field = newValue;
			OnNotifyPropertyChanged(propertyName);
		}


		private string _LogStr;
		public string LogStr
		{
			get { return _LogStr; }
			set { Set(ref _LogStr, value, "LogStr"); }
		}

		[MethodImpl(MethodImplOptions.Synchronized)]
		public static void CleanLog()
		{
			Instance.LogStr = DateTime.Now.ToString() + System.Environment.NewLine;
		}

		[MethodImpl(MethodImplOptions.Synchronized)]
		private static void Add(string text)
		{
			Instance.LogStr += DateTime.Now.ToString() + ":  " + text + System.Environment.NewLine;
		}

		public static void Log(string text)
		{
			Add(text);
		}
	}
}
