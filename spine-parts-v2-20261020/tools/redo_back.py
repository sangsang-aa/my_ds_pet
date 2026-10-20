from pathlib import Path
import sys,json
sys.path.insert(0,str(Path(__file__).resolve().parent))
from asset_clean import *

VIEW='back'
layout,out=read_view(VIEW)
records={r['name']:r for r in layout['parts']}

def curve_mask(size,commands,scale=4):
    """Sample cubic paths; no pixels are invented by the clipping operation."""
    pts=[];p=(0,0)
    for command in commands:
        op=command[0];v=command[1:]
        if op in ('M','L'):
            p=tuple(v);pts.append(p)
        elif op=='C':
            a,b,c=tuple(v[:2]),tuple(v[2:4]),tuple(v[4:6]);start=p
            for t in np.linspace(0,1,41)[1:]:
                pts.append(tuple((1-t)**3*start[i]+3*(1-t)**2*t*a[i]+3*(1-t)*t*t*b[i]+t**3*c[i] for i in (0,1)))
            p=c
    return polygon_mask(size,pts,scale)

def edge_clean(im,hair=False):
    im=clean_asset(im,'hair' if hair else 'mixed',component_ratio=.012)
    a=np.array(im);rgb=a[:,:,:3].astype(float);m=a[:,:,3]>5
    inside=ndi.distance_transform_edt(m)
    donor=m&(inside>=2)&(rgb.max(2)<150)
    if donor.any():
        dist,near=ndi.distance_transform_edt(~donor,return_indices=True)
        f=rgb[near[0],near[1]]
        # White-paper antialiasing is unmatted only at exposed boundaries.
        e=m&(inside<=1.5)&(dist<4.5)&(rgb.min(2)>45)
        den=255-f
        alpha=np.clip(((255-rgb)*den).sum(2)/np.maximum((den*den).sum(2),1),0,1)
        e&=(alpha<.95)&((rgb-f).mean(2)>12)
        a[e,:3]=np.clip((rgb[e]-255*(1-alpha[e,None]))/np.maximum(alpha[e,None],.01),0,255).astype('uint8')
        a[e,3]=(a[e,3]*alpha[e]).astype('uint8')
    # Discard low-alpha paper specks left outside an otherwise continuous contour.
    a[(inside<=1.01)&(rgb.min(2)>130),3]=0
    a[a[:,:,3]<45]=0
    m=a[:,:,3]>45
    smooth=ndi.binary_opening(m,structure=np.ones((3,3)))
    a[~smooth,3]=0
    boundary=ndi.distance_transform_edt(a[:,:,3]>45)<=1.01
    paper=boundary&(a[:,:,3]>0)&(a[:,:,:3].min(2)>120)
    a[paper,3]=0
    return rgba_clean(Image.fromarray(a,'RGBA'))

def supplement(name):
    return Image.open(OLD/VIEW/'supplements'/f'{name}.png').convert('RGBA')

def record(name,im,xy,method):
    r=dict(records[name]);r['note']='Clean v2 attachment; see method for restoration provenance.'
    return save_part(out,name,im,xy,r,method)

hair=edge_clean(supplement('rear_hair'),True)
W,H=hair.size
hair_paths={
 'hair_cap':[('M',0,0),('L',440,0),('L',440,125),('L',349,125),('C',355,178,375,222,403,248),('C',370,276,340,287,308,278),('C',287,263,277,247,261,255),('C',245,274,242,303,220,309),('C',200,291,184,269,163,255),('C',141,251,125,279,101,285),('C',72,283,48,270,28,252),('C',66,222,88,176,99,126),('L',0,126)],
 'hair_left':[('M',100,120),('C',130,140,144,180,158,219),('C',161,239,145,250,138,275),('C',121,310,116,339,143,367),('C',153,382,160,397,161,410),('C',162,428,151,440,136,455),('L',0,455),('L',0,120),('L',100,120)],
 'hair_center':[('M',143,247),('C',160,225,183,224,201,249),('C',213,268,219,286,223,292),('C',238,267,248,236,276,223),('C',289,218,303,225,303,246),('C',305,273,284,297,257,319),('C',235,337,242,357,248,378),('C',255,404,245,427,231,443),('L',224,455),('L',143,455),('C',159,440,166,426,163,410),('C',162,392,151,378,139,364),('C',126,349,118,332,122,313),('C',126,283,135,260,143,247)],
 'hair_right':[('M',350,120),('C',366,167,392,217,440,246),('L',440,455),('L',280,455),('C',253,437,242,410,244,384),('C',248,362,231,344,248,325),('C',271,300,300,277,300,245),('C',302,201,320,143,350,120)],
 'hair_loop_left':[('M',0,117),('L',101,117),('C',96,169,78,214,42,253),('L',40,268),('L',0,265)],
 'hair_loop_right':[('M',349,117),('L',440,117),('L',440,265),('L',404,265),('L',403,247),('C',375,216,355,174,349,117)]
}
hair_size=(633,689);hair_xy=(23,95)
ready={}
for name,path in hair_paths.items():
    if 'loop' in name:continue
    im=clipped(hair,curve_mask(hair.size,path))
    if 'loop' in name:
        im=clean_asset(im,'hair',opening=1)
    im=im.resize(hair_size,Image.Resampling.LANCZOS)
    ready[name]=(im,hair_xy,'Complete clean rear-hair redraw from back-restoration.png; curved strand mask, no original fragments or colour extrusion.')

