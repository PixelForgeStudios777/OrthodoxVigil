# Renders the four icons of the screen, the scratched-prayer wall, and silhouette portraits for
# characters with no concept art. Flat Workbench renders, post-processed with numpy.
# Run: blender -b --factory-startup --python Art/render_icons.py
import bpy, os, math, random
import numpy as np
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "Textures")
FONT = r"C:\Windows\Fonts\pala.ttf"
pi = math.pi


def new_scene(w, h, bg=(0, 0, 0)):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.display.shading.light = 'FLAT'
    sc.display.shading.color_type = 'MATERIAL'
    sc.display.shading.background_type = 'VIEWPORT'
    sc.display.shading.background_color = bg
    sc.view_settings.view_transform = 'Standard'
    sc.render.resolution_x, sc.render.resolution_y = w, h
    sc.render.film_transparent = False
    cam = bpy.data.cameras.new("cam"); cam.type = 'ORTHO'
    co = bpy.data.objects.new("cam", cam); sc.collection.objects.link(co); sc.camera = co
    co.location = (0, 0, 10)
    cam.ortho_scale = max(w, h) / max(w, h) * (1.0 if w >= h else h / w) if False else 1.0
    return sc, co, cam


def mat(color):
    key = "m%.3f_%.3f_%.3f" % color
    m = bpy.data.materials.get(key)
    if not m:
        m = bpy.data.materials.new(key)
        m.diffuse_color = (*color, 1)
    return m


def poly(pts, z, color):
    me = bpy.data.meshes.new("p")
    me.from_pydata([(x, y, z) for x, y in pts], [], [list(range(len(pts)))])
    o = bpy.data.objects.new("p", me)
    bpy.context.collection.objects.link(o)
    o.data.materials.append(mat(color))
    return o


def disc(cx, cy, r, z, color, n=48, sx=1.0, sy=1.0):
    return poly([(cx + r * sx * math.cos(2 * pi * i / n), cy + r * sy * math.sin(2 * pi * i / n)) for i in range(n)], z, color)


def ring(cx, cy, r, t, z, color, n=64):
    me = bpy.data.meshes.new("r")
    vs = []
    for i in range(n):
        a = 2 * pi * i / n
        vs += [(cx + r * math.cos(a), cy + r * math.sin(a), z), (cx + (r - t) * math.cos(a), cy + (r - t) * math.sin(a), z)]
    fs = [(2 * i, 2 * ((i + 1) % n), 2 * ((i + 1) % n) + 1, 2 * i + 1) for i in range(n)]
    me.from_pydata(vs, [], fs)
    o = bpy.data.objects.new("r", me); bpy.context.collection.objects.link(o); o.data.materials.append(mat(color))
    return o


def text(s, x, y, size, z, color, align='CENTER', rot=0.0):
    cu = bpy.data.curves.new("t", 'FONT')
    cu.body = s
    cu.font = bpy.data.fonts.load(FONT)
    cu.size = size
    cu.align_x = align
    cu.align_y = 'CENTER'
    o = bpy.data.objects.new("t", cu)
    bpy.context.collection.objects.link(o)
    o.location = (x, y, z)
    o.rotation_euler = (0, 0, rot)
    o.data.materials.append(mat(color))
    return o


