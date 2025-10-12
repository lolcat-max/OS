# Get all devices that are not currently present
$NonPresentDevices = Get-PnpDevice | Where-Object { $_.Present -eq $false }

# Loop through each non-present device and remove it
foreach ($Device in $NonPresentDevices) {
    $InstanceId = $Device.InstanceId
    Write-Host "Attempting to remove: $($Device.FriendlyName) ($InstanceId)"
    try {
        Remove-PnpDevice -InstanceId $InstanceId -Confirm:$false -ErrorAction Stop
        Write-Host " -> Successfully removed." -ForegroundColor Green
    } catch {
        Write-Host " -> Failed to remove. Error: $_" -ForegroundColor Red
    }
}

Write-Host "Device cleanup complete."
