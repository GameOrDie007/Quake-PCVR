# Shared helpers for the PowerShell setup: paths, Steam, pak reading, images.
#
# Lifted from the Quake II PCVR port, where these were written and verified
# first. Two separate repositories with separate releases, so this is a copy
# rather than a shared module - but keep them in step, because every trap
# recorded here was paid for once already.

Add-Type -AssemblyName System.Drawing

function PathJoin([string]$a, [string]$b) {
    # Deliberately not Join-Path. That is a provider cmdlet: it validates the
    # drive and throws DriveNotFoundException for a path on a drive this machine
    # does not have - and a list of install guesses is exactly a list of those.
    # Python's os.path.join, which this was ported from, is pure string handling.
    return [System.IO.Path]::Combine($a, $b)
}

function Write-TextLines([string]$path, [string[]]$lines, [string]$eol = "`r`n") {
    # ASCII, no BOM, and the caller picks the line ending: launchers are CRLF,
    # qc_strings.txt is LF because that is what the Python wrote.
    $text = ($lines -join $eol) + $eol
    [System.IO.File]::WriteAllText($path, $text, [System.Text.Encoding]::GetEncoding(28591))
}

function Copy-IfNeeded([string]$src, [string]$dst, [string]$label) {
    if (-not (Test-Path -PathType Leaf $src)) { return $false }
    if (Test-Path -PathType Leaf $dst) {
        if ((Get-Item $dst).Length -eq (Get-Item $src).Length) { return $true }
    }
    if ($label) { Write-Host ("  " + $label) }
    $dir = [System.IO.Path]::GetDirectoryName($dst)
    if (-not (Test-Path -PathType Container $dir)) { [void](New-Item -ItemType Directory -Force $dir) }
    Copy-Item -LiteralPath $src -Destination $dst -Force
    return $true
}

function Find-FileAnyCase([string]$folder, [string]$name) {
    # Steam ships the classic game as id1/PAK0.PAK and the re-release as
    # id1/pak0.pak.
    if (-not (Test-Path -PathType Container $folder)) { return $null }
    $want = $name.ToLower()
    foreach ($f in Get-ChildItem -LiteralPath $folder -File -ErrorAction SilentlyContinue) {
        if ($f.Name.ToLower() -eq $want) { return $f.FullName }
    }
    return $null
}

# --- where Steam and GOG keep things ---------------------------------------

