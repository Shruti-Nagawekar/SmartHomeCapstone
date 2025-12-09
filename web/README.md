# Web Dashboard Interface

This folder contains the web interface files for displaying power monitoring data from the STM32 embedded system.

## Folder Structure

```
web/
├── index.html          # Main dashboard HTML page
├── css/
│   └── style.css       # Stylesheet for the dashboard
├── js/
│   └── dashboard.js    # JavaScript for fetching and displaying data
└── README.md           # This file
```

## File Organization

- **`index.html`** - Main HTML page that displays the dashboard
- **`css/style.css`** - All CSS styling for the dashboard
- **`js/dashboard.js`** - JavaScript code that:
  - Fetches data from the web server API endpoint (`/api/energy`)
  - Updates the display with power A and power B values
  - Handles real-time data updates

## How to Use

1. **Place your HTML file** in `web/index.html`
2. **Place your CSS file** in `web/css/style.css`
3. **Place your JavaScript file** in `web/js/dashboard.js`

4. **Update the HTML file** to reference the CSS and JS files:
   ```html
   <link rel="stylesheet" href="css/style.css">
   <script src="js/dashboard.js"></script>
   ```

5. **Deploy to your web server** - Copy the entire `web/` folder contents to your web server's document root (or appropriate directory)

## API Endpoint

The embedded system sends JSON data to:
- **Endpoint:** `/api/energy`
- **Method:** POST
- **Format:** `{"t":1234,"pA":500,"pB":500,"fan":true}`
  - `t` - Timestamp (ticks)
  - `pA` - Power A in milliwatts
  - `pB` - Power B in milliwatts
  - `fan` - Fan state (true/false)

Your web server should:
1. Receive POST requests at `/api/energy`
2. Store/process the data
3. Provide a GET endpoint for the dashboard to fetch the latest data

## Example Server Setup

If using Node.js/Express:
- Place server files in a separate `server/` folder
- Serve static files from `web/` folder
- Handle API endpoints for receiving and serving data

