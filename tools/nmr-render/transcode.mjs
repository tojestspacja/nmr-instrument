// Transcode the canonical packed scene meshes (legacy/simulator/m/*.txt — the SAME files the Three.js site loads,
// themselves generated from the OpenSCAD by legacy/simulator/build/pack.py) into one little-endian scene.bin that the
// native renderer reads. This is a 1:1 re-container of the canonical geometry: no dimensions are redefined here.
//
//   scene.bin layout (LE):  magic "NMRS" | u32 nparts | per part: u32 nameLen, name, u32 nv, u32 ni,
//                           nv*3 f32 position, ni u32 index
import { readFileSync, writeFileSync, readdirSync } from 'node:fs';
import { gunzipSync } from 'node:zlib';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
const MDIR = join(here, '..', '..', 'legacy', 'simulator', 'm');
const OUT = join(here, 'assets', 'scene.bin');

const ids = readdirSync(MDIR).filter((f) => f.endsWith('.txt')).map((f) => f.replace('.txt', '')).sort();
const parts = [];
for (const id of ids) {
  const raw = gunzipSync(Buffer.from(readFileSync(join(MDIR, id + '.txt'), 'utf8').trim(), 'base64'));
  const nv = raw.readUInt32LE(0), ni = raw.readUInt32LE(4);
  const pos = Buffer.from(raw.buffer, raw.byteOffset + 8, nv * 12);
  const idx = Buffer.from(raw.buffer, raw.byteOffset + 8 + nv * 12, ni * 4);
  parts.push({ id, nv, ni, pos, idx });
}

const chunks = [];
const u32 = (v) => { const b = Buffer.alloc(4); b.writeUInt32LE(v >>> 0); return b; };
chunks.push(Buffer.from('NMRS', 'ascii'), u32(parts.length));
for (const p of parts) {
  const name = Buffer.from(p.id, 'ascii');
  chunks.push(u32(name.length), name, u32(p.nv), u32(p.ni), p.pos, p.idx);
}
writeFileSync(OUT, Buffer.concat(chunks));
const tris = parts.reduce((s, p) => s + p.ni / 3, 0);
console.log(`scene.bin: ${parts.length} parts, ${tris} triangles, ${(Buffer.concat(chunks).length / 1024 | 0)} KiB`);
console.log('parts:', parts.map((p) => p.id).join(', '));
