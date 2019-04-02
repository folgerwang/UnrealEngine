// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Globalization;
using System.Windows.Data;

namespace nDisplayLauncher.ValueConversion
{
	public class SizeConverter : IValueConverter
	{
		public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
		{
			double newSize = (System.Convert.ToDouble(value) - System.Convert.ToDouble(parameter));
			if (newSize < 0)
			{
				newSize = 32;
			}
			return newSize;
		}

		public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
		{
			return true;
		}
	}
}
