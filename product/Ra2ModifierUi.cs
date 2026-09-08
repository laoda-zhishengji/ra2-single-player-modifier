using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

// Red Alert 2 Modifier - Box Model Layout v0.6.0
// Each element has explicit bounds, text clips within boundaries

static class C
{
    public static readonly Color Bg = Color.FromArgb(11, 14, 19);
    public static readonly Color BgSide = Color.FromArgb(18, 22, 28);
    public static readonly Color BgCard = Color.FromArgb(23, 28, 35);
    public static readonly Color BgHover = Color.FromArgb(31, 38, 47);
    public static readonly Color BgActive = Color.FromArgb(39, 46, 56);
    public static readonly Color BgSec = Color.FromArgb(15, 20, 26);
    public static readonly Color BgHdr = Color.FromArgb(13, 17, 22);
    public static readonly Color BgTogOff = Color.FromArgb(41, 49, 59);
    public static readonly Color Acc = Color.FromArgb(218, 56, 66);
    public static readonly Color AccB = Color.FromArgb(255, 96, 104);
    public static readonly Color Grn = Color.FromArgb(42, 199, 169);
    public static readonly Color Yel = Color.FromArgb(214, 168, 79);
    public static readonly Color Red = Color.FromArgb(236, 82, 91);
    public static readonly Color Tw = Color.FromArgb(245, 247, 250);
    public static readonly Color Tg = Color.FromArgb(154, 166, 178);
    public static readonly Color Td = Color.FromArgb(101, 113, 124);
    public static readonly Color Bd = Color.FromArgb(40, 49, 60);
}

// Text label - clips within bounds with ellipsis
sealed class LB : Label
{
    public LB() { BackColor = Color.Transparent; AutoSize = false; AutoEllipsis = false; UseMnemonic = false; UseCompatibleTextRendering = false; TextAlign = ContentAlignment.MiddleLeft; }
}

// Icon label - never ellipsis, always centered
sealed class IC : Label
{
    public IC() { BackColor = Color.Transparent; AutoSize = false; UseMnemonic = false; UseCompatibleTextRendering = false; TextAlign = ContentAlignment.MiddleCenter; }
}

sealed class FeatureIcon : Control
{
    static readonly Image Sheet = LoadSheet();
    readonly int _index;
    public FeatureIcon(int index) { _index = Math.Max(0, Math.Min(7, index)); SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.SupportsTransparentBackColor, true); BackColor = Color.Transparent; }
    static Image LoadSheet() { try { string p = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "assets", "feature-icons.png"); return File.Exists(p) ? Image.FromFile(p) : null; } catch { return null; } }
    protected override void OnPaint(PaintEventArgs e) {
        if (Sheet == null) return;
        Graphics g = e.Graphics; g.SmoothingMode = SmoothingMode.HighQuality; g.InterpolationMode = InterpolationMode.HighQualityBicubic; g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        int col = _index % 4, row = _index / 4;
        Rectangle src = new Rectangle(78 + col * 446, 78 + row * 425, 290, 290);
        Rectangle dst = new Rectangle(2, 2, Math.Max(1, Width - 4), Math.Max(1, Height - 4));
        g.DrawImage(Sheet, dst, src, GraphicsUnit.Pixel);
    }
}

sealed class SmoothPanel : Panel
{
    public SmoothPanel() { DoubleBuffered = true; SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true); }
}

sealed class RoundButton : Button
{
    bool _hover, _pressed;
    public RoundButton() { FlatStyle = FlatStyle.Flat; FlatAppearance.BorderSize = 0; SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true); Cursor = Cursors.Hand; }
    protected override void OnMouseEnter(EventArgs e) { _hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { _hover = false; _pressed = false; Invalidate(); base.OnMouseLeave(e); }
    protected override void OnMouseDown(MouseEventArgs e) { _pressed = true; Invalidate(); base.OnMouseDown(e); }
    protected override void OnMouseUp(MouseEventArgs e) { _pressed = false; Invalidate(); base.OnMouseUp(e); }
    protected override void OnResize(EventArgs e) { base.OnResize(e); if (Width > 2 && Height > 2) Region = new Region(RR(new Rectangle(1, 1, Width - 3, Height - 3), 8)); }
    protected override void OnPaint(PaintEventArgs e) {
        Graphics g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent == null ? C.Bg : Parent.BackColor);
        Rectangle r = new Rectangle(1, 1, Math.Max(1, Width - 3), Math.Max(1, Height - 3));
        Color bg = _pressed ? Color.FromArgb(Math.Max(0, C.Grn.R - 22), Math.Max(0, C.Grn.G - 22), Math.Max(0, C.Grn.B - 22)) : _hover ? Color.FromArgb(Math.Min(255, C.Grn.R + 12), Math.Min(255, C.Grn.G + 12), Math.Min(255, C.Grn.B + 12)) : C.Grn;
        using (Brush br = new SolidBrush(bg)) g.FillPath(br, RR(r, 8));
        using (Pen pn = new Pen(Color.FromArgb(90, 255, 255, 255), 1)) g.DrawPath(pn, RR(r, 8));
        TextRenderer.DrawText(g, Text, Font, r, Color.White, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
    }
    static GraphicsPath RR(Rectangle r, int rad) { GraphicsPath p = new GraphicsPath(); int d = rad * 2; p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.Right - d, r.Y, d, d, 270, 90); p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90); p.AddArc(r.X, r.Bottom - d, d, d, 90, 90); p.CloseFigure(); return p; }
}

