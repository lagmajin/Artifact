using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Microsoft.CodeAnalysis.CSharp.Scripting;
using Microsoft.CodeAnalysis.Scripting;

public static class ArtifactScriptHost
{
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
