using System.Text.Json;

namespace SdCardBrowser;

internal sealed partial class BrowserForm
{
    private sealed class BrowserSettings
    {
        public string? Port { get; set; }
        public int BaudRate { get; set; } = 115200;
        public bool UseNor { get; set; }
        public bool TouchTab { get; set; }
        public string? PcFolder { get; set; }
        public string? SdFolder { get; set; }
        public string? NorFolder { get; set; }
        public string? UploadFolder { get; set; }
        public string? DownloadFolder { get; set; }
        public Rectangle WindowBounds { get; set; }
        public bool Maximized { get; set; }
        public double SplitterRatio { get; set; }
        public int[]? CardColumnWidths { get; set; }
        public int[]? PcColumnWidths { get; set; }
    }

    private static readonly string SettingsPath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "SdCardBrowser", "settings.json");

    private readonly BrowserSettings _settings = ReadSettings();

    private static BrowserSettings ReadSettings()
    {
        try
        {
            return JsonSerializer.Deserialize<BrowserSettings>(File.ReadAllText(SettingsPath)) ?? new();
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or JsonException)
        {
            return new();
        }
    }

    private void RestoreWindowSettings()
    {
        Rectangle bounds = _settings.WindowBounds;
        if (bounds.Width >= MinimumSize.Width && bounds.Height >= MinimumSize.Height &&
            Screen.AllScreens.Any(screen =>
            {
                Rectangle visible = Rectangle.Intersect(screen.WorkingArea, bounds);
                return visible.Width >= 100 && visible.Height >= 80;
            }))
        {
            StartPosition = FormStartPosition.Manual;
            Bounds = bounds;
        }
        if (_settings.Maximized) WindowState = FormWindowState.Maximized;
        RestoreColumnWidths(filesListView, _settings.CardColumnWidths);
        RestoreColumnWidths(pcListView, _settings.PcColumnWidths);
    }

    private static void RestoreColumnWidths(ListView list, int[]? widths)
    {
        if (widths is null || widths.Length != list.Columns.Count) return;
        for (int i = 0; i < widths.Length; ++i)
        {
            if (widths[i] is >= 0 and <= 10000) list.Columns[i].Width = widths[i];
        }
    }

    private void RestoreSplitter()
    {
        if (_settings.SplitterRatio is <= 0 or >= 1) return;
        int width = panes.ClientSize.Width - panes.SplitterWidth;
        int max = width - panes.Panel2MinSize;
        if (max < panes.Panel1MinSize) return;
        panes.SplitterDistance = Math.Clamp(
            (int)Math.Round(width * _settings.SplitterRatio), panes.Panel1MinSize, max);
    }

    private static string? ExistingLocalFolder(string? path) =>
        Path.IsPathFullyQualified(path ?? "") && Directory.Exists(path) ? path : null;

    private void SaveSettings()
    {
        _settings.BaudRate = baudComboBox.SelectedItem is int baudRate ? baudRate : 115200;
        _settings.UseNor = mediumComboBox.SelectedIndex == 1;
        _settings.TouchTab = TouchTabSelected;
        _settings.PcFolder = _pcHistory.Current ?? _settings.PcFolder;
        _settings.WindowBounds = WindowState == FormWindowState.Normal ? Bounds : RestoreBounds;
        _settings.Maximized = WindowState == FormWindowState.Maximized;
        _settings.CardColumnWidths = filesListView.Columns.Cast<ColumnHeader>().Select(column => column.Width).ToArray();
        _settings.PcColumnWidths = pcListView.Columns.Cast<ColumnHeader>().Select(column => column.Width).ToArray();

        string temporaryPath = SettingsPath + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(SettingsPath)!);
            File.WriteAllText(temporaryPath, JsonSerializer.Serialize(_settings, new JsonSerializerOptions { WriteIndented = true }));
            File.Move(temporaryPath, SettingsPath, overwrite: true);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            try { File.Delete(temporaryPath); }
            catch (Exception cleanup) when (cleanup is IOException or UnauthorizedAccessException) { }
            MessageBox.Show(this, "Не удалось сохранить настройки. " + ex.Message,
                "Настройки", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }
}
