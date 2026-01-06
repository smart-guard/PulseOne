# PulseOne Collector - Windows Service Registration Script
# Run this script as Administrator

$ServiceName = "PulseOneCollector"
$DisplayName = "PulseOne Data Collector"
$Description = "Industrial IoT Data Collection Engine for PulseOne"
$BinaryPath = "$PSScriptRoot\..\bin-windows\collector.exe"

# 1. Check Administrator Privileges
$currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "❌ This script must be run as Administrator."
    exit
}

# 2. Check Binary Path
if (-not (Test-Path $BinaryPath)) {
    Write-Warning "⚠️ Collector binary not found at $BinaryPath. Please build first."
    # Try alternative path
    $BinaryPath = "$PSScriptRoot\..\collector.exe"
    if (-not (Test-Path $BinaryPath)) {
        Write-Error "❌ Could not find collector.exe"
        exit
    }
}

# 3. Create Service
Write-Host "⚙️ Registering Windows Service: $DisplayName..."

if (Get-Service $ServiceName -ErrorAction SilentlyContinue) {
    Write-Host "🔄 Service already exists. Re-registering..."
    Stop-Service $ServiceName -ErrorAction SilentlyContinue
    sc.exe delete $ServiceName
    Start-Sleep -s 2
}

New-Service -Name $ServiceName `
            -BinaryPathName "`"$BinaryPath`"" `
            -DisplayName $DisplayName `
            -Description $Description `
            -StartupType Automatic

# 4. Set Recovery Options
sc.exe failure $ServiceName reset= 86400 actions= restart/5000/restart/10000/restart/15000

Write-Host "================================================================"
Write-Host "✅ Registration complete!"
Write-Host "To start service: Start-Service $ServiceName"
Write-Host "To stop service: Stop-Service $ServiceName"
Write-Host "To check status: Get-Service $ServiceName"
Write-Host "================================================================"
