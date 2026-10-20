from asset_clean import *
l,o=read_view('front')
out=Image.new('RGB',(1250,1100),'#181c26');d=ImageDraw.Draw(out)
names=['side_hair_screen_left','side_hair_screen_right','fringe_screen_left','fringe_center','fringe_screen_right','eye_screen_left','eye_screen_right','sleeve_screen_left','sleeve_screen_right','headband','chest_bow','collar','fin_screen_left','fin_screen_right','tail_fluke_upper']
for i,n in enumerate(names):
 im=source_part('front',next(r for r in l['parts'] if r['name']==n));im.thumbnail((230,320))
 out.paste(im,(i%5*250,i//5*365),im);d.text((i%5*250,i//5*365+330),n,fill='white')
out.save(ROOT/'qa/front-old-detail.png')
im=Image.open(ROOT/'generated/front-torso-clean.png');bg=Image.new('RGBA',im.size,'white');bg.alpha_composite(im);bg.convert('RGB').save(ROOT/'qa/torso-white.jpg')
