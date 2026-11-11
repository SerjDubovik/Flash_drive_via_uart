using System.IO.Ports;
using System.Text.Json;

namespace SdCardBrowser;

internal sealed record CardEntry(string Name, bool IsDirectory, uint Size);

internal sealed class FatFsException(int result) : IOException(Describe(result))
{
    public int Result { get; } = result;
    public bool RequiresReconnect { get; } = result is 1 or 2 or 3 or 9 or 12 or 13 or 15 or 16 or 18;

    private static string Describe(int result)
    {
        string detail = result switch
        {
            1 => "Ошибка ввода-вывода накопителя",
            3 => "Накопитель не готов",
            4 => "Файл не найден",
            5 => "Папка не найдена",
            6 => "Недопустимое имя или путь",
            7 => "Операция запрещена, папка непуста или на карте нет места",
            8 => "Объект с таким именем уже существует",
            10 => "Накопитель защищён от записи",
            12 => "Накопитель не смонтирован",
            13 => "На накопителе не найдена файловая система FAT",
            14 => "Не удалось создать файловую систему FAT",
            15 => "Истекло время ожидания на плате",
            19 => "Плата отклонила параметры команды",
            _ => "Ошибка файловой системы"
        };
        return $"{detail} (FatFs {result}).";
    }
}

internal sealed class CardLinkException(string message, Exception? inner = null) : IOException(message, inner);

// The form permits exactly one in-flight command and runs it off the UI thread.
internal sealed class CardClient : IDisposable
{
    private const int BlockSize = 2048;
    private readonly SerialPort _port;
    private bool _supportsTransfer;
    private uint _nextId = (uint)Random.Shared.NextInt64(1, uint.MaxValue);

    private CardClient(string portName, int baudRate)
    {
        _port = new SerialPort(portName, baudRate, Parity.None, 8, StopBits.One)
        {
            Handshake = Handshake.None,
            NewLine = "\n",
            ReadTimeout = 12000,
            WriteTimeout = 12000
        };
    }

    public static CardClient Open(string portName, int baudRate)
    {
        var client = new CardClient(portName, baudRate);
        try
        {
            client._port.Open();
            client._port.DiscardInBuffer();
            return client;
        }
        catch
        {
            client.Dispose();
            throw;
        }
    }

    public void Mount(bool nor)
    {
        try
        {
            Exchange(nor ? "mntnor" : "mount", null, null, null, onResult: reply =>
                _supportsTransfer = reply.TryGetProperty("version", out var version) && version.GetInt32() >= 2);
        }
        catch (FatFsException ex) when (nor && ex.Result is 6 or 19)
        {
            throw new IOException("Прошивка STM32 не поддерживает том W25Q16. Обновите её и подключитесь снова.", ex);
        }
    }

    public List<CardEntry> List(string path)
    {
        var entries = new List<CardEntry>();
        Exchange("list", path, null, reply =>
        {
            string name = reply.GetProperty("name").GetString()
                ?? throw new CardLinkException("Плата вернула пустое имя.");
            entries.Add(new CardEntry(name, reply.GetProperty("directory").GetBoolean(),
                reply.GetProperty("size").GetUInt32()));
        });
        return entries;
    }

    public ulong? GetFreeBytes()
    {
        ulong bytes = 0;
        int timeout = _port.ReadTimeout;
        try
        {
            // FatFs may scan the allocation table on the first query after a mount.
            _port.ReadTimeout = 60000;
            Exchange("space", null, null, reply => bytes = reply.GetProperty("freeBytes").GetUInt64(),
                singlePayload: true);
            return bytes;
        }
        catch (FatFsException)
        {
            return null;
        }
        finally
        {
            _port.ReadTimeout = timeout;
        }
    }

    public void Execute(string operation, string? path = null, string? destination = null) =>
        Exchange(operation, path, destination, null);

    public void Upload(string path, Stream source, bool replace, CancellationToken cancel, Action<int> progress)
    {
        EnsureTransferSupport();
        if (source.Length > uint.MaxValue)
            throw new IOException("Файл превышает максимальный размер FAT32 (4 ГиБ минус 1 байт).");

        uint size = (uint)source.Length;
        cancel.ThrowIfCancellationRequested();
        Exchange(replace ? "replace" : "put", path, null, null, size: size);
        try
        {
            var buffer = new byte[BlockSize];
            int lastPercent = -1;
            for (uint offset = 0; offset < size;)
            {
                cancel.ThrowIfCancellationRequested();
                int length = (int)Math.Min((uint)buffer.Length, size - offset);
                var bytes = buffer.AsSpan(0, length);
                source.ReadExactly(bytes);
                Exchange("write", null, null, null, offset: offset, crc: Crc32(bytes),
                    data: Convert.ToHexString(bytes));
                offset += (uint)length;
                ReportProgress(progress, offset, size, ref lastPercent);
            }
            cancel.ThrowIfCancellationRequested();
            Execute("finish");
        }
        catch (Exception ex) when (ex is not CardLinkException)
        {
            AbortAfterError();
            throw;
        }
    }

