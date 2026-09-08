/**
 * Offline shell for the EPI app.
 *
 * Same-origin requests are network-first: a deployed change reaches the
 * browser on the next load instead of being pinned by the cache, and the
 * cached copy is what answers when the network is gone. Bump CACHE to discard
 * everything an older worker stored.
 */
const CACHE = 'epi-app-v3';
const SHELL = [
  './',
  './index.html',
  './manifest.webmanifest',
  '../assets/img/epi-logo.png',
  '../assets/fonts/PlusJakartaSans-latin.woff2',
  '../assets/fonts/SpaceMono-400-latin.woff2',
  '../assets/fonts/SpaceMono-700-latin.woff2'
];

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE)
      .then((cache) => cache.addAll(SHELL))
      .then(() => self.skipWaiting())
      .catch(() => self.skipWaiting())
  );
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', (event) => {
  const request = event.request;
  if (request.method !== 'GET') return;
  if (new URL(request.url).origin !== self.location.origin) return;

  event.respondWith(
    fetch(request)
      .then((response) => {
        if (response.ok) {
          const copy = response.clone();
          caches.open(CACHE).then((cache) => cache.put(request, copy));
        }
        return response;
      })
      .catch(() => caches.match(request).then((cached) => {
        if (cached) return cached;
        // A navigation that never reached the network still gets the shell.
        return request.mode === 'navigate' ? caches.match('./index.html') : Response.error();
      }))
  );
});
