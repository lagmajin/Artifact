using Microsoft.CodeAnalysis.CSharp.Scripting;
using Microsoft.CodeAnalysis.Scripting;

namespace Artifact.Scripting;

public static class ArtifactScriptHost
{
    private static readonly ScriptOptions Options = ScriptOptions.Default
        .WithImports("System", "System.IO", "System.Linq", "System.Collections.Generic");

    public static string EvaluateCode(string code)
    {
        if (string.IsNullOrWhiteSpace(code)) return string.Empty;
        var state = CSharpScript.RunAsync(code, Options).GetAwaiter().GetResult();
        return state.ReturnValue?.ToString() ?? string.Empty;
    }

    public static string ExecuteFile(string path)
    {
        if (!File.Exists(path)) throw new FileNotFoundException("CSX file not found", path);
        return EvaluateCode(File.ReadAllText(path));
    }
}
