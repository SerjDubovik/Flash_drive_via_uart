namespace SdCardBrowser;

internal sealed partial class BrowserForm
{
    private const string CardDragFormat = "SdCardBrowser.CardFiles";
    private sealed record CardDrag(string Directory, CardEntry[] Files);
    private CardDrag? _cardDrag;

    private void InitializeDragAndDrop()
    {
        pcListView.ItemDrag += (_, e) =>
        {
            if (_busy || e.Button != MouseButtons.Left || e.Item is not ListViewItem { Tag: PcEntry { IsDirectory: false } }) return;
            string[] paths = pcListView.SelectedItems.Cast<ListViewItem>()
                .Select(item => item.Tag).OfType<PcEntry>()
                .Where(entry => !entry.IsDirectory).Select(entry => entry.Path).ToArray();
            if (paths.Length > 0) pcListView.DoDragDrop(new DataObject(DataFormats.FileDrop, paths), DragDropEffects.Copy);
        };
        filesListView.ItemDrag += (_, e) =>
        {
            if (_busy || e.Button != MouseButtons.Left || e.Item is not ListViewItem { Tag: CardEntry { IsDirectory: false } }) return;
            CardEntry[] entries = filesListView.SelectedItems.Cast<ListViewItem>()
                .Select(item => item.Tag).OfType<CardEntry>().Where(entry => !entry.IsDirectory).ToArray();
            if (entries.Length == 0) return;
            _cardDrag = new CardDrag(_path, entries);
            try
            {
                var data = new DataObject();
                data.SetData(CardDragFormat, autoConvert: false, "files");
                filesListView.DoDragDrop(data, DragDropEffects.Copy);
            }
            finally
            {
                _cardDrag = null;
            }
        };

        filesListView.DragEnter += PreviewUpload;
        filesListView.DragOver += PreviewUpload;
        filesListView.DragDrop += async (_, e) =>
        {
            if (!CanDrop(e, DataFormats.FileDrop) ||
                _client is null || e.Data?.GetData(DataFormats.FileDrop, false) is not string[] paths) return;
            Point point = filesListView.PointToClient(new Point(e.X, e.Y));
            string target = filesListView.GetItemAt(point.X, point.Y) switch
            {
                { Tag: CardEntry { IsDirectory: true } directory } => Join(_path, directory.Name),
                { Tag: null } => ParentPath(_path),
                _ => _path
            };
            string[] files = paths.Where(File.Exists).ToArray();
            if (files.Length == 0)
            {
                ReportError("Выберите файлы для копирования. Папки можно использовать как место назначения.");
                return;
            }
            await UploadFilesAsync(files, target);
        };

        pcListView.DragEnter += PreviewDownload;
        pcListView.DragOver += PreviewDownload;
        pcListView.DragDrop += async (_, e) =>
        {
            if (!CanDrop(e, CardDragFormat) || _cardDrag is not { } drag || _pcHistory.Current is not { } current) return;
            Point point = pcListView.PointToClient(new Point(e.X, e.Y));
            string target = pcListView.GetItemAt(point.X, point.Y) switch
            {
                { Tag: PcEntry { IsDirectory: true } directory } => directory.Path,
                { Tag: null } => Directory.GetParent(current)?.FullName ?? current,
                _ => current
            };
            await DownloadFilesAsync(drag, target);
        };
    }

    private bool CanDrop(DragEventArgs e, string format) =>
        !_busy && (e.AllowedEffect & DragDropEffects.Copy) != 0 && e.Data?.GetDataPresent(format, false) == true;

    private void PreviewUpload(object? sender, DragEventArgs e) =>
        e.Effect = _client is not null && CanDrop(e, DataFormats.FileDrop) ? DragDropEffects.Copy : DragDropEffects.None;

    private void PreviewDownload(object? sender, DragEventArgs e) =>
        e.Effect = _cardDrag is not null && _pcHistory.Current is not null && CanDrop(e, CardDragFormat)
            ? DragDropEffects.Copy : DragDropEffects.None;

