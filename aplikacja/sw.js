/**
 * Offline shell for the EPI app. Navigation is network-first and the app's own
 * files are revalidated in the background, so a cached copy is shown at once
 * but a deployed fix reaches returning visitors on their next visit rather
 * than being pinned forever. Bump CACHE to drop everything held by an older
 * version of this worker.
 */
const CACHE = 'epi-app-v2';
const SHELL = [
  './',
  './index.html',
  './app.css',
  './app.js',
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

  if (request.mode === 'navigate') {
    event.respondWith(
      fetch(request)
        .then((response) => {
          const copy = response.clone();
          caches.open(CACHE).then((cache) => cache.put('./index.html', copy));
          return response;
        })
        .catch(() => caches.match('./index.html'))
    );
    return;
  }

  event.respondWith(
    caches.match(request).then((cached) => {
      const sameOrigin = new URL(request.url).origin === self.location.origin;
      const fetched = fetch(request).then((response) => {
        if (response.ok && sameOrigin) {
          const copy = response.clone();
          caches.open(CACHE).then((cache) => cache.put(request, copy));
        }
        return response;
      });
      // Serve the cached copy immediately, refresh it for the next load.
      if (cached) {
        event.waitUntil(fetched.catch(() => {}));
        return cached;
      }
      return fetched;
    })
  );
});
