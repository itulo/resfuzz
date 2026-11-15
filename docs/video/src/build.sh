#!/bin/bash
set -e
cd "$(dirname "$0")"
F=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
cap(){ echo "drawbox=x=0:y=ih-170:w=iw:h=170:color=0x0d1117@0.92:t=fill,drawtext=fontfile=$F:textfile=cap_$1.txt:fontcolor=white:fontsize=36:line_spacing=10:x=(w-text_w)/2:y=h-150"; }
enc="-c:v libx264 -preset veryfast -crf 23 -pix_fmt yuv420p -r 30 -an"
# still card: name dur
still(){ ffmpeg -y -v error -loop 1 -t $2 -i $1.png -vf "scale=1920:1080,$(cap $1)" $enc seg_$1.mp4; }
# gif scene: name dur  (freeze last frame to fill)
gifscene(){ d=$(ffprobe -v error -show_entries format=duration -of csv=p=0 $1.gif); pad=$(python3 -c "print(max(0.1,$2-$d))");
  ffmpeg -y -v error -i $1.gif -vf "scale=1700:-2:flags=lanczos,pad=1920:1080:(ow-iw)/2:max(20\,(ih-ih)/2+40):color=0x0d1117,tpad=stop_mode=clone:stop_duration=$pad,$(cap $1)" -t $2 $enc seg_$1.mp4; }
still c1 8
still c2 14
gifscene boot 20
gifscene live 23
# report: pan down the full screenshot over 15 s
ffmpeg -y -v error -loop 1 -t 15 -i report_full.png -vf "scale=1920:-2,crop=1920:1080:0:'min(ih-1080, t/15*(ih-1080))',$(cap report)" $enc seg_report.mp4
still c5 10
gifscene bug1 30
gifscene bt 15
still c8 17
still c9 10
printf "file 'seg_%s.mp4'\n" c1 c2 boot live report c5 bug1 bt c8 c9 > list.txt
ffmpeg -y -v error -f concat -safe 0 -i list.txt -c copy draft.mp4
ffprobe -v error -show_entries format=duration,size -of default=nw=1 draft.mp4
