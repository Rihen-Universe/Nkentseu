param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [double]$DurationHours = 12.0,
    [int]$MaxRuns = 0,
    [double]$P99RegressionThreshold = 0.10,
    [int]$SleepSeconds = 0,
    [string]$Config = "Release",
    [string]$Platform = "windows",
    [string]$TestExecutable = "",
    [bool]$CleanConfigArtifacts = $true,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$Invariant = [System.Globalization.CultureInfo]::InvariantCulture

function Write-Step {
    param([string]$Message)
    Write-Host "[validate_release.ps1] $Message" -ForegroundColor Cyan
}

function Parse-DoubleInvariant {
    param([string]$Text)

    if ([string]::IsNullOrWhiteSpace($Text)) {
        return 0.0
    }

    $normalized = $Text.Trim().Replace(",", ".")
    $value = 0.0
    if ([double]::TryParse($normalized, [System.Globalization.NumberStyles]::Float, $Invariant, [ref]$value)) {
        return $value
    }

    return 0.0
}

function Resolve-TestExecutable {
    param(
        [string]$Root,
        [string]$ConfigName,
        [string]$OverridePath
    )

    if ($OverridePath) {
        if (-not (Test-Path $OverridePath)) {
            throw "Test executable not found: $OverridePath"
        }
        return (Resolve-Path $OverridePath).Path
    }

    $candidates = @(
        (Join-Path $Root "Build\Tests\$ConfigName-Windows\NKMemory_Tests.exe"),
        (Join-Path $Root "Build\Tests\$ConfigName\NKMemory_Tests.exe"),
        (Join-Path $Root "Build\Tests\$ConfigName-Windows\NKMemory_Tests")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "Unable to locate NKMemory_Tests executable. Build Release tests first or pass -TestExecutable."
}

function Parse-RunMetrics {
    param([string[]]$OutputLines)

    $metrics = [ordered]@{
        tests_passed = 0
        tests_total = 0
        asserts_passed = 0
        asserts_total = 0
        bench = @()
    }

    $testsLine = $OutputLines | Where-Object { $_ -match 'Tests\s*:' } | Select-Object -Last 1
    if ($testsLine -and $testsLine -match 'Tests\s*:\s*([0-9]+)[^0-9]+([0-9]+)') {
        $metrics.tests_passed = [int]$Matches[1]
        $metrics.tests_total = [int]$Matches[2]
    }

    $assertLine = $OutputLines | Where-Object { $_ -match 'Assertions\s*:' } | Select-Object -Last 1
    if ($assertLine -and $assertLine -match 'Assertions\s*:\s*([0-9]+)[^0-9]+([0-9]+)') {
        $metrics.asserts_passed = [int]$Matches[1]
        $metrics.asserts_total = [int]$Matches[2]
    }

    foreach ($line in $OutputLines) {
        if ($line -match '^\s*-\s*([A-Za-z0-9_]+)\s+avg=\s*([0-9]+(?:[.,][0-9]+)?)\s*ns\s+p99=\s*([0-9]+(?:[.,][0-9]+)?)\s*ns\s+failures=([0-9]+)\/([0-9]+)') {
            $metrics.bench += [pscustomobject]@{
                name = $Matches[1]
                avg_ns = Parse-DoubleInvariant $Matches[2]
                p99_ns = Parse-DoubleInvariant $Matches[3]
                failures = [int]$Matches[4]
                samples = [int]$Matches[5]
            }
        }
    }

    return $metrics
}

if (-not (Get-Command jenga -ErrorAction SilentlyContinue)) {
    throw "Command 'jenga' was not found in PATH."
}

if ($Platform.ToLowerInvariant() -ne "windows") {
    throw "This script is dedicated to Windows validation. Use scripts/validate_release.sh for Linux/WSL."
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$reportRoot = Join-Path $ProjectRoot "Build\Reports\NKMemory\Validation-Windows-$Config-$timestamp"
$logDir = Join-Path $reportRoot "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$runsCsv = Join-Path $reportRoot "runs.csv"
$benchCsv = Join-Path $reportRoot "benchmark.csv"
$regCsv = Join-Path $reportRoot "regressions.csv"
$summaryJson = Join-Path $reportRoot "summary.json"
$summaryMd = Join-Path $reportRoot "summary.md"

"run,start_iso,end_iso,duration_ms,exit_code,tests_passed,tests_total,asserts_passed,asserts_total" | Out-File -FilePath $runsCsv -Encoding utf8
"run,name,avg_ns,p99_ns,failures,samples" | Out-File -FilePath $benchCsv -Encoding utf8

Push-Location $ProjectRoot
try {
    if (-not $SkipBuild) {
        if ($CleanConfigArtifacts) {
            Write-Step "Cleaning config artifacts for $Config-Windows"
            $cleanTargets = @(
                (Join-Path $ProjectRoot "Build\Obj\$Config-Windows"),
                (Join-Path $ProjectRoot "Build\Lib\$Config-Windows"),
                (Join-Path $ProjectRoot "Build\Tests\$Config-Windows")
            )
            foreach ($target in $cleanTargets) {
                if (Test-Path $target) {
                    Remove-Item -Recurse -Force -ErrorAction Stop $target
                }
            }
        }

        Write-Step "Building project: jenga build --platform windows --config $Config"
        & jenga build --platform windows --config $Config
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with exit code $LASTEXITCODE."
        }
    }

    $testExe = Resolve-TestExecutable -Root $ProjectRoot -ConfigName $Config -OverridePath $TestExecutable
    Write-Step "Using test executable: $testExe"

    $startUtc = (Get-Date).ToUniversalTime()
    $deadline = (Get-Date).AddHours($DurationHours)
    $run = 0

    while ((Get-Date) -lt $deadline) {
        if ($MaxRuns -gt 0 -and $run -ge $MaxRuns) {
            break
        }

        $run++
        $runStart = Get-Date
        $logPath = Join-Path $logDir ("run_{0:D5}.log" -f $run)

        Write-Step "Run #$run"
        $output = & $testExe 2>&1
        $exitCode = $LASTEXITCODE
        $runEnd = Get-Date
        $durationMs = [math]::Round(($runEnd - $runStart).TotalMilliseconds, 3)

        $output | Out-File -FilePath $logPath -Encoding utf8
        $output | ForEach-Object { Write-Host $_ }

        $metrics = Parse-RunMetrics -OutputLines $output

        $durationMsText = ([double]$durationMs).ToString($Invariant)
        "{0},{1},{2},{3},{4},{5},{6},{7},{8}" -f `
            $run, `
            $runStart.ToString("o"), `
            $runEnd.ToString("o"), `
            $durationMsText, `
            $exitCode, `
            $metrics.tests_passed, `
            $metrics.tests_total, `
            $metrics.asserts_passed, `
            $metrics.asserts_total | Add-Content -Path $runsCsv

        foreach ($bench in $metrics.bench) {
            $avgText = ([double]$bench.avg_ns).ToString($Invariant)
            $p99Text = ([double]$bench.p99_ns).ToString($Invariant)
            "{0},{1},{2},{3},{4},{5}" -f `
                $run, `
                $bench.name, `
                $avgText, `
                $p99Text, `
                $bench.failures, `
                $bench.samples | Add-Content -Path $benchCsv
        }

        if ($exitCode -ne 0) {
            Write-Step "Stopping campaign on first failure (exit code $exitCode)."
            break
        }

        if ($SleepSeconds -gt 0) {
            Start-Sleep -Seconds $SleepSeconds
        }
    }

    $endUtc = (Get-Date).ToUniversalTime()
    $runs = @(Import-Csv $runsCsv)
    $benchRows = @(Import-Csv $benchCsv)

    $runFailures = ($runs | Where-Object { [int]$_.exit_code -ne 0 }).Count
    $totalRuns = $runs.Count
    $benchFailureSum = 0
    foreach ($row in $benchRows) {
        $benchFailureSum += [int]$row.failures
    }

    $regressions = @()
    $groups = $benchRows | Group-Object name
    foreach ($group in $groups) {
        $ordered = $group.Group | Sort-Object { [int]$_.run }
        if ($ordered.Count -eq 0) { continue }

        $baseline = Parse-DoubleInvariant $ordered[0].p99_ns
        $maxP99 = 0.0
        foreach ($row in $ordered) {
            $p99 = Parse-DoubleInvariant $row.p99_ns
            if ($p99 -gt $maxP99) {
                $maxP99 = $p99
            }
        }
        if ($baseline -le 0.0) { continue }

        $drift = ($maxP99 - $baseline) / $baseline
        if ($drift -gt $P99RegressionThreshold) {
            $regressions += [pscustomobject]@{
                name = $group.Name
                baseline_p99_ns = [math]::Round($baseline, 3)
                max_p99_ns = [math]::Round($maxP99, 3)
                drift_ratio = [math]::Round($drift, 6)
            }
        }
    }
    $regressions | Export-Csv -Path $regCsv -NoTypeInformation -Encoding utf8

    $status = "GO"
    if ($totalRuns -eq 0 -or $runFailures -gt 0 -or $benchFailureSum -gt 0 -or $regressions.Count -gt 0) {
        $status = "NO-GO"
    }

    $summaryObject = [ordered]@{
        status = $status
        platform = "windows"
        config = $Config
        start_utc = $startUtc.ToString("o")
        end_utc = $endUtc.ToString("o")
        duration_hours_requested = $DurationHours
        runs_executed = $totalRuns
        run_failures = $runFailures
        benchmark_failures = [int]$benchFailureSum
        p99_regressions = $regressions.Count
        p99_regression_threshold = $P99RegressionThreshold
        artifacts = [ordered]@{
            logs_dir = $logDir
            runs_csv = $runsCsv
            benchmark_csv = $benchCsv
            regressions_csv = $regCsv
            summary_md = $summaryMd
        }
    }
    $summaryObject | ConvertTo-Json -Depth 8 | Out-File -FilePath $summaryJson -Encoding utf8

    $latestRunId = if ($totalRuns -gt 0) { [int](($runs | Sort-Object { [int]$_.run } | Select-Object -Last 1).run) } else { 0 }
    $latestBench = @()
    if ($latestRunId -gt 0) {
        $latestBench = $benchRows | Where-Object { [int]$_.run -eq $latestRunId } | Sort-Object name
    }

    $md = @()
    $md += "# NKMemory Release Validation (Windows)"
    $md += ""
    $md += "- Status: **$status**"
    $md += "- Platform: Windows"
    $md += "- Config: $Config"
    $md += "- Runs: $totalRuns"
    $md += "- Run failures: $runFailures"
    $md += "- Benchmark failures: $benchFailureSum"
    $md += "- P99 regressions: $($regressions.Count) (threshold=$P99RegressionThreshold)"
    $md += ""
    $md += "## Artifacts"
    $md += ""
    $md += "- JSON summary: $summaryJson"
    $md += "- Runs CSV: $runsCsv"
    $md += "- Benchmark CSV: $benchCsv"
    $md += "- Regressions CSV: $regCsv"
    $md += "- Logs dir: $logDir"
    $md += ""
    $md += "## Latest Run Benchmarks"
    $md += ""
    $md += "| Allocator | Avg(ns) | P99(ns) | Failures | Samples |"
    $md += "|---|---:|---:|---:|---:|"
    foreach ($row in $latestBench) {
        $md += "| $($row.name) | $($row.avg_ns) | $($row.p99_ns) | $($row.failures) | $($row.samples) |"
    }
    if ($latestBench.Count -eq 0) {
        $md += "| (none) | - | - | - | - |"
    }

    $md += ""
    $md += "## GO / NO-GO Rule"
    $md += ""
    $md += "- GO if: no test run failure, no benchmark failure, no p99 regression over threshold."
    $md += "- NO-GO otherwise."
    $md | Out-File -FilePath $summaryMd -Encoding utf8

    Write-Step "Validation completed with status: $status"
    Write-Step "Summary JSON: $summaryJson"
    Write-Step "Summary Markdown: $summaryMd"
}
finally {
    Pop-Location
}