    private Task UploadFilesAsync(string[] paths, string targetDirectory)
    {
        if (_client is not { } client) return Task.CompletedTask;
        return RunTransferAsync(async cancel =>
        {
            List<CardEntry> existing = await Task.Run(() => client.List(targetDirectory));
            int copied = 0;
            foreach (string localPath in paths)
            {
                cancel.ThrowIfCancellationRequested();
                string name = Path.GetFileName(localPath);
                if (!CheckName(name)) continue;

                CardEntry? match = existing.Find(entry => StringComparer.OrdinalIgnoreCase.Equals(entry.Name, name));
                if (match is { IsDirectory: true })
                {
                    ReportError($"Имя «{name}» уже занято папкой на карте.");
                    continue;
                }
                if (match is not null && !ConfirmCopyReplacement(name, "на карте")) continue;

                Action<int> progress = TransferProgress("Запись на карту", name);
                await Task.Run(() =>
                {
                    using var source = File.OpenRead(localPath);
                    client.Upload(Join(targetDirectory, name), source, match is not null, cancel, progress);
                });
                if (match is null) existing.Add(new CardEntry(name, false, 0));
                ++copied;
                if (StringComparer.OrdinalIgnoreCase.Equals(targetDirectory, _path))
                    await LoadDirectoryAsync(client, _path);
            }
            statusLabel.Text = $"Скопировано на карту: {copied}.";
        });
    }

    private Task DownloadFilesAsync(CardDrag drag, string targetDirectory)
    {
        if (_client is not { } client) return Task.CompletedTask;
        return RunTransferAsync(async cancel =>
        {
            int copied = 0;
            foreach (CardEntry entry in drag.Files)
            {
                cancel.ThrowIfCancellationRequested();
                if (entry.Name.Length == 0 || entry.Name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0 ||
                    entry.Name.EndsWith('.') || entry.Name.EndsWith(' '))
                {
                    ReportError($"Имя «{entry.Name}» нельзя использовать для файла Windows.");
                    continue;
                }
                string target = Path.Join(targetDirectory, entry.Name);
                if (Directory.Exists(target))
                {
                    ReportError($"Имя «{entry.Name}» уже занято папкой на ПК.");
                    continue;
                }
                bool replace = File.Exists(target);
                if (replace && !ConfirmCopyReplacement(entry.Name, "на ПК")) continue;

                await SaveCardFileAsync(client, Join(drag.Directory, entry.Name), target, replace, cancel);
                ++copied;
                if (StringComparer.OrdinalIgnoreCase.Equals(targetDirectory, _pcHistory.Current))
                    await LoadPcDirectoryAsync(targetDirectory);
            }
            statusLabel.Text = $"Скопировано на ПК: {copied}.";
        });
    }

    private bool ConfirmCopyReplacement(string name, string location) =>
        MessageBox.Show(this, $"Заменить файл «{name}» {location}?", "Копирование",
            MessageBoxButtons.YesNo, MessageBoxIcon.Question, MessageBoxDefaultButton.Button2) == DialogResult.Yes;

    private Task SaveCardFileAsync(CardClient client, string cardPath, string target, bool replace, CancellationToken cancel)
    {
        Action<int> progress = TransferProgress("Скачивание", Path.GetFileName(cardPath));
        return Task.Run(() =>
        {
            string stage = Path.Combine(Path.GetDirectoryName(target)!, $".{Guid.NewGuid():N}.part");
            var output = new FileStream(stage, FileMode.CreateNew, FileAccess.Write, FileShare.None);
            try
            {
                using (output) client.Download(cardPath, output, cancel, progress);
                cancel.ThrowIfCancellationRequested();
                File.Move(stage, target, overwrite: replace);
            }
            finally
            {
                try { File.Delete(stage); }
                catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
                {
                    // Keep the transfer error if Windows also rejects temporary-file cleanup.
                }
            }
        });
    }
}
