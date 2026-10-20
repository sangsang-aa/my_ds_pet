from pathlib import Path
import sys, json, math
ROOT=Path(__file__).resolve().parents[1]
OLD=ROOT.parent/'spine-parts-20261020'
sys.path[:0]=[str(OLD/'.tools'),str(OLD/'tools')]
import numpy as np
from scipy import ndimage as ndi
from PIL import Image,ImageDraw,ImageFont

def rgba_clean(im):
    a=np.array(im.convert('RGBA'));a[a[:,:,3]<2]=0
    return Image.fromarray(a,'RGBA')

def keep_components(mask, ratio=.018, floor=20):
    labs,n=ndi.label(mask,np.ones((3,3)))
    sizes=np.bincount(labs.ravel());sizes[0]=0
    if n==0:return mask
    return (labs>0)&(sizes[labs]>=max(floor,sizes.max()*ratio))

def clean_asset(im,kind='mixed',component_ratio=.018,opening=0):
    """No colour extrusion. Conservative alpha cleanup, then white-matte decontamination."""
    a=np.array(im.convert('RGBA'));rgb=a[:,:,:3].astype(float);mask=a[:,:,3]>20
    if kind=='hair':
        # These character's hair highlights are blue. White fur/skin are never hair.
        allowed=(rgb[:,:,2]-rgb[:,:,0]>14)&(rgb[:,:,2]-rgb[:,:,1]>5)&(rgb[:,:,0]<180)
        mask&=allowed
    if opening:
        opened=ndi.binary_opening(mask,iterations=opening)
        # Remove fine stray projections without adding outside pixels.
        mask &= ndi.binary_dilation(opened,iterations=opening)
    mask=keep_components(mask,ratio=component_ratio)
    a[:,:,3]=np.where(mask,a[:,:,3],0)
    # Unmix only a white-contaminated edge against a dark nearby interior donor.
    inside=ndi.distance_transform_edt(mask)
    donor=(inside>=2.3)&(rgb.max(2)<210)
    if donor.any():
        dist,near=ndi.distance_transform_edt(~donor,return_indices=True)
        f=rgb[near[0],near[1]]
        edge=mask&(inside<=2.0)&(dist<4.5)&(rgb.min(2)>140)
        denom=255-f
        alpha=np.clip(np.sum((255-rgb)*denom,axis=2)/np.maximum(np.sum(denom*denom,axis=2),1),0,1)
        edge&=(alpha<.94)&(alpha>.03)
        a[edge,:3]=np.clip((rgb[edge]-255*(1-alpha[edge,None]))/alpha[edge,None],0,255).astype('uint8')
        a[edge,3]=(a[edge,3]*alpha[edge]).astype('uint8')
    return rgba_clean(Image.fromarray(a,'RGBA'))

def polygon_mask(size,points,scale=4):
    m=Image.new('L',(size[0]*scale,size[1]*scale));ImageDraw.Draw(m).polygon([(round(x*scale),round(y*scale)) for x,y in points],fill=255)
    return m.resize(size,Image.Resampling.LANCZOS)

def clipped(im,mask):
    a=np.array(im.convert('RGBA'));a[:,:,3]=(a[:,:,3].astype(float)*np.array(mask)/255).astype('uint8')
    return rgba_clean(Image.fromarray(a,'RGBA'))

def read_view(view):
    layout=json.loads((OLD/view/'layout.json').read_text(encoding='utf-8'))
    out=ROOT/view;(out/'parts').mkdir(parents=True,exist_ok=True)
    return layout,out

def source_part(view,r):
    return Image.open(OLD/view/'original_cutouts'/f'{r["name"]}.png').convert('RGBA')

def save_part(out,name,im,xy,record,method):
    im=rgba_clean(im);b=im.getbbox()
    if not b:raise ValueError('empty asset '+name)
    cropped=im.crop(b);pad=2
    canvas=Image.new('RGBA',(cropped.width+pad*2,cropped.height+pad*2));canvas.alpha_composite(cropped,(pad,pad))
    canvas.save(out/'parts'/f'{name}.png')
    result=dict(record);result['bbox_xywh']=[xy[0]+b[0]-pad,xy[1]+b[1]-pad,canvas.width,canvas.height]
    result['method']=method;result.pop('rig_bbox_xywh',None);result.pop('hidden_repair',None)
    return result

def checker(size,base=230,step=12):
    yy,xx=np.indices((size[1],size[0]));v=np.where((xx//step+yy//step)%2,base,base-18).astype('uint8')
    return Image.fromarray(np.dstack([v,v,v,np.full_like(v,255)]),'RGBA')

def finish_view(view,layout,records):
    out=ROOT/view
    result={'view':view,'canvas_size':layout['canvas_size'],'coordinate_convention':'top-left, x right, y down',
            'source_crop_xyxy':layout['source_crop_xyxy'],'parts':records}
    (out/'layout.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    W,H=layout['canvas_size'];assembled=Image.new('RGBA',(W,H))
    for r in records:assembled.alpha_composite(Image.open(out/'parts'/f'{r["name"]}.png'),tuple(r['bbox_xywh'][:2]))
    assembled.save(out/'assembled.png')
    for bg in ['#ffffff','#181c26']:
        p=Image.new('RGBA',(W,H),bg);p.alpha_composite(assembled);p.convert('RGB').save(out/('assembly-dark.jpg' if bg=='#181c26' else 'assembly-white.jpg'),quality=95)
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',12)
    for dark in [False,True]:
        tilew,tileh=230,245;cols=5;rows=math.ceil(len(records)/cols)
        sheet=Image.new('RGBA',(tilew*cols,tileh*rows),'#171c25' if dark else '#f3f5fa');d=ImageDraw.Draw(sheet)
        for i,r in enumerate(records):
            x=i%cols*tilew;y=i//cols*tileh;im=Image.open(out/'parts'/f'{r["name"]}.png');im.thumbnail((212,208),Image.Resampling.LANCZOS)
            tile=Image.new('RGBA',(218,214),'#171c25') if dark else checker((218,214))
            tile.alpha_composite(im,((218-im.width)//2,(214-im.height)//2));sheet.alpha_composite(tile,(x+6,y+3))
            d.text((x+7,y+224),r['name'],font=font,fill='#d8e3f5' if dark else '#23304a')
        sheet.convert('RGB').save(out/('parts-dark.jpg' if dark else 'parts-checker.jpg'),quality=96)
    return result
