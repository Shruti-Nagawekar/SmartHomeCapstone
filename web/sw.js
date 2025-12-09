// sw.js
// Service worker for PWA functionality
// Caches the UI shell; keeps API calls live over the network.

const CACHE_NAME = "energy-ui-v1";

const ASSETS = [
  "/",
  "/index.html",
  "/css/styles.css",
  "/js/app.js",
  "/js/config.js",
  "/manifest.webmanifest",
  "/sw.js",
  "/icons/icon-192.png",
  "/icons/icon-512.png",
  "https://cdn.jsdelivr.net/npm/chart.js"
];

self.addEventListener("install", (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME).then((cache) => cache.addAll(ASSETS))
  );
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(
        keys
          .filter((key) => key !== CACHE_NAME)
          .map((key) => caches.delete(key))
      )
    )
  );
});

self.addEventListener("fetch", (event) => {
  const url = new URL(event.request.url);
  // API endpoints: always try network first
  if (
    url.pathname.startsWith("/status") ||
    url.pathname.startsWith("/control") ||
    url.pathname.startsWith("/alerts")
  ) {
    event.respondWith(
      fetch(event.request).catch(() =>
        new Response(
          JSON.stringify({ offline: true }),
          {
            status: 200,
            headers: { "Content-Type": "application/json" }
          }
        )
      )
    );
    return;
  }

  // Static assets: cache-first
  event.respondWith(
    caches.match(event.request).then((cached) => cached || fetch(event.request))
  );
});

