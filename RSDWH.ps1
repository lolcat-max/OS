# Get the full list of devices from pnputil
$pnp = pnputil /enum-devices

# Split the output into blocks
$blocks = ($pnp -join "`n") -split '(?m)^\s*$'

# Initialize an array to hold the devices to remove
$rootDevices = @()

# Collect Instance IDs of software-only devices
foreach ($b in $blocks) {
    if ($b -match 'Instance ID:\s+(ROOT\\[^\r\n]+)') {
        $id = $matches[1].Trim()
        $classLine = $b | Select-String -Pattern 'Class:\s+(.+)$'
        $class = if ($classLine) { $classLine.Matches.Groups[1].Value.Trim() } else { "Unknown" }
        
        $rootDevices += [PSCustomObject]@{
            InstanceId = $id
            Class      = $class
        }
    }
}

# Display the list of devices to be removed
if ($rootDevices) {
    Write-Host "The following $($rootDevices.Count) software-only devices will be removed:" -ForegroundColor Yellow
    $rootDevices | Format-Table -AutoSize

    # Final, irrevocable confirmation
    $confirmation = Read-Host "Are you absolutely sure you want to remove these devices? (Y/N)"
    
    if ($confirmation -eq 'Y') {
        foreach ($device in $rootDevices) {
            Write-Host "Removing $($device.InstanceId) (Class: $($device.Class))..."
            # Use the correct Instance ID, not the class, for the removal command
            pnputil.exe /remove-device $device.InstanceId
        }
        Write-Host "Cleanup operation complete." -ForegroundColor Green
    } else {
        Write-Host "Operation cancelled by user."
    }
} else {
    Write-Host "No devices to remove."
}