coat=edge_clean(supplement('coat_body'))
hem_path=[('M',0,225),('L',29,225),('C',30,236,37,240,48,243),('C',60,255,72,258,84,267),('C',106,269,113,281,130,285),('C',148,287,154,297,174,300),('C',193,298,198,308,218,305),('C',235,300,246,309,261,306),('C',276,298,287,308,302,306),('C',318,303,330,294,347,294),('C',365,289,379,278,391,277),('C',408,263,421,258,432,255),('C',438,243,444,239,449,226),('L',480,225),('L',480,380),('L',0,380)]
rgb=np.array(coat)[:,:,:3].astype(int);yy,xx=np.indices(rgb.shape[:2])
fur=(rgb.min(2)>155)&((rgb.max(2)-rgb.min(2))<85)&(yy>190)&(np.array(coat)[:,:,3]>0)
labels,n=ndi.label(fur);sizes=np.bincount(labels.ravel());sizes[0]=0
fur=labels==sizes.argmax()
fur=ndi.binary_fill_holes(fur)
fur=ndi.binary_dilation(fur,iterations=4)
hem_mask=Image.fromarray((fur*255).astype('uint8'),'L')
# Full body painting is trimmed to the fur's attachment seam; matching overlap is just two source pixels.
body_mask=Image.fromarray((255-np.array(hem_mask)).astype('uint8'),'L')
body_mask=body_mask.filter(__import__('PIL.ImageFilter',fromlist=['MaxFilter']).MaxFilter(3))
rough=255-np.array(curve_mask(coat.size,hem_path))
body_mask=Image.fromarray(np.minimum(np.array(body_mask),rough).astype('uint8'),'L')
for name,im in [('coat_back',clipped(coat,body_mask)),('coat_hem',clipped(coat,hem_mask))]:
    ready[name]=(im.resize((542,375),Image.Resampling.LANCZOS),(75,490),'Complete clean coat redraw, split along its fur seam; no original garment scraps.')
for name,x in [('boot_left',216),('boot_right',358)]:
    ready[name]=(edge_clean(supplement(name)).resize((115,126),Image.Resampling.LANCZOS),(x,823),'Complete clean boot redraw replacing the occluded source; one boot per attachment.')

