using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

internal sealed class SwitchControl : Control
{
    bool value;
    public bool Interactive { get; set; }
    public bool Value { get { return value; } set { if (this.value != value) { this.value = value; Invalidate(); } } }
    public event EventHandler ValueChanged;
    public SwitchControl() { Width = 64; Height = 30; Cursor = Cursors.Hand; Interactive = true; DoubleBuffered = true; SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true); }
    public void Toggle() { Value = !Value; if (ValueChanged != null) ValueChanged(this, EventArgs.Empty); }
    protected override void OnClick(EventArgs e) { base.OnClick(e); if (Interactive) Toggle(); }
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e); e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        Rectangle r = new Rectangle(1, 3, Width - 2, Height - 6);
        using (Brush b = new SolidBrush(Value ? Color.FromArgb(30, 191, 105) : Color.FromArgb(55, 72, 91))) e.Graphics.FillPath(b, Round(r, 14));
        using (Pen p = new Pen(Value ? Color.FromArgb(98, 235, 153) : Color.FromArgb(104, 126, 148), 1)) e.Graphics.DrawPath(p, Round(r, 14));
        using (Brush b = new SolidBrush(Value ? Color.FromArgb(222, 255, 232) : Color.FromArgb(190, 202, 215))) e.Graphics.FillEllipse(b, Value ? new Rectangle(Width - 26, 7, 16, 16) : new Rectangle(10, 7, 16, 16));
    }
    static GraphicsPath Round(Rectangle r, int radius) { GraphicsPath p = new GraphicsPath(); int d = radius * 2; p.AddArc(r.X, r.Y, d, d, 90, 180); p.AddArc(r.Right - d, r.Y, d, d, 270, 180); p.CloseFigure(); return p; }
}

internal sealed class FeatureBadge : Control
{
    readonly int kind;
    static Image sprite;
    static bool spriteChecked;
    static readonly Rectangle[] spriteSlices = {
        new Rectangle(35, 38, 376, 369), new Rectangle(478, 38, 374, 369), new Rectangle(920, 38, 374, 369), new Rectangle(1363, 38, 374, 369),
        new Rectangle(35, 467, 376, 366), new Rectangle(478, 467, 374, 366), new Rectangle(920, 467, 374, 366), new Rectangle(1363, 467, 374, 366)
    };
    public FeatureBadge(int featureKind) { kind = featureKind; Width = 30; Height = 30; BackColor = Color.FromArgb(15, 28, 43); SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true); }
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e); e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        if (!spriteChecked) { spriteChecked = true; try { string path = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "assets", "feature-icons.png"); if (File.Exists(path)) sprite = Image.FromFile(path); } catch { sprite = null; } }
        if (sprite != null && kind >= 0 && kind < spriteSlices.Length) { e.Graphics.DrawImage(sprite, new Rectangle(1, 1, 28, 28), spriteSlices[kind].X, spriteSlices[kind].Y, spriteSlices[kind].Width, spriteSlices[kind].Height, GraphicsUnit.Pixel); return; }
        using (Brush b = new SolidBrush(Color.FromArgb(25, 56, 83))) e.Graphics.FillPath(b, Round(new Rectangle(1, 1, 28, 28), 7));
        using (Pen p = new Pen(Color.FromArgb(73, 166, 230), 1.5F))
        using (Brush b = new SolidBrush(Color.FromArgb(136, 216, 255)))
        {
            if (kind == 0) { e.Graphics.DrawEllipse(p, 8, 8, 12, 7); e.Graphics.DrawEllipse(p, 9, 12, 12, 7); e.Graphics.FillEllipse(b, 12, 9, 4, 3); }
            else if (kind == 1) { Point[] pts = { new Point(17, 5), new Point(10, 16), new Point(15, 16), new Point(12, 25), new Point(22, 13), new Point(17, 13) }; e.Graphics.FillPolygon(b, pts); }
            else if (kind == 2) { e.Graphics.FillRectangle(b, 7, 14, 16, 10); e.Graphics.FillRectangle(b, 10, 9, 10, 6); e.Graphics.DrawLine(p, 5, 24, 25, 24); }
            else if (kind == 3) { e.Graphics.DrawEllipse(p, 7, 7, 16, 16); e.Graphics.DrawEllipse(p, 11, 11, 8, 8); e.Graphics.FillEllipse(b, 14, 14, 3, 3); }
            else if (kind == 4) { e.Graphics.DrawEllipse(p, 5, 10, 20, 10); e.Graphics.FillEllipse(b, 12, 13, 6, 6); }
            else if (kind == 5) { e.Graphics.DrawEllipse(p, 8, 8, 14, 14); e.Graphics.DrawLine(p, 15, 4, 15, 26); e.Graphics.DrawLine(p, 4, 15, 26, 15); }
            else if (kind == 6) { e.Graphics.DrawLine(p, 8, 22, 19, 11); e.Graphics.DrawLine(p, 6, 24, 10, 20); e.Graphics.DrawLine(p, 18, 10, 22, 6); e.Graphics.DrawEllipse(p, 17, 5, 7, 7); }
            else { e.Graphics.DrawRectangle(p, 8, 10, 14, 14); e.Graphics.DrawLine(p, 6, 10, 24, 10); e.Graphics.DrawLine(p, 12, 24, 12, 17); e.Graphics.DrawLine(p, 18, 24, 18, 17); }
        }
    }
    static GraphicsPath Round(Rectangle r, int radius) { GraphicsPath p = new GraphicsPath(); int d = radius * 2; p.AddArc(r.X, r.Y, d, d, 90, 180); p.AddArc(r.Right - d, r.Y, d, d, 270, 180); p.CloseFigure(); return p; }
}

