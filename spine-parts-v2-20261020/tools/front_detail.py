from asset_clean import *
layout,out=read_view('front'); rr={r['name']:r for r in layout['parts']}
names=['eye_screen_left','eye_screen_right','chest_bow','snowflake','boot_screen_right','side_hair_screen_right']
for name in names:
 im=source_part('front',rr[name]);bg=Image.new('RGBA',im.size,'#181c26');bg.alpha_composite(im);bg=bg.resize((im.width*5,im.height*5))
 d=ImageDraw.Draw(bg)
 for x in range(0,im.width,10):d.line([(x*5,0),(x*5,bg.height)],fill=(255,200,0,90));d.text((x*5,0),str(x),fill='red')
 for y in range(0,im.height,10):d.line([(0,y*5),(bg.width,y*5)],fill=(255,200,0,90));d.text((0,y*5),str(y),fill='red')
 bg.save(ROOT/'qa'/f'{name}-grid.png')
