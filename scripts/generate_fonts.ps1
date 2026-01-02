#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Generates all font variants for the microreader project
.DESCRIPTION
    This script generates NotoSans and Bookerly fonts in three sizes (26, 28, 30)
    with all style variants (Regular, Bold, Italic, BoldItalic), plus UI fonts (Font14, Font27, Font28).
#>

param(
    [string]$CharsFile = "resources/chars_input.txt",
    [switch]$Clean,
    [switch]$Help
)

if ($Help) {
    Get-Help $PSCommandPath -Detailed
    exit 0
}

# Configuration
$PythonCmd = "C:/Users/Patrick/Desktop/microreader/.venv/Scripts/python.exe"
$Sizes = @(26, 28, 30)
$UISizes = @(14, 32, 20)
$RepoRoot = Split-Path -Parent $PSScriptRoot

# Font definitions
$Fonts = @(
    @{
        Family = "NotoSans"
        Variants = @(
            @{ Style = ""; File = "NotoSans.ttf"; Variation = $null }
            @{ Style = "Bold"; File = "NotoSans.ttf"; Variation = "wght=700" }
            @{ Style = "Italic"; File = "NotoSans-Italic.ttf"; Variation = $null }
            @{ Style = "BoldItalic"; File = "NotoSans-Italic.ttf"; Variation = "wght=700" }
        )
    }
    @{
        Family = "Bookerly"
        Variants = @(
            @{ Style = ""; File = "Bookerly.ttf"; Variation = $null }
            @{ Style = "Bold"; File = "Bookerly Bold.ttf"; Variation = $null }
            @{ Style = "Italic"; File = "Bookerly Italic.ttf"; Variation = $null }
            @{ Style = "BoldItalic"; File = "Bookerly Bold Italic.ttf"; Variation = $null }
        )
    }
)

# Change to repo root
Push-Location $RepoRoot

try {
    # Clean old fonts if requested
    if ($Clean) {
        Write-Host "Cleaning old font files..." -ForegroundColor Yellow
        $FoldersToClean = @("src/resources/fonts/notosans", "src/resources/fonts/bookerly", "src/resources/fonts/other")
        foreach ($folder in $FoldersToClean) {
            if (Test-Path $folder) {
                Get-ChildItem -Path $folder -Filter "*.h" | Remove-Item -Force
                Get-ChildItem -Path $folder -Filter "*.png" | Remove-Item -Force
                Write-Host "  Cleaned $folder" -ForegroundColor Gray
            }
        }
    }

    # Verify Python and chars file exist
    if (-not (Test-Path $PythonCmd)) {
        Write-Error "Python executable not found: $PythonCmd"
        Write-Host "Run from repo root: python -m venv .venv" -ForegroundColor Yellow
        Write-Host "Then: .venv\Scripts\activate" -ForegroundColor Yellow
        Write-Host "Then: pip install -r scripts/generate_simplefont/requirements.txt" -ForegroundColor Yellow
        exit 1
    }

    if (-not (Test-Path $CharsFile)) {
        Write-Error "Characters file not found: $CharsFile"
        exit 1
    }

    # Generate fonts
    $TotalFonts = ($Fonts.Count * $Sizes.Count * 4) + $UISizes.Count  # 4 variants per family/size + UI fonts
    $CurrentFont = 0

    Write-Host ""
    Write-Host "Generating $TotalFonts font files..." -ForegroundColor Cyan
    Write-Host ""

    # Generate UI fonts first
    foreach ($size in $UISizes) {
        $CurrentFont++
        # Use descriptive names: MenuFontSmall, MenuHeader, MenuFontBig
        if ($size -eq 14) {
            $name = "MenuFontSmall"
            $ttfPath = "resources/fonts/Terminus-Bold.ttf"
        } elseif ($size -eq 32) {
            $name = "MenuHeader"
            $ttfPath = "resources/fonts/Terminus-Bold.ttf"
        } else {
            $name = "MenuFontBig"
            $ttfPath = "resources/fonts/Terminus-Bold.ttf"
        }
        $outPath = "src/resources/fonts/other/$name.h"
        
        $progressPercent = [int](($CurrentFont / $TotalFonts) * 100)
        Write-Progress -Activity "Generating Fonts" -Status "$name ($CurrentFont/$TotalFonts)" -PercentComplete $progressPercent
        
        Write-Host "[$CurrentFont/$TotalFonts] Generating $name..." -ForegroundColor Green
        
        # Build command arguments (UI fonts use B/W only)
        $args = @(
            "-m", "scripts.generate_simplefont.cli",
            "--name", $name,
            "--size", $size,
            "--chars-file", $CharsFile,
            "--ttf", $ttfPath,
            "--out", $outPath,
            "--no-grayscale"
        )
        
        # Run generation
        $output = & $PythonCmd @args 2>&1
        
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Failed to generate $name"
            Write-Host $output -ForegroundColor Red
            exit 1
        }
        
        # Show warnings if any
        $warnings = $output | Where-Object { $_ -match "WARNING:" }
        if ($warnings) {
            foreach ($warning in $warnings) {
                Write-Host "  $warning" -ForegroundColor Yellow
            }
        }
    }

    # Generate reading fonts
    foreach ($fontDef in $Fonts) {
        $familyLower = $fontDef.Family.ToLower()
        
        foreach ($size in $Sizes) {
            foreach ($variant in $fontDef.Variants) {
                $CurrentFont++
                $name = "$($fontDef.Family)$size$($variant.Style)"
                $ttfPath = "resources/fonts/$($variant.File)"
                $outPath = "src/resources/fonts/$familyLower/$name.h"
                
                $progressPercent = [int](($CurrentFont / $TotalFonts) * 100)
                Write-Progress -Activity "Generating Fonts" -Status "$name ($CurrentFont/$TotalFonts)" -PercentComplete $progressPercent
                
                Write-Host "[$CurrentFont/$TotalFonts] Generating $name..." -ForegroundColor Green
                
                # Build command arguments (reading fonts use grayscale)
                $args = @(
                    "-m", "scripts.generate_simplefont.cli",
                    "--name", $name,
                    "--size", $size,
                    "--chars-file", $CharsFile,
                    "--ttf", $ttfPath,
                    "--out", $outPath
                )
                
                if ($variant.Variation) {
                    $args += "--var"
                    $args += $variant.Variation
                }
                
                # Run generation
                $output = & $PythonCmd @args 2>&1
                
                if ($LASTEXITCODE -ne 0) {
                    Write-Error "Failed to generate $name"
                    Write-Host $output -ForegroundColor Red
                    exit 1
                }
                
                # Show warnings if any
                $warnings = $output | Where-Object { $_ -match "WARNING:" }
                if ($warnings) {
                    foreach ($warning in $warnings) {
                        Write-Host "  $warning" -ForegroundColor Yellow
                    }
                }
            }
        }
    }

    Write-Progress -Activity "Generating Fonts" -Completed
    Write-Host ""
    Write-Host "Successfully generated $TotalFonts font files!" -ForegroundColor Green
    Write-Host ""

} finally {
    Pop-Location
}
