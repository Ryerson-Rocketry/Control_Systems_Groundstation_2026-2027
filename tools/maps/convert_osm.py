"""
Builds assets/maps/<site>.json, the offline street map the GPS tracker draws.

1. Download roads, water and place names around the launch site from
   OpenStreetMap's Overpass API (needs internet, run once):

   curl -s -A "MetRocketry-Groundstation" --data-urlencode 'data=[out:json][timeout:60];(
     way["highway"](42.665,-77.228,42.721,-77.153);
     way["natural"="water"](42.665,-77.228,42.721,-77.153);
     way["waterway"](42.665,-77.228,42.721,-77.153);
     node["place"](42.64,-77.25,42.74,-77.04););out geom;' \
     https://overpass-api.de/api/interpreter -o area.json

   The box is (south, west, north, east); widen it for a bigger map.

2. python3 tools/maps/convert_osm.py area.json assets/maps/urrg.json

3. Set 'origin' below to the launch site, then rebuild the app.

Map data © OpenStreetMap contributors, ODbL. The tracker shows this
attribution on screen, which the licence requires.
"""
# Converts an Overpass JSON download into the compact map file the GPS tracker reads.
import json, sys
src, dst = sys.argv[1], sys.argv[2]
d = json.load(open(src))
road_class = {'track': 0, 'service': 0, 'path': 0, 'footway': 0,
              'residential': 1, 'unclassified': 1,
              'tertiary': 2, 'secondary': 3, 'primary': 3}
def pts(e):
    out = []
    for g in e['geometry']:
        out += [round(g['lat'], 6), round(g['lon'], 6)]
    return out
roads, water, streams, places = [], [], [], []
for e in d['elements']:
    t = e.get('tags', {})
    if e['type'] == 'node':
        places.append({'n': t.get('name', ''), 'k': t.get('place', ''), 'p': [round(e['lat'], 6), round(e['lon'], 6)]})
    elif 'highway' in t and t['highway'] in road_class:
        r = {'c': road_class[t['highway']], 'p': pts(e)}
        if t.get('ref'): r['ref'] = t['ref']
        roads.append(r)
    elif t.get('natural') == 'water':
        water.append({'p': pts(e)})
    elif 'waterway' in t:
        streams.append({'p': pts(e)})
out = {
    'name': 'URRG',
    'region': 'PENN YAN, NY',
    'site': 'Torrey Farms, 4272 State Route 364, Potter NY (approximate)',
    'origin': [42.692791, -77.190618],
    'attribution': '© OpenStreetMap contributors',
    'roads': roads, 'water': water, 'streams': streams, 'places': places,
}
json.dump(out, open(dst, 'w'), separators=(',', ':'), ensure_ascii=False)
print(len(roads), 'roads', len(water), 'water', len(streams), 'streams', len(places), 'places')
