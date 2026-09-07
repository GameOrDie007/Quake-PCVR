# Build the re-release episodes' message text from this install's own game data.
#
# A port of make-qc-strings.py with no dependencies. The two are verified against
# each other by generating qc_strings.txt both ways and comparing byte for byte,
# which matters more here than anywhere else: this is heuristics over string
# matching, and Python and .NET disagree about sorting and capitalisation unless
# they are told not to.

$script:QcGames = @('id1', 'hipnotic', 'rogue', 'dopa', 'mg1', 'mg3')

# "$token" : "text", with \" allowed inside the text
$script:QcFgdPair = [regex]'"(\$[A-Za-z0-9_]+)"\s*:\s*"((?:[^"\\]|\\.)*)"'
$script:QcToken   = [regex]'\$[A-Za-z0-9_]+'

# Where neither source has the wording, the token still says what the message
# was about. Dropping the leading context, which names the mod and the map
# rather than saying anything, leaves something a player can read.
$script:QcContext = [regex]'^(qc|m|map|hub|mg[0-9]|dopa|hip|hipnotic|rogue|map[0-9]+[a-z]?)$'

function Get-ProgsStrings([byte[]]$data) {
    if ($data.Length -lt 56) { return @() }
    $ofsStr = [BitConverter]::ToInt32($data, 8 + 8 * 4)
    $nStr   = [BitConverter]::ToInt32($data, 8 + 9 * 4)
    if ($ofsStr -lt 0 -or $nStr -lt 0 -or ($ofsStr + $nStr) -gt $data.Length) { return @() }
    $block = [System.Text.Encoding]::GetEncoding(28591).GetString($data, $ofsStr, $nStr)
    return $block.Split([char]0)
}

function Get-BspEntities([byte[]]$data) {
    if ($data.Length -lt 12) { return '' }
    $eo = [BitConverter]::ToInt32($data, 4)
    $el = [BitConverter]::ToInt32($data, 8)
    if ($eo -lt 0 -or $el -lt 0 -or ($eo + $el) -gt $data.Length) { return '' }
    return [System.Text.Encoding]::GetEncoding(28591).GetString($data, $eo, $el)
}

function Get-TokenWords([string]$tok) {
    $body = [regex]::Replace($tok.TrimStart('$'), '^[a-z0-9]*_?qc_', '')
    return @($body.Split('_') | Where-Object { $_ -ne '' })
}

function Get-SpelledOut([string[]]$words) {
    $w = @($words)
    while ($w.Count -gt 1 -and $script:QcContext.IsMatch($w[0])) {
        $w = @($w[1..($w.Count - 1)])
    }
    $s = ($w -join ' ')
    if ($s.Length -eq 0) { return $s }
    # Python's str.capitalize(): first character upper, everything else lower.
    return $s.Substring(0, 1).ToUpperInvariant() + $s.Substring(1).ToLowerInvariant()
}

function Get-ClassicMatch([string[]]$words, $candidates) {
    $best = $null
    foreach ($c in $candidates) {
        $cl = $c.ToLowerInvariant()
        $all = $true
        foreach ($w in $words) {
            if (-not [regex]::IsMatch($cl, '\b' + [regex]::Escape($w))) { $all = $false; break }
        }
        if (-not $all) { continue }
        if ($null -eq $best -or $c.Length -lt $best.Length) { $best = $c }
    }
    return $best
}

function Build-QcStrings([string]$dest) {
    $tokens = New-Object 'System.Collections.Generic.HashSet[string]'
    $fgd = New-Object 'System.Collections.Specialized.OrderedDictionary'
    $classic = New-Object System.Collections.ArrayList

    foreach ($game in $script:QcGames) {
        $pak = PathJoin $dest ($game + '\pak0.pak')
        if (-not (Test-Path -PathType Leaf $pak)) { continue }

        # One open per pak, not one per entry. mg1 and mg3 are around half a
        # gigabyte each with hundreds of maps in them, and reopening the file
        # for every entry turns a few seconds into minutes.
        $entries = Get-PakEntries $pak
        $fs = [System.IO.File]::OpenRead($pak)
        try {
            foreach ($e in $entries) {
                $name = $e[0]; $off = $e[1]; $len = $e[2]
                $lname = $name.ToLowerInvariant()

                $isFgd = $lname.EndsWith('.fgd')
                $isProgs = ($lname -eq 'progs.dat')
                $isBsp = $lname.EndsWith('.bsp')
                if (-not ($isFgd -or $isProgs -or $isBsp)) { continue }

                if ($len -le 0 -or ($off + $len) -gt $fs.Length) { continue }
                $fs.Position = $off
                $data = New-Object byte[] $len
                $read = 0
                while ($read -lt $len) {
                    $n = $fs.Read($data, $read, $len - $read)
                    if ($n -le 0) { break }
                    $read += $n
                }

                if ($isFgd) {
                    $text = [System.Text.Encoding]::GetEncoding(28591).GetString($data)
                    foreach ($m in $script:QcFgdPair.Matches($text)) {
                        $k = $m.Groups[1].Value
                        if (-not $fgd.Contains($k)) { $fgd[$k] = $m.Groups[2].Value }
                    }
                } elseif ($isProgs) {
                    foreach ($s in (Get-ProgsStrings $data)) {
                        if ($s.StartsWith('$')) {
                            [void]$tokens.Add($s)
                        } elseif ($game -eq 'id1' -and $s.Contains(' ') -and $s.Length -lt 80) {
                            [void]$classic.Add($s)
                        }
                    }
                } elseif ($isBsp) {
                    foreach ($m in $script:QcToken.Matches((Get-BspEntities $data))) {
                        [void]$tokens.Add($m.Value)
                    }
                }
            }
        } finally { $fs.Close() }
    }

    foreach ($k in $fgd.Keys) { [void]$tokens.Add($k) }

    if ($tokens.Count -eq 0) {
        Write-Host "  no re-release episodes installed, nothing to do"
        return
    }

    # Ordinal, to match Python's sort. The default here is culture-aware and
    # would order punctuation and case differently.
    $sorted = [System.Linq.Enumerable]::ToArray($tokens)
    [Array]::Sort($sorted, [System.StringComparer]::Ordinal)

    $rows = New-Object System.Collections.ArrayList
    $fromFgd = 0
    $fromClassic = 0

    foreach ($tok in $sorted) {
        if ($fgd.Contains($tok)) {
            $text = $fgd[$tok]
            $fromFgd++
        } else {
            $words = Get-TokenWords $tok
            $text = $null
            if ($words.Count -gt 0) { $text = Get-ClassicMatch $words $classic }
            if ($text) {
                $fromClassic++
            } else {
                $text = Get-SpelledOut $words
            }
        }
        # One entry per line, so a message's own line breaks stay escaped and the
        # engine turns them back into newlines as it reads them.
        $text = $text.Replace("`r", '').Replace("`n", '\n')
        [void]$rows.Add(($tok + "`t" + $text))
    }

    $lines = New-Object System.Collections.ArrayList
    [void]$lines.Add('// Message text for the re-release episodes, built by')
    [void]$lines.Add('// tools/make-qc-strings.py from this install''s own game data.')
    foreach ($r in $rows) { [void]$lines.Add($r) }

    $path = PathJoin $dest 'id1\qc_strings.txt'
    Write-TextLines $path $lines.ToArray() "`n"

    Write-Host ("  {0} messages: {1} from their own .fgd, {2} recovered from Quake, {3} unknown" -f `
        $rows.Count, $fromFgd, $fromClassic, ($rows.Count - $fromFgd - $fromClassic))
}
