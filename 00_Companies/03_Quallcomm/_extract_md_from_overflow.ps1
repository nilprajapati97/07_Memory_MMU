# Extracts the outer ```markdown fenced block from each overflow content.txt and
# writes it to the target doc. Handles inner fenced code blocks correctly.

$pairs = @(
    @{ Name = '02_Scheduling_and_Synchronization.md';            Src = 'toolu_vrtx_01EEiik5dP7WEDJEiMZuN9mx__vscode-1779310768708' }
    @{ Name = '03_Interrupts_IPI_and_Watchdog.md';               Src = 'toolu_vrtx_01SBDyScd9jLAg5K3VBmYiHq__vscode-1779310768709' }
    @{ Name = '04_Linux_Drivers_DT_proc_sysfs_Syscalls.md';      Src = 'toolu_vrtx_01LrHzgL7VPHticsnAMp2Nyt__vscode-1779310768710' }
)
$root = 'c:\Users\AnilKumar\AppData\Roaming\Code\User\workspaceStorage\d8037d713bc2ccad666eddc1a540eea2\GitHub.copilot-chat\chat-session-resources\ad0dd9f2-77c1-4714-9d11-bd4040f3ea33'
$dest = 'c:\My_Workspace\Documents\drive-download-20260515T102331Z-3-001\ARM_Linux_Kernel_Knowledgebase'

function Extract-OuterMarkdownBlock {
    param([string]$text)
    $lines = $text -split "`r?`n"
    $start = -1
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i] -match '^```(markdown|md)\s*$') { $start = $i + 1; break }
    }
    if ($start -lt 0) { return $text }
    $depth = 0
    $end = $lines.Length - 1
    for ($i = $start; $i -lt $lines.Length; $i++) {
        $l = $lines[$i]
        if ($l -match '^```[\w+-]+\s*$') { $depth++; continue }
        if ($l -match '^```\s*$') {
            if ($depth -gt 0) { $depth-- }
            else { $end = $i - 1; break }
        }
    }
    return ($lines[$start..$end] -join "`n")
}

foreach ($p in $pairs) {
    $src = Join-Path $root ($p.Src + '\content.txt')
    if (-not (Test-Path $src)) { Write-Warning "Missing $src"; continue }
    $raw = Get-Content -Raw -LiteralPath $src
    $md  = Extract-OuterMarkdownBlock $raw
    $out = Join-Path $dest $p.Name
    [System.IO.File]::WriteAllText($out, $md, [System.Text.Encoding]::UTF8)
    $lc = ($md -split "`n").Count
    Write-Host ("Wrote {0,-50} {1,6} lines  {2,7} bytes" -f $p.Name, $lc, $md.Length)
}
