using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Globalization;
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

    private static readonly SessionGlobals _globals = new();
    private static ScriptState<object?>? _session;
    private static readonly ScriptOptions DefaultOptions =
        ScriptOptions.Default
            .AddImports("System", "System.IO", "System.Linq", "System.Collections.Generic");

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int EvaluateCode(IntPtr codePointer, IntPtr resultPointer, int capacity)
    {
        try
        {
            var code = Marshal.PtrToStringUTF8(codePointer) ?? string.Empty;
            var result = CSharpScript.EvaluateAsync<object?>(code, DefaultOptions)
                .GetAwaiter()
                .GetResult();
            WriteResult(result?.ToString() ?? string.Empty, resultPointer, capacity);
            return 0;
        }
        catch (Exception exception)
        {
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
            WriteResult(_session.ReturnValue?.ToString() ?? string.Empty, resultPointer, capacity);
            return 0;
        }
        catch (Exception exception)
        {
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
            WriteResult(string.Empty, resultPointer, capacity);
            return 0;
        }
        catch (Exception exception)
        {
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
            WriteResult(replacement.ReturnValue?.ToString() ?? string.Empty, resultPointer, capacity);
            return 0;
        }
        catch (Exception exception)
        {
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
        WriteResult(string.Empty, resultPointer, capacity);
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
            WriteResult(result?.ToString() ?? string.Empty, resultPointer, capacity);
            return 0;
        }
        catch (Exception exception)
        {
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
