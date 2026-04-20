#pragma once
#include <pgmspace.h>

const char WEBPAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Mini Energy Monitoring System</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Inter:wght@400;600;700&display=swap');

    :root {
      --bg-a:#0a0a0f; --bg-b:#0d1117; --bg-c:#0f1923;
      --card-bg:rgba(16,18,26,0.95);
      --card-border:rgba(255,255,255,0.07);
      --label:#555; --body-text:#e0e0e0;
      --grid:rgba(255,255,255,0.05);
      --axis:#444;
    }
    body.light {
      --bg-a:#eef2f7; --bg-b:#e8edf5; --bg-c:#f0f4fa;
      --card-bg:rgba(255,255,255,0.96);
      --card-border:rgba(0,0,0,0.09);
      --label:#999; --body-text:#1a1a2e;
      --grid:rgba(0,0,0,0.06);
      --axis:#bbb;
    }

    *{margin:0;padding:0;box-sizing:border-box;}
    body{
      font-family:'Inter',sans-serif;
      background:linear-gradient(135deg,var(--bg-a) 0%,var(--bg-b) 50%,var(--bg-c) 100%);
      color:var(--body-text); min-height:100vh;
      display:flex; flex-direction:column; align-items:center;
      padding:80px 20px 56px; transition:background .4s,color .3s;
    }

    /* ── Reconnect Banner ── */
    #reconnect-banner{
      display:none; position:fixed; top:0; left:0; right:0;
      background:#c62828; color:#fff; text-align:center;
      padding:10px 16px; font-weight:700; font-size:.82rem;
      letter-spacing:1.5px; z-index:2000;
      animation:blink-bg 1s infinite;
    }
    @keyframes blink-bg{0%,100%{opacity:1}50%{opacity:.75}}

    /* ── Live badge ── */
    .live-badge{
      position:fixed; top:18px; right:18px;
      display:flex; align-items:center; gap:7px;
      background:rgba(10,10,15,.85);
      border:1px solid rgba(105,240,174,.3); color:#69f0ae;
      padding:7px 14px; border-radius:20px;
      font-size:.72rem; font-weight:600; letter-spacing:1px;
      backdrop-filter:blur(10px); z-index:100; transition:background .3s;
    }
    body.light .live-badge{background:rgba(240,244,250,.92);border-color:rgba(0,137,91,.35);color:#00897b;}
    .live-dot{width:7px;height:7px;border-radius:50%;background:#69f0ae;animation:livePulse 1.2s infinite;}
    body.light .live-dot{background:#00897b;}
    @keyframes livePulse{0%,100%{opacity:1;transform:scale(1)}50%{opacity:.3;transform:scale(.65)}}

    /* ── Header ── */
    .header{margin-bottom:36px;text-align:center;}
    h1{
      font-size:clamp(1.4rem,3.5vw,2.2rem);
      background:linear-gradient(135deg,#00e5ff,#448aff,#b388ff);
      -webkit-background-clip:text;-webkit-text-fill-color:transparent;
      background-clip:text; font-weight:700; letter-spacing:-.02em;
    }
    .subtitle{color:var(--label);font-size:.85rem;margin-top:6px;}

    /* ── Grid ── */
    .dashboard{
      display:grid;
      grid-template-columns:repeat(auto-fit,minmax(260px,1fr));
      gap:20px; max-width:1140px; width:100%;
    }

    /* ── Card ── */
    .card{
      background:var(--card-bg); border:1px solid var(--card-border);
      border-radius:20px; padding:28px 24px 22px;
      position:relative; overflow:hidden;
      transition:transform .3s,box-shadow .3s,background .3s,border-color .3s;
    }
    .card:hover{transform:translateY(-4px);box-shadow:0 16px 40px rgba(0,0,0,.45);}
    .card::before{content:'';position:absolute;top:0;left:0;right:0;height:3px;}
    .card.voltage::before{background:linear-gradient(90deg,#00e5ff,#0091ea);}
    .card.current::before{background:linear-gradient(90deg,#ffea00,#ff6d00);}
    .card.power  ::before{background:linear-gradient(90deg,#ff5252,#d50000);}
    .card.energy ::before{background:linear-gradient(90deg,#69f0ae,#00c853);}
    .card.time   ::before{background:linear-gradient(90deg,#69f0ae,#00c853);}
    .card.status ::before{background:linear-gradient(90deg,#e040fb,#aa00ff);}
    .card.trend  ::before{background:linear-gradient(90deg,#448aff,#00e5ff);}
    .card.peaks  ::before{background:linear-gradient(90deg,#ffab40,#ff6d00);}
    .card.controls::before{background:linear-gradient(90deg,#b388ff,#e040fb);}

    /* full-width cards */
    .card.trend,.card.peaks,.card.controls{grid-column:1/-1;}

    /* ── Metric label ── */
    .metric-label{
      font-size:.72rem; color:var(--label); font-weight:600;
      text-transform:uppercase; letter-spacing:1.5px; margin-bottom:16px;
      display:flex; align-items:center; gap:8px;
    }
    .metric-label::after{content:'';flex:1;height:1px;background:var(--grid);}

    /* ── Readout ── */
    .readout{
      font-family:'Share Tech Mono','Courier New',monospace;
      font-size:clamp(2rem,6vw,3.4rem); font-weight:400;
      line-height:1; letter-spacing:.02em;
      white-space:nowrap; overflow:hidden; text-overflow:ellipsis;
      transition:color .2s;
    }
    @keyframes segFlicker{0%{opacity:1}15%{opacity:.6}30%{opacity:1}45%{opacity:.85}60%{opacity:1}}
    .flicker{animation:segFlicker .25s ease;}

    .voltage .readout{color:#00e5ff;text-shadow:0 0 18px rgba(0,229,255,.45);}
    .current .readout{color:#ffea00;text-shadow:0 0 18px rgba(255,234,0,.45);}
    .power   .readout{color:#ff5252;text-shadow:0 0 18px rgba(255,82,82,.45);}
    .energy  .readout{color:#69f0ae;text-shadow:0 0 18px rgba(105,240,174,.4);font-size:clamp(1.6rem,4vw,2.6rem);}
    .time    .readout{color:#69f0ae;text-shadow:0 0 18px rgba(105,240,174,.4);font-size:clamp(1rem,2.5vw,1.35rem);letter-spacing:.05em;}
    .status  .readout{color:#ce93d8;text-shadow:0 0 18px rgba(206,147,216,.4);font-size:clamp(.9rem,2.5vw,1.25rem);}

    body.light .voltage .readout{color:#0077b6;text-shadow:none;}
    body.light .current .readout{color:#d35400;text-shadow:none;}
    body.light .power   .readout{color:#c0392b;text-shadow:none;}
    body.light .energy  .readout{color:#00897b;text-shadow:none;}
    body.light .time    .readout{color:#00897b;text-shadow:none;}
    body.light .status  .readout{color:#7b1fa2;text-shadow:none;}

    /* ── Stability bar ── */
    .stability-row{display:flex;align-items:center;gap:8px;margin-top:14px;}
    .stability-label{font-size:.65rem;color:#444;text-transform:uppercase;letter-spacing:1px;width:50px;}
    body.light .stability-label{color:#aaa;}
    .stability-track{flex:1;height:3px;background:var(--grid);border-radius:2px;overflow:hidden;}
    .stability-fill{height:100%;border-radius:2px;transition:width .8s ease;}
    .voltage .stability-fill{background:#00e5ff;}
    .current .stability-fill{background:#ffea00;}
    .power   .stability-fill{background:#ff5252;}

    /* ── Trend chart ── */
    #trendChart{width:100%;height:190px;display:block;border-radius:8px;}
    .chart-legend{
      display:flex; flex-wrap:wrap; gap:18px; margin-top:12px;
      font-size:.72rem; font-weight:600; letter-spacing:.5px;
    }
    .leg{display:flex;align-items:center;gap:6px;color:var(--label);}
    .leg-dot{width:22px;height:3px;border-radius:2px;}
    .leg-v .leg-dot{background:#00e5ff;}
    .leg-c .leg-dot{background:#ffea00;}
    .leg-p .leg-dot{background:#ff5252;}
    .chart-note{margin-left:auto;font-size:.68rem;color:var(--label);}

    /* ── Peak chart ── */
    #peakChart{width:100%;display:block;border-radius:8px;}

    .peak-info{
      display:flex; flex-wrap:wrap; gap:24px; margin-top:14px;
      font-size:.75rem; font-weight:600;
    }
    .peak-stat{display:flex;flex-direction:column;gap:3px;}
    .peak-stat-label{color:var(--label);font-size:.65rem;text-transform:uppercase;letter-spacing:1px;}
    .peak-stat-value{
      font-family:'Share Tech Mono',monospace;
      color:#ffab40; font-size:1.1rem;
      text-shadow:0 0 12px rgba(255,171,64,.5);
    }
    body.light .peak-stat-value{color:#e65100;text-shadow:none;}
    .peak-reset-note{
      margin-left:auto; font-size:.65rem; color:var(--label);
      align-self:flex-end; font-style:italic;
    }

    /* ── Controls ── */
    .controls-row{display:flex;flex-wrap:wrap;gap:12px;align-items:center;}
    .btn{
      font-family:'Inter',sans-serif; font-size:.78rem; font-weight:600;
      padding:9px 18px; border-radius:12px; border:none; cursor:pointer;
      letter-spacing:.5px; transition:opacity .2s,transform .15s;
    }
    .btn:hover{opacity:.82;transform:translateY(-1px);}
    .btn:active{transform:translateY(0);}
    .btn-zero {background:rgba(0,229,255,.12);  color:#00e5ff; border:1px solid rgba(0,229,255,.3);}
    .btn-time {background:rgba(105,240,174,.12); color:#69f0ae; border:1px solid rgba(105,240,174,.3);}
    .btn-reset{background:rgba(255,82,82,.12);   color:#ff5252; border:1px solid rgba(255,82,82,.3);}
    .btn-prst {background:rgba(255,171,64,.12);  color:#ffab40; border:1px solid rgba(255,171,64,.3);}
    .btn-theme{background:rgba(179,136,255,.12); color:#b388ff; border:1px solid rgba(179,136,255,.3);}
    body.light .btn-zero {color:#0077b6;border-color:#0077b6;background:rgba(0,119,182,.08);}
    body.light .btn-time {color:#00897b;border-color:#00897b;background:rgba(0,137,123,.08);}
    body.light .btn-reset{color:#c0392b;border-color:#c0392b;background:rgba(192,57,43,.08);}
    body.light .btn-prst {color:#e65100;border-color:#e65100;background:rgba(230,81,0,.08);}
    body.light .btn-theme{color:#7b1fa2;border-color:#7b1fa2;background:rgba(123,31,162,.08);}
    .ctrl-msg{font-size:.75rem;margin-top:10px;min-height:1.3em;font-weight:600;letter-spacing:.3px;}

    .err{color:#ff5252!important;font-size:1.1rem!important;}

    @media(max-width:640px){
      .dashboard{grid-template-columns:1fr;}
      .card{padding:22px 18px 18px;}
      .card.trend,.card.peaks,.card.controls{grid-column:unset;}
    }
  </style>
</head>
<body>

  <div id="reconnect-banner">⚠ CONNECTION LOST — Reconnecting…</div>

  <div class="live-badge">
    <div class="live-dot" id="live-dot"></div>LIVE
  </div>

  <div class="header">
    <h1>Mini Energy Monitoring System</h1>
    <div class="subtitle">Real-Time Energy Monitoring Dashboard</div>
    <div class="subtitle">Measurement Range: 0–36 V • 0–1.3 A</div>
  </div>

  <div class="dashboard">

    <!-- Voltage -->
    <div class="card voltage">
      <div class="metric-label">Voltage</div>
      <div class="readout" id="v">Reading…</div>
      <div class="stability-row">
        <span class="stability-label">Stable</span>
        <div class="stability-track"><div class="stability-fill" id="bar-v" style="width:0%"></div></div>
      </div>
    </div>

    <!-- Current -->
    <div class="card current">
      <div class="metric-label">Current</div>
      <div class="readout" id="c">Reading…</div>
      <div class="stability-row">
        <span class="stability-label">Stable</span>
        <div class="stability-track"><div class="stability-fill" id="bar-c" style="width:0%"></div></div>
      </div>
    </div>

    <!-- Power -->
    <div class="card power">
      <div class="metric-label">Power</div>
      <div class="readout" id="p">Reading…</div>
      <div class="stability-row">
        <span class="stability-label">Stable</span>
        <div class="stability-track"><div class="stability-fill" id="bar-p" style="width:0%"></div></div>
      </div>
    </div>

    <!-- Energy -->
    <div class="card energy">
      <div class="metric-label">Energy Accumulated</div>
      <div class="readout" id="e">--</div>
    </div>

    <!-- Date & Time -->
    <div class="card time">
      <div class="metric-label">Date &amp; Time</div>
      <div class="readout" id="t">--</div>
    </div>

    <!-- Status -->
    <div class="card status">
      <div class="metric-label">Status</div>
      <div class="readout" id="s">Connecting…</div>
    </div>

    <!-- Trend chart (full width) -->
    <div class="card trend">
      <div class="metric-label">Trend — Last 60 Readings (~1.1 min window)</div>
      <canvas id="trendChart"></canvas>
      <div class="chart-legend">
        <span class="leg leg-v"><span class="leg-dot"></span>Voltage</span>
        <span class="leg leg-c"><span class="leg-dot"></span>Current</span>
        <span class="leg leg-p"><span class="leg-dot"></span>Power</span>
        <span class="chart-note">Each point = 1 reading (normalised per-series)</span>
      </div>
    </div>

    <!-- Peak solar by hour (full width) -->
    <div class="card peaks">
      <div class="metric-label">Peak Solar Power by Hour — Today</div>
      <canvas id="peakChart"></canvas>
      <div class="peak-info">
        <div class="peak-stat">
          <span class="peak-stat-label">Peak Power</span>
          <span class="peak-stat-value" id="pk-power">--</span>
        </div>
        <div class="peak-stat">
          <span class="peak-stat-label">Peak Voltage</span>
          <span class="peak-stat-value" id="pk-voltage">--</span>
        </div>
        <div class="peak-stat">
          <span class="peak-stat-label">Peak Current</span>
          <span class="peak-stat-value" id="pk-current">--</span>
        </div>
        <div class="peak-stat">
          <span class="peak-stat-label">Best Hour</span>
          <span class="peak-stat-value" id="pk-hour">--</span>
        </div>
        <span class="peak-reset-note">Resets automatically at midnight</span>
      </div>
    </div>

    <!-- Controls (full width) -->
    <div class="card controls">
      <div class="metric-label">Controls</div>
      <div class="controls-row">
        <button class="btn btn-zero"  onclick="zeroCal()">⊙ Zero Current</button>
        <button class="btn btn-time"  onclick="syncTime()">⏱ Sync Clock</button>
        <button class="btn btn-reset" onclick="resetEnergy()">↺ Reset Energy</button>
        <button class="btn btn-prst"  onclick="resetPeaks()">↺ Reset Peaks</button>
        <button class="btn btn-theme" id="theme-btn" onclick="toggleTheme()">☀ Light Mode</button>
      </div>
      <div class="ctrl-msg" id="ctrl-msg"></div>
    </div>

  </div><!-- /dashboard -->

  <script>
    /* ── State ──────────────────────────────────── */
    const prev = {v:null,c:null,p:null};
    let failCount = 0;
    const MAX_FAILS = 3;
    let isDark = true;

    /* ── Helpers ────────────────────────────────── */
    function clamp(x,lo,hi){return Math.max(lo,Math.min(hi,x));}

    function flickerEl(el){
      el.classList.remove('flicker');
      void el.offsetWidth;
      el.classList.add('flicker');
    }

    function setStability(id,curr,prevVal){
      const bar = document.getElementById('bar-'+id);
      if(!bar||prevVal===null){if(bar)bar.style.width='100%';return;}
      const ref  = Math.max(Math.abs(parseFloat(prevVal)||0),0.001);
      const diff = Math.abs((parseFloat(curr)||0)-(parseFloat(prevVal)||0));
      const s    = clamp(1-(diff/ref)*4,0,1);
      bar.style.width=(s*100).toFixed(1)+'%';
    }

    function ctrlMsg(msg,ok){
      const el=document.getElementById('ctrl-msg');
      if(!el)return;
      el.textContent=msg;
      el.style.color=ok?'#69f0ae':'#ff5252';
      setTimeout(()=>{el.textContent='';},5000);
    }

    /* ── Main data fetch ────────────────────────── */
    function update(){
      fetch('/data')
        .then(r=>{if(!r.ok)throw new Error();return r.json();})
        .then(d=>{
          failCount=0;
          document.getElementById('reconnect-banner').style.display='none';
          ['v','c','p','e','t','s'].forEach(id=>{
            const el=document.getElementById(id);
            if(!el||el.textContent===d[id])return;
            el.textContent=d[id]; flickerEl(el);
          });
          setStability('v',d.v,prev.v);
          setStability('c',d.c,prev.c);
          setStability('p',d.p,prev.p);
          prev.v=d.v; prev.c=d.c; prev.p=d.p;
          const dot=document.getElementById('live-dot');
          if(dot) dot.style.background=
            d.s==='CLIENT CONNECTED'?(isDark?'#69f0ae':'#00897b')
                                    :(isDark?'#ffea00':'#e65100');
        })
        .catch(()=>{
          failCount++;
          if(failCount>=MAX_FAILS)
            document.getElementById('reconnect-banner').style.display='block';
          const s=document.getElementById('s');
          if(s){s.textContent='No Response';s.className='readout err';}
        });
    }

    /* ── Trend chart ────────────────────────────── */
    function drawTrend(vArr,cArr,pArr){
      const canvas=document.getElementById('trendChart');
      if(!canvas||vArr.length<2)return;
      const W=canvas.offsetWidth||700,H=190;
      canvas.width=W; canvas.height=H;
      const ctx=canvas.getContext('2d');
      ctx.clearRect(0,0,W,H);
      const P={t:10,r:12,b:22,l:12};
      const cW=W-P.l-P.r, cH=H-P.t-P.b;
      ctx.strokeStyle=isDark?'rgba(255,255,255,0.05)':'rgba(0,0,0,0.06)';
      ctx.lineWidth=1;
      for(let i=0;i<=4;i++){
        const y=P.t+cH*i/4;
        ctx.beginPath();ctx.moveTo(P.l,y);ctx.lineTo(P.l+cW,y);ctx.stroke();
      }
      function drawLine(arr,color,glow){
        if(arr.length<2)return;
        let mn=arr[0],mx=arr[0];
        arr.forEach(v=>{if(v<mn)mn=v;if(v>mx)mx=v;});
        const rng=(mx-mn)||1;
        ctx.strokeStyle=color; ctx.lineWidth=1.8;
        if(isDark){ctx.shadowColor=glow;ctx.shadowBlur=5;}
        ctx.beginPath();
        arr.forEach((v,i)=>{
          const x=P.l+(i/(arr.length-1))*cW;
          const y=P.t+cH-((v-mn)/rng)*cH;
          i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);
        });
        ctx.stroke(); ctx.shadowBlur=0;
      }
      drawLine(vArr,'#00e5ff','rgba(0,229,255,0.6)');
      drawLine(cArr,'#ffea00','rgba(255,234,0,0.6)');
      drawLine(pArr,'#ff5252','rgba(255,82,82,0.6)');
    }

    /* ── Peak bar chart (24-hour) ───────────────── */
    function drawPeaks(peakPower, currentHour){
      const canvas=document.getElementById('peakChart');
      if(!canvas)return;
      const W=canvas.offsetWidth||700;
      const BAR_W=Math.max(8, Math.floor((W-48)/24));
      const H=Math.max(140, BAR_W*4);
      canvas.width=W; canvas.height=H;
      const ctx=canvas.getContext('2d');
      ctx.clearRect(0,0,W,H);

      const PAD={t:8,r:12,b:36,l:12};
      const chartW=W-PAD.l-PAD.r;
      const chartH=H-PAD.t-PAD.b;
      const maxP=Math.max(...peakPower,1);

      // Hour labels (every 3 hours, 12h format)
      const labels=['12 AM','3 AM','6 AM','9 AM','12 PM','3 PM','6 PM','9 PM'];
      ctx.fillStyle=isDark?'#555':'#bbb';
      ctx.font=`${Math.max(9,BAR_W-2)}px Inter,sans-serif`;
      ctx.textAlign='center';
      for(let i=0;i<8;i++){
        const h=i*3;
        const x=PAD.l+(h/23)*chartW;
        ctx.fillText(labels[i],x,H-6);
      }

      for(let h=0;h<24;h++){
        const barH=peakPower[h]>0?(peakPower[h]/maxP)*chartH:2;
        const x=PAD.l+(h/23)*chartW-(BAR_W/2);
        const y=PAD.t+chartH-barH;

        // Active bar vs current hour highlight vs empty
        let color;
        if(peakPower[h]===0){
          color=isDark?'rgba(255,255,255,0.04)':'rgba(0,0,0,0.05)';
        } else if(h===currentHour){
          color='#ffea00'; // bright yellow for current hour
          if(isDark){ctx.shadowColor='rgba(255,234,0,0.6)';ctx.shadowBlur=8;}
        } else {
          // gradient from dim orange (low) to bright orange (peak)
          const ratio=peakPower[h]/maxP;
          const r=Math.round(255);
          const g=Math.round(100+ratio*71);
          color=`rgba(${r},${g},0,${0.5+ratio*0.5})`;
        }

        ctx.fillStyle=color;
        ctx.beginPath();
        ctx.roundRect(x,y,BAR_W,barH,[3,3,0,0]);
        ctx.fill();
        ctx.shadowBlur=0;
      }
    }

    function updatePeaks(){
      fetch('/peaks')
        .then(r=>r.json())
        .then(d=>{
          drawPeaks(d.power||new Array(24).fill(0), d.current_hour||0);

          const pkPow  =document.getElementById('pk-power');
          const pkVolt =document.getElementById('pk-voltage');
          const pkCurr =document.getElementById('pk-current');
          const pkHour =document.getElementById('pk-hour');
          if(pkPow)  pkPow.textContent  = d.best_power   || '--';
          if(pkVolt) pkVolt.textContent = d.best_voltage  || '--';
          if(pkCurr) pkCurr.textContent = d.best_current  || '--';
          if(pkHour) pkHour.textContent = d.best_hour_label || '--';
        })
        .catch(()=>{});
    }

    function updateHistory(){
      fetch('/history')
        .then(r=>r.json())
        .then(d=>{drawTrend(d.v||[],d.c||[],d.p||[]);})
        .catch(()=>{});
    }

    /* ── Controls ───────────────────────────────── */
    function zeroCal(){
      ctrlMsg('Measuring zero offset…',true);
      fetch('/zerocal').then(r=>r.json())
        .then(d=>{ if(d.error) ctrlMsg('✗ '+d.error,false);
                   else        ctrlMsg('✓ Current zeroed. Offset: '+d.offset+' mA',true); })
        .catch(()=>ctrlMsg('✗ Request failed',false));
    }

    function syncTime(){
      const epoch=Math.floor(Date.now()/1000);
      fetch('/settime?epoch='+epoch).then(r=>r.json())
        .then(d=>{ if(d.error) ctrlMsg('✗ '+d.error,false);
                   else        ctrlMsg('✓ Clock synced → '+d.time,true); })
        .catch(()=>ctrlMsg('✗ Request failed',false));
    }

    function resetEnergy(){
      if(!confirm('Reset the energy counter to 0?')) return;
      fetch('/resetenergy').then(r=>r.json())
        .then(()=>ctrlMsg('✓ Energy counter reset to 0',true))
        .catch(()=>ctrlMsg('✗ Request failed',false));
    }

    function resetPeaks(){
      if(!confirm('Reset today\'s peak readings?')) return;
      fetch('/resetpeaks').then(r=>r.json())
        .then(()=>{ ctrlMsg('✓ Peak readings reset',true); updatePeaks(); })
        .catch(()=>ctrlMsg('✗ Request failed',false));
    }

    function toggleTheme(){
      isDark=!isDark;
      document.body.classList.toggle('light',!isDark);
      document.getElementById('theme-btn').textContent=isDark?'☀ Light Mode':'🌙 Dark Mode';
      updateHistory(); updatePeaks();
    }

    /* ── Start ──────────────────────────────────── */
    update();
    updateHistory();
    updatePeaks();
    setInterval(update,         1100);
    setInterval(updateHistory,  10000);
    setInterval(updatePeaks,    10000);
  </script>
</body>
</html>
)rawliteral";
