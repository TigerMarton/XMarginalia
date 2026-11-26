// Daybook's web page and API. See DaybookWebApi.h.
//
// The files are the same ones the Daybook app uses, read and written whole on
// every request. The app shows its "Web" screen while this is served and
// re-reads the files when that screen closes, so nothing else is writing them.
#include "DaybookWebApi.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "DaybookCore.h"

namespace cal = daybook;

namespace {

constexpr const char* kTag = "DBW";
constexpr const char* kDir = "/.crosspoint/calendar";
constexpr const char* kEventsPath = "/.crosspoint/calendar/events.txt";
constexpr const char* kTasksPath = "/.crosspoint/calendar/tasks.txt";
constexpr const char* kListsPath = "/.crosspoint/calendar/lists.txt";

bool readText(const char* path, size_t maxBytes, std::string& out) {
  out.clear();
  if (Storage.exists(path)) return Storage.readFileToString(kTag, path, maxBytes, out);
  const std::string tmp = std::string(path) + ".tmp";
  return Storage.exists(tmp.c_str()) && Storage.readFileToString(kTag, tmp, maxBytes, out);
}

bool writeText(const char* path, const std::string& text) {
  Storage.ensureDirectoryExists(kDir);
  const std::string tmp = std::string(path) + ".tmp";
  {
    HalFile file;
    if (!Storage.openFileForWrite(kTag, tmp.c_str(), file)) return false;
    if (file.write(text.data(), text.size()) != text.size()) {
      file.close();
      Storage.remove(tmp.c_str());
      return false;
    }
    file.flush();
  }
  if (!Storage.replaceFile(tmp.c_str(), path)) {
    if (Storage.exists(path)) Storage.remove(tmp.c_str());
    return false;
  }
  return true;
}

// At most `max` bytes, never cutting inside a UTF-8 character; no tabs or
// line breaks (they are the file's separators).
std::string clip(std::string s, const size_t max) {
  for (char& c : s) {
    if (c == '\t' || c == '\n' || c == '\r') c = ' ';
  }
  if (s.size() > max) {
    size_t n = max;
    while (n > 0 && (static_cast<uint8_t>(s[n]) & 0xC0) == 0x80) n--;
    s.resize(n);
  }
  return s;
}

struct Data {
  std::vector<cal::Event> events;
  std::vector<cal::Task> tasks;
  std::vector<cal::TaskList> lists;
};

void load(Data& d) {
  std::string text;
  if (readText(kEventsPath, cal::kMaxFileBytes, text)) cal::parse(text, d.events);
  if (readText(kTasksPath, cal::kMaxTaskFileBytes, text)) cal::parseTasks(text, d.tasks);
  if (readText(kListsPath, 4096, text)) cal::parseLists(text, d.lists);
  cal::repairLists(d.lists, d.tasks);
}

bool saveEvents(const Data& d) { return writeText(kEventsPath, cal::serialize(d.events)); }
bool saveTasks(const Data& d) { return writeText(kTasksPath, cal::serializeTasks(d.tasks)); }
bool saveLists(const Data& d) { return writeText(kListsPath, cal::serializeLists(d.lists)); }

bool dayOf(JsonVariantConst v, cal::Day& out) {
  const char* s = v.as<const char*>();
  return s && cal::parseDate(s, out);
}

template <typename T>
size_t findId(const std::vector<T>& v, const uint32_t id) {
  for (size_t i = 0; i < v.size(); i++) {
    if (v[i].id == id) return i;
  }
  return v.size();
}

const char kPage[] = R"HTML(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Daybook</title><style>
*{box-sizing:border-box}body{font:16px system-ui,sans-serif;margin:0;background:#f4f4f4;color:#111}
header{background:#111;color:#fff;padding:10px 16px;display:flex;gap:14px;align-items:center;flex-wrap:wrap}
header b{font-size:20px}header a{color:#ccc;text-decoration:none}
main{max-width:860px;margin:0 auto;padding:10px}
.bar{display:flex;gap:6px;align-items:center;flex-wrap:wrap;margin:6px 0}.grow{flex:1;min-width:0}
button,input,select,textarea{font:inherit;padding:6px 10px;border:1px solid #888;border-radius:6px;background:#fff}
button{cursor:pointer}button.on,button.p{background:#111;color:#fff}button.d{color:#b00020}
.card{background:#fff;border:1px solid #ccc;border-radius:8px;padding:10px;margin:8px 0}
.grid{display:grid;grid-template-columns:repeat(7,1fr);gap:2px}
.dow{text-align:center;font-size:12px;color:#666;padding:2px}
.cell{min-height:74px;background:#fff;border:1px solid #ccc;border-radius:4px;padding:2px 3px;cursor:pointer;overflow:hidden}
.cell.out{background:#eee;color:#999}.cell.today .n{background:#111;color:#fff;border-radius:10px;padding:0 6px}
.cell.sel{outline:3px solid #111}.n{font-size:13px;font-weight:700;display:inline-block}
.chip{font-size:11px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;background:#e6e6e6;border-radius:3px;margin-top:1px;padding:0 3px}
.chip.t{background:#fff;border:1px solid #111}.chip.o{background:#b00020;color:#fff}
.row{display:flex;align-items:center;gap:8px;padding:6px 0;border-bottom:1px solid #eee;cursor:pointer}.row:last-child{border:0}
.sub{color:#666;font-size:13px}.over .tt{color:#b00020;font-weight:700}.done .tt{text-decoration:line-through;color:#888}
.mon{font-weight:700;margin:12px 0 2px;border-bottom:2px solid #111}.ovr{background:#111;color:#fff;border-radius:6px;padding:2px 8px;margin:2px 0}
dialog{border:1px solid #111;border-radius:10px;padding:14px;max-width:420px;width:94%}dialog::backdrop{background:#0006}
dialog label{display:block;margin:6px 0 2px;font-size:13px;color:#444}dialog input[type=text],dialog input[type=date],dialog input[type=time],dialog input[type=number],dialog select{width:100%}
.two{display:flex;gap:8px}.two>*{flex:1}#msg{color:#b00020;min-height:1.2em}
@media(max-width:560px){.cell{min-height:52px}.chip{display:none}.cell .dot{display:inline-block}}
.dot{display:none;width:6px;height:6px;border-radius:3px;background:#111;margin:1px}
</style></head><body>
<header><b>Daybook</b><span class="grow"></span>
<button id="vm" class="on">Month</button><button id="vl">List</button><button id="vt">To-do</button></header>
<main><div id="msg"></div><div id="app"></div></main>
<dialog id="ed"><form method="dialog" id="ef"><h3 id="eh">Event</h3>
<label>Title</label><input type="text" id="et" maxlength="60" required>
<div class="two"><div><label>Start</label><input type="date" id="es" required></div><div><label>End</label><input type="date" id="ee"></div></div>
<label><input type="checkbox" id="ea" checked> All day</label>
<div class="two" id="etm"><div><label>From</label><input type="time" id="est"></div><div><label>To</label><input type="time" id="een"></div></div>
<div class="two"><div><label>Repeat</label><select id="er"><option value="0">Never</option><option value="1">Daily</option><option value="2">Weekly</option><option value="3">Monthly</option><option value="4">Yearly</option></select></div>
<div><label>Every</label><input type="number" id="eev" min="1" value="1"></div></div>
<label>Repeat until</label><input type="date" id="eu"><label>Place</label><input type="text" id="el" maxlength="60"><label>Note</label><input type="text" id="en" maxlength="240">
<div class="bar"><button class="p">Save</button><button type="button" id="ex">Cancel</button><span class="grow"></span><button type="button" class="d" id="edel">Delete</button></div></form></dialog>
<dialog id="td"><form method="dialog" id="tf"><h3 id="th">Task</h3>
<label>Title</label><input type="text" id="tt" maxlength="60" required><label>List</label><select id="tl"></select>
<label>Due</label><input type="date" id="tdue"><label>Note</label><input type="text" id="tn" maxlength="240">
<div class="bar"><button class="p">Save</button><button type="button" id="tx">Cancel</button><span class="grow"></span><button type="button" class="d" id="tdel">Delete</button></div></form></dialog>
<script>
const $=i=>document.getElementById(i);
let D={first:1,events:[],tasks:[],lists:[],occ:[]},view='m',cur=1,sel='',Y=0,M=0,eid=0,tid=0;
const z=n=>String(n).padStart(2,'0'),iso=(y,m,d)=>y+'-'+z(m)+'-'+z(d);
const isoD=d=>iso(d.getFullYear(),d.getMonth()+1,d.getDate());
const TODAY=isoD(new Date());
const dt=s=>{const p=s.split('-');return new Date(Date.UTC(+p[0],+p[1]-1,+p[2]))};
const addD=(s,n)=>{const d=dt(s);d.setUTCDate(d.getUTCDate()+n);return d.toISOString().slice(0,10)};
const MN=['January','February','March','April','May','June','July','August','September','October','November','December'],DN=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
const mm=m=>z(Math.floor(m/60))+':'+z(m%60),tm=s=>{const p=(s||'').split(':');return p.length==2?(+p[0])*60+(+p[1]):0};
function el(t,c,x){const e=document.createElement(t);if(c)e.className=c;if(x!==undefined)e.textContent=x;return e}
async function api(b){try{const r=await fetch('/api/daybook',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});
const j=await r.json();if(!j.ok)throw new Error(j.error||'failed');$('msg').textContent='';await load()}catch(e){$('msg').textContent='Error: '+e.message}}
function range(){if(view=='m'){return[iso(Y,M,1),addD(iso(Y,M,1),70)].map((x,i)=>i?x:addD(x,-8))}return view=='l'?[TODAY,addD(TODAY,365)]:[TODAY,TODAY]}
async function load(){try{const r=range();D=await(await fetch('/api/daybook?from='+r[0]+'&to='+r[1])).json();draw()}catch(e){$('msg').textContent='Load failed'}}
const evById=id=>D.events.find(e=>e.id==id);
const tlabel=e=>(e.allDay?'':mm(e.startMin)+' ')+e.title;
function occOn(d){return D.occ.filter(o=>o[1]<=d&&d<=o[2]).map(o=>({e:evById(o[0]),s:o[1]})).filter(x=>x.e).sort((a,b)=>(b.e.allDay-a.e.allDay)||(a.e.startMin-b.e.startMin))}
const tasksOn=d=>D.tasks.filter(t=>t.due==d);
function openEvent(e,date){eid=e?e.id:0;$('eh').textContent=e?'Edit event':'New event';
$('et').value=e?e.title:'';$('es').value=e?e.start:date;$('ee').value=e?e.end:'';$('ea').checked=e?e.allDay:true;
$('est').value=e&&!e.allDay?mm(e.startMin):'';$('een').value=e&&e.endMin?mm(e.endMin):'';$('er').value=e?e.repeat:0;$('eev').value=e?e.every:1;
$('eu').value=e?e.until||'':'';$('el').value=e?e.location:'';$('en').value=e?e.note:'';$('edel').style.display=e?'':'none';$('etm').style.display=$('ea').checked?'none':'';$('ed').showModal()}
function openTask(t,date){tid=t?t.id:0;$('th').textContent=t?'Edit task':'New task';$('tt').value=t?t.title:'';
$('tl').textContent='';D.lists.forEach(l=>{const o=el('option','',l.name);o.value=l.id;$('tl').append(o)});$('tl').value=t?t.list:cur;
$('tdue').value=t?t.due:date||'';$('tn').value=t?t.note:'';$('tdel').style.display=t?'':'none';$('td').showModal()}
$('ea').onchange=()=>$('etm').style.display=$('ea').checked?'none':'';
$('ex').onclick=()=>$('ed').close();$('tx').onclick=()=>$('td').close();
$('ef').onsubmit=()=>{const a=$('ea').checked;api({op:'eventSave',id:eid,title:$('et').value,location:$('el').value,note:$('en').value,start:$('es').value,end:$('ee').value||$('es').value,allDay:a,startMin:a?0:tm($('est').value),endMin:a?0:tm($('een').value),repeat:+$('er').value,every:+$('eev').value||1,until:$('eu').value})};
$('tf').onsubmit=()=>{api({op:'taskSave',id:tid,list:+$('tl').value,title:$('tt').value,note:$('tn').value,due:$('tdue').value,today:TODAY})};
$('edel').onclick=()=>{if(confirm('Delete event?')){$('ed').close();api({op:'eventDelete',id:eid})}};
$('tdel').onclick=()=>{if(confirm('Delete task?')){$('td').close();api({op:'taskDelete',id:tid})}};
function taskRow(t){const r=el('div','row'+(t.done?' done':(t.due&&t.due<TODAY?' over':''))),c=el('input');c.type='checkbox';c.checked=t.done;
c.onclick=ev=>ev.stopPropagation();c.onchange=()=>api({op:'taskToggle',id:t.id,today:TODAY});
const g=el('div','grow'),l=D.lists.find(x=>x.id==t.list);g.append(el('div','tt',t.title),el('div','sub',(t.due?'due '+t.due:'')+(t.done&&t.doneOn?' · done '+t.doneOn:'')+(l&&view!='t'?' · '+l.name:'')+(t.note?' · '+t.note:'')));
r.onclick=()=>openTask(t);r.append(c,g);return r}
function evRow(e,d){const r=el('div','row'),g=el('div','grow');g.append(el('div','tt',e.title),el('div','sub',(d||e.start)+(e.allDay?'':' · '+mm(e.startMin)+(e.endMin?'–'+mm(e.endMin):''))+(e.repeat?' · repeats':'')+(e.location?' · '+e.location:'')));r.append(g);r.onclick=()=>openEvent(e);return r}
function drawMonth(a){if(!Y){const t=TODAY.split('-');Y=+t[0];M=+t[1]}sel=sel||TODAY;
const bar=el('div','bar'),pv=el('button','','‹'),nx=el('button','','›'),tt=el('button','',"Today");
pv.onclick=()=>{M--;if(M<1){M=12;Y--}load()};nx.onclick=()=>{M++;if(M>12){M=1;Y++}load()};tt.onclick=()=>{const t=TODAY.split('-');Y=+t[0];M=+t[1];sel=TODAY;load()};
bar.append(pv,el('b','grow',MN[M-1]+' '+Y),tt,nx);a.append(bar);
const g=el('div','grid');for(let i=0;i<7;i++)g.append(el('div','dow',DN[(D.first+i)%7]));
const f=iso(Y,M,1),lead=(dt(f).getUTCDay()-D.first+7)%7;let d=addD(f,-lead);
for(let i=0;i<42;i++,d=addD(d,1)){const dd=d,c=el('div','cell'+(dd.slice(0,7)!=f.slice(0,7)?' out':'')+(dd==TODAY?' today':'')+(dd==sel?' sel':''));
c.append(el('span','n',+dd.slice(8)));const o=occOn(dd),k=tasksOn(dd);let n=0;
o.forEach(x=>{if(n++<2){c.append(el('div','chip',tlabel(x.e)))}c.append(el('span','dot'))});
k.forEach(t=>{if(n++<2){c.append(el('div','chip '+(!t.done&&t.due<TODAY?'o':'t'),'☐ '+t.title))}c.append(el('span','dot'))});
if(n>2)c.append(el('div','chip','+'+(n-2)));c.onclick=()=>{sel=dd;draw()};g.append(c)}
a.append(g);
const p=el('div','card'),b=el('div','bar');b.append(el('b','grow',DN[dt(sel).getUTCDay()]+', '+sel));
const be=el('button','p','+ Event'),bt=el('button','','+ Task');be.onclick=()=>openEvent(null,sel);bt.onclick=()=>openTask(null,sel);b.append(be,bt);p.append(b);
const o=occOn(sel),k=tasksOn(sel);o.forEach(x=>p.append(evRow(x.e,sel)));k.forEach(t=>p.append(taskRow(t)));
if(!o.length&&!k.length)p.append(el('div','sub','Nothing on this day'));a.append(p)}
function drawList(a){const bar=el('div','bar'),be=el('button','p','+ Event'),bt=el('button','','+ Task');be.onclick=()=>openEvent(null,TODAY);bt.onclick=()=>openTask(null,TODAY);bar.append(el('b','grow','Upcoming'),be,bt);a.append(bar);
const c=el('div','card');const od=D.tasks.filter(t=>!t.done&&t.due&&t.due<TODAY).sort((x,y)=>x.due<y.due?-1:1);
if(od.length){c.append(el('div','ovr','Overdue'));od.forEach(t=>c.append(taskRow(t)))}
const it=[];D.occ.forEach(o=>{const e=evById(o[0]);if(e&&o[2]>=TODAY)it.push({d:o[1]<TODAY?TODAY:o[1],e})});
D.tasks.forEach(t=>{if(!t.done&&t.due>=TODAY)it.push({d:t.due,t})});it.sort((x,y)=>x.d<y.d?-1:x.d>y.d?1:0);
let lm='';it.forEach(x=>{const m=x.d.slice(0,7);if(m!=lm){lm=m;c.append(el('div','mon',MN[+m.slice(5)-1]+' '+m.slice(0,4)))}c.append(x.e?evRow(x.e,x.d):taskRow(x.t))});
if(!od.length&&!it.length)c.append(el('div','sub','Nothing upcoming'));a.append(c)}
function drawTodo(a){if(!D.lists.some(l=>l.id==cur))cur=D.lists.length?D.lists[0].id:1;
const tabs=el('div','bar');D.lists.forEach(l=>{const b=el('button',l.id==cur?'on':'',l.name);b.onclick=()=>{cur=l.id;draw()};tabs.append(b)});a.append(tabs);
const bar=el('div','bar'),bt=el('button','p','+ Task'),bl=el('button','','+ List'),br=el('button','','Rename list'),bd=el('button','d','Delete list');
bt.onclick=()=>openTask(null,'');
bl.onclick=()=>{const n=prompt('New list name');if(n)api({op:'listAdd',name:n})};
br.onclick=()=>{const n=prompt('Rename list',(D.lists.find(l=>l.id==cur)||{}).name);if(n)api({op:'listRename',id:cur,name:n})};
bd.onclick=()=>{if(cur!=1&&confirm('Delete this list and its tasks?'))api({op:'listDelete',id:cur})};bar.append(bt,bl,br,bd);a.append(bar);
const c=el('div','card'),T=D.tasks.filter(t=>t.list==cur).sort((x,y)=>(x.done-y.done)||((x.due||'9')<(y.due||'9')?-1:1));
T.forEach(t=>c.append(taskRow(t)));if(!T.length)c.append(el('div','sub','No tasks'));a.append(c)}
function draw(){const a=$('app');a.textContent='';['m','l','t'].forEach(v=>$('v'+v).className=v==view?'on':'');(view=='m'?drawMonth:view=='l'?drawList:drawTodo)(a)}
['m','l','t'].forEach(v=>$('v'+v).onclick=()=>{view=v;load()});
const t0=TODAY.split('-');Y=+t0[0];M=+t0[1];load();
</script></body></html>
)HTML";

}  // namespace

namespace daybook::web {

void page(IO& io) { io.sendHtml(kPage); }

void get(IO& io) {
  Data d;
  load(d);
  // ?from=YYYY-MM-DD&to=YYYY-MM-DD: the days the page is showing; "occ" holds
  // every showing of every event in that window (repeats expanded here, by the
  // same code the device uses), so the page never has to know the repeat rules.
  cal::Day from = 0, to = 0;
  const bool window = cal::parseDate(io.arg("from").c_str(), from) && cal::parseDate(io.arg("to").c_str(), to) &&
                      to >= from && to - from <= 800;
  int first = 1;
  {
    std::string text;
    if (readText("/.crosspoint/calendar/settings.txt", 512, text)) {
      int f = 1;
      if (std::sscanf(text.c_str(), "first=%d", &f) == 1 && (f == 0 || f == 6)) first = f;
    }
  }
  io.beginChunked("application/json");
  JsonDocument doc;
  std::string out;
  const auto flush = [&](const char* s) { io.chunk(s); };
  char num[24];
  std::snprintf(num, sizeof num, "{\"first\":%d,", first);
  flush(num);

  flush("\"events\":[");
  for (size_t i = 0; i < d.events.size(); i++) {
    const cal::Event& e = d.events[i];
    doc.clear();
    doc["id"] = e.id;
    doc["title"] = e.title;
    doc["location"] = e.location;
    doc["note"] = e.note;
    doc["start"] = cal::formatDate(e.start);
    doc["end"] = cal::formatDate(e.end);
    doc["allDay"] = e.allDay;
    doc["startMin"] = e.startMin;
    doc["endMin"] = e.endMin;
    doc["repeat"] = static_cast<int>(e.repeat);
    doc["every"] = e.every;
    doc["until"] = e.hasUntil ? cal::formatDate(e.until) : std::string();
    out.clear();
    serializeJson(doc, out);
    if (i) flush(",");
    flush(out.c_str());
  }
  flush("],\"tasks\":[");
  for (size_t i = 0; i < d.tasks.size(); i++) {
    const cal::Task& t = d.tasks[i];
    doc.clear();
    doc["id"] = t.id;
    doc["list"] = t.list;
    doc["title"] = t.title;
    doc["note"] = t.note;
    doc["due"] = t.hasDue ? cal::formatDate(t.due) : std::string();
    doc["done"] = t.done;
    doc["doneOn"] = t.done ? cal::formatDate(t.doneOn) : std::string();
    out.clear();
    serializeJson(doc, out);
    if (i) flush(",");
    flush(out.c_str());
  }
  flush("],\"lists\":[");
  for (size_t i = 0; i < d.lists.size(); i++) {
    doc.clear();
    doc["id"] = d.lists[i].id;
    doc["name"] = d.lists[i].name;
    out.clear();
    serializeJson(doc, out);
    if (i) flush(",");
    flush(out.c_str());
  }
  flush("],\"occ\":[");
  if (window) {
    std::vector<cal::Occurrence> occ;
    cal::occurrencesIn(d.events, from, to, occ);
    constexpr size_t kMaxOcc = 1500;
    if (occ.size() > kMaxOcc) occ.resize(kMaxOcc);
    for (size_t i = 0; i < occ.size(); i++) {
      out = std::string(i ? "," : "") + "[" + std::to_string(d.events[occ[i].event].id) + ",\"" +
            cal::formatDate(occ[i].start) + "\",\"" + cal::formatDate(occ[i].end) + "\"]";
      flush(out.c_str());
    }
  }
  flush("]}");
  io.endChunked();
}

void post(IO& io, const std::string& body) {
  const auto reply = [&io](const bool ok, const char* error) {
    const std::string json = ok ? std::string("{\"ok\":true}") : std::string("{\"ok\":false,\"error\":\"") + error + "\"}";
    io.sendJson(ok ? 200 : 400, json);
  };
  if (body.empty()) return reply(false, "no body");
  JsonDocument doc;
  if (deserializeJson(doc, body)) return reply(false, "bad json");
  const std::string op = doc["op"] | std::string();
  Data d;
  load(d);

  if (op == "eventSave") {
    cal::Event e;
    uint32_t id = doc["id"] | 0u;
    e.title = clip(doc["title"] | std::string(), cal::kMaxTitle);
    if (e.title.empty()) return reply(false, "title is empty");
    e.location = clip(doc["location"] | std::string(), cal::kMaxLocation);
    e.note = clip(doc["note"] | std::string(), cal::kMaxNote);
    if (!dayOf(doc["start"], e.start)) return reply(false, "bad start date");
    if (!dayOf(doc["end"], e.end) || e.end < e.start) e.end = e.start;
    e.allDay = doc["allDay"] | true;
    e.startMin = static_cast<int16_t>(std::clamp<int>(doc["startMin"] | 0, 0, 1439));
    e.endMin = static_cast<int16_t>(std::clamp<int>(doc["endMin"] | 0, 0, 1439));
    if (e.allDay) e.startMin = e.endMin = 0;
    const int rep = std::clamp<int>(doc["repeat"] | 0, 0, 4);
    e.repeat = static_cast<cal::Repeat>(rep);
    e.every = static_cast<uint16_t>(std::clamp<int>(doc["every"] | 1, 1, 999));
    if (rep != 0 && dayOf(doc["until"], e.until)) e.hasUntil = e.until >= e.start;
    const size_t at = id ? findId(d.events, id) : d.events.size();
    if (at < d.events.size()) {
      e.id = id;
      d.events[at] = e;
    } else {
      if (d.events.size() >= cal::kMaxEvents) return reply(false, "event list is full");
      e.id = cal::nextId(d.events);
      d.events.push_back(e);
    }
    return saveEvents(d) ? reply(true, "") : reply(false, "write failed");
  }
  if (op == "eventDelete") {
    const size_t at = findId(d.events, doc["id"] | 0u);
    if (at >= d.events.size()) return reply(false, "no such event");
    d.events.erase(d.events.begin() + at);
    return saveEvents(d) ? reply(true, "") : reply(false, "write failed");
  }
  if (op == "taskSave") {
    cal::Task t;
    const uint32_t id = doc["id"] | 0u;
    t.title = clip(doc["title"] | std::string(), cal::kMaxTaskTitle);
    if (t.title.empty()) return reply(false, "title is empty");
    t.note = clip(doc["note"] | std::string(), cal::kMaxTaskNote);
    t.list = doc["list"] | static_cast<uint32_t>(cal::kMainList);
    if (findId(d.lists, t.list) >= d.lists.size()) t.list = cal::kMainList;
    t.hasDue = dayOf(doc["due"], t.due);
    const size_t at = id ? findId(d.tasks, id) : d.tasks.size();
    if (at < d.tasks.size()) {
      const cal::Task old = d.tasks[at];
      t.id = old.id;
      t.created = old.created;
      t.done = old.done;
      t.doneOn = old.doneOn;
      d.tasks[at] = t;
    } else {
      if (d.tasks.size() >= cal::kMaxTasks) return reply(false, "task list is full");
      t.id = cal::nextTaskId(d.tasks);
      cal::Day today = 0;
      t.created = dayOf(doc["today"], today) ? today : (t.hasDue ? t.due : 0);
      d.tasks.push_back(t);
    }
    return saveTasks(d) ? reply(true, "") : reply(false, "write failed");
  }
  if (op == "taskToggle") {
    const size_t at = findId(d.tasks, doc["id"] | 0u);
    if (at >= d.tasks.size()) return reply(false, "no such task");
    cal::Task& t = d.tasks[at];
    t.done = !t.done;
    cal::Day today = 0;
    t.doneOn = t.done && dayOf(doc["today"], today) ? today : 0;
    return saveTasks(d) ? reply(true, "") : reply(false, "write failed");
  }
  if (op == "taskDelete") {
    const size_t at = findId(d.tasks, doc["id"] | 0u);
    if (at >= d.tasks.size()) return reply(false, "no such task");
    d.tasks.erase(d.tasks.begin() + at);
    return saveTasks(d) ? reply(true, "") : reply(false, "write failed");
  }
  if (op == "listAdd") {
    const std::string name = clip(doc["name"] | std::string(), cal::kMaxListName);
    if (name.empty()) return reply(false, "name is empty");
    if (d.lists.size() >= cal::kMaxLists) return reply(false, "too many lists");
    cal::TaskList l;
    l.id = cal::nextListId(d.lists);
    l.name = name;
    d.lists.push_back(l);
    return saveLists(d) ? reply(true, "") : reply(false, "write failed");
  }
  if (op == "listRename") {
    const std::string name = clip(doc["name"] | std::string(), cal::kMaxListName);
    const size_t at = findId(d.lists, doc["id"] | 0u);
    if (name.empty() || at >= d.lists.size()) return reply(false, "bad list");
    d.lists[at].name = name;
    return saveLists(d) ? reply(true, "") : reply(false, "write failed");
  }
  if (op == "listDelete") {
    const uint32_t id = doc["id"] | 0u;
    const size_t at = findId(d.lists, id);
    if (id == cal::kMainList || at >= d.lists.size()) return reply(false, "cannot delete that list");
    d.tasks.erase(std::remove_if(d.tasks.begin(), d.tasks.end(), [id](const cal::Task& t) { return t.list == id; }),
                  d.tasks.end());
    d.lists.erase(d.lists.begin() + at);
    return (saveTasks(d) && saveLists(d)) ? reply(true, "") : reply(false, "write failed");
  }
  return reply(false, "unknown op");
}

}  // namespace daybook::web
