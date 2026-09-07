# Prepare an install: game data, expansions, soundtrack, menu artwork and the
# episodes' message text.
#
# A port of setup.py that needs nothing installed. PowerShell ships with Windows
# and System.Drawing comes with it, so a downloaded release runs this and plays -
# no Python, no Pillow, no download. The Python tools are kept for building
# releases here, and the two are verified against each other: every generated
# file compared byte for byte, including the menu artwork and qc_strings.txt.
#
# Everything it produces is built from the game data on this machine. Nothing
# belonging to id Software or MachineGames is carried in the repository, and
# nothing produced here may be redistributed.
#
#   powershell -ExecutionPolicy Bypass -File tools\setup.ps1 [install dir] [quake dir]

param(
    [string]$InstallDir = ".",
    [string]$QuakeDir = ""
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. ([System.IO.Path]::Combine($here, 'common.ps1'))
. ([System.IO.Path]::Combine($here, 'menu-art.ps1'))
. ([System.IO.Path]::Combine($here, 'qc-strings.ps1'))

# Kept as a last resort for a hand-copied install that no installer knows about.
$QuakeGuesses = @(
    "C:\Program Files (x86)\Steam\steamapps\common\Quake",
    "C:\Program Files\Steam\steamapps\common\Quake",
    "D:\SteamLibrary\steamapps\common\Quake",
    "C:\GOG Games\Quake",
    "C:\Program Files (x86)\GOG Galaxy\Games\Quake"
)

# gamedir, where the pak comes from, launcher name, engine switch
$Expansions = @(
    @{ Dir = 'hipnotic'; Rel = 'hipnotic\pak0.pak';       Label = 'Scourge of Armagon';       Args = '-hipnotic' },
    @{ Dir = 'rogue';    Rel = 'rogue\pak0.pak';          Label = 'Dissolution of Eternity';  Args = '-rogue' },
    @{ Dir = 'dopa';     Rel = 'rerelease\dopa\pak0.pak'; Label = 'Dimension of the Past';    Args = '-game dopa' },
    @{ Dir = 'mg1';      Rel = 'rerelease\mg1\pak0.pak';  Label = 'Dimension of the Machine'; Args = '-game mg1' },
    @{ Dir = 'mg3';      Rel = 'rerelease\mg3\pak0.pak';  Label = 'Dawn of the Machine';      Args = '-game mg3' }
)

function Write-Launcher([string]$path, [string]$extraArgs) {
    Write-TextLines $path @(
        '@echo off',
        'rem Start Virtual Desktop on the headset and connect it FIRST.',
        'cd /d "%~dp0"',
        ('start "" "%~dp0darkplaces-sdl.exe" -basedir . -nohome ' + $extraArgs + ' %*')
    ) "`r`n"
}

# --- finding Quake ----------------------------------------------------------

function Get-QuakeScore([string]$path) {
    # How complete an install is, so the best one wins rather than the first
    # found. The pieces are not always together: the classic paks, the
    # re-release episodes and the soundtrack can each be somewhere different.
    $p0 = Find-FileAnyCase (PathJoin $path 'id1') 'pak0.pak'
    $p0r = Find-FileAnyCase (PathJoin $path 'rerelease\id1') 'pak0.pak'
    if (-not $p0 -and -not $p0r) {
        $script:LastScoreWhy = 'no id1\pak0.pak'
        return -1
    }

    $score = 0
    $why = New-Object System.Collections.ArrayList
    if ($p0) { $score += 4; [void]$why.Add('id1') }
    if ($p0r) { $score += 2; [void]$why.Add('re-release id1') }
    if (Find-FileAnyCase (PathJoin $path 'id1') 'pak1.pak') { $score += 4; [void]$why.Add('registered') }
    if (Test-Path -PathType Leaf (PathJoin $path 'hipnotic\pak0.pak')) { $score += 2; [void]$why.Add('hipnotic') }
    if (Test-Path -PathType Leaf (PathJoin $path 'rogue\pak0.pak')) { $score += 2; [void]$why.Add('rogue') }
    if (Test-Path -PathType Leaf (PathJoin $path 'rerelease\mg1\pak0.pak')) { $score += 2; [void]$why.Add('machine') }
    if (Test-Path -PathType Container (PathJoin $path 'rerelease\id1\music')) { $score += 2; [void]$why.Add('soundtrack') }
    if (Test-Path -PathType Leaf (PathJoin $path 'rerelease\QuakeEX.kpf')) { $score += 1; [void]$why.Add('menu font') }

    $script:LastScoreWhy = ($why -join ', ')
    return $score
}

function Find-Quake([string]$given) {
    $script:QuakeSeen = New-Object System.Collections.ArrayList
    $script:QuakeRoots = New-Object System.Collections.ArrayList

    if ($given) {
        if (Test-Path -PathType Container $given) {
            [void]$script:QuakeRoots.Add($given)
            return $given
        }
        return $null
    }
    if ($env:QQ_QUAKEDIR -and (Test-Path -PathType Container $env:QQ_QUAKEDIR)) {
        [void]$script:QuakeRoots.Add($env:QQ_QUAKEDIR)
        return $env:QQ_QUAKEDIR
    }

    $cands = New-Object System.Collections.ArrayList
    foreach ($lib in (Get-SteamLibraries)) {
        $common = PathJoin $lib 'steamapps\common'
        if (-not (Test-Path -PathType Container $common)) { continue }
        try { $dirs = Get-ChildItem -LiteralPath $common -Directory -ErrorAction SilentlyContinue }
        catch { continue }
        foreach ($d in $dirs) { [void]$cands.Add($d.FullName) }
    }
    foreach ($g in (Get-GogGameDirs)) { [void]$cands.Add($g) }
    foreach ($p in $QuakeGuesses) { [void]$cands.Add($p) }

    $best = $null
    $bestScore = -1
    $seen = @{}
    foreach ($c in $cands) {
        $key = $c.ToLower().TrimEnd('\')
        if ($seen.ContainsKey($key)) { continue }
        $seen[$key] = $true
        $sc = Get-QuakeScore $c
        if ($sc -ge 0) {
            [void]$script:QuakeSeen.Add([pscustomobject]@{ Path = $c; Score = $sc; Why = $script:LastScoreWhy })
            [void]$script:QuakeRoots.Add($c)
            if ($sc -gt $bestScore) { $best = $c; $bestScore = $sc }
        } elseif ((Test-Path -PathType Container (PathJoin $c 'rerelease\id1\music')) -or
                  (Test-Path -PathType Leaf (PathJoin $c 'rerelease\QuakeEX.kpf'))) {
            # No base game, but it still holds something wanted.
            [void]$script:QuakeRoots.Add($c)
        }
    }
    return $best
}

function Find-Piece([string]$relative, [switch]$Directory) {
    # Each piece from wherever it actually is. On a machine with more than one
    # Quake install - the classic and the re-release, say - taking everything
    # from one folder loses whatever the other one had.
    foreach ($r in $script:QuakeRoots) {
        $p = PathJoin $r $relative
        if ($Directory) {
            if ((Test-Path -PathType Container $p) -and
                @(Get-ChildItem -LiteralPath $p -File -ErrorAction SilentlyContinue).Count -gt 0) {
                return $p
            }
        } elseif (Test-Path -PathType Leaf $p) {
            return $p
        }
    }
    return $null
}

# --- the install ------------------------------------------------------------

$dest = (Resolve-Path $InstallDir).Path
$quake = Find-Quake $QuakeDir

if ($quake) {
    Write-Host ("Quake found at " + $quake)
    if ($script:QuakeSeen.Count -gt 1) {
        Write-Host "  other Quake data found:"
        foreach ($c in ($script:QuakeSeen | Sort-Object -Property Score -Descending)) {
            if ($c.Path -eq $quake) { continue }
            Write-Host ("    {0,4}  {1}  ({2})" -f $c.Score, $c.Path, $c.Why)
        }
    }

    Write-Host "Game data..."
    $id1 = PathJoin $dest 'id1'
    [void](New-Item -ItemType Directory -Force $id1)
    $copied = 0
    foreach ($pak in @('pak0.pak', 'pak1.pak')) {
        if (Test-Path -PathType Leaf (PathJoin $id1 $pak)) { continue }
        # The classic paks are preferred: they are what DarkPlaces was written
        # against, and pak1 is the registered content.
        $src = $null
        foreach ($r in $script:QuakeRoots) {
            $src = Find-FileAnyCase (PathJoin $r 'id1') $pak
            if ($src) { break }
            $src = Find-FileAnyCase (PathJoin $r 'rerelease\id1') $pak
            if ($src) { break }
        }
        if ($src) {
            Write-Host ("  " + $pak + "...")
            Copy-Item -LiteralPath $src -Destination (PathJoin $id1 $pak) -Force
            $copied++
        }
    }
    if ($copied -eq 0) { Write-Host "  already here" }
}

if (-not (Test-Path -PathType Leaf (PathJoin $dest 'id1\pak0.pak'))) {
    Write-Host ""
    Write-Host ("No Quake data found, and none in " + (PathJoin $dest 'id1'))
    Write-Host ""
    $libs = @(Get-SteamLibraries)
    if ($libs.Count -gt 0) {
        Write-Host "Steam libraries searched:"
        foreach ($l in $libs) { Write-Host ("  " + $l) }
    }
    Write-Host ""
    Write-Host "Either install Quake where this can find it, or copy pak0.pak and"
    Write-Host "pak1.pak from your own copy into the id1 folder and run this again."
    Write-Host "If it is somewhere unusual:"
    Write-Host ""
    Write-Host "  set QQ_QUAKEDIR=D:\Games\Quake"
    Write-Host "  Setup.bat"
    exit 1
}

if ($quake) {
    Write-Host "Expansions..."
    $added = 0
    foreach ($x in $Expansions) {
        $src = Find-Piece $x.Rel
        if (-not $src) { continue }
        # Quake and Quake II both call a mission pack folder 'rogue', so check
        # this really is Quake data before copying it in.
        if (-not (Test-QuakeOnePak $src)) {
            Write-Host ("  ignoring " + $src + " - not Quake data")
            continue
        }
        $target = PathJoin $dest $x.Dir
        [void](New-Item -ItemType Directory -Force $target)
        $pak = PathJoin $target 'pak0.pak'
        if (-not (Test-Path -PathType Leaf $pak)) {
            Write-Host ("  " + $x.Label + "...")
            Copy-Item -LiteralPath $src -Destination $pak -Force
        }
        Write-Launcher (PathJoin $dest ($x.Label + '.bat')) $x.Args
        $added++
    }
    if ($added -eq 0) { Write-Host "  none found" }

    Write-Host "Soundtrack..."
    # The re-release carries it as ogg. DarkPlaces plays it from
    # sound/cdtracks, which is where their Quest build expects it too.
    $music = Find-Piece 'rerelease\id1\music' -Directory
    $tracks = 0
    if ($music) {
        $target = PathJoin $dest 'id1\sound\cdtracks'
        [void](New-Item -ItemType Directory -Force $target)
        foreach ($f in (Get-ChildItem -LiteralPath $music -File | Sort-Object Name)) {
            if ($f.Extension.ToLower() -notin @('.ogg', '.mp3', '.wav', '.flac')) { continue }
            $out = PathJoin $target $f.Name.ToLower()
            if (Test-Path -PathType Leaf $out) { continue }
            Copy-Item -LiteralPath $f.FullName -Destination $out -Force
            $tracks++
        }
    }
    if ($tracks -gt 0) { Write-Host ("  " + $tracks + " tracks") }
    else { Write-Host "  already here, or none to copy" }
} else {
    Write-Host "No Quake install found, so no expansions or soundtrack were added."
    Write-Host "Set QQ_QUAKEDIR to point at it if you have one."
}

Write-Host "Episode message text..."
Build-QcStrings $dest

Write-Host "Menu artwork..."
$kpfRoot = $null
foreach ($r in $script:QuakeRoots) {
    if (Test-Path -PathType Leaf (PathJoin $r 'rerelease\QuakeEX.kpf')) { $kpfRoot = $r; break }
}
Build-MenuArt $dest $kpfRoot

Write-Host ""
Write-Host 'Done. Start Virtual Desktop, connect it, then run "Quake VR.bat".'
exit 0
