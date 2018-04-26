// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSyncLauncher
{
	partial class UpdateErrorWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(UpdateErrorWindow));
			this.ServerTextBox = new System.Windows.Forms.TextBox();
			this.DepotPathTextBox = new System.Windows.Forms.TextBox();
			this.label1 = new System.Windows.Forms.Label();
			this.DepotPathLabel = new System.Windows.Forms.Label();
			this.label2 = new System.Windows.Forms.Label();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.RetryBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.ViewLogBtn = new System.Windows.Forms.Button();
			this.groupBox1.SuspendLayout();
			this.SuspendLayout();
			// 
			// ServerTextBox
			// 
			this.ServerTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ServerTextBox.Location = new System.Drawing.Point(106, 26);
			this.ServerTextBox.Name = "ServerTextBox";
			this.ServerTextBox.Size = new System.Drawing.Size(664, 23);
			this.ServerTextBox.TabIndex = 0;
			// 
			// DepotPathTextBox
			// 
			this.DepotPathTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.DepotPathTextBox.Location = new System.Drawing.Point(106, 55);
			this.DepotPathTextBox.Name = "DepotPathTextBox";
			this.DepotPathTextBox.Size = new System.Drawing.Size(664, 23);
			this.DepotPathTextBox.TabIndex = 1;
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(28, 29);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(42, 15);
			this.label1.TabIndex = 3;
			this.label1.Text = "Server:";
			// 
			// DepotPathLabel
			// 
			this.DepotPathLabel.AutoSize = true;
			this.DepotPathLabel.Location = new System.Drawing.Point(28, 58);
			this.DepotPathLabel.Name = "DepotPathLabel";
			this.DepotPathLabel.Size = new System.Drawing.Size(69, 15);
			this.DepotPathLabel.TabIndex = 5;
			this.DepotPathLabel.Text = "Depot Path:";
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(12, 19);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(538, 15);
			this.label2.TabIndex = 6;
			this.label2.Text = "UnrealGameSync could not be synced from Perforce. Verify that your connection set" +
    "tings are correct.";
			// 
			// groupBox1
			// 
			this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox1.Controls.Add(this.DepotPathTextBox);
			this.groupBox1.Controls.Add(this.ServerTextBox);
			this.groupBox1.Controls.Add(this.DepotPathLabel);
			this.groupBox1.Controls.Add(this.label1);
			this.groupBox1.Location = new System.Drawing.Point(25, 49);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(803, 98);
			this.groupBox1.TabIndex = 7;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = "Connection Settings";
			// 
			// RetryBtn
			// 
			this.RetryBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.RetryBtn.Location = new System.Drawing.Point(751, 163);
			this.RetryBtn.Name = "RetryBtn";
			this.RetryBtn.Size = new System.Drawing.Size(89, 27);
			this.RetryBtn.TabIndex = 8;
			this.RetryBtn.Text = "Retry";
			this.RetryBtn.UseVisualStyleBackColor = true;
			this.RetryBtn.Click += new System.EventHandler(this.RetryBtn_Click);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(656, 163);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(89, 27);
			this.CancelBtn.TabIndex = 9;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			this.CancelBtn.Click += new System.EventHandler(this.CancelBtn_Click);
			// 
			// ViewLogBtn
			// 
			this.ViewLogBtn.Location = new System.Drawing.Point(12, 163);
			this.ViewLogBtn.Name = "ViewLogBtn";
			this.ViewLogBtn.Size = new System.Drawing.Size(87, 27);
			this.ViewLogBtn.TabIndex = 10;
			this.ViewLogBtn.Text = "View Log";
			this.ViewLogBtn.UseVisualStyleBackColor = true;
			this.ViewLogBtn.Click += new System.EventHandler(this.ViewLogBtn_Click);
			// 
			// UpdateErrorWindow
			// 
			this.AcceptButton = this.RetryBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(852, 202);
			this.Controls.Add(this.ViewLogBtn);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.RetryBtn);
			this.Controls.Add(this.groupBox1);
			this.Controls.Add(this.label2);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Name = "UpdateErrorWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "UnrealGameSync Launcher";
			this.groupBox1.ResumeLayout(false);
			this.groupBox1.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.TextBox ServerTextBox;
		private System.Windows.Forms.TextBox DepotPathTextBox;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label DepotPathLabel;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.Button RetryBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Button ViewLogBtn;
	}
}