using System.Text;

namespace SdCardBrowser;

internal static class FilePreview
{
    public const uint MaxTextBytes = 1024 * 1024;
    public const uint MaxImageBytes = 16 * 1024 * 1024;

    public static bool IsText(string name) => Path.GetExtension(name).ToLowerInvariant() is
        ".txt" or ".csv" or ".log" or ".json" or ".xml" or ".ini" or ".md" or ".cfg" or ".conf" or ".yaml" or ".yml";

    public static bool IsImage(string name) => Path.GetExtension(name).ToLowerInvariant() is
        ".png" or ".jpg" or ".jpeg" or ".bmp" or ".gif" or ".tif" or ".tiff";

    public static void ShowImage(Form owner, string name, Stream data)
    {
        using var dialog = Window(name);
        try
        {
            data.Position = 0;
            using Image image = Image.FromStream(data);
            if ((long)image.Width * image.Height > 32_000_000)
                throw new IOException("Изображение превышает лимит просмотра в 32 мегапикселя.");

            var picture = new PictureBox { Dock = DockStyle.Fill, SizeMode = PictureBoxSizeMode.Zoom, Image = image };
            dialog.Controls.Add(picture);
            try
            {
                dialog.ShowDialog(owner);
            }
            finally
            {
                picture.Image = null;
            }
        }
        catch (Exception ex) when (ex is ArgumentException or OutOfMemoryException)
        {
            throw new IOException("Файл не удалось открыть как изображение.", ex);
        }
    }