internal sealed class FeatureRow : Panel
{
    readonly Label title = new Label(); readonly Label description = new Label(); readonly Label status = new Label(); readonly SwitchControl toggle = new SwitchControl(); readonly FeatureBadge badge;
    bool hovered; bool available = true;
    public bool EnabledState { get { return toggle.Value; } set { toggle.Value = value; if (available) { status.Text = value ? "已开启" : "已关闭"; status.ForeColor = value ? Color.FromArgb(92, 231, 148) : Color.FromArgb(147, 163, 180); } } }
    public void SetAvailable(bool value) { available = value; Enabled = true; toggle.Interactive = value; if (!value) { status.Text = "需连接游戏"; status.ForeColor = Color.FromArgb(191, 139, 83); } else { status.Text = toggle.Value ? "已开启" : "已关闭"; status.ForeColor = toggle.Value ? Color.FromArgb(92, 231, 148) : Color.FromArgb(147, 163, 180); } }
    public FeatureRow(string name, string desc, int iconKind, Action<bool> changed)
    {
        Height = 78; Dock = DockStyle.Top; BackColor = Color.FromArgb(8, 20, 32); Padding = new Padding(18, 9, 18, 9);
        badge = new FeatureBadge(iconKind); badge.Location = new Point(12, 19); title.Text = name; title.BackColor = Color.Transparent; title.ForeColor = Color.FromArgb(236, 242, 248); title.Font = new Font("Microsoft YaHei UI", 11F, FontStyle.Bold); title.AutoSize = true; title.Location = new Point(54, 10);
        description.Text = desc; description.BackColor = Color.Transparent; description.ForeColor = Color.FromArgb(139, 158, 178); description.Font = new Font("Microsoft YaHei UI", 8.5F); description.AutoSize = true; description.Location = new Point(54, 36);
        status.Text = "已关闭"; status.BackColor = Color.Transparent; status.ForeColor = Color.FromArgb(147, 163, 180); status.Font = new Font("Microsoft YaHei UI", 8.5F); status.AutoSize = true; status.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        toggle.Anchor = AnchorStyles.Top | AnchorStyles.Right; toggle.ValueChanged += delegate { status.Text = toggle.Value ? "已开启" : "已关闭"; status.ForeColor = toggle.Value ? Color.FromArgb(92, 231, 148) : Color.FromArgb(147, 163, 180); if (changed != null) changed(toggle.Value); };
        Controls.Add(badge); Controls.Add(title); Controls.Add(description); Controls.Add(status); Controls.Add(toggle); Resize += delegate { toggle.Location = new Point(Width - 84, 20); status.Location = new Point(Width - 145, 27); };
        MouseEventHandler rowClick = delegate(object sender, MouseEventArgs e) { if (e.Button == MouseButtons.Left && available) toggle.Toggle(); };
        MouseClick += rowClick; badge.MouseClick += rowClick; title.MouseClick += rowClick; description.MouseClick += rowClick; status.MouseClick += rowClick;
        EventHandler enter = delegate { hovered = true; Invalidate(); };
        EventHandler leave = delegate { hovered = false; Invalidate(); };
        MouseEnter += enter; MouseLeave += leave; title.MouseEnter += enter; title.MouseLeave += leave; description.MouseEnter += enter; description.MouseLeave += leave; status.MouseEnter += enter; status.MouseLeave += leave;
    }
    protected override void OnPaintBackground(PaintEventArgs e)
    {
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        using (Brush bg = new SolidBrush(Color.FromArgb(8, 20, 32))) e.Graphics.FillRectangle(bg, ClientRectangle);
        Rectangle r = new Rectangle(7, 4, Width - 14, Height - 8);
        Color top = hovered ? Color.FromArgb(25, 50, 72) : Color.FromArgb(14, 31, 47);
        Color bottom = hovered ? Color.FromArgb(17, 38, 57) : Color.FromArgb(10, 25, 39);
        using (LinearGradientBrush b = new LinearGradientBrush(r, top, bottom, LinearGradientMode.Vertical)) e.Graphics.FillPath(b, Round(r, 8));
        using (Pen p = new Pen(hovered ? Color.FromArgb(57, 93, 119) : Color.FromArgb(29, 54, 75), 1)) e.Graphics.DrawPath(p, Round(r, 8));
        using (Brush b = new SolidBrush(toggle.Value ? Color.FromArgb(196, 47, 58) : Color.FromArgb(68, 91, 111))) e.Graphics.FillPath(b, Round(new Rectangle(7, 4, 4, Height - 8), 2));
    }
    protected override void OnPaint(PaintEventArgs e) { base.OnPaint(e); using (Pen p = new Pen(Color.FromArgb(30, 56, 76))) e.Graphics.DrawLine(p, 64, Height - 1, Width - 22, Height - 1); }
    static GraphicsPath Round(Rectangle r, int radius) { GraphicsPath p = new GraphicsPath(); int d = radius * 2; p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.Right - d, r.Y, d, d, 270, 90); p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90); p.AddArc(r.X, r.Bottom - d, d, d, 90, 90); p.CloseFigure(); return p; }
}

