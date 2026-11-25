# Procedural textures (numpy inside Blender's Python) + portraits cropped from the concept art.
# Run: blender -b --factory-startup --python Art/build_textures.py
import bpy, os, sys, math
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "Textures")
os.makedirs(OUT, exist_ok=True)
CONCEPT = os.path.join(os.path.expanduser("~"), "Downloads", "Orthodox Vigil", "Concept Art")
rng = np.random.default_rng(543)


def save(name, rgb, alpha=None, linear=False):
    h, w = rgb.shape[:2]
    a = np.ones((h, w), np.float32) if alpha is None else alpha.astype(np.float32)
    px = np.dstack([np.clip(rgb, 0, 1).astype(np.float32), np.clip(a, 0, 1)])
    img = bpy.data.images.new(name, w, h, alpha=True)
    if linear:
        img.colorspace_settings.name = 'Non-Color'
    img.pixels = px[::-1].ravel()          # numpy row 0 = top of the picture
    img.filepath_raw = os.path.join(OUT, name + ".png")
    img.file_format = 'PNG'
    img.save()
    bpy.data.images.remove(img)
    print("TEX", name, w, h)


def noise(n, cells, seed, octaves=5, persist=0.55):
    """Tileable fractal value noise in 0..1."""
    r = np.random.default_rng(seed)
    out = np.zeros((n, n), np.float32)
    amp, tot = 1.0, 0.0
    c = cells
    for o in range(octaves):
        g = r.random((c, c)).astype(np.float32)
        y = np.linspace(0, c, n, endpoint=False)
        y0 = np.floor(y).astype(int); fy = y - y0
        fy = fy * fy * (3 - 2 * fy)
        y1 = (y0 + 1) % c
        a = g[y0][:, y0] * (1 - fy)[None, :] + g[y0][:, y1] * fy[None, :]
        b = g[y1][:, y0] * (1 - fy)[None, :] + g[y1][:, y1] * fy[None, :]
        layer = a * (1 - fy)[:, None] + b * fy[:, None]
        out += layer * amp
        tot += amp
        amp *= persist
        c *= 2
    return out / tot


def normal_from_height(h, strength=3.0):
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * strength
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * strength
    nz = np.ones_like(h)
    l = np.sqrt(dx * dx + dy * dy + nz * nz)
    # image rows go top->bottom, so +V is -row: flip dy for an OpenGL-style map, Unreal flips green on import
    return np.dstack([(-dx / l) * 0.5 + 0.5, (dy / l) * 0.5 + 0.5, (nz / l) * 0.5 + 0.5])


def col(c):
    return np.array(c, np.float32)[None, None, :]


def lerp(a, b, t):
    return a + (b - a) * t[..., None]


N = 1024
yy, xx = np.mgrid[0:N, 0:N].astype(np.float32) / N

# ---------------------------------------------------------------- mud brick (UV 1 = 2 m)
def mudbrick():
    rows = 16          # 12.5 cm courses over 2 m
    bricks = 6         # 33 cm bricks
    v = yy * rows
    row = np.floor(v)
    u = xx * bricks + (row % 2) * 0.5
    fu, fv = u - np.floor(u), v - row
    mortar = np.minimum(np.minimum(fu, 1 - fu) * 3.0 / bricks * 12, np.minimum(fv, 1 - fv) * 2.0)
    mortar = np.clip(mortar * 6.0, 0, 1)
    bid = (np.floor(u) * 7.31 + row * 13.7) % 1.0
    n1 = noise(N, 8, 1)
    n2 = noise(N, 32, 2, octaves=4)
    base = lerp(col((0.42, 0.31, 0.21)), col((0.55, 0.43, 0.30)), np.clip(bid * 0.7 + n1 * 0.5, 0, 1))
    base *= (0.85 + 0.3 * n2)[..., None]
    mortar_col = col((0.5, 0.42, 0.33)) * (0.9 + 0.2 * n2)[..., None]
    rgb = lerp(mortar_col, base, mortar)
    # plaster render worn away in patches
    patch = np.clip((noise(N, 4, 3) - 0.52) * 8, 0, 1)
    plaster = col((0.62, 0.53, 0.42)) * (0.9 + 0.15 * n2)[..., None]
    rgb = lerp(rgb, plaster, patch * 0.85)
    h = mortar * (1 - patch * 0.7) * 0.6 + n2 * 0.4
    save("T_MudBrick_D", rgb)
    save("T_MudBrick_N", normal_from_height(h, 4.0), linear=True)


