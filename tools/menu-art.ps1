# Render the game select page's entries in the 2021 re-release's menu font.
#
# A port of make-menu-art.py with no dependencies - PowerShell and System.Drawing
# both ship with Windows. The maths is that script's, unchanged, and the two are
# verified against each other by rendering every name both ways and comparing the
# TGA files byte for byte.
#
# Quake's ornate menu lettering exists only as pre-rendered pictures, so names
# like "Dimension of the Machine" cannot be drawn in it. The re-release does have
# that alphabet as a real font and draws its own expansion menu with it, which is
# the look being matched.
#
#   QuakeEX.kpf is a zip. fonts/qfont.png is the atlas and fonts/qfont.kfont is a
#   plain text table of "codepoint x y width height offset", one line per glyph.
#
# The font is read from the owner's own Quake install and the pictures written
# into his own install. Nothing is copied into this repository.

# Must match the gameselect_games table in menu.c.
$script:MenuArtNames = @(
    @('quake',    'Quake'),
    @('hipnotic', 'Scourge of Armagon'),
    @('rogue',    'Dissolution of Eternity'),
    @('dopa',     'Dimension of the Past'),
    @('mg1',      'Dimension of the Machine'),
    @('mg3',      'Dawn of the Machine')
)

$script:MenuArtTracking = 1     # pixels between glyphs, on top of each glyph's width

# The atlas glyphs are a dark bronze, which reads well on the re-release's own
# pale menu but disappears against a dimmed Quake level, and worse through a
# headset. Lifted so they carry at the edge of vision; the hue is untouched.
$script:MenuArtBrighten = 1.7

function Get-QuakeFont([string]$kpfPath) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::OpenRead($kpfPath)
    try {
        $atlasEntry = $zip.Entries | Where-Object { $_.FullName -eq 'fonts/qfont.png' } | Select-Object -First 1
        $kfontEntry = $zip.Entries | Where-Object { $_.FullName -eq 'fonts/qfont.kfont' } | Select-Object -First 1
        if (-not $atlasEntry -or -not $kfontEntry) { return $null }

        # Read both entries fully into memory - a zip stream is not seekable and
        # the image decoder wants to seek.
        $ms = New-Object System.IO.MemoryStream
        $s = $atlasEntry.Open(); try { $s.CopyTo($ms) } finally { $s.Dispose() }
        $atlas = Read-ImgFromBytes $ms.ToArray()
        $ms.Dispose()

        $ms2 = New-Object System.IO.MemoryStream
        $s2 = $kfontEntry.Open(); try { $s2.CopyTo($ms2) } finally { $s2.Dispose() }
        $text = [System.Text.Encoding]::GetEncoding(28591).GetString($ms2.ToArray())
        $ms2.Dispose()

        $glyphs = @{}
        foreach ($line in ($text -split "`r?`n")) {
            $parts = $line.Trim() -split '\s+'
            if ($parts.Count -eq 6 -and $parts[0] -match '^\d+$') {
                $glyphs[[int]$parts[0]] = @([int]$parts[1], [int]$parts[2], [int]$parts[3],
                                            [int]$parts[4], [int]$parts[5])
            }
        }
        return @{ Atlas = $atlas; Glyphs = $glyphs }
    } finally { $zip.Dispose() }
}

function Invoke-RenderText($font, [string]$text) {
    $atlas = $font.Atlas
    $glyphs = $font.Glyphs

    $used = New-Object System.Collections.ArrayList
    foreach ($ch in $text.ToCharArray()) {
        $cp = [int][char]$ch
        if ($glyphs.ContainsKey($cp)) { [void]$used.Add($glyphs[$cp]) }
    }
    if ($used.Count -eq 0) { return $null }

    $width = 0
    $height = 0
    foreach ($g in $used) {
        $width += $g[2]
        $t = $g[3] + $g[4]
        if ($t -gt $height) { $height = $t }
    }
    $width += $script:MenuArtTracking * ($used.Count - 1)

    $out = New-Img $width $height

    $x = 0
    foreach ($g in $used) {
        $gx = $g[0]; $gy = $g[1]; $gw = $g[2]; $gh = $g[3]; $off = $g[4]
        # A straight blit, as Pillow's paste with no mask is - it replaces the
        # destination including its alpha rather than compositing onto it.
        for ($row = 0; $row -lt $gh; $row++) {
            $srcOfs = (($gy + $row) * $atlas.W + $gx) * 4
            $dstOfs = (($off + $row) * $width + $x) * 4
            [Array]::Copy($atlas.P, $srcOfs, $out.P, $dstOfs, $gw * 4)
        }
        $x += $gw + $script:MenuArtTracking
    }

    for ($i = 0; $i -lt $width * $height; $i++) {
        $o = $i * 4
        if ($out.P[$o + 3] -eq 0) { continue }
        for ($c = 0; $c -lt 3; $c++) {
            $v = [int][Math]::Truncate($out.P[$o + $c] * $script:MenuArtBrighten)
            if ($v -gt 255) { $v = 255 }
            $out.P[$o + $c] = [byte]$v
        }
    }

    return $out
}

function Build-MenuArt([string]$dest, [string]$quakeDir) {
    $kpf = $null
    if ($quakeDir) { $kpf = PathJoin $quakeDir 'rerelease\QuakeEX.kpf' }
    if (-not $kpf -or -not (Test-Path -PathType Leaf $kpf)) {
        Write-Host "  no QuakeEX.kpf in this install - the re-release supplies the font,"
        Write-Host "  so the game select page falls back to plain text without it."
        return
    }

    $font = Get-QuakeFont $kpf
    if (-not $font) {
        Write-Host "  QuakeEX.kpf has no font atlas in it - skipping menu artwork"
        return
    }

    $gfx = PathJoin $dest 'id1\gfx'
    [void](New-Item -ItemType Directory -Force $gfx)

    foreach ($pair in $script:MenuArtNames) {
        $img = Invoke-RenderText $font $pair[1]
        if (-not $img) { continue }
        Save-Tga $img (PathJoin $gfx ('gs_' + $pair[0] + '.tga'))
        Write-Host ("  {0,-24} {1}x{2}" -f $pair[1], $img.W, $img.H)
    }
}