internal sealed class FeatureSection : Panel
{
    int nextY = 60;
    public FeatureSection(string titleText, string subtitle)
    {
        Dock = DockStyle.None; BackColor = Color.FromArgb(8, 20, 32); Padding = new Padding(12, 10, 12, 0); Margin = new Padding(0, 0, 0, 14); Height = 60;
        Paint += delegate(object s, PaintEventArgs e) { using (Brush b = new SolidBrush(Color.FromArgb(191, 43, 54))) e.Graphics.FillRectangle(b, 12, 15, 4, 27); using (Pen p = new Pen(Color.FromArgb(31, 56, 76))) e.Graphics.DrawLine(p, 18, 51, Width - 12, 51); };
        Controls.Add(new Label { Text = titleText, BackColor = Color.Transparent, ForeColor = Color.FromArgb(245, 248, 251), Font = new Font("Microsoft YaHei UI", 12F, FontStyle.Bold), AutoSize = true, Location = new Point(28, 10) });
        Controls.Add(new Label { Text = subtitle, BackColor = Color.Transparent, ForeColor = Color.FromArgb(129, 153, 174), Font = new Font("Microsoft YaHei UI", 8.5F), AutoSize = true, Location = new Point(29, 34) });
        Resize += delegate { foreach (Control c in Controls) if (c is FeatureRow) c.Width = Math.Max(260, Width - 24); };
    }
    public void AddFeature(FeatureRow row) { row.Dock = DockStyle.None; row.Width = Math.Max(260, Width - 24); row.Location = new Point(12, nextY); Controls.Add(row); row.BringToFront(); nextY += row.Height; Height += row.Height; }
}

internal sealed class Ra2ProductUi : Form
{
    [DllImport("user32.dll")] static extern bool ReleaseCapture();
    [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr hWnd, int msg, IntPtr wParam, IntPtr lParam);
    [DllImport("dwmapi.dll")] static extern int DwmSetWindowAttribute(IntPtr hwnd, int attribute, ref int value, int valueSize);
    const int WM_NCLBUTTONDOWN = 0xA1; const int HTCAPTION = 0x2;
    readonly string hub; readonly Label connection = new Label(); readonly Label pidLabel = new Label(); readonly Label session = new Label(); readonly Label lastAction = new Label(); readonly TextBox activity = new TextBox(); readonly Panel content = new Panel(); readonly FeatureRow[] rows = new FeatureRow[8]; readonly Label[] navItems = new Label[4]; Button closeAllButton; FeatureSection resourcesSection, productionSection, battlefieldSection, repairSection; readonly Timer monitor = new Timer(); bool productionOn, superOn; int activeNav;
    static readonly Color Background = Color.FromArgb(7, 18, 29);

