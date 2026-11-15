#!/usr/bin/env python3
"""Compose docs/video/v2.mp4 from rendered assets in $WORK (see render.sh).
Terminals are placed near-native (1776 px wide), highlight boxes are drawn at
row/col positions measured from agg's 40 px font (cell 23.94 x 56.27, pad 26)."""
import os, subprocess
D = os.path.dirname(os.path.abspath(__file__))
WORK = os.environ.get("WORK", os.path.expanduser("~/vid2"))
OUT = os.path.join(D, "..", "v2.mp4")
SANS = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
OX, OY, PAD, CH = 72, 8, 26, 56.27
ENC = ["-c:v", "libx264", "-preset", "veryfast", "-crf", "22", "-pix_fmt", "yuv420p", "-r", "30", "-an"]
ORANGE = "0xff9f1c"

def cap(name):
    f = os.path.join(D, f"cap_{name}.txt")
    return (f"drawbox=x=0:y=ih-176:w=iw:h=176:color=0x000000@0.72:t=fill,"
            f"drawtext=fontfile={SANS}:textfile={f}:fontcolor=white:fontsize=46:line_spacing=14:"
            f"x=(w-text_w)/2:y=h-158:borderw=2:bordercolor=0x000000")

def row_y(r): return OY + PAD + r * CH

def box(r0, r1, x0, x1, t):
    y0 = row_y(r0) - 6; h = (r1 - r0 + 1) * CH + 12
    return (f"drawbox=x={OX + x0 - 14}:y={y0:.0f}:w={x1 - x0 + 28}:h={h:.0f}:color={ORANGE}:t=6:"
            f"enable='gte(t,{t})'")

def callout(text, r, t, x=None):
    f = os.path.join(WORK, f"co_{abs(hash(text))}.txt"); open(f, "w").write(text)
    xx = "(w-text_w)/2" if x is None else str(x)
    return (f"drawtext=fontfile={BOLD}:textfile={f}:fontcolor={ORANGE}:fontsize=48:"
            f"x={xx}:y={row_y(r):.0f}:box=1:boxcolor=0x1a1206@0.85:boxborderw=14:enable='gte(t,{t})'")

def run(args): subprocess.run(["ffmpeg", "-y", "-v", "error", *args], check=True)

def still(name, dur, img=None, extra="", caption=True):
    img = img or name
    vf = "scale=1920:1080" + (("," + extra) if extra else "") + (("," + cap(name)) if caption else "")
    run(["-loop", "1", "-t", str(dur), "-i", f"{WORK}/{img}.png", "-vf", vf, *ENC, f"{WORK}/v2_{name}.mp4"])

def gifdur(n):
    return float(subprocess.check_output(["ffprobe", "-v", "error", "-show_entries", "format=duration",
                                          "-of", "csv=p=0", f"{WORK}/{n}.gif"]).decode())

def term(name, dur, overlays, speed=1.0, label=None):
    d = gifdur(name) / speed
    pad = max(0.1, dur - d)
    vf = (f"setpts=PTS/{speed},fps=30,pad=1920:1080:{OX}:{OY}:color=0x282a36,"
          f"tpad=stop_mode=clone:stop_duration={pad:.2f}")
    if label:
        f = os.path.join(WORK, "lbl.txt"); open(f, "w").write(label)
        vf += (f",drawtext=fontfile={BOLD}:textfile={f}:fontsize=44:fontcolor=black:box=1:boxcolor={ORANGE}:"
               f"boxborderw=12:x=w-text_w-40:y=row_y:enable='lt(t,{d:.2f})'").replace("row_y", str(int(row_y(6))))
    for o in overlays:
        vf += "," + o(d)
    vf += "," + cap(name)
    run(["-i", f"{WORK}/{name}.gif", "-vf", vf, "-t", str(dur), *ENC, f"{WORK}/v2_{name}.mp4"])

segs = []
def S(n): segs.append(n)

still("title", 5, caption=False); S("title")
still("iface", 11); S("iface")
term("boot", 7, [lambda d: box(1, 2, 27, 1266, round(d, 2))], speed=4.0, label="4× speed"); S("boot")
term("live", 14, [lambda d: box(5, 9, 75, 1437, d + 0.3),
                  lambda d: callout("≈376 execs/s on QNX under TCG · 107 edges · 18 crashes in 40 s", 11, d + 1.3)]); S("live")
still("qnx", 16); S("qnx")
still("r1", 9); S("r1")
term("bug1", 13, [lambda d: box(1, 1, 27, 1383, d + 0.2),
                  lambda d: box(3, 4, 27, 1626, d + 1.4),
                  lambda d: callout("271 recorded messages → 2", 10, d + 1.4, x=110),
                  lambda d: box(6, 8, 78, 807, d + 2.8),
                  lambda d: callout("replay 3/3 → exit 10 (crash)", 10, d + 2.8, x=1010)]); S("bug1")
term("bt", 10, [lambda d: box(1, 1, 27, 909, d + 0.3),
                lambda d: callout("crash site: buggy_logic.c:112, the devctl handler", 10, d + 1.0)]); S("bt")
still("r2", 9); S("r2")
still("impact", 15); S("impact")
still("results", 14); S("results")
still("close", 6, caption=False); S("close")

lst = os.path.join(WORK, "v2_list.txt")
open(lst, "w").write("".join(f"file '{WORK}/v2_{n}.mp4'\n" for n in segs))
run(["-f", "concat", "-safe", "0", "-i", lst, "-c", "copy", OUT])
print(subprocess.check_output(["ffprobe", "-v", "error", "-show_entries", "format=duration,size",
                               "-of", "default=nw=1", OUT]).decode())
