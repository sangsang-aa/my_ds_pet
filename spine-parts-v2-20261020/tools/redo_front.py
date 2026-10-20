from asset_clean import *
from PIL import ImageOps,ImageFilter
layout,out=read_view('front');records={r['name']:r for r in layout['parts']}
spec=json.loads((OLD/'specs/front.json').read_text(encoding='utf-8'))
polys={r['name']:r['polygon'] for r in spec['parts']}
source=Image.open(OLD/'reference/original.png').convert('RGBA')
sx,sy=source.width/2048,source.height/1143
crop=layout['source_crop_xyxy'];source=source.crop(crop)
ready={}

def edge(im,hair=False):
    a=np.array(im.convert('RGBA'));rgb=a[:,:,:3].astype(float);m=a[:,:,3]>16
    if hair:m&=(rgb[:,:,2]>rgb[:,:,0]+7)&(rgb[:,:,2]>rgb[:,:,1]+4)&(rgb[:,:,0]<185)
    m=keep_components(m,.015,8);a[:,:,3]=np.where(m,a[:,:,3],0)
    inside=ndi.distance_transform_edt(m);donor=m&(inside>=2)&(rgb.max(2)<155)
    if donor.any():
        dist,near=ndi.distance_transform_edt(~donor,return_indices=True);f=rgb[near[0],near[1]]
        e=m&(inside<=1.5)&(dist<4)&(rgb.min(2)>105)
        den=255-f;alpha=np.clip(((255-rgb)*den).sum(2)/np.maximum((den*den).sum(2),1),0,1);e&=alpha<.96
        a[e,:3]=np.clip((rgb[e]-255*(1-alpha[e,None]))/np.maximum(alpha[e,None],.01),0,255).astype('uint8')
        a[e,3]=(a[e,3]*alpha[e]).astype('uint8')
    return rgba_clean(Image.fromarray(a,'RGBA'))

def supplement(name,view='front'):
    im=Image.open(OLD/view/'supplements'/f'{name}.png').convert('RGBA')
    return edge(im)

def add(name,im,xy,method,parent=None,pivot=None):
    r=dict(records.get(name,{'name':name,'parent':parent or 'root','pivot_xy':pivot or list(xy)}))
    if parent:r['parent']=parent
    if pivot:r['pivot_xy']=pivot
    r['note']='Clean v2 attachment. '+method
    ready[name]=(im,tuple(xy),r,method)

def original(name,hair=False):
    points=polys[name]
    if name=='side_hair_screen_right':
        points=[(min(x,588) if 380<y<445 else x,y) for x,y in points]
    pts=[(x*sx-crop[0],y*sy-crop[1]) for x,y in points]
    im=clipped(source,polygon_mask(source.size,pts))
    # Keep source-paint boundary for mixed features, complete polygon for hair.
    if not hair:
        r=records[name];old=source_part('front',r)
        im=old;xy=r['original_bbox_xywh'][:2]
    else:xy=(0,0)
    return edge(im,hair),xy

# Complete hair painting. Separating broad locks avoids fabric and eye fragments.
hair=supplement('rear_hair','back');hair=edge(hair,True)
hw,hh=hair.size
paths={
 'rear_hair_base':[(0,0),(hw,0),(hw,hh*.50),(hw*.76,hh*.64),(hw*.50,hh*.72),(hw*.24,hh*.64),(0,hh*.50)],
 'rear_hair_screen_left_outer':[(0,hh*.37),(hw*.31,hh*.40),(hw*.47,hh*.66),(hw*.52,hh),(0,hh)],
 'rear_hair_screen_right_outer':[(hw*.68,hh*.39),(hw,hh*.37),(hw,hh),(hw*.47,hh),(hw*.46,hh*.66)]}
for name,pts in paths.items():
    im=clipped(hair,polygon_mask(hair.size,pts)).resize((665,661),Image.Resampling.LANCZOS)
    add(name,im,(46,114),'Complete rear-hair restoration, separated into broad mesh locks; no old fragments.')

