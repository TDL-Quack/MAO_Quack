# Dessine l'icône de MAO Quack : un canard dont le corps est fait de barres de niveau sonore.
from PIL import Image, ImageDraw
import sys
T = 1024
img = Image.new("RGBA", (T, T), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
fond, jaune, orange, sombre = (43, 43, 43, 255), (255, 214, 64, 255), (255, 150, 40, 255), (30, 30, 30, 255)
d.rounded_rectangle((32, 32, T - 32, T - 32), radius=200, fill=fond)
# Le corps : cinq barres arrondies, comme un indicateur de niveau, qui dessinent le dos et la queue
barres = [(170, 560), (290, 470), (410, 430), (530, 470), (650, 560)]
largeur, bas = 92, 800
for x, haut in barres:
    d.rounded_rectangle((x, haut, x + largeur, bas), radius=46, fill=jaune)
# La queue relevée : une barre plus courte et plus haute à gauche
d.rounded_rectangle((170, 500, 262, 640), radius=46, fill=jaune)
# La tête, posée sur les barres de droite
d.ellipse((545, 262, 805, 522), fill=jaune)
# Le bec
d.rounded_rectangle((755, 370, 915, 444), radius=34, fill=orange)
# L'oeil
d.ellipse((675, 342, 725, 392), fill=sombre)
# La ligne d'eau : une forme d'onde stylisée
pts = []
import math
for i in range(0, 761, 8):
    pts.append((132 + i, 868 + 22 * math.sin(i / 760 * math.pi * 6)))
d.line(pts, fill=orange, width=22, joint="curve")
img256 = img.resize((512, 512), Image.LANCZOS)
img256.save(sys.argv[1])
img.resize((128, 128), Image.LANCZOS).save(sys.argv[2])
