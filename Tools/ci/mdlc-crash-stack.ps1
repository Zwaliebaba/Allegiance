# Re-runs the mdlc batch that failed in an Art.proj build under cdb and prints
# the faulting stack, with the last commands the batch reported before it.
param(
    [Parameter(Mandatory)] [string] $Mdlc,
    [Parameter(Mandatory)] [string] $Log,      # Art.proj file log
    [Parameter(Mandatory)] [string] $ArtSource,
    [Parameter(Mandatory)] [string] $ArtOut,
    [Parameter(Mandatory)] [string] $Out
)

$m = Select-String -Path $Log -Pattern '-batch "([^"]+\.batch)"' | Select-Object -Last 1
if (-not $m) { Write-Host "No failed mdlc batch found in $Log"; exit 0 }
$batch = $m.Matches[0].Groups[1].Value
# The model batches run in the output's Models folder (Art.proj).
$work = if ((Split-Path $batch -Leaf) -in 'models.batch', 'textmodels.batch') { Join-Path $ArtOut 'Models' } else { $ArtOut }

$cdb = @(
    "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\cdb.exe",
    "${env:ProgramFiles}\Windows Kits\10\Debuggers\x64\cdb.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $cdb) { Write-Host "cdb not found"; exit 0 }

$Mdlc = (Resolve-Path $Mdlc).Path
New-Item -ItemType Directory -Force $Out | Out-Null
$stack = Join-Path $Out ((Split-Path $batch -LeafBase) + '.stack.txt')
Write-Host "Re-running $batch in $work under cdb"
$cdbArgs = "-g -G -y `"$(Split-Path $Mdlc)`" -c `".lines -e; r; kpn 40; q`" `"$Mdlc`" -batch `"$batch`""
$d = Start-Process -FilePath $cdb -ArgumentList $cdbArgs -WorkingDirectory $work `
    -NoNewWindow -PassThru -RedirectStandardOutput $stack -RedirectStandardError "$stack.err"
if (-not $d.WaitForExit(1200000)) { $d.Kill() }

$lines = Get-Content $stack -ErrorAction SilentlyContinue
$fault = ($lines | Select-String -Pattern 'Access violation|assertion failed|code [0-9a-f]{8} \(first' | Select-Object -First 1).LineNumber
Write-Host "---- last batch output before the fault ----"
if ($fault) { $lines[[math]::Max(0, $fault - 8)..($fault - 1)] | ForEach-Object { Write-Host $_ } }
Write-Host "---- crash stack ----"
$lines | Select-String -Pattern '^\s*[0-9a-f]{2} |Access violation|assertion failed|^\(' | Select-Object -First 60 | ForEach-Object { Write-Host $_.Line }
