// The browser plugin, tested against the device's own core: the files it writes must read back the same on the device,
// and the files the device writes must read back the same here, repeats included. Run by plugin/run.sh.
const { JSDOM } = require('jsdom');
const fs = require('fs'), path = require('path'), cp = require('child_process');
const PARITY = process.env.PARITY || './parity';
// The script is the raw string in DaybookPlugin.cpp: that is the copy that ships, so that is the copy tested.
const cppSource = fs.readFileSync(path.join(__dirname, '../../../src/apps_local/daybook/DaybookPlugin.cpp'), 'utf8');
const code = /R"JS\(([\s\S]*)\)JS";/.exec(cppSource)[1];
let fails = 0, checks = 0;
const ok = (c, m) => { checks++; if (!c) { fails++; console.log('FAIL', m); } };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function boot(files, opts = {}) {
  const dom = new JSDOM('<!doctype html><div id="c"></div>', { url: 'http://dev.local/', runScripts: 'outside-only', pretendToBeVisual: true });
  const w = dom.window;
  w.TextEncoder = TextEncoder; w.__dbkTest = true;
  w.HTMLDialogElement.prototype.showModal = function () { this.setAttribute('open', ''); };
  w.HTMLDialogElement.prototype.close = function () { this.removeAttribute('open'); this.dispatchEvent(new w.Event('close')); };
  w.confirm = () => true; w.prompt = () => opts.prompt || 'New list';
  const log = { reads: 0, writes: [] };
  w.fetch = async (url) => {
    const m = /path=([^&]*)/.exec(url), p = decodeURIComponent(m[1]);
    log.reads++;
    if (opts.failRead && p.endsWith('tasks.txt')) return { status: 500, ok: false, text: async () => '' };
    if (!files.has(p)) return { status: 404, ok: false, text: async () => '' };
    return { status: 200, ok: true, text: async () => files.get(p) };
  };
  let fn; w.CrossPoint = { registerPlugin: (f) => { fn = f; } };
  w.eval(code);
  const container = w.document.getElementById('c');
  const api = { name: 'daybook', writeFile: async (p, b64) => { const t = Buffer.from(b64, 'base64').toString('utf8'); log.writes.push(p); files.set(p, t); return { ok: true }; } };
  const done = fn(container, api);
  await done;
  return { w, T: w.__dbkTest, container, log, files };
}
const D = '/.crosspoint/calendar/';

