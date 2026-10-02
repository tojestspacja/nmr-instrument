"""ASCII STL -> gzip(header + float32 positions + uint32 indices), vertices merged at 0.01 mm."""
import gzip, re, struct, sys
from pathlib import Path
import numpy as np
out = Path("../m"); out.mkdir(parents=True, exist_ok=True)
for f in sorted(Path("meshes").glob("*.stl")):
    txt = f.read_text()
    v = np.array(re.findall(r"vertex\s+(\S+)\s+(\S+)\s+(\S+)", txt), dtype=np.float64)
    q = np.round(v * 100).astype(np.int64)
    uniq, inv = np.unique(q, axis=0, return_inverse=True)
    pos = (uniq / 100).astype(np.float32)
    idx = inv.astype(np.uint32).ravel()
    blob = struct.pack("<II", len(pos), len(idx)) + pos.tobytes() + idx.tobytes()
    gz = gzip.compress(blob, 9)
    import base64
    (out / f"{f.stem}.txt").write_text(base64.b64encode(gz).decode("ascii"), encoding="ascii")   # .txt: hosts that refuse .bin serve it
    lo, hi = pos.min(0), pos.max(0)
    print(f"{f.stem:12} tris {len(idx)//3:7}  {len(gz)/1e6:6.2f} MB  bbox {lo.round(1)} .. {hi.round(1)}")
