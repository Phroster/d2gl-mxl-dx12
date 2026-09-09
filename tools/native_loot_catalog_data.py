"""Installed item identities and visual priorities. No price/affix estimates."""
import collections,csv,hashlib,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
STYLES=['Beam','Rune','Gem','Arcane','Shrine','Signet','Rare','Base','Scythe','Relic','Quest','Orb','Treasure']
# Colours: blue, gold, green, violet, red, white.
PROFILES=[('None',0,0,'Beam'),
 ('RuneLow',1,0,'Rune'),('RuneHigh',2,1,'Rune'),('RuneEnchanted',3,3,'Rune'),('RuneGreat',4,4,'Rune'),
 ('RuneFire',3,4,'Rune'),('RuneStone',3,1,'Rune'),('RuneArcane',3,3,'Rune'),('RunePoison',3,2,'Rune'),('RuneLight',3,5,'Rune'),('RuneIce',3,0,'Rune'),('RuneXis',4,5,'Rune'),
 ('GemLesser',1,0,'Gem'),*[(f'Gem{n}',2,i,'Gem') for i,n in enumerate(['Blue','Gold','Green','Violet','Red','White'])],('GemCluster',3,5,'Gem'),
 ('ArcaneShard',2,3,'Arcane'),('ArcaneCrystal',2,0,'Arcane'),('ArcaneCluster',3,3,'Arcane'),('CorruptShard',2,4,'Arcane'),('CorruptCrystal',3,4,'Arcane'),
 ('Shrine',2,2,'Shrine'),('Signet',1,1,'Signet'),('Learning',2,1,'Signet'),('GreaterSignet',3,1,'Signet'),
 ('Supply',1,0,'Orb'),('Oil',1,1,'Orb'),('RareOil',2,1,'Orb'),('MysticOrb',1,3,'Orb'),('UniqueOrb',4,3,'Orb'),
 ('Unique',2,1,'Beam'),('SacredUnique',3,1,'Beam'),('Set',2,2,'Beam'),('SacredSet',3,2,'Beam'),
 ('Rare',1,1,'Rare'),('SacredRare',2,1,'Rare'),('SacredBase',1,0,'Base'),('CraftBase',2,0,'Base'),
 ('ScytheBase',2,0,'Scythe'),('ScytheUnique',3,1,'Scythe'),('ScytheSet',3,2,'Scythe'),
 ('Angelic',4,5,'Beam'),('Mastercrafted',4,4,'Beam'),('Relic',4,3,'Relic'),('Effigy',3,3,'Relic'),('Ultimate',4,5,'Relic'),
 ('Quest',2,5,'Quest'),('Trophy',3,1,'Quest'),('Fragment',2,1,'Quest'),
 ('Cycle',2,3,'Shrine'),('LargeCycle',3,3,'Shrine'),('GoldenCycle',4,1,'Shrine'),
 ('Essence',2,0,'Arcane'),('Special',3,3,'Quest'),('Scroll',2,3,'Quest'),
 ('Cache',2,1,'Treasure'),('Treasure',3,1,'Treasure'),('CraftScroll',3,3,'Shrine'),('Dye',2,3,'Orb'),
 ('HealingPotion',1,4,'Orb'),('ManaPotion',1,0,'Orb')]
PROFILE_ID={p[0]:i for i,p in enumerate(PROFILES)}
CRAFT={'Ancient Armor','Light Plate','Leather Gloves','Gauntlets','Boots','Greaves','Light Belt','Plated Belt','Circlet','Diadem','Warp Blade','Naginata','Reflex Bow','Recurve Bow','Stinger Crossbow','War Scepter','Flamen Staff','Bonesplitter','Blackguard Helm','Hundsgugel','Setzschild','Gilded Shield','Aerin Shield','Ceremonial Armor','Scythe','Raptor Scythe'}
SUPERIOR={'Warp Blade','Naginata','Reflex Bow','Recurve Bow','Stinger Crossbow','War Scepter','Crystal Sword','Kriegsmesser','Ancient Armor','Light Plate','Diadem','Plated Belt','Greaves','Gauntlets','Scythe','Raptor Scythe'}
ROUTINE={'elx','hrt','brz','jaw','eyz','hrn','tal','flg','fng','qll','sol','scz','spe','tch','hrb','ear','tbk','ibk','tsc','isc','key','vic','mec'}
ULTIMATE={'f702','ir80','ir90','ir70','ir75','f665','basd','b79X','vry5'}
GREAT_RUNES={'r51','r52','r53','r54','r55','r56'}

