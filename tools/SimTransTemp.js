// ============================================================================
// SimTransTemp.js - SavvyCAN script to simulate transmission temperature
// Sends CAN ID 0x418 with temperature in byte 2 (Fahrenheit, 1-253)
//
// Modes:
//   autoSweep = 1 : automatic sweep from tempMinC to tempMaxC (0.5s steps)
//   autoSweep = 0 : manual control via tempManualC parameter
//
// Use host parameters panel in SavvyCAN to change values at runtime
// ============================================================================

// --- User-adjustable parameters (visible in SavvyCAN parameter panel) ---
var autoSweep  = 1;       // 1 = auto sweep, 0 = manual mode
var tempManualC = 80;     // Manual temperature (C) - used when autoSweep = 0
var tempMinC   = 40;      // Sweep start temperature (C)
var tempMaxC   = 110;     // Sweep end temperature (C)
var stepC      = 1;       // Temperature increment per tick (C)
var sweepLoop  = 1;       // 1 = loop back to min after reaching max, 0 = stop at max
var canBus     = 0;       // CAN bus index (0 = first adapter)

// --- Internal state ---
var currentTempC = 40;    // Current sweep temperature
var sweepDir     = 1;     // 1 = ascending, -1 = descending (for ping-pong mode)

function setup() {
    host.log("=== SimTransTemp v1.0 ===");
    host.log("CAN ID: 0x418 | Byte 2 = Temp (F)");
    host.log("Tick interval: 500ms");

    // Expose parameters to SavvyCAN UI
    host.addParameter("autoSweep");
    host.addParameter("tempManualC");
    host.addParameter("tempMinC");
    host.addParameter("tempMaxC");
    host.addParameter("stepC");
    host.addParameter("sweepLoop");
    host.addParameter("canBus");

    // 500ms tick = 0.5s steps
    host.setTickInterval(500);

    currentTempC = tempMinC;
}

function celsiusToFahrenheit(c) {
    return Math.round((c * 9.0 / 5.0) + 32);
}

function clampRaw(rawF) {
    if (rawF < 1)   return 1;
    if (rawF > 253) return 253;
    return rawF;
}

function sendTempFrame(tempC) {
    var rawF = clampRaw(celsiusToFahrenheit(tempC));

    // CAN ID 0x418 - Gearbox message
    // Byte 0: Gear code (0x44 = Drive)
    // Byte 1: Reserved
    // Byte 2: Transmission temperature (Fahrenheit)
    // Byte 3-7: Padding
    var data = [0x44, 0x00, rawF, 0x00, 0x00, 0x00, 0x00, 0x00];

    can.sendFrame(canBus, 0x418, 8, data);
}

function tick() {
    var tempC;

    if (autoSweep == 1) {
        // Auto sweep mode
        tempC = currentTempC;
        sendTempFrame(tempC);

        var rawF = clampRaw(celsiusToFahrenheit(tempC));
        host.log("AUTO | " + tempC.toFixed(1) + " C (" + rawF + " F) | range: "
                 + tempMinC + "-" + tempMaxC + " C");

        // Advance sweep
        currentTempC += stepC;

        if (currentTempC > tempMaxC) {
            if (sweepLoop == 1) {
                currentTempC = tempMinC; // Loop back
                host.log("--- Sweep restart ---");
            } else {
                currentTempC = tempMaxC; // Hold at max
            }
        }
    } else {
        // Manual mode - send the manual temperature
        tempC = tempManualC;
        sendTempFrame(tempC);

        var rawF = clampRaw(celsiusToFahrenheit(tempC));
        host.log("MANUAL | " + tempC.toFixed(1) + " C (" + rawF + " F)");
    }
}
