using System.IO.Ports;

namespace SdCardBrowser;

internal sealed partial class BrowserForm : Form, IMessageFilter
{
    private CardClient? _client;
    private readonly FolderHistory _cardHistory = new();
    private string _path => _cardHistory.Current ?? "/";
    private bool _busy;
    private bool _closeRequested;
    private CancellationTokenSource? _transfer;

    public BrowserForm()
    {
        InitializeComponent();
        baudComboBox.SelectedItem = baudComboBox.Items.Contains(_settings.BaudRate) ? _settings.BaudRate : 115200;
        exitMenuItem.Click += (_, _) => Close();
        mediumComboBox.SelectedIndexChanged += async (_, _) =>
        {
            cardHeading.Text = mediumComboBox.SelectedIndex == 1 ? "W25Q16 STM32" : "SD-карта STM32";
            if (_client is not null) await RunAsync(MountSelectedAsync);
        };
        mediumComboBox.SelectedIndex = _settings.UseNor ? 1 : 0;

        scanButton.Click += (_, _) => ScanPorts();
        portsComboBox.SelectedIndexChanged += (_, _) => UpdateControls();
        portsComboBox.SelectionChangeCommitted += (_, _) => _settings.Port = portsComboBox.SelectedItem as string;
        connectButton.Click += async (_, _) => await ToggleConnectionAsync();
        refreshButton.Click += async (_, _) => await NavigateAsync(_path);
        createButton.Click += async (_, _) => await CreateAsync(directory: false);
        mkdirButton.Click += async (_, _) => await CreateAsync(directory: true);
        renameButton.Click += async (_, _) => await MoveAsync(rename: true);
        moveButton.Click += async (_, _) => await MoveAsync(rename: false);
        deleteButton.Click += async (_, _) => await DeleteAsync();
        uploadButton.Click += async (_, _) => await UploadAsync();
        downloadButton.Click += async (_, _) => await DownloadAsync();
        openButton.Click += async (_, _) => await OpenAsync();
        cancelButton.Click += (_, _) => _transfer?.Cancel();
        ConfigureFileDrawing(filesListView);
        InitializePcPane();
        InitializeDragAndDrop();
        InitializeTouchTab();
        RestoreWindowSettings();
        if (_settings.TouchTab) _workspaceTabs.SelectedTab = _touchTab;
        Shown += (_, _) =>
        {
            RestoreSplitter();
            panes.SplitterMoved += (_, _) =>
            {
                int width = panes.ClientSize.Width - panes.SplitterWidth;
                if (!TouchTabSelected && width > 0) _settings.SplitterRatio = panes.SplitterDistance / (double)width;
            };
        };
        filesListView.SelectedIndexChanged += (_, _) => UpdateControls();
        filesListView.MouseClick += async (_, e) =>
        {
            if (e.Button == MouseButtons.Left && filesListView.GetItemAt(e.X, e.Y) is { Tag: null })
                await NavigateAsync(ParentPath(_path));
        };
        filesListView.ItemActivate += async (_, _) => await OpenAsync();
        FormClosing += (_, e) =>
        {
            if (_busy)
            {
                _closeRequested = true;
                _transfer?.Cancel();
                e.Cancel = true;
                return;
            }
            SaveSettings();
            Disconnect();
        };

        SetLocation("/");
        ScanPorts();
    }

    private ListViewItem? SelectedItem => filesListView.SelectedItems.Count == 1 ? filesListView.SelectedItems[0] : null;

    private CardEntry? Selected => SelectedItem?.Tag as CardEntry;

    private static void ConfigureFileDrawing(ListView list)
    {
        list.OwnerDraw = true;
        list.DrawColumnHeader += (_, e) => e.DrawDefault = true;
        list.DrawSubItem += DrawFileCell;
    }

