$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $scriptDir)))
$sourcePaths = @(
    (Join-Path $repoRoot "components/apps/homecare_hub/HomeCareHub.cpp"),
    (Join-Path $repoRoot "components/apps/homecare_hub/HomeCareWeather.cpp"),
    (Join-Path $repoRoot "components/apps/homecare_hub/HomeCareWeatherCity.cpp"),
    (Join-Path $repoRoot "components/apps/setting/Setting.cpp")
)
$fontPath = Join-Path $repoRoot "managed_components/lvgl__lvgl/scripts/built_in_font/SimSun.woff"

foreach ($sourcePath in $sourcePaths) {
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        throw "Font source not found: $sourcePath"
    }
}
if (-not (Test-Path -LiteralPath $fontPath)) {
    throw "SimSun font not found: $fontPath"
}

function Get-CppStringLiteralText {
    param([string]$Text)

    $result = New-Object System.Text.StringBuilder
    $state = "code"
    $length = $Text.Length

    for ($i = 0; $i -lt $length; $i++) {
        $ch = $Text[$i]
        $next = if ($i + 1 -lt $length) { $Text[$i + 1] } else { [char]0 }

        switch ($state) {
            "code" {
                if ($ch -eq "/" -and $next -eq "/") {
                    $state = "line_comment"
                    $i++
                } elseif ($ch -eq "/" -and $next -eq "*") {
                    $state = "block_comment"
                    $i++
                } elseif ($ch -eq "'") {
                    $state = "char"
                } elseif ($ch -eq '"') {
                    $state = "string"
                }
            }
            "line_comment" {
                if ($ch -eq "`n") {
                    $state = "code"
                }
            }
            "block_comment" {
                if ($ch -eq "*" -and $next -eq "/") {
                    $state = "code"
                    $i++
                }
            }
            "char" {
                if ($ch -eq "\") {
                    $i++
                } elseif ($ch -eq "'") {
                    $state = "code"
                }
            }
            "string" {
                if ($ch -eq "\") {
                    if ($i + 1 -lt $length) {
                        [void]$result.Append($Text[$i + 1])
                        $i++
                    }
                } elseif ($ch -eq '"') {
                    $state = "code"
                } else {
                    [void]$result.Append($ch)
                }
            }
        }
    }

    return $result.ToString()
}

$stringText = ""
foreach ($sourcePath in $sourcePaths) {
    $source = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8
    $stringText += Get-CppStringLiteralText $source
}
$symbols = [regex]::Matches($stringText, '[\p{IsCJKUnifiedIdeographs}\u3000-\u303F\uFF00-\uFFEF]') |
    ForEach-Object { $_.Value } |
    Sort-Object -Unique
$symbolText = $symbols -join ''

if (-not $symbolText) {
    throw "No CJK symbols found in font source files"
}

foreach ($size in 14, 16, 20, 28) {
    $output = Join-Path $scriptDir "homecare_font_simsun_$size.c"
    npx --yes lv_font_conv `
        --no-compress `
        --no-prefilter `
        --bpp 4 `
        --size $size `
        --font $fontPath `
        -r 0x20-0x7F `
        --symbols $symbolText `
        --format lvgl `
        -o $output `
        --force-fast-kern-format

    if ($LASTEXITCODE -ne 0) {
        throw "lv_font_conv failed for size $size"
    }
}

Write-Host "Generated HomeCareHub fonts with $($symbols.Count) CJK symbols."
