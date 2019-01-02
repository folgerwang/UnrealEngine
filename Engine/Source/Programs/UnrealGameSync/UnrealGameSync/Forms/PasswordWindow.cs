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

namespace UnrealGameSync
{
	public partial class PasswordWindow : Form
	{
		public string Password;

		public PasswordWindow(string Prompt, string Password)
		{
			InitializeComponent();

			this.Password = Password;

			PromptLabel.Text = Prompt;
			PasswordTextBox.Text = Password;
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			Password = PasswordTextBox.Text;
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{

		}
	}
}
