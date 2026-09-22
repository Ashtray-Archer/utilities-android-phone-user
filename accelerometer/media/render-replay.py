#!/usr/bin/env python3
import csv
import sys
from pathlib import Path

import imageio_ffmpeg
import numpy as np
from PIL import Image

W, H = 768, 1440
FPS = 50
BG = (24,24,24)
FG = (238,238,238)
SECONDARY = (168,168,168)
TRACK = (80,80,80)

G = {
' ':[0,0,0,0,0,0,0], '+':[0,4,4,31,4,4,0], '-':[0,0,0,31,0,0,0],
'.':[0,0,0,0,0,12,12], '/':[1,2,2,4,8,8,16],
'0':[14,17,19,21,25,17,14], '1':[4,12,20,4,4,4,31],
'2':[14,17,1,2,4,8,31], '3':[30,1,1,14,1,1,30],
'4':[2,6,10,18,31,2,2], '5':[31,16,16,30,1,1,30],
'6':[14,16,16,30,17,17,14], '7':[31,1,2,4,8,8,8],
'8':[14,17,17,14,17,17,14], '9':[14,17,17,15,1,1,14],
'S':[15,16,16,14,1,1,30], 'P':[30,17,17,30,16,16,16],
'R':[30,17,17,30,20,18,17], 'I':[31,4,4,4,4,4,31],
'N':[17,25,25,21,19,19,17], 'G':[15,16,16,23,17,17,15],
'X':[17,17,10,4,10,17,17], 'Y':[17,17,10,4,4,4,4], 'Z':[31,1,2,4,8,16,31],
'a':[0,0,14,1,15,17,15], 'd':[1,1,15,17,17,17,15],
'e':[0,0,14,17,31,16,14], 'h':[16,16,30,17,17,17,17],
'i':[4,0,12,4,4,4,14], 'm':[0,0,26,21,21,21,21],
'n':[0,0,30,17,17,17,17], 'o':[0,0,14,17,17,17,14],
'p':[0,0,30,17,30,16,16], 'r':[0,0,22,25,16,16,16],
's':[0,0,15,16,14,1,30], 't':[4,4,31,4,4,4,3],
'u':[0,0,17,17,17,19,13], 'y':[0,0,17,17,15,1,14],
}

def cell(rows, r, c):
    return 0 <= r < 7 and 0 <= c < 5 and bool(rows[r] & (1 << (4-c)))

CACHE = {}
def mask(ch, scale):
    key=(ch,scale)
    if key in CACHE:
        return CACHE[key]
    rows=G.get(ch,G[' '])
    w,h=5*scale,7*scale
    out=np.zeros((h,w),dtype=np.uint8)
    den=8*scale
    full=den*den
    for py in range(h):
        for px in range(w):
            covered=0
            for sy in range(4):
                for sx in range(4):
                    xn=8*px+2*sx+1-4*scale
                    yn=8*py+2*sy+1-4*scale
                    c=xn//den
                    r=yn//den
                    xr=xn-c*den
                    yr=yn-r*den
                    x0,x1=den-xr,xr
                    y0,y1=den-yr,yr
                    weighted=0
                    if cell(rows,r,c): weighted += x0*y0
                    if cell(rows,r,c+1): weighted += x1*y0
                    if cell(rows,r+1,c): weighted += x0*y1
                    if cell(rows,r+1,c+1): weighted += x1*y1
                    if 5*weighted >= 2*full:
                        covered += 1
            out[py,px]=(covered*255+8)//16
    CACHE[key]=out
    return out

def glyph(arr,ch,left,top,scale,color):
    if ch == ' ':
        return
    m=mask(ch,scale)
    hh,ww=m.shape
    x0=max(0,left); y0=max(0,top)
    x1=min(arr.shape[1],left+ww); y1=min(arr.shape[0],top+hh)
    if x1<=x0 or y1<=y0:
        return
    a=m[y0-top:y1-top,x0-left:x1-left].astype(np.float32)/255.0
    dst=arr[y0:y1,x0:x1].astype(np.float32)
    col=np.array(color,dtype=np.float32)
    arr[y0:y1,x0:x1]=np.clip(dst*(1-a[...,None])+col*a[...,None],0,255).astype(np.uint8)