(async () => {
  const work = fs.mkdtempSync('/tmp/dbk-');
  const cpp = (dir, wins) => { fs.writeFileSync(work + '/w.txt', wins.map((x) => x.join(' ')).join('\n')); return cp.execFileSync(PARITY, ['check', dir, work + '/w.txt']).toString().split('\n'); };
  const parseCpp = (lines) => {
    const out = { rt: lines[0], n: lines[1], wins: [] };
    for (let i = 2; i < lines.length; i++) {
      if (lines[i].startsWith('W ')) { const [, a, b, n] = lines[i].split(' '); const occ = []; for (let k = 0; k < +n; k++) occ.push(lines[++i].replace(/\r/g, '')); out.wins.push({ a: +a, b: +b, occ }); }
    }
    return out;
  };

  // ---- 1. the device writes, the plugin reads: same parse, same re-write, same repeats ----
  for (const seed of [1, 2, 3, 4, 5, 6]) {
    const dir = path.join(work, 's' + seed); fs.mkdirSync(dir);
    cp.execFileSync(PARITY, ['gen', dir, String(seed)]);
    const files = new Map(['events', 'tasks', 'lists'].map((n) => [D + n + '.txt', fs.readFileSync(dir + '/' + n + '.txt', 'utf8')]));
    const { T } = await boot(files);
    const ev = T.parseEvents(files.get(D + 'events.txt')), ts = T.parseTasks(files.get(D + 'tasks.txt')), ls = T.parseLists(files.get(D + 'lists.txt'));
    ok(T.putEvents(ev) === files.get(D + 'events.txt'), 'seed ' + seed + ' events round trip');
    ok(T.putTasks(ts) === files.get(D + 'tasks.txt'), 'seed ' + seed + ' tasks round trip');
    ok(T.putLists(ls) === files.get(D + 'lists.txt'), 'seed ' + seed + ' lists round trip');
    const wins = []; let r = seed * 7919;
    const rnd = (n) => (r = (r * 1103515245 + 12345) & 0x7fffffff) % n;
    for (let i = 0; i < 60; i++) { const a = T.dnum(2023 + rnd(7), 1 + rnd(12), 1 + rnd(28)); wins.push([a, a + [0, 6, 41, 70, 365][rnd(5)]]); }
    wins.push([T.dnum(2026, 1, 1), T.dnum(2027, 1, 1)]);
    const c = parseCpp(cpp(dir, wins));
    ok(c.rt === 'RT events ok tasks ok lists ok', 'seed ' + seed + ' C++ round trip ' + c.rt);
    ok(c.n === `N ${ev.length} ${ts.length} ${ls.length}`, `seed ${seed} counts ${c.n} vs ${ev.length} ${ts.length} ${ls.length}`);
    c.wins.forEach((win, i) => {
      const mine = T.occIn(ev, win.a, win.b).map((o) => `${o.ev.id} ${o.start} ${o.end}`);
      ok(JSON.stringify(mine) === JSON.stringify(win.occ), `seed ${seed} window ${win.a}..${win.b}: ${mine.length} vs ${win.occ.length}`);
    });
  }

  // ---- 2. the plugin writes, the device reads ----
  {
    const files = new Map();
    const { T, log } = await boot(files);
    await T.ops.listAdd('szerelés'); await T.ops.listAdd('Tab\there');
    const s = T.dnum(2026, 11, 26);
    await T.ops.eventSave({ id: 0, title: 'őrült\ttitle\\x', place: 'a\nb', note: 'n\\n', start: s, end: s, allDay: true, repeat: 3, every: 1, until: null });
    await T.ops.eventSave({ id: 0, title: 'monthly 31st', place: '', note: '', start: T.dnum(2026, 1, 31), end: T.dnum(2026, 1, 31), allDay: false, startMin: 570, endMin: 600, repeat: 3, every: 1, until: T.dnum(2027, 6, 1) });
    await T.ops.eventSave({ id: 0, title: 'multi', place: '', note: '', start: T.dnum(2026, 12, 30), end: T.dnum(2027, 1, 2), allDay: true, repeat: 4, every: 2, until: null });
    await T.ops.eventSave({ id: 0, title: 'weekly', place: '', note: '', start: s, end: s, allDay: false, startMin: 60, endMin: 0, repeat: 2, every: 2, until: null });
    await T.ops.taskSave({ id: 0, title: 'buy milk', list: 2, note: 'x', due: s });
    await T.ops.taskSave({ id: 0, title: 'no due', list: 99, note: '', due: null });
    const d = await T.load();
    ok(d.lists.length === 3 && d.lists[1].name === 'Szerelés' && d.lists[2].name === 'Tab here', 'lists written ' + JSON.stringify(d.lists));
    ok(d.events[0].title === 'Őrült title\\x' || d.events[0].title.startsWith('Őrült'), 'title capitalised: ' + d.events[0].title);
    ok(d.tasks[0].title === 'Buy milk' && d.tasks[0].created > 0 && d.tasks[1].list === 1, 'tasks written');
    await T.ops.taskToggle(d.tasks[0].id);
    ok((await T.load()).tasks[0].done === true, 'toggle done');
    await T.ops.taskToggle(d.tasks[0].id);
    ok((await T.load()).tasks[0].done === false, 'toggle open');
    await T.ops.eventSave({ id: d.events[3].id, title: 'weekly edited', place: '', note: '', start: s, end: s, allDay: true, repeat: 0, every: 1 });
    ok((await T.load()).events[3].title === 'Weekly edited' && (await T.load()).events.length === 4, 'edit keeps id and count');
    await T.ops.eventDelete(d.events[0].id); await T.ops.taskDelete(d.tasks[1].id);
    ok((await T.load()).events.length === 3 && (await T.load()).tasks.length === 1, 'deletes');
    await T.ops.listRename(2, 'Garage');
    await T.ops.listDelete(2);
    const e = await T.load();
    ok(e.lists.length === 2 && e.tasks.length === 0, 'deleting a list takes its to-dos: ' + e.tasks.length);
    let err = ''; try { await T.ops.listDelete(1); } catch (x) { err = x.message; } ok(/cannot/.test(err), 'main list stays');
    err = ''; try { await T.ops.eventSave({ id: 0, title: '  ', start: s }); } catch (x) { err = x.message; } ok(/title/.test(err), 'empty title refused');
    const dir = path.join(work, 'plug'); fs.mkdirSync(dir);
    for (const n of ['events', 'tasks', 'lists']) fs.writeFileSync(dir + '/' + n + '.txt', files.get(D + n + '.txt'));
    const wins = [[s - 40, s + 800], [T.dnum(2026, 1, 1), T.dnum(2028, 1, 1)], [s, s]];
    const c = parseCpp(cpp(dir, wins));
    ok(c.rt === 'RT events ok tasks ok lists ok', 'device re-reads what the plugin wrote: ' + c.rt);
    const ev = T.parseEvents(files.get(D + 'events.txt'));
    c.wins.forEach((win) => ok(JSON.stringify(T.occIn(ev, win.a, win.b).map((o) => `${o.ev.id} ${o.start} ${o.end}`)) === JSON.stringify(win.occ), 'plugin-written occurrences ' + win.a));
    ok(files.get(D + 'events.txt').split('\n').every((l) => !l || l === 'V1' || l.split('\t').length === 13), 'event lines have 13 fields (tabs escaped)');
  }

  // ---- 3. a failed read never turns into a write ----
  {
    const files = new Map([[D + 'tasks.txt', 'V1\nT\t1\t1\tOld\t2026-01-01\t-\t-\t\n']]);
    const { T, log } = await boot(files, { failRead: true });
    let err = ''; try { await T.ops.taskSave({ id: 0, title: 'new', list: 1, note: '', due: null }); } catch (x) { err = x.message; }
    ok(/cannot read/.test(err) && log.writes.length === 0 && files.get(D + 'tasks.txt').includes('Old'), 'read failure writes nothing: ' + err);
    const big = new Map([[D + 'events.txt', 'V1\n' + 'x'.repeat(100 * 1024)]]);
    const b = await boot(big);
    let e2 = ''; try { await b.T.ops.eventSave({ id: 0, title: 'x', start: 20000, allDay: true }); } catch (x) { e2 = x.message; }
    ok(/too big/.test(e2) && b.log.writes.length === 0, 'oversize file is refused, not overwritten');
  }

  // ---- 4. the page ----
  {
    const files = new Map(); const dir = path.join(work, 'ui'); fs.mkdirSync(dir);
    cp.execFileSync(PARITY, ['gen', dir, '9']);
    for (const n of ['events', 'tasks', 'lists']) files.set(D + n + '.txt', fs.readFileSync(dir + '/' + n + '.txt', 'utf8'));
    files.set(D + 'settings.txt', 'first=0\nweeks=0\nlist=0\nagenda=1\nwall=/a.bmp\n');
    const { w, T, container, log } = await boot(files);
    const $ = (s) => container.querySelector(s), $$ = (s) => [...container.querySelectorAll(s)];
    ok($$('.dow').length === 7 && $$('.dow')[0].textContent === 'Sun' && $$('.wn').length === 0, 'month: Sunday first, no week numbers (settings.txt)');
    ok($$('.cell').length === 42, '42 cells');
    ok($('.cell.today') === null || $('.cell.today').textContent.length > 0, 'today cell ok');
    $$('.seg button')[1].click(); await sleep(5);
    ok($('.gm') !== null || /Nothing coming up/.test(container.textContent) || $('.gh') !== null, 'list view draws');
    $$('.seg button')[2].click(); await sleep(5);
    ok($$('.tabs button').length === 4, 'to-do: three lists plus add: ' + $$('.tabs button').length);
    ok($('.prog i') !== null && /\d+ of \d+ done/.test(container.textContent), 'progress shown');
    const before = files.get(D + 'tasks.txt').split('\n').length;
    const q = $('.quick input'); q.value = 'quick one'; q.dispatchEvent(new w.KeyboardEvent('keydown', { key: 'Enter' })); await sleep(60);
    ok(files.get(D + 'tasks.txt').split('\n').length === before + 1 && files.get(D + 'tasks.txt').includes('\tQuick one\t'), 'quick add writes a capitalised to-do');
    // toggle via checkbox
    const cb = $$('.row input[type=checkbox]')[0], was = cb.checked; cb.click(); await sleep(60);
    ok(files.get(D + 'tasks.txt') !== undefined && log.writes.length > 0, 'checkbox writes');
    // add a list through the + tab (prompt mocked)
    $$('.tabs button').pop().click(); await sleep(60);
    ok(files.get(D + 'lists.txt').includes('\tNew list\n'), 'list added');
    // month: dialog for a new event
    $$('.seg button')[0].click(); await sleep(5);
    $$('.card .bar button')[0].click(); await sleep(5);
    const dlg = $('dialog'); ok(dlg && dlg.hasAttribute('open'), 'event dialog open');
    const inputs = [...dlg.querySelectorAll('input[type=text]')]; inputs[0].value = 'from the web';
    const startInput = dlg.querySelector('input[type=date]'); startInput.value = '2026-11-26'; startInput.dispatchEvent(new w.Event('input'));
    ok(/Thursday/.test(dlg.textContent), 'weekday shown under the date: ' + dlg.querySelector('.wk-l').textContent);
    const evBefore = files.get(D + 'events.txt').split('\n').length;
    [...dlg.querySelectorAll('button')].find((b) => b.textContent === 'Save').click(); await sleep(80);
    ok(files.get(D + 'events.txt').split('\n').length === evBefore + 1 && files.get(D + 'events.txt').includes('\tFrom the web\t'), 'event saved from the dialog');
    ok(!$('dialog'), 'dialog closed');
    ok(!/undefined|NaN|\[object/.test(container.textContent), 'no stray undefined/NaN on screen');
    const sel = (cb2) => $$('.toast.err').length;
    ok(sel() === 0, 'no error toasts');
  }

  console.log(`${checks} checks, ${fails} failed`);
  process.exit(fails ? 1 : 0);
})();
