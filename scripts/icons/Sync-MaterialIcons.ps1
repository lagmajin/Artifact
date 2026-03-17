param(
    [Parameter(Mandatory = $true)]
    [string[]]$Icons,
    [ValidateSet("materialicons", "materialiconsoutlined", "materialiconsround", "materialiconssharp", "materialiconstwotone")]
    [string]$Family = "materialiconsoutlined",
    [ValidateSet("20", "24", "36", "48")]
    [string]$Size = "24",
    [string]$Category = "general",
    [switch]$GenerateVsColorVariants,
    [switch]$UpdateIconList
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..\\..\\..")).Path
}

function Ensure-Directory([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Resolve-MaterialIconUrl([string]$FamilyName, [string]$IconId, [string]$PixelSize) {
    $versions = 80..1
    foreach ($v in $versions) {
        $url = "https://fonts.gstatic.com/s/i/$FamilyName/$IconId/v$v/${PixelSize}px.svg"
        try {
            $response = Invoke-WebRequest -Uri $url -Method Head -UseBasicParsing -TimeoutSec 15
            if ($response.StatusCode -eq 200) {
                return $url
            }
        } catch {
            continue
        }
    }
    throw "Icon '$IconId' was not found on fonts.gstatic.com for family '$FamilyName'."
}

function Download-Icon([string]$Url, [string]$OutPath) {
    Invoke-WebRequest -Uri $Url -OutFile $OutPath -UseBasicParsing -TimeoutSec 30
}

function Convert-ToVsPalette([string]$SvgText, [string]$HexColor) {
    $result = $SvgText
    $result = [regex]::Replace($result, 'fill="(?!none|transparent)[^"]*"', "fill=""$HexColor""")
    $result = [regex]::Replace($result, '<(path|circle|rect|polygon|polyline|ellipse)(?![^>]*\bfill=)([^>]*)>', "<`$1 fill=""$HexColor""`$2>")
    return $result
}

function Write-VsVariants([string]$SourceSvgPath, [string]$IconId, [string]$VsBaseDir) {
    $palette = [ordered]@{
        neutral = "#C5C5C5"
        blue    = "#4FC1FF"
        green   = "#89D185"
        yellow  = "#DCDCAA"
        orange  = "#CE9178"
        red     = "#F48771"
        purple  = "#C586C0"
    }

    $svg = Get-Content -Raw -LiteralPath $SourceSvgPath
    foreach ($tone in $palette.Keys) {
        $toneDir = Join-Path $VsBaseDir $tone
        Ensure-Directory -Path $toneDir
        $outPath = Join-Path $toneDir "$IconId.svg"
        $colored = Convert-ToVsPalette -SvgText $svg -HexColor $palette[$tone]
        Set-Content -LiteralPath $outPath -Value $colored -NoNewline
    }
}

function Update-IconListJson([string]$JsonPath, [string[]]$IconIds, [string]$IconCategory) {
    $raw = Get-Content -Raw -LiteralPath $JsonPath
    $data = $raw | ConvertFrom-Json

    $existingIds = @{}
    foreach ($entry in $data.icons) {
        $existingIds[$entry.id] = $true
    }

    foreach ($iconId in $IconIds) {
        if (-not $existingIds.ContainsKey($iconId)) {
            $entry = [ordered]@{
                path     = "App/Icon/Material/$iconId.svg"
                category = $IconCategory
                id       = $iconId
                file     = "$iconId.svg"
            }
            $data.icons += [pscustomobject]$entry
            $existingIds[$iconId] = $true
        }
    }

    $data.count = @($data.icons).Count
    $json = $data | ConvertTo-Json -Depth 10
    Set-Content -LiteralPath $JsonPath -Value $json
}

$repoRoot = Get-RepoRoot
$materialDir = Join-Path $repoRoot "Artifact\\App\\Icon\\Material"
$vsDir = Join-Path $repoRoot "Artifact\\App\\Icon\\MaterialVS"
$iconListPath = Join-Path $repoRoot "Artifact\\App\\Icon\\icon_list.json"

$normalizedIcons = @()
foreach ($entry in $Icons) {
    $normalizedIcons += ($entry -split ",")
}

Ensure-Directory -Path $materialDir
if ($GenerateVsColorVariants) {
    Ensure-Directory -Path $vsDir
}

$downloaded = @()
foreach ($iconId in $normalizedIcons) {
    $normalizedId = $iconId.Trim()
    if ([string]::IsNullOrWhiteSpace($normalizedId)) {
        continue
    }

    Write-Host "Resolving '$normalizedId'..."
    $url = Resolve-MaterialIconUrl -FamilyName $Family -IconId $normalizedId -PixelSize $Size
    $dest = Join-Path $materialDir "$normalizedId.svg"
    Download-Icon -Url $url -OutPath $dest
    Write-Host "Downloaded: $normalizedId -> $dest"

    if ($GenerateVsColorVariants) {
        Write-VsVariants -SourceSvgPath $dest -IconId $normalizedId -VsBaseDir $vsDir
        Write-Host "Generated VS color variants for: $normalizedId"
    }

    $downloaded += $normalizedId
}

if ($UpdateIconList -and $downloaded.Count -gt 0) {
    Update-IconListJson -JsonPath $iconListPath -IconIds $downloaded -IconCategory $Category
    Write-Host "Updated icon list: $iconListPath"
}

Write-Host "Done. Downloaded icons: $($downloaded -join ', ')"
