# Transforme docs/Guide.md en une page web autonome (assets/Guide.html).
import markdown, sys
src = open(sys.argv[1], encoding="utf-8").read()
corps = markdown.markdown(src, extensions=["tables", "toc", "sane_lists"])
css = """
:root { --fond:#2b2b2b; --panneau:#353535; --texte:#e6e6e6; --doux:#a8a8a8; --accent:#ffb532; --trait:#4a4a4a; }
* { box-sizing:border-box; }
body { margin:0; background:var(--fond); color:var(--texte); font:17px/1.6 "Segoe UI",system-ui,sans-serif; }
main { max-width:860px; margin:0 auto; padding:32px 24px 80px; }
h1 { color:var(--accent); font-size:2.2em; margin:0 0 .3em; }
h2 { color:var(--accent); border-bottom:1px solid var(--trait); padding-bottom:.25em; margin-top:2.2em; }
h3 { margin-top:1.8em; } h4 { color:var(--accent); margin-top:1.5em; }
a { color:var(--accent); }
code { background:var(--panneau); padding:.1em .35em; border-radius:4px; font-size:.92em; }
table { border-collapse:collapse; width:100%; margin:1em 0; font-size:.95em; }
th, td { border:1px solid var(--trait); padding:.5em .7em; text-align:left; vertical-align:top; }
th { background:var(--panneau); color:var(--accent); }
blockquote { margin:1.2em 0; padding:.6em 1em; background:var(--panneau); border-left:4px solid var(--accent); border-radius:4px; }
blockquote p { margin:0; }
hr { border:0; border-top:1px solid var(--trait); margin:2.5em 0; }
.toc { background:var(--panneau); border-radius:6px; padding:.8em 1.2em; font-size:.95em; }
.toc ul { margin:.2em 0; padding-left:1.2em; }
li { margin:.25em 0; }
@media print { body { background:#fff; color:#111; } h1,h2,h4,th,a { color:#b36b00; } code,th,blockquote,.toc { background:#f0f0f0; } }
"""
html = f"""<!DOCTYPE html>
<html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Guide de MAO Quack</title><style>{css}</style></head><body><main>{corps}</main></body></html>"""
open(sys.argv[2], "w", encoding="utf-8").write(html)
