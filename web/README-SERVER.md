# How to Run the Web App

## Quick Start - Python (Easiest)

If you have Python installed:

1. Open a terminal/command prompt
2. Navigate to the `web` folder:
   ```bash
   cd web
   ```
3. Run the server:
   ```bash
   python -m http.server 8000
   ```
   Or double-click `start-server.bat`

4. Open your browser and go to:
   ```
   http://localhost:8000
   ```

## Option 2: Node.js http-server

If you have Node.js installed:

1. Install http-server globally (one time):
   ```bash
   npm install -g http-server
   ```

2. Navigate to the `web` folder:
   ```bash
   cd web
   ```

3. Run the server:
   ```bash
   http-server -p 8000
   ```

4. Open: `http://localhost:8000`

## Option 3: VS Code Live Server

1. Install the "Live Server" extension in VS Code
2. Right-click on `index.html`
3. Select "Open with Live Server"

## Important: Backend API Server

⚠️ **The web app needs a backend server** to work properly!

The app.js file makes API calls to:
- `GET /status` - to fetch power data
- `POST /control` - to send control commands

You need to:
1. **Update `web/js/config.js`** - Set `BASE_URL` to your backend server address
2. **Create/run a backend server** that handles:
   - Receiving POST requests from your STM32 at `/api/energy`
   - Providing GET `/status` endpoint for the dashboard
   - Providing POST `/control` endpoint for control commands

### Example Backend Server (Node.js/Express)

Create a simple server in a separate folder:

```javascript
// server.js
const express = require('express');
const app = express();
app.use(express.json());

let latestData = { pA: 0, pB: 0, fan: false };

// Receive data from STM32
app.post('/api/energy', (req, res) => {
  latestData = req.body;
  res.json({ status: 'OK' });
});

// Dashboard status endpoint
app.get('/status', (req, res) => {
  res.json({
    totals: {
      total_power: (latestData.pA + latestData.pB) / 1000, // Convert mW to W
      energy_today_kWh: 0
    },
    loads: {
      fan: { power: latestData.pA / 1000, state: latestData.fan ? 'ON' : 'OFF' },
      lamp: { power: latestData.pB / 1000, state: 'OFF' },
      charger: { power: 0 }
    }
  });
});

app.listen(3000, () => console.log('Server on http://localhost:3000'));
```

Then update `web/js/config.js`:
```javascript
export const BASE_URL = "http://localhost:3000";
```

## Testing Without Backend

The web app will show "offline" status if it can't connect to the backend, but you can still see the UI structure.

