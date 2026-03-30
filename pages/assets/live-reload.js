(function () {
  const host = window.location.hostname;
  if (host !== "127.0.0.1" && host !== "localhost") {
    return;
  }

  let lastToken = null;

  async function pollReloadToken() {
    try {
      const response = await fetch("./.preview-reload?ts=" + Date.now(), {
        cache: "no-store",
      });
      if (!response.ok) {
        return;
      }

      const token = (await response.text()).trim();
      if (lastToken !== null && token !== lastToken) {
        window.location.reload();
        return;
      }

      lastToken = token;
    } catch (_error) {
    }
  }

  pollReloadToken();
  window.setInterval(pollReloadToken, 1000);
})();
