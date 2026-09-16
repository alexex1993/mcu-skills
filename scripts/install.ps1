<#
Install a board skill from this repo into ~/.claude/skills.

  .\scripts\install.ps1 <skill-name>            link (default; git pull updates it)
  .\scripts\install.ps1 <skill-name> --copy     copy, so you can edit locally
  .\scripts\install.ps1 --all [--copy]
  .\scripts\install.ps1 --list
  .\scripts\install.ps1 --uninstall <skill-name>

CLAUDE_SKILLS_DIR overrides the destination (e.g. a project's .claude/skills).

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
$Dest = $env:CLAUDE_SKILLS_DIR
if ([string]::IsNullOrEmpty($Dest)) { $Dest = Join-Path $HOME '.claude\skills' }

$Mode = 'link'
$Action = 'install'
$Names = @()

$HelpText = @"
Install a board skill from this repo into ~/.claude/skills.

  .\scripts\install.ps1 <skill-name>            link (default; git pull updates it)
  .\scripts\install.ps1 <skill-name> --copy     copy, so you can edit locally
  .\scripts\install.ps1 --all [--copy]
  .\scripts\install.ps1 --list
  .\scripts\install.ps1 --uninstall <skill-name>

CLAUDE_SKILLS_DIR overrides the destination (e.g. a project's .claude/skills).
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

if ($Action -eq 'list') {
    Write-Host ("{0,-28} {1,-10} {2}" -f 'SKILL', 'FAMILY', 'INSTALLED')
    foreach ($p in (Get-SkillDirs)) {
        $n = $p.Name
        $f = $p.Parent.Name
        $target = Join-Path $Dest $n
        $status = '-'
        if (Test-Path -LiteralPath $target) {
            $item = Get-Item -LiteralPath $target -Force
            if (Test-IsLink $item) { $status = 'link' }
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
    Exit-WithError "nothing to do - pass a skill name, --all or --list" 2
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
