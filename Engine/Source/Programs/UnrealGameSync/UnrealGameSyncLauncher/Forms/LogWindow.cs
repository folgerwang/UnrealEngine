// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSyncLauncher
{
	public partial class LogWindow : Form
	{
		public LogWindow(string Text)
		{
			InitializeComponent();

			LogTextBox.Text = Text;
			LogTextBox.Select(Text.Length, 0);
		}
	}
}
