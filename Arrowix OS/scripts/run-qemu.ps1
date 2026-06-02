<#
    Arrowix OS - Windows/PowerShell wrapper to run the kernel in QEMU.

    On Windows the toolchain lives under WSL2, so by default this delegates to
    the bash script inside WSL. If a native Windows qemu-system-x86_64 is on
    PATH, pass -Native to use it directly.
#>
param(
    [switch]$Debug,
    [switch]$Native
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot

if (-not $Native) {
    # Delegate to the bash script under WSL.
    $args = @("scripts/run-qemu.sh")
    if ($Debug) { $args += "--debug" }
    Push-Location $Root
    try {
        wsl bash @args
    } finally {
        Pop-Location
    }
    return
}

# Native Windows QEMU path.
$Iso = Join-Path $Root "build/arrowix.iso"
if (-not (Test-Path $Iso)) {
    Write-Error "ISO not found at $Iso. Build it first (make-iso)."
}

$qemuArgs = @("-cdrom", $Iso, "-m", "512M", "-serial", "stdio", "-no-reboot")
if ($Debug) { $qemuArgs += @("-S", "-gdb", "tcp::1234") }

& qemu-system-x86_64 @qemuArgs
