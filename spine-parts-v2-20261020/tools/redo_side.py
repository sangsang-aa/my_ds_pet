from pathlib import Path
import sys,json,math
from asset_clean import *

VIEW='side'
layout,out=read_view(VIEW)
byname={r['name']:r for r in layout['parts']}
(out/'audit').mkdir(exist_ok=True)
(ROOT/'prompts').mkdir(exist_ok=True)
(out/'generated').mkdir(exist_ok=True)

def contact(items,path,dark=True):
    cols=5;tw=240;th=270
    sheet=Image.new('RGBA',(cols*tw,math.ceil(len(items)/cols)*th),'#171c25' if dark else '#ffffff')
    d=ImageDraw.Draw(sheet);font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',12)
    for i,(name,im) in enumerate(items):
        im=im.copy();im.thumbnail((220,235),Image.Resampling.LANCZOS)
        x=i%cols*tw;y=i//cols*th
        sheet.alpha_composite(im,(x+(tw-im.width)//2,y+(240-im.height)//2))
        d.text((x+6,y+248),name,font=font,fill='white' if dark else '#202020')
    sheet.convert('RGB').save(path,quality=97)

def audit_sources():
    contact([(r['name'],source_part(VIEW,r)) for r in layout['parts']],out/'audit/source-dark.jpg')
    Image.open(OLD/'reference/original.png').crop(layout['source_crop_xyxy']).save(out/'audit/reference-side.png')

def extract_generated():
    im=Image.open(out/'generated/clothing-repair.png').convert('RGBA')
    targets={'coat_torso':((25,100,955,935),(470,450)),
             'collar':((745,45,1510,390),(1120,260)),
             'headband':((960,395,1480,995),(1248,710))}
    items=[]
    for name,(box,seed) in targets.items():
        tile=im.crop(box);a=np.array(tile);rgb=a[:,:,:3]
        ink=(rgb[:,:,0]<48)&(rgb[:,:,1]<45)&(rgb[:,:,2]<110)
        wall=ndi.binary_dilation(ink,iterations=1)
        lab,n=ndi.label(~wall)
        region=lab==lab[seed[1]-box[1],seed[0]-box[0]]
        mask=ndi.binary_dilation(region,iterations=4)
        if name=='headband':
            lab,n=ndi.label(ink)
            sizes=np.bincount(lab.ravel());sizes[0]=0
            border=np.unique(np.r_[lab[0],lab[-1],lab[:,0],lab[:,-1]])
            sizes[border]=0
            line=lab==sizes.argmax()
            mask=ndi.binary_fill_holes(ndi.binary_closing(line,iterations=1))
            mask=ndi.binary_dilation(mask,iterations=1)
        if name=='coat_torso':
            # A cloth body has no apertures. Interior painted dark folds must not
            # become transparent merely because the contour detector sees ink.
            for y,row in enumerate(mask):
                xs=np.where(row)[0]
                if len(xs):mask[y,xs.min():xs.max()+1]=True
        a[:,:,3]=mask.astype('uint8')*255
        result=Image.fromarray(a,'RGBA');result=result.crop(result.getbbox())
        result.save(out/'generated'/f'{name}.png');items.append((name,result))
    contact(items,out/'audit/generated-extract.jpg')

def mask_poly(im,points):
    return clipped(im,polygon_mask(im.size,points))

def smooth_inpaint(im,box):
    """Small retouch within an already-painted face; never expands the silhouette."""
    a=np.array(im).astype(float);x0,y0,x1,y1=box
    tile=a[y0-3:y1+3,x0-3:x1+3,:3].copy()
    target=np.zeros(tile.shape[:2],bool);target[3:-3,3:-3]=True
    tile[target]=np.median(tile[~target],axis=0)
    for _ in range(700):
        avg=ndi.convolve(tile,np.array([[0,.25,0],[.25,0,.25],[0,.25,0]])[:,:,None],mode='nearest')
        tile[target]=avg[target]
    a[y0-3:y1+3,x0-3:x1+3,:3]=tile
    return Image.fromarray(np.clip(a,0,255).astype('uint8'),'RGBA')

def generated_face():
    im=Image.open(OLD/'side/supplements/face_blank.png').convert('RGBA')
    a=np.array(im)
    # Retouch the tiny pink mouth mark only in the skin, preserving the cheek contour.
    for y in range(292,320):
        for x in range(12,49):
            if a[y,x,0]>185 and a[y,x,1]>135 and a[y,x,3]>150:
                a[y,x,:3]=np.mean(a[y,55:65,:3],axis=0)
    im=Image.fromarray(a,'RGBA')
    src=np.array(im);W,H=244,358
    tx=np.arange(W)+74;ty=np.arange(H)+206
    xx=np.interp(tx,[74,285,318],[0,216,im.width-1])
    yy=np.interp(ty,[206,420,447,461,511,564],[0,224,252,270,337,im.height-1])
    gy,gx=np.meshgrid(yy,xx,indexing='ij')
    a=np.stack([ndi.map_coordinates(src[:,:,i].astype(float),[gy,gx],order=1,mode='constant') for i in range(4)],2)
    result=Image.fromarray(a.astype('uint8'),'RGBA').crop((0,0,W,338))
    return clean_asset(result), (74,206)

def rear_parts():
    full=clean_asset(Image.open(OLD/'side/supplements/rear_hair.png').convert('RGBA'),'hair')
    polys={
      'rear_hair_base':[(125,25),(180,16),(270,30),(340,95),(382,150),(384,410),(338,425),(285,421),(218,425),(135,411),(145,360),(163,340),(163,265),(125,190),(125,130),(125,82)],
      'rear_hair_inner_lock':[(190,284),(244,286),(301,290),(322,330),(328,388),(326,431),(317,461),(323,486),(342,514),(340,548),(139,548),(139,394),(151,356),(175,326)],
      'rear_hair_outer_lock':[(318,244),(348,258),(385,280),(385,515),(327,515),(314,488),(310,466),(319,443),(328,415),(325,385),(315,350),(310,311)]}
    result={}
    # All three posterior layers share the same affine map, preserving exact line matches.
    for name,pts in polys.items():
        im=mask_poly(full,pts).resize((501,695),Image.Resampling.LANCZOS)
        result[name]=(clean_asset(im,'hair'),(90,71))
    return result

def generated_front_hair():
    full=clean_asset(Image.open(OLD/'side/supplements/rear_hair.png').convert('RGBA'),'hair')
    polys={
      'front_bangs_inner':([(94,65),(109,83),(118,126),(121,171),(122,215),(116,244),(97,221),(81,193),(73,158),(73,111),(80,85)],(129,152,80,253)),
      'front_hair_near':([(101,53),(126,42),(152,61),(174,97),(192,145),(199,206),(199,244),(187,295),(173,326),(157,351),(130,385),(111,417),(117,434),(99,427),(86,408),(80,387),(85,367),(103,346),(125,331),(101,341),(80,347),(105,328),(124,309),(100,316),(73,314),(94,302),(109,280),(120,253),(122,219),(119,175),(118,130),(110,90)],(162,137,145,460))}
    result={}
    for name,(pts,box) in polys.items():
        im=mask_poly(full,pts);im=im.crop(im.getbbox()).resize(tuple(box[2:]),Image.Resampling.LANCZOS)
        result[name]=(clean_asset(im,'hair'),tuple(box[:2]))
    return result

def generated_tail():
    full=clean_asset(Image.open(OLD/'side/supplements/tail_complete.png').convert('RGBA'))
    polys={
      'tail_trunk':[(0,0),(180,0),(240,112),(278,135),(306,137),(334,146),(321,165),(250,209),(0,209)],
      'tail_fluke_upper':[(280,0),(391,0),(391,86),(346,127),(322,158),(297,144),(278,102)],
      'tail_fluke_lower':[(313,132),(349,96),(481,76),(481,209),(310,209),(320,160)]}
    return {n:(mask_poly(full,p).resize((356,155),Image.Resampling.LANCZOS),(289,747)) for n,p in polys.items()}

def edge_unmatte(im):
    """Remove white matte only from existing antialiased contour pixels."""
    a=np.array(im.convert('RGBA'));rgb=a[:,:,:3].astype(float);m=a[:,:,3]>32
    inside=ndi.distance_transform_edt(m)
    donor=(inside>=1.5)&(rgb.max(2)<145)&(rgb[:,:,0]<110)&(a[:,:,3]>180)
    if not donor.any():return im
    dist,near=ndi.distance_transform_edt(~donor,return_indices=True)
    f=rgb[near[0],near[1]];denom=255-f
    alpha=np.clip(np.sum((255-rgb)*denom,axis=2)/np.maximum(np.sum(denom*denom,axis=2),1),0,1)
    edge=m&(inside<=1.5)&(dist<3.6)&(rgb.min(2)>90)&(rgb.mean(2)-f.mean(2)>35)&(alpha>.04)&(alpha<.93)
    a[edge,:3]=np.clip((rgb[edge]-255*(1-alpha[edge,None]))/alpha[edge,None],0,255)
    a[edge,3]=(a[edge,3]*alpha[edge]).astype('uint8')
    return rgba_clean(Image.fromarray(a,'RGBA'))

def fitted_headband(im):
    """Warp the complete new painted band to the original curved silhouette."""
    src=np.array(im);sa=src[:,:,3];target=source_part(VIEW,byname['headband']);ta=np.array(target)[:,:,3]
    H,W=ta.shape;ys=np.arange(H)
    left=np.array([np.where(row>80)[0].min() if (row>80).any() else 0 for row in ta],float)
    right=np.array([np.where(row>80)[0].max() if (row>80).any() else 0 for row in ta],float)
    left[153:207]=np.interp(np.arange(153,207),[152,207],[left[152],left[207]])
    left[262:295]=np.interp(np.arange(262,295),[261,295],[left[261],left[295]])
    left=ndi.gaussian_filter1d(left,1.2);right=ndi.gaussian_filter1d(right,1.2)
    a=np.zeros((H,W,4),dtype='uint8')
    for y in range(H):
        sy=y*(im.height-1)/(H-1);row=sa[round(sy)];valid=np.where(row>20)[0]
        if not len(valid):continue
        l,r=valid[0],valid[-1];tx=np.arange(W)
        sx=l+(tx-left[y])*(r-l)/max(1,right[y]-left[y])
        gy=np.full(W,sy)
        for c in range(4):a[y,:,c]=ndi.map_coordinates(src[:,:,c].astype(float),[gy,sx],order=1,mode='constant')
        a[y,(tx<left[y])|(tx>right[y]),3]=0
    return Image.fromarray(a,'RGBA')

def line_layer(name):
    if name=='eyebrow':
        im=Image.new('RGBA',(64*4,24*4));d=ImageDraw.Draw(im)
        pts=[(2,8),(12,6),(24,7),(36,10),(48,15),(59,20)]
        d.line([(x*4,y*4) for x,y in pts],fill='#332241',width=9,joint='curve')
        return im.resize((64,24),Image.Resampling.LANCZOS),(109,309)
    im=Image.new('RGBA',(27*4,12*4));d=ImageDraw.Draw(im)
    d.line([(4*4,6*4),(10*4,5*4),(16*4,5*4),(21*4,6*4)],fill='#a67b86',width=4,joint='curve')
    return im.resize((27,12),Image.Resampling.LANCZOS),(80,478)

def original_clean(name,r):
    im=source_part(VIEW,r);xy=tuple(r['original_bbox_xywh'][:2])
    kind='hair' if 'hair' in name or 'bang' in name or name=='ahoge' else 'mixed'
    im=clean_asset(im,kind,component_ratio=.02,opening=0)
    if name=='visible_eye':
        ox,oy=layout['source_crop_xyxy'][:2];x,y=xy
        im=Image.open(OLD/'reference/original.png').convert('RGBA').crop((ox+x,oy+y,ox+x+im.width,oy+y+im.height))
        im=mask_poly(im,[(3,19),(13,10),(27,9),(43,11),(51,9),(65,16),(76,26),(72,33),(61,44),(56,69),(47,91),(37,101),(22,101),(12,90),(7,64),(5,36),(0,31)])
    elif name=='snowflake':
        ox,oy=layout['source_crop_xyxy'][:2];x,y=xy
        im=Image.open(OLD/'reference/original.png').convert('RGBA').crop((ox+x,oy+y,ox+x+im.width,oy+y+im.height))
        im=mask_poly(im,[(43,3),(54,1),(62,11),(77,12),(89,22),(90,34),(103,41),(111,53),(104,65),(92,67),(84,80),(75,82),(66,94),(54,87),(42,94),(34,84),(28,78),(25,67),(18,63),(22,49),(18,40),(25,29),(35,28),(35,12)])
        a=np.array(im);rgb=a[:,:,:3].astype(int)
        for _ in range(2):
            edge=ndi.distance_transform_edt(a[:,:,3]>20)<=1.5
            a[edge&(rgb.min(2)>160),3]=0
        im=Image.fromarray(a,'RGBA')
    elif name=='coat_fur_trim':
        im=mask_poly(im,[(66,56),(104,57),(91,101),(82,153),(81,178),(120,186),(196,187),(274,174),(329,154),(375,150),(389,150),(393,164),(384,183),(356,192),(338,210),(299,222),(251,234),(210,240),(155,241),(102,234),(61,222),(43,207),(20,206),(5,192),(0,173),(7,157),(21,148),(29,151),(40,159),(53,118)])
        a=np.array(im);rgb=a[:,:,:3].astype(int)
        fur=(rgb[:,:,0]>138)&(rgb[:,:,1]>125)&(rgb[:,:,2]>150)&(a[:,:,3]>80)
        a[~ndi.binary_dilation(fur,iterations=2),3]=0
        im=Image.fromarray(a,'RGBA')
    elif name=='visible_cuff':
        # Remove skin-colored fingertips mistakenly included along the bottom center.
        a=np.array(im);rgb=a[:,:,:3].astype(int);skin=(rgb[:,:,0]-rgb[:,:,2]>12)&(rgb[:,:,0]>155)
        a[skin,3]=0;im=Image.fromarray(a,'RGBA')
    elif name=='front_bangs_outer':
        # Nose-contour tail below the final blue hair point is not part of the bang.
        a=np.array(im);a[301:,:,3]=0
        ref=Image.open(OLD/'reference/original.png').convert('RGBA')
        ox,oy=layout['source_crop_xyxy'][:2];x,y=xy
        raw=np.array(ref.crop((ox+x,oy+y,ox+x+im.width,oy+y+im.height)))
        yy,xx=np.indices(a.shape[:2]);col=raw[:,:,:3].astype(float)
        actual=(yy>176)&(yy<206)&(xx>85)&(xx<130)&(col[:,:,2]-col[:,:,0]>14)&(col[:,:,2]-col[:,:,1]>5)&(col[:,:,0]<160)
        a[actual]=raw[actual]
        im=Image.fromarray(a,'RGBA')
    elif name=='front_bangs_inner':
        # The eyebrow extraction removed a small wedge of real hair. Restore that
        # wedge from the original painted material, then retouch the line locally.
        a=np.array(im);ref=Image.open(OLD/'reference/original.png').convert('RGBA')
        ox,oy=layout['source_crop_xyxy'][:2];x,y=xy
        raw=np.array(ref.crop((ox+x,oy+y,ox+x+im.width,oy+y+im.height)))
        yy,xx=np.indices(a.shape[:2]);region=(yy>150)&(yy<190)&(xx<43)
        col=raw[:,:,:3].astype(float)
        real=region&(col[:,:,2]-col[:,:,0]>14)&(col[:,:,2]-col[:,:,1]>5)&(col[:,:,0]<160)
        a[real]=raw[real]
        known=a[:,:,:3].astype(float);valid=a[:,:,3]>80
        repair=(yy>155)&(yy<185)&(xx<43)&valid
        known[repair]=np.median(known[(yy>150)&(yy<190)&(xx>45)&(xx<60)&valid],axis=0)
        weights=ndi.convolve(valid.astype(float),np.array([[0,.25,0],[.25,0,.25],[0,.25,0]]),mode='nearest')
        for _ in range(500):
            avg=ndi.convolve(known*valid[:,:,None],np.array([[0,.25,0],[.25,0,.25],[0,.25,0]])[:,:,None],mode='nearest')/np.maximum(weights[:,:,None],1e-5)
            known[repair]=avg[repair]
        a[repair,:3]=np.clip(known[repair],0,255)
        a[169:185,:3,3]=0
        im=Image.fromarray(a,'RGBA')
    elif name in ('cord_pompom_back','cord_pompom_front'):
        a=np.array(im);rgb=a[:,:,:3].astype(int)
        yy,xx=np.indices(a.shape[:2])
        # The light area next to the strings was coat fabric, not a ribbon.
        upper=yy<(45 if name.endswith('back') else 39)
        a[upper&(rgb[:,:,0]>130),3]=0
        im=Image.fromarray(a,'RGBA')
        if name=='cord_pompom_back':
            donor=source_part(VIEW,byname['cord_pompom_front']).crop((0,39,61,93))
            donor=clean_asset(donor).resize((47,49),Image.Resampling.LANCZOS)
            a=np.array(im);a[43:,:,3]=0;im=Image.fromarray(a,'RGBA');im.alpha_composite(donor,(0,43))
    elif name=='visible_sleeve':
        im=mask_poly(im,[(17,15),(29,4),(56,0),(83,4),(101,25),(109,63),(122,120),(113,127),(96,116),(26,115),(4,127),(0,123),(7,58)])
        a=np.array(im);yy,xx=np.indices(a.shape[:2]);rgb=a[:,:,:3].astype(int)
        a[(yy>im.height-20)&(rgb.min(2)>150),3]=0;im=Image.fromarray(a,'RGBA')
    elif name=='head_bow':
        a=np.array(im);rgb=a[:,:,:3].astype(int);yy,xx=np.indices(a.shape[:2])
        a[(yy<17)&(xx>25)&(xx<53)&(rgb.min(2)>165),3]=0;im=Image.fromarray(a,'RGBA')
    elif name=='chest_bow':
        im=mask_poly(im,[(33,2),(43,4),(49,10),(41,32),(34,61),(25,64),(10,59),(2,47),(4,33),(17,24)])
    elif name=='fin_ornament':
        im=mask_poly(im,[(5,23),(20,27),(30,27),(43,8),(49,3),(65,17),(97,34),(130,47),(158,51),(161,58),(150,67),(132,70),(118,79),(98,84),(78,85),(62,90),(40,85),(21,75),(11,62),(5,44)])
    return clean_asset(im,kind,component_ratio=.02),xy

def build():
    replacements=rear_parts()
    replacements.update(generated_tail())
    replacements['face_ear_neck']=generated_face()
    for name,box in {'coat_torso':(85,503,355,322),'collar':(113,483,224,114),'headband':(161,62,272,325)}.items():
        im=Image.open(out/'generated'/f'{name}.png').convert('RGBA')
        if name=='collar':im=im.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        if name=='headband':im=im.rotate(20,expand=True,resample=Image.Resampling.BICUBIC)
        im=im.crop(im.getbbox())
        replacements[name]=(clean_asset(im.resize(tuple(box[2:]),Image.Resampling.LANCZOS)),tuple(box[:2]))
    hand=clean_asset(Image.open(OLD/'side/supplements/hand.png').convert('RGBA')).rotate(90,expand=True)
    replacements['visible_hand']=(hand.resize((55,57),Image.Resampling.LANCZOS),(222,688))
    for name in ['eyebrow','mouth']:replacements[name]=line_layer(name)
    records=[]
    for r in layout['parts']:
        name=r['name']
        if name in replacements:
            im,xy=replacements[name];method='完整插画替换；原坐标适配；无原碎片叠加'
        else:
            im,xy=original_clean(name,r);method='原画轮廓精修；去白边、邻件色和碎片；无颜色外扩'
        records.append(save_part(out,name,edge_unmatte(im),xy,r,method))
    # The complete face skin sits below posterior hair; the ear opening is in the
    # rear-hair contour so a painted scalp never leaks through the accessory gap.
    face=next(r for r in records if r['name']=='face_ear_neck')
    records.remove(face);records.insert(6,face)
    for i,r in enumerate(records):r['draw_order']=i
    finish_view(VIEW,layout,records)
    # Fixed four-item review pages show every asset against both white and dark.
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',14)
    for page in range(7):
        sheet=Image.new('RGBA',(1024,900),'white');d=ImageDraw.Draw(sheet)
        for j,r in enumerate(records[page*4:page*4+4]):
            bx=j%2*512;by=j//2*450
            im=Image.open(out/'parts'/f'{r["name"]}.png').convert('RGBA')
            scale=min(234/im.width,405/im.height)
            im=im.resize((max(1,round(im.width*scale)),max(1,round(im.height*scale))),Image.Resampling.LANCZOS)
            for side,bg in enumerate(['white','#171c25']):
                tile=Image.new('RGBA',(256,420),bg)
                tile.alpha_composite(im,((256-im.width)//2,(420-im.height)//2))
                sheet.alpha_composite(tile,(bx+side*256,by))
            d.text((bx+7,by+426),r['name'],font=font,fill='#202020')
        sheet.convert('RGB').save(out/'audit'/f'final-parts-{page+1}.jpg',quality=98)
    checks=[]
    for r in records:
        im=Image.open(out/'parts'/f'{r["name"]}.png');a=np.array(im);b=im.getbbox()
        assert b==(2,2,im.width-2,im.height-2),(r['name'],b,im.size)
        assert im.size==tuple(r['bbox_xywh'][2:])
        assert not a[a[:,:,3]==0,:3].any(),r['name']
        checks.append({'name':r['name'],'size':list(im.size),'transparent_margin_px':2})
    (out/'audit/validation.json').write_text(json.dumps({'part_count':len(records),'all_checks_passed':True,'parts':checks},indent=2),encoding='utf-8')

if __name__=='__main__':
    audit_sources()
    if (out/'generated/clothing-repair.png').exists():extract_generated()
    if (out/'generated/coat_torso.png').exists():build()
