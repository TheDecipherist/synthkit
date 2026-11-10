import fs from 'fs';
const file = process.argv[2];
const txt = fs.readFileSync(file, 'utf8');
const blocks = [...txt.matchAll(/```mermaid\n([\s\S]*?)\n```/g)].map(m => m[1]);
const { JSDOM } = await import('jsdom');
const dom = new JSDOM('<!doctype html><html><body></body></html>');
global.window = dom.window; global.document = dom.window.document;
const mermaid = (await import('mermaid')).default;
mermaid.initialize({ startOnLoad: false });
let bad = 0;
for (let i = 0; i < blocks.length; i++) {
  try { await mermaid.parse(blocks[i]); console.log(`  diagram ${i + 1}: ok`); }
  catch (e) { bad++; console.log(`  diagram ${i + 1}: FAILS -> ${String(e.message).split('\n')[0]}`); 
    const m = String(e.message).match(/line (\d+)/i);
    if (m) { const L = blocks[i].split('\n'); const n = +m[1]; 
      for (let j = Math.max(0, n-2); j < Math.min(L.length, n+1); j++) console.log(`      ${j+1}: ${L[j]}`); } }
}
console.log(`\n  ${blocks.length} diagram(s), ${bad} failing`);