pts=[(170,450),(179,402),(190,351),(212,300),(248,270),(298,247),(347,235),(410,228),(475,239),(520,260),(558,290),(583,325),(598,368),(580,390),(580,440),(510,440),(310,440)]
scalp=clipped(source,polygon_mask(source.size,[(x*sx-crop[0],y*sy-crop[1]) for x,y in pts]))
scalp=edge(scalp,True);b=scalp.getbbox();scalp=scalp.crop(b);scalp=scalp.resize((scalp.width+32,scalp.height+20),Image.Resampling.LANCZOS)
capxy=(b[0]-16,b[1]-13)
boundary=[(95,370),(115,320),(126,280),(150,230),(185,193),(230,162),(280,145),(340,133),(404,136),(465,162),(520,209),(567,270),(590,360),(590,430),(95,430)]
scalp=clipped(scalp,polygon_mask(scalp.size,[(x-capxy[0],y-capxy[1]) for x,y in boundary]))
add('front_hair_cap',scalp,capxy,'Original painted scalp under the front locks, slightly enlarged inside the headband overlap.',parent='rear_hair_base',pivot=[351,230])

# One continuous tail is preferable to three truncated, contaminated wedges.
tail=supplement('tail_complete').resize((286,171),Image.Resampling.LANCZOS)
add('tail_trunk',tail,(447,754),'Complete restored tail; use a short mesh bone chain.')

for name in ['boot_screen_left','boot_screen_right']:
    im,xy=original(name)
    a=np.array(im);rgb=a[:,:,:3].astype(int);m=a[:,:,3]>20
    # Remove fragments above the cuff, retaining the full boot and its pompom.
    if name=='boot_screen_left':pts=[(2,31),(13,22),(19,12),(35,8),(63,7),(89,8),(107,12),(110,43),(110,96),(0,96)]
    else:pts=[(0,15),(10,10),(37,8),(63,8),(81,12),(90,18),(106,24),(111,36),(107,56),(101,65),(96,95),(0,95)]
    im=clipped(im,polygon_mask(im.size,pts));xy=[xy[0],xy[1]-12];add(name,im,xy,'Original boot contour cleaned.',parent='root')

# Newly restored sleeveless garment; no pasted sleeve remains underneath.
coat=Image.open(ROOT/'generated/front-torso-clean.png').convert('RGBA');aa=np.array(coat);aa[aa[:,:,3]<20]=0;coat=Image.fromarray(aa,'RGBA');coat=coat.crop(coat.getbbox())
coat=edge(coat).resize((462,341),Image.Resampling.LANCZOS)
add('coat_torso',coat,(124,536),'Sudorelay complete sleeveless torso redraw, including patterned skirt and fur hem. Use mesh deformation.')

# Collar uses a complete restoration; top is hidden by the face.
collar=supplement('collar','back').resize((320,99),Image.Resampling.LANCZOS)
add('collar',collar,(195,493),'Complete fur collar restoration; no cut-out bow hole.')

# Full mitten hands remain below the separate sleeves.
for side,xy in [('left',(123,701)),('right',(562,702))]:
    hand=supplement('hand_screen_'+side).resize((36,44),Image.Resampling.LANCZOS)
    add('hand_screen_'+side,hand,xy,'Complete simple chibi hand restoration.')

# Whole single-segment sleeve includes fur cuff. Natural seam can be meshed.
sleeve=supplement('sleeve').resize((139,171),Image.Resampling.LANCZOS)
add('sleeve_screen_left',sleeve,(105,572),'Complete sleeve and cuff restoration; no original clothing fragments.')
add('sleeve_screen_right',ImageOps.mirror(sleeve),(475,574),'Mirrored complete sleeve and cuff restoration.')

for name in ['cord_screen_left','cord_screen_right','pompom_screen_left','pompom_screen_right','chest_star']:
    im,xy=original(name);add(name,im,xy,'Original ornament, alpha and white-matte edge cleanup.')
# Bow and its star must be continuous under their attachment overlap.
name='chest_bow';pts=[(x*sx-crop[0],y*sy-crop[1]) for x,y in polys[name]]
bow=clipped(source,polygon_mask(source.size,pts));a=np.array(bow);rgb=a[:,:,:3].astype(int)
blue=(rgb[:,:,2]>rgb[:,:,0]+7)&(rgb[:,:,2]>rgb[:,:,1]+3)&(rgb[:,:,0]<135)
a[:,:,3]=np.where(blue,a[:,:,3],0);bow=edge(Image.fromarray(a,'RGBA'))
add(name,bow,(0,0),'Original bow re-extracted from the reference; star remains separate.')

face=supplement('face_blank');face=face.crop((0,0,face.width,336)).resize((318,320),Image.Resampling.LANCZOS)
add('face_base',face,(197,208),'Complete clean blank face restoration; no old skin fragments or hair shards.')

