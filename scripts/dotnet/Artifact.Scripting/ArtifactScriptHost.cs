using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Globalization;
using System.Text;
using Microsoft.CodeAnalysis.CSharp.Scripting;
using Microsoft.CodeAnalysis.Scripting;

public static class ArtifactScriptHost
{
    public sealed class SessionGlobals
    {
        public double Time { get; set; }
        public double DeltaTime { get; set; }
        public ulong Frame { get; set; }
    }

    private sealed class ErrorInfo
    {
        public string Type = string.Empty;
        public string Message = string.Empty;
        public string Stack = string.Empty;
        public readonly List<string> Diagnostics = new();

        public void Clear()
        {
            Type = string.Empty;
            Message = string.Empty;
            Stack = string.Empty;
            Diagnostics.Clear();
        }
    }

    private static readonly SessionGlobals _globals = new();
    private static ScriptState<object?>? _session;
    private static readonly ErrorInfo _lastError = new();

    // Roslyn scripting does not inherit the host project's references. Without
    // explicit references, imports such as System.Linq / System.IO resolve as
    // namespaces but the types inside them fail with CS0246 / CS1061.
    private static readonly ScriptOptions DefaultOptions =
        ScriptOptions.Default
            .AddImports("System", "System.IO", "System.Linq", "System.Collections.Generic",
                        "System.Text", "System.Text.RegularExpressions")
            .AddReferences(new[]
            {
                typeof(object).Assembly,
                typeof(Enumerable).Assembly,
                typeof(File).Assembly,
                typeof(Path).Assembly,
                typeof(Dictionary<,>).Assembly,
                typeof(Console).Assembly,
                typeof(System.Text.RegularExpressions.Regex).Assembly
            }.Distinct().ToArray());

    private static void RecordError(Exception exception)
    {
        _lastError.Clear();
        _lastError.Type = exception.GetType().FullName ?? exception.GetType().Name;
        _lastError.Message = exception.Message;
        _lastError.Stack = exception.StackTrace ?? string.Empty;
        if (exception is CompilationErrorException compilation)
        {
            foreach (var diagnostic in compilation.Diagnostics)
            {
                _lastError.Diagnostics.Add(
                    diagnostic.Id + ": " + diagnostic.GetMessage(CultureInfo.InvariantCulture));
            }
        }
    }

    private static void ClearError() => _lastError.Clear();

    private static string JsonEscape(string value)
    {
        var builder = new StringBuilder(value.Length + 2);
        foreach (var character in value)
        {
            switch (character)
            {
                case '\\': builder.Append("\\\\"); break;
                case '"': builder.Append("\\\""); break;
                case '\n': builder.Append("\\n"); break;
                case '\r': builder.Append("\\r"); break;
                case '\t': builder.Append("\\t"); break;
                default:
                    if (character < ' ')
                    {
                        builder.Append("\\u").Append(
                            ((int)character).ToString("x4", CultureInfo.InvariantCulture));
                    }
                    else
                    {
                        builder.Append(character);
                    }
                    break;
            }
        }
        return builder.ToString();
    }