# Preserve the character-specific original ornaments and tail after independent contour cleanup.
for name in ['side_fin_left','side_fin_right','headband','whale_hair_accessory','bow_left','bow_right','pompom_left','pompom_right']:
    r=records[name];im=source_part(VIEW,r)
    if name.startswith('bow_'):
        a=np.array(im);rgb=a[:,:,:3].astype(int)
        yy,xx=np.indices(rgb.shape[:2])
        white=(rgb.min(2)>170)&((rgb.max(2)-rgb.min(2))<75)&(yy<22)
        a[white,3]=0
        im=Image.fromarray(a,'RGBA')
    if name == 'headband':
        # Trace the fur's inner contour; preserve the blue-violet fold shadows within the white band.
        hole=curve_mask(im.size,[('M',63,252),('C',83,230,87,205,85,173),('C',92,149,111,126,134,107),('C',166,83,198,68,224,62),('C',234,61,239,57,247,58),('C',263,64,281,66,296,74),('C',326,85,349,104,370,129),('C',395,157,411,182,416,208),('L',435,252)])
        im=clipped(im,Image.fromarray(255-np.array(hole),'L'))
    if name.startswith('pompom_'):
        a=np.array(im);rgb=a[:,:,:3].astype(int)
        blue=(rgb[:,:,2]-rgb[:,:,0]>43)&(rgb[:,:,0]<115)
        a[blue,3]=0
        im=Image.fromarray(a,'RGBA')
    if name=='side_fin_left':
        im=clipped(im,curve_mask(im.size,[('M',0,0),('L',75,0),('C',82,15,86,45,86,88),('L',0,100)]))
    if name=='side_fin_right':
        im=clipped(im,curve_mask(im.size,[('M',9,0),('C',4,18,6,36,14,55),('L',22,82),('L',100,100),('L',100,0)]))
    im=edge_clean(im)
    if name.startswith('pompom_'):
        a=np.array(im);a[:13,:,3]=0;ball=Image.fromarray(a,'RGBA')
        loop=Image.new('RGBA',(im.width*4,im.height*4));dr=ImageDraw.Draw(loop);cx=im.width/2
        dr.ellipse(((cx-7)*4,1*4,(cx+7)*4,18*4),fill='#efedf6',outline='#232348',width=7)
        loop=loop.resize(im.size,Image.Resampling.LANCZOS);loop.alpha_composite(ball);im=loop
    ready[name]=(im,tuple(r['original_bbox_xywh'][:2]),'Original painted feature, separately cleaned alpha and white-matte edges; adjacent colours removed.')

# Both fin roots are smoothly trimmed where they insert beneath the bows.
fin_left=ready['side_fin_left'][0];fin_left=fin_left.crop(fin_left.getbbox())
ready['side_fin_left']=(fin_left.resize((106,91),Image.Resampling.LANCZOS),(6,325),'Original painted side fin with smooth concealed root cut and restored attachment width.')
ready['side_fin_right']=(fin_left.transpose(Image.Transpose.FLIP_LEFT_RIGHT).resize((106,82),Image.Resampling.LANCZOS),(549,332),'Original painted side fin, mirrored to its matching partner; smooth concealed root cut.')

# A complete clean tail painting provides three smooth attachments with painted overlap.
tail=edge_clean(supplement('tail_complete'))
tail_paths={
 'tail_trunk':[('M',0,0),('L',252,0),('L',252,177),('C',263,191,263,209,254,221),('C',237,231,216,233,194,230),('L',0,230)],
 'tail_fluke_down':[('M',232,197),('C',250,193,269,215,277,237),('L',330,370),('L',175,370),('L',175,250),('C',205,246,215,214,232,197)],
 'tail_fluke_right':[('M',241,178),('C',262,159,292,146,328,142),('L',440,135),('L',440,260),('L',308,260),('L',299,244),('C',277,228,260,215,242,216),('C',230,207,227,190,241,178)]
}
for name,path in tail_paths.items():
    ready[name]=(clipped(tail,curve_mask(tail.size,path)).resize((306,209),Image.Resampling.LANCZOS),(294,818),'Complete clean tail redraw split at smooth painted joint overlaps; old angular root scraps discarded.')

# Ropes are redrawn as continuous narrow painted curves rather than retaining white-background cuts.
for name,x0,y0,dx,dy in [('cord_left',116,322,-1,100),('cord_right',553,321,7,105)]:
    im=Image.new('RGBA',(28*4,112*4));d=ImageDraw.Draw(im)
    pts=[]
    for t in np.linspace(0,1,120):
        x=10+dx*t+2*np.sin(t*np.pi);y=3+dy*t
        pts.append((int(x*4),int(y*4)))
    d.line(pts,fill='#181a43',width=7*4,joint='curve');d.line(pts,fill='#444b86',width=3*4,joint='curve')
    im=im.resize((28,112),Image.Resampling.LANCZOS)
    ready[name]=(im,(x0-10,y0),'Continuous shaded cord redraw replacing the source white-background notch.')

output=[]
for r in layout['parts']:
    name=r['name']
    if 'hair_loop_' in name:continue
    im,xy,method=ready[name]
    if name in ['hair_left','hair_right']:
        records[name]['pivot_xy']=[181,325] if name=='hair_left' else [518,325]
        method+=' Upper loop is merged into this side lock to preserve clean shared crossing lines.'
    output.append(record(name,im,xy,method))
for obsolete in ['hair_loop_left','hair_loop_right']:
    (out/'parts'/f'{obsolete}.png').unlink(missing_ok=True)
finish_view(VIEW,layout,output)
print(json.dumps({'view':VIEW,'count':len(output),'output':str(out)},ensure_ascii=False))
