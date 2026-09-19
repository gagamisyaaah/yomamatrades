"""Render bend/data/evo_tree.json (+ supers.json, replay tables) as the Tree of Life page: one lane per family, eras
left→right, every node tied to its parents, the winner of each family crowned with its proof."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
tree = json.load(open(ROOT / (sys.argv[1] if len(sys.argv) > 1 else "bend/data/evo_tree.json")))
supers = json.load(open(ROOT / "bend/data/supers.json")) if (ROOT / "bend/data/supers.json").exists() else {}
MEAN = json.load(open(ROOT / "bend/evo/meaning.json")) if (ROOT / "bend/evo/meaning.json").exists() else {}
# prune for the page: keep every finalist and its ancestry, every node that survived ≥ 2 eras, and the 250 best-ranked per family
nodes = tree["nodes"]; keep = set()
def anc(k):
    if k in keep or k not in nodes: return
    keep.add(k)
    for q in nodes[k].get("parents", []): anc(q)
for fam, v in tree["families"].items():
    for k in v["finalists"]: anc(k)
    ranked = sorted((k for k in nodes if nodes[k]["family"] == fam), key=lambda k: -(nodes[k].get("rank") or 0))
    for k in ranked[:120]: anc(k)
for k, n in nodes.items():
    if len(n.get("eras", {})) >= 3 and (n.get("rank") or 0) > 0.15: anc(k)
tree["nodes"] = {k: {kk: vv for kk, vv in nodes[k].items() if kk != "n_in"} for k in keep}
tree["pruned_from"] = len(nodes)
data = json.dumps({"tree": tree, "supers": supers, "meaning": MEAN})

html = r"""<title>Tree of Life</title>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Fraunces:opsz,wght@9..144,300;9..144,600&family=Source+Sans+3:wght@400;600&family=JetBrains+Mono:wght@400;500&display=swap">
<style>
:root{--bg:#EEF1F4;--panel:#FFFFFF;--ink:#16202B;--muted:#5B6B7B;--line:#CBD3DB;--structure:#2B5D8C;--force:#C24A2E;--energy:#1E8C7A;--win:#2E7D32;--loss:#B3261E;--gold:#B8860B;--shade:rgba(22,32,43,.06)}
@media (prefers-color-scheme: dark){:root:not([data-theme="light"]){--bg:#0F1720;--panel:#16212C;--ink:#E6ECF2;--muted:#9AA9B8;--line:#2C3A48;--structure:#6FA3D6;--force:#E8845F;--energy:#4FC2AC;--win:#66BB6A;--loss:#EF5350;--gold:#E0B84A;--shade:rgba(230,236,242,.06)}}
:root[data-theme="dark"]{--bg:#0F1720;--panel:#16212C;--ink:#E6ECF2;--muted:#9AA9B8;--line:#2C3A48;--structure:#6FA3D6;--force:#E8845F;--energy:#4FC2AC;--win:#66BB6A;--loss:#EF5350;--gold:#E0B84A;--shade:rgba(230,236,242,.06)}
body{background:var(--bg);color:var(--ink);font-family:"Source Sans 3",system-ui,sans-serif;font-size:15px;line-height:1.45}
.wrap{max-width:1280px;margin:0 auto;padding:28px 24px 64px}
h1{font-family:Fraunces,Georgia,serif;font-weight:300;font-size:44px;letter-spacing:-.01em;margin:0;text-wrap:balance}
h2{font-family:Fraunces,Georgia,serif;font-weight:600;font-size:22px;margin:0 0 6px}
.sub{color:var(--muted);max-width:66ch;margin:8px 0 0}
.hero{display:grid;grid-template-columns:200px 1fr;gap:24px;align-items:center;margin-bottom:28px}
.legend{display:flex;gap:18px;flex-wrap:wrap;font-size:13px;color:var(--muted);margin-top:14px}
.legend span::before{content:"";display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px;vertical-align:-1px;background:var(--c)}
.lane{background:var(--panel);border:1px solid var(--line);border-radius:6px;padding:16px 18px;margin:18px 0}
.lane header{display:flex;justify-content:space-between;align-items:baseline;gap:16px;flex-wrap:wrap}
.lane header .stat{font-family:"JetBrains Mono",monospace;font-size:12.5px;color:var(--muted)}
.svgbox{overflow-x:auto;margin-top:10px}
svg text{font-family:"JetBrains Mono",monospace;font-size:11px;fill:var(--muted)}
.node{cursor:pointer}
.node circle{stroke:var(--panel);stroke-width:1.5}
.node.dead circle{opacity:.35}
.edge{fill:none;stroke:var(--line);stroke-width:1}
.edge.lit{stroke:var(--ink);stroke-width:1.6}
.era-label{font-weight:500;fill:var(--ink)}
.detail{position:sticky;bottom:0;background:var(--panel);border-top:2px solid var(--line);padding:14px 18px;font-size:14px;display:grid;grid-template-columns:1.2fr 1fr 1fr;gap:18px}
.detail h3{margin:0 0 4px;font-family:Fraunces,Georgia,serif;font-weight:600;font-size:17px}
.detail .mono{font-family:"JetBrains Mono",monospace;font-size:12.5px;font-variant-numeric:tabular-nums}
.detail table{border-collapse:collapse;width:100%}.detail td{padding:2px 6px 2px 0;border-bottom:1px solid var(--line)}
.win{color:var(--win)}.loss{color:var(--loss)}
.winners{display:grid;grid-template-columns:repeat(2,1fr);gap:16px;margin-top:8px}
.card{background:var(--panel);border:1px solid var(--line);border-left:5px solid var(--c);border-radius:6px;padding:14px 16px}
.card .rule{font-family:"JetBrains Mono",monospace;font-size:12.5px;margin:6px 0 10px;word-break:break-word}
.card ul{margin:0;padding-left:18px}.card li{margin:2px 0}
.crown{color:var(--gold);font-weight:600}
@media (max-width:900px){.hero{grid-template-columns:1fr}.winners{grid-template-columns:1fr}.detail{grid-template-columns:1fr}}
</style>
<div class="wrap">
<div class="hero">
 <svg viewBox="0 0 200 200" width="200" height="200" aria-hidden="true"><g fill="none" stroke="var(--ink)" stroke-width="3" stroke-linecap="round"><path d="M100 190 V120"/><path d="M100 120 C 80 100, 60 100, 40 70"/><path d="M100 120 C 120 100, 140 100, 160 70"/><path d="M100 120 V 40"/><path d="M40 70 C 30 55, 25 45, 20 30"/><path d="M40 70 C 50 55, 55 50, 60 40"/><path d="M160 70 C 170 55, 175 45, 180 30"/><path d="M160 70 C 150 55, 145 50, 140 40"/><path d="M100 40 C 90 30, 85 25, 80 15"/><path d="M100 40 C 110 30, 115 25, 120 15"/></g><g><circle cx="20" cy="30" r="7" fill="var(--structure)"/><circle cx="60" cy="40" r="5" fill="var(--structure)"/><circle cx="80" cy="15" r="7" fill="var(--force)"/><circle cx="120" cy="15" r="5" fill="var(--force)"/><circle cx="180" cy="30" r="7" fill="var(--energy)"/><circle cx="140" cy="40" r="5" fill="var(--energy)"/><rect x="70" y="188" width="60" height="4" fill="var(--line)"/></g></svg>
 <div><h1>Tree of Life</h1><p class="sub" id="intro"></p>
 <div class="legend"><span style="--c:var(--structure)">STRUCTURE · the shape of the move</span><span style="--c:var(--force)">FORCE · the momentum of the move</span><span style="--c:var(--energy)">ENERGY · the volatility state</span><span style="--c:var(--gold)">crowned = the family's super indicator</span></div></div>
</div>
<section><h2>The winners: one runner, three base-hits</h2><div class="winners" id="winners"></div></section>
<div id="lanes"></div>
<p class="sub">Hover a node to light its lineage; click to pin it in the panel below. A node is born in the era column where it first survived; it is dimmed in the era where it died. Fitness is the excess of the +2/−4 ATR bracket outcome over random entries of the same year and volatility, in ATR; a rule survives only if that excess is positive in every era so far and on the bear-regime and bull-regime bars separately. 2024+ was never used to select.</p>
</div>
<div class="detail" id="detail"><div><h3>Pick a node</h3><div class="mono">the lineage, the blocks and the numbers appear here</div></div><div></div><div></div></div>
<script id="data" type="application/json">__DATA__</script>
<script>
const D=JSON.parse(document.getElementById('data').textContent);const T=D.tree,S=D.supers,M=D.meaning||{};
const nodes=T.nodes,eras=T.eras,cols=[...eras,T.holdout];const col={STRUCTURE:'var(--structure)',FORCE:'var(--force)',ENERGY:'var(--energy)'};
const fams=Object.keys(T.families);const nAll=T.pruned_from||Object.keys(nodes).length;
document.getElementById('intro').textContent=`${nAll} rules were born across ${fams.length} families and ${eras.length} training eras (${eras[0].slice(3)} → ${eras[eras.length-1].slice(3)}), each from one seed block or from parents by adding, tweaking, dropping or crossing blocks, with Jev proposing combos each era. What you see is every road that survived at least one era; the crowned node in each lane is the road that survived them all and then the ${T.holdout} holdout.`;
const fmt=(x,d=3)=>x==null?'—':(x>0?'+':'')+Number(x).toFixed(d);
const meaning=b=>M[b]||'';
function blocksOf(rule){return rule.split(' & ').map(c=>{const m=c.match(/^([a-z0-9_]+)(≤|≥|=)(?:p\d+\()?(-?[0-9.e+-]+)\)?$/);return m?{b:m[1],op:m[2],thr:m[3]}:{b:c,op:'',thr:''}})}
// winners
for(const f of fams){const s=S[f];if(s&&s.rule){const hit=Object.keys(nodes).find(k=>nodes[k].family===f&&nodes[k].rule===s.rule);if(hit){T.families[f].finalists=[hit,...T.families[f].finalists.filter(x=>x!==hit)]}}}
const W=document.getElementById('winners');
if(S.RUNNER){const r=S.RUNNER;const card=document.createElement('div');card.className='card';card.style.setProperty('--c','var(--gold)');card.innerHTML=`<div class="crown">RUNNER lane · ${r.name}</div><div class="rule">${r.rule} · universe $300M–5B, ATR ≥ 5 % · exit: hole breaks down</div><ul>${Object.entries(r.meaning||{}).map(([k,v])=>`<li><b>${k}</b> — ${v}</li>`).join('')}</ul><table class="mono" style="margin-top:10px">${Object.entries(r.evidence||{}).map(([k,v])=>`<tr><td>${k}</td><td>${v}</td></tr>`).join('')}</table>`;W.appendChild(card)}
for(const f of fams){const fin=T.families[f].finalists[0];if(!fin)continue;const n=nodes[fin];const h=n.holdout||{},hb=n.holdout_bear||{};const sup=S[f]||{};
 const card=document.createElement('div');card.className='card';card.style.setProperty('--c',col[f]);
 card.innerHTML=`<div class="crown">${f} · super indicator</div><div class="rule">${n.rule}</div><ul>${blocksOf(n.rule).map(x=>`<li><b>${x.b}</b> ${x.op} ${Number(x.thr).toPrecision(4)} — ${meaning(x.b)}</li>`).join('')}</ul>
 <table class="mono" style="margin-top:10px"><tr><td>weakest training era</td><td>${fmt(n.rank)} ATR</td></tr><tr><td>bear bars 2016-23</td><td>${fmt(n.bear&&n.bear.mean)} ATR</td></tr><tr><td>bull bars 2016-23</td><td>${fmt(n.bull&&n.bull.mean)} ATR</td></tr><tr><td>holdout ${T.holdout}</td><td class="${(h.mean||0)>0?'win':'loss'}">${fmt(h.mean)} ATR · z ${h.z??'—'} · win ${h.win??'—'} · n ${h.n??'—'}</td></tr><tr><td>holdout bear bars</td><td class="${(hb.mean||0)>0?'win':'loss'}">${fmt(hb.mean)} ATR · n ${hb.n??'—'}</td></tr>${sup.replay?Object.entries(sup.replay).map(([k,v])=>`<tr><td>${k}</td><td>${v}</td></tr>`).join(''):''}</table>`;
 W.appendChild(card)}
// lanes
const L=document.getElementById('lanes');const byId={};
for(const f of fams){const ids=Object.keys(nodes).filter(k=>nodes[k].family===f);const lane=document.createElement('section');lane.className='lane';
 const alive=ids.filter(k=>!nodes[k].died).length;lane.innerHTML=`<header><h2 style="color:${col[f]}">${f}</h2><span class="stat">${ids.length} roads · ${alive} alive at the end · ${ids.filter(k=>nodes[k].source.startsWith('jev')).length} proposed by Jev</span></header><div class="svgbox"></div>`;
 const W0=1180,colW=(W0-60)/cols.length,rowH=16;const byEra={};ids.forEach(k=>{(byEra[nodes[k].era]=byEra[nodes[k].era]||[]).push(k)});
 let maxRows=1;for(const e of eras){const arr=(byEra[e]||[]).sort((a,b)=>nodes[b].rank-nodes[a].rank);arr.forEach((k,i)=>{byId[k]={x:60+cols.indexOf(e)*colW+colW*0.5,y:40+i*rowH};});maxRows=Math.max(maxRows,arr.length)}
 const Hh=60+maxRows*rowH;let svg=`<svg width="${W0}" height="${Hh}" viewBox="0 0 ${W0} ${Hh}">`;
 cols.forEach((e,i)=>{svg+=`<text class="era-label" x="${60+i*colW+colW*0.5}" y="18" text-anchor="middle">${e}</text><line x1="${60+i*colW}" y1="26" x2="${60+i*colW}" y2="${Hh}" stroke="var(--line)" stroke-dasharray="2 4"/>`});
 for(const k of ids){const n=nodes[k];for(const p of n.parents||[]){if(!byId[p]||!byId[k])continue;const a=byId[p],b=byId[k];svg+=`<path class="edge" data-from="${p}" data-to="${k}" d="M${a.x} ${a.y} C ${(a.x+b.x)/2} ${a.y}, ${(a.x+b.x)/2} ${b.y}, ${b.x} ${b.y}"/>`}}
 for(const k of ids){const n=nodes[k],p=byId[k];if(!p)continue;const fin=T.families[f].finalists[0]===k;const r=fin?7:(3+Math.min(3,Math.max(0,n.rank*10)));
  svg+=`<g class="node ${n.died?'dead':''}" data-id="${k}"><circle cx="${p.x}" cy="${p.y}" r="${r}" fill="${fin?'var(--gold)':col[f]}"/>${fin?`<text x="${p.x+10}" y="${p.y+4}" class="era-label">★ ${n.rule.length>48?n.rule.slice(0,48)+'…':n.rule}</text>`:''}</g>`;
  if(n.holdout){const hx=60+cols.indexOf(T.holdout)*colW+colW*0.5;svg+=`<path class="edge" data-from="${k}" data-to="h${k}" d="M${p.x} ${p.y} C ${(p.x+hx)/2} ${p.y}, ${(p.x+hx)/2} ${p.y}, ${hx} ${p.y}"/><g class="node" data-id="${k}"><circle cx="${hx}" cy="${p.y}" r="${fin?7:4}" fill="${n.holdout.mean>0?'var(--win)':'var(--loss)'}"/></g>`}}
 svg+='</svg>';lane.querySelector('.svgbox').innerHTML=svg;L.appendChild(lane)}
// interaction
const det=document.getElementById('detail');let pinned=null;
function ancestors(id,acc=new Set()){acc.add(id);for(const p of nodes[id].parents||[])ancestors(p,acc);return acc}
function light(id){const anc=ancestors(id);document.querySelectorAll('.edge').forEach(e=>e.classList.toggle('lit',anc.has(e.dataset.to)&&anc.has(e.dataset.from)||(e.dataset.to==='h'+id)))}
function show(id){const n=nodes[id];const h=n.holdout||{};const chain=[...ancestors(id)].map(k=>nodes[k]).sort((a,b)=>a.rule.split(' & ').length-b.rule.split(' & ').length);
 det.innerHTML=`<div><h3 style="color:${col[n.family]}">${n.family} · born ${n.era} via ${n.source}${n.died?` · died ${n.died}`:''}</h3><div class="mono">${n.rule}</div><ul style="margin:6px 0 0;padding-left:18px">${blocksOf(n.rule).map(x=>`<li><b>${x.b}</b> ${x.op} ${Number(x.thr).toPrecision(4)} — ${meaning(x.b)}</li>`).join('')}</ul></div>
 <div><h3>Fitness by era (excess ATR vs random)</h3><table class="mono">${Object.entries(n.eras||{}).map(([e,s])=>`<tr><td>${e}</td><td class="${s.mean>0?'win':'loss'}">${fmt(s.mean)}</td><td>z ${s.z}</td><td>win ${s.win}</td><td>n ${s.n}</td></tr>`).join('')}<tr><td>bear bars</td><td class="${n.bear&&n.bear.mean>0?'win':'loss'}">${fmt(n.bear&&n.bear.mean)}</td><td colspan=3>n ${n.bear?n.bear.n:'—'}</td></tr><tr><td>bull bars</td><td class="${n.bull&&n.bull.mean>0?'win':'loss'}">${fmt(n.bull&&n.bull.mean)}</td><td colspan=3>n ${n.bull?n.bull.n:'—'}</td></tr>${n.holdout?`<tr><td><b>${T.holdout} holdout</b></td><td class="${h.mean>0?'win':'loss'}"><b>${fmt(h.mean)}</b></td><td>z ${h.z}</td><td>win ${h.win}</td><td>n ${h.n}</td></tr>`:''}</table></div>
 <div><h3>Lineage (${chain.length} roads)</h3><div class="mono">${chain.map(a=>`${a.era.slice(0,2)} ${a.source.padEnd(6)} ${a.rule}`).join('<br>')}</div></div>`}
document.querySelectorAll('.node').forEach(g=>{g.addEventListener('mouseenter',()=>{if(!pinned){light(g.dataset.id);show(g.dataset.id)}});g.addEventListener('click',()=>{pinned=pinned===g.dataset.id?null:g.dataset.id;light(g.dataset.id);show(g.dataset.id)})});
for(const f of fams){const fin=T.families[f].finalists[0];if(fin){show(fin);light(fin);break}}
</script>"""
out = ROOT / "bend/data/tree_of_life.html"
out.write_text(html.replace("__DATA__", data.replace("</", "<\\/")), encoding="utf-8")
print(out, len(html) // 1024, "KB template;", len(data) // 1024, "KB data")
