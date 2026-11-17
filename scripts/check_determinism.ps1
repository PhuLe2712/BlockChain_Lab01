param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable).Path
$projectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$logPath = Join-Path $projectRoot 'logs/run.log'
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('lab01-determinism-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $tempRoot | Out-Null

try {
    $outputs = @()
    $logs = @()
    for ($run = 1; $run -le 2; $run++) {
        Push-Location $projectRoot
        try {
            $output = & $exe 2>&1 | Out-String
            if ($LASTEXITCODE -ne 0) {
                throw "Lab01 run $run failed with exit code $LASTEXITCODE`n$output"
            }
            $outputs += $output
            $snapshot = Join-Path $tempRoot "run$run.log"
            Copy-Item -LiteralPath $logPath -Destination $snapshot
            $logs += (Get-FileHash -LiteralPath $snapshot -Algorithm SHA256).Hash
        }
        finally {
            Pop-Location
        }
    }

    if ($outputs[0] -cne $outputs[1]) {
        throw 'T8 failed: console output (including the final state hash) differs.'
    }
    if ($logs[0] -cne $logs[1]) {
        throw 'T8 failed: run.log differs between identical runs.'
    }
    Write-Output 'T8 passed: repeated runs produced identical output, state hash, and run.log.'
    Write-Output $outputs[0].TrimEnd()
}
finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}
