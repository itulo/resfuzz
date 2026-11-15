#!/usr/bin/env python3
"""Make two zoomed, highlighted copies of the run5 HTML report for the video:
r1.html = findings table (row 1 outlined), r2.html = finding 1 detail (frame #0 outlined).
Only layout changes (zoom, removed sections, shortened libc path); text is the report's."""
import sys, os
src = open(os.path.join(os.path.dirname(__file__), "../../results/qnx-run5/report.html")).read()
src = src.replace("/builds/workspace/QNXOS_805/build_x86_64/", "…/")
out = sys.argv[1] if len(sys.argv) > 1 else "."

def variant(name, keep_from, zoom, extra_js, css=""):
    js = f"""<style>body{{max-width:none!important;margin:24px 40px!important}} {css}</style><script>
document.addEventListener('DOMContentLoaded',()=>{{
 const b=document.body; b.style.zoom='{zoom}';
 const start=document.querySelector({keep_from!r});
 let n=b.firstElementChild; while(n && n!==start){{const x=n.nextElementSibling; n.remove(); n=x;}}
 {extra_js}
}});</script>"""
    open(os.path.join(out, name + ".html"), "w").write(src.replace("</head>", js + "</head>"))

variant("r1", "h2", 1.58, """
 const rows=document.querySelectorAll('table')[0].querySelectorAll('tr');
 rows[1].style.outline='6px solid #ff9f1c'; rows[1].style.outlineOffset='-3px'; rows[1].style.background='#fff4e0';
 [...document.querySelectorAll('h3,ul,p,pre')].forEach(e=>e.remove());
 [...rows].slice(7).forEach(r=>r.remove());
""", css="td,th{white-space:nowrap;padding:.95em .8em!important}")
variant("r2", "h3[id='f1']", 1.42, """
 const h=[...document.querySelectorAll('h3')]; h.slice(1).forEach(e=>{let n=e; while(n){const x=n.nextElementSibling; n.remove(); n=x;}});
 const pres=document.querySelectorAll('pre');
 pres[0].textContent=pres[0].textContent.split('\\n').map(l=>l.slice(0,78)+(l.length>78?' …':'')).join('\\n');
 const lines=pres[1].textContent.split('\\n');
 pres[1].innerHTML=lines.map((l,i)=> i==0? '<span style="outline:5px solid #ff9f1c;background:#fff4e0;font-weight:bold">'+l+'</span>' : l).join('\\n');
 document.querySelectorAll('ul li').forEach((li,i)=>{ if(i==3) li.remove(); });
""", css="pre{font-size:20px;line-height:1.45}")