// Pill toggle with explicit bounds
sealed class Pill : Control
{
    bool _on, _ok = true, _hv;
    public bool On { get { return _on; } set { if (_on != value) { _on = value; Invalidate(); } } }
    public bool OK { get { return _ok; } set { _ok = value; Invalidate(); } }
    public event EventHandler Tog;
    public Pill() { SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true); Cursor = Cursors.Hand; }
    public void Click2() { if (!_ok) return; _on = !_on; Invalidate(); if (Tog != null) Tog(this, EventArgs.Empty); }
    protected override void OnClick(EventArgs e) { Click2(); }
    protected override void OnMouseEnter(EventArgs e) { _hv = true; Invalidate(); }
    protected override void OnMouseLeave(EventArgs e) { _hv = false; Invalidate(); }
    protected override void OnPaint(PaintEventArgs e) {
        Graphics g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias;
        Rectangle r = ClientRectangle;
        Color bg = !_ok ? Color.FromArgb(30, 30, 48) : _on ? C.Acc : _hv ? Color.FromArgb(52, 52, 78) : C.BgTogOff;
        using (Brush br = new SolidBrush(bg)) g.FillPath(br, RR(r, Height / 2));
        if (_on) using (Pen pn = new Pen(C.AccB, 1)) g.DrawPath(pn, RR(r, Height / 2));
        string txt = _on ? "\u5F00\u542F" : "\u5173\u95ED";
        Color tc = _on ? Color.White : (_ok ? C.Tg : C.Td);
        using (StringFormat sf = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center })
            g.DrawString(txt, Font, new SolidBrush(tc), r, sf);
    }
    static GraphicsPath RR(Rectangle r, int rad) {
        GraphicsPath p = new GraphicsPath(); int d = rad * 2;
        p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90); p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure(); return p;
    }
}

// Modifier row - box model: [pad][icon 36][gap 12][text fill][gap 12][toggle 72][pad]
sealed class Row : Panel
{
    readonly Control _icon; readonly LB _name, _desc;
    readonly Pill _tog;
    bool _hv, _av = true, _on;
    public bool IsOn { get { return _on; } set { _on = value; _tog.On = value; } }

    public Row(string icon, string name, string desc, Font nf, Font df, Action<bool> cb)
    {
        Dock = DockStyle.Top; SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true);
        BackColor = C.BgCard; Margin = new Padding(0); Padding = new Padding(0);
        _icon = new FeatureIcon(TileFor(name));
        _name = new LB { Text = name, Font = nf, ForeColor = C.Tw, AutoEllipsis = true };
        _desc = new LB { Text = desc, Font = df, ForeColor = C.Tg, AutoEllipsis = true };
        _tog = new Pill();
        _tog.Tog += delegate { _on = _tog.On; if (cb != null) cb(_tog.On); };
        Controls.Add(_icon); Controls.Add(_name); Controls.Add(_desc); Controls.Add(_tog);

