// LittStudio.cs - the C# front door of the Litt engine.
// Modern .NET WinForms app: browse games, launch the Vulkan player or
// the C++ viewer, generate new worlds, run proofs - all without a shell.
//
// Build:  litt studio  (or)  tools\build-studio.bat
// Run :   studio\LittStudio.exe
//
// Design notes:
// - talks to the SAME native binaries as everything else (littcli,
//   littview, ENGINE.bat) - one source of truth, zero duplicated logic
// - dark theme, game grid with live mode/entity data from littcli
// - "New World" drives template/tools/worldgen/make_game.py with a
//   seed field so results are reproducible
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Windows.Forms;

namespace LittStudio;

static class LittStudio
{
    public static string Repo = FindRepo();
    public static string BinDir = Path.Combine(Repo, "native", "bin");
    public static string Projects = Path.Combine(Repo, "Project");

    static string FindRepo()
    {
        // walk up from the exe location until we find Project/ + tools/
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        for (int i = 0; i < 8 && dir != null; i++, dir = dir.Parent)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, "Project")) &&
                Directory.Exists(Path.Combine(dir.FullName, "tools")))
                return dir.FullName;
        }
        return AppContext.BaseDirectory;
    }

    [STAThread]
    static int Main(string[] args)
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new MainForm());
        return 0;
    }
}

public class GameInfo
{
    public string Name = "";
    public bool Shippable;
    public string Mode = "?";
    public int Entities;
    public bool Ok;
}

public class MainForm : Form
{
    readonly List<GameInfo> _games = new();
    DataGridView _grid;
    Button _playBtn, _viewBtn, _proofBtn, _newBtn, _benchBtn, _refreshBtn;
    TextBox _seedBox, _aboutBox;
    PictureBox _preview;
    StatusStrip _status;
    ToolStripStatusLabel _statusText;

