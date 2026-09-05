# AEGIS Warden Connection Diagnostic Script
# Run this on the backend machine (10.75.11.83)

Write-Host "`n=== AEGIS WARDEN CONNECTION DIAGNOSTICS ===`n" -ForegroundColor Cyan

# Check 1: Backend port listening status
Write-Host "[1/6] Checking if backend is listening on port 8000..." -ForegroundColor Yellow
$listening = netstat -an | Select-String ":8000" | Select-String "LISTENING"
if ($listening -match "0.0.0.0:8000") {
    Write-Host "   ✓ Backend is listening on ALL interfaces (0.0.0.0:8000)" -ForegroundColor Green
} elseif ($listening -match "127.0.0.1:8000") {
    Write-Host "   ✗ Backend is ONLY listening on localhost (127.0.0.1:8000)" -ForegroundColor Red
    Write-Host "   → FIX: Restart backend with: python -m uvicorn app.main:app --host 0.0.0.0 --port 8000" -ForegroundColor Yellow
} else {
    Write-Host "   ✗ Backend is NOT listening on port 8000" -ForegroundColor Red
    Write-Host "   → FIX: Start backend with: python -m uvicorn app.main:app --host 0.0.0.0 --port 8000" -ForegroundColor Yellow
}

# Check 2: IP address configuration
Write-Host "`n[2/6] Checking network configuration..." -ForegroundColor Yellow
$ipConfig = ipconfig | Select-String "IPv4" | Select-Object -First 1
Write-Host "   Current IP: $ipConfig" -ForegroundColor White
if ($ipConfig -match "10\.75\.11\.83") {
    Write-Host "   ✓ IP address matches expected backend IP (10.75.11.83)" -ForegroundColor Green
} else {
    Write-Host "   ! Warning: IP address may not match expected 10.75.11.83" -ForegroundColor Yellow
}

# Check 3: Firewall status
Write-Host "`n[3/6] Checking Windows Firewall status..." -ForegroundColor Yellow
try {
    $firewallStatus = Get-NetFirewallProfile | Select-Object Name, Enabled
    $anyEnabled = $firewallStatus | Where-Object { $_.Enabled -eq $true }
    if ($anyEnabled) {
        Write-Host "   ! Firewall is ENABLED on some profiles" -ForegroundColor Yellow
        Write-Host "   → May need to add firewall rule for port 8000" -ForegroundColor Yellow
        Write-Host "   → Run: netsh advfirewall firewall add rule name=`"AEGIS Backend`" dir=in action=allow protocol=TCP localport=8000" -ForegroundColor Cyan
    } else {
        Write-Host "   ✓ Firewall is disabled" -ForegroundColor Green
    }
} catch {
    Write-Host "   ? Could not check firewall status (may need admin privileges)" -ForegroundColor Yellow
}

# Check 4: Test localhost backend
Write-Host "`n[4/6] Testing backend on localhost..." -ForegroundColor Yellow
try {
    $response = Invoke-WebRequest -Uri "http://localhost:8000/" -TimeoutSec 3 -ErrorAction Stop
    if ($response.StatusCode -eq 200) {
        Write-Host "   ✓ Backend responds on localhost" -ForegroundColor Green
    }
} catch {
    Write-Host "   ✗ Backend NOT responding on localhost" -ForegroundColor Red
    Write-Host "   → Is backend running? Start with: python -m uvicorn app.main:app --host 0.0.0.0 --port 8000" -ForegroundColor Yellow
}

# Check 5: Test backend on network IP
Write-Host "`n[5/6] Testing backend on network IP (10.75.11.83)..." -ForegroundColor Yellow
try {
    $response = Invoke-WebRequest -Uri "http://10.75.11.83:8000/" -TimeoutSec 3 -ErrorAction Stop
    if ($response.StatusCode -eq 200) {
        Write-Host "   ✓ Backend is accessible from network IP" -ForegroundColor Green
    }
} catch {
    Write-Host "   ✗ Backend NOT accessible on network IP" -ForegroundColor Red
    Write-Host "   → Backend may be listening on 127.0.0.1 only" -ForegroundColor Yellow
    Write-Host "   → Or firewall is blocking port 8000" -ForegroundColor Yellow
}

# Check 6: Ping Warden IP
Write-Host "`n[6/6] Testing connectivity to Warden (10.75.11.50)..." -ForegroundColor Yellow
$pingResult = Test-Connection -ComputerName "10.75.11.50" -Count 2 -Quiet
if ($pingResult) {
    Write-Host "   ✓ Warden device is reachable on network" -ForegroundColor Green
} else {
    Write-Host "   ✗ Warden device is NOT reachable" -ForegroundColor Red
    Write-Host "   → Is Warden powered on?" -ForegroundColor Yellow
    Write-Host "   → Is Warden connected to Wi-Fi (SSID: A34)?" -ForegroundColor Yellow
    Write-Host "   → Did DHCP assign a different IP address?" -ForegroundColor Yellow
}

Write-Host "`n=== SUMMARY ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Expected connections in backend logs:" -ForegroundColor White
Write-Host "  ✓ 10.75.11.50:xxxxx - `"POST /api/v1/telemetry/ingest HTTP/1.1`" 200 OK" -ForegroundColor Green
Write-Host ""
Write-Host "If you only see 127.0.0.1 connections:" -ForegroundColor White
Write-Host "  1. Stop backend (Ctrl+C)" -ForegroundColor Yellow
Write-Host "  2. Run: python -m uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload" -ForegroundColor Cyan
Write-Host "  3. Check Warden serial monitor for Wi-Fi connection" -ForegroundColor Yellow
Write-Host "  4. Hard refresh frontend (Ctrl+Shift+R)" -ForegroundColor Yellow
Write-Host ""
Write-Host "For detailed troubleshooting, see:" -ForegroundColor White
Write-Host "  E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\VERIFY_WARDEN_CONNECTION.md" -ForegroundColor Cyan
Write-Host ""