    public Ra2ProductUi()
    {
        AutoScaleMode = AutoScaleMode.None; FormBorderStyle = FormBorderStyle.Sizable; ControlBox = true; MinimizeBox = true; MaximizeBox = false; Text = "红色警戒2 · 单机游戏修改器"; Width = 900; Height = 650; MinimumSize = new Size(820, 560); StartPosition = FormStartPosition.CenterScreen; BackColor = Background; Opacity = 1.0; hub = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "ra2_product_hub.exe");
        BuildShell(); BuildSidebar(); BuildMain(); NormalizeTextSurfaces(this); SetActiveNav(0); Shown += delegate { ApplyNativeChrome(); Execute("--self-check"); Execute("--status"); monitor.Interval = 3000; monitor.Tick += delegate { Execute("--status", false); }; monitor.Start(); }; FormClosed += delegate { monitor.Stop(); };
    }
    static void NormalizeTextSurfaces(Control root)
    {
        foreach (Control c in root.Controls) { if (c is Label) c.BackColor = Color.Transparent; if (c.HasChildren) NormalizeTextSurfaces(c); }
    }
    void ApplyNativeChrome()
    {
        try { int dark = 1; DwmSetWindowAttribute(Handle, 20, ref dark, 4); int caption = 10 | (25 << 8) | (39 << 16); DwmSetWindowAttribute(Handle, 35, ref caption, 4); int text = 0x00FFFFFF; DwmSetWindowAttribute(Handle, 36, ref text, 4); } catch { }
    }
    void BuildShell()
    {
        Panel header = new Panel { Dock = DockStyle.Top, Height = 68, BackColor = Color.FromArgb(6, 15, 25) };
        header.MouseDown += DragWindow;
        header.Paint += delegate(object s, PaintEventArgs e) { using (Pen p = new Pen(Color.FromArgb(188, 47, 58), 2)) e.Graphics.DrawLine(p, 0, header.Height - 1, header.Width, header.Height - 1); };
        header.Controls.Add(new Label { Text = "红色警戒2", ForeColor = Color.White, Font = new Font("Microsoft YaHei UI", 18F, FontStyle.Bold), AutoSize = true, Location = new Point(24, 13) });
        header.Controls.Add(new Label { Text = "单机游戏修改器  ·  经典原版", ForeColor = Color.FromArgb(116, 140, 163), Font = new Font("Microsoft YaHei UI", 9F), AutoSize = true, Location = new Point(26, 42) });
        connection.Text = "●  游戏进程检查中"; connection.ForeColor = Color.FromArgb(242, 187, 77); connection.Font = new Font("Microsoft YaHei UI", 10F, FontStyle.Bold); connection.AutoSize = true; connection.Anchor = AnchorStyles.Top | AnchorStyles.Right; connection.Location = new Point(560, 18); header.Controls.Add(connection);
        pidLabel.Text = "PID —"; pidLabel.ForeColor = Color.FromArgb(111, 139, 161); pidLabel.Font = new Font("Microsoft YaHei UI", 8F); pidLabel.AutoSize = true; pidLabel.Anchor = AnchorStyles.Top | AnchorStyles.Right; pidLabel.Location = new Point(562, 39); header.Controls.Add(pidLabel);
        Controls.Add(header);
    }
    void DragWindow(object sender, MouseEventArgs e) { if (e.Button == MouseButtons.Left) { ReleaseCapture(); SendMessage(Handle, WM_NCLBUTTONDOWN, (IntPtr)HTCAPTION, IntPtr.Zero); } }
    void BuildSidebar()
    {
        Panel side = new Panel { Dock = DockStyle.Left, Width = 228, BackColor = Color.FromArgb(9, 23, 37), Padding = new Padding(14, 18, 14, 14) };
        side.Paint += delegate(object s, PaintEventArgs e) { using (LinearGradientBrush b = new LinearGradientBrush(side.ClientRectangle, Color.FromArgb(13, 34, 52), Color.FromArgb(7, 18, 29), LinearGradientMode.Vertical)) e.Graphics.FillRectangle(b, side.ClientRectangle); using (Pen p = new Pen(Color.FromArgb(28, 55, 76))) e.Graphics.DrawLine(p, side.Width - 1, 0, side.Width - 1, side.Height); using (Brush b = new SolidBrush(Color.FromArgb(180, 38, 50))) e.Graphics.FillRectangle(b, 0, 0, 3, side.Height); };
        try { string gamePath = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe"; if (File.Exists(gamePath)) Icon = Icon.ExtractAssociatedIcon(gamePath); } catch { }
        PictureBox logo = new PictureBox { Size = new Size(42, 42), Location = new Point(18, 18), SizeMode = PictureBoxSizeMode.CenterImage, BackColor = Color.FromArgb(19, 42, 61) };
        try { string gamePath = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe"; if (File.Exists(gamePath)) logo.Image = Icon.ExtractAssociatedIcon(gamePath).ToBitmap(); } catch { }
        side.Controls.Add(logo);
        side.Controls.Add(new Label { Text = "红警2原版\r\nSteam · V1.006", ForeColor = Color.White, Font = new Font("Microsoft YaHei UI", 12F, FontStyle.Bold), AutoSize = true, Location = new Point(72, 20) });
        side.Controls.Add(new Label { Text = "SINGLE-PLAYER EDITION", ForeColor = Color.FromArgb(191, 62, 70), Font = new Font("Microsoft YaHei UI", 7.5F, FontStyle.Bold), AutoSize = true, Location = new Point(74, 65) });
        side.Controls.Add(new Label { Text = "只作用于当前单机游戏会话", ForeColor = Color.FromArgb(114, 141, 164), Font = new Font("Microsoft YaHei UI", 8.5F), AutoSize = true, Location = new Point(20, 76) });
        string[] nav = { "资源与电力", "生产与超武", "战场辅助", "维修与建造" }; int y = 132;
        int navIndex = 0; foreach (string n in nav) { int currentIndex = navIndex; Label item = new Label { Text = n, ForeColor = Color.FromArgb(190, 207, 222), Font = new Font("Microsoft YaHei UI", 10F, FontStyle.Bold), AutoSize = false, Width = 190, Height = 42, Location = new Point(18, y), Padding = new Padding(16, 11, 0, 0), Cursor = Cursors.Hand }; navItems[currentIndex] = item; item.MouseEnter += delegate(object s, EventArgs e) { if (activeNav != currentIndex) ((Label)s).BackColor = Color.FromArgb(16, 42, 65); }; item.MouseLeave += delegate(object s, EventArgs e) { if (activeNav != currentIndex) ((Label)s).BackColor = Color.Transparent; }; item.Click += delegate { SetActiveNav(currentIndex); FeatureSection target = currentIndex == 0 ? resourcesSection : currentIndex == 1 ? productionSection : currentIndex == 2 ? battlefieldSection : repairSection; if (target != null) content.AutoScrollPosition = new Point(0, target.Location.Y); }; side.Controls.Add(item); y += 52; navIndex++; }
        SetActiveNav(0);
        side.Controls.Add(new Label { Text = "会话保护\r\n所有修改可随时关闭\r\n退出游戏后自动失效", ForeColor = Color.FromArgb(110, 220, 154), Font = new Font("Microsoft YaHei UI", 8.5F), AutoSize = true, Location = new Point(20, 500) });
        string mapPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "assets", "tactical-map.png"); if (File.Exists(mapPath)) { PictureBox map = new PictureBox { Image = Image.FromFile(mapPath), SizeMode = PictureBoxSizeMode.Zoom, Size = new Size(192, 82), Location = new Point(18, 394), Anchor = AnchorStyles.Left | AnchorStyles.Bottom }; side.Controls.Add(map); }
        Controls.Add(side);
    }
    void SetActiveNav(int index)
    {
        activeNav = index;
        for (int i = 0; i < navItems.Length; i++) if (navItems[i] != null) { navItems[i].BackColor = i == index ? Color.FromArgb(128, 32, 43) : Color.Transparent; navItems[i].ForeColor = i == index ? Color.White : Color.FromArgb(190, 207, 222); }
    }
    void BuildMain()
    {
        TableLayoutPanel main = new TableLayoutPanel { Dock = DockStyle.Fill, BackColor = Background, Padding = new Padding(22, 16, 22, 0), RowCount = 3, ColumnCount = 1 }; main.RowStyles.Add(new RowStyle(SizeType.Absolute, 92)); main.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); main.RowStyles.Add(new RowStyle(SizeType.Absolute, 48)); Controls.Add(main); main.BringToFront();
        Panel hero = new Panel { Dock = DockStyle.Top, Height = 92, BackColor = Color.FromArgb(10, 25, 39) }; hero.Paint += delegate(object s, PaintEventArgs e) { using (LinearGradientBrush b = new LinearGradientBrush(hero.ClientRectangle, Color.FromArgb(16, 42, 62), Color.FromArgb(8, 20, 32), LinearGradientMode.Horizontal)) e.Graphics.FillRectangle(b, hero.ClientRectangle); using (Pen p = new Pen(Color.FromArgb(167, 38, 50), 2)) e.Graphics.DrawLine(p, 0, hero.Height - 1, hero.Width, hero.Height - 1); };
        hero.Controls.Add(new Label { Text = "战场控制台", ForeColor = Color.White, Font = new Font("Microsoft YaHei UI", 20F, FontStyle.Bold), AutoSize = true, Location = new Point(20, 10) }); hero.Controls.Add(new Label { Text = "TACTICAL CONTROL CENTER", ForeColor = Color.FromArgb(190, 48, 59), Font = new Font("Microsoft YaHei UI", 7.5F, FontStyle.Bold), AutoSize = true, Location = new Point(22, 45) }); hero.Controls.Add(new Label { Text = "按类别管理功能，开关会立即作用于当前游戏会话", ForeColor = Color.FromArgb(133, 158, 179), Font = new Font("Microsoft YaHei UI", 9F), AutoSize = true, Location = new Point(22, 62) });
        string artPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "assets", "ra2-steam-hero.png"); if (!File.Exists(artPath)) artPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "assets", "tactical-map.png"); if (File.Exists(artPath)) { PictureBox art = new PictureBox { Image = Image.FromFile(artPath), SizeMode = PictureBoxSizeMode.Zoom, Location = new Point(350, 10), Size = new Size(150, 70), Anchor = AnchorStyles.Top | AnchorStyles.Right }; hero.Controls.Add(art); }
        Button refresh = new Button { Text = "刷新状态", FlatStyle = FlatStyle.Flat, ForeColor = Color.FromArgb(194, 215, 232), BackColor = Color.FromArgb(14, 35, 54), Width = 78, Height = 34, Anchor = AnchorStyles.Top | AnchorStyles.Right, Location = new Point(505, 28) }; refresh.FlatAppearance.BorderColor = Color.FromArgb(61, 91, 116); refresh.Click += delegate { Execute("--status"); }; hero.Controls.Add(refresh);
        closeAllButton = new Button { Text = "全部关闭", FlatStyle = FlatStyle.Flat, ForeColor = Color.FromArgb(255, 235, 235), BackColor = Color.FromArgb(124, 32, 43), Width = 100, Height = 34, Anchor = AnchorStyles.Top | AnchorStyles.Right, Location = new Point(592, 28) }; closeAllButton.FlatAppearance.BorderColor = Color.FromArgb(186, 53, 62); closeAllButton.Click += delegate { CloseAllEnabledFeatures(); }; hero.Controls.Add(closeAllButton); main.Controls.Add(hero, 0, 0);
        content.Dock = DockStyle.Fill; content.AutoScroll = true; content.BackColor = Background; content.HorizontalScroll.Enabled = false; content.HorizontalScroll.Visible = false; content.Resize += delegate { int w = Math.Max(300, content.ClientSize.Width - 35); if (resourcesSection != null) resourcesSection.Width = w; if (productionSection != null) productionSection.Width = w; if (battlefieldSection != null) battlefieldSection.Width = w; if (repairSection != null) repairSection.Width = w; }; main.Controls.Add(content, 0, 1);
        resourcesSection = new FeatureSection("资源与电力", "掌握经济命脉，确保战局持续"); resourcesSection.AddFeature(rows[0] = Feature("金钱不减", "花费金钱不再减少，可正常获得收入", "--money-on", "--money-off", 0)); resourcesSection.AddFeature(rows[1] = Feature("电力无负载", "不受基地电力负载限制", "--power-on", "--power-off", 1)); AddSection(resourcesSection, 0);
        productionSection = new FeatureSection("生产与超武", "加速军事工业，随时释放战术力量"); productionSection.AddFeature(rows[2] = Feature("即时生产", "单位与建筑完成生产无需等待", "--production-toggle", "--production-toggle", 2)); productionSection.AddFeature(rows[3] = Feature("即时超武", "超级武器就绪后可立即使用", "--super-toggle", "--super-toggle", 3)); AddSection(productionSection, resourcesSection.Height + 14);
        battlefieldSection = new FeatureSection("战场辅助", "获取战场信息，减少规则限制"); battlefieldSection.AddFeature(rows[4] = Feature("去除迷雾", "显示完整地图视野", "--fog-on", "--fog-off", 4)); battlefieldSection.AddFeature(rows[5] = Feature("无视建筑距离", "保留地形与占用检查，自由选择建造位置", "--distance-on", "--distance-off", 5)); AddSection(battlefieldSection, resourcesSection.Height + productionSection.Height + 28);
        repairSection = new FeatureSection("维修与建造", "让基地持续保持最佳状态"); repairSection.AddFeature(rows[6] = Feature("自动修理", "己方建筑自动恢复生命值", "--repair-auto-on", "--repair-auto-off", 6)); repairSection.AddFeature(rows[7] = Feature("驻军建筑修理", "己方单位驻军的民用建筑自动修理", "--repair-garrison-on", "--repair-garrison-off", 7)); AddSection(repairSection, resourcesSection.Height + productionSection.Height + battlefieldSection.Height + 42); content.AutoScrollPosition = new Point(0, 0);
        Panel footer = new Panel { Dock = DockStyle.Fill, BackColor = Color.FromArgb(6, 15, 25), Padding = new Padding(12, 7, 12, 7) }; session.Text = "●  会话保护：修改仅对当前游戏有效，退出游戏后自动失效"; session.ForeColor = Color.FromArgb(110, 220, 154); session.Font = new Font("Microsoft YaHei UI", 8.5F); session.AutoSize = true; session.Location = new Point(12, 15); lastAction.Text = "等待操作"; lastAction.ForeColor = Color.FromArgb(116, 140, 163); lastAction.Font = new Font("Microsoft YaHei UI", 8.5F, FontStyle.Bold); lastAction.AutoSize = true; lastAction.Anchor = AnchorStyles.Top | AnchorStyles.Right; lastAction.Location = new Point(690, 15); Button logButton = new Button { Text = "查看日志", FlatStyle = FlatStyle.Flat, ForeColor = Color.FromArgb(194, 215, 232), BackColor = Color.FromArgb(14, 35, 54), Width = 78, Height = 30, Anchor = AnchorStyles.Top | AnchorStyles.Right, Location = new Point(590, 9), TabStop = false }; logButton.FlatAppearance.BorderColor = Color.FromArgb(53, 79, 101); logButton.Click += delegate { ShowLog(); }; footer.Controls.Add(session); footer.Controls.Add(logButton); footer.Controls.Add(lastAction); main.Controls.Add(footer, 0, 2);
    }
    void AddSection(FeatureSection section, int y) { section.Width = Math.Max(300, content.ClientSize.Width - 35); section.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right; section.Location = new Point(0, y); content.Controls.Add(section); }
    FeatureRow Feature(string name, string desc, string on, string off, int iconKind) { return new FeatureRow(name, desc, iconKind, delegate(bool enabled) { if (on == "--production-toggle") { ToggleProduction(); } else if (on == "--super-toggle") { ToggleSuper(); } else Execute(enabled ? on : off); }); }
    void ToggleProduction() { if (!productionOn) { if (Execute("--production-capture") && Execute("--production-on")) productionOn = true; } else if (Execute("--production-off")) productionOn = false; SyncRows(); }
    void ToggleSuper() { if (!superOn) { if (Execute("--super-capture") && Execute("--super-on")) superOn = true; } else if (Execute("--super-off")) superOn = false; SyncRows(); }
    void CloseAllEnabledFeatures()
    {
        bool attempted = false; bool failed = false;
        if (rows[0] != null && rows[0].EnabledState) { attempted = true; if (!Execute("--money-off")) failed = true; }
        if (rows[1] != null && rows[1].EnabledState) { attempted = true; if (!Execute("--power-off")) failed = true; }
        if (rows[2] != null && rows[2].EnabledState) { attempted = true; if (!Execute("--production-off")) failed = true; }
        if (rows[3] != null && rows[3].EnabledState) { attempted = true; if (!Execute("--super-off")) failed = true; }
        if (rows[4] != null && rows[4].EnabledState) { attempted = true; if (!Execute("--fog-off")) failed = true; }
        if (rows[5] != null && rows[5].EnabledState) { attempted = true; if (!Execute("--distance-off")) failed = true; }
        if ((rows[6] != null && rows[6].EnabledState) || (rows[7] != null && rows[7].EnabledState)) { attempted = true; if (!Execute("--repair-all-off")) failed = true; }
        lastAction.Text = failed ? "! 部分功能未能关闭" : (attempted ? "✓ 已关闭已启用功能" : "已全部关闭");
        lastAction.ForeColor = failed ? Color.FromArgb(242, 150, 112) : Color.FromArgb(110, 220, 154);
        Execute("--status", false);
    }
    bool Execute(string argument) { return Execute(argument, true); }
    bool Execute(string argument, bool record)
    {
        if (!File.Exists(hub)) { Log("修改器核心组件不存在"); return false; }
        try { ProcessStartInfo psi = new ProcessStartInfo(hub, argument) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true, StandardOutputEncoding = Encoding.Unicode, StandardErrorEncoding = Encoding.Unicode }; using (Process p = Process.Start(psi)) { string output = p.StandardOutput.ReadToEnd(); string error = p.StandardError.ReadToEnd(); p.WaitForExit(); if (record) Log("[" + argument + "]\r\n" + output + error); bool ok = p.ExitCode == 0; if (record && argument != "--status" && argument != "--self-check") { lastAction.Text = ok ? "✓ 操作完成" : "! 操作未完成"; lastAction.ForeColor = ok ? Color.FromArgb(110, 220, 154) : Color.FromArgb(242, 150, 112); } if (argument == "--status") UpdateState(output); if (record && argument != "--status" && argument != "--self-check") Execute("--status"); return ok; } }
        catch (Exception ex) { Log("操作失败：" + ex.Message); return false; }
    }
    void UpdateState(string output) { bool game = output.IndexOf("version=MATCHED", StringComparison.OrdinalIgnoreCase) >= 0; bool task = game && output.IndexOf("production=NOT_IN_TASK", StringComparison.OrdinalIgnoreCase) < 0 && output.IndexOf("superweapons=NOT_IN_TASK", StringComparison.OrdinalIgnoreCase) < 0; string pid = ReadField(output, "pid="); pidLabel.Text = game && pid.Length > 0 ? "PID " + pid : "PID —"; connection.Text = !game ? "●  游戏进程：未连接" : (task ? "●  游戏进程：运行中 · V1.006" : "●  游戏已启动 · 等待进入任务"); connection.ForeColor = task ? Color.FromArgb(92, 231, 148) : Color.FromArgb(242, 187, 77); session.Text = task ? "●  会话保护：修改仅对当前游戏有效，退出游戏后自动失效" : (!game ? "●  等待游戏启动 · 请先进入单机任务" : "●  请先进入单人任务或遭遇战，功能将自动解锁"); if (closeAllButton != null) closeAllButton.Enabled = task; for (int i = 0; i < rows.Length; i++) if (rows[i] != null) rows[i].SetAvailable(task); productionOn = output.IndexOf("VALUES=15,15,15,15,15", StringComparison.OrdinalIgnoreCase) >= 0; superOn = output.IndexOf("SUPER_STATUS", StringComparison.OrdinalIgnoreCase) >= 0 && output.IndexOf("applied=YES", StringComparison.OrdinalIgnoreCase) >= 0; SyncRows(); }
    static string ReadField(string output, string key) { int p = output.IndexOf(key, StringComparison.OrdinalIgnoreCase); if (p < 0) return ""; p += key.Length; int end = p; while (end < output.Length && !char.IsWhiteSpace(output[end])) end++; return output.Substring(p, end - p).Trim(); }
    void ShowLog()
    {
        Form dialog = new Form { Text = "操作日志", Width = 760, Height = 460, StartPosition = FormStartPosition.CenterParent, BackColor = Background, ForeColor = Color.White, MinimizeBox = false, MaximizeBox = false };
        TextBox box = new TextBox { Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, Dock = DockStyle.Fill, BackColor = Color.FromArgb(8, 20, 32), ForeColor = Color.FromArgb(202, 218, 231), BorderStyle = BorderStyle.FixedSingle, Font = new Font("Consolas", 9F), Text = activity.Text, Padding = new Padding(10) };
        Panel buttons = new Panel { Dock = DockStyle.Bottom, Height = 46, BackColor = Color.FromArgb(6, 15, 25) };
        Button copy = new Button { Text = "复制", Width = 72, Height = 30, Location = new Point(10, 8), FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(14, 35, 54), ForeColor = Color.FromArgb(194, 215, 232) }; copy.FlatAppearance.BorderColor = Color.FromArgb(53, 79, 101); copy.Click += delegate { if (box.Text.Length > 0) Clipboard.SetText(box.Text); };
        Button export = new Button { Text = "导出日志", Width = 86, Height = 30, Location = new Point(90, 8), FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(14, 35, 54), ForeColor = Color.FromArgb(194, 215, 232) }; export.FlatAppearance.BorderColor = Color.FromArgb(53, 79, 101); export.Click += delegate { using (SaveFileDialog save = new SaveFileDialog { Filter = "日志文件 (*.txt)|*.txt", FileName = "ra2-product-log.txt", Title = "导出修改器日志" }) if (save.ShowDialog(dialog) == DialogResult.OK) File.WriteAllText(save.FileName, box.Text, Encoding.UTF8); };
        Button close = new Button { Text = "关闭", Width = 72, Height = 30, Anchor = AnchorStyles.Top | AnchorStyles.Right, Location = new Point(662, 8), FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(124, 32, 43), ForeColor = Color.White }; close.FlatAppearance.BorderColor = Color.FromArgb(186, 53, 62); close.Click += delegate { dialog.Close(); }; buttons.Controls.Add(copy); buttons.Controls.Add(export); buttons.Controls.Add(close); dialog.Controls.Add(box); dialog.Controls.Add(buttons); dialog.ShowDialog(this);
    }
    void SyncRows() { if (rows[2] != null) rows[2].EnabledState = productionOn; if (rows[3] != null) rows[3].EnabledState = superOn; }
    void Log(string text) { activity.AppendText(DateTime.Now.ToString("HH:mm:ss") + "  " + text + "\r\n"); }
    [STAThread] static void Main() { try { Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false); Application.Run(new Ra2ProductUi()); } catch (Exception ex) { try { File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "ui-crash.txt"), ex.ToString(), Encoding.UTF8); } catch { } MessageBox.Show("修改器启动失败：" + ex.Message, "红色警戒2修改器", MessageBoxButtons.OK, MessageBoxIcon.Error); } }
}
