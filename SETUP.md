# Setup Guide - Smart Energy Monitor

## Quick Start

### 1. Start the Backend Server

The backend server receives data from your STM32 and serves it to the web dashboard.

**Option A: Using the batch file (Windows)**
```bash
cd server
start-server.bat
```

**Option B: Manual setup**
```bash
cd server
npm install
node server.js
```

The server will run on `http://localhost:3000`

### 2. Start the Web Server (for viewing the dashboard)

In a **separate terminal**, start the web server:

```bash
cd web
python -m http.server 8000
```

Then open: `http://localhost:8000`

**OR** you can access the dashboard directly through the backend server at: `http://localhost:3000`

### 3. Configure STM32

Update your STM32 code to send data to:
- **URL:** `http://localhost:3000/api/energy` (or your computer's IP address)
- **Method:** POST
- **Format:** `{"t":1234,"pA":500,"pB":500,"fan":true}`

## Data Flow

1. **STM32** → Sends JSON to `/api/energy` every 500ms
   - `pA`: Power A in mW
   - `pB`: Power B in mW  
   - `fan`: true when both cross threshold, false otherwise

2. **Backend Server** → Receives data, stores it, serves to web app

3. **Web Dashboard** → Fetches data from `/status` every 500ms
   - Displays Fan A and Fan B power in mW
   - Shows individual fan states (ON/OFF based on threshold)
   - Shows overall fan control state (when both cross threshold)

## Web Dashboard Features

- **Total Power**: Sum of Fan A + Fan B (in mW)
- **Fan A**: Individual power and state
- **Fan B**: Individual power and state  
- **Fan Control**: Overall state (ON when both cross threshold)
- **Real-time Chart**: Shows total power over time
- **Settings**: Configure thresholds (in mW)

## Troubleshooting

- **Dashboard shows "offline"**: Make sure the backend server is running on port 3000
- **No data appearing**: Check that STM32 is sending to correct endpoint
- **Wrong IP address**: Update `web/js/config.js` with your server's IP if not using localhost