def base_profile(x):
 cs=set(x['classes']);name=re.sub('ÿc.','',x['name']).strip();low=name.lower();code=x['code_text']
 if code in {'vic','mec'}:return 'Supply'
 if re.fullmatch(r'hp[1-5]',code):return 'HealingPotion'
 if re.fullmatch(r'mp[1-5]',code):return 'ManaPotion'
 if low in {'unused','invisible'} or code in ROUTINE:return 'None'
 if x['tier']=='Angelic':return 'Angelic'
 if x['tier']=='Mastercrafted':return 'Mastercrafted'
 if code in ULTIMATE:return 'Ultimate'
 if x['table']!='misc':return 'Quest' if x['quest'] else 'None'
 if 28 in cs:
  if code in GREAT_RUNES or 30 in cs:return 'RuneGreat'
  if code=='r98':return 'RuneXis'
  if 29 in cs:return 'RuneEnchanted'
  if 31 in cs:return {'r57':'RuneFire','r58':'RuneStone','r59':'RuneArcane','r60':'RunePoison','r61':'RuneLight','r62':'RuneIce'}.get(code,'RuneArcane')
  return 'RuneHigh' if x['required_level']>=55 else 'RuneLow'
 if 70 in cs:return 'ArcaneShard'
 if 71 in cs:return 'ArcaneCluster' if 'cluster' in low else 'ArcaneCrystal'
 if 90 in cs:return 'CorruptShard'
 if 86 in cs:return 'CorruptCrystal'
 if 32 in cs:
  if 33 not in cs:return 'GemLesser'
  for key,colour in [('amethyst','Violet'),('topaz','Gold'),('sapphire','Blue'),('emerald','Green'),('ruby','Red'),('bloodstone','Red'),('onyx','Violet'),('turquoise','Blue'),('amber','Gold')]:
   if key in low:return 'Gem'+colour
  return 'GemWhite'
 if re.fullmatch(r'gc\d\d',code) or low.startswith('great ') and any(n in low for n in ['skull','stone','ruby','sapphire','topaz']):return 'GemCluster'
 if re.fullmatch(r'(rc|ec)\d\d',code):return 'RuneHigh' if code.startswith('rc') else 'RuneEnchanted'
 if 66 in cs:return 'UniqueOrb'
 if 84 in cs or code=='qrel':return 'Relic'
 if 89 in cs or re.fullmatch(r'ir8[1-9]',code):return 'Effigy'
 if 48 in cs or low.endswith(' vessel'):return 'Shrine'
 if 'signet' in low:
  if any(s in low for s in ['greater','skill']):return 'GreaterSignet'
  if 'learning' in low:return 'Learning'
  return 'Signet'
 if 69 in cs:return 'Learning'
 if 73 in cs:return 'Dye'
 if 75 in cs or 'trophy' in low:return 'Fragment' if 'fragment' in low else 'Trophy'
 if 76 in cs:return 'GoldenCycle' if 'golden' in low else 'LargeCycle' if 'large' in low else 'Cycle'
 if 80 in cs:return 'Special' if 'charged' in low else 'Essence'
 if 82 in cs or 91 in cs:return 'CraftScroll'
 if 85 in cs or 'essence' in low or code in {'f667','f668'}:return 'Essence'
 if 87 in cs:return 'Special'
 if 22 in cs or 83 in cs or x['quest']:return 'Quest'
 if 65 in cs:return 'MysticOrb'
 if 27 in cs:return 'Supply'
 if low.startswith('oil of '):return 'RareOil' if any(s in low for s in ['greater','intensity','conjuration','reflection']) else 'Oil'
 if 'catalyst' in low or code=='sic1':return 'Supply'
 if 'cache' in low:return 'Cache'
 if 'treasure' in low or code in {'bbox','obox','ltbx'}:return 'Treasure'
 if code.startswith('du') and code[2:].isdigit():return 'CraftScroll'
 if low.startswith('tenet ') or 'scroll' in low or 'tome' in low or 'book' in low:return 'Scroll'
 if any(s in low for s in ['shard','fragment','crystal','cluster']):return 'Essence'
 if any(s in low for s in ['belladonna','apple']) or code in {'abar','bbar','rbar','hnrx'}:return 'Supply'
 # Cover untyped endgame rewards by actual installed ID. Exclude utilities above.
 if x['table']=='misc' and not cs and low!='unused':return 'Special'
 return 'None'

