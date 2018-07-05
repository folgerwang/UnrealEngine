using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;

namespace UnrealGameSync
{
	public partial class CustomListViewControl : ListView
	{
		VisualStyleRenderer SelectedItemRenderer;
		VisualStyleRenderer TrackedItemRenderer;
		public int HoverItem = -1;

		public CustomListViewControl()
		{
            if (Application.RenderWithVisualStyles) 
            { 
				SelectedItemRenderer = new VisualStyleRenderer("Explorer::ListView", 1, 3);
				TrackedItemRenderer = new VisualStyleRenderer("Explorer::ListView", 1, 2); 
			}
		}

		protected override void OnMouseLeave(EventArgs e)
		{
			base.OnMouseLeave(e);

			if(HoverItem != -1)
			{
				HoverItem = -1;
				Invalidate();
			}
		}

		protected override void OnMouseMove(MouseEventArgs e)
		{
			ListViewHitTestInfo HitTest = this.HitTest(e.Location);

			int PrevHoverItem = HoverItem;
			HoverItem = (HitTest.Item == null)? -1 : HitTest.Item.Index;
			if(HoverItem != PrevHoverItem)
			{
				if(HoverItem != -1)
				{
					RedrawItems(HoverItem, HoverItem, true);
				}
				if(PrevHoverItem != -1 && PrevHoverItem < Items.Count)
				{
					RedrawItems(PrevHoverItem, PrevHoverItem, true);
				}
			}

			base.OnMouseMove(e);
		}

		public void DrawBackground(Graphics Graphics, ListViewItem Item)
		{
			if(Item.Selected)
			{
				DrawSelectedBackground(Graphics, Item.Bounds);
			}
			else if(Item.Index == HoverItem)
			{
				DrawTrackedBackground(Graphics, Item.Bounds);
			}
			else
			{
				DrawDefaultBackground(Graphics, Item.Bounds);
			}
		}

		public void DrawDefaultBackground(Graphics Graphics, Rectangle Bounds)
		{
			Graphics.FillRectangle(SystemBrushes.Window, Bounds);
		}

		public void DrawSelectedBackground(Graphics Graphics, Rectangle Bounds)
		{
			if(Application.RenderWithVisualStyles)
			{
				SelectedItemRenderer.DrawBackground(Graphics, Bounds);
			}
			else
			{
				Graphics.FillRectangle(SystemBrushes.ButtonFace, Bounds);
			}
		}

		public void DrawTrackedBackground(Graphics Graphics, Rectangle Bounds)
		{
			if(Application.RenderWithVisualStyles)
			{
				TrackedItemRenderer.DrawBackground(Graphics, Bounds);
			}
			else
			{
				Graphics.FillRectangle(SystemBrushes.Window, Bounds);
			}
		}
	}
}