        MouseClick += delegate(object s, MouseEventArgs e) { if (e.Button == MouseButtons.Left && _av) _tog.Click2(); };
        _name.MouseClick += delegate(object s, MouseEventArgs e) { if (e.Button == MouseButtons.Left && _av) _tog.Click2(); };
        _desc.MouseClick += delegate(object s, MouseEventArgs e) { if (e.Button == MouseButtons.Left && _av) _tog.Click2(); };
        MouseEnter += delegate { _hv = true; Invalidate(); };
        MouseLeave += delegate { _hv = false; Invalidate(); };
        _name.MouseEnter += delegate { _hv = true; Invalidate(); };
        _name.MouseLeave += delegate { _hv = false; Invalidate(); };
        _desc.MouseEnter += delegate { _hv = true; Invalidate(); };
        _desc.MouseLeave += delegate { _hv = false; Invalidate(); };
        Resize += DoLayout;
    }

    static int TileFor(string name) {
        if (name == "金钱不减") return 0; if (name == "电力无负载") return 1;
        if (name == "即时生产") return 2; if (name == "即时超武") return 3;
        if (name == "去除迷雾") return 4; if (name == "无视建筑距离") return 5;
        if (name == "自动修理") return 6; if (name == "驻军建筑修理") return 7;
        return 0;
    }

    public void SetAv(bool v) { _av = v; _tog.OK = v; }

    void DoLayout(object s, EventArgs e)
    {
        int pad = 16, iconW = 36, gap = 12, togW = 72, togH = 30;
        // Horizontal
        _icon.Location = new Point(pad, (Height - iconW) / 2);
        _icon.Size = new Size(iconW, iconW);
        int textX = pad + iconW + gap;
        int textW = Width - textX - gap - togW - pad;
        if (textW < 20) textW = 20;
        _name.Location = new Point(textX, 8);
        _name.Size = new Size(textW, 22);
        _desc.Location = new Point(textX, 32);
        _desc.Size = new Size(textW, 20);
        _tog.Location = new Point(Width - pad - togW, (Height - togH) / 2);
        _tog.Size = new Size(togW, togH);
    }

    protected override void OnPaint(PaintEventArgs e) {
        Rectangle card = new Rectangle(8, 3, Math.Max(1, Width - 16), Math.Max(1, Height - 6));
        using (Brush br = new SolidBrush(C.BgCard)) e.Graphics.FillPath(br, RR(card, 8));
        if (_hv && _av) {
            using (Brush br = new SolidBrush(C.BgHover)) e.Graphics.FillPath(br, RR(card, 8));
        }
        using (Pen pn = new Pen(C.Bd, 1)) e.Graphics.DrawPath(pn, RR(card, 8));
    }
    static GraphicsPath RR(Rectangle r, int rad) {
        GraphicsPath p = new GraphicsPath(); int d = rad * 2;
        p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90); p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure(); return p;
    }
}

// Section header
sealed class Sec : Panel
{
    readonly Control _icon; readonly LB _title; readonly Control _chev;
    bool _open = true;
    public bool Open { get { return _open; } }
    public event EventHandler Tog;

    public Sec(string icon, string title, Font tf)
    {
        Dock = DockStyle.Top; BackColor = C.BgSec; Cursor = Cursors.Hand;
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true);
        _icon = new IC { Text = icon, Font = new Font("Segoe UI Emoji", 13f), ForeColor = C.Acc };
        _title = new LB { Text = title, Font = tf, ForeColor = C.Tw };
        _chev = new IC { Text = "\u25BC", Font = new Font("Segoe UI", 10f), ForeColor = C.Td };
        Controls.Add(_icon); Controls.Add(_title); Controls.Add(_chev);
        EventHandler tg = delegate { _open = !_open; _chev.Text = _open ? "\u25BC" : "\u25B6"; if (Tog != null) Tog(this, EventArgs.Empty); };
        Click += tg; _icon.Click += tg; _title.Click += tg; _chev.Click += tg;
        Resize += DoLayout;
    }

    void DoLayout(object s, EventArgs e)
    {
        int pad = 16, iconW = 28, chevW = 28;
        _icon.Location = new Point(pad, (Height - iconW) / 2);
        _icon.Size = new Size(iconW, iconW);
        _title.Location = new Point(pad + iconW + 8, 0);
        _title.Size = new Size(Width - pad - iconW - 8 - chevW - pad, Height);
        _chev.Location = new Point(Width - chevW - pad, 0);
        _chev.Size = new Size(chevW, Height);
    }

    protected override void OnPaint(PaintEventArgs e) {
        using (Pen pn = new Pen(C.Bd, 1)) e.Graphics.DrawLine(pn, 0, 0, Width, 0);
    }
}

// Status
sealed class Stat : Control
{
    Color _c = C.Yel; string _t = "\u68C0\u67E5\u4E2D"; string _s = "";
    public void Set(Color c, string t, string s) { _c = c; _t = t; _s = s; Invalidate(); }
    public Stat() { SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true); }
    protected override void OnPaint(PaintEventArgs e) {
        Graphics g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias;
        using (Brush br = new SolidBrush(_c)) g.FillEllipse(br, 12, 8, 8, 8);
        using (Pen pn = new Pen(Color.FromArgb(40, _c), 6)) g.DrawEllipse(pn, 10, 6, 12, 12);
        g.DrawString(_t, Font, new SolidBrush(C.Tw), 28, 3);
        if (_s.Length > 0) g.DrawString(_s, new Font(Font.FontFamily, Font.Size - 1f), new SolidBrush(C.Tg), 28, 19);
    }
}

