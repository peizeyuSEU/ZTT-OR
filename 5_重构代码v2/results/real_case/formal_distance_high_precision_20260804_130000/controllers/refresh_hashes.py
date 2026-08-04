import hashlib,json,sys
from pathlib import Path
R=Path(sys.argv[1])
for d in sorted(R.glob('D*/seed_*')):
 files=[p for p in d.iterdir() if p.is_file() and p.name!='HASH_MANIFEST.json']
 (d/'HASH_MANIFEST.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(files)},indent=2)+'\n')
