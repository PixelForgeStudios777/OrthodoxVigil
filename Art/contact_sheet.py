# Tiles PNGs into one sheet: blender -b --python contact_sheet.py -- out.png cols in1.png in2.png ...
import bpy, sys, numpy as np
a = sys.argv[sys.argv.index("--") + 1:]
out, cols, files = a[0], int(a[1]), a[2:]
imgs = []
for f in files:
    im = bpy.data.images.load(f)
    w, h = im.size
    px = np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4)
    imgs.append(px)
h = max(i.shape[0] for i in imgs); w = max(i.shape[1] for i in imgs)
rows = (len(imgs) + cols - 1) // cols
sheet = np.zeros((rows * h, cols * w, 4), dtype=np.float32); sheet[..., 3] = 1
for k, px in enumerate(imgs):
    r, c = divmod(k, cols)
    r = rows - 1 - r
    sheet[r * h:r * h + px.shape[0], c * w:c * w + px.shape[1]] = px
img = bpy.data.images.new("sheet", cols * w, rows * h)
img.pixels = sheet.ravel()
img.filepath_raw = out; img.file_format = 'PNG'; img.save()
print("SHEET", out)
