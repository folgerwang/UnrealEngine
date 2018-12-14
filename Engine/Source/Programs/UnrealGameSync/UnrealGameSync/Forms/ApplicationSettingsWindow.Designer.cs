// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class ApplicationSettingsWindow
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.label1 = new System.Windows.Forms.Label();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
			this.UserNameTextBox = new UnrealGameSync.TextBoxWithCueBanner();
			this.ServerTextBox = new UnrealGameSync.TextBoxWithCueBanner();
			this.label3 = new System.Windows.Forms.Label();
			this.label4 = new System.Windows.Forms.Label();
			this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
			this.UseUnstableBuildCheckBox = new System.Windows.Forms.CheckBox();
			this.DepotPathTextBox = new UnrealGameSync.TextBoxWithCueBanner();
			this.OkBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.ViewLogBtn = new System.Windows.Forms.Button();
			this.groupBox2 = new System.Windows.Forms.GroupBox();
			this.tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
			this.KeepInTrayCheckBox = new System.Windows.Forms.CheckBox();
			this.AutomaticallyRunAtStartupCheckBox = new System.Windows.Forms.CheckBox();
			this.groupBox1.SuspendLayout();
			this.tableLayoutPanel1.SuspendLayout();
			this.tableLayoutPanel2.SuspendLayout();
			this.groupBox2.SuspendLayout();
			this.tableLayoutPanel3.SuspendLayout();
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(3, 8);
			this.label1.Margin = new System.Windows.Forms.Padding(3, 0, 10, 0);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(42, 15);
			this.label1.TabIndex = 0;
			this.label1.Text = "Server:";
			// 
			// groupBox1
			// 
			this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox1.Controls.Add(this.tableLayoutPanel1);
			this.groupBox1.Location = new System.Drawing.Point(17, 106);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(822, 143);
			this.groupBox1.TabIndex = 1;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = "Auto-Update";
			// 
			// tableLayoutPanel1
			// 
			this.tableLayoutPanel1.AutoSize = true;
			this.tableLayoutPanel1.ColumnCount = 2;
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel1.Controls.Add(this.UserNameTextBox, 1, 1);
			this.tableLayoutPanel1.Controls.Add(this.label1, 0, 0);
			this.tableLayoutPanel1.Controls.Add(this.ServerTextBox, 1, 0);
			this.tableLayoutPanel1.Controls.Add(this.label3, 0, 1);
			this.tableLayoutPanel1.Controls.Add(this.label4, 0, 2);
			this.tableLayoutPanel1.Controls.Add(this.tableLayoutPanel2, 1, 2);
			this.tableLayoutPanel1.Location = new System.Drawing.Point(22, 27);
			this.tableLayoutPanel1.Name = "tableLayoutPanel1";
			this.tableLayoutPanel1.RowCount = 3;
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 33.33333F));
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 33.33333F));
			this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 33.33333F));
			this.tableLayoutPanel1.Size = new System.Drawing.Size(787, 97);
			this.tableLayoutPanel1.TabIndex = 0;
			// 
			// UserNameTextBox
			// 
			this.UserNameTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.UserNameTextBox.CueBanner = "Default";
			this.UserNameTextBox.Location = new System.Drawing.Point(85, 36);
			this.UserNameTextBox.Name = "UserNameTextBox";
			this.UserNameTextBox.Size = new System.Drawing.Size(699, 23);
			this.UserNameTextBox.TabIndex = 1;
			// 
			// ServerTextBox
			// 
			this.ServerTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.ServerTextBox.CueBanner = "Default";
			this.ServerTextBox.Location = new System.Drawing.Point(85, 4);
			this.ServerTextBox.Name = "ServerTextBox";
			this.ServerTextBox.Size = new System.Drawing.Size(699, 23);
			this.ServerTextBox.TabIndex = 0;
			// 
			// label3
			// 
			this.label3.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(3, 40);
			this.label3.Margin = new System.Windows.Forms.Padding(3, 0, 10, 0);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(33, 15);
			this.label3.TabIndex = 2;
			this.label3.Text = "User:";
			// 
			// label4
			// 
			this.label4.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.label4.AutoSize = true;
			this.label4.Location = new System.Drawing.Point(3, 73);
			this.label4.Margin = new System.Windows.Forms.Padding(3, 0, 10, 0);
			this.label4.Name = "label4";
			this.label4.Size = new System.Drawing.Size(69, 15);
			this.label4.TabIndex = 4;
			this.label4.Text = "Depot Path:";
			// 
			// tableLayoutPanel2
			// 
			this.tableLayoutPanel2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
			this.tableLayoutPanel2.AutoSize = true;
			this.tableLayoutPanel2.ColumnCount = 2;
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
			this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.tableLayoutPanel2.Controls.Add(this.UseUnstableBuildCheckBox, 1, 0);
			this.tableLayoutPanel2.Controls.Add(this.DepotPathTextBox, 0, 0);
			this.tableLayoutPanel2.Location = new System.Drawing.Point(82, 66);
			this.tableLayoutPanel2.Margin = new System.Windows.Forms.Padding(0);
			this.tableLayoutPanel2.Name = "tableLayoutPanel2";
			this.tableLayoutPanel2.RowCount = 1;
			this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle());
			this.tableLayoutPanel2.Size = new System.Drawing.Size(705, 29);
			this.tableLayoutPanel2.TabIndex = 5;
			// 
			// UseUnstableBuildCheckBox
			// 
			this.UseUnstableBuildCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Right;
			this.UseUnstableBuildCheckBox.AutoSize = true;
			this.UseUnstableBuildCheckBox.Location = new System.Drawing.Point(578, 5);
			this.UseUnstableBuildCheckBox.Margin = new System.Windows.Forms.Padding(10, 3, 3, 3);
			this.UseUnstableBuildCheckBox.Name = "UseUnstableBuildCheckBox";
			this.UseUnstableBuildCheckBox.Size = new System.Drawing.Size(124, 19);
			this.UseUnstableBuildCheckBox.TabIndex = 1;
			this.UseUnstableBuildCheckBox.Text = "Use Unstable Build";
			this.UseUnstableBuildCheckBox.UseVisualStyleBackColor = true;
			// 
			// DepotPathTextBox
			// 
			this.DepotPathTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.DepotPathTextBox.CueBanner = null;
			this.DepotPathTextBox.Location = new System.Drawing.Point(3, 3);
			this.DepotPathTextBox.Name = "DepotPathTextBox";
			this.DepotPathTextBox.Size = new System.Drawing.Size(562, 23);
			this.DepotPathTextBox.TabIndex = 0;
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.Location = new System.Drawing.Point(661, 261);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(89, 27);
			this.OkBtn.TabIndex = 2;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			this.OkBtn.Click += new System.EventHandler(this.OkBtn_Click);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(756, 261);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(89, 27);
			this.CancelBtn.TabIndex = 3;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			this.CancelBtn.Click += new System.EventHandler(this.CancelBtn_Click);
			// 
			// ViewLogBtn
			// 
			this.ViewLogBtn.Location = new System.Drawing.Point(0, 0);
			this.ViewLogBtn.Name = "ViewLogBtn";
			this.ViewLogBtn.Size = new System.Drawing.Size(75, 23);
			this.ViewLogBtn.TabIndex = 0;
			// 
			// groupBox2
			// 
			this.groupBox2.Controls.Add(this.tableLayoutPanel3);
			this.groupBox2.Location = new System.Drawing.Point(17, 12);
			this.groupBox2.Name = "groupBox2";
			this.groupBox2.Size = new System.Drawing.Size(822, 88);
			this.groupBox2.TabIndex = 0;
			this.groupBox2.TabStop = false;
			this.groupBox2.Text = "Startup and shutdown";
			// 
			// tableLayoutPanel3
			// 
			this.tableLayoutPanel3.ColumnCount = 1;
			this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 50F));
			this.tableLayoutPanel3.Controls.Add(this.KeepInTrayCheckBox, 0, 1);
			this.tableLayoutPanel3.Controls.Add(this.AutomaticallyRunAtStartupCheckBox, 0, 0);
			this.tableLayoutPanel3.Location = new System.Drawing.Point(22, 24);
			this.tableLayoutPanel3.Name = "tableLayoutPanel3";
			this.tableLayoutPanel3.RowCount = 2;
			this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
			this.tableLayoutPanel3.Size = new System.Drawing.Size(787, 52);
			this.tableLayoutPanel3.TabIndex = 6;
			// 
			// KeepInTrayCheckBox
			// 
			this.KeepInTrayCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.KeepInTrayCheckBox.AutoSize = true;
			this.KeepInTrayCheckBox.Location = new System.Drawing.Point(3, 29);
			this.KeepInTrayCheckBox.Name = "KeepInTrayCheckBox";
			this.KeepInTrayCheckBox.Size = new System.Drawing.Size(377, 19);
			this.KeepInTrayCheckBox.TabIndex = 1;
			this.KeepInTrayCheckBox.Text = "Keep program running in the system notification area when closed";
			this.KeepInTrayCheckBox.UseVisualStyleBackColor = true;
			// 
			// AutomaticallyRunAtStartupCheckBox
			// 
			this.AutomaticallyRunAtStartupCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
			this.AutomaticallyRunAtStartupCheckBox.AutoSize = true;
			this.AutomaticallyRunAtStartupCheckBox.Location = new System.Drawing.Point(3, 3);
			this.AutomaticallyRunAtStartupCheckBox.Name = "AutomaticallyRunAtStartupCheckBox";
			this.AutomaticallyRunAtStartupCheckBox.Size = new System.Drawing.Size(174, 19);
			this.AutomaticallyRunAtStartupCheckBox.TabIndex = 0;
			this.AutomaticallyRunAtStartupCheckBox.Text = "Automatically run at startup";
			this.AutomaticallyRunAtStartupCheckBox.UseVisualStyleBackColor = true;
			// 
			// ApplicationSettingsWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(857, 300);
			this.Controls.Add(this.groupBox2);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.OkBtn);
			this.Controls.Add(this.groupBox1);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = global::UnrealGameSync.Properties.Resources.Icon;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "ApplicationSettingsWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "Application Settings";
			this.groupBox1.ResumeLayout(false);
			this.groupBox1.PerformLayout();
			this.tableLayoutPanel1.ResumeLayout(false);
			this.tableLayoutPanel1.PerformLayout();
			this.tableLayoutPanel2.ResumeLayout(false);
			this.tableLayoutPanel2.PerformLayout();
			this.groupBox2.ResumeLayout(false);
			this.tableLayoutPanel3.ResumeLayout(false);
			this.tableLayoutPanel3.PerformLayout();
			this.ResumeLayout(false);

		}

		#endregion
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Button ViewLogBtn;
		private System.Windows.Forms.Label label3;
		public System.Windows.Forms.CheckBox UseUnstableBuildCheckBox;
		private TextBoxWithCueBanner DepotPathTextBox;
		private System.Windows.Forms.Label label4;
		private TextBoxWithCueBanner ServerTextBox;
		private TextBoxWithCueBanner UserNameTextBox;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
		private System.Windows.Forms.GroupBox groupBox2;
		private System.Windows.Forms.CheckBox AutomaticallyRunAtStartupCheckBox;
		private System.Windows.Forms.CheckBox KeepInTrayCheckBox;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
	}
}