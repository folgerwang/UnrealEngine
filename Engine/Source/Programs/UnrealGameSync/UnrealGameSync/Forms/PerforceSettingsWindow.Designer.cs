// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class PerforceSettingsWindow
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
			this.ServerTextBox = new UnrealGameSync.TextBoxWithCueBanner();
			this.label1 = new System.Windows.Forms.Label();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.UserNameTextBox = new UnrealGameSync.TextBoxWithCueBanner();
			this.label3 = new System.Windows.Forms.Label();
			this.RetryBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.ViewLogBtn = new System.Windows.Forms.Button();
			this.groupBox2 = new System.Windows.Forms.GroupBox();
			this.UseUnstableBuildCheckBox = new System.Windows.Forms.CheckBox();
			this.DepotPathTextBox = new UnrealGameSync.TextBoxWithCueBanner();
			this.label4 = new System.Windows.Forms.Label();
			this.AdvancedBtn = new System.Windows.Forms.Button();
			this.groupBox1.SuspendLayout();
			this.groupBox2.SuspendLayout();
			this.SuspendLayout();
			// 
			// ServerTextBox
			// 
			this.ServerTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ServerTextBox.CueBanner = "Default";
			this.ServerTextBox.Location = new System.Drawing.Point(106, 29);
			this.ServerTextBox.Name = "ServerTextBox";
			this.ServerTextBox.Size = new System.Drawing.Size(692, 23);
			this.ServerTextBox.TabIndex = 1;
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(19, 32);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(42, 15);
			this.label1.TabIndex = 0;
			this.label1.Text = "Server:";
			// 
			// groupBox1
			// 
			this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox1.Controls.Add(this.UserNameTextBox);
			this.groupBox1.Controls.Add(this.label3);
			this.groupBox1.Controls.Add(this.ServerTextBox);
			this.groupBox1.Controls.Add(this.label1);
			this.groupBox1.Location = new System.Drawing.Point(17, 19);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(822, 103);
			this.groupBox1.TabIndex = 1;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = "Default Connection";
			// 
			// UserNameTextBox
			// 
			this.UserNameTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.UserNameTextBox.CueBanner = "Default";
			this.UserNameTextBox.Location = new System.Drawing.Point(106, 58);
			this.UserNameTextBox.Name = "UserNameTextBox";
			this.UserNameTextBox.Size = new System.Drawing.Size(692, 23);
			this.UserNameTextBox.TabIndex = 3;
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(19, 61);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(33, 15);
			this.label3.TabIndex = 2;
			this.label3.Text = "User:";
			// 
			// RetryBtn
			// 
			this.RetryBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.RetryBtn.Location = new System.Drawing.Point(756, 225);
			this.RetryBtn.Name = "RetryBtn";
			this.RetryBtn.Size = new System.Drawing.Size(89, 29);
			this.RetryBtn.TabIndex = 4;
			this.RetryBtn.Text = "Ok";
			this.RetryBtn.UseVisualStyleBackColor = true;
			this.RetryBtn.Click += new System.EventHandler(this.OkBtn_Click);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(661, 225);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(89, 29);
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
			this.groupBox2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox2.Controls.Add(this.UseUnstableBuildCheckBox);
			this.groupBox2.Controls.Add(this.DepotPathTextBox);
			this.groupBox2.Controls.Add(this.label4);
			this.groupBox2.Location = new System.Drawing.Point(17, 132);
			this.groupBox2.Name = "groupBox2";
			this.groupBox2.Size = new System.Drawing.Size(822, 73);
			this.groupBox2.TabIndex = 5;
			this.groupBox2.TabStop = false;
			this.groupBox2.Text = "Auto-Update";
			// 
			// UseUnstableBuildCheckBox
			// 
			this.UseUnstableBuildCheckBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.UseUnstableBuildCheckBox.AutoSize = true;
			this.UseUnstableBuildCheckBox.Location = new System.Drawing.Point(674, 31);
			this.UseUnstableBuildCheckBox.Name = "UseUnstableBuildCheckBox";
			this.UseUnstableBuildCheckBox.Size = new System.Drawing.Size(124, 19);
			this.UseUnstableBuildCheckBox.TabIndex = 6;
			this.UseUnstableBuildCheckBox.Text = "Use Unstable Build";
			this.UseUnstableBuildCheckBox.UseVisualStyleBackColor = true;
			// 
			// DepotPathTextBox
			// 
			this.DepotPathTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.DepotPathTextBox.Location = new System.Drawing.Point(106, 29);
			this.DepotPathTextBox.Name = "DepotPathTextBox";
			this.DepotPathTextBox.Size = new System.Drawing.Size(553, 23);
			this.DepotPathTextBox.TabIndex = 5;
			// 
			// label4
			// 
			this.label4.AutoSize = true;
			this.label4.Location = new System.Drawing.Point(19, 32);
			this.label4.Name = "label4";
			this.label4.Size = new System.Drawing.Size(69, 15);
			this.label4.TabIndex = 4;
			this.label4.Text = "Depot Path:";
			// 
			// AdvancedBtn
			// 
			this.AdvancedBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.AdvancedBtn.Location = new System.Drawing.Point(12, 225);
			this.AdvancedBtn.Name = "AdvancedBtn";
			this.AdvancedBtn.Size = new System.Drawing.Size(115, 29);
			this.AdvancedBtn.TabIndex = 6;
			this.AdvancedBtn.Text = "Advanced...";
			this.AdvancedBtn.UseVisualStyleBackColor = true;
			this.AdvancedBtn.Click += new System.EventHandler(this.AdvancedBtn_Click);
			// 
			// PerforceSettingsWindow
			// 
			this.AcceptButton = this.RetryBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(857, 266);
			this.Controls.Add(this.AdvancedBtn);
			this.Controls.Add(this.groupBox2);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.RetryBtn);
			this.Controls.Add(this.groupBox1);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = global::UnrealGameSync.Properties.Resources.Icon;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "PerforceSettingsWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "Perforce Settings";
			this.groupBox1.ResumeLayout(false);
			this.groupBox1.PerformLayout();
			this.groupBox2.ResumeLayout(false);
			this.groupBox2.PerformLayout();
			this.ResumeLayout(false);

		}

		#endregion
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.Button RetryBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Button ViewLogBtn;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.GroupBox groupBox2;
		private System.Windows.Forms.CheckBox UseUnstableBuildCheckBox;
		private TextBoxWithCueBanner DepotPathTextBox;
		private System.Windows.Forms.Label label4;
		private System.Windows.Forms.Button AdvancedBtn;
		private TextBoxWithCueBanner ServerTextBox;
		private TextBoxWithCueBanner UserNameTextBox;
	}
}