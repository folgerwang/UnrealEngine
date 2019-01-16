// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	public partial class SdkInfoWindow : Form
	{
		class SdkItem
		{
			public string Category;
			public string Description;
			public string Install;
			public string Browse;
		}

		class BadgeInfo
		{
			public string UniqueId;
			public string Label;
			public Rectangle Rectangle;
			public Action OnClick;
		}

		Font BadgeFont;
		string HoverBadgeUniqueId;

		public SdkInfoWindow(string[] SdkInfoEntries, Dictionary<string, string> Variables, Font BadgeFont)
		{
			InitializeComponent();

			this.BadgeFont = BadgeFont;

			Dictionary<string, ConfigObject> UniqueIdToObject = new Dictionary<string, ConfigObject>(StringComparer.InvariantCultureIgnoreCase);
			foreach(string SdkInfoEntry in SdkInfoEntries)
			{
				ConfigObject Object = new ConfigObject(SdkInfoEntry);

				string UniqueId = Object.GetValue("UniqueId", Guid.NewGuid().ToString());

				ConfigObject ExistingObject;
				if(UniqueIdToObject.TryGetValue(UniqueId, out ExistingObject))
				{
					ExistingObject.AddOverrides(Object, null);
				}
				else
				{
					UniqueIdToObject.Add(UniqueId, Object);
				}
			}

			List<SdkItem> Items = new List<SdkItem>();
			foreach(ConfigObject Object in UniqueIdToObject.Values)
			{
				SdkItem Item = new SdkItem();
				Item.Category = Object.GetValue("Category", "Other");
				Item.Description = Object.GetValue("Description", "");

				Item.Install = Utility.ExpandVariables(Object.GetValue("Install", ""), Variables);
				if(Item.Install.Contains("$("))
				{
					Item.Install = null;
				}

				Item.Browse = Utility.ExpandVariables(Object.GetValue("Browse", ""), Variables);
				if(Item.Browse.Contains("$("))
				{
					Item.Browse = null;
				}

				if(!String.IsNullOrEmpty(Item.Install) && String.IsNullOrEmpty(Item.Browse))
				{
					try
					{
						Item.Browse = Path.GetDirectoryName(Item.Install);
					}
					catch
					{
						Item.Browse = null;
					}
				}

				Items.Add(Item);
			}

			foreach(IGrouping<string, SdkItem> ItemGroup in Items.GroupBy(x => x.Category).OrderBy(x => x.Key))
			{
				ListViewGroup Group = new ListViewGroup(ItemGroup.Key);
				SdkListView.Groups.Add(Group);

				foreach(SdkItem Item in ItemGroup)
				{
					ListViewItem NewItem = new ListViewItem(Group);
					NewItem.SubItems.Add(Item.Description);
					NewItem.SubItems.Add(new ListViewItem.ListViewSubItem(){ Tag = Item });
					SdkListView.Items.Add(NewItem);
				}
			}

			System.Reflection.PropertyInfo DoubleBufferedProperty = typeof(Control).GetProperty("DoubleBuffered", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
			DoubleBufferedProperty.SetValue(SdkListView, true, null);

			if(SdkListView.Items.Count > 0)
			{
				int ItemsHeight = SdkListView.Items[SdkListView.Items.Count - 1].Bounds.Bottom + 20;
				Height = SdkListView.Top + ItemsHeight + (Height - SdkListView.Bottom);
			}
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.OK;
			Close();
		}

		private void SdkListView_DrawItem(object sender, DrawListViewItemEventArgs e)
		{
			SdkListView.DrawBackground(e.Graphics, e.Item);
		}

		private void SdkListView_DrawSubItem(object sender, DrawListViewSubItemEventArgs e)
		{
			if(e.ColumnIndex != columnHeader3.Index)
			{
				TextRenderer.DrawText(e.Graphics, e.SubItem.Text, SdkListView.Font, e.Bounds, SdkListView.ForeColor, TextFormatFlags.EndEllipsis | TextFormatFlags.SingleLine | TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
			}
			else
			{
				e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;

				List<BadgeInfo> Badges = GetBadges(e.Item, e.SubItem);
				for(int Idx = 0; Idx < Badges.Count; Idx++)
				{
					Color BadgeColor = (HoverBadgeUniqueId == Badges[Idx].UniqueId)? Color.FromArgb(140, 180, 230) : Color.FromArgb(112, 146, 190);
					if(Badges[Idx].OnClick != null)
					{
						DrawBadge(e.Graphics, Badges[Idx].Label, Badges[Idx].Rectangle, (Idx > 0), (Idx < Badges.Count - 1), BadgeColor);
					}
				}
			}
		}

		private void SdkListView_MouseMove(object sender, MouseEventArgs e)
		{
			string NewHoverUniqueId = null;

			ListViewHitTestInfo HitTest = SdkListView.HitTest(e.Location);
			if(HitTest.Item != null && HitTest.SubItem == HitTest.Item.SubItems[2])
			{
				List<BadgeInfo> Badges = GetBadges(HitTest.Item, HitTest.SubItem);
				foreach(BadgeInfo Badge in Badges)
				{
					if(Badge.Rectangle.Contains(e.Location))
					{
						NewHoverUniqueId = Badge.UniqueId;
					}
				}
			}

			if(NewHoverUniqueId != HoverBadgeUniqueId)
			{
				HoverBadgeUniqueId = NewHoverUniqueId;
				SdkListView.Invalidate();
			}
		}

		private void SdkListView_MouseLeave(object sender, EventArgs e)
		{
			HoverBadgeUniqueId = null;
		}

		private void SdkListView_MouseDown(object sender, MouseEventArgs e)
		{
			ListViewHitTestInfo HitTest = SdkListView.HitTest(e.Location);
			if(HitTest.Item != null && HitTest.SubItem == HitTest.Item.SubItems[2])
			{
				List<BadgeInfo> Badges = GetBadges(HitTest.Item, HitTest.SubItem);
				foreach(BadgeInfo Badge in Badges)
				{
					if(Badge.Rectangle.Contains(e.Location))
					{
						Badge.OnClick();
					}
				}
			}
		}

		private List<BadgeInfo> GetBadges(ListViewItem Item, ListViewItem.ListViewSubItem SubItem)
		{
			string UniqueIdPrefix = String.Format("{0}_", Item.Index);

			List<BadgeInfo> Badges = new List<BadgeInfo>();

			SdkItem Sdk = (SdkItem)SubItem.Tag;

			Action InstallAction = null;
			if(!String.IsNullOrEmpty(Sdk.Install))
			{
				InstallAction = () => { Install(Sdk.Install); };
			}
			Badges.Add(new BadgeInfo(){ UniqueId = UniqueIdPrefix + "_Install", Label = "Install", OnClick = InstallAction });

			Action BrowseAction = null;
			if(!String.IsNullOrEmpty(Sdk.Browse))
			{
				BrowseAction = () => { Browse(Sdk.Browse); };
			}
			Badges.Add(new BadgeInfo(){ UniqueId = UniqueIdPrefix + "_Browse", Label = "Browse", OnClick = BrowseAction });

			int Right = SubItem.Bounds.Right - 10;
			for(int Idx = Badges.Count - 1; Idx >= 0; Idx--)
			{
				Size BadgeSize = GetBadgeSize(Badges[Idx].Label);
				Right -= BadgeSize.Width;
				Badges[Idx].Rectangle = new Rectangle(Right, SubItem.Bounds.Y + (SubItem.Bounds.Height - BadgeSize.Height) / 2, BadgeSize.Width, BadgeSize.Height);
			}

			return Badges;
		}

		private void Browse(string DirectoryName)
		{
			try
			{
				Process.Start("explorer.exe", String.Format("\"{0}\"", DirectoryName));
			}
			catch(Exception Ex)
			{
				MessageBox.Show(String.Format("Unable to open explorer to {0}: {1}", DirectoryName, Ex.Message));
			}
		}

		private void Install(string FileName)
		{
			try
			{
				Process.Start(FileName);
			}
			catch(Exception Ex)
			{
				MessageBox.Show(String.Format("Unable to run {0}: {1}", FileName, Ex.Message));
			}
		}

		private Size GetBadgeSize(string BadgeText)
		{
			Size LabelSize = TextRenderer.MeasureText(BadgeText, BadgeFont);
			int BadgeHeight = BadgeFont.Height + 1;

			return new Size(LabelSize.Width + BadgeHeight - 4, BadgeHeight);
		}

		private void DrawBadge(Graphics Graphics, string BadgeText, Rectangle BadgeRect, bool bMergeLeft, bool bMergeRight, Color BadgeColor)
		{
			using (GraphicsPath Path = new GraphicsPath())
			{
				Path.StartFigure();
				Path.AddLine(BadgeRect.Left + (bMergeLeft? 1 : 0), BadgeRect.Top, BadgeRect.Left - (bMergeLeft? 1 : 0), BadgeRect.Bottom);
				Path.AddLine(BadgeRect.Left - (bMergeLeft? 1 : 0), BadgeRect.Bottom, BadgeRect.Right - 1 - (bMergeRight? 1 : 0), BadgeRect.Bottom);
				Path.AddLine(BadgeRect.Right - 1 - (bMergeRight? 1 : 0), BadgeRect.Bottom, BadgeRect.Right - 1 + (bMergeRight? 1 : 0), BadgeRect.Top);
				Path.AddLine(BadgeRect.Right - 1 + (bMergeRight? 1 : 0), BadgeRect.Top, BadgeRect.Left + (bMergeLeft? 1 : 0), BadgeRect.Top);
				Path.CloseFigure();

				using(SolidBrush Brush = new SolidBrush(BadgeColor))
				{
					Graphics.FillPath(Brush, Path);
				}
			}

			TextRenderer.DrawText(Graphics, BadgeText, BadgeFont, BadgeRect, Color.White, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.NoPrefix | TextFormatFlags.PreserveGraphicsClipping);
		}
	}
}