    private static string StructuredErrorJson()
    {
        var builder = new StringBuilder();
        builder.Append("{\"type\":\"").Append(JsonEscape(_lastError.Type)).Append('\"');
        builder.Append(",\"message\":\"").Append(JsonEscape(_lastError.Message)).Append('\"');
        builder.Append(",\"stack\":\"").Append(JsonEscape(_lastError.Stack)).Append('\"');
        builder.Append(",\"diagnostics\":[");
        for (var index = 0; index < _lastError.Diagnostics.Count; ++index)
        {
            if (index > 0) builder.Append(',');
            builder.Append('\"').Append(JsonEscape(_lastError.Diagnostics[index])).Append('\"');
        }
        builder.Append("]}");
        return builder.ToString();
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int EvaluateCode(IntPtr codePointer, IntPtr resultPointer, int capacity)
    {
        try
        {
            var code = Marshal.PtrToStringUTF8(codePointer) ?? string.Empty;
            var result = CSharpScript.EvaluateAsync<object?>(code, DefaultOptions)
                .GetAwaiter()
                .GetResult();
            ClearError();
            WriteResult(result?.ToString() ?? string.Empty, resultPointer, capacity);
            return 0;
        }
        catch (Exception exception)
        {
            RecordError(exception);
            WriteResult(exception.ToString(), resultPointer, capacity);
            return 1;
        }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int EvaluateSession(IntPtr codePointer, IntPtr resultPointer, int capacity)
    {
        try
        {
            var code = Marshal.PtrToStringUTF8(codePointer) ?? string.Empty;
            _session = (_session == null
                ? CSharpScript.RunAsync(code, _globals, DefaultOptions)
                : _session.ContinueWithAsync(code, DefaultOptions))
                .GetAwaiter().GetResult();
            ClearError();
            WriteResult(_session.ReturnValue?.ToString() ?? string.Empty, resultPointer, capacity);
            return 0;
        }
        catch (Exception exception)
        {
            RecordError(exception);
            WriteResult(exception.ToString(), resultPointer, capacity);
            return 1;
        }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int SetSessionTime(IntPtr payloadPointer, IntPtr resultPointer, int capacity)
    {
        try
        {
            var payload = Marshal.PtrToStringUTF8(payloadPointer) ?? string.Empty;
            var fields = payload.Split(';');
            if (fields.Length != 3)
                throw new FormatException("Expected time;deltaTime;frame");
            _globals.Time = double.Parse(fields[0], CultureInfo.InvariantCulture);
            _globals.DeltaTime = double.Parse(fields[1], CultureInfo.InvariantCulture);
            _globals.Frame = ulong.Parse(fields[2], CultureInfo.InvariantCulture);
            ClearError();
            WriteResult(string.Empty, resultPointer, capacity);
            return 0;
        }
        catch (Exception exception)
        {
            RecordError(exception);
            WriteResult(exception.ToString(), resultPointer, capacity);
            return 1;
        }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int ReloadSession(IntPtr codePointer, IntPtr resultPointer, int capacity)
    {
        try
        {
            var code = Marshal.PtrToStringUTF8(codePointer) ?? string.Empty;
            // Keep the current state untouched until the replacement has
            // compiled and evaluated successfully.
            var replacement = CSharpScript.RunAsync(code, _globals, DefaultOptions)
                .GetAwaiter().GetResult();
            _session = replacement;
            ClearError();
            WriteResult(replacement.ReturnValue?.ToString() ?? string.Empty, resultPointer, capacity);
            return 0;
        }
        catch (Exception exception)
        {
            RecordError(exception);
            WriteResult(exception.ToString(), resultPointer, capacity);
            return 1;
        }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int ResetSession(IntPtr ignored, IntPtr resultPointer, int capacity)
    {
        _session = null;
        _globals.Time = 0.0;
        _globals.DeltaTime = 0.0;
        _globals.Frame = 0;
        ClearError();
        WriteResult(string.Empty, resultPointer, capacity);
        return 0;
    }

    /// <summary>
    /// Returns the most recent error as a small JSON document:
    /// { "type", "message", "stack", "diagnostics": [ ... ] }.
    /// Companion to the existing human-readable exception string, so callers
    /// can inspect structured information without parsing exception.ToString().
    /// </summary>
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int GetLastErrorJson(IntPtr ignored, IntPtr resultPointer, int capacity)
    {
        WriteResult(StructuredErrorJson(), resultPointer, capacity);
        return 0;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int ExecuteFile(IntPtr pathPointer, IntPtr resultPointer, int capacity)
    {
        try
        {
            var path = Marshal.PtrToStringUTF8(pathPointer) ?? string.Empty;
            var code = File.ReadAllText(path);
            var result = CSharpScript.EvaluateAsync<object?>(code, DefaultOptions)
                .GetAwaiter()
                .GetResult();
            ClearError();
            WriteResult(result?.ToString() ?? string.Empty, resultPointer, capacity);
            return 0;
        }
        catch (Exception exception)
        {
            RecordError(exception);
            WriteResult(exception.ToString(), resultPointer, capacity);
            return 1;
        }
    }

    private static void WriteResult(string value, IntPtr destination, int capacity)
    {
        if (destination == IntPtr.Zero || capacity <= 0)
            return;

        var bytes = System.Text.Encoding.UTF8.GetBytes(value);
        var count = Math.Min(bytes.Length, capacity - 1);
        Marshal.Copy(bytes, 0, destination, count);
        Marshal.WriteByte(destination, count, 0);
    }
}
