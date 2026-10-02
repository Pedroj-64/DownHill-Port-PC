<!doctype html><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1"><title>Scene graph __NAME__</title>
<style>
:root{--bg:#14171c;--fg:#d8dde6;--dim:#7b8594;--line:#2a303a}
body{background:var(--bg);color:var(--fg);font:13px/1.5 ui-monospace,monospace;margin:0;padding:12px 16px}
h1{font-size:15px;margin:0 0 6px}#bar{position:sticky;top:0;background:var(--bg);padding:6px 0;border-bottom:1px solid var(--line);z-index:1}
input{background:#1d222a;color:var(--fg);border:1px solid var(--line);padding:3px 6px}button{background:#1d222a;color:var(--fg);border:1px solid var(--line);padding:3px 8px;cursor:pointer}
ul{list-style:none;margin:0;padding-left:16px;border-left:1px solid var(--line)}li{white-space:nowrap}
.tw{cursor:pointer;color:var(--dim);display:inline-block;width:14px}.leaf .tw{visibility:hidden}
.t-1{color:#7fb7ff}.t-2{color:#ffd166}.t-3,.t-4{color:#8bd48b}.t-0{color:#ff8a65}.t-6{color:#c792ea}.t-8{color:#80cbc4}.t-25,.t-11{color:#f48fb1}.t-other{color:var(--dim)}
.ref{opacity:.55}.dim{color:var(--dim)}.hit{background:#3a3515}
.lg span{margin-right:12px}
</style>
<div id=bar><h1>Scene graph — __NAME__</h1>
<div class=lg><span class=t-1>1 group</span><span class=t-2>2 LOD selector</span><span class=t-3>3/4 transform</span><span class=t-0>0 payload (mesh)</span><span class=t-6>6 list</span><span class=t-8>8 link</span><span class=t-25>25/11 objects/hooks</span><span class=t-other>other</span><span class=dim>faded = already shown elsewhere (DAG)</span></div>
<input id=q placeholder="type:1  kind:1629  layer:99  addr 0x648900" size=34> <button id=go>find</button> <button id=ex>expand 3 levels</button> <span id=n class=dim></span></div>
<ul id=root></ul>
<script>
const TREE=__TREE__,VIS=__VIS__,NAMES={1:'group',2:'LOD',3:'translate',4:'matrix',6:'list',8:'link',0:'payload',25:'obj25',11:'hook11'};
function sub(d){let n=1;(d.c||[]).forEach(c=>n+=sub(c));return n}
function label(d){if(d.t<0)return'NGP roots';const nm=NAMES[d.t]||('type '+d.t);let s=nm+' '+d.o;if(d.k)s+=' kind='+d.k;if(d.l!==undefined)s+=' layer='+d.l;if(d.rad)s+=' r='+d.rad;if(VIS[d.o])s+=' ×'+VIS[d.o]+' visits';if(d.c)s+=' ['+d.c.length+' ch, '+sub(d)+' nodes]';if(d.ref)s+=' ↻';return s}
function mk(d){const li=document.createElement('li');li.className=d.c?'':'leaf';li._d=d;const tw=document.createElement('span');tw.className='tw';tw.textContent='▸';const sp=document.createElement('span');sp.className=('t-'+([0,1,2,3,4,6,8,11,25].includes(d.t)?d.t:'other'))+(d.ref?' ref':'');sp.textContent=label(d);sp.dataset.l=sp.textContent.toLowerCase();li.append(tw,sp);tw.onclick=()=>tog(li);return li}
function tog(li,open){const d=li._d;if(!d.c)return;let u=li.querySelector(':scope>ul');if(u){if(open===true)return;u.remove();li.firstChild.textContent='▸';return}u=document.createElement('ul');d.c.forEach(c=>u.append(mk(c)));li.append(u);li.firstChild.textContent='▾'}
function expand(li,n){if(n<=0)return;tog(li,true);li.querySelectorAll(':scope>ul>li').forEach(c=>expand(c,n-1))}
const root=document.getElementById('root'),r=mk(TREE);root.append(r);expand(r,2);
document.getElementById('ex').onclick=()=>expand(r,3);
function find(){const q=document.getElementById('q').value.trim().toLowerCase();document.querySelectorAll('.hit').forEach(e=>e.classList.remove('hit'));if(!q)return;
const m=q.match(/^(type|kind|layer):(\d+)$/);const key=m?({type:(m[2]==='0'?'payload':null),kind:'kind='+m[2]+' ',layer:'layer='+m[2]+' '}[m[1]]):q;
function ok(d){const l=label(d).toLowerCase()+' ';if(m&&m[1]==='type')return d.t==+m[2];return l.includes(key)}
let hits=0;(function w(li){const d=li._d;if(ok(d)&&hits<400){hits++;let p=li,path=[];while(p&&p.tagName==='LI'){path.unshift(p);p=p.parentElement.closest('li')}}})
;function walk(d,path){if(ok(d)&&hits<400){hits++;path.forEach(x=>x);reveal(path.concat(d))}(d.c||[]).forEach(c=>walk(c,path.concat(d)))}
function reveal(p){let li=r;for(let i=1;i<p.length;i++){tog(li,true);li=[...li.querySelectorAll(':scope>ul>li')].find(x=>x._d===p[i])}li.firstElementChild.nextSibling.classList.add('hit')}
walk(TREE,[]);document.getElementById('n').textContent=hits+' matches'+(hits>=400?' (first 400)':'')}
document.getElementById('go').onclick=find;document.getElementById('q').onkeydown=e=>e.key==='Enter'&&find();
</script>
