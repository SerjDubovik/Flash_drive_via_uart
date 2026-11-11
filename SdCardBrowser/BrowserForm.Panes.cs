namespace SdCardBrowser;

internal sealed partial class BrowserForm
{
    private sealed record PcEntry(string Path, bool IsDirectory, long Size);
    private readonly FolderHistory _pcHistory = new();
    private bool _pcPanelActive;

    private void InitializePcPane()
    {
        ConfigureFileDrawing(pcListView);
        cardPanel.Enter += (_, _) => _pcPanelActive = false;
        pcPanel.Enter += (_, _) => _pcPanelActive = true;
        drivesComboBox.DropDown += (_, _) => RefreshDrives();
        drivesComboBox.SelectionChangeCommitted += async (_, _) =>
        {
            if (drivesComboBox.SelectedItem is string root) await NavigatePcAsync(root);
            SelectCurrentDrive();
        };
        pcListView.MouseClick += async (_, e) =>
        {
            if (e.Button == MouseButtons.Left && pcListView.GetItemAt(e.X, e.Y) is { Tag: null })
                await GoUpAsync(pc: true);
        };
        pcListView.ItemActivate += async (_, _) =>
        {
            if (pcListView.SelectedItems.Count != 1) return;
            switch (pcListView.SelectedItems[0].Tag)
            {
                case null: await GoUpAsync(pc: true); break;
                case PcEntry { IsDirectory: true } entry: await NavigatePcAsync(entry.Path); break;
            }
        };
        Shown += async (_, _) =>
        {
            Application.AddMessageFilter(this);
            RefreshDrives();
            string initial = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
            if (!Directory.Exists(initial)) initial = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            if (!Directory.Exists(initial)) initial = Path.GetPathRoot(Environment.SystemDirectory)!;
            await NavigatePcAsync(initial);
        };
        Disposed += (_, _) => Application.RemoveMessageFilter(this);
    }

    private void RefreshDrives()
    {
        drivesComboBox.Items.Clear();
        drivesComboBox.Items.AddRange(Directory.GetLogicalDrives());
        SelectCurrentDrive();
    }

    private void SelectCurrentDrive()
    {
        string? root = Path.GetPathRoot(_pcHistory.Current);
        drivesComboBox.SelectedIndex = root is null ? -1 : drivesComboBox.FindStringExact(root);
    }

    private Task NavigatePcAsync(string path, int historyStep = 0) =>
        RunAsync(() => LoadPcDirectoryAsync(path, historyStep));

    private async Task LoadPcDirectoryAsync(string path, int historyStep = 0)
    {
        var folder = new DirectoryInfo(path);
        List<ListViewItem> rows = await Task.Run(() =>
        {
            var items = new List<ListViewItem>();
            if (folder.Parent is not null) items.Add(new ListViewItem(["..", "Родительская папка", ""]));
            foreach (FileSystemInfo info in folder.GetFileSystemInfos()
                .OrderByDescending(info => info is DirectoryInfo)
                .ThenBy(info => info.Name, StringComparer.CurrentCultureIgnoreCase))
            {
                var entry = new PcEntry(info.FullName, info is DirectoryInfo, info is FileInfo file ? file.Length : 0);
                items.Add(new ListViewItem([info.Name, entry.IsDirectory ? "Папка" : "Файл",
                    entry.IsDirectory ? "" : entry.Size.ToString("N0")]) { Tag = entry });
            }
            return items;
        });

        pcListView.BeginUpdate();
        try
        {
            pcListView.Items.Clear();
            pcListView.Items.AddRange(rows.ToArray());
        }
        finally
        {
            pcListView.EndUpdate();
        }
        SetPcLocation(folder);
        _pcHistory.Visit(folder.FullName, historyStep);
        SelectCurrentDrive();
        statusLabel.Text = $"ПК • Элементов: {rows.Count - (folder.Parent is null ? 0 : 1)}";
    }