    public void Download(string path, Stream destination, CancellationToken cancel, Action<int> progress,
        uint maxSize = uint.MaxValue)
    {
        cancel.ThrowIfCancellationRequested();
        EnsureTransferSupport();
        uint size = 0;
        Exchange("get", path, null, reply => size = reply.GetProperty("size").GetUInt32(), singlePayload: true);
        try
        {
            if (size > maxSize)
                throw new IOException($"Файл больше лимита просмотра ({maxSize / 1024 / 1024} МиБ). Скачайте его на ПК.");

            var buffer = new byte[BlockSize];
            int lastPercent = -1;
            for (uint offset = 0; offset < size;)
            {
                cancel.ThrowIfCancellationRequested();
                int expected = (int)Math.Min((uint)buffer.Length, size - offset);
                Exchange("read", null, null, reply =>
                {
                    string data = reply.GetProperty("data").GetString() ?? "";
                    if (reply.GetProperty("offset").GetUInt32() != offset || data.Length != expected * 2 ||
                        Convert.FromHexString(data, buffer, out _, out int written) != System.Buffers.OperationStatus.Done ||
                        written != expected || Crc32(buffer.AsSpan(0, written)) != reply.GetProperty("crc").GetUInt32())
                    {
                        throw new CardLinkException("Нарушена целостность блока данных с карты.");
                    }
                }, singlePayload: true, offset: offset);
                destination.Write(buffer, 0, expected);
                offset += (uint)expected;
                ReportProgress(progress, offset, size, ref lastPercent);
            }
            cancel.ThrowIfCancellationRequested();
            Execute("finish");
        }
        catch (Exception ex) when (ex is not CardLinkException)
        {
            AbortAfterError();
            throw;
        }
    }

    private void EnsureTransferSupport()
    {
        if (!_supportsTransfer)
            throw new IOException("Для передачи файлов прошейте обновлённую прошивку STM32 и подключитесь заново.");
    }

    private void AbortAfterError()
    {
        try
        {
            Execute("abort");
        }
        catch (FatFsException ex) when (ex.RequiresReconnect)
        {
            throw new CardLinkException("Плата не смогла закрыть передачу. Подключитесь снова.", ex);
        }
    }

    private static void ReportProgress(Action<int> progress, uint done, uint size, ref int lastPercent)
    {
        int percent = (int)(100UL * done / size);
        if (percent == lastPercent) return;
        lastPercent = percent;
        progress(percent);
    }

    private static uint Crc32(ReadOnlySpan<byte> bytes)
    {
        uint crc = uint.MaxValue;
        foreach (byte value in bytes)
        {
            crc ^= value;
            for (int bit = 0; bit < 8; bit++)
                crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xEDB88320u : 0);
        }
        return ~crc;
    }

    private void Exchange(string operation, string? path, string? destination, Action<JsonElement>? onPayload,
        bool singlePayload = false, uint? size = null, uint? offset = null, uint? crc = null, string? data = null,
        Action<JsonElement>? onResult = null)
    {
        uint id = ++_nextId;
        if (id == 0) id = ++_nextId;
        var request = new Dictionary<string, object> { ["id"] = id, ["op"] = operation };
        if (path is not null) request.Add("path", path);
        if (destination is not null) request.Add("to", destination);
        if (size is not null) request.Add("size", size.Value);
        if (offset is not null) request.Add("offset", offset.Value);
        if (crc is not null) request.Add("crc", crc.Value);
        if (data is not null) request.Add("data", data);

        try
        {
            // A new record separator discards a previous unfinished frame.
            _port.WriteLine("\u001e" + JsonSerializer.Serialize(request));
            bool receivedPayload = false;
            while (true)
            {
                using var line = JsonDocument.Parse(_port.ReadLine());
                JsonElement reply = line.RootElement;
                uint responseId = reply.GetProperty("id").GetUInt32();
                if (responseId == 0) throw new CardLinkException("Плата обнаружила повреждённый кадр UART.");
                if (responseId != id) throw new CardLinkException("Плата вернула ответ на другую команду.");

                if (reply.TryGetProperty("result", out JsonElement result))
                {
                    int code = result.GetInt32();
                    if (code != 0) throw new FatFsException(code);
                    if (singlePayload && !receivedPayload)
                        throw new CardLinkException("В ответе платы отсутствуют данные.");
                    onResult?.Invoke(reply);
                    return;
                }

                if (onPayload is null || (singlePayload && receivedPayload))
                    throw new CardLinkException("Получен неожиданный ответ платы.");
                onPayload(reply);
                receivedPayload = true;
            }
        }
        catch (TimeoutException ex)
        {
            throw new CardLinkException(
                "Истекло время ожидания ответа платы. После подключения проверьте состояние файлов.", ex);
        }
        catch (Exception ex) when (ex is InvalidOperationException or JsonException or KeyNotFoundException or
            FormatException or UnauthorizedAccessException || ex is IOException and not (FatFsException or CardLinkException))
        {
            throw new CardLinkException($"Ошибка обмена с платой: {ex.Message}", ex);
        }
    }

    public void Dispose() => _port.Dispose();
}