    private static void DrawFileCell(object? sender, DrawListViewSubItemEventArgs e)
    {
        if (e.ColumnIndex != 0 || e.Item is not { Tag: null or CardEntry { IsDirectory: true } or PcEntry { IsDirectory: true } } item ||
            item.Selected || sender is not ListView { Enabled: true } list || SystemInformation.HighContrast)
        {
            e.DrawDefault = true;
            return;
        }

        // Native ListView drawing substitutes its default text color on hover.
        e.DrawBackground();
        Rectangle bounds = Rectangle.Intersect(e.Bounds, item.GetBounds(ItemBoundsPortion.Label));
        TextRenderer.DrawText(e.Graphics, item.Text, list.Font, bounds, Color.FromArgb(23, 90, 156),
            TextFormatFlags.SingleLine | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
    }

    private void ScanPorts()
    {
        string? previous = _settings.Port ?? portsComboBox.SelectedItem as string;
        string[] names = SerialPort.GetPortNames().Order(StringComparer.OrdinalIgnoreCase).ToArray();
        portsComboBox.Items.Clear();
        portsComboBox.Items.AddRange(names);
        if (previous is not null && portsComboBox.Items.Contains(previous)) portsComboBox.SelectedItem = previous;
        else if (previous is null && names.Length > 0) portsComboBox.SelectedIndex = 0;
        statusLabel.Text = names.Length == 0 ? "COM-порты не найдены." : "Выберите COM-порт платы.";
        UpdateControls();
    }

    private void UpdateControls()
    {
        bool connected = _client is not null;
        bool ready = connected && !_busy;
        baudComboBox.Enabled = portsComboBox.Enabled = scanButton.Enabled = !connected && !_busy;
        mediumComboBox.Enabled = !_busy && !TouchTabSelected;
        connectButton.Enabled = !_busy && (connected || portsComboBox.SelectedItem is not null);
        connectButton.Text = connected ? "Отключиться" : "Подключиться";
        refreshButton.Enabled = createButton.Enabled = mkdirButton.Enabled = filesListView.Enabled = ready;
        locationBar.Enabled = ready;
        pcPanel.Enabled = !_busy;
        renameButton.Enabled = moveButton.Enabled = deleteButton.Enabled = ready && Selected is not null;
        openButton.Enabled = ready && SelectedItem is not null;
        uploadButton.Enabled = ready;
        downloadButton.Enabled = ready && Selected is { IsDirectory: false };
        cancelButton.Enabled = _transfer is not null;
        UseWaitCursor = _busy && _transfer is null;
    }

    private async Task<bool> RunAsync(Func<Task> action, Form? errorOwner = null, bool quiet = false)
    {
        if (_busy) return false;
        _busy = true;
        if (!quiet)
        {
            statusLabel.Text = "Выполняется операция…";
            UpdateControls();
        }
        try
        {
            await action();
            return true;
        }
        catch (OperationCanceledException)
        {
            statusLabel.Text = "Передача отменена.";
        }
        catch (FatFsException ex)
        {
            if (ex.RequiresReconnect) Disconnect();
            ReportError(ex.Message, errorOwner);
        }
        catch (CardLinkException ex)
        {
            Disconnect();
            ReportError(ex.Message, errorOwner);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or
            InvalidOperationException or ArgumentException)
        {
            ReportError(ex.Message, errorOwner);
        }
        finally
        {
            _busy = false;
            UpdateControls();
            if (_closeRequested) BeginInvoke(Close);
        }
        return false;
    }

    private void ReportError(string message, Form? owner = null)
    {
        statusLabel.Text = message;
        if (!_closeRequested)
            MessageBox.Show(owner ?? this, message, "Ошибка", MessageBoxButtons.OK, MessageBoxIcon.Error);
    }

    private async Task ToggleConnectionAsync()
    {
        if (_client is not null)
        {
            Disconnect();
            statusLabel.Text = "Соединение закрыто.";
            UpdateControls();
            return;
        }

        if (portsComboBox.SelectedItem is not string port || baudComboBox.SelectedItem is not int baudRate) return;
        await RunAsync(async () =>
        {
            _client = await Task.Run(() => CardClient.Open(port, baudRate));
            _settings.Port = port;
            if (TouchTabSelected)
            {
                _mountOnFilesTab = true;
                statusLabel.Text = $"Подключено к {port}.";
            }
            else
                await MountSelectedAsync();
        });
    }

    private async Task MountSelectedAsync()
    {
        if (_client is not { } client) return;
        bool nor = mediumComboBox.SelectedIndex == 1;
        filesListView.Items.Clear();
        freeSpaceLabel.Text = "Свободно —";
        _cardHistory.Clear();
        SetLocation("/");

        try
        {
            try
            {
                await Task.Run(() => client.Mount(nor));
            }
            catch (FatFsException ex) when (nor && ex.Result == 13)
            {
                var choice = MessageBox.Show(this,
                    "На W25Q16 не найдена файловая система FAT. Создать её? Данные, записанные ранее, могут стать недоступны.",
                    "Инициализация W25Q16", MessageBoxButtons.YesNo, MessageBoxIcon.Warning, MessageBoxDefaultButton.Button2);
                if (choice != DialogResult.Yes)
                {
                    Disconnect();
                    statusLabel.Text = "Инициализация W25Q16 отменена.";
                    return;
                }

                statusLabel.Text = "Создаётся файловая система на W25Q16…";
                await Task.Run(() => client.Execute("norinit"));
                await Task.Run(() => client.Mount(true));
            }

            string folder = (nor ? _settings.NorFolder : _settings.SdFolder) ?? "/";
            if (folder.StartsWith('/') && folder != "/")
            {
                try
                {
                    await LoadDirectoryAsync(client, folder);
                    return;
                }
                catch (FatFsException ex) when (ex.Result is 4 or 5 or 6 or 19) { }
            }
            await LoadDirectoryAsync(client, "/");
        }
        catch
        {
            Disconnect();
            throw;
        }
    }

    private void Disconnect()
    {
        CardClient? client = _client;
        _client = null;
        _mountOnFilesTab = false;
        ResetTouchView();
        try
        {
            client?.Dispose();
        }
        catch (IOException)
        {
            // The USB adapter may already be disconnected.
        }
        filesListView.Items.Clear();
        freeSpaceLabel.Text = "Свободно —";
        _cardHistory.Clear();
        SetLocation("/");
    }

    private void SetLocation(string path)
    {
        locationBar.SuspendLayout();
        try
        {
            foreach (Control link in locationBar.Controls.Cast<Control>().ToArray()) link.Dispose();
            AddPathLink(locationBar, "/", () => NavigateAsync("/"));
            string directory = "/";
            foreach (string segment in path.Split('/', StringSplitOptions.RemoveEmptyEntries))
            {
                string label = directory == "/" ? segment : "/" + segment;
                directory = Join(directory, segment);
                string target = directory;
                AddPathLink(locationBar, label, () => NavigateAsync(target));
            }
        }
        finally
        {
            locationBar.ResumeLayout();
        }
        locationBar.ScrollControlIntoView(locationBar.Controls[^1]);
    }

    private static void AddPathLink(FlowLayoutPanel bar, string label, Func<Task> navigate)
    {
        var link = new LinkLabel
        {
            Text = label,
            AutoSize = true,
            UseMnemonic = false,
            Margin = new Padding(1, 5, 1, 0)
        };
        link.LinkClicked += async (_, _) => await navigate();
        bar.Controls.Add(link);
    }

    private Task NavigateAsync(string path, int historyStep = 0)
    {
        CardClient? client = _client;
        return client is null ? Task.CompletedTask : RunAsync(() => LoadDirectoryAsync(client, path, historyStep));
    }

    private async Task LoadDirectoryAsync(CardClient client, string path, int historyStep = 0)
    {
        List<CardEntry> entries = await Task.Run(() => client.List(path));
        filesListView.BeginUpdate();
        try
        {
            filesListView.Items.Clear();
            if (path != "/") filesListView.Items.Add(new ListViewItem(["..", "Родительская папка", ""]));
            foreach (CardEntry entry in entries.OrderByDescending(e => e.IsDirectory)
                .ThenBy(e => e.Name, StringComparer.CurrentCultureIgnoreCase))
            {
                var item = new ListViewItem(entry.Name) { Tag = entry };
                item.SubItems.Add(entry.IsDirectory ? "Папка" : "Файл");
                item.SubItems.Add(entry.IsDirectory ? "" : entry.Size.ToString("N0"));
                filesListView.Items.Add(item);
            }
        }
        finally
        {
            filesListView.EndUpdate();
        }
        SetLocation(path);
        _cardHistory.Visit(path, historyStep);
        if (mediumComboBox.SelectedIndex == 1) _settings.NorFolder = path;
        else _settings.SdFolder = path;
        statusLabel.Text = $"{portsComboBox.SelectedItem} • Элементов: {entries.Count}";
        ulong? freeBytes = await Task.Run(client.GetFreeBytes);
        freeSpaceLabel.Text = freeBytes is ulong bytes ? $"Свободно {bytes:N0} байт" : "Свободно —";
    }

    private async Task CreateAsync(bool directory)
    {
        string? name = Prompt(directory ? "Новая папка" : "Новый файл", "Имя", "");
        if (name is not null && CheckName(name))
            await ChangeAsync(directory ? "mkdir" : "create", Join(_path, name));
    }

    private async Task MoveAsync(bool rename)
    {
        if (Selected is not { } entry) return;
        string? value = rename
            ? Prompt("Переименовать", "Новое имя", entry.Name)
            : Prompt("Переместить", "Путь существующей папки на накопителе, например /Архив", _path);
        if (value is null) return;
        if (rename && !CheckName(value)) return;
        if (!rename && !value.StartsWith('/'))
        {
            ReportError("Введите абсолютный путь к папке, начиная с /.");
            return;
        }

        string source = Join(_path, entry.Name);
        string destination = rename ? Join(_path, value) : Join(value, entry.Name);
        if (source != destination) await ChangeAsync("move", source, destination);
    }

    private async Task DeleteAsync()
    {
        if (Selected is not { } entry) return;
        string target = Join(_path, entry.Name);
        if (MessageBox.Show(this, $"Удалить «{target}»?", "Удаление с накопителя",
            MessageBoxButtons.YesNo, MessageBoxIcon.Warning, MessageBoxDefaultButton.Button2) == DialogResult.Yes)
            await ChangeAsync("delete", target);
    }

    private Task ChangeAsync(string operation, string path, string? destination = null)
    {
        CardClient? client = _client;
        if (client is null) return Task.CompletedTask;
        return RunAsync(async () =>
        {
            await Task.Run(() => client.Execute(operation, path, destination));
            filesListView.Items.Clear();
            await LoadDirectoryAsync(client, _path);
        });
    }

    private Task<bool> RunTransferAsync(Func<CancellationToken, Task> action) => RunAsync(async () =>
    {
        using var cancellation = new CancellationTokenSource();
        _transfer = cancellation;
        UpdateControls();
        try
        {
            await action(cancellation.Token);
        }
        finally
        {
            _transfer = null;
            transferProgress.Visible = false;
        }
    });

    private Action<int> TransferProgress(string action, string name)
    {
        CancellationTokenSource? current = _transfer;
        IProgress<int> progress = new Progress<int>(percent =>
        {
            if (_transfer != current) return;
            transferProgress.Value = percent;
            statusLabel.Text = $"{action} «{name}» — {percent}%";
        });
        transferProgress.Value = 0;
        transferProgress.Visible = true;
        statusLabel.Text = $"{action} «{name}» — 0%";
        return progress.Report;
    }

    private async Task UploadAsync()
    {
        if (_client is null || _busy) return;
        using var dialog = new OpenFileDialog
        {
            Title = "Файлы для записи на накопитель",
            Multiselect = true,
            InitialDirectory = ExistingLocalFolder(_settings.UploadFolder) ?? _pcHistory.Current ?? ""
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            _settings.UploadFolder = Path.GetDirectoryName(dialog.FileName);
            await UploadFilesAsync(dialog.FileNames, _path);
        }
    }

    private async Task DownloadAsync()
    {
        if (_client is not { } client || Selected is not { IsDirectory: false } entry) return;
        using var dialog = new SaveFileDialog
        {
            Title = "Сохранить файл на ПК",
            FileName = entry.Name,
            InitialDirectory = ExistingLocalFolder(_settings.DownloadFolder) ?? _pcHistory.Current ?? "",
            AddExtension = false
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        string target = dialog.FileName;
        _settings.DownloadFolder = Path.GetDirectoryName(target);
        bool replace = File.Exists(target);
        string cardPath = Join(_path, entry.Name);

        await RunTransferAsync(async cancel =>
        {
            await SaveCardFileAsync(client, cardPath, target, replace, cancel);
            if (StringComparer.OrdinalIgnoreCase.Equals(Path.GetDirectoryName(target), _pcHistory.Current))
                await LoadPcDirectoryAsync(_pcHistory.Current!);
            statusLabel.Text = $"Сохранено на ПК: {target}";
        });
    }

    private async Task OpenAsync()
    {
        if (_client is not { } client || SelectedItem is not { } item) return;
        if (item.Tag is not CardEntry entry)
        {
            await NavigateAsync(ParentPath(_path));
            return;
        }

        string cardPath = Join(_path, entry.Name);
        if (entry.IsDirectory)
        {
            await NavigateAsync(cardPath);
            return;
        }

        bool text = FilePreview.IsText(entry.Name);
        if (!text && !FilePreview.IsImage(entry.Name))
        {
            ReportError("Встроенный просмотр доступен для текстов и изображений. Этот файл можно скачать на ПК.");
            return;
        }

        using var content = new MemoryStream();
        bool downloaded = await RunTransferAsync(async cancel =>
        {
            Action<int> progress = TransferProgress("Открытие", entry.Name);
            await Task.Run(() => client.Download(cardPath, content, cancel, progress,
                text ? FilePreview.MaxTextBytes : FilePreview.MaxImageBytes));
            statusLabel.Text = $"Открыто: {entry.Name}";
        });
        if (!downloaded) return;

        try
        {
            if (text)
            {
                FilePreview.EditText(this, entry.Name, content.ToArray(), (editor, bytes, cancel, progress) =>
                    RunAsync(async () =>
                    {
                        if (_client is not { } connected)
                            throw new IOException("Соединение с платой закрыто. Скопируйте правки перед закрытием редактора.");
                        using var source = new MemoryStream(bytes, writable: false);
                        await Task.Run(() => connected.Upload(cardPath, source, true, cancel, progress));
                        // The save has committed even if the subsequent directory refresh fails.
                    }, editor));
                if (_client is not null) await NavigateAsync(_path);
            }
            else
            {
                FilePreview.ShowImage(this, entry.Name, content);
            }
        }
        catch (Exception ex) when (ex is IOException or ArgumentException)
        {
            ReportError(ex.Message);
        }
    }

    private bool CheckName(string name)
    {
        if (name.Length is > 0 and <= 255 && !name.StartsWith(' ') &&
            !name.EndsWith(' ') && !name.EndsWith('.') &&
            !name.Any(ch => ch < ' ' || ch == 0x7F || "\"*/:<>?\\|".Contains(ch)))
            return true;

        ReportError("Введите допустимое имя файла или папки длиной от 1 до 255 символов.");
        return false;
    }

    private static string Join(string path, string name) => path.TrimEnd('/') + "/" + name;
    private static string ParentPath(string path) => path[..Math.Max(1, path.LastIndexOf('/'))];

    private string? Prompt(string title, string label, string initialValue)
    {
        using var dialog = new Form
        {
            Text = title,
            ClientSize = new Size(470, 110),
            StartPosition = FormStartPosition.CenterParent,
            FormBorderStyle = FormBorderStyle.FixedDialog,
            MaximizeBox = false,
            MinimizeBox = false,
            ShowInTaskbar = false
        };
        var hint = new Label { Text = label, AutoSize = true, Location = new Point(12, 12) };
        var input = new TextBox { Text = initialValue, Location = new Point(12, 34), Width = 446 };
        var ok = new Button { Text = "OK", DialogResult = DialogResult.OK, Location = new Point(302, 72) };
        var cancel = new Button { Text = "Отмена", DialogResult = DialogResult.Cancel, Location = new Point(383, 72) };
        dialog.Controls.AddRange([hint, input, ok, cancel]);
        dialog.AcceptButton = ok;
        dialog.CancelButton = cancel;
        dialog.ActiveControl = input;
        input.SelectAll();
        return dialog.ShowDialog(this) == DialogResult.OK ? input.Text : null;
    }


}
