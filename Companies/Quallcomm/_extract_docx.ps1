# Extract text from all .docx files in the script's directory into _raw_text\*.md
# Uses .NET zip API + XML parse of word/document.xml. Preserves headings & paragraph breaks.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root   = Split-Path -Parent $MyInvocation.MyCommand.Path
$outDir = Join-Path $root 'ARM_Linux_Kernel_Knowledgebase\_raw_text'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$ns = @{ w = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main' }

function Convert-DocxToText {
    param([string]$DocxPath, [string]$OutPath)

    $zip = [System.IO.Compression.ZipFile]::OpenRead($DocxPath)
    try {
        $entry = $zip.Entries | Where-Object { $_.FullName -eq 'word/document.xml' } | Select-Object -First 1
        if (-not $entry) { Write-Warning "No document.xml in $DocxPath"; return }
        $stream = $entry.Open()
        $reader = New-Object System.IO.StreamReader($stream)
        $xmlText = $reader.ReadToEnd()
        $reader.Close(); $stream.Close()
    } finally { $zip.Dispose() }

    $xml = [xml]$xmlText
    $nsMgr = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
    $nsMgr.AddNamespace('w', $ns.w)

    $sb = New-Object System.Text.StringBuilder
    $body = $xml.SelectSingleNode('//w:body', $nsMgr)
    if (-not $body) { Write-Warning "No body in $DocxPath"; return }

    foreach ($p in $body.SelectNodes('w:p | w:tbl', $nsMgr)) {
        if ($p.LocalName -eq 'tbl') {
            # render table rows as pipe-separated lines
            foreach ($row in $p.SelectNodes('w:tr', $nsMgr)) {
                $cells = @()
                foreach ($tc in $row.SelectNodes('w:tc', $nsMgr)) {
                    $cellTxt = ($tc.SelectNodes('.//w:t', $nsMgr) | ForEach-Object { $_.InnerText }) -join ''
                    $cells += ($cellTxt -replace '\s+', ' ').Trim()
                }
                if ($cells.Count -gt 0) { [void]$sb.AppendLine('| ' + ($cells -join ' | ') + ' |') }
            }
            [void]$sb.AppendLine('')
            continue
        }

        # paragraph
        $styleNode = $p.SelectSingleNode('w:pPr/w:pStyle', $nsMgr)
        $style = if ($styleNode) { $styleNode.GetAttribute('val', $ns.w) } else { '' }

        $textRuns = @()
        foreach ($node in $p.SelectNodes('.//w:t | .//w:tab | .//w:br', $nsMgr)) {
            switch ($node.LocalName) {
                't'   { $textRuns += $node.InnerText }
                'tab' { $textRuns += "`t" }
                'br'  { $textRuns += "`n" }
            }
        }
        $line = ($textRuns -join '').TrimEnd()
        if ([string]::IsNullOrWhiteSpace($line)) { [void]$sb.AppendLine(''); continue }

        # heading detection
        $prefix = ''
        if ($style -match '^Heading(\d)$' -or $style -match '^Title$') {
            if ($style -eq 'Title') { $prefix = '# ' }
            else {
                $lvl = [int]$Matches[1]
                if ($lvl -lt 1) { $lvl = 1 }
                if ($lvl -gt 6) { $lvl = 6 }
                $prefix = ('#' * $lvl) + ' '
            }
        }

        # list detection
        $numPr = $p.SelectSingleNode('w:pPr/w:numPr', $nsMgr)
        if ($numPr -and -not $prefix) { $prefix = '- ' }

        [void]$sb.AppendLine($prefix + $line)
    }

    [System.IO.File]::WriteAllText($OutPath, $sb.ToString(), [System.Text.Encoding]::UTF8)
}

$docs = Get-ChildItem -Path $root -Filter *.docx -File
Write-Host "Found $($docs.Count) .docx files"
foreach ($d in $docs) {
    $out = Join-Path $outDir ($d.BaseName + '.md')
    try {
        Convert-DocxToText -DocxPath $d.FullName -OutPath $out
        $size = (Get-Item $out).Length
        Write-Host ("  OK  {0,-65} -> {1} bytes" -f $d.Name, $size)
    } catch {
        Write-Warning ("FAIL {0}: {1}" -f $d.Name, $_.Exception.Message)
    }
}
Write-Host "Done. Output: $outDir"