    public static void EditText(Form owner, string name, byte[] data,
        Func<Form, byte[], CancellationToken, Action<int>, Task<bool>> save)
    {
        (Encoding encoding, string text, bool readOnly) = Decode(data);
        string lineEnding = FirstLineEnding(text);
        string savedText = text.ReplaceLineEndings("\r\n");
        using var dialog = Window(name);
        var editor = new TextBox
        {
            Dock = DockStyle.Fill, Multiline = true, ScrollBars = ScrollBars.Both,
            WordWrap = false, AcceptsReturn = true, AcceptsTab = true, ReadOnly = readOnly,
            MaxLength = (int)MaxTextBytes * 2, Text = savedText
        };
        var saveButton = new Button { Text = "Сохранить на карту", AutoSize = true, Visible = !readOnly };
        var closeButton = new Button { Text = "Закрыть", AutoSize = true };
        var status = new Label
        {
            Text = readOnly ? "Повреждённый текст или двоичные данные. Только просмотр." : encoding.EncodingName,
            AutoSize = true, Margin = new Padding(8)
        };
        var actions = new FlowLayoutPanel { Dock = DockStyle.Bottom, AutoSize = true, Padding = new Padding(4) };
        actions.Controls.AddRange([saveButton, closeButton, status]);
        dialog.Controls.Add(editor);
        dialog.Controls.Add(actions);

        CancellationTokenSource? pending = null;
        void UpdateSaveButton() => saveButton.Enabled = !readOnly && pending is null && editor.Text != savedText;
        editor.TextChanged += (_, _) => UpdateSaveButton();
        closeButton.Click += (_, _) =>
        {
            if (pending is not null) pending.Cancel();
            else dialog.Close();
        };
        dialog.FormClosing += (_, args) =>
        {
            if (pending is not null)
            {
                pending.Cancel();
                args.Cancel = true;
            }
            else if (!readOnly && editor.Text != savedText)
            {
                args.Cancel = MessageBox.Show(dialog, "Закрыть без сохранения правок?", "Редактор",
                    MessageBoxButtons.YesNo, MessageBoxIcon.Question, MessageBoxDefaultButton.Button2) != DialogResult.Yes;
            }
        };
        editor.KeyDown += (_, args) =>
        {
            if (args.Control && args.KeyCode == Keys.S)
            {
                args.SuppressKeyPress = true;
                saveButton.PerformClick();
            }
        };

        saveButton.Click += async (_, _) =>
        {
            string edited = editor.Text;
            byte[] bytes;
            try
            {
                byte[] preamble = encoding.GetPreamble();
                string content = edited.ReplaceLineEndings(lineEnding);
                int length = preamble.Length + encoding.GetByteCount(content);
                if (length > MaxTextBytes) throw new IOException("Текст превышает лимит редактора в 1 МиБ.");
                bytes = new byte[length];
                preamble.CopyTo(bytes, 0);
                encoding.GetBytes(content, bytes.AsSpan(preamble.Length));
            }
            catch (Exception ex) when (ex is IOException or EncoderFallbackException)
            {
                MessageBox.Show(dialog, ex.Message, "Редактор", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            using var cancellation = new CancellationTokenSource();
            pending = cancellation;
            closeButton.Text = "Отменить запись";
            editor.ReadOnly = true;
            status.Text = "Запись на карту…";
            UpdateSaveButton();
            var progress = new Progress<int>(percent =>
            {
                if (pending == cancellation) status.Text = $"Запись на карту… {percent}%";
            });
            try
            {
                if (await save(dialog, bytes, cancellation.Token, ((IProgress<int>)progress).Report))
                {
                    savedText = edited;
                    status.Text = "Сохранено на карте";
                }
                else
                {
                    status.Text = "Правки остались в редакторе";
                }
            }
            finally
            {
                pending = null;
                closeButton.Text = "Закрыть";
                editor.ReadOnly = false;
                UpdateSaveButton();
            }
        };

        UpdateSaveButton();
        dialog.ActiveControl = editor;
        editor.SelectionStart = 0;
        dialog.ShowDialog(owner);
    }

    private static (Encoding Encoding, string Text, bool ReadOnly) Decode(byte[] data)
    {
        Encoding[] bomEncodings =
        [
            new UTF32Encoding(false, true, true), new UTF32Encoding(true, true, true),
            new UTF8Encoding(true, true), new UnicodeEncoding(false, true, true),
            new UnicodeEncoding(true, true, true)
        ];
        foreach (Encoding encoding in bomEncodings)
        {
            byte[] preamble = encoding.GetPreamble();
            if (data.AsSpan().StartsWith(preamble))
                return DecodeForPreview(encoding, data.AsSpan(preamble.Length));
        }

        var utf8 = new UTF8Encoding(false, true);
        try
        {
            return InspectText(utf8, utf8.GetString(data));
        }
        catch (DecoderFallbackException)
        {
            Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
            Encoding ansi = Encoding.GetEncoding(1251, EncoderFallback.ExceptionFallback, DecoderFallback.ExceptionFallback);
            return DecodeForPreview(ansi, data);
        }
    }

    private static (Encoding Encoding, string Text, bool ReadOnly) DecodeForPreview(
        Encoding encoding, ReadOnlySpan<byte> data)
    {
        try
        {
            return InspectText(encoding, encoding.GetString(data));
        }
        catch (DecoderFallbackException)
        {
            var previewEncoding = Encoding.GetEncoding(encoding.CodePage, EncoderFallback.ExceptionFallback,
                new DecoderReplacementFallback("�"));
            return InspectText(encoding, previewEncoding.GetString(data), damaged: true);
        }
    }

    private static (Encoding Encoding, string Text, bool ReadOnly) InspectText(
        Encoding encoding, string text, bool damaged = false)
    {
        static bool IsBinaryControl(char ch) => char.IsControl(ch) && ch is not ('\t' or '\r' or '\n' or '\f');
        if (!text.Any(IsBinaryControl)) return (encoding, text, damaged);

        // WinForms text controls can truncate at a NUL. Show control bytes visibly.
        var visible = new StringBuilder(text.Length);
        foreach (char ch in text)
        {
            if (IsBinaryControl(ch)) visible.Append($"\\x{(int)ch:X2}");
            else visible.Append(ch);
        }
        return (encoding, visible.ToString(), true);
    }

    private static string FirstLineEnding(string text)
    {
        int index = text.AsSpan().IndexOfAny('\r', '\n');
        if (index < 0 || text[index] == '\n') return "\n";
        return index + 1 < text.Length && text[index + 1] == '\n' ? "\r\n" : "\r";
    }

    private static Form Window(string title) => new()
    {
        Text = title,
        StartPosition = FormStartPosition.CenterParent,
        ClientSize = new Size(800, 560),
        MinimumSize = new Size(480, 280),
        AutoScaleMode = AutoScaleMode.Dpi,
        ShowInTaskbar = false
    };
}