def plaster():
    n1 = noise(N, 6, 11); n2 = noise(N, 48, 12, octaves=3)
    stain = np.clip((noise(N, 3, 13) - 0.45) * 2.5, 0, 1) * (1 - yy) ** 0.5
    rgb = col((0.86, 0.84, 0.78)) * (0.9 + 0.12 * n1 + 0.06 * n2)[..., None]
    rgb = lerp(rgb, col((0.58, 0.52, 0.42)), stain * 0.5)
    save("T_Plaster_D", rgb)
    save("T_Plaster_N", normal_from_height(n2 * 0.6 + n1 * 0.4, 2.5), linear=True)


def stone():
    # limestone blocks 50 x 33 cm (UV 1 = 2 m)
    rows = 6; cols_ = 4
    v = yy * rows; row = np.floor(v)
    u = xx * cols_ + (row % 2) * 0.37
    fu, fv = u - np.floor(u), v - row
    joint = np.clip(np.minimum(np.minimum(fu, 1 - fu) * 40, np.minimum(fv, 1 - fv) * 26), 0, 1)
    bid = (np.floor(u) * 5.17 + row * 3.3) % 1.0
    n1 = noise(N, 16, 21); n2 = noise(N, 64, 22, octaves=3)
    rgb = lerp(col((0.5, 0.46, 0.4)), col((0.66, 0.61, 0.53)), np.clip(bid * 0.6 + n1 * 0.6, 0, 1))
    rgb *= (0.85 + 0.25 * n2)[..., None]
    rgb = lerp(col((0.22, 0.2, 0.18)), rgb, joint)
    save("T_Stone_D", rgb)
    save("T_Stone_N", normal_from_height(joint * 0.5 + n2 * 0.5, 4.0), linear=True)


def ground():
    # UV 1 = 4 m
    n1 = noise(N, 4, 31); n2 = noise(N, 32, 32); n3 = noise(N, 128, 33, octaves=2)
    rgb = lerp(col((0.30, 0.24, 0.17)), col((0.46, 0.38, 0.27)), np.clip(n1 * 1.2 - 0.1, 0, 1))
    rgb *= (0.8 + 0.35 * n2)[..., None]
    pebbles = np.clip((n3 - 0.72) * 10, 0, 1)
    rgb = lerp(rgb, col((0.55, 0.5, 0.42)), pebbles * 0.7)
    grass = np.clip((noise(N, 6, 34) - 0.58) * 5, 0, 1)
    rgb = lerp(rgb, col((0.22, 0.25, 0.13)), grass * 0.55 * n2)
    save("T_Ground_D", rgb)
    save("T_Ground_N", normal_from_height(n2 * 0.5 + pebbles * 0.5 + n3 * 0.2, 3.0), linear=True)


def wood():
    n1 = noise(N, 8, 41); n2 = noise(N, 64, 42, octaves=3)
    planks = 5
    u = xx * planks
    pid = np.floor(u)
    grain = np.sin((yy * 30 + n1 * 6 + pid * 1.7) * math.pi * 2) * 0.5 + 0.5
    gap = np.clip(np.minimum(u - pid, 1 - (u - pid)) * 60, 0, 1)
    rgb = lerp(col((0.22, 0.14, 0.08)), col((0.42, 0.29, 0.17)), np.clip(grain * 0.5 + ((pid * 0.37) % 1) * 0.5, 0, 1))
    rgb *= (0.8 + 0.3 * n2)[..., None]
    rgb = lerp(col((0.06, 0.04, 0.03)), rgb, gap)
    save("T_Wood_D", rgb)
    save("T_Wood_N", normal_from_height(grain * 0.3 + gap * 0.5 + n2 * 0.2, 2.5), linear=True)