    public MainForm()
    {
        Text = "Litt Studio";
        Font = new Font("Segoe UI Variable", 9.5f);
        Size = new Size(1180, 720);
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Color.FromArgb(24, 22, 26);
        DoubleBuffered = true;

        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1,
            BackColor = Color.Transparent,
        };
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 62f));
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 38f));

        // ---------------- left: title + grid + actions
        var left = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.TopDown,
            Padding = new Padding(16, 12, 8, 8),
            WrapContents = false,
        };
        left.Controls.Add(new Label
        {
            Text = "LITT STUDIO",
            ForeColor = Color.FromArgb(235, 120, 60),
            Font = new Font("Segoe UI Variable Display", 20f, FontStyle.Bold),
            AutoSize = true,
            Margin = new Padding(0, 0, 0, 2),
        });
        left.Controls.Add(new Label
        {
            Text = "C/C++ core  ·  C# cockpit  ·  Rust player (optional)",
            ForeColor = Color.FromArgb(150, 145, 155),
            AutoSize = true,
            Margin = new Padding(2, 0, 0, 10),
        });

        _grid = MakeGrid();
        left.Controls.Add(_grid);

        var actions = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.LeftToRight,
            AutoSize = true,
            Margin = new Padding(0, 10, 0, 0),
        };
        _playBtn = Btn("▶ Play (Vulkan)", 150);
        _viewBtn = Btn("👁 C++ Viewer", 130);
        _benchBtn = Btn("⏱ Benchmark", 120);
        _proofBtn = Btn("✔ Native Proof", 130);
        _refreshBtn = Btn("⟳ Refresh", 100);
        _playBtn.Click += (s, e) => Launch("play");
        _viewBtn.Click += (s, e) => Launch("view");
        _benchBtn.Click += (s, e) => Bench();
        _proofBtn.Click += (s, e) => Proof();
        _refreshBtn.Click += (s, e) => { Reload(); };
        actions.Controls.AddRange(new Control[] {
            _playBtn, _viewBtn, _benchBtn, _proofBtn, _refreshBtn });
        left.Controls.Add(actions);

        // ---------------- right: preview + worldgen
        var right = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.TopDown,
            Padding = new Padding(8, 12, 16, 8),
            WrapContents = false,
        };

        _preview = new PictureBox
        {
            Size = new Size(400, 225),
            SizeMode = PictureBoxSizeMode.Zoom,
            BackColor = Color.FromArgb(32, 28, 34),
            Margin = new Padding(0, 0, 0, 8),
        };
        right.Controls.Add(_preview);

        right.Controls.Add(SideLabel("CREATE A NEW WORLD"));
        _aboutBox = new TextBox
        {
            Width = 400,
            Margin = new Padding(0, 2, 0, 2),
        };
        _aboutBox.ForeColor = Color.FromArgb(60, 55, 65); // cue-color until focus
        _aboutBox.Text = "one line about the game...";
        _aboutBox.GotFocus += (s, e) =>
        {
            if (_aboutBox.ForeColor.R < 100)
            {
                _aboutBox.Text = "";
                _aboutBox.ForeColor = Color.Gainsboro;
            }
        };
        _aboutBox.LostFocus += (s, e) =>
        {
            if (_aboutBox.Text.Length == 0)
            {
                _aboutBox.Text = "one line about the game...";
                _aboutBox.ForeColor = Color.FromArgb(60, 55, 65);
            }
        };
        right.Controls.Add(_aboutBox);
        var row = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.LeftToRight,
            AutoSize = true,
            Margin = new Padding(0, 4, 0, 0),
        };
        _seedBox = new TextBox { Width = 110, Text = "" };
        _newBtn = Btn("🍳 Cook it", 110);
        _newBtn.Click += (s, e) => NewWorld();
        row.Controls.Add(new Label { Text = "seed ", AutoSize = true, ForeColor = Color.Gainsboro });
        row.Controls.Add(_seedBox);
        row.Controls.Add(_newBtn);
        right.Controls.Add(row);

        right.Controls.Add(SideLabel("TIP"));
        right.Controls.Add(new Label
        {
            Text = "double-click a game = play\nlitt view <game> --shot f.bmp = still\nENGINE.bat plays · VIEW.bat inspects",
            ForeColor = Color.FromArgb(140, 135, 145),
            AutoSize = true,
            Margin = new Padding(2, 4, 0, 0),
        });

        root.Controls.Add(left, 0, 0);
        root.Controls.Add(right, 1, 0);
        Controls.Add(root);

        _status = new StatusStrip();
        _statusText = new ToolStripStatusLabel("ready");
        _status.Items.Add(_statusText);
        Controls.Add(_status);

        Load += (s, e) => { Reload(); };
        _grid.CellDoubleClick += (s, e) => Launch("play");
        _grid.SelectionChanged += (s, e) => PreviewSelected();
        AcceptButton = null;
    }

    DataGridView MakeGrid()
    {
        var g = new DataGridView
        {
            Dock = DockStyle.Fill,
            BackgroundColor = Color.FromArgb(30, 27, 33),
            BorderStyle = BorderStyle.None,
            EnableHeadersVisualStyles = false,
            RowHeadersVisible = false,
            AllowUserToAddRows = false,
            AllowUserToDeleteRows = false,
            ReadOnly = true,
            SelectionMode = DataGridViewSelectionMode.FullRowSelect,
            MultiSelect = false,
            AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill,
            RowTemplate = { Height = 30 },
        };
        g.ColumnHeadersDefaultCellStyle.BackColor = Color.FromArgb(44, 40, 48);
        g.ColumnHeadersDefaultCellStyle.ForeColor = Color.FromArgb(230, 225, 235);
        g.ColumnHeadersDefaultCellStyle.Padding = new Padding(6, 6, 6, 6);
        g.DefaultCellStyle.BackColor = Color.FromArgb(30, 27, 33);
        g.DefaultCellStyle.ForeColor = Color.FromArgb(220, 215, 228);
        g.DefaultCellStyle.SelectionBackColor = Color.FromArgb(70, 45, 35);
        g.DefaultCellStyle.SelectionForeColor = Color.White;
        g.GridColor = Color.FromArgb(50, 46, 54);
        return g;
    }

    Button Btn(string text, int width)
    {
        return new Button
        {
            Text = text,
            Width = width,
            Height = 36,
            FlatStyle = FlatStyle.Flat,
            BackColor = Color.FromArgb(58, 52, 64),
            ForeColor = Color.FromArgb(240, 236, 245),
            Margin = new Padding(0, 0, 8, 0),
        };
    }

    Label SideLabel(string t)
    {
        return new Label
        {
            Text = t,
            ForeColor = Color.FromArgb(200, 130, 90),
            Font = new Font("Segoe UI", 9f, FontStyle.Bold),
            AutoSize = true,
            Margin = new Padding(0, 10, 0, 2),
        };
    }

    void Reload()
    {
        _games.Clear();
        var cli = Path.Combine(LittStudio.BinDir,
            Environment.OSVersion.Platform == PlatformID.Win32NT ? "littcli.exe" : "littcli");
        foreach (var dir in Directory.GetDirectories(LittStudio.Projects))
        {
            if (!File.Exists(Path.Combine(dir, "world_state.json"))) continue;
            var info = new GameInfo { Name = Path.GetFileName(dir) };
            info.Shippable = Directory.Exists(Path.Combine(dir, "story"));
            if (File.Exists(cli))
            {
                try
                {
                    var psi = new ProcessStartInfo(cli,
                        $"validate \"{dir}\" --frames 10")
                    {
                        RedirectStandardOutput = true,
                        UseShellExecute = false,
                        CreateNoWindow = true,
                    };
                    using var p = Process.Start(psi);
                    var json = p.StandardOutput.ReadToEnd();
                    p.WaitForExit(30000);
                    var m = Regex.Match(json, "\"mode\":\"([^\"]+)\"");
                    var n = Regex.Match(json, "\"interactives\":(\\d+)");
                    var ok = Regex.Match(json, "\"ok\":(true|false)");
                    if (m.Success) info.Mode = m.Groups[1].Value;
                    if (n.Success) info.Entities = int.Parse(n.Groups[1].Value);
                    info.Ok = ok.Success && ok.Groups[1].Value == "true";
                }
                catch { /* leave defaults */ }
            }
            _games.Add(info);
        }

        _grid.DataSource = null;
        _grid.Columns.Clear();
        _grid.DataSource = _games.Select(g => new {
            Game = g.Name,
            Shippable = g.Shippable ? "yes" : "",
            Mode = g.Mode,
            Entities = g.Entities,
            Health = g.Ok ? "ok" : "FAIL",
        }).ToList();

        if (_grid.Columns["Game"] != null) _grid.Columns["Game"].FillWeight = 42;
        if (_grid.Columns["Mode"] != null) _grid.Columns["Mode"].FillWeight = 20;
        Status($"{_games.Count} games ({_games.Count(x => x.Shippable)} shippable)");
    }

    GameInfo Selected()
    {
        if (_grid.CurrentRow?.Index is int i && i >= 0 && i < _games.Count)
            return _games[i];
        return null;
    }

    void PreviewSelected()
    {
        var gi = Selected();
        if (gi == null) return;
        var view = Path.Combine(LittStudio.BinDir, "littview.exe");
        if (!File.Exists(view)) return;
        var bmp = Path.Combine(Path.GetTempPath(), "litt_studio_preview.bmp");
        try
        {
            var psi = new ProcessStartInfo(view,
                $"render \"{Path.Combine(LittStudio.Projects, gi.Name)}\" --out \"{bmp}\"")
            {
                RedirectStandardOutput = true,
                UseShellExecute = false,
                CreateNoWindow = true,
            };
            using var p = Process.Start(psi);
            p.WaitForExit(60000);
            if (File.Exists(bmp))
            {
                using var fs = new FileStream(bmp, FileMode.Open, FileAccess.Read);
                _preview.Image = Image.FromStream(fs);
            }
        }
        catch { /* preview is best-effort */ }
    }

    void Launch(string verb)
    {
        var gi = Selected();
        if (gi == null) { Status("select a game first"); return; }
        var py = Path.Combine(LittStudio.Repo, "tools", "litt.py");
        var psi = new ProcessStartInfo("python",
            $"\"{py}\" {verb} \"{gi.Name}\"")
        {
            WorkingDirectory = LittStudio.Repo,
            UseShellExecute = false,
        };
        Process.Start(psi);
        Status($"{verb} {gi.Name} launched in its own window");
    }

    void NewWorld()
    {
        string about = _aboutBox.Text.Trim();
        if (about.Length == 0 || about.StartsWith("one line"))
        {
            Status("give me one line about the game");
            return;
        }
        string seedArg = uint.TryParse(_seedBox.Text, out var sd)
            ? $"--seed {sd}" : "";
        var script = Path.Combine(LittStudio.Repo,
            "template/tools/worldgen/make_game.py");
        Status("cooking world...");
        var psi = new ProcessStartInfo("python",
            $"\"{script}\" --about \"{about}\" --scale small {seedArg}")
        {
            WorkingDirectory = LittStudio.Repo,
            UseShellExecute = false,
        };
        Process.Start(psi);
        Status("world cooking in background - hit Refresh in a minute");
    }

    void Bench()
    {
        var gi = Selected() ?? new GameInfo { Name = "drowned-vow-42" };
        var view = Path.Combine(LittStudio.BinDir, "littview.exe");
        if (!File.Exists(view)) { Status("native core missing - litt build"); return; }
        var psi = new ProcessStartInfo(view,
            $"bench \"{Path.Combine(LittStudio.Projects, gi.Name)}\" --frames 200")
        {
            RedirectStandardOutput = true,
            UseShellExecute = false,
            CreateNoWindow = true,
        };
        using var p = Process.Start(psi);
        var js = p.StandardOutput.ReadToEnd().Trim();
        p.WaitForExit();
        var m = Regex.Match(js, "\"ms_per_frame\":([0-9.]+)");
        Status(m.Success
            ? $"{gi.Name}: {m.Groups[1].Value} ms/frame"
            : "bench failed");
    }

    void Proof()
    {
        var script = Path.Combine(LittStudio.Repo,
            "template/tools/assets/native_proof.py");
        var psi = new ProcessStartInfo("python", $"\"{script}\"")
        {
            WorkingDirectory = LittStudio.Repo,
            UseShellExecute = false,
        };
        Process.Start(psi);
        Status("native proof running in background console");
    }

    void Status(string s) { _statusText.Text = s; }
}