function Get-SteamLibraries {
    # Ask Steam rather than guessing drive letters. A hardcoded list only ever
    # finds the machine it was written on.
    $roots = New-Object System.Collections.ArrayList
    foreach ($k in @('HKCU:\Software\Valve\Steam',
                     'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam',
                     'HKLM:\SOFTWARE\Valve\Steam')) {
        try { $v = Get-ItemProperty $k -ErrorAction SilentlyContinue } catch { continue }
        if (-not $v) { continue }
        foreach ($p in @($v.SteamPath, $v.InstallPath)) {
            if ($p) { [void]$roots.Add(($p -replace '/', '\')) }
        }
    }

    $libs = New-Object System.Collections.ArrayList
    $seen = @{}
    foreach ($root in $roots) {
        if (-not (Test-Path -PathType Container $root)) { continue }
        $key = $root.ToLower().TrimEnd('\')
        if (-not $seen.ContainsKey($key)) { $seen[$key] = $true; [void]$libs.Add($root) }

        $vdf = PathJoin $root 'steamapps\libraryfolders.vdf'
        if (-not (Test-Path -PathType Leaf $vdf)) { continue }
        foreach ($line in [System.IO.File]::ReadAllLines($vdf)) {
            $m = [regex]::Match($line, '"path"\s+"(.+?)"')
            if (-not $m.Success) { continue }
            $lib = $m.Groups[1].Value -replace '\\\\', '\'
            $key = $lib.ToLower().TrimEnd('\')
            if (-not $seen.ContainsKey($key)) { $seen[$key] = $true; [void]$libs.Add($lib) }
        }
    }
    return $libs
}

function Get-GogGameDirs {
    $out = New-Object System.Collections.ArrayList
    foreach ($k in @('HKLM:\SOFTWARE\WOW6432Node\GOG.com\Games',
                     'HKLM:\SOFTWARE\GOG.com\Games')) {
        try { $keys = Get-ChildItem $k -ErrorAction SilentlyContinue } catch { continue }
        foreach ($g in $keys) {
            try { $v = Get-ItemProperty $g.PSPath -ErrorAction SilentlyContinue } catch { continue }
            if ($v -and $v.path) { [void]$out.Add($v.path) }
        }
    }
    return $out
}

# --- pak files --------------------------------------------------------------

function Get-PakEntries([string]$path) {
    # [name, offset, length] for every file in a Quake pak.
    $out = New-Object System.Collections.ArrayList
    try {
        $fs = [System.IO.File]::OpenRead($path)
    } catch { return $out }
    try {
        $br = New-Object System.IO.BinaryReader($fs)
        if ([System.Text.Encoding]::ASCII.GetString($br.ReadBytes(4)) -ne 'PACK') { return $out }
        $ofs = $br.ReadInt32(); $len = $br.ReadInt32()
        if ($ofs -le 0 -or $len -le 0 -or ($ofs + $len) -gt $fs.Length) { return $out }
        $fs.Position = $ofs
        for ($i = 0; $i -lt [int]($len / 64); $i++) {
            $rec = $br.ReadBytes(64)
            $z = [Array]::IndexOf($rec, [byte]0, 0, 56)
            if ($z -lt 0) { $z = 56 }
            $name = [System.Text.Encoding]::GetEncoding(28591).GetString($rec, 0, $z)
            [void]$out.Add(@($name,
                             [BitConverter]::ToInt32($rec, 56),
                             [BitConverter]::ToInt32($rec, 60)))
        }
    } finally { $fs.Close() }
    return $out
}

function Read-PakEntry([string]$path, [int]$offset, [int]$length) {
    $fs = [System.IO.File]::OpenRead($path)
    try {
        $fs.Position = $offset
        $buf = New-Object byte[] $length
        $read = 0
        while ($read -lt $length) {
            $n = $fs.Read($buf, $read, $length - $read)
            if ($n -le 0) { break }
            $read += $n
        }
        return $buf
    } finally { $fs.Close() }
}

function Test-QuakeOnePak([string]$pak) {
    # Quake and Quake II both call a mission pack folder 'rogue', so a search
    # across a Steam library can hand back the wrong game entirely. Quake keeps
    # its game logic in progs.dat inside the pak; Quake II keeps it in a DLL.
    foreach ($e in (Get-PakEntries $pak)) {
        if ($e[0].ToLower() -eq 'progs.dat') { return $true }
    }
    return $false
}

# --- images -----------------------------------------------------------------

# An image is width, height and a BGRA byte array, which is the order
# System.Drawing uses in memory.
function New-Img([int]$w, [int]$h) {
    [pscustomobject]@{ W = $w; H = $h; P = (New-Object byte[] ($w * $h * 4)) }
}

function ConvertTo-Img($bitmap) {
    $w = $bitmap.Width; $h = $bitmap.Height
    $img = New-Img $w $h
    $rect = New-Object System.Drawing.Rectangle(0, 0, $w, $h)
    $data = $bitmap.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($y = 0; $y -lt $h; $y++) {
        [System.Runtime.InteropServices.Marshal]::Copy(
            [IntPtr]::Add($data.Scan0, $y * $data.Stride), $img.P, $y * $w * 4, $w * 4)
    }
    $bitmap.UnlockBits($data)
    return $img
}

function Read-ImgFromBytes([byte[]]$bytes) {
    $ms = New-Object System.IO.MemoryStream(, $bytes)
    try {
        $bmp = [System.Drawing.Image]::FromStream($ms)
        try { return ConvertTo-Img $bmp } finally { $bmp.Dispose() }
    } finally { $ms.Dispose() }
}

function Save-Tga($img, [string]$path) {
    # 32 bit uncompressed BGRA, bottom-up - the plainest thing DarkPlaces reads,
    # and byte for byte what the Python wrote.
    $w = $img.W; $h = $img.H
    $out = New-Object byte[] (18 + $w * $h * 4)
    $out[2] = 2                       # uncompressed true-colour
    $out[12] = [byte]($w -band 0xFF); $out[13] = [byte](($w -shr 8) -band 0xFF)
    $out[14] = [byte]($h -band 0xFF); $out[15] = [byte](($h -shr 8) -band 0xFF)
    $out[16] = 32                     # bits per pixel
    $out[17] = 8                      # 8 bits of alpha
    $o = 18
    for ($y = $h - 1; $y -ge 0; $y--) {
        [Array]::Copy($img.P, $y * $w * 4, $out, $o, $w * 4)
        $o += $w * 4
    }
    [System.IO.File]::WriteAllBytes($path, $out)
}
