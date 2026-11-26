# Runs the CppUnitTest DLL (tests/UnitTests) with vstest.console and writes a
# summary of the results.
#
#   pwsh tools/ci/unit-tests.ps1 -Dll out/bin/x64/Debug/UnitTests.dll -Out out/logs -Label Debug

param(
    [Parameter(Mandatory)] [string] $Dll,
    [Parameter(Mandatory)] [string] $Out,
    [string] $Label = ''
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path $Dll)) { throw "$Dll was not built" }
$Dll = (Resolve-Path $Dll).Path
New-Item -ItemType Directory -Force $Out | Out-Null
$Out = (Resolve-Path $Out).Path

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vstest = & $vswhere -latest -products * -find '**\TestPlatform\vstest.console.exe' | Select-Object -First 1
if (-not $vstest) { throw "vstest.console.exe not found" }
Write-Host "vstest: $vstest"

$trx = "unit-tests$(if ($Label) { "-$Label" }).trx"
& $vstest $Dll /Platform:x64 "/logger:trx;LogFileName=$trx" "/logger:console;verbosity=normal" "/ResultsDirectory:$Out"
$code = $LASTEXITCODE

# The summary: counts, and each failed test with its message
$trxPath = Join-Path $Out $trx
if (Test-Path $trxPath) {
    [xml] $x = Get-Content $trxPath
    $ns = @{ t = 'http://microsoft.com/schemas/VisualStudio/TeamTest/2010' }
    $c = (Select-Xml -Xml $x -XPath '//t:ResultSummary/t:Counters' -Namespace $ns).Node
    $md = @("## Unit tests$(if ($Label) { " ($Label)" })", '', "$($c.total) tests: $($c.passed) passed, $($c.failed) failed")
    foreach ($r in Select-Xml -Xml $x -XPath '//t:UnitTestResult[@outcome!="Passed"]' -Namespace $ns) {
        $msg = $r.Node.Output.ErrorInfo.Message -replace '\s+', ' '
        $md += "- **$($r.Node.testName)** ($($r.Node.outcome)): $msg"
    }
} else {
    $md = @("## Unit tests$(if ($Label) { " ($Label)" })", '', "No results file: vstest exited with $code")
}
$md -join "`n" | Write-Host
if ($env:GITHUB_STEP_SUMMARY) { ($md -join "`n") | Out-File -FilePath $env:GITHUB_STEP_SUMMARY -Append -Encoding utf8 }

exit $code
