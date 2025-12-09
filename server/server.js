// server.js
// Backend server for Smart Energy Monitor
// Receives data from STM32 and serves it to the web dashboard

const express = require('express');
const cors = require('cors');
const path = require('path');

const app = express();
const PORT = 3000;

// Middleware
app.use(cors());
app.use(express.json());

// Store latest sensor data
let latestData = {
  t: 0,        // timestamp
  pA: 0,       // power A in mW
  pB: 0,       // power B in mW
  fan: false   // fan state (true = ON, false = OFF)
};

// Serve static files from web folder
app.use(express.static(path.join(__dirname, '../web')));

// ===== API Endpoints =====

// Receive data from STM32
app.post('/api/energy', (req, res) => {
  const { t, pA, pB, fan } = req.body;
  
  // Update latest data
  latestData = {
    t: t || 0,
    pA: pA || 0,
    pB: pB || 0,
    fan: fan === true || fan === 1 || fan === 'true' || fan === '1'
  };
  
  console.log(`[${new Date().toISOString()}] Received: pA=${latestData.pA} mW, pB=${latestData.pB} mW, fan=${latestData.fan ? 'ON' : 'OFF'}`);
  
  res.json({ status: 'OK', message: 'Data received' });
});

// Serve status to web dashboard
app.get('/status', (req, res) => {
  const totalPower = latestData.pA + latestData.pB; // Total in mW
  const threshold = 600; // Default threshold in mW (matches STM32)
  
  // Individual fan states based on power values
  const fanAState = latestData.pA > threshold ? 'ON' : 'OFF';
  const fanBState = latestData.pB > threshold ? 'ON' : 'OFF';
  
  // Overall fan control state (from STM32 - when both cross threshold)
  const fanControlState = latestData.fan ? 'ON' : 'OFF';
  
  res.json({
    totals: {
      total_power: totalPower,  // in mW
      energy_today_kWh: 0        // TODO: implement energy accumulation
    },
    loads: {
      fanA: {
        power: latestData.pA,   // in mW
        state: fanAState
      },
      fanB: {
        power: latestData.pB,   // in mW
        state: fanBState
      }
    },
    fan: {
      state: fanControlState  // Overall fan state (when both cross threshold)
    },
    auto_control_enabled: false,
    thresholds: {
      fan_power_limit: threshold,  // in mW (default threshold)
      total_power_limit: 1200      // in mW
    }
  });
});

// Control endpoint (for future use)
app.post('/control', (req, res) => {
  const body = req.body;
  console.log('Control command received:', body);
  
  // For now, just acknowledge
  res.json({ status: 'OK', message: 'Command received' });
});

// Start server
app.listen(PORT, () => {
  console.log(`\n=== Smart Energy Monitor Server ===`);
  console.log(`Server running on http://localhost:${PORT}`);
  console.log(`Web dashboard: http://localhost:${PORT}`);
  console.log(`API endpoint: http://localhost:${PORT}/api/energy`);
  console.log(`Status endpoint: http://localhost:${PORT}/status`);
  console.log(`\nWaiting for STM32 data...\n`);
});

