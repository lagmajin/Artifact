using System.Runtime.InteropServices;

namespace Artifact.Scripting;

public static class NativeCallbacks
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void OutputCallback(IntPtr text, int isError);

    public static void Report(OutputCallback callback, string text, bool isError)
    {
        var bytes = System.Text.Encoding.UTF8.GetBytes(text + "\0");
        var memory = Marshal.AllocHGlobal(bytes.Length);
        try
        {
            Marshal.Copy(bytes, 0, memory, bytes.Length);
            callback(memory, isError ? 1 : 0);
        }
        finally { Marshal.FreeHGlobal(memory); }
    }
}