def rockwall():
    n1 = noise(N, 6, 51); n2 = noise(N, 24, 52); n3 = noise(N, 96, 53, octaves=3)
    h = n1 * 0.5 + n2 * 0.35 + n3 * 0.15
    rgb = lerp(col((0.27, 0.24, 0.2)), col((0.5, 0.45, 0.37)), np.clip(h * 1.4 - 0.2, 0, 1))
    save("T_RockWall_D", rgb)
    save("T_RockWall_N", normal_from_height(h, 6.0), linear=True)


def thatch():
    n1 = noise(N, 8, 61); n2 = noise(N, 128, 62, octaves=2)
    streak = noise(N, 4, 63)
    s = np.sin((xx * 220 + n1 * 30) * math.pi) * 0.5 + 0.5
    h = s * 0.6 + n2 * 0.4
    rgb = lerp(col((0.25, 0.19, 0.1)), col((0.5, 0.4, 0.22)), np.clip(h * 0.8 + streak * 0.4 - 0.1, 0, 1))
    save("T_Thatch_D", rgb)
    save("T_Thatch_N", normal_from_height(h, 3.0), linear=True)


def parchment():
    n1 = noise(N, 4, 71); n2 = noise(N, 64, 72, octaves=3)
    edge = np.clip(np.minimum(np.minimum(xx, 1 - xx), np.minimum(yy, 1 - yy)) * 12, 0, 1)
    rgb = col((0.8, 0.72, 0.56)) * (0.82 + 0.18 * n1 + 0.08 * n2)[..., None]
    rgb = lerp(col((0.42, 0.3, 0.18)), rgb, edge ** 0.6)
    save("T_Parchment", rgb)


def closed_eye(name, size=512, stroke=(0.16, 0.15, 0.14), alpha_mult=0.92):
    """The sign drawn in ash on every lintel: a circle with one vertical stroke, like an eye closed top to bottom."""
    n = size
    y, x = np.mgrid[0:n, 0:n].astype(np.float32) / n - 0.5
    rough = noise(n, 16, 81, octaves=4) - 0.5
    r = np.sqrt(x * x + y * y)
    ring = np.clip(1 - np.abs(r - 0.31 + rough * 0.03) / (0.035 + rough * 0.02), 0, 1)
    line = np.clip(1 - np.abs(x + rough * 0.02 + 0.01 * np.sin(y * 20)) / (0.028 + rough * 0.012), 0, 1) * (np.abs(y) < 0.4)
    a = np.clip(np.maximum(ring, line) * 1.6, 0, 1)
    smear = noise(n, 8, 82)
    a *= np.clip(0.55 + smear * 0.7, 0, 1) * alpha_mult
    rgb = col(stroke) * (0.8 + 0.4 * smear)[..., None] * np.ones((n, n, 1), np.float32)
    save(name, rgb, a)


def altar_cloth():
    """Dark red altar cloth with the closed eye embroidered in pale thread."""
    n = 1024
    y, x = np.mgrid[0:n, 0:n].astype(np.float32) / n - 0.5
    weave = (np.sin(x * n * 0.9) * np.sin(y * n * 0.9)) * 0.5 + 0.5
    n1 = noise(n, 8, 91)
    rgb = col((0.28, 0.05, 0.05)) * (0.75 + 0.25 * weave + 0.2 * n1)[..., None]
    r = np.sqrt(x * x + y * y)
    ring = np.clip(1 - np.abs(r - 0.25) / 0.02, 0, 1)
    line = np.clip(1 - np.abs(x) / 0.016, 0, 1) * (np.abs(y) < 0.32)
    border = np.clip(1 - np.abs(np.maximum(np.abs(x), np.abs(y)) - 0.44) / 0.012, 0, 1)
    thread = np.maximum(np.maximum(ring, line), border * 0.8)
    rgb = lerp(rgb, col((0.72, 0.65, 0.5)) * (0.8 + 0.2 * weave)[..., None], thread)
    save("T_AltarCloth", rgb)


