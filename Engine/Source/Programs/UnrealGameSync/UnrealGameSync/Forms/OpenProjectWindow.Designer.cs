namespace UnrealGameSync
{
	partial class OpenProjectWindow
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
			this.WorkspaceNameLabel = new System.Windows.Forms.Label();
			this.WorkspacePathLabel = new System.Windows.Forms.Label();
			this.groupBox2 = new System.Windows.Forms.GroupBox();
			this.WorkspacePathBrowseBtn = new System.Windows.Forms.Button();
			this.WorkspacePathTextBox = new System.Windows.Forms.TextBox();
			this.WorkspaceNameBrowseBtn = new System.Windows.Forms.Button();
			this.WorkspaceNameNewBtn = new System.Windows.Forms.Button();
			this.WorkspaceNameTextBox = new System.Windows.Forms.TextBox();
			this.WorkspaceRadioBtn = new System.Windows.Forms.RadioButton();
			this.ServerLabel = new System.Windows.Forms.Label();
			this.ChangeLink = new System.Windows.Forms.LinkLabel();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.OkBtn = new System.Windows.Forms.Button();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.LocalFileBrowseBtn = new System.Windows.Forms.Button();
			this.LocalFileTextBox = new System.Windows.Forms.TextBox();
			this.LocalFileLabel = new System.Windows.Forms.Label();
			this.LocalFileRadioBtn = new System.Windows.Forms.RadioButton();
			this.groupBox2.SuspendLayout();
			this.groupBox1.SuspendLayout();
			this.SuspendLayout();
			// 
			// WorkspaceNameLabel
			// 
			this.WorkspaceNameLabel.AutoSize = true;
			this.WorkspaceNameLabel.Location = new System.Drawing.Point(23, 36);
			this.WorkspaceNameLabel.Name = "WorkspaceNameLabel";
			this.WorkspaceNameLabel.Size = new System.Drawing.Size(42, 15);
			this.WorkspaceNameLabel.TabIndex = 1;
			this.WorkspaceNameLabel.Text = "Name:";
			// 
			// WorkspacePathLabel
			// 
			this.WorkspacePathLabel.AutoSize = true;
			this.WorkspacePathLabel.Location = new System.Drawing.Point(23, 66);
			this.WorkspacePathLabel.Name = "WorkspacePathLabel";
			this.WorkspacePathLabel.Size = new System.Drawing.Size(34, 15);
			this.WorkspacePathLabel.TabIndex = 5;
			this.WorkspacePathLabel.Text = "Path:";
			// 
			// groupBox2
			// 
			this.groupBox2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox2.Controls.Add(this.WorkspacePathBrowseBtn);
			this.groupBox2.Controls.Add(this.WorkspacePathTextBox);
			this.groupBox2.Controls.Add(this.WorkspacePathLabel);
			this.groupBox2.Controls.Add(this.WorkspaceNameBrowseBtn);
			this.groupBox2.Controls.Add(this.WorkspaceNameNewBtn);
			this.groupBox2.Controls.Add(this.WorkspaceNameTextBox);
			this.groupBox2.Controls.Add(this.WorkspaceNameLabel);
			this.groupBox2.Location = new System.Drawing.Point(22, 142);
			this.groupBox2.Name = "groupBox2";
			this.groupBox2.Size = new System.Drawing.Size(851, 110);
			this.groupBox2.TabIndex = 1;
			this.groupBox2.TabStop = false;
			// 
			// WorkspacePathBrowseBtn
			// 
			this.WorkspacePathBrowseBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspacePathBrowseBtn.Location = new System.Drawing.Point(736, 61);
			this.WorkspacePathBrowseBtn.Name = "WorkspacePathBrowseBtn";
			this.WorkspacePathBrowseBtn.Size = new System.Drawing.Size(94, 27);
			this.WorkspacePathBrowseBtn.TabIndex = 7;
			this.WorkspacePathBrowseBtn.Text = "Browse...";
			this.WorkspacePathBrowseBtn.UseVisualStyleBackColor = true;
			this.WorkspacePathBrowseBtn.Click += new System.EventHandler(this.WorkspacePathBrowseBtn_Click);
			// 
			// WorkspacePathTextBox
			// 
			this.WorkspacePathTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspacePathTextBox.Location = new System.Drawing.Point(83, 63);
			this.WorkspacePathTextBox.Name = "WorkspacePathTextBox";
			this.WorkspacePathTextBox.Size = new System.Drawing.Size(647, 23);
			this.WorkspacePathTextBox.TabIndex = 6;
			this.WorkspacePathTextBox.TextChanged += new System.EventHandler(this.WorkspacePathTextBox_TextChanged);
			this.WorkspacePathTextBox.Enter += new System.EventHandler(this.WorkspacePathTextBox_Enter);
			// 
			// WorkspaceNameBrowseBtn
			// 
			this.WorkspaceNameBrowseBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspaceNameBrowseBtn.Location = new System.Drawing.Point(736, 30);
			this.WorkspaceNameBrowseBtn.Name = "WorkspaceNameBrowseBtn";
			this.WorkspaceNameBrowseBtn.Size = new System.Drawing.Size(94, 27);
			this.WorkspaceNameBrowseBtn.TabIndex = 4;
			this.WorkspaceNameBrowseBtn.Text = "Browse...";
			this.WorkspaceNameBrowseBtn.UseVisualStyleBackColor = true;
			this.WorkspaceNameBrowseBtn.Click += new System.EventHandler(this.WorkspaceBrowseBtn_Click);
			// 
			// WorkspaceNameNewBtn
			// 
			this.WorkspaceNameNewBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspaceNameNewBtn.Location = new System.Drawing.Point(641, 30);
			this.WorkspaceNameNewBtn.Name = "WorkspaceNameNewBtn";
			this.WorkspaceNameNewBtn.Size = new System.Drawing.Size(89, 27);
			this.WorkspaceNameNewBtn.TabIndex = 3;
			this.WorkspaceNameNewBtn.Text = "New...";
			this.WorkspaceNameNewBtn.UseVisualStyleBackColor = true;
			this.WorkspaceNameNewBtn.Click += new System.EventHandler(this.WorkspaceNewBtn_Click);
			// 
			// WorkspaceNameTextBox
			// 
			this.WorkspaceNameTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspaceNameTextBox.Location = new System.Drawing.Point(83, 33);
			this.WorkspaceNameTextBox.Name = "WorkspaceNameTextBox";
			this.WorkspaceNameTextBox.Size = new System.Drawing.Size(552, 23);
			this.WorkspaceNameTextBox.TabIndex = 2;
			this.WorkspaceNameTextBox.TextChanged += new System.EventHandler(this.WorkspaceNameTextBox_TextChanged);
			this.WorkspaceNameTextBox.Enter += new System.EventHandler(this.WorkspaceNameTextBox_Enter);
			// 
			// WorkspaceRadioBtn
			// 
			this.WorkspaceRadioBtn.AutoSize = true;
			this.WorkspaceRadioBtn.Location = new System.Drawing.Point(39, 140);
			this.WorkspaceRadioBtn.Name = "WorkspaceRadioBtn";
			this.WorkspaceRadioBtn.Size = new System.Drawing.Size(83, 19);
			this.WorkspaceRadioBtn.TabIndex = 8;
			this.WorkspaceRadioBtn.Text = "Workspace";
			this.WorkspaceRadioBtn.UseVisualStyleBackColor = true;
			this.WorkspaceRadioBtn.CheckedChanged += new System.EventHandler(this.WorkspaceRadioBtn_CheckedChanged);
			// 
			// ServerLabel
			// 
			this.ServerLabel.AutoSize = true;
			this.ServerLabel.Location = new System.Drawing.Point(19, 19);
			this.ServerLabel.Name = "ServerLabel";
			this.ServerLabel.Size = new System.Drawing.Size(331, 15);
			this.ServerLabel.TabIndex = 5;
			this.ServerLabel.Text = "Using default Perforce connection (perforce:1666, Ben.Marsh)";
			// 
			// ChangeLink
			// 
			this.ChangeLink.AutoSize = true;
			this.ChangeLink.Location = new System.Drawing.Point(356, 19);
			this.ChangeLink.Name = "ChangeLink";
			this.ChangeLink.Size = new System.Drawing.Size(57, 15);
			this.ChangeLink.TabIndex = 6;
			this.ChangeLink.TabStop = true;
			this.ChangeLink.Text = "Change...";
			this.ChangeLink.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.ChangeLink_LinkClicked);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(672, 266);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(98, 27);
			this.CancelBtn.TabIndex = 3;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.Location = new System.Drawing.Point(776, 266);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(97, 27);
			this.OkBtn.TabIndex = 4;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			this.OkBtn.Click += new System.EventHandler(this.OkBtn_Click);
			// 
			// groupBox1
			// 
			this.groupBox1.Controls.Add(this.LocalFileBrowseBtn);
			this.groupBox1.Controls.Add(this.LocalFileTextBox);
			this.groupBox1.Controls.Add(this.LocalFileLabel);
			this.groupBox1.Location = new System.Drawing.Point(22, 52);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(851, 84);
			this.groupBox1.TabIndex = 8;
			this.groupBox1.TabStop = false;
			// 
			// LocalFileBrowseBtn
			// 
			this.LocalFileBrowseBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.LocalFileBrowseBtn.Location = new System.Drawing.Point(736, 33);
			this.LocalFileBrowseBtn.Name = "LocalFileBrowseBtn";
			this.LocalFileBrowseBtn.Size = new System.Drawing.Size(94, 27);
			this.LocalFileBrowseBtn.TabIndex = 10;
			this.LocalFileBrowseBtn.Text = "Browse...";
			this.LocalFileBrowseBtn.UseVisualStyleBackColor = true;
			this.LocalFileBrowseBtn.Click += new System.EventHandler(this.LocalFileBrowseBtn_Click);
			// 
			// LocalFileTextBox
			// 
			this.LocalFileTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.LocalFileTextBox.Location = new System.Drawing.Point(84, 35);
			this.LocalFileTextBox.Name = "LocalFileTextBox";
			this.LocalFileTextBox.Size = new System.Drawing.Size(646, 23);
			this.LocalFileTextBox.TabIndex = 9;
			this.LocalFileTextBox.TextChanged += new System.EventHandler(this.LocalFileTextBox_TextChanged);
			this.LocalFileTextBox.Enter += new System.EventHandler(this.LocalFileTextBox_Enter);
			// 
			// LocalFileLabel
			// 
			this.LocalFileLabel.AutoSize = true;
			this.LocalFileLabel.Location = new System.Drawing.Point(23, 38);
			this.LocalFileLabel.Name = "LocalFileLabel";
			this.LocalFileLabel.Size = new System.Drawing.Size(28, 15);
			this.LocalFileLabel.TabIndex = 8;
			this.LocalFileLabel.Text = "File:";
			// 
			// LocalFileRadioBtn
			// 
			this.LocalFileRadioBtn.AutoSize = true;
			this.LocalFileRadioBtn.Location = new System.Drawing.Point(40, 50);
			this.LocalFileRadioBtn.Name = "LocalFileRadioBtn";
			this.LocalFileRadioBtn.Size = new System.Drawing.Size(74, 19);
			this.LocalFileRadioBtn.TabIndex = 0;
			this.LocalFileRadioBtn.Text = "Local File";
			this.LocalFileRadioBtn.UseVisualStyleBackColor = true;
			this.LocalFileRadioBtn.CheckedChanged += new System.EventHandler(this.LocalFileRadioBtn_CheckedChanged);
			// 
			// OpenProjectWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(893, 305);
			this.Controls.Add(this.LocalFileRadioBtn);
			this.Controls.Add(this.WorkspaceRadioBtn);
			this.Controls.Add(this.groupBox1);
			this.Controls.Add(this.ChangeLink);
			this.Controls.Add(this.ServerLabel);
			this.Controls.Add(this.OkBtn);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.groupBox2);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = global::UnrealGameSync.Properties.Resources.Icon;
			this.Name = "OpenProjectWindow";
			this.ShowInTaskbar = false;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Open Project";
			this.groupBox2.ResumeLayout(false);
			this.groupBox2.PerformLayout();
			this.groupBox1.ResumeLayout(false);
			this.groupBox1.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion
		private System.Windows.Forms.Label WorkspaceNameLabel;
		private System.Windows.Forms.Label WorkspacePathLabel;
		private System.Windows.Forms.GroupBox groupBox2;
		private System.Windows.Forms.Button WorkspacePathBrowseBtn;
		private System.Windows.Forms.TextBox WorkspacePathTextBox;
		private System.Windows.Forms.TextBox WorkspaceNameTextBox;
		private System.Windows.Forms.Label ServerLabel;
		private System.Windows.Forms.LinkLabel ChangeLink;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.RadioButton WorkspaceRadioBtn;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.RadioButton LocalFileRadioBtn;
		private System.Windows.Forms.Button LocalFileBrowseBtn;
		private System.Windows.Forms.TextBox LocalFileTextBox;
		private System.Windows.Forms.Label LocalFileLabel;
		private System.Windows.Forms.Button WorkspaceNameBrowseBtn;
		private System.Windows.Forms.Button WorkspaceNameNewBtn;
	}
}