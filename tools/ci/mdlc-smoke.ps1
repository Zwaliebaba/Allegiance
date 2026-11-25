# Smoke test for the art toolchain: does mdlc (which starts the engine with its
# software DirectDraw path) run on this machine, and does it produce output?
#
#   pwsh tools/ci/mdlc-smoke.ps1 -Mdlc out/bin/Win32/Debug/mdlc.exe -Art Artwork/256 -Out out/art-smoke
#
# mdlc reports failure by printing a message and exiting 0, and a failed device
# init or a debug assert can show a modal dialog, so every call runs with a
# timeout and success is judged by the output file.

param(
    [Parameter(Mandatory)] [string] $Mdlc,
    [Parameter(Mandatory)] [string] $Art,
    [Parameter(Mandatory)] [string] $Out,
    [int] $TimeoutSeconds = 60
)

$ErrorActionPreference = 'Stop'
$Mdlc = (Resolve-Path $Mdlc).Path
$Art  = (Resolve-Path $Art).Path
New-Item -ItemType Directory -Force $Out | Out-Null
$Out  = (Resolve-Path $Out).Path

# mdlc resolves inputs relative to the current directory, like the original
# makefiles (which cd'd into the output directory), so stage the inputs there.
foreach ($f in 'acbuild.bmp', 'animlaunch.bmp', 'fontsource.mdl') {
    Copy-Item (Join-Path $Art $f) $Out -Force
}

$results = @()
function Invoke-Mdlc([string] $Label, [string[]] $MdlcArgs, [string] $Expect) {
    $log = Join-Path $Out "$Label.log"
    $p = Start-Process -FilePath $Mdlc -ArgumentList $MdlcArgs -WorkingDirectory $Out `
        -NoNewWindow -PassThru -RedirectStandardOutput $log -RedirectStandardError "$log.err"
    $finished = $p.WaitForExit($TimeoutSeconds * 1000)
    if (-not $finished) { $p.Kill(); $state = "TIMEOUT after ${TimeoutSeconds}s (likely a modal dialog)" }
    else { $state = "exit $($p.ExitCode)" }
    $outFile = Join-Path $Out $Expect
    $ok = $finished -and (Test-Path $outFile) -and ((Get-Item $outFile).Length -gt 0)
    $size = if (Test-Path $outFile) { (Get-Item $outFile).Length } else { 0 }
    $text = ((Get-Content $log -ErrorAction SilentlyContinue) + (Get-Content "$log.err" -ErrorAction SilentlyContinue)) -join ' | '
    $script:results += [pscustomobject]@{ Step = $Label; Result = $(if ($ok) { 'OK' } else { 'FAIL' }); Process = $state; Output = "$Expect ($size bytes)"; Message = $text }

    # On a crash, rerun under the console debugger to get a symbolised stack.
    if ($finished -and $p.ExitCode -lt 0 -and $script:cdb) {
        $stack = Join-Path $Out "$Label.stack.txt"
        # Start-Process does not quote array elements, so build the command line by hand.
        $cdbArgs = "-g -G -y `"$(Split-Path $Mdlc)`" -c `".lines -e; r; kpn 40; q`" `"$Mdlc`" " + ($MdlcArgs -join ' ')
        $d = Start-Process -FilePath $script:cdb -ArgumentList $cdbArgs -WorkingDirectory $Out `
            -NoNewWindow -PassThru -RedirectStandardOutput $stack -RedirectStandardError "$stack.err"
        if (-not $d.WaitForExit($TimeoutSeconds * 2000)) { $d.Kill() }
        Write-Host "---- $Label crash stack (cdb) ----"
        Get-Content $stack -ErrorAction SilentlyContinue | Select-String -Pattern '^\s*[0-9a-f]{2} |ExceptionAddress|ExceptionCode|Access violation|^\(|\.cpp|\.h' | Select-Object -First 60 | ForEach-Object { Write-Host $_.Line }
    }
}

# The debugger has to match mdlc's architecture: read the PE machine field
$pe = [IO.File]::ReadAllBytes($Mdlc)
$machine = [BitConverter]::ToUInt16($pe, [BitConverter]::ToInt32($pe, 0x3C) + 4)
$dbgArch = if ($machine -eq 0x8664) { 'x64' } else { 'x86' }
$script:cdb = @(
    "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\$dbgArch\cdb.exe",
    "${env:ProgramFiles}\Windows Kits\10\Debuggers\$dbgArch\cdb.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
Write-Host "cdb: $(if ($script:cdb) { $script:cdb } else { 'not found' })"

Invoke-Mdlc 'convert-bitmap' @('-convert', 'acbuild.bmp', 'acbuildbmp.mdl') 'acbuildbmp.mdl'
Invoke-Mdlc 'compile-font'   @('fontsource', 'font.mdl') 'font.mdl'
Invoke-Mdlc 'convert-anim'   @('-convert', 'animlaunch.bmp', 'animlaunchbmp.mdl') 'animlaunchbmp.mdl'
Invoke-Mdlc 'compress-anim'  @('-compressanim', '4', '4', 'animlaunchbmp', 'animlaunch.mdl') 'animlaunch.mdl'

$results | Format-Table -AutoSize -Wrap | Out-String -Width 200 | Write-Host

if ($env:GITHUB_STEP_SUMMARY) {
    $md = @('## mdlc smoke test', '', '| Step | Result | Process | Output | Message |', '|---|---|---|---|---|')
    $md += $results | ForEach-Object { "| $($_.Step) | $($_.Result) | $($_.Process) | $($_.Output) | $($_.Message) |" }
    ($md -join "`n") | Out-File -FilePath $env:GITHUB_STEP_SUMMARY -Append -Encoding utf8
}

if ($results.Result -contains 'FAIL') { exit 1 }
