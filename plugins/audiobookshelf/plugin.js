CrossPoint.registerPlugin(async (container, api) => {
  const b64Utf8 = (text) => {
    const bytes = new TextEncoder().encode(text);
    let bin = '';
    for (let i = 0; i < bytes.length; i++) bin += String.fromCharCode(bytes[i]);
    return btoa(bin);
  };

  const readJson = async (filename) => {
    try {
      const r = await fetch(api.pluginFile(filename), { cache: 'no-store' });
      if (!r.ok) return null;
      return await r.json();
    } catch (e) {
      return null;
    }
  };

  const cfg = (await readJson('config.json')) || {};
  const tokObj = (await readJson('token.json')) || {};

  const el = (tag, attrs, ...children) => {
    const node = document.createElement(tag);
    if (attrs) {
      for (const [k, v] of Object.entries(attrs)) {
        if (k === 'style') node.setAttribute('style', v);
        else if (k in node) node[k] = v;
        else node.setAttribute(k, v);
      }
    }
    for (const c of children) {
      if (c != null) node.append(typeof c === 'string' ? document.createTextNode(c) : c);
    }
    return node;
  };

  const statusLine = el('p', { style: 'margin:0.5rem 0;font-size:0.95em;' },
    tokObj.token ? 'Token: configured on SD card' : 'Token: not signed in yet');

  const urlInput = el('input', {
    type: 'url', placeholder: 'https://abs.example.com',
    value: cfg.server_url || '', style: 'width:100%;margin-bottom:0.5rem;'
  });
  const destInput = el('input', {
    type: 'text', placeholder: '/Books/Audiobookshelf',
    value: cfg.dest_dir || '/Books/Audiobookshelf', style: 'width:100%;margin-bottom:0.75rem;'
  });
  const userInput = el('input', {
    type: 'text', placeholder: 'Username',
    value: cfg.username || '', style: 'width:100%;margin-bottom:0.5rem;'
  });
  const hasSavedPassword = Boolean(cfg.has_password || cfg.password);
  const passInput = el('input', {
    type: 'password',
    placeholder: hasSavedPassword ? 'Password saved on SD (leave blank to keep)' : 'Password (leave blank to keep API token)',
    value: '', style: 'width:100%;margin-bottom:0.5rem;'
  });
  const rememberCheck = el('input', {
    type: 'checkbox',
    checked: hasSavedPassword
  });
  const apiTokenInput = el('input', {
    type: 'password', placeholder: 'Or paste an Audiobookshelf API Token',
    value: '', style: 'width:100%;margin-bottom:0.5rem;'
  });

  const setStatus = (msg, isError) => {
    statusLine.textContent = msg;
    statusLine.style.color = isError ? '#c0392b' : '#2e7d32';
  };

  const normalizeServerUrl = (raw) => raw.trim().replace(/\/+$/, '');
  const normalizeDirPath = (raw, fallback) => {
    const trimmed = String(raw || '').trim();
    if (!trimmed) return fallback;
    if (/^\/+$/.test(trimmed)) return '/';
    const noTrailing = trimmed.replace(/\/+$/, '');
    return noTrailing.startsWith('/') ? noTrailing : '/' + noTrailing;
  };
  const normalizeDestDir = (raw) => normalizeDirPath(raw, '/Books/Audiobookshelf');
  const normalizeScanRoot = (raw) => normalizeDirPath(raw, '/Books');

  const writeConfig = async (serverUrl, destDir, username, passwordOpts) => {
    const config = { server_url: serverUrl, dest_dir: destDir };
    if (username) config.username = username;
    else config.username = null;

    if (typeof api.saveConfig === 'function') {
      const payload = { config };
      if (passwordOpts && passwordOpts.password) {
        payload.secrets = { password: passwordOpts.password };
      } else if (passwordOpts && passwordOpts.preserve) {
        payload.preserveSecrets = ['password'];
      } else {
        payload.clearSecrets = ['password'];
      }
      const res = await api.saveConfig(payload);
      const savedHasPw = Boolean(res && res.has_password);
      passInput.placeholder = savedHasPw
        ? 'Password saved on SD (leave blank to keep)'
        : 'Password (leave blank to keep API token)';
      return;
    }

    if (passwordOpts && (passwordOpts.password || passwordOpts.preserve)) {
      throw new Error('Firmware 1.0.6+ is required to save protected passwords on SD.');
    }
    const fallback = { server_url: serverUrl, dest_dir: destDir };
    if (username) fallback.username = username;
    await api.writeFile(api.dir + '/config.json', b64Utf8(JSON.stringify(fallback)));
  };

  const saveAndSignIn = async () => {
    const serverUrl = normalizeServerUrl(urlInput.value);
    const destDir = normalizeDestDir(destInput.value);
    const username = userInput.value.trim();
    const password = passInput.value;
    const pastedToken = apiTokenInput.value.trim();

    if (!serverUrl) {
      setStatus('Enter your Audiobookshelf Server URL.', true);
      return;
    }

    try {
      let token = pastedToken;
      if (!token) {
        if (!username || !password) {
          if (tokObj.token) {
            await writeConfig(serverUrl, destDir, username || cfg.username || '',
              { preserve: rememberCheck.checked });
            setStatus('Saved settings (kept existing token' +
              (rememberCheck.checked ? '' : ', cleared saved password') + ').', false);
            return;
          }
          setStatus('Enter username + password or paste an API token.', true);
          return;
        }
        setStatus('Signing in to Audiobookshelf...', false);
        const res = await api.relay('POST', serverUrl + '/login',
          { 'Content-Type': 'application/json' },
          JSON.stringify({ username, password }));
        if (res.status < 200 || res.status >= 300) {
          setStatus('Login failed (HTTP ' + res.status + ').', true);
          return;
        }
        const parsed = JSON.parse(res.body || '{}');
        token = parsed && parsed.user && parsed.user.token;
        if (!token) {
          setStatus('Login response did not include user.token.', true);
          return;
        }
        await writeConfig(serverUrl, destDir, username,
          rememberCheck.checked ? { password } : { clear: true });
      } else {
        await writeConfig(serverUrl, destDir, username, { clear: true });
      }

      await api.writeFile(api.dir + '/token.json', b64Utf8(JSON.stringify({ token })));
      tokObj.token = token;
      passInput.value = '';
      apiTokenInput.value = '';
      setStatus('Saved and authenticated with Audiobookshelf.', false);
    } catch (e) {
      setStatus('Error: ' + ((e && e.message) || e), true);
    }
  };

  const testConnection = async () => {
    const serverUrl = normalizeServerUrl(urlInput.value || cfg.server_url || '');
    const latestTok = (await readJson('token.json')) || tokObj;
    if (!serverUrl || !latestTok.token) {
      setStatus('Configure Server URL and sign in before testing.', true);
      return;
    }
    try {
      setStatus('Testing connection...', false);
      const res = await api.relay('GET', serverUrl + '/api/libraries',
        { Authorization: 'Bearer ' + latestTok.token }, '');
      if (res.status < 200 || res.status >= 300) {
        setStatus('Connection check failed (HTTP ' + res.status + ').', true);
        return;
      }
      const data = JSON.parse(res.body || '{}');
      const libs = Array.isArray(data.libraries) ? data.libraries : [];
      const names = libs.map((l) => l.name).filter(Boolean).join(', ');
      setStatus('Connected: ' + libs.length + ' librar' + (libs.length === 1 ? 'y' : 'ies') +
        (names ? ' (' + names + ')' : ''), false);
    } catch (e) {
      setStatus('Test failed: ' + ((e && e.message) || e), true);
    }
  };

  const saveBtn = el('button', { type: 'button', onclick: saveAndSignIn }, 'Save & Sign In');
  const testBtn = el('button', { type: 'button', onclick: testConnection, style: 'margin-left:0.5rem;' }, 'Test Connection');

  const scanRootInput = el('input', {
    type: 'text', placeholder: '/Books (or / for whole SD card)',
    value: '/Books', style: 'width:100%;margin-bottom:0.5rem;'
  });
  const syncStatusLine = el('p', { style: 'margin:0.5rem 0;font-size:0.95em;' }, '');
  const syncListEl = el('div', { style: 'margin-top:0.5rem;' });
  const saveLinksBtn = el('button', {
    type: 'button', hidden: true, style: 'margin-top:0.5rem;'
  }, 'Save Selected Links');

  const setSyncStatus = (msg, isError) => {
    syncStatusLine.textContent = msg;
    syncStatusLine.style.color = isError ? '#c0392b' : '#2e7d32';
  };

  const normalizeText = (s) => String(s || '')
    .toLowerCase()
    .replace(/\.epub$/i, '')
    .replace(/[^a-z0-9]+/g, ' ')
    .trim();

  const tokenize = (s) => {
    const norm = normalizeText(s);
    return norm ? norm.split(' ').filter((w) => w.length > 1) : [];
  };

  const scoreMatch = (epubName, book) => {
    const fileNorm = normalizeText(epubName);
    const titleNorm = normalizeText(book.title);
    if (!fileNorm || !titleNorm) return 0;
    if (fileNorm === titleNorm) return 1.0;
    if (fileNorm.includes(titleNorm) || titleNorm.includes(fileNorm)) {
      const ratio = Math.min(fileNorm.length, titleNorm.length) / Math.max(fileNorm.length, titleNorm.length);
      return 0.72 + 0.25 * ratio;
    }
    const fTokens = new Set(tokenize(epubName));
    const tTokens = tokenize(book.title);
    if (!fTokens.size || !tTokens.length) return 0;
    let hits = 0;
    for (const t of tTokens) {
      if (fTokens.has(t)) hits++;
    }
    const titleCoverage = hits / tTokens.length;
    const authorTokens = tokenize(book.author);
    let authorHits = 0;
    for (const a of authorTokens) {
      if (fTokens.has(a)) authorHits++;
    }
    const authorBonus = authorTokens.length ? 0.1 * (authorHits / authorTokens.length) : 0;
    return titleCoverage * 0.85 + authorBonus;
  };

  const bestAbsMatch = (epubName, absBooks) => {
    let best = null;
    let bestScore = 0;
    for (const b of absBooks) {
      const s = scoreMatch(epubName, b);
      if (s > bestScore) {
        bestScore = s;
        best = b;
      }
    }
    return bestScore >= 0.55 ? { id: best.id, score: bestScore } : { id: '', score: 0 };
  };

  const listDir = async (dirPath) => {
    const r = await fetch('/api/files?path=' + encodeURIComponent(dirPath), { cache: 'no-store' });
    if (!r.ok) return [];
    const data = await r.json();
    return Array.isArray(data) ? data : [];
  };

  const readSidecar = async (fullMetaPath) => {
    try {
      const r = await fetch('/download?path=' + encodeURIComponent(fullMetaPath), { cache: 'no-store' });
      if (!r.ok) return {};
      const parsed = await r.json();
      return parsed && typeof parsed === 'object' ? parsed : {};
    } catch (e) {
      return {};
    }
  };

  const scanSdEpubs = async (rootDir, onProgress) => {
    const normRoot = normalizeScanRoot(rootDir);
    const results = [];
    const queue = [{ path: normRoot, depth: 0 }];
    const MAX_DEPTH = 6;
    while (queue.length) {
      const cur = queue.shift();
      if (onProgress) onProgress('Scanning SD folder ' + cur.path + ' (' + results.length + ' EPUBs found)...');
      const entries = await listDir(cur.path);
      const fileNames = new Set(entries.filter((e) => !e.isDirectory).map((e) => e.name));
      for (const entry of entries) {
        if (entry.isDirectory) {
          if (cur.depth < MAX_DEPTH && entry.name && !entry.name.startsWith('.')) {
            const sub = (cur.path === '/' ? '' : cur.path) + '/' + entry.name;
            queue.push({ path: sub, depth: cur.depth + 1 });
          }
          continue;
        }
        if (!entry.isEpub && !/\.epub$/i.test(entry.name)) continue;
        const fullPath = (cur.path === '/' ? '' : cur.path) + '/' + entry.name;
        const metaName = entry.name + '.meta.json';
        const hasSidecar = fileNames.has(metaName);
        const existingMeta = hasSidecar ? await readSidecar(fullPath + '.meta.json') : {};
        results.push({
          dirPath: cur.path,
          name: entry.name,
          fullPath,
          hasSidecar,
          existingMeta,
          existingAbsId: String(existingMeta.abs_item_id || '')
        });
      }
    }
    return results;
  };

  const fetchBookLibraries = async (serverUrl, token) => {
    const libRes = await api.relay('GET', serverUrl + '/api/libraries', { Authorization: 'Bearer ' + token }, '');
    if (libRes.status === 401 || libRes.status === 403) {
      throw new Error('Audiobookshelf rejected token (HTTP ' + libRes.status + '). Click Save & Sign In again.');
    }
    if (libRes.status < 200 || libRes.status >= 300) {
      throw new Error('Failed to fetch libraries (HTTP ' + libRes.status + ')');
    }
    const libsData = JSON.parse(libRes.body || '{}');
    const libs = Array.isArray(libsData.libraries) ? libsData.libraries : [];
    return libs.filter((lib) => !lib.mediaType || lib.mediaType === 'book');
  };

  const searchQueriesForFile = (filename) => {
    const stem = String(filename || '')
      .replace(/\.epub$/i, '')
      .replace(/\[[^\]]*\]|\([^)]*\)/g, ' ')
      .replace(/_/g, ' ')
      .replace(/\s+/g, ' ')
      .trim();
    if (!stem) return [];
    const queries = [];
    const pushUnique = (q) => {
      const cleaned = q.replace(/\s+/g, ' ').trim();
      if (cleaned && !queries.includes(cleaned)) queries.push(cleaned);
    };
    const dashParts = stem.split(/\s+[-–—]\s+/).filter((p) => p && !/^\d+$/.test(p));
    if (dashParts.length > 1) {
      // Often "Author - Title" or "01 - Title"; longest part or trailing title searches best.
      dashParts.slice().sort((a, b) => b.length - a.length).forEach(pushUnique);
    }
    pushUnique(stem.replace(/[-–—]+/g, ' '));
    const words = stem.replace(/[^a-zA-Z0-9\s]/g, ' ').split(/\s+/).filter((w) => w.length > 1);
    if (words.length > 4) pushUnique(words.slice(0, 4).join(' '));
    return queries.slice(0, 3);
  };

  const searchAbsBooks = async (serverUrl, token, libs, query) => {
    const q = String(query || '').trim();
    if (!q) return [];
    const hits = [];
    const seen = new Set();
    for (const lib of libs) {
      let limit = 6;
      let res;
      for (;;) {
        const url = serverUrl + '/api/libraries/' + encodeURIComponent(lib.id) +
          '/search?q=' + encodeURIComponent(q) + '&limit=' + limit;
        try {
          res = await api.relay('GET', url, { Authorization: 'Bearer ' + token }, '');
          break;
        } catch (e) {
          if (limit > 3) { limit = 3; continue; }
          if (limit > 1) { limit = 1; continue; }
          throw e;
        }
      }
      if (res.status === 401 || res.status === 403) {
        throw new Error('Audiobookshelf rejected token (HTTP ' + res.status + '). Click Save & Sign In again.');
      }
      if (res.status < 200 || res.status >= 300) continue;
      const data = JSON.parse(res.body || '{}');
      const rows = Array.isArray(data.book) ? data.book : [];
      for (const entry of rows) {
        const item = (entry && entry.libraryItem) || entry;
        const media = item && item.media;
        const id = String((item && item.id) || '');
        if (!id || seen.has(id) || !media || (!media.ebookFile && !media.ebookFormat)) continue;
        seen.add(id);
        const meta = media.metadata || {};
        hits.push({
          id,
          title: String(meta.title || id || 'Untitled'),
          author: String(meta.authorName || ''),
          libraryName: String(lib.name || '')
        });
      }
    }
    return hits;
  };

  let scannedRows = [];

  const scanAndMatch = async () => {
    const serverUrl = normalizeServerUrl(urlInput.value || cfg.server_url || '');
    const latestTok = (await readJson('token.json')) || tokObj;
    if (!serverUrl || !latestTok.token) {
      setSyncStatus('Configure Server URL and sign in first.', true);
      return;
    }
    scanBtn.disabled = true;
    scanBtn.textContent = 'Scanning...';
    saveLinksBtn.hidden = true;
    syncListEl.replaceChildren();
    scannedRows = [];
    try {
      setSyncStatus('Scanning SD card EPUBs...', false);
      const sdBooks = await scanSdEpubs(scanRootInput.value, (msg) => setSyncStatus(msg, false));
      if (!sdBooks.length) {
        setSyncStatus('No EPUB files found under ' + normalizeScanRoot(scanRootInput.value) + '.', true);
        return;
      }
      setSyncStatus('Found ' + sdBooks.length + ' SD EPUB(s). Reading Audiobookshelf libraries...', false);
      const libs = await fetchBookLibraries(serverUrl, latestTok.token);
      if (!libs.length) {
        setSyncStatus('No book libraries found in Audiobookshelf.', true);
        return;
      }

      const unlinkedCount = sdBooks.filter((sd) => !sd.existingAbsId).length;
      const listFragment = document.createDocumentFragment();
      let autoMatchedCount = 0;
      let linkedCount = 0;
      let searchedIdx = 0;

      for (const sd of sdBooks) {
        const isLinked = Boolean(sd.existingAbsId);
        let candidates = [];
        let match = { id: '', score: 0 };

        if (isLinked) {
          linkedCount++;
          match = { id: sd.existingAbsId, score: 1 };
        } else {
          searchedIdx++;
          const byId = new Map();
          const queries = searchQueriesForFile(sd.name);
          for (const q of queries) {
            setSyncStatus('Searching ABS (' + searchedIdx + '/' + unlinkedCount + '): ' + q + '...', false);
            const found = await searchAbsBooks(serverUrl, latestTok.token, libs, q);
            for (const book of found) {
              if (!byId.has(book.id)) byId.set(book.id, book);
            }
            const cur = bestAbsMatch(sd.name, Array.from(byId.values()));
            if (cur.score >= 0.72) {
              match = cur;
              break;
            }
          }
          candidates = Array.from(byId.values());
          if (!match.id) match = bestAbsMatch(sd.name, candidates);
          if (match.id) autoMatchedCount++;
        }

        const cb = el('input', { type: 'checkbox', checked: !isLinked && Boolean(match.id) });
        const badge = el('span', {
          style: 'font-size:0.8em;padding:2px 6px;border-radius:4px;margin-left:0.5rem;' +
            (isLinked ? 'background:#e8f5e9;color:#2e7d32;' :
              match.id ? 'background:#e3f2fd;color:#1565c0;' : 'background:#f5f5f5;color:#666;')
        }, isLinked ? 'Linked' : (match.id ? 'Matched' : 'Unmatched'));

        const select = el('select', { style: 'width:100%;margin-top:0.25rem;' });
        const populateSelect = (booksList, selectedId) => {
          select.replaceChildren(el('option', { value: '' }, '-- Do not link --'));
          const seenOpt = new Set();
          if (sd.existingAbsId) {
            const existingFound = booksList.find((b) => b.id === sd.existingAbsId);
            if (!existingFound) {
              select.appendChild(el('option', { value: sd.existingAbsId }, 'Current ABS link (' + sd.existingAbsId + ')'));
              seenOpt.add(sd.existingAbsId);
            }
          }
          for (const ab of booksList) {
            if (seenOpt.has(ab.id)) continue;
            seenOpt.add(ab.id);
            const label = ab.title + (ab.author ? ' — ' + ab.author : '') + (ab.libraryName ? ' [' + ab.libraryName + ']' : '');
            select.appendChild(el('option', { value: ab.id }, label));
          }
          select.value = selectedId || '';
        };
        populateSelect(candidates, match.id);
        select.addEventListener('change', () => {
          cb.checked = Boolean(select.value) && select.value !== sd.existingAbsId;
        });

        const defaultQ = searchQueriesForFile(sd.name)[0] || sd.name.replace(/\.epub$/i, '');
        const rowSearchInput = el('input', {
          type: 'text', value: defaultQ, placeholder: 'Search Audiobookshelf title/author',
          style: 'flex:1;min-width:0;'
        });
        const findBtn = el('button', { type: 'button', style: 'margin-left:0.4rem;' }, 'Find');
        findBtn.onclick = async () => {
          const q = rowSearchInput.value.trim();
          if (!q) return;
          findBtn.disabled = true;
          findBtn.textContent = '...';
          try {
            const found = await searchAbsBooks(serverUrl, latestTok.token, libs, q);
            const best = bestAbsMatch(sd.name, found);
            const pickId = best.id || (found[0] ? found[0].id : '');
            populateSelect(found, pickId);
            cb.checked = Boolean(select.value) && select.value !== sd.existingAbsId;
            if (!isLinked) {
              badge.textContent = found.length ? 'Searched (' + found.length + ')' : 'No ABS matches';
            }
          } catch (err) {
            setSyncStatus('Search failed: ' + ((err && err.message) || err), true);
          } finally {
            findBtn.disabled = false;
            findBtn.textContent = 'Find';
          }
        };

        const rowDiv = el('div', {
          style: 'border:1px solid var(--border-color,#ddd);border-radius:6px;padding:0.5rem;margin-bottom:0.5rem;'
        },
          el('label', { style: 'display:flex;align-items:center;font-weight:600;word-break:break-word;' },
            cb, el('span', { style: 'margin-left:0.4rem;' }, sd.fullPath), badge),
          select,
          el('div', { style: 'display:flex;align-items:center;margin-top:0.3rem;' }, rowSearchInput, findBtn)
        );
        listFragment.appendChild(rowDiv);
        scannedRows.push({ sd, cb, select, badge });
      }

      syncListEl.replaceChildren(listFragment);
      saveLinksBtn.hidden = false;
      setSyncStatus('Done: ' + sdBooks.length + ' SD EPUB(s) checked (' +
        linkedCount + ' linked, ' + autoMatchedCount + ' auto-matched).', false);
    } catch (e) {
      setSyncStatus('Scan failed: ' + ((e && e.message) || e), true);
    } finally {
      scanBtn.disabled = false;
      scanBtn.textContent = 'Scan SD Books & Audiobookshelf';
    }
  };

  const saveSelectedLinks = async () => {
    const targets = scannedRows.filter((r) => r.cb.checked && r.select.value);
    if (!targets.length) {
      setSyncStatus('Select at least one book with a matching Audiobookshelf item.', true);
      return;
    }
    scanBtn.disabled = true;
    saveLinksBtn.disabled = true;
    let saved = 0;
    try {
      for (const row of targets) {
        setSyncStatus('Writing sidecar ' + (saved + 1) + '/' + targets.length + ' (' + row.sd.name + ')...', false);
        const payload = Object.assign({}, row.sd.existingMeta, { abs_item_id: row.select.value });
        const form = new FormData();
        form.append('file', new Blob([JSON.stringify(payload)], { type: 'application/json' }), row.sd.name + '.meta.json');
        const res = await fetch('/upload?path=' + encodeURIComponent(row.sd.dirPath) + '&overwrite=1', {
          method: 'POST',
          body: form
        });
        if (!res.ok) {
          throw new Error('Failed writing sidecar for ' + row.sd.fullPath + ' (HTTP ' + res.status + ')');
        }
        row.sd.existingMeta = payload;
        row.sd.existingAbsId = row.select.value;
        row.cb.checked = false;
        row.badge.textContent = 'Linked';
        row.badge.setAttribute('style', 'font-size:0.8em;padding:2px 6px;border-radius:4px;margin-left:0.5rem;background:#e8f5e9;color:#2e7d32;');
        saved++;
      }
      setSyncStatus('Linked ' + saved + ' book(s). Open them on the device and choose Sync Progress.', false);
    } catch (e) {
      setSyncStatus('Error saving sidecars: ' + ((e && e.message) || e), true);
    } finally {
      scanBtn.disabled = false;
      saveLinksBtn.disabled = false;
    }
  };

  const scanBtn = el('button', { type: 'button', onclick: scanAndMatch }, 'Scan SD Books & Audiobookshelf');
  saveLinksBtn.onclick = saveSelectedLinks;

  container.replaceChildren(
    el('h2', null, 'Audiobookshelf'),
    el('p', { style: 'margin-top:0;' }, 'Browse, download EPUBs, and sync progress directly with your Audiobookshelf server.'),
    el('label', null, 'Server URL', urlInput),
    el('label', null, 'Download Folder on SD', destInput),
    el('h3', { style: 'margin:0.5rem 0 0.25rem;' }, 'Sign In (Username & Password)'),
    el('label', null, 'Username', userInput),
    el('label', null, 'Password', passInput),
    el('label', { style: 'display:block;margin-bottom:0.75rem;' }, rememberCheck, ' Save password protected on SD for automatic re-auth'),
    el('h3', { style: 'margin:0.5rem 0 0.25rem;' }, 'Or API Token'),
    el('label', null, 'API Token (preferred if you want no password on SD)', apiTokenInput),
    el('div', { style: 'margin-top:0.5rem;' }, saveBtn, testBtn),
    statusLine,
    el('div', { style: 'margin-top:1.25rem;border-top:1px solid var(--border-color,#ddd);padding-top:1rem;' },
      el('h3', { style: 'margin-top:0;' }, 'Sync Existing SD Card Books'),
      el('p', { style: 'margin-top:0;font-size:0.95em;' },
        'Link EPUBs already on your SD card to Audiobookshelf items so two-way Sync Progress and auto-sync work without re-downloading them.'),
      el('label', null, 'SD Folder to Scan', scanRootInput),
      scanBtn,
      syncStatusLine,
      syncListEl,
      saveLinksBtn
    )
  );
});
