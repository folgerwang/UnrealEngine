// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	interface IModalTask
	{
		bool Run(out string ErrorMessage);
	}

	public enum ModalTaskResult
	{
		Succeeded,
		Failed,
		Aborted,
	}

	static class ModalTask
	{
		public static ModalTaskResult Execute(IWin32Window Owner, IModalTask Task, string InTitle, string InMessage, out string ErrorMessage)
		{
			ModalTaskWindow Window = new ModalTaskWindow(Task, InTitle, InMessage, FormStartPosition.CenterParent);
			Window.Complete += () => Window.Close();
			Window.ShowDialog(Owner);
			ErrorMessage = Window.ErrorMessage;
			return Window.Result;
		}
	}
}
