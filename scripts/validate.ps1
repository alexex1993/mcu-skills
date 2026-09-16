<#
Structural checks for board skills. Not a substitute for testing on hardware -
see CONTRIBUTING.md section 5.

  .\scripts\validate.ps1 <skill-name>
  .\scripts\validate.ps1 --all
#>
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Args_
)

$Repo = Split-Path -Parent $PSScriptRoot
$Script:FailCount = 0
$Script:WarnCount = 0

function Write-Err  ($msg) { Write-Host "  FAIL  $msg"; $Script:FailCount++ }
function Write-Warn2($msg) { Write-Host "  warn  $msg"; $Script:WarnCount++ }
function Write-Ok   ($msg) { Write-Host "  ok    $msg" }

function Test-Skill {
    param([string]$Dir)

    $name = Split-Path -Leaf $Dir
    Write-Host "$name  ($Dir)"

    $skillFile = Join-Path $Dir 'SKILL.md'
    if (-not (Test-Path -LiteralPath $skillFile -PathType Leaf)) {
        Write-Err "no SKILL.md"
        Write-Host ""
        return
    }

    $lines = @(Get-Content -LiteralPath $skillFile -Encoding UTF8)

    # --- frontmatter -----------------------------------------------------------
    if ($lines.Count -eq 0 -or $lines[0].Trim() -ne '---') {
        Write-Err "SKILL.md does not start with a '---' frontmatter block"
        Write-Host ""
        return
    }

    $fmEndIdx = -1
    for ($i = 1; $i -lt $lines.Count; $i++) {
        if ($lines[$i].Trim() -eq '---') { $fmEndIdx = $i; break }
    }
    if ($fmEndIdx -eq -1) {
        Write-Err "frontmatter is not closed with '---'"
        Write-Host ""
        return
    }
    $fm = if ($fmEndIdx -gt 1) { $lines[1..($fmEndIdx - 1)] } else { @() }

    $fmName = ''
    foreach ($l in $fm) {
        if ($l -match '^name:\s*(.*)$') { $fmName = $Matches[1].Trim().Trim('"').Trim("'"); break }
    }
    if ([string]::IsNullOrEmpty($fmName)) {
        Write-Err "frontmatter has no 'name:'"
    } elseif ($fmName -ne $name) {
        Write-Err "name: '$fmName' != directory '$name'"
    } elseif ($fmName -cnotmatch '^[a-z0-9]+(-[a-z0-9]+)*$') {
        Write-Err "name '$fmName' must be lowercase letters, digits and single hyphens"
    } else {
        Write-Ok "name: $fmName"
    }

    # description (may span multiple lines until the next top-level key)
    $descLines = @()
    $inDesc = $false
    foreach ($l in $fm) {
        if ($l -match '^description:\s*(.*)$') {
            $inDesc = $true
            $descLines += $Matches[1]
            continue
        }
        if ($inDesc) {
            if ($l -match '^[a-z-]+:') { $inDesc = $false }
            else { $descLines += $l }
        }
    }
    $desc = ($descLines -join "`n").Trim()
    $dlen = $desc.Length
    if ($dlen -eq 0) {
        Write-Err "frontmatter has no 'description:'"
    } elseif ($dlen -lt 120) {
        Write-Err "description is $dlen chars - too short to trigger reliably (aim for 300+)"
    } elseif ($dlen -lt 250) {
        Write-Warn2 "description is $dlen chars - consider naming more peripherals and task shapes"
    } elseif ($dlen -gt 1024) {
        Write-Err "description is $dlen chars - over the 1024 limit"
    } else {
        Write-Ok "description: $dlen chars"
    }
    if ($desc -notmatch '(?i)\buse when\b|\bwhen (working|the user|you)') {
        Write-Warn2 "description has no 'Use when ...' clause - it says what, not when"
    }

    if ($fmName -match '^REPLACE' -or $fmName -match '<') {
        Write-Err "frontmatter still contains skeleton placeholders"
    }

    # --- body ------------------------------------------------------------------
    $lineCount = $lines.Count
    if ($lineCount -gt 700) {
        Write-Err "SKILL.md is $lineCount lines - move detail into reference/"
    } elseif ($lineCount -gt 500) {
        Write-Warn2 "SKILL.md is $lineCount lines - aim for under 500"
    } else {
        Write-Ok "SKILL.md: $lineCount lines"
    }

    $bodyText = $lines -join "`n"
    if ($bodyText -notmatch '(?im)^#+\s.*(flash|upload|program|burn)') {
        Write-Warn2 "no flashing section in SKILL.md"
    }
    if ($bodyText -notmatch '(?i)(rule|pitfall|gotcha|mistake|trap)') {
        Write-Warn2 "no rules/pitfalls section - that is where most of a skill's value lives"
    }

    # --- directories -----------------------------------------------------------
    $refDir = Join-Path $Dir 'reference'
    $refFiles = @()
    if (Test-Path -LiteralPath $refDir -PathType Container) {
        $refFiles = @(Get-ChildItem -LiteralPath $refDir -Filter '*.md' -File -Force -ErrorAction SilentlyContinue)
        Write-Ok "reference/ ($($refFiles.Count) files)"
    } else {
        Write-Warn2 "no reference/ directory"
    }

    $tmplDir = Join-Path $Dir 'template'
    if (Test-Path -LiteralPath $tmplDir -PathType Container) {
        Write-Ok "template/"
        $sc = Join-Path $tmplDir 'variants\new-project.sh'
        if (-not (Test-Path -LiteralPath $sc -PathType Leaf)) {
            Write-Warn2 "no template/variants/new-project.sh scaffold script"
        }
        # NOTE: Windows has no executable bit, so the chmod +x check from
        # validate.sh has no equivalent here - only presence is checked.
        $readme = Join-Path $tmplDir 'README.md'
        if (-not (Test-Path -LiteralPath $readme -PathType Leaf)) {
            Write-Warn2 "no template/README.md mapping files to subsystems"
        }
    } else {
        Write-Warn2 "no template/ directory - a skill with no buildable project is much weaker"
    }

    foreach ($f in $refFiles) {
        if ($bodyText -notmatch [Regex]::Escape($f.Name)) {
            Write-Warn2 "reference/$($f.Name) is never referenced from SKILL.md"
        }
    }

    # --- hygiene -----------------------------------------------------------------
    $allFiles = @(Get-ChildItem -LiteralPath $Dir -Recurse -File -Force -ErrorAction SilentlyContinue)
    $absHits = @()
    foreach ($f in $allFiles) {
        if ($absHits.Count -ge 5) { break }
        $content = Get-Content -LiteralPath $f.FullName -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
        if ($content -match '/Users/[a-zA-Z0-9_.\-]+|/home/[a-zA-Z0-9_.\-]+|[A-Z]:\\') {
            $absHits += $f.FullName
        }
    }
    if ($absHits.Count -gt 0) {
        Write-Err "machine-specific absolute paths in: $($absHits -join ' ')"
    }

    $junkFileNames = @('.DS_Store', '*.elf', '*.bin', '*.o')
    $junkDirNames = @('.pio', 'build', '.vscode', 'node_modules')
    $junk = @()
    $junk += Get-ChildItem -Path (Join-Path $Dir '*') -Recurse -File -Force -Include $junkFileNames -ErrorAction SilentlyContinue
    $junk += Get-ChildItem -LiteralPath $Dir -Recurse -Directory -Force -ErrorAction SilentlyContinue |
        Where-Object { $junkDirNames -contains $_.Name }
    $junk = $junk | Select-Object -First 5
    if ($junk.Count -gt 0) {
        Write-Err "build output or IDE files committed: $(($junk | ForEach-Object { $_.FullName }) -join ' ')"
    }

    Write-Host ""
}

function Get-SkillDirs {
    Get-ChildItem -Path (Join-Path $Repo 'skills') -Directory | ForEach-Object {
        Get-ChildItem -Path $_.FullName -Directory
    } | Sort-Object FullName
}

$targets = $Args_
if (-not $targets -or $targets.Count -eq 0) { $targets = @('--all') }

if ($targets[0] -eq '--all') {
    foreach ($d in (Get-SkillDirs)) { Test-Skill -Dir $d.FullName }
} else {
    foreach ($n in $targets) {
        $d = Get-SkillDirs | Where-Object { $_.Name -eq $n } | Select-Object -First 1
        if (-not $d) {
            Write-Host "no such skill: $n"
            $Script:FailCount++
            continue
        }
        Test-Skill -Dir $d.FullName
    }
}

Write-Host "$Script:FailCount failed, $Script:WarnCount warnings"
if ($Script:FailCount -eq 0) { exit 0 } else { exit 1 }
