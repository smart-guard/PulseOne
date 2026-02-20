#!/bin/bash
# Wine entrypoint with KST timezone registry fix

# Start Xvfb for headless Wine
Xvfb :99 -screen 0 1024x768x16 &
export DISPLAY=:99

# Initialize Wine prefix if first run
if [ ! -f "$WINEPREFIX/system.reg" ]; then
    echo "[Wine] Initializing Wine prefix..."
    wineboot --init 2>/dev/null
    sleep 5
fi

# Set Korea Standard Time in Wine Windows registry
# KST = UTC+9, Bias = -540 min = 0xFFFFFDA4 (as signed 32-bit)
echo "[Wine] Setting KST timezone in registry..."
wine64 reg add "HKLM\\System\\CurrentControlSet\\Control\\TimeZoneInformation" \
    /v StandardName /t REG_SZ /d "Korea Standard Time" /f 2>/dev/null || true
wine64 reg add "HKLM\\System\\CurrentControlSet\\Control\\TimeZoneInformation" \
    /v Bias /t REG_DWORD /d 0xFFFFFDA4 /f 2>/dev/null || true
wine64 reg add "HKLM\\System\\CurrentControlSet\\Control\\TimeZoneInformation" \
    /v StandardBias /t REG_DWORD /d 0x00000000 /f 2>/dev/null || true
wine64 reg add "HKLM\\System\\CurrentControlSet\\Control\\TimeZoneInformation" \
    /v DaylightBias /t REG_DWORD /d 0x00000000 /f 2>/dev/null || true
wine64 reg add "HKLM\\System\\CurrentControlSet\\Control\\TimeZoneInformation" \
    /v ActiveTimeBias /t REG_DWORD /d 0xFFFFFDA4 /f 2>/dev/null || true
echo "[Wine] Timezone set to KST (UTC+9)"

exec "$@"
