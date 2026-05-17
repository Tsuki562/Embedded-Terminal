$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$sourcePath = Join-Path $root "components/apps/homecare_hub/HomeCareHub.cpp"
$fontDir = Join-Path $root "components/apps/homecare_hub/fonts"

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

$source = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8
$stringText = Get-CppStringLiteralText $source
$needed = [regex]::Matches($stringText, '[\p{IsCJKUnifiedIdeographs}\u3000-\u303F\uFF00-\uFFEF]') |
    ForEach-Object { $_.Value } |
    Sort-Object -Unique

$fontFiles = @(
    "homecare_font_simsun_14.c",
    "homecare_font_simsun_16.c",
    "homecare_font_simsun_20.c",
    "homecare_font_simsun_28.c"
)

$failed = $false
foreach ($fontFile in $fontFiles) {
    $fontPath = Join-Path $fontDir $fontFile
    $header = Get-Content -LiteralPath $fontPath -TotalCount 8 -Encoding UTF8 | Out-String
    $match = [regex]::Match($header, '--symbols (?<symbols>.*?) --format')
    if (-not $match.Success) {
        Write-Error "Could not find --symbols in $fontFile"
    }

    $symbols = $match.Groups["symbols"].Value
    $missing = $needed | Where-Object { $symbols.IndexOf($_) -lt 0 }
    if ($missing) {
        $failed = $true
        Write-Host "$fontFile missing: $($missing -join '')"
    }
}

if ($failed) {
    exit 1
}

Write-Host "All HomeCareHub CJK glyphs are present in generated font symbols."
