// app.js

import { BASE_URL } from "./config.js";

const els = {
  statusDot: document.getElementById("statusDot"),
  totalPower: document.getElementById("totalPower"),
  energyToday: document.getElementById("energyToday"),
  fanAPower: document.getElementById("fanAPower"),
  fanAState: document.getElementById("fanAState"),
  fanBPower: document.getElementById("fanBPower"),
  fanBState: document.getElementById("fanBState"),
  fanControlState: document.getElementById("fanControlState"),
  alertBar: document.getElementById("alertBar"),
};

let chart;
let cumulativeEnergy = 0; // Energy in mWh
let lastUpdateTime = null;

const maxPoints = 120; // ~60s if we refresh every 500ms
const UPDATE_INTERVAL_MS = 500; // Update every 500ms

// === notifications / vibration for alerts ===

async function ensureNotifyPermission() {
  if (!("Notification" in window)) return false;
  if (Notification.permission === "granted") return true;
  if (Notification.permission !== "denied") {
    const res = await Notification.requestPermission();
    return res === "granted";
  }
  return false;
}

function notifyAlert(title, body) {
  if ("Notification" in window && Notification.permission === "granted") {
    new Notification(title, {
      body,
      icon: "icons/icon-192.png",
    });
  }
  if ("vibrate" in navigator) {
    navigator.vibrate([100, 60, 100]);
  }
}

// === chart ===

function initChart() {
  const ctx = document.getElementById("powerChart").getContext("2d");
  chart = new Chart(ctx, {
    type: "line",
    data: {
      labels: [],
      datasets: [
        {
          label: "Total Power (mW)",
          data: [],
          tension: 0.25,
          borderColor: "#5eead4",
          backgroundColor: "rgba(94, 234, 212, 0.1)",
          yAxisID: 'y',
        },
        {
          label: "Cumulative Energy (mWh)",
          data: [],
          tension: 0.25,
          borderColor: "#f59e0b",
          backgroundColor: "rgba(245, 158, 11, 0.1)",
          yAxisID: 'y1',
        },
      ],
    },
    options: {
      animation: false,
      responsive: true,
      interaction: {
        mode: 'index',
        intersect: false,
      },
      scales: {
        x: { display: false },
        y: { 
          type: 'linear',
          display: true,
          position: 'left',
          beginAtZero: true,
          title: {
            display: true,
            text: "Power (mW)",
            color: "#5eead4"
          },
          ticks: {
            color: "#5eead4"
          }
        },
        y1: {
          type: 'linear',
          display: true,
          position: 'right',
          beginAtZero: true,
          title: {
            display: true,
            text: "Energy (mWh)",
            color: "#f59e0b"
          },
          ticks: {
            color: "#f59e0b"
          },
          grid: {
            drawOnChartArea: false,
          },
        },
      },
    },
  });
}

// === UI helpers ===

function setOnline(ok) {
  els.statusDot.classList.toggle("online", ok);
  els.statusDot.classList.toggle("offline", !ok);
}

function toast(msg) {
  els.toast.textContent = msg;
  els.toast.classList.add("show");
  setTimeout(() => els.toast.classList.remove("show"), 1500);
}

// === API calls ===

async function fetchStatus() {
  try {
    const res = await fetch(`${BASE_URL}/status`, { cache: "no-store" });
    if (!res.ok) throw new Error(res.statusText);
    const j = await res.json();
    setOnline(true);

    // Totals (in mW)
    const totals = j.totals || { total_power: 0, energy_today_kWh: 0 };
    const totalPower = totals.total_power ?? 0; // Already in mW
    const energyToday = totals.energy_today_kWh ?? 0;
    els.totalPower.textContent = `${Math.round(totalPower)} mW`;
    els.energyToday.textContent = energyToday.toFixed(3);

    // Per-load data
    const loads = j.loads || {};
    const fanA = loads.fanA || {};
    const fanB = loads.fanB || {};
    const fanControl = j.fan || {};

    // Fan A (in mW)
    const fanAPower = fanA.power ?? 0;
    els.fanAPower.textContent = `${Math.round(fanAPower)} mW`;
    els.fanAState.textContent = fanA.state || "OFF";

    // Fan B (in mW)
    const fanBPower = fanB.power ?? 0;
    els.fanBPower.textContent = `${Math.round(fanBPower)} mW`;
    els.fanBState.textContent = fanB.state || "OFF";

    // Fan Control State (overall - when both cross threshold)
    const fanControlState = fanControl.state || "OFF";
    els.fanControlState.textContent = fanControlState;

    // Alert bar + notification
    if (j.alert && j.alert.active) {
      const msg = j.alert.message || "Alert";
      const wasHidden = els.alertBar.classList.contains("hidden");
      els.alertBar.textContent = msg;
      els.alertBar.classList.remove("hidden");
      if (wasHidden) {
        ensureNotifyPermission().then((ok) => {
          if (ok) notifyAlert("Energy Alert", msg);
        });
      }
    } else {
      els.alertBar.classList.add("hidden");
    }

    // Calculate energy (integral of power over time)
    const now = Date.now();
    if (lastUpdateTime !== null) {
      // Time delta in hours (500ms = 0.0001389 hours)
      const timeDeltaHours = UPDATE_INTERVAL_MS / (1000 * 60 * 60);
      // Energy = Power * Time (in mWh)
      cumulativeEnergy += totalPower * timeDeltaHours;
    }
    lastUpdateTime = now;

    // Chart update
    chart.data.labels.push("");
    chart.data.datasets[0].data.push(totalPower); // Power in mW
    chart.data.datasets[1].data.push(cumulativeEnergy); // Energy in mWh
    
    if (chart.data.labels.length > maxPoints) {
      chart.data.labels.shift();
      chart.data.datasets[0].data.shift();
      chart.data.datasets[1].data.shift();
    }
    chart.update();
  } catch (err) {
    setOnline(false);
    // Optional: show offline message
    // els.alertBar.textContent = "Disconnected from board…";
    // els.alertBar.classList.remove("hidden");
  }
}

async function postControl(body) {
  try {
    const res = await fetch(`${BASE_URL}/control`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    const j = await res.json().catch(() => ({}));
    if (j.status === "OK") toast(j.message || "OK");
    else toast(j.message || "Error");
  } catch {
    toast("Failed to send command");
  }
}

// === event handlers ===
// (No settings controls needed - STM32 handles its own thresholds)

// === boot ===

initChart();
fetchStatus();
setInterval(fetchStatus, 500); // 2 Hz UI updates

