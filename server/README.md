# Backend Server

This server receives data from the STM32 and serves it to the web dashboard.

## Setup

1. Install Node.js (if not already installed)
2. Install dependencies:
   ```bash
   cd server
   npm install
   ```

## Running

```bash
npm start
```

Or:
```bash
node server.js
```

The server will:
- Listen on port 3000
- Receive POST requests from STM32 at `/api/energy`
- Serve the web dashboard at `http://localhost:3000`
- Provide status API at `/status` for the dashboard

## Configuration

Update the STM32 code to send data to:
- URL: `http://localhost:3000/api/energy` (or your server's IP)
- Method: POST
- Format: `{"t":1234,"pA":500,"pB":500,"fan":true}`

