namespace UnrealGameSync
{
	partial class ProjectWindow
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
			this.WorkspaceLabel = new System.Windows.Forms.Label();
			this.WorkspaceNewBtn = new System.Windows.Forms.Button();
			this.PathLabel = new System.Windows.Forms.Label();
			this.groupBox2 = new System.Windows.Forms.GroupBox();
			this.PathBrowseBtn = new System.Windows.Forms.Button();
			this.WorkspacePathTextBox = new System.Windows.Forms.TextBox();
			this.WorkspaceBrowseBtn = new System.Windows.Forms.Button();
			this.WorkspaceNameTextBox = new System.Windows.Forms.TextBox();
			this.ServerLabel = new System.Windows.Forms.Label();
			this.ChangeLink = new System.Windows.Forms.LinkLabel();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.OkBtn = new System.Windows.Forms.Button();
			this.button1 = new System.Windows.Forms.Button();
			this.groupBox2.SuspendLayout();
			this.SuspendLayout();
			// 
			// WorkspaceLabel
			// 
			this.WorkspaceLabel.AutoSize = true;
			this.WorkspaceLabel.Location = new System.Drawing.Point(23, 39);
			this.WorkspaceLabel.Name = "WorkspaceLabel";
			this.WorkspaceLabel.Size = new System.Drawing.Size(68, 15);
			this.WorkspaceLabel.TabIndex = 1;
			this.WorkspaceLabel.Text = "Workspace:";
			// 
			// WorkspaceNewBtn
			// 
			this.WorkspaceNewBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspaceNewBtn.Location = new System.Drawing.Point(641, 33);
			this.WorkspaceNewBtn.Name = "WorkspaceNewBtn";
			this.WorkspaceNewBtn.Size = new System.Drawing.Size(89, 27);
			this.WorkspaceNewBtn.TabIndex = 3;
			this.WorkspaceNewBtn.Text = "New...";
			this.WorkspaceNewBtn.UseVisualStyleBackColor = true;
			this.WorkspaceNewBtn.Click += new System.EventHandler(this.WorkspaceNewBtn_Click);
			// 
			// PathLabel
			// 
			this.PathLabel.AutoSize = true;
			this.PathLabel.Location = new System.Drawing.Point(23, 76);
			this.PathLabel.Name = "PathLabel";
			this.PathLabel.Size = new System.Drawing.Size(34, 15);
			this.PathLabel.TabIndex = 5;
			this.PathLabel.Text = "Path:";
			// 
			// groupBox2
			// 
			this.groupBox2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox2.Controls.Add(this.PathBrowseBtn);
			this.groupBox2.Controls.Add(this.WorkspacePathTextBox);
			this.groupBox2.Controls.Add(this.PathLabel);
			this.groupBox2.Controls.Add(this.WorkspaceBrowseBtn);
			this.groupBox2.Controls.Add(this.WorkspaceNewBtn);
			this.groupBox2.Controls.Add(this.WorkspaceNameTextBox);
			this.groupBox2.Controls.Add(this.WorkspaceLabel);
			this.groupBox2.Location = new System.Drawing.Point(21, 57);
			this.groupBox2.Name = "groupBox2";
			this.groupBox2.Size = new System.Drawing.Size(851, 128);
			this.groupBox2.TabIndex = 1;
			this.groupBox2.TabStop = false;
			this.groupBox2.Text = "Project";
			// 
			// PathBrowseBtn
			// 
			this.PathBrowseBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.PathBrowseBtn.Location = new System.Drawing.Point(736, 70);
			this.PathBrowseBtn.Name = "PathBrowseBtn";
			this.PathBrowseBtn.Size = new System.Drawing.Size(94, 27);
			this.PathBrowseBtn.TabIndex = 7;
			this.PathBrowseBtn.Text = "Select...";
			this.PathBrowseBtn.UseVisualStyleBackColor = true;
			this.PathBrowseBtn.Click += new System.EventHandler(this.PathBrowseBtn_Click);
			// 
			// WorkspacePathTextBox
			// 
			this.WorkspacePathTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspacePathTextBox.Location = new System.Drawing.Point(108, 73);
			this.WorkspacePathTextBox.Name = "WorkspacePathTextBox";
			this.WorkspacePathTextBox.Size = new System.Drawing.Size(622, 23);
			this.WorkspacePathTextBox.TabIndex = 6;
			// 
			// WorkspaceBrowseBtn
			// 
			this.WorkspaceBrowseBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspaceBrowseBtn.Location = new System.Drawing.Point(736, 33);
			this.WorkspaceBrowseBtn.Name = "WorkspaceBrowseBtn";
			this.WorkspaceBrowseBtn.Size = new System.Drawing.Size(94, 27);
			this.WorkspaceBrowseBtn.TabIndex = 4;
			this.WorkspaceBrowseBtn.Text = "Select...";
			this.WorkspaceBrowseBtn.UseVisualStyleBackColor = true;
			this.WorkspaceBrowseBtn.Click += new System.EventHandler(this.WorkspaceBrowseBtn_Click);
			// 
			// WorkspaceNameTextBox
			// 
			this.WorkspaceNameTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.WorkspaceNameTextBox.Location = new System.Drawing.Point(108, 36);
			this.WorkspaceNameTextBox.Name = "WorkspaceNameTextBox";
			this.WorkspaceNameTextBox.Size = new System.Drawing.Size(527, 23);
			this.WorkspaceNameTextBox.TabIndex = 2;
			// 
			// ServerLabel
			// 
			this.ServerLabel.AutoSize = true;
			this.ServerLabel.Location = new System.Drawing.Point(19, 23);
			this.ServerLabel.Name = "ServerLabel";
			this.ServerLabel.Size = new System.Drawing.Size(331, 15);
			this.ServerLabel.TabIndex = 5;
			this.ServerLabel.Text = "Using default Perforce connection (perforce:1666, Ben.Marsh)";
			// 
			// ChangeLink
			// 
			this.ChangeLink.AutoSize = true;
			this.ChangeLink.Location = new System.Drawing.Point(356, 23);
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
			this.CancelBtn.Location = new System.Drawing.Point(671, 201);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(98, 27);
			this.CancelBtn.TabIndex = 3;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.Location = new System.Drawing.Point(775, 201);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(97, 27);
			this.OkBtn.TabIndex = 4;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			this.OkBtn.Click += new System.EventHandler(this.OkBtn_Click);
			// 
			// button1
			// 
			this.button1.Location = new System.Drawing.Point(22, 201);
			this.button1.Name = "button1";
			this.button1.Size = new System.Drawing.Size(145, 27);
			this.button1.TabIndex = 7;
			this.button1.Text = "Browse For File...";
			this.button1.UseVisualStyleBackColor = true;
			// 
			// ProjectWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(893, 239);
			this.Controls.Add(this.button1);
			this.Controls.Add(this.ChangeLink);
			this.Controls.Add(this.ServerLabel);
			this.Controls.Add(this.OkBtn);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.groupBox2);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = global::UnrealGameSync.Properties.Resources.Icon;
			this.Name = "ProjectWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Open Project";
			this.groupBox2.ResumeLayout(false);
			this.groupBox2.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion
		private System.Windows.Forms.Label WorkspaceLabel;
		private System.Windows.Forms.Button WorkspaceNewBtn;
		private System.Windows.Forms.Label PathLabel;
		private System.Windows.Forms.GroupBox groupBox2;
		private System.Windows.Forms.Button PathBrowseBtn;
		private System.Windows.Forms.TextBox WorkspacePathTextBox;
		private System.Windows.Forms.Button WorkspaceBrowseBtn;
		private System.Windows.Forms.TextBox WorkspaceNameTextBox;
		private System.Windows.Forms.Label ServerLabel;
		private System.Windows.Forms.LinkLabel ChangeLink;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Button button1;
	}
}