def build(catalog,audit=None):
 c=json.loads(catalog.read_text())
 protected={28,33,48,66,69,70,71,73,75,76,80,82,83,84,85,86,87,90,91}
 for x in c['items']:
  if set(x['classes'])&protected and x['name'].lower() not in {'unused','invisible'}:
   assert base_profile(x)!='None',('Missing important material/reward',x['name'])
 for code in GREAT_RUNES:
  assert base_profile(next(x for x in c['items'] if x['code_text']==code))=='RuneGreat'
 lines=['// Generated by tools/make_native_loot_catalog.py. GPL-3.0-or-later.','// Native base order: weapons, armor, misc. Priorities are not market prices.',
 'enum class Style : unsigned char { '+','.join(STYLES)+' };','enum Profile : unsigned short { '+','.join('P_'+p[0] for p in PROFILES)+', ProfileCount };',
 'struct EffectProfile { unsigned char rank, colour; Style style; const char* name; };','inline constexpr EffectProfile profiles[] = {']
 for name,rank,colour,style in PROFILES:lines.append(f'    {{{rank},{colour},Style::{style},"{name}"}},')
 lines+=['};','struct Base { unsigned short profile; unsigned char sacred,craft,superior,scythe,tier,gear,jewel,disabled,potionGrade; };','inline constexpr Base bases[] = {']
 counts=dict(weapons=0,armor=0,misc=0);last=0;rows=[]
 for index,x in enumerate(c['items']):
  t=['weapons','armor','misc'].index(x['table']);assert t>=last and x['index']==counts[x['table']]
  last=t;counts[x['table']]+=1;name=x['name'].removesuffix(' (Sacred)');sacred=x['tier']=='Sacred';cs=set(x['classes'])
  profile=base_profile(x);disabled=x['name'].lower() in {'unused','invisible'}
  values=[f'P_{profile}',int(sacred),int(sacred and name in CRAFT),int(sacred and name in SUPERIOR),int('scythe' in name.lower()),x['tier'] if isinstance(x['tier'],int) else 0,int(bool(cs&{2,5,6})),int(27 in cs),int(disabled)]
  values.append(int(x['code_text'][2]) if re.fullmatch(r'[hm]p[1-5]',x['code_text']) else 0)
  lines.append('    {'+','.join(map(str,values))+'}, // '+json.dumps(x['code_text'])+' '+json.dumps(x['name']))
  rows.append(dict(base=index,code=x['code_text'],name=x['name'],tier=x['tier'],profile=profile,rank=PROFILES[PROFILE_ID[profile]][1],classes=' '.join(map(str,x['classes'])),gear=values[6],scythe=values[4]))
 assert counts==dict(weapons=688,armor=476,misc=1304)
 lines+=['};',''];out=ROOT/'src/dx12/native_loot_catalog.generated.h';out.write_text('\n'.join(lines),encoding='utf-8')
 audit=Path(audit) if audit else ROOT/'build/native-loot-audit';audit.mkdir(parents=True,exist_ok=True)
 with (audit/'item-effect-coverage.csv').open('w',newline='',encoding='utf-8-sig') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 summary=dict(counts=counts,profiles=len(PROFILES)-1,default_profile_counts=dict(collections.Counter(r['profile'] for r in rows)),source_catalog_sha256=hashlib.sha256(catalog.read_bytes()).hexdigest(),sha256=hashlib.sha256(out.read_bytes()).hexdigest())
 (audit/'item-effect-catalog.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary))