def flame_ring():
    """HUD candle-flame ring for the crucifix standoff and prayer knots."""
    n = 256
    y, x = np.mgrid[0:n, 0:n].astype(np.float32) / n - 0.5
    r = np.sqrt(x * x + y * y)
    a = np.clip(1 - np.abs(r - 0.42) / 0.05, 0, 1) ** 1.5
    rgb = np.ones((n, n, 3), np.float32) * col((1.0, 0.75, 0.4))
    save("T_RingSoft", rgb, a)
    g = np.clip(1 - r / 0.5, 0, 1) ** 2
    save("T_Glow", np.ones((n, n, 3), np.float32), g)


def vignette():
    n = 512
    y, x = np.mgrid[0:n, 0:n].astype(np.float32) / n - 0.5
    r = np.sqrt(x * x + y * y) / 0.7071
    a = np.clip((r - 0.35) / 0.65, 0, 1) ** 1.6
    save("T_Vignette", np.zeros((n, n, 3), np.float32), a)


def crop(src, box, name, out_size=512, feather=True):
    """Crop a region of a concept image into a square portrait with soft dark edges."""
    img = bpy.data.images.load(os.path.join(CONCEPT, src))
    w, h = img.size
    px = np.array(img.pixels[:], np.float32).reshape(h, w, 4)[::-1]   # row 0 = top
    x0, y0, x1, y1 = box
    part = px[y0:y1, x0:x1, :3]
    # nearest-neighbour resize then a small blur pass would do; use simple bilinear via index maps
    ys = np.linspace(0, part.shape[0] - 1, out_size)
    xs = np.linspace(0, part.shape[1] - 1, out_size)
    y0i = np.floor(ys).astype(int); x0i = np.floor(xs).astype(int)
    y1i = np.minimum(y0i + 1, part.shape[0] - 1); x1i = np.minimum(x0i + 1, part.shape[1] - 1)
    fy = (ys - y0i)[:, None, None]; fx = (xs - x0i)[None, :, None]
    a = part[y0i][:, x0i]; b = part[y0i][:, x1i]; c = part[y1i][:, x0i]; d = part[y1i][:, x1i]
    out = (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy
    if feather:
        yy_, xx_ = np.mgrid[0:out_size, 0:out_size].astype(np.float32) / out_size - 0.5
        r = np.sqrt(xx_ ** 2 + yy_ ** 2) / 0.7071
        dark = np.clip((r - 0.45) / 0.55, 0, 1) ** 1.3
        # grade towards the game's candlelit palette
        out = out * (1 - dark[..., None] * 0.85)
        out = out * col((1.05, 0.95, 0.82))
    save(name, out)
    bpy.data.images.remove(img)


def cover():
    img = bpy.data.images.load(os.path.join(CONCEPT, "Cover Art.png"))
    w, h = img.size
    px = np.array(img.pixels[:], np.float32).reshape(h, w, 4)[::-1]
    save("T_Cover", px[..., :3])
    bpy.data.images.remove(img)


mudbrick(); plaster(); stone(); ground(); wood(); rockwall(); thatch(); parchment()
closed_eye("T_AshSign")
altar_cloth(); flame_ring(); vignette()
# Portraits from the concept art (front views; boxes are in pixels of each 1536x1024 sheet, x0, y0, x1, y1)
crop("Young Priest.png", (380, 20, 590, 230), "T_Portrait_Alexios")
crop("Orthodox Nun.png", (400, 25, 600, 225), "T_Portrait_Anastasia")
crop("Orthodox Priest Ceremonial.png", (370, 10, 600, 240), "T_Portrait_Patriarch")
cover()
print("BUILD_TEXTURES DONE")
