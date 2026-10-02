#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Genera las barras de progreso del README (estilo decomp.dev) desde docs/progress.json. Uso: tools/progress.py
Escribe docs/progress/progress_<id>_<lang>.svg y reemplaza el bloque <!-- progress:start --> ... <!-- progress:end --> de README.md y README.es.md.
EN: builds the README progress bars from docs/progress.json (single source); done = verified against game evidence, wip = implemented but unverified."""
import json, os, re
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
DATA = json.load(open(os.path.join(ROOT, 'docs', 'progress.json'), encoding='utf-8'))
L = {'en': dict(verified='Verified', wip='Implemented, unverified', missing='Missing', area='Area', note='Detail', overall='Overall',
                head='Downhill Domination: {v:.1f}% verified, {i:.1f}% implemented', avg='Average of all rows. Verified = checked against evidence from the game (savestates, the game\'s own camera, bit-identical reference). Implemented = works but unverified or based on hypotheses. Counted rows show real units; the rest are rough maintainer estimates.',
                est='est.', link='Source data: [`docs/progress.json`](docs/progress.json), regenerate with `python3 tools/progress.py`.'),
     'es': dict(verified='Verificado', wip='Implementado, sin verificar', missing='Pendiente', area='Área', note='Detalle', overall='Total',
                head='Downhill Domination: {v:.1f}% verificado, {i:.1f}% implementado', avg='Promedio de todas las filas. Verificado = comprobado con evidencia del juego (savestates, la cámara del propio juego, referencia idéntica bit a bit). Implementado = funciona pero sin verificar o basado en hipótesis. Las filas con unidades muestran cifras reales; el resto son estimaciones aproximadas de los mantenedores.',
                est='est.', link='Datos de origen: [`docs/progress.json`](docs/progress.json), se regeneran con `python3 tools/progress.py`.')}
CSS = ('<style>text{font:13px system-ui,-apple-system,Segoe UI,Roboto,sans-serif;fill:#1f2328}.d{fill:#8b949e}.t{fill:#d0d7de}.v{fill:#2da44e}.w{fill:#bf8700}'
       '@media(prefers-color-scheme:dark){text{fill:#e6edf3}.d{fill:#8b949e}.t{fill:#30363d}.v{fill:#3fb950}.w{fill:#d29922}}</style>')

def pct(r, k): return 100.0 * r[k] / r['total']
def label(r, lang):
    if r['exact']: return f"{r['done']} / {r['total']} {r['unit'][lang]}" if not r['wip'] else f"{r['done']}+{r['wip']} / {r['total']} {r['unit'][lang]}"
    return f"{pct(r, 'done'):.0f}%" + (f" (+{pct(r, 'wip'):.0f}%)" if r['wip'] else '') + f" {L[lang]['est']}"
def svg(sec, lang):
    rows = sec['rows']; W, H0, RH = 820, 34, 28; H = H0 + RH * len(rows) + 8; bx, bw = 330, 270
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img" aria-label="{sec["title"][lang]}">', CSS,
         f'<text x="0" y="20" font-weight="600">{sec["title"][lang]}</text>']
    for i, r in enumerate(rows):
        y = H0 + RH * i; d, w = bw * r['done'] / r['total'], bw * r['wip'] / r['total']
        o.append(f'<text x="0" y="{y + 15}">{r["name"][lang]}</text><rect class="t" x="{bx}" y="{y + 4}" width="{bw}" height="14" rx="3"/>')
        if d: o.append(f'<rect class="v" x="{bx}" y="{y + 4}" width="{d:.1f}" height="14" rx="3"/>')
        if w: o.append(f'<rect class="w" x="{bx + d:.1f}" y="{y + 4}" width="{w:.1f}" height="14" rx="3"/>')
        o.append(f'<text class="d" x="{bx + bw + 12}" y="{y + 15}">{label(r, lang)}</text>')
    o.append('</svg>'); return '\n'.join(o)
def overall():
    rows = [r for s in DATA['sections'] for r in s['rows']]
    return sum(pct(r, 'done') for r in rows) / len(rows), sum(pct(r, 'done') + pct(r, 'wip') for r in rows) / len(rows)
def legend(lang):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="560" height="24" viewBox="0 0 560 24">{CSS}<rect class="v" x="0" y="5" width="14" height="14" rx="3"/><text x="22" y="17">{L[lang]["verified"]}</text>'
            f'<rect class="w" x="150" y="5" width="14" height="14" rx="3"/><text x="172" y="17">{L[lang]["wip"]}</text><rect class="t" x="390" y="5" width="14" height="14" rx="3"/><text x="412" y="17">{L[lang]["missing"]}</text></svg>')
def block(lang):
    v, i = overall(); t = L[lang]; out = [f'**{t["head"].format(v=v, i=i)}**', '', t['avg'], '', f'<img src="docs/progress/legend_{lang}.svg" alt="{t["verified"]} / {t["wip"]} / {t["missing"]}">', '']
    for s in DATA['sections']:
        out += [f'<img src="docs/progress/progress_{s["id"]}_{lang}.svg" alt="{s["title"][lang]}">', '']
    out += ['<details><summary>' + t['note'] + '</summary>', '', f'| {t["area"]} | {t["note"]} |', '|---|---|']
    out += [f'| {r["name"][lang]} | {r["note"][lang]} |' for s in DATA['sections'] for r in s['rows']]
    out += ['', '</details>', '', t['link']]
    return '\n'.join(out)
if __name__ == '__main__':
    d = os.path.join(ROOT, 'docs', 'progress'); os.makedirs(d, exist_ok=True)
    for lang in ('en', 'es'):
        for s in DATA['sections']: open(os.path.join(d, f'progress_{s["id"]}_{lang}.svg'), 'w', encoding='utf-8').write(svg(s, lang))
        open(os.path.join(d, f'legend_{lang}.svg'), 'w', encoding='utf-8').write(legend(lang))
        p = os.path.join(ROOT, 'README.md' if lang == 'en' else 'README.es.md'); s = open(p, encoding='utf-8').read()
        new = re.sub(r'<!-- progress:start -->.*?<!-- progress:end -->', lambda m: '<!-- progress:start -->\n' + block(lang) + '\n<!-- progress:end -->', s, flags=re.S)
        assert new != s or '<!-- progress:start -->' in s, f'faltan los marcadores en {p}'
        open(p, 'w', encoding='utf-8').write(new)
    v, i = overall(); print(f'{v:.1f}% verified, {i:.1f}% implemented')