    private void SetPcLocation(DirectoryInfo folder)
    {
        pcLocationBar.SuspendLayout();
        try
        {
            foreach (Control link in pcLocationBar.Controls.Cast<Control>().ToArray()) link.Dispose();
            var segments = new Stack<DirectoryInfo>();
            for (DirectoryInfo? part = folder; part is not null; part = part.Parent) segments.Push(part);
            while (segments.TryPop(out DirectoryInfo? segment))
            {
                string target = segment.FullName;
                string label = segment.Parent is null ? target : segment.Name + Path.DirectorySeparatorChar;
                AddPathLink(pcLocationBar, label, () => NavigatePcAsync(target));
            }
        }
        finally
        {
            pcLocationBar.ResumeLayout();
        }
        pcLocationBar.ScrollControlIntoView(pcLocationBar.Controls[^1]);
    }

    private Task GoUpAsync(bool pc)
    {
        if (pc)
        {
            string? parent = _pcHistory.Current is { } path ? Directory.GetParent(path)?.FullName : null;
            return parent is not null ? NavigatePcAsync(parent) : Task.CompletedTask;
        }

        return _path != "/" ? NavigateAsync(ParentPath(_path)) : Task.CompletedTask;
    }

    private Task NavigateHistoryAsync(bool pc, int step)
    {
        FolderHistory history = pc ? _pcHistory : _cardHistory;
        if (history.Adjacent(step) is not { } path) return Task.CompletedTask;
        return pc ? NavigatePcAsync(path, step) : NavigateAsync(path, step);
    }

    protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
    {
        switch (keyData)
        {
            case Keys.Back:
                _ = GoUpAsync(_pcPanelActive);
                return true;
            case Keys.BrowserBack:
            case Keys.BrowserForward:
                _ = NavigateHistoryAsync(_pcPanelActive, keyData == Keys.BrowserBack ? -1 : 1);
                return true;
            default:
                return base.ProcessCmdKey(ref msg, keyData);
        }
    }

    public bool PreFilterMessage(ref Message message)
    {
        const int wmXButtonDown = 0x020B;
        const int wmXButtonUp = 0x020C;
        const int wmXButtonDoubleClick = 0x020D;
        if (message.Msg is not (wmXButtonDown or wmXButtonUp or wmXButtonDoubleClick) ||
            IsPcPane(message.HWnd) is not bool pc) return false;
        int button = (int)((message.WParam.ToInt64() >> 16) & 0xFFFF);
        if (button is not (1 or 2)) return false;

        // Consume both halves so the native control never captures this button.
        if (message.Msg == wmXButtonUp) _ = NavigateHistoryAsync(pc, button == 1 ? -1 : 1);
        return true;
    }

    protected override void WndProc(ref Message message)
    {
        const int wmAppCommand = 0x0319;
        if (message.Msg == wmAppCommand)
        {
            int command = (int)((message.LParam.ToInt64() >> 16) & 0x0FFF);
            if (command is 1 or 2)
            {
                _ = NavigateHistoryAsync(IsPcPane(message.WParam) ?? _pcPanelActive, command == 1 ? -1 : 1);
                message.Result = 1;
                return;
            }
        }
        base.WndProc(ref message);
    }

    private bool? IsPcPane(nint handle)
    {
        for (Control? control = Control.FromChildHandle(handle); control is not null; control = control.Parent)
        {
            if (control == pcPanel) return true;
            if (control == cardPanel) return false;
        }
        return null;
    }

    private sealed class FolderHistory
    {
        private readonly List<string> _paths = [];
        private int _index = -1;
        public string? Current => _index >= 0 ? _paths[_index] : null;

        public string? Adjacent(int step)
        {
            int next = _index + step;
            return next >= 0 && next < _paths.Count ? _paths[next] : null;
        }

        public void Visit(string path, int step)
        {
            if (step != 0)
            {
                _index += step;
            }
            else if (!StringComparer.OrdinalIgnoreCase.Equals(Current, path))
            {
                _paths.RemoveRange(_index + 1, _paths.Count - _index - 1);
                _paths.Add(path);
                ++_index;
            }
            _paths[_index] = path;
        }

        public void Clear()
        {
            _paths.Clear();
            _index = -1;
        }
    }
}