// Main
sealed class App : Form
{
    [DllImport("dwmapi.dll")] static extern int DwmSetWindowAttribute(IntPtr h, int a, ref int v, int s);
    [DllImport("user32.dll")] static extern bool ShowScrollBar(IntPtr hWnd, int wBar, bool bShow);
    readonly string _hub; readonly string _instantService; readonly string _superService; readonly Timer _tm; readonly TextBox _log;
    Stat _stat; readonly Panel _main;
    readonly Row[] _rows = new Row[8];
    readonly Panel[] _sc = new Panel[4];
    bool _po, _so, _ar, _gr;
    static Image GameBadge() { try { string p = @"C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Red Alert II\game.exe"; using (Icon i = File.Exists(p) ? Icon.ExtractAssociatedIcon(p) : null) return i == null ? null : i.ToBitmap(); } catch { return null; } }

    public App()
    {
        AutoScaleMode = AutoScaleMode.Dpi;
        Font = new Font("Microsoft YaHei UI", 9f, FontStyle.Regular, GraphicsUnit.Point);
        FormBorderStyle = FormBorderStyle.Sizable;
        ControlBox = true; MinimizeBox = true; MaximizeBox = false;
        Text = "Red Alert 2 \u00B7 \u5355\u673A\u6E38\u620F\u4FEE\u6539\u5668";
        Width = 920; Height = 680; MinimumSize = new Size(760, 560);
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = C.Bg;
        _hub = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "ra2_product_hub.exe");
        _instantService = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "ra2_product_instant_service.exe");
        _superService = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "ra2_product_super_service.exe");

        // Root: sidebar(fill=220px) + content(fill)
        Panel root = new SmoothPanel { Dock = DockStyle.Fill, BackColor = C.Bg };
        Panel side = BuildSide();
        side.Dock = DockStyle.Left; side.Width = 220;
        Panel content = new SmoothPanel { Dock = DockStyle.Fill, BackColor = C.Bg };
        root.Controls.Add(content); root.Controls.Add(side);

        // Content: main(scroll) + footer
        Panel foot = BuildFoot();
        foot.Dock = DockStyle.Bottom; foot.Height = 32;
        _main = new SmoothPanel { Dock = DockStyle.Fill, BackColor = C.Bg, AutoScroll = true, Padding = new Padding(12, 10, 12, 10) };
        _main.HorizontalScroll.Enabled = false; _main.HorizontalScroll.Visible = false;
        _main.Resize += delegate { ShowScrollBar(_main.Handle, 1, false); };
        content.Controls.Add(_main); content.Controls.Add(foot);

        Controls.Add(root);

        _log = new TextBox { Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, Dock = DockStyle.Fill, BackColor = C.BgCard, ForeColor = C.Tg, BorderStyle = BorderStyle.None, Font = new Font("Microsoft YaHei UI", 9f), Visible = false };

        BuildContent();
        ApplyChrome();
        _tm = new Timer { Interval = 3000 };
        _tm.Tick += delegate { Poll(false); };
        Shown += delegate { Exec("--self-check"); Poll(true); _tm.Start(); };
        FormClosed += delegate { _tm.Stop(); StopService("ra2_product_instant_service", "Local\\RA2ProductInstantProductionStop"); StopService("ra2_product_super_service", "Local\\RA2ProductSuperStop"); };
    }

    Panel BuildSide()
    {
        Panel side = new SmoothPanel { BackColor = C.BgSide };
        // Use Dock layout: top=logo area, fill=nav, bottom=status+version
        Panel topArea = new SmoothPanel { Dock = DockStyle.Top, Height = 168, BackColor = C.BgSide };
        Panel logo = new Panel { Width = 52, Height = 52, BackColor = C.BgCard, Location = new Point(16, 14) };
        Image badge = GameBadge();
        logo.Paint += delegate(object s, PaintEventArgs e) {
            Graphics g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias;
            using (Brush br = new SolidBrush(C.BgCard)) g.FillPath(br, RP(new Rectangle(0, 0, 52, 52), 10));
            using (Pen pn = new Pen(Color.FromArgb(125, C.Acc.R, C.Acc.G, C.Acc.B), 1)) g.DrawPath(pn, RP(new Rectangle(1, 1, 50, 50), 10));
            if (badge != null) { g.InterpolationMode = InterpolationMode.HighQualityBicubic; g.DrawImage(badge, new Rectangle(5, 5, 42, 42)); }
        };
        LB t1 = new LB { Text = "Red Alert 2", Font = new Font("Segoe UI", 15f, FontStyle.Bold), ForeColor = C.Tw, Location = new Point(16, 78), AutoSize = true };
        LB t2 = new LB { Text = "Steam \u00B7 V1.006", Font = new Font("Segoe UI", 9f), ForeColor = C.Tg, Location = new Point(16, 108), AutoSize = true };
        LB t3 = new LB { Text = "\u5355\u673A\u6A21\u5F0F  /  SINGLE PLAYER", Font = new Font("Segoe UI", 7.5f, FontStyle.Bold), ForeColor = C.Yel, Location = new Point(16, 130), AutoSize = true };
        topArea.Paint += delegate(object s, PaintEventArgs e) { using (Pen pn = new Pen(C.Bd, 1)) e.Graphics.DrawLine(pn, 16, 157, topArea.Width - 16, 157); };
        topArea.Controls.Add(logo); topArea.Controls.Add(t1); topArea.Controls.Add(t2); topArea.Controls.Add(t3);

        Panel botArea = new SmoothPanel { Dock = DockStyle.Bottom, Height = 124, BackColor = C.BgSide };
        _stat = new Stat { Dock = DockStyle.Top, Height = 40 };
        LB ver = new LB { Text = "v0.6.0", Dock = DockStyle.Top, Height = 24, ForeColor = C.Td, TextAlign = ContentAlignment.MiddleCenter };
        // Start button
        Panel btnP = new Panel { Dock = DockStyle.Top, Height = 36, Padding = new Padding(12, 4, 12, 4), BackColor = C.BgSide };
        RoundButton btn = new RoundButton { Text = "\u25B6  \u542F\u52A8\u6E38\u620F", Font = new Font("Microsoft YaHei UI", 9.5f, FontStyle.Bold), ForeColor = Color.White, BackColor = C.Grn, Dock = DockStyle.Fill };
        btn.FlatAppearance.BorderSize = 0;
        btn.Click += delegate { try { Process.Start("steam://rungameid/2229850"); } catch { } };
        btnP.Controls.Add(btn);
        botArea.Controls.Add(ver); botArea.Controls.Add(_stat); botArea.Controls.Add(btnP);

        // Nav (fill)
        Panel nav = new SmoothPanel { Dock = DockStyle.Fill, BackColor = C.BgSide };
        string[] ico = { "\uD83D\uDCB0", "\u2699\uFE0F", "\uD83D\uDDFA\uFE0F", "\uD83D\uDD27" };
        string[] ttl = { "\u8D44\u6E90\u4E0E\u7535\u529B", "\u751F\u4EA7\u4E0E\u6B66\u5668", "\u6218\u573A\u8F85\u52A9", "\u7EF4\u4FEE\u4E0E\u5EFA\u9020" };
        int ny = 0;
        for (int i = 0; i < 4; i++) {
            int idx = i;
            Panel item = new SmoothPanel { Dock = DockStyle.Top, Height = 50, BackColor = Color.Transparent, Cursor = Cursors.Hand };
            IC il = new IC { Text = ico[i], Font = new Font("Segoe UI Emoji", 12f), ForeColor = C.Acc, Location = new Point(16, 0), Size = new Size(28, 50) };
            LB tl = new LB { Text = ttl[i], Font = new Font("Microsoft YaHei UI", 9.5f, FontStyle.Bold), ForeColor = C.Tw, Location = new Point(52, 0), Size = new Size(160, 50), TextAlign = ContentAlignment.MiddleLeft };
            item.Controls.Add(il); item.Controls.Add(tl);
            item.Paint += delegate(object s, PaintEventArgs e) { if (idx == 0) using (Brush br = new SolidBrush(C.BgActive)) e.Graphics.FillRectangle(br, 0, 0, 3, item.Height); };
            item.Click += delegate { NavTo(idx); }; il.Click += delegate { NavTo(idx); }; tl.Click += delegate { NavTo(idx); };
            item.MouseEnter += delegate { if (idx != 0) item.BackColor = C.BgHover; };
            item.MouseLeave += delegate { if (idx != 0) item.BackColor = Color.Transparent; };
            il.MouseEnter += delegate { if (idx != 0) item.BackColor = C.BgHover; };
            il.MouseLeave += delegate { if (idx != 0) item.BackColor = Color.Transparent; };
            tl.MouseEnter += delegate { if (idx != 0) item.BackColor = C.BgHover; };
            tl.MouseLeave += delegate { if (idx != 0) item.BackColor = Color.Transparent; };
            nav.Controls.Add(item);
        }

        side.Controls.Add(nav);
        side.Controls.Add(botArea);
        side.Controls.Add(topArea);
        return side;
    }

    void BuildContent()
    {
        _main.Controls.Clear();
        BuildSec(3, "\uD83D\uDD27", "\u7EF4\u4FEE\u4E0E\u5EFA\u9020", new string[][] {
            new string[] { "\u2699\uFE0F", "\u81EA\u52A8\u4FEE\u7406", "\u5DF2\u65B9\u5EFA\u7B51\u81EA\u52A8\u6062\u590D\u751F\u547D\u503C", "--repair-auto-on", "--repair-auto-off" },
            new string[] { "\uD83C\uDFE0", "\u9A7B\u519B\u5EFA\u7B51\u4FEE\u7406", "\u5DF2\u65B9\u5355\u4F4D\u9A7B\u519B\u7684\u6C11\u7528\u5EFA\u7B51\u81EA\u52A8\u4FEE\u7406", "--repair-garrison-on", "--repair-garrison-off" }
        });
        BuildSec(2, "\uD83D\uDDFA\uFE0F", "\u6218\u573A\u8F85\u52A9", new string[][] {
            new string[] { "\uD83D\uDD0D", "\u53BB\u9664\u8FF7\u96FE", "\u663E\u793A\u5B8C\u6574\u5730\u56FE\u89C6\u91CE", "--fog-on", "--fog-off" },
            new string[] { "\uD83D\uDCCF", "\u65E0\u89C6\u5EFA\u7B51\u8DDD\u79BB", "\u4FDD\u7559\u5730\u5F62\u4E0E\u5360\u7528\u68C0\u67E5\uFF0C\u81EA\u7531\u9009\u62E9\u5EFA\u9020\u4F4D\u7F6E", "--distance-on", "--distance-off" }
        });
        BuildSec(1, "\u2699\uFE0F", "\u751F\u4EA7\u4E0E\u6B66\u5668", new string[][] {
            new string[] { "\u2699\uFE0F", "\u5373\u65F6\u751F\u4EA7", "\u5355\u4F4D\u4E0E\u5EFA\u7B51\u5B8C\u6210\u751F\u4EA7\u65E0\u9700\u7B49\u5F85", "--production-toggle", "--production-toggle" },
            new string[] { "\uD83C\uDF1F", "\u5373\u65F6\u8D85\u6B66", "\u8D85\u7EA7\u6B66\u5668\u5C31\u7EEA\u540E\u53EF\u7ACB\u5373\u4F7F\u7528", "--super-toggle", "--super-toggle" }
        });
        BuildSec(0, "\uD83D\uDCB0", "\u8D44\u6E90\u4E0E\u7535\u529B", new string[][] {
            new string[] { "\uD83D\uDCB5", "\u91D1\u94B1\u4E0D\u51CF", "\u82B1\u8D39\u91D1\u94B1\u4E0D\u518D\u51CF\u5C11\uFF0C\u53EF\u6B63\u5E38\u83B7\u5F97\u6536\u5165", "--money-on", "--money-off" },
            new string[] { "\u26A1", "\u7535\u529B\u65E0\u8D1F\u8F7D", "\u4E0D\u53D7\u57FA\u5730\u7535\u529B\u8D1F\u8F7D\u9650\u5236", "--power-on", "--power-off" }
        });
        _main.AutoScrollPosition = new Point(0, 0);
    }

    void BuildSec(int idx, string icon, string title, string[][] features)
    {
        Panel wrap = new SmoothPanel { Dock = DockStyle.Top, BackColor = C.BgSec };
        Sec bar = new Sec(icon, title, new Font("Microsoft YaHei UI", 11f, FontStyle.Bold)) { Dock = DockStyle.Top, Height = 44 };
        Panel content = new SmoothPanel { Dock = DockStyle.Top, BackColor = C.BgSec };
        int rowH = 60;
        content.Height = features.Length * rowH;
        int br = idx * 2;
        Font nf = new Font("Microsoft YaHei UI", 10.5f);
        Font df = new Font("Microsoft YaHei UI", 8.5f);
        for (int i = features.Length - 1; i >= 0; i--) {
            string[] f = features[i];
            string onC = f[3], offC = f[4];
            int ri = br + i;
            Row row = new Row(f[0], f[1], f[2], nf, df, delegate(bool en) {
                if (onC == "--production-toggle") ToggleProd();
                else if (onC == "--super-toggle") ToggleSuper();
                else Exec(en ? onC : offC);
            }) { Dock = DockStyle.Top, Height = rowH };
            _rows[ri] = row;
            content.Controls.Add(row);
        }
        bar.Tog += delegate { content.Visible = bar.Open; wrap.Height = bar.Height + (bar.Open ? content.Height : 0); };
        wrap.Controls.Add(content);
        wrap.Controls.Add(bar);
        wrap.Height = bar.Height + content.Height;
        _sc[idx] = content;
        _main.Controls.Add(wrap);
    }

    Panel BuildFoot()
    {
        Panel foot = new SmoothPanel { BackColor = C.BgHdr };
        foot.Paint += delegate(object s, PaintEventArgs e) { using (Pen pn = new Pen(C.Bd, 1)) e.Graphics.DrawLine(pn, 0, 0, foot.Width, 0); };
        LB l = new LB { Text = "\u25CF  \u6240\u6709\u4FEE\u6539\u53EF\u968F\u65F6\u5173\u95ED \u00B7 \u9000\u51FA\u6E38\u620F\u540E\u81EA\u52A8\u5931\u6548", Font = new Font("Microsoft YaHei UI", 8f), ForeColor = C.Grn, Dock = DockStyle.Left, Width = 400, TextAlign = ContentAlignment.MiddleLeft };
        LB r = new LB { Text = "\u7B49\u5F85\u64CD\u4F5C", Font = new Font("Microsoft YaHei UI", 8f), ForeColor = C.Td, Dock = DockStyle.Right, Width = 80, TextAlign = ContentAlignment.MiddleRight };
        Button lb = new Button { Text = "\u65E5\u5FD7", FlatStyle = FlatStyle.Flat, Font = new Font("Microsoft YaHei UI", 8f), ForeColor = C.Tg, BackColor = C.BgCard, Width = 50, Height = 22, Dock = DockStyle.Right, TabStop = false };
        lb.FlatAppearance.BorderColor = C.Bd;
        lb.Click += delegate { ShowLog(); };
        foot.Controls.Add(l); foot.Controls.Add(r); foot.Controls.Add(lb);
        return foot;
    }

    void NavTo(int idx) {
        if (idx >= 0 && idx < 4 && _sc[idx] != null && _sc[idx].Parent != null)
            _main.AutoScrollPosition = new Point(0, _sc[idx].Parent.Location.Y);
    }

    bool Exec(string a) { return Exec(a, true); }
    bool Exec(string a, bool rec) {
        if (!File.Exists(_hub)) { Log("\u6838\u5FC3\u7EC4\u4EF6\u4E0D\u5B58\u5728"); return false; }
        try {
            ProcessStartInfo psi = new ProcessStartInfo(_hub, a) {
                UseShellExecute = false, CreateNoWindow = true,
                RedirectStandardOutput = true, RedirectStandardError = true,
                StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8
            };
            using (Process p = Process.Start(psi)) {
                string o = p.StandardOutput.ReadToEnd() + p.StandardError.ReadToEnd();
                p.WaitForExit();
                if (rec) Log("[" + a + "]\r\n" + o);
                if (a == "--status") UpdateState(o);
                if (rec && a != "--status" && a != "--self-check") Exec("--status");
                return p.ExitCode == 0;
            }
        } catch (Exception ex) { Log("\u5931\u8D25: " + ex.Message); return false; }
    }

    void Poll(bool r) { Exec("--status", r); }
    void ToggleProd() { if (!_po) { if (StartService(_instantService)) _po = true; } else { StopService("ra2_product_instant_service", "Local\\RA2ProductInstantProductionStop"); _po = false; } Sync(); }
    void ToggleSuper() { if (!_so) { if (StartService(_superService)) _so = true; } else { StopService("ra2_product_super_service", "Local\\RA2ProductSuperStop"); _so = false; } Sync(); }

    bool StartService(string path) { if (!File.Exists(path)) { Log("缺少后台即时服务: " + Path.GetFileName(path)); return false; } string name = Path.GetFileNameWithoutExtension(path); foreach (Process p in Process.GetProcessesByName(name)) { try { if (!p.HasExited) return true; } catch { } } try { string ev = name == "ra2_product_instant_service" ? "Local\\RA2ProductInstantProductionStop" : name == "ra2_product_super_service" ? "Local\\RA2ProductSuperStop" : ""; if (ev.Length > 0) using (System.Threading.EventWaitHandle h = new System.Threading.EventWaitHandle(false, System.Threading.EventResetMode.ManualReset, ev)) h.Reset(); Process.Start(new ProcessStartInfo(path) { WorkingDirectory = Path.GetDirectoryName(path), UseShellExecute = false, CreateNoWindow = true }); return true; } catch (Exception ex) { Log("启动后台服务失败: " + ex.Message); return false; } }
    void StopService(string name, string eventName) { try { using (System.Threading.EventWaitHandle e = new System.Threading.EventWaitHandle(false, System.Threading.EventResetMode.ManualReset, eventName)) { e.Set(); } } catch { } foreach (Process p in Process.GetProcessesByName(name)) { try { if (!p.HasExited) p.WaitForExit(2500); } catch { } } }

    void UpdateState(string o) {
        bool ok = o.IndexOf("version=MATCHED", StringComparison.OrdinalIgnoreCase) >= 0;
        bool task = ok && o.IndexOf("production=NOT_IN_TASK", StringComparison.OrdinalIgnoreCase) < 0 && o.IndexOf("superweapons=NOT_IN_TASK", StringComparison.OrdinalIgnoreCase) < 0;
        string pid = F(o, "pid=");
        if (!ok) _stat.Set(C.Red, "\u6E38\u620F\u672A\u8FDE\u63A5", "\u8BF7\u5148\u542F\u52A8\u6E38\u620F\u5E76\u8FDB\u5165\u5355\u4EBA\u4EFB\u52A1");
        else if (!task) _stat.Set(C.Yel, "\u7B49\u5F85\u8FDB\u5165\u4EFB\u52A1", "\u8FDB\u5165\u5355\u4EBA\u4EFB\u52A1\u6216\u906D\u9047\u6218\u540E\u529F\u80FD\u5C06\u89E3\u9501");
        else _stat.Set(C.Grn, "\u5DF2\u8FDE\u63A5 \u00B7 \u8FD0\u884C\u4E2D", pid.Length > 0 ? "PID " + pid : "");
        bool av = ok && task;
        for (int i = 0; i < _rows.Length; i++) if (_rows[i] != null) _rows[i].SetAv((i == 2 || i == 3 || i == 6) ? ok : av);
        _po = Process.GetProcessesByName("ra2_product_instant_service").Length > 0;
        _so = Process.GetProcessesByName("ra2_product_super_service").Length > 0;
        _ar = o.IndexOf("auto_repair=ON", StringComparison.OrdinalIgnoreCase) >= 0;
        _gr = o.IndexOf("garrison_repair=ON", StringComparison.OrdinalIgnoreCase) >= 0;
        Sync();
    }

    void Sync() { if (_rows[2] != null) _rows[2].IsOn = _po; if (_rows[3] != null) _rows[3].IsOn = _so; if (_rows[6] != null) _rows[6].IsOn = _ar; if (_rows[7] != null) _rows[7].IsOn = _gr; }

    static string F(string o, string k) {
        int p = o.IndexOf(k, StringComparison.OrdinalIgnoreCase);
        if (p < 0) return ""; p += k.Length; int e = p;
        while (e < o.Length && !char.IsWhiteSpace(o[e])) e++;
        return o.Substring(p, e - p).Trim();
    }

    void Log(string t) { _log.AppendText(DateTime.Now.ToString("HH:mm:ss") + "  " + t + "\r\n"); }

    void ShowLog() {
        Form d = new Form { Text = "\u64CD\u4F5C\u65E5\u5FD7", Width = 700, Height = 400, StartPosition = FormStartPosition.CenterParent, BackColor = C.Bg, ForeColor = C.Tw, MinimizeBox = false, MaximizeBox = false };
        TextBox b = new TextBox { Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, Dock = DockStyle.Fill, BackColor = C.BgCard, ForeColor = C.Tg, BorderStyle = BorderStyle.None, Font = new Font("Microsoft YaHei UI", 9f), Text = _log.Text };
        Panel bp = new Panel { Dock = DockStyle.Bottom, Height = 36, BackColor = C.BgHdr };
        Button cp = MB("\u590D\u5236", 64, delegate { if (b.Text.Length > 0) Clipboard.SetText(b.Text); }); cp.Location = new Point(10, 4);
        Button ex = MB("\u5BFC\u51FA", 64, delegate { SaveFileDialog sv = new SaveFileDialog { Filter = "*.txt|*.txt", FileName = "ra2-log.txt" }; if (sv.ShowDialog() == DialogResult.OK) File.WriteAllText(sv.FileName, b.Text, Encoding.UTF8); }); ex.Location = new Point(82, 4);
        Button cl = MB("\u5173\u95ED", 64, delegate { d.Close(); }); cl.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        bp.Controls.AddRange(new Control[] { cp, ex, cl });
        d.Controls.Add(b); d.Controls.Add(bp); d.ShowDialog(this);
    }

    static Button MB(string t, int w, EventHandler h) {
        RoundButton b = new RoundButton { Text = t, Font = new Font("Microsoft YaHei UI", 8.5f), ForeColor = Color.White, BackColor = C.Grn, Width = w, Height = 26 };
        b.Click += h; return b;
    }

    void ApplyChrome() {
        try { int d = 1; DwmSetWindowAttribute(Handle, 20, ref d, 4); int c = 0x002E1A1A; DwmSetWindowAttribute(Handle, 35, ref c, 4); int t = 0x00FFFFFF; DwmSetWindowAttribute(Handle, 36, ref t, 4); } catch { }
        try { string gp = @"C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Red Alert II\game.exe"; if (File.Exists(gp)) Icon = Icon.ExtractAssociatedIcon(gp); } catch { }
    }

    static GraphicsPath RP(Rectangle r, int rad) {
        GraphicsPath p = new GraphicsPath(); int d = rad * 2;
        p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90); p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure(); return p;
    }

    [STAThread]
    static void Main() {
        try { Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false); Application.Run(new App()); }
        catch (Exception ex) { try { File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "ui-crash.txt"), ex.ToString(), Encoding.UTF8); } catch { } MessageBox.Show("\u542F\u52A8\u5931\u8D25: " + ex.Message, "RA2 Modifier", MessageBoxButtons.OK, MessageBoxIcon.Error); }
    }
}