# Preserve the original iris artwork, but crop the eyelash silhouette deliberately.
eye_polys={
 'eye_screen_left':[(1,34),(17,26),(30,14),(44,12),(68,10),(82,12),(90,17),(94,26),(99,40),(98,59),(95,73),(88,85),(77,90),(59,90),(47,84),(34,75),(26,62),(24,43),(17,55),(10,52),(3,43)],
 'eye_screen_right':[(7,22),(15,13),(35,8),(63,7),(83,11),(96,18),(109,29),(107,42),(99,54),(95,34),(89,55),(84,69),(72,79),(51,81),(33,76),(22,66),(17,50),(18,34),(23,23)]}
for name,pts in eye_polys.items():
    r=records[name];x,y,w,h=r['original_bbox_xywh'];im=source.crop((x,y,x+w,y+h))
    im=clipped(im,polygon_mask(im.size,pts));a=np.array(im);m=keep_components(a[:,:,3]>20,.08,10)
    a[:,:,3]=np.where(m,a[:,:,3],0);im=Image.fromarray(a,'RGBA')
    add(name,im,r['original_bbox_xywh'][:2],'Original iris and lashes, manually bounded silhouette, no colour extrusion.')

# Minimal chibi mouth drawn with the source tone, replacing the tiny pink rectangle.
im=Image.new('RGBA',(80,32));d=ImageDraw.Draw(im);d.line([(21,14),(55,14)],fill=(153,104,112,200),width=5);im=im.resize((20,8),Image.Resampling.LANCZOS)
add('mouth',im,(341,490),'Clean short chibi mouth stroke.')

for name in ['fin_screen_left','fin_screen_right']:
    im,xy=original(name);add(name,im,xy,'Original fin ornament with cleaned boundary.')

for name in ['side_hair_screen_left','side_hair_screen_right','fringe_screen_left','fringe_center','fringe_screen_right','ahoge']:
    im,xy=original(name,True)
    a=np.array(im);m=keep_components(a[:,:,3]>20,.05,20);a[:,:,3]=np.where(m,a[:,:,3],0);im=Image.fromarray(a,'RGBA')
    add(name,im,xy,'Re-extracted original blue hair within a dedicated contour; skin, fur and red lashes excluded.')

for name in ['headband','snowflake','head_bow_screen_right']:
    im,xy=original(name)
    if name=='snowflake':
        pts=[(0,46),(8,35),(8,24),(19,23),(22,14),(27,18),(30,10),(42,17),(48,22),(55,22),(58,27),(57,42),(61,49),(69,58),(70,69),(65,76),(57,77),(53,86),(48,91),(39,87),(30,87),(25,81),(20,81),(17,76),(7,75),(5,68),(6,59),(1,51)]
        im=clipped(im,polygon_mask(im.size,pts))
    if name=='head_bow_screen_right':
        a=np.array(im);rgb=a[:,:,:3].astype(int);m=(a[:,:,3]>16)&~((rgb.min(2)>165)&(rgb.max(2)-rgb.min(2)<70))
        m=ndi.binary_fill_holes(ndi.binary_closing(m,iterations=1));a[:,:,3]=np.where(m,a[:,:,3],0);im=Image.fromarray(a,'RGBA')
    add(name,im,xy,'Original ornament with isolated contour and edge decontamination.')

# Keep visible overlaps in source order. Parent links refer only to exported bones.
order=['tail_trunk','rear_hair_base','rear_hair_screen_left_outer','rear_hair_screen_right_outer','boot_screen_left','boot_screen_right','coat_torso','collar','hand_screen_left','hand_screen_right','sleeve_screen_left','sleeve_screen_right','cord_screen_left','cord_screen_right','pompom_screen_left','pompom_screen_right','chest_bow','chest_star','face_base','eye_screen_left','eye_screen_right','mouth','fin_screen_left','fin_screen_right','front_hair_cap','side_hair_screen_left','side_hair_screen_right','fringe_screen_left','fringe_center','fringe_screen_right','ahoge','headband','snowflake','head_bow_screen_right']
order.remove('front_hair_cap');order.insert(order.index('face_base'),'front_hair_cap')
output=[]
for i,name in enumerate(order):
    im,xy,r,method=ready[name];r['draw_order']=i
    if r['parent'] not in order:r['parent']='root'
    output.append(save_part(out,name,im,xy,r,method))
finish_view('front',layout,output)
print(json.dumps({'view':'front','count':len(output),'output':str(out)}))
