from asset_clean import *
from write_psd import write_psd
from psd_tools import PSDImage
import csv,zipfile,hashlib

reports=[]
for view in ['front','side','back']:
    out=ROOT/view;layout=json.loads((out/'layout.json').read_text(encoding='utf-8'))
    rs=layout['parts'];W,H=layout['canvas_size'];byname={r['name']:r for r in rs}
    layers=[];audit=[]
    for r in rs:
        p=out/'parts'/f'{r["name"]}.png';im=Image.open(p).convert('RGBA');x,y,w,h=r['bbox_xywh']
        assert im.size==(w,h),(p,im.size,(w,h))
        box=im.getbbox();assert box is not None
        padding=[box[0],box[1],w-box[2],h-box[3]]
        assert padding==[2,2,2,2],(p,padding)
        a=np.array(im);assert not np.any(a[a[:,:,3]==0,:3]),p
        lab,n=ndi.label(a[:,:,3]>32,np.ones((3,3)));sizes=np.bincount(lab.ravel())[1:]
        audit.append({'name':r['name'],'size':[w,h],'padding':padding,'alpha_components_over_8_pixels':int((sizes>8).sum())})
        layers.append({'name':r['name'],'image':im,'offset':(x,y)})
    write_psd(out/f'{view}-clean.psd',(W,H),layers)
    psd=PSDImage.open(out/f'{view}-clean.psd')
    assert len(psd)==len(rs)
    for r,layer in zip(rs,psd):
        im=layer.topil().convert('RGBA');expected=Image.open(out/'parts'/f'{r["name"]}.png').convert('RGBA')
        assert im.size==expected.size
        assert np.array_equal(np.array(im),np.array(expected)),r['name']
        assert list(layer.offset)==r['bbox_xywh'][:2]

    # A setup skeleton holds only bones and region attachments. No weights or animation are claimed.
    origin=(W/2,H-30);body=(W/2,H*.69);head=(W/2,H*.54)
    world={'root':origin,'body':body,'head':head}
    bones=[{'name':'root'},{'name':'body','parent':'root','x':0,'y':round(origin[1]-body[1],3)},
           {'name':'head','parent':'body','x':0,'y':round(body[1]-head[1],3)}];visiting=set()
    def emit(name):
        if name in world:return
        assert name not in visiting,('parent cycle',name)
        visiting.add(name);r=byname[name];parent=r.get('parent','root')
        if parent not in byname:
            parent='head' if any(k in name for k in ['hair','face','headband','ahoge','eye','ear','fin_','snowflake','head_bow']) else 'body'
        emit(parent);p=r.get('pivot_xy',r['bbox_xywh'][:2]);q=world[parent]
        bones.append({'name':name,'parent':parent,'x':round(p[0]-q[0],3),'y':round(q[1]-p[1],3)})
        world[name]=p;visiting.remove(name)
    for r in rs:emit(r['name'])
    slots=[];attachments={}
    for r in rs:
        n=r['name'];x,y,w,h=r['bbox_xywh'];px,py=world[n]
        slots.append({'name':n,'bone':n,'attachment':n})
        attachments[n]={n:{'type':'region','path':n,'x':round(x+w/2-px,3),'y':round(py-y-h/2,3),'width':w,'height':h}}
    data={'skeleton':{'spine':'4.2.00','x':-W/2,'y':-30,'width':W,'height':H,'images':'./parts/','audio':''},
          'bones':bones,'slots':slots,'skins':[{'name':'default','attachments':attachments}],'animations':{}}
    (out/f'{view}-setup-spine42.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    with (out/'parts.csv').open('w',encoding='utf-8-sig',newline='') as f:
        wr=csv.writer(f);wr.writerow(['name','draw_order','parent','left','top','width','height','pivot_x','pivot_y','method'])
        for i,r in enumerate(rs):wr.writerow([r['name'],i,r['parent'],*r['bbox_xywh'],*r['pivot_xy'],r['method']])
    (out/'validation.json').write_text(json.dumps({'parts':audit,'psd_pixel_roundtrip':'passed','setup_json':'references and dimensions checked; not opened in Spine'},indent=2),encoding='utf-8')
    reports.append({'view':view,'parts':len(rs),'psd_layers':len(psd),'png_alpha_padding':'2 pixels each side','pixel_roundtrip':'passed'})

# A concise view of the three completed assemblies, on a contrasting background.
overview=Image.new('RGB',(1490,850),'#181c26');d=ImageDraw.Draw(overview)
font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',20)
for i,view in enumerate(['front','side','back']):
    im=Image.open(ROOT/view/'assembled.png').convert('RGBA');im.thumbnail((470,790),Image.Resampling.LANCZOS)
    overview.paste(im,(i*495+(490-im.width)//2,35),im);d.text((i*495+20,815),view.upper(),fill='#d8e3f5',font=font)
overview.save(ROOT/'preview.jpg',quality=96)

examples=[('front','side_hair_screen_left'),('front','eye_screen_left'),('front','sleeve_screen_left'),('front','coat_torso'),('back','hair_cap'),('side','coat_torso')]
comparison=Image.new('RGB',(1200,1000),'#181c26');d=ImageDraw.Draw(comparison)
small=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',15)
for i,(view,name) in enumerate(examples):
    tx=i%3*400;ty=i//3*500
    for j,(label,path) in enumerate([('OLD',OLD/view/'rig_parts'/f'{name}.png'),('V2',ROOT/view/'parts'/f'{name}.png')]):
        im=Image.open(path).convert('RGBA');im=im.crop(im.getbbox());im.thumbnail((184,410),Image.Resampling.LANCZOS)
        comparison.paste(im,(tx+j*200+(190-im.width)//2,ty+35+(420-im.height)//2),im)
        d.text((tx+j*200+12,ty+8),label,fill='#c5d3ec',font=small)
    d.text((tx+12,ty+470),view+'/'+name,fill='white',font=small)
comparison.save(ROOT/'before-after.jpg',quality=96)

counts='、'.join(f"{x['view']} {x['parts']} 件" for x in reports)
readme=f'''# 鲸鱼少女 Spine 静态拆件 · 返工版

共 {sum(x['parts'] for x in reports)} 个透明 PNG：{counts}。

## 从哪里开始

先看 preview.jpg 和每个视角的 parts-dark.jpg。深色底预览用于看白边，PNG 本身都是透明背景。
每个视角包含：
- parts/：裁紧的 RGBA PNG，外侧只保留 2 像素透明留边。
- *-clean.psd：同一画布上按实际位置排好的独立图层，方便继续修图。
- *-setup-spine42.json：按 Spine 4.2 数据结构写出的初始骨骼与 region 附件，图片路径为同级 parts/。
- layout.json / parts.csv：坐标、层序、建议轴心、父级与补绘来源。
- assembled.png：重组预览，保留透明背景。

## 此次修改

停用了旧版的宽边缘扩张，以及把邻近像素塞回切片的办法。补绘部件用完整图替换，避免原切片碎片叠在新底色上产生脏点。原图中仍可完整提取的眼睛、刘海及饰物按轮廓重新裁切。
正面袖子与毛袖口、正面整条尾巴合并，适合再加网格变形；头发中难分的交叉发环按发束合并。没有为增加数量而保留缺失严重的小碎层。

## 使用范围与已知差异

这是静态美术拆件和初始定位数据，不是已经绑定好权重的 Spine 工程，也不包含动作关键帧。Spine 4.2 JSON 已检查图片引用、层序、尺寸和父级，尚未在 Spine 编辑器中实机导入验证。
遮挡区与缺损处采用静态补绘，因此部分发束形状、衣服印花、毛领和袖子与原图存在差异。PSD 可继续细修。三个视角是三套独立二维素材，不能直接通过骨骼旋转互相变成另一视角。
画布坐标以左上角为原点，x 向右、y 向下；left/right 指画面左右。建议先完成正面绑定，再为侧面及背面配置替换附件或独立骨架。
小幅摆动可使用骨骼与网格；大幅抬臂、转头等仍需按实际动作检查遮挡和关节搭接。

## 验证

已检查 RGBA、透明边距、坐标、PSD 图层数及逐像素往返一致性，并查看白底/深底组装和散件预览。validation.json 记录了逐件结果。文件检查不等于 Spine 运行验证。

## 生成记录

需要补全的静态部件使用 sudorelay-imagegen 技能，通过 Sudorelay API 调用 gpt-image-2.5-sunburst。此次新增提示词保留在 prompts/；其余完整补绘沿用了上一轮生成素材。未使用 AI 视频生成。
'''
(ROOT/'README.md').write_text(readme,encoding='utf-8')
(ROOT/'validation.json').write_text(json.dumps(reports,ensure_ascii=False,indent=2),encoding='utf-8')

archive=ROOT.parent/'whale-girl-spine-parts-v2.zip'
files=[ROOT/'README.md',ROOT/'preview.jpg',ROOT/'before-after.jpg',ROOT/'validation.json',*sorted((ROOT/'prompts').glob('*.txt'))]
for view in ['front','side','back']:
    out=ROOT/view;layout=json.loads((out/'layout.json').read_text(encoding='utf-8'))
    files.extend(out/'parts'/f'{r["name"]}.png' for r in layout['parts'])
    files.extend(out/name for name in ['layout.json','parts.csv','validation.json','assembled.png','assembly-dark.jpg','assembly-white.jpg','parts-dark.jpg','parts-checker.jpg',f'{view}-clean.psd',f'{view}-setup-spine42.json'])
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for f in files:z.write(f,f.relative_to(ROOT))
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
print(json.dumps({'archive':str(archive),'bytes':archive.stat().st_size,'files':len(files),'reports':reports},ensure_ascii=False,indent=2))
