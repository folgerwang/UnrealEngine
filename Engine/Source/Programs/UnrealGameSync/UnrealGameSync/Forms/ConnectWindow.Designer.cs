namespace UnrealGameSync
{
	partial class ConnectWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ConnectWindow));
			this.UserNameLabel = new System.Windows.Forms.Label();
			this.UserNameTextBox = new System.Windows.Forms.TextBox();
			this.UseDefaultConnectionSettings = new System.Windows.Forms.CheckBox();
			this.ServerAndPortTextBox = new System.Windows.Forms.TextBox();
			this.ServerAndPortLabel = new System.Windows.Forms.Label();
			this.OkBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// UserNameLabel
			// 
			this.UserNameLabel.AutoSize = true;
			this.UserNameLabel.Location = new System.Drawing.Point(17, 79);
			this.UserNameLabel.Name = "UserNameLabel";
			this.UserNameLabel.Size = new System.Drawing.Size(66, 15);
			this.UserNameLabel.TabIndex = 6;
			this.UserNameLabel.Text = "User name:";
			// 
			// UserNameTextBox
			// 
			this.UserNameTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.UserNameTextBox.Location = new System.Drawing.Point(104, 76);
			this.UserNameTextBox.Name = "UserNameTextBox";
			this.UserNameTextBox.Size = new System.Drawing.Size(475, 23);
			this.UserNameTextBox.TabIndex = 5;
			// 
			// UseDefaultConnectionSettings
			// 
			this.UseDefaultConnectionSettings.AutoSize = true;
			this.UseDefaultConnectionSettings.Location = new System.Drawing.Point(20, 16);
			this.UseDefaultConnectionSettings.Name = "UseDefaultConnectionSettings";
			this.UseDefaultConnectionSettings.Size = new System.Drawing.Size(192, 19);
			this.UseDefaultConnectionSettings.TabIndex = 4;
			this.UseDefaultConnectionSettings.Text = "Use default connection settings";
			this.UseDefaultConnectionSettings.UseVisualStyleBackColor = true;
			this.UseDefaultConnectionSettings.CheckedChanged += new System.EventHandler(this.UseCustomSettings_CheckedChanged);
			// 
			// ServerAndPortTextBox
			// 
			this.ServerAndPortTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ServerAndPortTextBox.Location = new System.Drawing.Point(104, 47);
			this.ServerAndPortTextBox.Name = "ServerAndPortTextBox";
			this.ServerAndPortTextBox.Size = new System.Drawing.Size(475, 23);
			this.ServerAndPortTextBox.TabIndex = 3;
			// 
			// ServerAndPortLabel
			// 
			this.ServerAndPortLabel.AutoSize = true;
			this.ServerAndPortLabel.Location = new System.Drawing.Point(17, 50);
			this.ServerAndPortLabel.Name = "ServerAndPortLabel";
			this.ServerAndPortLabel.Size = new System.Drawing.Size(42, 15);
			this.ServerAndPortLabel.TabIndex = 1;
			this.ServerAndPortLabel.Text = "Server:";
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.Location = new System.Drawing.Point(480, 114);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(99, 27);
			this.OkBtn.TabIndex = 2;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			this.OkBtn.Click += new System.EventHandler(this.OkBtn_Click);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(375, 114);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(99, 27);
			this.CancelBtn.TabIndex = 3;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			this.CancelBtn.Click += new System.EventHandler(this.CancelBtn_Click);
			// 
			// ConnectWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(591, 156);
			this.Controls.Add(this.UserNameLabel);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.UserNameTextBox);
			this.Controls.Add(this.OkBtn);
			this.Controls.Add(this.UseDefaultConnectionSettings);
			this.Controls.Add(this.ServerAndPortTextBox);
			this.Controls.Add(this.ServerAndPortLabel);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "ConnectWindow";
			this.ShowInTaskbar = false;
			this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Connection Settings";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion
		private System.Windows.Forms.Label UserNameLabel;
		private System.Windows.Forms.TextBox UserNameTextBox;
		private System.Windows.Forms.TextBox ServerAndPortTextBox;
		private System.Windows.Forms.Label ServerAndPortLabel;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.CheckBox UseDefaultConnectionSettings;
	}
}