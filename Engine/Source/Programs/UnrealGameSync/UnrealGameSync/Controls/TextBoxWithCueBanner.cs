// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	class TextBoxWithCueBanner : TextBox
	{
		private const int EM_SETCUEBANNER = 0x1501;

		[DllImport("user32.dll", CharSet = CharSet.Unicode)]
		private static extern IntPtr SendMessage(IntPtr hWnd, int Msg, IntPtr Wparam, string Lparam);

		private string CueBannerValue;

		public string CueBanner
		{
			get { return CueBannerValue; }
			set { CueBannerValue = value; UpdateCueBanner(); }
		}

		private void UpdateCueBanner()
		{
			if(IsHandleCreated)
			{
				SendMessage(Handle, EM_SETCUEBANNER, (IntPtr)1, CueBannerValue);
			}
		}

		protected override void OnHandleCreated(EventArgs e)
		{
			base.OnHandleCreated(e);
			UpdateCueBanner();
		}
	}
}
