<#
Install board skills for Claude Code (default) or Codex (--codex).

  .\scripts\install.ps1 <skill-name>            link (default; git pull updates it)
  .\scripts\install.ps1 <skill-name> --copy     copy, so you can edit locally
  .\scripts\install.ps1 --all [--copy]
  .\scripts\install.ps1 --list                show Claude and Codex installations
  .\scripts\install.ps1 --uninstall <skill-name>
  .\scripts\install.ps1 --codex --all          install all skills for Codex

Claude: ~/.claude/skills, overridden by CLAUDE_SKILLS_DIR.
Codex:  ~/.agents/skills, overridden by CODEX_SKILLS_DIR.
Add --yes to replace existing installations without prompting.

"link" is a directory junction, not a symlink: junctions need neither
Administrator nor Developer Mode, while New-Item -ItemType SymbolicLink in
Windows PowerShell 5.1 requires Administrator even with Developer Mode on.

If script execution is disabled on this machine, run it as
  powershell -ExecutionPolicy Bypass -File .\scripts\install.ps1 <args>
#>
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Args_
)

$ErrorActionPreference = 'Stop'

$Repo = Split-Path -Parent $PSScriptRoot
$Agent = 'claude'
$Replace = $false

$Mode = 'link'
$Action = 'install'
$Names = @()

$HelpText = @"
Install board skills for Claude Code (default) or Codex (--codex).

  .\scripts\install.ps1 <skill-name>            link (default; git pull updates it)
  .\scripts\install.ps1 <skill-name> --copy     copy, so you can edit locally
  .\scripts\install.ps1 --all [--copy]
  .\scripts\install.ps1 --list                show Claude and Codex installations
  .\scripts\install.ps1 --uninstall <skill-name>
  .\scripts\install.ps1 --codex --all          install all skills for Codex

Claude: ~/.claude/skills, overridden by CLAUDE_SKILLS_DIR.
Codex:  ~/.agents/skills, overridden by CODEX_SKILLS_DIR.
Add --yes to replace existing installations without prompting.
"@

# Write-Error under ErrorActionPreference=Stop throws, so the exit code would
# always be 1 and the user gets a stack trace; print to stderr and exit instead.
function Exit-WithError {
    param([string]$Message, [int]$Code = 1)
    [Console]::Error.WriteLine($Message)
    exit $Code
}

foreach ($arg in $Args_) {
    switch ($arg) {
        '--codex'     { $Agent = 'codex' }
        '--claude'    { $Agent = 'claude' }
        '--yes'       { $Replace = $true }
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
            if ($arg -like '-*') { Exit-WithError "unknown option: $arg" 2 }
            $Names += $arg
        }
    }
}

$ClaudeDest = $env:CLAUDE_SKILLS_DIR
if ([string]::IsNullOrEmpty($ClaudeDest)) { $ClaudeDest = Join-Path $HOME '.claude/skills' }
$CodexDest = $env:CODEX_SKILLS_DIR
if ([string]::IsNullOrEmpty($CodexDest)) { $CodexDest = Join-Path $HOME '.agents/skills' }
if ($Agent -eq 'codex') { $Dest = $CodexDest } else { $Dest = $ClaudeDest }

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

function Test-IsLink {
    param($Item)
    [bool]($Item.Attributes -band [IO.FileAttributes]::ReparsePoint)
}