def text_width(s,scale):
    return 0 if not s else (len(s)*6-1)*scale

def text(arr,s,left,top,scale,color):
    x=left
    for ch in s:
        glyph(arr,ch,x,top,scale,color)
        x += 6*scale

def centered(arr,s,top,scale,color):
    text(arr,s,(arr.shape[1]-text_width(s,scale))//2,top,scale,color)

def load_points(path):
    rows=[]
    with open(path,newline='') as f:
        for r in csv.DictReader(f):
            rows.append((float(r['timestamp_ns']),float(r['x']),float(r['y']),float(r['z'])))
    return np.array(rows,dtype=float)

def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: render-replay.py sample.csv replay.mp4 poster.png")
    points=load_points(sys.argv[1])
    t=(points[:,0]-points[0,0])/1e9
    times=np.arange(0,float(t[-1])+0.5/FPS,1/FPS)
    xyz=np.column_stack([np.interp(times,t,points[:,i]) for i in (1,2,3)])

    scale=max(2,min(6,W//180))
    text_scale=max(2,min(2*(scale+1),W//60))
    title_scale=max(3,text_scale-4)
    unit_scale=max(3,text_scale-scale)
    super_scale=max(2,unit_scale-1)
    springs_scale=title_scale+1
    title_line_height=15*title_scale
    reading_stride=H//5
    readings_top=(2*H)//5
    bar_height=6*scale
    track_height=2*scale
    bar_margin=2*scale
    center_x=W//2
    half_width=(W-2*bar_margin)//2
    track_width=W-2*bar_margin
    axis_left=4*scale
    value_left=W//4
    unit_left=(7*W)//10

    base=np.full((H,W,3),BG,dtype=np.uint8)
    top=(9*H)//100
    centered(base,"there are",top,title_scale,SECONDARY)
    centered(base,"SPRINGS",top+title_line_height,springs_scale,FG)
    centered(base,"inside",top+2*title_line_height,title_scale,SECONDARY)
    centered(base,"your phone",top+3*title_line_height,title_scale,SECONDARY)

    def frame(v):
        arr=base.copy()
        for axis,ch in enumerate("XYZ"):
            y=readings_top+axis*reading_stride
            glyph(arr,ch,axis_left,y,text_scale,FG)
            text(arr,f"{v[axis]:+.1f}",value_left,y,text_scale,FG)
            unit_y=y+7*(text_scale-unit_scale)
            text(arr,"m/s",unit_left,unit_y,unit_scale,FG)
            end=unit_left+text_width("m/s",unit_scale)+unit_scale
            glyph(arr,'2',end,unit_y-2*super_scale,super_scale,FG)
            bar_y=y+9*text_scale
            arr[bar_y+(bar_height-track_height)//2:bar_y+(bar_height-track_height)//2+track_height,
                bar_margin:bar_margin+track_width]=TRACK
            arr[bar_y:bar_y+bar_height,center_x:center_x+1]=SECONDARY
            clipped=max(-20.0,min(20.0,float(v[axis])))
            length=int((clipped/20.0)*half_width)
            if length >= 0:
                arr[bar_y:bar_y+bar_height,center_x:center_x+length]=SECONDARY
            else:
                arr[bar_y:bar_y+bar_height,center_x+length:center_x]=SECONDARY
        return arr

    output=Path(sys.argv[2])
    writer=imageio_ffmpeg.write_frames(
        str(output),(W,H),fps=FPS,codec="libx264",quality=7,
        pix_fmt_in="rgb24",pix_fmt_out="yuv420p",ffmpeg_log_level="error")
    writer.send(None)
    first=None
    for v in xyz:
        fr=frame(v)
        if first is None:
            first=fr.copy()
        writer.send(fr.tobytes())
    writer.close()
    Image.fromarray(first).save(sys.argv[3])

if __name__ == "__main__":
    main()
