<#
Install a board skill from this repo into ~/.claude/skills.

  .\scripts\install.ps1 <skill-name>            symlink (default; git pull updates it)
  .\scripts\install.ps1 <skill-name> --copy     copy, so you can edit locally
  .\scripts\install.ps1 --all [--copy]
  .\scripts\install.ps1 --list
  .\scripts\install.ps1 --uninstall <skill-name>

CLAUDE_SKILLS_DIR overrides the destination (e.g. a project's .claude/skills).

Creating symlinks on Windows requires either Developer Mode enabled or the
script to run as Administrator. If that's not available, use --copy.
#>
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Args_
)

$ErrorActionPreference = 'Stop'

$Repo = Split-Path -Parent $PSScriptRoot
$Dest = $env:CLAUDE_SKILLS_DIR
if ([string]::IsNullOrEmpty($Dest)) { $Dest = Join-Path $HOME '.claude\skills' }

$Mode = 'link'
$Action = 'install'
$Names = @()

$HelpText = @"
Install a board skill from this repo into ~/.claude/skills.

  .\scripts\install.ps1 <skill-name>            symlink (default; git pull updates it)
  .\scripts\install.ps1 <skill-name> --copy     copy, so you can edit locally
  .\scripts\install.ps1 --all [--copy]
  .\scripts\install.ps1 --list
  .\scripts\install.ps1 --uninstall <skill-name>

CLAUDE_SKILLS_DIR overrides the destination (e.g. a project's .claude/skills).
"@

foreach ($arg in $Args_) {
    switch ($arg) {
        '--copy'      { $Mode = 'copy' }
        '--link'      { $Mode = 'link' }
        '--all'       { $Action = 'all' }
        '--list'      { $Action = 'list' }
        '--uninstall' { $Action = 'uninstall' }
        { $_ -eq '-h' -or $_ -eq '--help' } {
            Write-Host $HelpText
            exit 0
        }
        default {
            if ($arg -like '-*') {
                Write-Error "unknown option: $arg"
                exit 2
            }
            $Names += $arg
        }
    }
}

function Get-SkillDirs {
    Get-ChildItem -Path (Join-Path $Repo 'skills') -Directory | ForEach-Object {
        Get-ChildItem -Path $_.FullName -Directory
    } | Sort-Object FullName
}

function Find-SkillPath {
    param([string]$Name)
    $d = Get-SkillDirs | Where-Object { $_.Name -eq $Name } | Select-Object -First 1
    if ($d) { $d.FullName } else { $null }
}

if ($Action -eq 'list') {
    Write-Host ("{0,-28} {1,-10} {2}" -f 'SKILL', 'FAMILY', 'INSTALLED')
    foreach ($p in (Get-SkillDirs)) {
        $n = $p.Name
        $f = $p.Parent.Name
        $target = Join-Path $Dest $n
        $status = '-'
        if (Test-Path -LiteralPath $target) {
            $item = Get-Item -LiteralPath $target -Force
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { $status = 'symlink' }
            elseif ($item.PSIsContainer) { $status = 'copy' }
        }
        Write-Host ("{0,-28} {1,-10} {2}" -f $n, $f, $status)
    }
    exit 0
}

if ($Action -eq 'all') {
    $Names = @(Get-SkillDirs | ForEach-Object { $_.Name })
}

if ($Names.Count -eq 0) {
    Write-Error "nothing to do - pass a skill name, --all or --list"
    exit 2
}

New-Item -ItemType Directory -Path $Dest -Force | Out-Null

foreach ($name in $Names) {
    $target = Join-Path $Dest $name

    if ($Action -eq 'uninstall') {
        if (Test-Path -LiteralPath $target) {
            $item = Get-Item -LiteralPath $target -Force
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                $item.Delete()
            } else {
                Remove-Item -LiteralPath $target -Recurse -Force
            }
            Write-Host "removed  $target"
        } else {
            Write-Host "not installed: $name"
        }
        continue
    }

    $src = Find-SkillPath -Name $name
    if ([string]::IsNullOrEmpty($src)) {
        Write-Error "no such skill: $name (try --list)"
        exit 1
    }

    if (Test-Path -LiteralPath $target) {
        $item = Get-Item -LiteralPath $target -Force
        $isLink = [bool]($item.Attributes -band [IO.FileAttributes]::ReparsePoint)
        if ($isLink -and $item.Target -and ($item.Target[0] -eq $src)) {
            Write-Host "ok       $name (already linked)"
            continue
        }
        $reply = Read-Host "$target exists - replace? [y/N]"
        if ($reply -match '^[Yy]$') {
            if ($isLink) { $item.Delete() } else { Remove-Item -LiteralPath $target -Recurse -Force }
        } else {
            Write-Host "skipped  $name"
            continue
        }
    }

    if ($Mode -eq 'link') {
        try {
            New-Item -ItemType SymbolicLink -Path $target -Target $src -Force | Out-Null
            Write-Host "linked   $target -> $src"
        } catch {
            Write-Host "failed to create symlink for $name : $($_.Exception.Message)"
            Write-Host "Tip: enable Windows Developer Mode, run PowerShell as Administrator, or use --copy instead."
            exit 1
        }
    } else {
        Copy-Item -Path $src -Destination $target -Recurse -Force
        Write-Host "copied   $target"
    }
}
