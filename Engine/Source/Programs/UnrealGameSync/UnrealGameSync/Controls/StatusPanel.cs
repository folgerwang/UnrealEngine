// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	class StatusElementResources
	{
		Dictionary<FontStyle, Font> FontCache = new Dictionary<FontStyle, Font>();
		public readonly Font BadgeFont;

		public StatusElementResources(Font BaseFont)
		{
			FontCache.Add(FontStyle.Regular, BaseFont);

			BadgeFont = new Font(BaseFont.FontFamily, BaseFont.Size - 1.25f, FontStyle.Bold);
		}

		public void Dispose()
		{
			foreach(KeyValuePair<FontStyle, Font> FontPair in FontCache)
			{
				if(FontPair.Key != FontStyle.Regular)
				{
					FontPair.Value.Dispose();
				}
			}

			BadgeFont.Dispose();
		}

		public Font FindOrAddFont(FontStyle Style)
		{
			Font Result;
			if(!FontCache.TryGetValue(Style, out Result))
			{
				Result = new Font(FontCache[FontStyle.Regular], Style);
				FontCache[Style] = Result;
			}
			return Result;
		}
	}

	abstract class StatusElement
	{
		public Cursor Cursor = Cursors.Arrow;
		public bool bMouseOver;
		public bool bMouseDown;
		public Rectangle Bounds;

		public Point Layout(Graphics Graphics, Point Location, StatusElementResources Resources)
		{
			Size Size = Measure(Graphics, Resources);
			Bounds = new Rectangle(Location.X, Location.Y - (Size.Height + 1) / 2, Size.Width, Size.Height);
			return new Point(Location.X + Size.Width, Location.Y);
		}

		public abstract Size Measure(Graphics Graphics, StatusElementResources Resources);
		public abstract void Draw(Graphics Grahpics, StatusElementResources Resources);

		public virtual void OnClick(Point Location)
		{
		}
	}

	class IconStatusElement : StatusElement
	{
		Image Icon;

		public IconStatusElement(Image InIcon)
		{
			Icon = InIcon;
		}

		public override Size Measure(Graphics Graphics, StatusElementResources Resources)
		{
			return new Size((int)(Icon.Width * Graphics.DpiX / 96.0f), (int)(Icon.Height * Graphics.DpiY / 96.0f));
		}

		public override void Draw(Graphics Graphics, StatusElementResources Resources)
		{
			Graphics.DrawImage(Icon, Bounds.Location);
		}
	}

	class IconStripStatusElement : StatusElement
	{
		Image Strip;
		Size IconSize;
		int Index;

		public IconStripStatusElement(Image InStrip, Size InIconSize, int InIndex)
		{
			Strip = InStrip;
			IconSize = InIconSize;
			Index = InIndex;
		}

		public override Size Measure(Graphics Graphics, StatusElementResources Resources)
		{
			return new Size((int)(IconSize.Width * Graphics.DpiX / 96.0f), (int)(IconSize.Height * Graphics.DpiY / 96.0f));
		}

		public override void Draw(Graphics Graphics, StatusElementResources Resources)
		{
			Graphics.DrawImage(Strip, Bounds, new Rectangle(IconSize.Width * Index, 0, IconSize.Width, IconSize.Height), GraphicsUnit.Pixel);
		}
	}

	class TextStatusElement : StatusElement
	{
		string Text;
		FontStyle Style;

		public TextStatusElement(string InText, FontStyle InStyle)
		{
			Text = InText;
			Style = InStyle;
		}

		public override Size Measure(Graphics Graphics, StatusElementResources Resources)
		{
			return TextRenderer.MeasureText(Graphics, Text, Resources.FindOrAddFont(Style), new Size(int.MaxValue, int.MaxValue), TextFormatFlags.NoPadding);
		}

		public override void Draw(Graphics Graphics, StatusElementResources Resources)
		{
			TextRenderer.DrawText(Graphics, Text, Resources.FindOrAddFont(Style), Bounds.Location, SystemColors.ControlText, TextFormatFlags.NoPadding);
		}
	}

	class LinkStatusElement : StatusElement
	{
		string Text;
		FontStyle Style;
		Action<Point, Rectangle> LinkAction;

		public LinkStatusElement(string InText, FontStyle InStyle, Action<Point, Rectangle> InLinkAction)
		{
			Text = InText;
			Style = InStyle;
			LinkAction = InLinkAction;
			Cursor = Cursors.Hand;
		}

		public override void OnClick(Point Location)
		{
			LinkAction(Location, Bounds);
		}

		public override Size Measure(Graphics Graphics, StatusElementResources Resources)
		{
			return TextRenderer.MeasureText(Graphics, Text, Resources.FindOrAddFont(Style), new Size(int.MaxValue, int.MaxValue), TextFormatFlags.NoPadding);
		}

		public override void Draw(Graphics Graphics, StatusElementResources Resources)
		{
			Color TextColor = SystemColors.HotTrack;
			if(bMouseDown)
			{
				TextColor = Color.FromArgb(TextColor.B / 2, TextColor.G / 2, TextColor.R);
			}
			else if(bMouseOver)
			{
				TextColor = Color.FromArgb(TextColor.B, TextColor.G, TextColor.R);
			}
			TextRenderer.DrawText(Graphics, Text, Resources.FindOrAddFont(Style), Bounds.Location, TextColor, TextFormatFlags.NoPadding);
		}
	}

	class BadgeStatusElement : StatusElement
	{
		string Name;
		Color BackgroundColor;
		Color HoverBackgroundColor;
		Action ClickAction;

		public BadgeStatusElement(string InName, Color InBackgroundColor, Action InClickAction)
		{
			Name = InName;
			BackgroundColor = InBackgroundColor;
			if(ClickAction == null)
			{
				HoverBackgroundColor = BackgroundColor;
			}
			else
			{
				HoverBackgroundColor = Color.FromArgb(Math.Min(BackgroundColor.R + 32, 255), Math.Min(BackgroundColor.G + 32, 255), Math.Min(BackgroundColor.B + 32, 255));
			}
			ClickAction = InClickAction;
			if(ClickAction != null)
			{
				Cursor = Cursors.Hand;
			}
		}

		public override void OnClick(Point Location)
		{
			if(ClickAction != null)
			{
				ClickAction();
			}
		}

		public override Size Measure(Graphics Graphics, StatusElementResources Resources)
		{
			Size LabelSize = TextRenderer.MeasureText(Name, Resources.BadgeFont);
			int BadgeHeight = Resources.BadgeFont.Height + 1;
			return new Size(LabelSize.Width + BadgeHeight - 4, BadgeHeight);
		}

		public override void Draw(Graphics Graphics, StatusElementResources Resources)
		{
			using(SolidBrush Brush = new SolidBrush(bMouseOver? HoverBackgroundColor : BackgroundColor))
			{
				Graphics.FillRectangle(Brush, Bounds);
			}
			TextRenderer.DrawText(Graphics, Name, Resources.BadgeFont, Bounds, Color.White, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.NoPrefix | TextFormatFlags.PreserveGraphicsClipping);
		}
	}

	class ProgressBarStatusElement : StatusElement
	{
		float Progress;

		public ProgressBarStatusElement(float InProgress)
		{
			Progress = InProgress;
		}

		public override Size Measure(Graphics Graphics, StatusElementResources Resources)
		{
			int Height = (int)(Resources.FindOrAddFont(FontStyle.Regular).Height * 0.9f);
			return new Size(Height * 16, Height);
		}

		public override void Draw(Graphics Graphics, StatusElementResources Resources)
		{
			Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.None;

			Graphics.DrawRectangle(Pens.Black, Bounds.Left, Bounds.Top, Bounds.Width - 1, Bounds.Height - 1);
			Graphics.FillRectangle(Brushes.White, Bounds.Left + 1, Bounds.Top + 1, Bounds.Width - 2, Bounds.Height - 2);

			int ProgressX = Bounds.Left + 2 + (int)((Bounds.Width - 4) * Progress);
			using(Brush ProgressBarBrush = new SolidBrush(Color.FromArgb(112, 146, 190)))
			{
				Graphics.FillRectangle(ProgressBarBrush, Bounds.Left + 2, Bounds.Y + 2, ProgressX - (Bounds.Left + 2), Bounds.Height - 4);
			}
		}
	}

	class StatusLine
	{
		List<StatusElement> Elements = new List<StatusElement>();

		public StatusLine()
		{
			LineHeight = 1.0f;
		}

		public float LineHeight
		{
			get;
			set;
		}

		public Rectangle Bounds
		{
			get;
			private set;
		}

		public void AddIcon(Image InIcon)
		{
			Elements.Add(new IconStatusElement(InIcon));
		}

		public void AddIcon(Image InStrip, Size InIconSize, int InIndex)
		{
			Elements.Add(new IconStripStatusElement(InStrip, InIconSize, InIndex));
		}

		public void AddText(string InText, FontStyle InStyle = FontStyle.Regular)
		{
			Elements.Add(new TextStatusElement(InText, InStyle));
		}

		public void AddLink(string InText, FontStyle InStyle, Action InLinkAction)
		{
			Elements.Add(new LinkStatusElement(InText, InStyle, (P, R) => { InLinkAction(); }));
		}

		public void AddLink(string InText, FontStyle InStyle, Action<Point, Rectangle> InLinkAction)
		{
			Elements.Add(new LinkStatusElement(InText, InStyle, InLinkAction));
		}

		public void AddBadge(string InText, Color InBackgroundColor, Action InClickAction)
		{
			Elements.Add(new BadgeStatusElement(InText, InBackgroundColor, InClickAction));
		}

		public void AddProgressBar(float Progress)
		{
			Elements.Add(new ProgressBarStatusElement(Progress));
		}

		public bool HitTest(Point Location, out StatusElement OutElement)
		{
			OutElement = null;
			if(Bounds.Contains(Location))
			{
				foreach(StatusElement Element in Elements)
				{
					if(Element.Bounds.Contains(Location))
					{
						OutElement = Element;
						return true;
					}
				}
			}
			return false;
		}

		public void Layout(Graphics Graphics, Point Location, StatusElementResources Resources)
		{
			Bounds = new Rectangle(Location, new Size(0, 0));

			Point NextLocation = Location;
			foreach(StatusElement Element in Elements)
			{
				NextLocation = Element.Layout(Graphics, NextLocation, Resources);
				Bounds = Rectangle.Union(Bounds, Element.Bounds);
			}
		}

		public void Draw(Graphics Graphics, StatusElementResources Resources)
		{
			foreach(StatusElement Element in Elements)
			{
				Element.Draw(Graphics, Resources);
			}
		}
	}

	class StatusPanel : Panel
	{
		const float LineSpacing = 1.35f;

		Image ProjectLogo;
		bool bDisposeProjectLogo;
		Rectangle ProjectLogoBounds;
		StatusElementResources Resources;
		List<StatusLine> Lines = new List<StatusLine>();
		StatusLine Caption;
		Pen AlertDividerPen;
		int AlertDividerY;
		StatusLine Alert;
		Color? TintColor;
		Point? MouseOverLocation;
		StatusElement MouseOverElement;
		Point? MouseDownLocation;
		StatusElement MouseDownElement;
		int ContentWidth = 400;
		int SuspendDisplayCount;

		public StatusPanel()
		{
			DoubleBuffered = true;

			AlertDividerPen = new Pen(Color.Black);
			AlertDividerPen.DashPattern = new float[] { 2, 2 };
		}

		public void SuspendDisplay()
		{
			SuspendDisplayCount++;
		}

		public void ResumeDisplay()
		{
			SuspendDisplayCount--;
		}

		public void SetContentWidth(int NewContentWidth)
		{
			if(ContentWidth != NewContentWidth)
			{
				ContentWidth = NewContentWidth;
				LayoutElements();
				Invalidate();
			}
		}

		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);

			if(disposing)
			{
				if(AlertDividerPen != null)
				{
					AlertDividerPen.Dispose();
					AlertDividerPen = null;
				}
				if(ProjectLogo != null)
				{
					if(bDisposeProjectLogo)
					{
						ProjectLogo.Dispose();
					}
					ProjectLogo = null;
				}
				ResetFontCache();
			}
		}

		private void ResetFontCache()
		{
			if(Resources != null)
			{
				Resources.Dispose();
				Resources = null;
			}
			Resources = new StatusElementResources(Font);
		}

		public void SetProjectLogo(Image NewProjectLogo, bool bDispose)
		{
			if(ProjectLogo != null)
			{
				if(bDisposeProjectLogo)
				{
					ProjectLogo.Dispose();
				}
			}
			ProjectLogo = NewProjectLogo;
			bDisposeProjectLogo = bDispose;
			Invalidate();
		}

		public void Clear()
		{
			InvalidateElements();
			Lines.Clear();
			Caption = null;
		}

		public void Set(IEnumerable<StatusLine> NewLines, StatusLine NewCaption, StatusLine NewAlert, Color? NewTintColor)
		{
			if(Resources == null)
			{
				Resources = new StatusElementResources(Font);
			}

			InvalidateElements();
			Lines.Clear();
			Lines.AddRange(NewLines);
			Caption = NewCaption;
			Alert = NewAlert;
			TintColor = NewTintColor;
			LayoutElements();
			InvalidateElements();

			MouseOverElement = null;
			MouseDownElement = null;
			SetMouseOverLocation(MouseOverLocation);
			SetMouseDownLocation(MouseDownLocation);
		}

		protected void InvalidateElements()
		{
			Invalidate(ProjectLogoBounds);

			foreach(StatusLine Line in Lines)
			{
				Invalidate(Line.Bounds);
			}
			if(Caption != null)
			{
				Invalidate(Caption.Bounds);
			}
			if(Alert != null)
			{
				Invalidate(Alert.Bounds);
			}
		}

		protected bool HitTest(Point Location, out StatusElement OutElement)
		{
			OutElement = null;
			foreach(StatusLine Line in Lines)
			{
				if(Line.HitTest(Location, out OutElement))
				{
					return true;
				}
			}
			if(Caption != null)
			{
				if(Caption.HitTest(Location, out OutElement))
				{
					return true;
				}
			}
			if(Alert != null)
			{
				if(Alert.HitTest(Location, out OutElement))
				{
					return true;
				}
			}
			return false;
		}

		protected override void OnFontChanged(EventArgs e)
		{
			base.OnFontChanged(e);

			ResetFontCache();
			LayoutElements();
		}

		protected void LayoutElements()
		{
			using(Graphics Graphics = CreateGraphics())
			{
				LayoutElements(Graphics);
			}
		}

		protected void LayoutElements(Graphics Graphics)
		{
			// Layout the alert message
			int BodyHeight = Height;
			if(Alert != null)
			{
				AlertDividerY = Height - (int)(Font.Height * 2);
				Alert.Layout(Graphics, Point.Empty, Resources);
				Alert.Layout(Graphics, new Point((Width - Alert.Bounds.Width) / 2, (Height + AlertDividerY) / 2), Resources);
				BodyHeight = AlertDividerY;
			}

			// Get the logo size
			Image DrawProjectLogo = ProjectLogo ?? Properties.Resources.DefaultProjectLogo;
			float LogoScale = Math.Min((float)BodyHeight - ((Caption != null)? Font.Height : 0) / DrawProjectLogo.Height, Graphics.DpiY / 96.0f);
			int LogoWidth = (int)(DrawProjectLogo.Width * LogoScale);
			int LogoHeight = (int)(DrawProjectLogo.Height * LogoScale);

			// Figure out where the split between content and the logo is going to be
			int DividerX = ((Width - LogoWidth - ContentWidth) / 2) + LogoWidth;

			// Get the logo position
			int LogoX = DividerX - LogoWidth;
			int LogoY = (BodyHeight - LogoHeight) / 2;

			// Layout the caption. We may move the logo to make room for this.
			LogoY -= Font.Height / 2;
			if(Caption != null)
			{
				Caption.Layout(Graphics, Point.Empty, Resources);
				int CaptionWidth = Caption.Bounds.Width;
				Caption.Layout(Graphics, new Point(Math.Min(LogoX + (LogoWidth / 2) - (CaptionWidth / 2), DividerX - CaptionWidth), LogoY + LogoHeight), Resources);
			}

			// Set the logo rectangle
			ProjectLogoBounds = new Rectangle(LogoX, LogoY, LogoWidth, LogoHeight);

			// Measure up all the line height
			float TotalLineHeight = Lines.Sum(x => x.LineHeight);

			// Space out all the lines
			float LineY = (BodyHeight - TotalLineHeight * (int)(Font.Height * LineSpacing)) / 2;
			foreach(StatusLine Line in Lines)
			{
				LineY += (int)(Font.Height * LineSpacing * Line.LineHeight * 0.5f);
				Line.Layout(Graphics, new Point(DividerX + 5, (int)LineY), Resources);
				LineY += (int)(Font.Height * LineSpacing * Line.LineHeight * 0.5f);
			}
		}

		protected override void OnMouseMove(MouseEventArgs e)
		{
			base.OnMouseMove(e);

			SetMouseOverLocation(e.Location);
		}

		protected override void OnMouseDown(MouseEventArgs e)
		{
			base.OnMouseDown(e);

			if(e.Button == MouseButtons.Left)
			{
				SetMouseDownLocation(e.Location);
			}
		}

		protected override void OnMouseUp(MouseEventArgs e)
		{
			base.OnMouseUp(e);

			if(MouseDownElement != null && MouseOverElement == MouseDownElement)
			{
				MouseDownElement.OnClick(e.Location);
			}

			SetMouseDownLocation(null);
		}

		protected override void OnMouseLeave(EventArgs e)
		{
			base.OnMouseLeave(e);

			SetMouseDownLocation(null);
			SetMouseOverLocation(null);
		}

		protected override void OnResize(EventArgs eventargs)
		{
			base.OnResize(eventargs);

			LayoutElements();
			Invalidate();
		}

		protected override void OnPaintBackground(PaintEventArgs e)
		{
			base.OnPaintBackground(e);

			if(TintColor.HasValue)
			{
				int TintSize = Width / 2;
				using(LinearGradientBrush BackgroundBrush = new LinearGradientBrush(new Point(Width, 0), new Point(Width - TintSize, TintSize), TintColor.Value, BackColor))
				{
					BackgroundBrush.WrapMode = WrapMode.TileFlipXY;
					using (GraphicsPath Path = new GraphicsPath())
					{
						Path.StartFigure();
						Path.AddLine(Width, 0, Width - TintSize * 2, 0);
						Path.AddLine(Width, TintSize * 2, Width, 0);
						Path.CloseFigure();

						e.Graphics.FillPath(BackgroundBrush, Path);
					}
				}
			}
		}

		protected override void OnPaint(PaintEventArgs e)
		{
			base.OnPaint(e);

			if(SuspendDisplayCount == 0)
			{
				e.Graphics.DrawImage(ProjectLogo ?? Properties.Resources.DefaultProjectLogo, ProjectLogoBounds);

				e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.None;

				foreach(StatusLine Line in Lines)
				{
					Line.Draw(e.Graphics, Resources);
				}
				if(Caption != null)
				{
					Caption.Draw(e.Graphics, Resources);
				}
				if(Alert != null)
				{
					e.Graphics.DrawLine(AlertDividerPen, 0, AlertDividerY, Width, AlertDividerY);
					Alert.Draw(e.Graphics, Resources);
				}
			}
		}

		protected void SetMouseOverLocation(Point? NewMouseOverLocation)
		{
			MouseOverLocation = NewMouseOverLocation;

			StatusElement NewMouseOverElement = null;
			if(MouseOverLocation.HasValue)
			{
				HitTest(MouseOverLocation.Value, out NewMouseOverElement);
			}

			if(NewMouseOverElement != MouseOverElement)
			{
				if(MouseOverElement != null)
				{
					MouseOverElement.bMouseOver = false;
					Cursor = Cursors.Arrow;
					Invalidate(MouseOverElement.Bounds);
				}

				MouseOverElement = NewMouseOverElement;

				if(MouseOverElement != null)
				{
					MouseOverElement.bMouseOver = true;
					Cursor = MouseOverElement.Cursor;
					Invalidate(MouseOverElement.Bounds);
				}
			}
		}

		protected void SetMouseDownLocation(Point? NewMouseDownLocation)
		{
			MouseDownLocation = NewMouseDownLocation;

			StatusElement NewMouseDownElement = null;
			if(MouseDownLocation.HasValue)
			{
				HitTest(MouseDownLocation.Value, out NewMouseDownElement);
			}

			if(NewMouseDownElement != MouseDownElement)
			{
				if(MouseDownElement != null)
				{
					MouseDownElement.bMouseDown = false;
					Invalidate(MouseDownElement.Bounds);
				}

				MouseDownElement = NewMouseDownElement;

				if(MouseDownElement != null)
				{
					MouseDownElement.bMouseDown = true;
					Invalidate(MouseDownElement.Bounds);
				}
			}
		}
	}
}