def render(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def age(path, strength=0.25, seed=1, vignette=0.4):
    img = bpy.data.images.load(path)
    w, h = img.size
    px = np.array(img.pixels[:], np.float32).reshape(h, w, 4)
    r = np.random.default_rng(seed)
    # smoky darkening, a little grain and craquelure
    g = r.random((h // 8 + 1, w // 8 + 1)).astype(np.float32)
    g = np.kron(g, np.ones((8, 8), np.float32))[:h, :w]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    v = np.sqrt(((xx / w) - 0.5) ** 2 + ((yy / h) - 0.5) ** 2) / 0.7071
    dark = 1.0 - strength * g - vignette * v ** 2
    crack = (np.sin(xx * 0.9 + np.sin(yy * 0.13) * 6) * np.sin(yy * 0.7 + np.sin(xx * 0.11) * 5)) > 0.985
    dark = dark - crack * 0.25
    px[..., :3] *= np.clip(dark, 0.3, 1.0)[..., None]
    img.pixels = px.ravel()
    img.save()
    bpy.data.images.remove(img)


GOLD = (0.72, 0.52, 0.18)
GOLD_L = (0.86, 0.68, 0.3)
RED = (0.48, 0.07, 0.05)
BLUE = (0.07, 0.13, 0.32)
SKIN = (0.62, 0.42, 0.26)
SKIN_D = (0.42, 0.26, 0.15)
DARK = (0.08, 0.05, 0.04)
GREEN = (0.15, 0.25, 0.14)
OCHRE = (0.5, 0.36, 0.17)
CREAM = (0.82, 0.76, 0.6)


def figure(robe, cloak, hair, beard=None, cloak_over=True, wings=None, maph=False):
    # halo
    disc(0, 0.22, 0.17, 0.0, GOLD_L)
    ring(0, 0.22, 0.17, 0.012, 0.01, RED)
    if wings:
        for s in (-1, 1):
            poly([(s * 0.08, 0.1), (s * 0.29, 0.28), (s * 0.3, 0.05), (s * 0.27, -0.2), (s * 0.12, -0.05)], 0.02, wings)
            for k in range(4):
                poly([(s * (0.12 + k * 0.04), 0.2 - k * 0.07), (s * (0.28), 0.22 - k * 0.11), (s * (0.27), 0.18 - k * 0.11)], 0.025, GOLD)
    # body (robe) trapezoid
    poly([(-0.1, 0.1), (0.1, 0.1), (0.17, -0.42), (-0.17, -0.42)], 0.03, robe)
    if cloak_over:
        poly([(-0.13, 0.08), (0.04, 0.1), (0.13, -0.42), (-0.19, -0.42)], 0.04, cloak)
    # head
    if maph:
        poly([(-0.1, 0.1), (-0.085, 0.27), (0, 0.33), (0.085, 0.27), (0.1, 0.1), (0.18, -0.15), (-0.18, -0.15)], 0.045, cloak)
        for (sx, sy) in ((-0.06, 0.26), (0.0, 0.31), (0.12, 0.02)):
            disc(sx, sy, 0.012, 0.06, GOLD_L, n=5)
    disc(0, 0.2, 0.06, 0.05, SKIN, sx=0.85)
    if not maph:
        poly([(-0.06, 0.24), (0, 0.27), (0.06, 0.24), (0.06, 0.14), (0.045, 0.21), (-0.045, 0.21), (-0.06, 0.14)], 0.055, hair)
    if beard:
        poly([(-0.045, 0.17), (0.045, 0.17), (0.03, 0.1), (0, 0.08), (-0.03, 0.1)], 0.055, beard)
    # eyes and brows
    for s in (-1, 1):
        disc(s * 0.02, 0.205, 0.006, 0.06, DARK)
        poly([(s * 0.008, 0.222), (s * 0.035, 0.226), (s * 0.035, 0.221), (s * 0.008, 0.218)], 0.06, DARK)
    poly([(-0.003, 0.2), (0.003, 0.2), (0.004, 0.175), (-0.004, 0.175)], 0.06, SKIN_D)
    # blessing hand
    disc(0.07, 0.0, 0.025, 0.07, SKIN)


def icon(name, build, label):
    sc, co, cam = new_scene(512, 760, bg=GOLD)
    cam.ortho_scale = 1.0 * 760 / 512 * 0.68
    poly([(-0.34, -0.5), (0.34, -0.5), (0.34, 0.5), (-0.34, 0.5)], -0.1, GOLD)
    # red border
    for (a, b, c, d) in (((-0.34, 0.48), (0.34, 0.48), (0.34, 0.5), (-0.34, 0.5)), ((-0.34, -0.5), (0.34, -0.5), (0.34, -0.48), (-0.34, -0.48)),
                         ((-0.34, -0.5), (-0.32, -0.5), (-0.32, 0.5), (-0.34, 0.5)), ((0.32, -0.5), (0.34, -0.5), (0.34, 0.5), (0.32, 0.5))):
        poly([a, b, c, d], 0.08, RED)
    build()
    for (t, x, y) in label:
        text(t, x, y, 0.06, 0.09, RED)
    path = os.path.join(OUT, name + ".png")
    render(path)
    age(path, seed=len(name))
    print("ICON", name)


icon("T_Icon_Christ", lambda: (figure(RED, BLUE, DARK, beard=DARK), poly([(-0.12, -0.02), (-0.02, -0.02), (-0.02, -0.16), (-0.12, -0.16)], 0.08, CREAM),
                               text("+", -0.07, -0.09, 0.07, 0.09, RED)), [("IC", -0.21, 0.24), ("XC", 0.21, 0.24)])
icon("T_Icon_Theotokos", lambda: (figure(BLUE, RED, DARK, maph=True), disc(-0.03, 0.0, 0.07, 0.08, GOLD_L), disc(-0.03, 0.02, 0.035, 0.09, SKIN),
                                  poly([(-0.08, -0.03), (0.02, -0.03), (0.03, -0.12), (-0.09, -0.12)], 0.09, OCHRE)), [("MP", -0.21, 0.3), ("ΘY", 0.21, 0.3)])
icon("T_Icon_Michael", lambda: (figure(RED, (0.12, 0.2, 0.35), (0.35, 0.22, 0.08), wings=(0.55, 0.38, 0.14)),
                                poly([(0.11, 0.3), (0.125, 0.3), (0.125, -0.4), (0.11, -0.4)], 0.08, DARK), disc(-0.07, -0.02, 0.05, 0.08, (0.5, 0.6, 0.65))),
     [("O APX", -0.22, 0.32), ("MIXAHΛ", 0.21, 0.32)])
icon("T_Icon_Forerunner", lambda: (figure(OCHRE, GREEN, DARK, beard=DARK), poly([(0.05, 0.02), (0.15, 0.02), (0.16, -0.25), (0.06, -0.25)], 0.08, CREAM)),
     [("O ΠPO", -0.21, 0.32), ("ΔPOMOC", 0.21, 0.32)])

# ---------------------------------------------------------------- scratched wall: Κύριε ἐλέησον, hundreds of times
sc, co, cam = new_scene(1024, 768, bg=(0, 0, 0))
cam.ortho_scale = 1.0
r = random.Random(7)
lines = 22
for i in range(lines):
    y = 0.36 - i * 0.034
    x = -0.5
    depth = i / lines
    while x < 0.5:
        s = r.uniform(0.022, 0.03) * (1.0 + depth * 0.5)
        t = text("ΚΥΡΙΕ ΕΛΕΗΣΟΝ", x, y + r.uniform(-0.004, 0.004) * (1 + depth * 3), s, 0, (0.75 + depth * 0.25,) * 3, align='LEFT', rot=r.uniform(-0.03, 0.03) * (1 + depth * 2))
        x += s * 8.8 + r.uniform(0.0, 0.03)
render(os.path.join(OUT, "T_Scratch.png"))
print("ICON T_Scratch")


# ---------------------------------------------------------------- silhouette portraits for characters without concept art
def silhouette(name, char_fbx, rim=(1.0, 0.65, 0.3), bg=(0.08, 0.05, 0.03), hide_body=False, body_col=(0.05, 0.04, 0.03)):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.scale_length = 0.01
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.display.shading.light = 'STUDIO'
    sc.display.shading.color_type = 'MATERIAL'
    sc.display.shading.background_type = 'VIEWPORT'
    sc.display.shading.background_color = bg
    sc.display.shading.show_cavity = True
    sc.render.resolution_x = sc.render.resolution_y = 512
    bpy.ops.import_scene.fbx(filepath=os.path.join(HERE, "mannequin", "SKM_Manny_Simple.fbx"))
    for o in bpy.data.objects:
        if o.type == 'MESH':
            o.data.materials.clear(); o.data.materials.append(mat(body_col))
            o.hide_render = hide_body
    bpy.ops.import_scene.fbx(filepath=os.path.join(HERE, "FBX", "Chars", char_fbx + ".fbx"))
    for m in bpy.data.materials:
        if not m.name.startswith("m0") and not m.name.startswith("m1"):
            m.diffuse_color = (0.06, 0.05, 0.045, 1) if "Bright" not in m.name else (0.9, 0.88, 0.8, 1)
            if m.name.startswith(("Void", "Dark")):
                m.diffuse_color = (0, 0, 0, 1)
    cam = bpy.data.cameras.new("c"); cam.type = 'ORTHO'; cam.ortho_scale = 58
    co = bpy.data.objects.new("c", cam); sc.collection.objects.link(co); sc.camera = co
    co.location = (0, -300, 160); co.rotation_euler = (pi / 2, 0, 0)
    path = os.path.join(OUT, name + ".png")
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    # warm lamp grade + vignette
    img = bpy.data.images.load(path)
    w, h = img.size
    px = np.array(img.pixels[:], np.float32).reshape(h, w, 4)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    v = np.sqrt(((xx / w) - 0.35) ** 2 + ((yy / h) - 0.45) ** 2)
    glow = np.clip(1 - v * 1.6, 0, 1) ** 2
    px[..., 0] = px[..., 0] * 0.9 + glow * rim[0] * 0.18
    px[..., 1] = px[..., 1] * 0.8 + glow * rim[1] * 0.18
    px[..., 2] = px[..., 2] * 0.7 + glow * rim[2] * 0.18
    img.pixels = px.ravel(); img.save(); bpy.data.images.remove(img)
    print("ICON", name)


silhouette("T_Portrait_Hooded", "SK_Hooded")
silhouette("T_Portrait_Thecla", "SK_Thecla", body_col=(0.3, 0.2, 0.13))
silhouette("T_Portrait_Besa", "SK_Besa", body_col=(0.28, 0.18, 0.11))
silhouette("T_Portrait_Hor", "SK_Besa", body_col=(0.2, 0.14, 0.09), rim=(0.6, 0.7, 1.0), bg=(0.02, 0.03, 0.06))
silhouette("T_Portrait_Bright", "SK_BrightOne", hide_body=True, rim=(0.9, 0.9, 1.0), bg=(0.0, 0.0, 0.0))
silhouette("T_Portrait_Kosmas", "SK_Alexios", body_col=(0.3, 0.2, 0.13), rim=(1.0, 0.85, 0.5), bg=(0.12, 0.09, 0.05))
print("RENDER_ICONS DONE")