# Item.Target is string[] in Windows PowerShell 5.1 and a plain string in
# PowerShell 7, where indexing it would return the first character.
function Get-LinkTarget {
    param($Item)
    $t = @($Item.Target)
    if ($t.Count -eq 0 -or [string]::IsNullOrEmpty($t[0])) { return $null }
    ([string]$t[0]).TrimEnd('\')
}

function Get-InstallStatus {
    param($Item)
    if ($null -eq $Item) { return '-' }
    if (Test-IsLink $Item) {
        if (Test-Path -LiteralPath $Item.FullName) { return 'link' }
        return 'broken link'
    }
    if ($Item.PSIsContainer) { return 'copy' }
    return 'file'
}

function Show-InstallPath {
    param([string]$Label, $Item)
    if ($null -eq $Item) { return }
    if (Test-IsLink $Item) {
        Write-Host ("  {0}: {1} -> {2}" -f $Label, $Item.FullName, (Get-LinkTarget $Item))
    } else {
        Write-Host ("  {0}: {1}" -f $Label, $Item.FullName)
    }
}

if ($Action -eq 'list') {
    Write-Host "Claude: $ClaudeDest"
    Write-Host "Codex:  $CodexDest"
    Write-Host ''
    Write-Host ("{0,-28} {1,-10} {2,-12} {3}" -f 'SKILL', 'FAMILY', 'CLAUDE', 'CODEX')
    foreach ($p in (Get-SkillDirs)) {
        $n = $p.Name
        $f = $p.Parent.Name
        $claudeItem = Get-Item -LiteralPath (Join-Path $ClaudeDest $n) -Force -ErrorAction SilentlyContinue
        $codexItem = Get-Item -LiteralPath (Join-Path $CodexDest $n) -Force -ErrorAction SilentlyContinue
        Write-Host ("{0,-28} {1,-10} {2,-12} {3}" -f $n, $f,
            (Get-InstallStatus $claudeItem), (Get-InstallStatus $codexItem))
        Show-InstallPath 'Claude' $claudeItem
        Show-InstallPath 'Codex' $codexItem
    }
    exit 0
}

if ($Action -eq 'all') {
    $Names = @(Get-SkillDirs | ForEach-Object { $_.Name })
}

if ($Names.Count -eq 0) {
    Exit-WithError "nothing to do - pass a skill name, --all or --list" 2
}

# Validate the entire request before changing any installation.
foreach ($name in $Names) {
    if ($name -cnotmatch '^[a-z0-9]+(-[a-z0-9]+)*$') {
        Exit-WithError "invalid skill name: $name" 2
    }
    if ($Action -ne 'uninstall') {
        $src = Find-SkillPath -Name $name
        if ([string]::IsNullOrEmpty($src) -or !(Test-Path -LiteralPath (Join-Path $src 'SKILL.md') -PathType Leaf)) {
            Exit-WithError "no such skill: $name (try --list)"
        }
    }
}

New-Item -ItemType Directory -Path $Dest -Force | Out-Null

foreach ($name in $Names) {
    $target = Join-Path $Dest $name

    if ($Action -eq 'uninstall') {
        if (Test-Path -LiteralPath $target) {
            $item = Get-Item -LiteralPath $target -Force
            # Remove-Item -Recurse on a link in PS 5.1 deletes the files it
            # points to; Delete() removes only the link itself.
            if (Test-IsLink $item) {
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
        Exit-WithError "no such skill: $name (try --list)"
    }

    if (Test-Path -LiteralPath $target) {
        $item = Get-Item -LiteralPath $target -Force
        $isLink = Test-IsLink $item
        if ($isLink -and $Mode -eq 'link' -and (Get-LinkTarget $item) -eq $src.TrimEnd('\')) {
            Write-Host "ok       $name (already linked)"
            continue
        }
        $reply = 'y'
        if (!$Replace) { $reply = Read-Host "$target exists - replace? [y/N]" }
        if ($reply -match '^[Yy]$') {
            if ($isLink) { $item.Delete() } else { Remove-Item -LiteralPath $target -Recurse -Force }
        } else {
            Write-Host "skipped  $name"
            continue
        }
    }

    if ($Mode -eq 'link') {
        try {
            New-Item -ItemType Junction -Path $target -Target $src | Out-Null
            Write-Host "linked   $target -> $src"
        } catch {
            Exit-WithError ("failed to link $name : $($_.Exception.Message)`n" +
                "Tip: junctions only work between local drives - use --copy instead.")
        }
    } else {
        Copy-Item -LiteralPath $src -Destination $target -Recurse -Force
        Write-Host "copied   $target"
    }
}
