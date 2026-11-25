# Builds every static mesh for the Orthodox Vigil slice (Act I, Tarsenis) and exports FBX.
# Run: blender -b --factory-startup --python Art/build_env.py
# Convention: a building's front (door) faces +X. Origin = centre of the base.
# Unreal reads Blender (x, y, z) as (x, -y, z), which keeps shapes un-mirrored.
import bpy, bmesh, math, os, sys, random
from mathutils import Vector, Matrix
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blib as B

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "FBX", "Env")
B.reset()
R = random.Random(14)
ONLY = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def want(name):
    return not ONLY or name in ONLY


pi = math.pi

# ------------------------------------------------------------------ terrain
def ground_h(x, y):
    """Height in metres at UE coords (x, y). Mirrored by VigilWorld::GroundZ in C++ only loosely; C++ traces."""
    h = 0.0
    # low rise the village sits on
    d = math.hypot((x - 2) / 30.0, y / 26.0)
    h += 0.9 * math.exp(-d * d * 1.6)
    # canal on the east: bed at -1.6 m between x=30 and 40, banks slope
    if x > 24:
        t = min(1.0, (x - 24) / 6.0)
        h -= 1.9 * t * t * (3 - 2 * t)
    if x > 40:
        t = min(1.0, (x - 40) / 5.0)
        h += 2.0 * t * t * (3 - 2 * t)
    # ridge of tombs far west
    if x < -40:
        t = min(1.0, (-40 - x) / 14.0)
        h += 7.0 * t * t * (3 - 2 * t)
    # gentle noise
    h += 0.08 * math.sin(x * 0.37) * math.cos(y * 0.29) + 0.05 * math.sin(x * 1.1 + y * 0.7)
    return h


if want("SM_Ground"):
    N = 140
    S = 150.0
    verts, faces = [], []
    for j in range(N + 1):
        for i in range(N + 1):
            x = -S / 2 + S * i / N
            y = -S / 2 + S * j / N
            verts.append((x, -y, ground_h(x, y)))
    for j in range(N):
        for i in range(N):
            a = j * (N + 1) + i
            faces.append((a, a + 1, a + N + 2, a + N + 1))
    g = B.from_pydata("SM_Ground", verts, faces, "Ground")
    # faces wound so normals face up in Blender (y flipped)
    if g.data.polygons[0].normal.z < 0:
        for p in g.data.polygons:
            p.flip()
    B.uv_box(g, 0.25)
    B.grime(g, dark_base=1.0, height=0, noise=0.25, seed=3)
    B.smooth(g, 1.2)
    B.export(g, os.path.join(OUT, "SM_Ground.fbx"))
    print("EXPORT SM_Ground verts=%d" % len(verts))
    bpy.data.objects.remove(g)

if want("SM_Water"):
    w = B.plane(12, 150, mat="Water")
    B.finish(w, OUT, "SM_Water", vc=False)

# ------------------------------------------------------------------ mud-brick houses
def house(name, L, W, H, door_w=0.95, door_h=1.95, lean_to=False, upper=False, seed=0):
    parts = []
    t = 0.38
    # walls as a hollow-free block with a recessed doorway on +X
    body = B.box((L, W, H), mat="MudBrick")
    B.subdivide_edges(body, 3)
    B.jitter(body, 0.025, seed)
    # taper slightly: mud-brick walls lean in
    B.taper(body, H, 0.965)
    parts.append(body)
    # door recess frame: dark void + wooden lintel
    parts.append(B.box((0.05, door_w + 0.1, door_h + 0.05), loc=(L / 2 + 0.005, 0, 0), mat="Dark"))
    parts.append(B.box((0.22, door_w + 0.6, 0.16), loc=(L / 2 + 0.06, 0, door_h + 0.05), mat="DarkWood"))
    # threshold stone
    parts.append(B.box((0.45, door_w + 0.3, 0.08), loc=(L / 2 + 0.2, 0, 0), mat="Stone"))
    # parapet + roof slab
    parts.append(B.box((L + 0.1, W + 0.1, 0.18), loc=(0, 0, H), mat="MudBrick"))
    for sx in (-1, 1):
        parts.append(B.box((L + 0.1, 0.25, 0.35), loc=(0, sx * (W / 2 - 0.08), H + 0.18), mat="MudBrick"))
    parts.append(B.box((0.25, W + 0.1, 0.35), loc=(L / 2 - 0.08, 0, H + 0.18), mat="MudBrick"))
    parts.append(B.box((0.25, W + 0.1, 0.35), loc=(-L / 2 + 0.08, 0, H + 0.18), mat="MudBrick"))
    # palm-log roof beams poking out of the walls
    n = int(W / 0.55)
    for i in range(n):
        y = -W / 2 + 0.35 + i * (W - 0.7) / max(1, n - 1)
        b = B.cyl(0.07, L + 0.6, loc=(-L / 2 - 0.3, y, H - 0.12), rot=(0, pi / 2, 0), verts=7, mat="PalmTrunk")
        parts.append(b)
    # small high window recess on the side
    parts.append(B.box((0.5, 0.05, 0.35), loc=(L * 0.15, -W / 2 - 0.01, H * 0.62), mat="Dark"))
    # palm-frond bundle on the roof
    for k in range(3):
        f = B.box((R.uniform(0.8, 1.6), R.uniform(0.4, 0.8), 0.18), loc=(R.uniform(-L / 3, L / 3), R.uniform(-W / 4, W / 4), H + 0.18),
                  rot=(0, 0, R.uniform(0, pi)), mat="Thatch")
        B.jitter(f, 0.05, seed + k)
        parts.append(f)
    if upper:
        u = B.box((L * 0.45, W * 0.55, 1.6), loc=(-L * 0.22, 0, H + 0.18), mat="MudBrick")
        B.subdivide_edges(u, 2)
        B.jitter(u, 0.02, seed + 9)
        parts.append(u)
        parts.append(B.box((0.05, 0.45, 0.6), loc=(-L * 0.22 + L * 0.225 + 0.01, 0, H + 0.8), mat="Dark"))
        parts.append(B.box((L * 0.45 + 0.1, W * 0.55 + 0.1, 0.12), loc=(-L * 0.22, 0, H + 1.78), mat="MudBrick"))
    if lean_to:
        for sy in (-1, 1):
            parts.append(B.cyl(0.06, 2.1, loc=(L / 2 + 1.6, sy * (W / 2 - 0.3), 0), verts=6, mat="PalmTrunk"))
        lt = B.box((1.9, W + 0.2, 0.12), loc=(L / 2 + 0.85, 0, 2.05), rot=(0, -0.12, 0), mat="Thatch")
        B.subdivide_edges(lt, 2)
        B.jitter(lt, 0.05, seed + 4)
        parts.append(lt)
    o = B.join(parts, name)
    B.finish(o, OUT, name, uv_scale=0.5, dark_base=0.5, height=0.9, seed=seed)


if want("SM_HouseA"): house("SM_HouseA", 5.0, 4.2, 2.8, seed=1)
if want("SM_HouseB"): house("SM_HouseB", 4.4, 4.4, 3.0, upper=True, seed=2)
if want("SM_HouseC"): house("SM_HouseC", 6.0, 3.8, 2.6, lean_to=True, seed=3)

if want("SM_Door"):
    # plank door, origin on the hinge edge (local y=0), door spans +Y, faces +X
    parts = []
    for i in range(5):
        p = B.box((0.06, 0.18, 1.92), loc=(0, 0.1 + i * 0.19, 0), mat="DarkWood")
        B.jitter(p, 0.004, i)
        parts.append(p)
    for z in (0.3, 1.5):
        parts.append(B.box((0.04, 0.92, 0.12), loc=(0.05, 0.47, z), mat="Wood"))
    o = B.join(parts, "SM_Door")
    B.finish(o, OUT, center_xy=False, dark_base=0.7, height=1.0)

if want("SM_AshSign"):
    # 0.6 m square decal, UV 0..1, faces +X
    o = B.from_pydata("SM_AshSign", [(0, -0.3, 0), (0, 0.3, 0), (0, 0.3, 0.6), (0, -0.3, 0.6)], [(0, 1, 2, 3)], "AshSign")
    me = o.data
    uv = me.uv_layers.new(name="UV")
    for li, (u, v) in zip(range(4), [(0, 1), (1, 1), (1, 0), (0, 0)]):
        uv.data[li].uv = (u, v)
    if me.polygons[0].normal.x < 0:
        me.polygons[0].flip()
    B.finish(o, OUT, vc=False)

if want("SM_Bread"):
    s = B.sphere(0.11, scale=(1.0, 0.8, 0.55), seg=14, rings=8, mat="Bread")
    # cut in half: flatten vertices with x>0 onto x=0
    for v in s.data.vertices:
        if v.co.x > 0:
            v.co.x = 0.0
    B.jitter(s, 0.006, 4)
    B.finish(s, OUT, "SM_Bread", dark_base=0.8, height=0.05)

# ------------------------------------------------------------------ well
if want("SM_Well"):
    parts = []
    # stone lip: ring of blocks
    n = 14
    for i in range(n):
        a = 2 * pi * i / n
        blk = B.box((0.34, 0.42, 0.72), loc=(0.68 * math.cos(a), 0.68 * math.sin(a), 0), rot=(0, 0, a), mat="Stone")
        B.bevel(blk, 0.03, 2)
        B.jitter(blk, 0.012, i)
        parts.append(blk)
    parts.append(B.cyl(0.92, 0.12, loc=(0, 0, 0.72), verts=24, mat="Stone", cap=False))
    # inner shaft (open topped tube going down)
    shaft = B.cyl(0.55, 5.0, loc=(0, 0, -4.8), verts=20, mat="Stone", cap=False)
    for p in shaft.data.polygons:
        p.flip()
    parts.append(shaft)
    # palm-trunk frame + crossbeam + pulley
    for sx in (-1, 1):
        post = B.cyl(0.1, 2.5, loc=(0, sx * 1.05, 0), verts=8, mat="PalmTrunk")
        B.jitter(post, 0.01, sx + 5)
        parts.append(post)
    parts.append(B.cyl(0.08, 2.4, loc=(0, -1.2, 2.35), rot=(-pi / 2, 0, 0), verts=8, mat="PalmTrunk"))
    parts.append(B.torus(0.16, 0.035, loc=(0, 0, 2.2), rot=(pi / 2, 0, 0), mat="Wood", maj=16, mino=6))
    # trough
    parts.append(B.box((0.5, 1.2, 0.35), loc=(1.25, 0.2, 0), mat="Stone"))
    o = B.join(parts, "SM_Well")
    B.finish(o, OUT, keep_origin=True, dark_base=0.6, height=0.8)

if want("SM_WellWater"):
    w = B.cyl(0.55, 0.01, verts=20, mat="Water")
    B.finish(w, OUT, "SM_WellWater", vc=False)

if want("SM_Bucket"):
    b = B.cyl(0.14, 0.26, r2=0.17, verts=12, mat="Wood", cap=True)
    h = B.torus(0.15, 0.012, loc=(0, 0, 0.3), rot=(pi / 2, 0, 0), mat="Iron", maj=12, mino=4)
    o = B.join([b, h], "SM_Bucket")
    B.finish(o, OUT, dark_base=0.7, height=0.2)

if want("SM_Rope"):
    r_ = B.cyl(0.012, 1.0, verts=6, mat="Rope")
    B.finish(r_, OUT, "SM_Rope", vc=False)

# ------------------------------------------------------------------ dovecote, palms, reeds, goat, laundry
if want("SM_Dovecote"):
    parts = []
    t = B.cyl(1.7, 5.2, verts=20, r2=0.9, mat="Plaster")
    B.subdivide_edges(t, 2)
    B.jitter(t, 0.03, 7)
    parts.append(t)
    parts.append(B.sphere(0.95, loc=(0, 0, 5.2), scale=(1, 1, 0.7), mat="Plaster"))
    # rows of clay pot mouths
    for row in range(6):
        z = 1.4 + row * 0.6
        rad = 1.7 + (0.9 - 1.7) * (z / 5.2)
        for i in range(10):
            a = 2 * pi * (i + row * 0.5) / 10
            parts.append(B.cyl(0.07, 0.12, loc=(math.cos(a) * (rad - 0.02), math.sin(a) * (rad - 0.02), z), rot=(0, pi / 2, a), verts=8, mat="Dark"))
    for i in range(6):
        a = 2 * pi * i / 6
        parts.append(B.cyl(0.03, 0.6, loc=(math.cos(a) * 1.0, math.sin(a) * 1.0, 3.0), rot=(0, pi / 2, a), verts=5, mat="PalmTrunk"))
    o = B.join(parts, "SM_Dovecote")
    B.finish(o, OUT, dark_base=0.55, height=1.5)


def palm(name, h, lean, seed):
    rr = random.Random(seed)
    parts = []
    segs = 12
    rings = []
    for i in range(segs + 1):
        t = i / segs
        z = h * t
        x = lean * (t ** 1.6)
        r = 0.2 * (1 - 0.35 * t)
        ring = []
        for k in range(8):
            a = 2 * pi * k / 8
            bump = 1.0 + (0.08 if i % 2 == 0 else 0.0)
            ring.append(Vector((x + r * bump * math.cos(a), r * bump * math.sin(a), z)))
        rings.append(ring)
    parts.append(B.ring_loft(rings, name + "_t", "PalmTrunk", cap_top=True))
    top = Vector((lean, 0, h))
    for f in range(11):
        a = 2 * pi * f / 11 + rr.uniform(-0.2, 0.2)
        length = rr.uniform(2.2, 3.0)
        droop = rr.uniform(0.8, 1.4)
        verts, faces = [], []
        steps = 8
        for s in range(steps + 1):
            u = s / steps
            d = length * u
            zc = top.z + 0.5 * u - droop * u * u
            cx = top.x + math.cos(a) * d
            cy = top.y + math.sin(a) * d
            wdt = 0.42 * math.sin(pi * min(1.0, u * 1.15 + 0.05))
            nx, ny = -math.sin(a), math.cos(a)
            verts += [(cx - nx * wdt, cy - ny * wdt, zc - 0.05 * u), (cx, cy, zc + 0.04), (cx + nx * wdt, cy + ny * wdt, zc - 0.05 * u)]
        for s in range(steps):
            b = s * 3
            faces += [(b, b + 3, b + 4, b + 1), (b + 1, b + 4, b + 5, b + 2)]
        parts.append(B.from_pydata(name + "_f%d" % f, verts, faces, "Palm"))
    o = B.join(parts, name)
    B.finish(o, OUT, name, center_xy=False, dark_base=0.6, height=h, seed=seed)


if want("SM_PalmA"): palm("SM_PalmA", 7.5, 0.9, 1)
if want("SM_PalmB"): palm("SM_PalmB", 9.0, -1.3, 2)
if want("SM_PalmC"): palm("SM_PalmC", 5.5, 0.4, 3)

if want("SM_Reeds"):
    rr = random.Random(5)
    verts, faces = [], []
    for i in range(46):
        x, y = rr.uniform(-0.7, 0.7), rr.uniform(-0.7, 0.7)
        hgt = rr.uniform(1.3, 2.4)
        lx, ly = rr.uniform(-0.35, 0.35), rr.uniform(-0.35, 0.35)
        a = rr.uniform(0, pi)
        w = 0.03
        dx, dy = math.cos(a) * w, math.sin(a) * w
        b = len(verts)
        verts += [(x - dx, y - dy, 0), (x + dx, y + dy, 0), (x + lx + dx * 0.2, y + ly + dy * 0.2, hgt), (x + lx - dx * 0.2, y + ly - dy * 0.2, hgt)]
        faces.append((b, b + 1, b + 2, b + 3))
        # seed head
        verts += [(x + lx - 0.02, y + ly, hgt - 0.25), (x + lx + 0.02, y + ly, hgt - 0.25), (x + lx + 0.02, y + ly, hgt + 0.05), (x + lx - 0.02, y + ly, hgt + 0.05)]
        faces.append((b + 4, b + 5, b + 6, b + 7))
    o = B.from_pydata("SM_Reeds", verts, faces, "Reed")
    B.finish(o, OUT, center_xy=False, dark_base=0.5, height=1.5)

if want("SM_Goat"):
    parts = [B.sphere(0.32, loc=(0, 0, 0.62), scale=(1.5, 0.75, 0.8), mat="Goat"),
             B.sphere(0.13, loc=(0.55, 0, 0.92), scale=(1.4, 0.8, 0.9), mat="Goat"),
             B.cyl(0.07, 0.3, loc=(0.42, 0, 0.7), rot=(0, 0.9, 0), verts=8, mat="Goat")]
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(B.cyl(0.035, 0.48, loc=(sx * 0.3, sy * 0.13, 0), verts=6, mat="Goat"))
        parts.append(B.cyl(0.02, 0.22, loc=(0.6, sx * 0.06, 1.02), rot=(sx * 0.3, -0.8, 0), verts=5, r2=0.004, mat="Bone"))
    parts.append(B.sphere(0.012, loc=(0.7, 0.07, 0.96), mat="Dark"))
    parts.append(B.sphere(0.012, loc=(0.7, -0.07, 0.96), mat="Dark"))
    o = B.join(parts, "SM_Goat")
    B.finish(o, OUT, dark_base=0.8, height=0.5)

if want("SM_Laundry"):
    parts = []
    for sx in (-1, 1):
        parts.append(B.cyl(0.05, 2.1, loc=(0, sx * 2.5, 0), verts=6, mat="PalmTrunk"))
    parts.append(B.cyl(0.008, 5.0, loc=(0, -2.5, 1.95), rot=(-pi / 2, 0, 0), verts=4, mat="Rope"))
    rr = random.Random(9)
    for i, (y, w, h, m) in enumerate([(-1.6, 0.7, 0.9, "Cloth"), (-0.6, 0.9, 1.1, "ClothBlue"), (0.5, 0.6, 0.7, "Cloth"), (1.4, 0.8, 1.0, "ClothRed")]):
        c = B.box((0.02, w, h), loc=(0, y, 1.95 - h), mat=m)
        B.subdivide_edges(c, 3)
        for v in c.data.vertices:
            v.co.x += 0.04 * math.sin(v.co.y * 6 + i) * (1.95 - v.co.z)
        parts.append(c)
    o = B.join(parts, "SM_Laundry")
    B.finish(o, OUT, center_xy=False, vc=False)

if want("SM_LowWall"):
    w = B.box((4.0, 0.45, 1.15), mat="MudBrick")
    B.subdivide_edges(w, 3)
    B.jitter(w, 0.03, 11)
    cap = B.box((4.05, 0.5, 0.08), loc=(0, 0, 1.15), mat="MudBrick")
    B.jitter(cap, 0.02, 12)
    o = B.join([w, cap], "SM_LowWall")
    B.finish(o, OUT, dark_base=0.5, height=0.8)

# ------------------------------------------------------------------ canal: landing, boats, shed
if want("SM_LandingStone"):
    parts = []
    s = B.box((3.2, 2.4, 1.4), mat="Stone")
    B.subdivide_edges(s, 2)
    B.bevel(s, 0.08, 3)
    B.jitter(s, 0.03, 2)
    parts.append(s)
    for i in range(3):
        st = B.box((0.5, 1.6, 0.35 + i * 0.32), loc=(-1.6 - 0.5 * (2 - i) - 0.25, 0, 0), mat="Stone")
        B.bevel(st, 0.04, 2)
        parts.append(st)
    parts.append(B.torus(0.12, 0.025, loc=(1.62, 0.6, 1.05), rot=(0, pi / 2, 0), mat="Rust"))
    parts.append(B.cyl(0.05, 0.12, loc=(1.55, 0.6, 1.05), rot=(0, pi / 2, 0), mat="Rust"))
    o = B.join(parts, "SM_LandingStone")
    B.finish(o, OUT, dark_base=0.45, height=1.3)


def hull(name, L, W, D, seed):
    rings = []
    steps = 12
    for i in range(steps + 1):
        u = i / steps
        x = -L / 2 + L * u
        k = math.sin(pi * u) ** 0.7
        w = W / 2 * max(0.05, k)
        dep = D * max(0.2, k)
        rise = 0.25 * (abs(u - 0.5) * 2) ** 3
        ring = []
        for j in range(9):
            a = pi * j / 8
            ring.append(Vector((x, -w * math.cos(a), D + rise - dep * math.sin(a))))
        rings.append(ring)
    h = B.ring_loft(rings, name, "DarkWood")
    for p in h.data.polygons:
        p.flip()
    return h


if want("SM_Felucca"):
    parts = [hull("hull", 6.5, 1.9, 0.75, 1)]
    parts.append(B.box((5.0, 1.6, 0.04), loc=(0, 0, 0.45), mat="Wood"))
    parts.append(B.cyl(0.07, 6.0, loc=(1.2, 0, 0.4), verts=8, mat="PalmTrunk"))
    # lateen yard + sail
    parts.append(B.cyl(0.05, 8.0, loc=(4.2, 0, 1.2), rot=(0, -1.12, 0), verts=6, mat="PalmTrunk"))
    sail = B.from_pydata("sail", [(4.0, 0.05, 1.3), (-2.9, 0.05, 6.3), (-0.8, 0.05, 1.1)], [(0, 1, 2)], "Sail")
    B.subdivide_edges(sail, 4)
    for v in sail.data.vertices:
        v.co.y += 0.35 * math.sin(pi * max(0, min(1, (v.co.z - 1.1) / 5.2)))
    parts.append(sail)
    o = B.join(parts, "SM_Felucca")
    B.finish(o, OUT, dark_base=0.7, height=0.6)

if want("SM_Rowboat"):
    parts = [hull("hull", 4.2, 1.3, 0.55, 2), B.box((0.3, 1.1, 0.04), loc=(0.3, 0, 0.35), mat="Wood")]
    o = B.join(parts, "SM_Rowboat")
    B.finish(o, OUT, dark_base=0.7, height=0.4)

if want("SM_BoatShed"):
    # reed-and-timber shed, 6 x 4.5, open front on +X; roof exported separately
    parts = []
    L, W, H = 6.0, 4.6, 2.5
    for x in (-L / 2, 0, L / 2):
        for y in (-W / 2, W / 2):
            p = B.cyl(0.09, H + 0.2, loc=(x, y, 0), verts=7, mat="PalmTrunk")
            B.jitter(p, 0.012, int(x * 3 + y))
            parts.append(p)
    for y in (-W / 2, W / 2):
        parts.append(B.cyl(0.07, L + 0.3, loc=(-L / 2 - 0.15, y, H), rot=(0, pi / 2, 0), verts=6, mat="PalmTrunk"))
    # reed mat walls on back and sides
    def mat_wall(loc, size):
        m = B.box(size, loc=loc, mat="Reed")
        B.subdivide_edges(m, 3)
        B.jitter(m, 0.03, int(loc[0] * 7 + loc[1] * 3))
        return m
    parts.append(mat_wall((-L / 2, 0, 0.0), (0.12, W, H - 0.1)))
    parts.append(mat_wall((0, -W / 2, 0.0), (L, 0.12, H - 0.1)))
    parts.append(mat_wall((-L / 4, W / 2, 0.0), (L / 2, 0.12, H - 0.1)))
    # nets hung on the back wall, floats, a low bench and a dim lamp shelf
    net = B.box((0.06, 2.6, 1.6), loc=(-L / 2 + 0.12, 0.3, 0.5), mat="Net")
    B.subdivide_edges(net, 4)
    for v in net.data.vertices:
        v.co.x += 0.08 * math.sin(v.co.z * 5) + 0.05 * math.sin(v.co.y * 7)
    parts.append(net)
    for i in range(7):
        parts.append(B.sphere(0.09, loc=(-L / 2 + 0.2, -0.9 + i * 0.32, 2.0 - 0.1 * (i % 2)), mat="Clay"))
    parts.append(B.box((1.8, 0.5, 0.45), loc=(-0.5, -W / 2 + 0.45, 0), mat="Wood"))
    parts.append(B.box((0.6, 0.3, 0.05), loc=(-L / 2 + 0.3, -1.6, 1.4), mat="Wood"))
    o = B.join(parts, "SM_BoatShed")
    B.finish(o, OUT, center_xy=False, dark_base=0.5, height=1.2)
    roof = B.box((L + 0.6, W + 0.6, 0.3), loc=(0, 0, H + 0.05), rot=(0, 0.08, 0), mat="Thatch")
    B.subdivide_edges(roof, 4)
    B.jitter(roof, 0.06, 21)
    roof.name = "SM_BoatShedRoof"
    B.uv_box(roof, 0.5)
    B.grime(roof, dark_base=0.8, height=3.0)
    B.smooth(roof)
    B.export(roof, os.path.join(OUT, "SM_BoatShedRoof.fbx"))
    print("EXPORT SM_BoatShedRoof (origin kept at shed base)")
    bpy.data.objects.remove(roof)

if want("SM_Nets"):
    n = B.box((1.4, 1.0, 0.25), mat="Net")
    B.subdivide_edges(n, 4)
    B.jitter(n, 0.07, 3)
    B.finish(n, OUT, "SM_Nets", dark_base=0.6, height=0.2)

# ------------------------------------------------------------------ church of the Archangel Michael
# Nave runs along X, 11 m x 7 m outside, walls 0.6 thick, 5 m high. Door on the east (+X) wall.
CL, CW, CH, CT = 11.0, 7.0, 5.0, 0.6


def wall_with_holes(length, height, thick, holes, axis, offset, name, mat="Plaster"):
    """A wall along `axis` ('x' or 'y') built from boxes around rectangular holes.
    holes: list of (center_along, bottom, width, top). Wall centred on 0 along its length, placed at `offset` across."""
    parts = []
    cuts = sorted(holes)
    # split into vertical strips
    edges = [-length / 2]
    for c, b, w, t in cuts:
        edges += [c - w / 2, c + w / 2]
    edges.append(length / 2)
    def put(a0, a1, z0, z1):
        if a1 - a0 < 1e-3 or z1 - z0 < 1e-3:
            return
        mid = (a0 + a1) / 2
        if axis == 'x':
            parts.append(B.box((a1 - a0, thick, z1 - z0), loc=(mid, offset, z0), mat=mat))
        else:
            parts.append(B.box((thick, a1 - a0, z1 - z0), loc=(offset, mid, z0), mat=mat))
    # solid strips between holes
    for i in range(0, len(edges), 2):
        put(edges[i], edges[i + 1], 0, height)
    # above / below each hole
    for c, b, w, t in cuts:
        put(c - w / 2, c + w / 2, 0, b)
        put(c - w / 2, c + w / 2, t, height)
    o = B.join(parts, name)
    B.subdivide_edges(o, 1)
    B.jitter(o, 0.012, len(name))
    return o


def church():
    win = [(-2.6, 3.0, 0.6, 4.0), (2.2, 3.0, 0.6, 4.0)]
    # long walls along X at y = +-(CW/2 - CT/2). Blender +y is UE -y (north in our map), so N wall is +y here.
    if want("SM_ChurchWallN"):
        w = wall_with_holes(CL, CH, CT, win, 'x', CW / 2 - CT / 2, "SM_ChurchWallN")
        B.finish(w, OUT, center_xy=False, dark_base=0.62, height=1.5)
    if want("SM_ChurchWallS"):
        w = wall_with_holes(CL, CH, CT, win, 'x', -(CW / 2 - CT / 2), "SM_ChurchWallS")
        B.finish(w, OUT, center_xy=False, dark_base=0.62, height=1.5)
    if want("SM_ChurchWallE"):
        w = wall_with_holes(CW - 2 * CT, CH, CT, [(0, 0, 1.5, 2.7)], 'y', CL / 2 - CT / 2, "SM_ChurchWallE")
        parts = [w]
        # gable triangle + arch moulding + cornice
        tri = B.from_pydata("gE", [(CL / 2, -CW / 2, CH), (CL / 2, CW / 2, CH), (CL / 2, 0, CH + 1.9),
                                   (CL / 2 - CT, -CW / 2, CH), (CL / 2 - CT, CW / 2, CH), (CL / 2 - CT, 0, CH + 1.9)],
                            [(0, 1, 2), (5, 4, 3), (0, 3, 4, 1), (1, 4, 5, 2), (2, 5, 3, 0)], "Plaster")
        parts.append(tri)
        for i in range(9):
            a = pi * i / 8
            parts.append(B.box((0.12, 0.22, 0.22), loc=(CL / 2 + 0.04, 0.85 * math.cos(a), 2.7 + 0.45 * math.sin(a)), rot=(a - pi / 2, 0, 0), mat="Stone"))
        parts.append(B.box((0.14, 0.22, 2.7), loc=(CL / 2 + 0.04, -0.86, 0), mat="Stone"))
        parts.append(B.box((0.14, 0.22, 2.7), loc=(CL / 2 + 0.04, 0.86, 0), mat="Stone"))
        parts.append(B.box((0.6, 2.4, 0.15), loc=(CL / 2 + 0.3, 0, 0), mat="Stone"))
        o = B.join(parts, "SM_ChurchWallE")
        B.finish(o, OUT, center_xy=False, dark_base=0.62, height=1.5)
    if want("SM_ChurchWallW"):
        w = wall_with_holes(CW - 2 * CT, CH, CT, [(0, 0, 3.0, 3.6)], 'y', -(CL / 2 - CT / 2), "SM_ChurchWallW")
        tri = B.from_pydata("gW", [(-CL / 2, CW / 2, CH), (-CL / 2, -CW / 2, CH), (-CL / 2, 0, CH + 1.9),
                                   (-CL / 2 + CT, CW / 2, CH), (-CL / 2 + CT, -CW / 2, CH), (-CL / 2 + CT, 0, CH + 1.9)],
                            [(0, 1, 2), (5, 4, 3), (0, 3, 4, 1), (1, 4, 5, 2), (2, 5, 3, 0)], "Plaster")
        # apse: half cylinder + quarter sphere behind the west wall
        ap = B.cyl(2.0, 3.8, loc=(-CL / 2, 0, 0), verts=24, mat="Plaster", cap=False)
        bm = bmesh.new(); bm.from_mesh(ap.data)
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.x > -CL / 2 + 0.01], context='VERTS')
        bm.to_mesh(ap.data); bm.free()
        dome = B.sphere(2.0, loc=(-CL / 2, 0, 3.8), seg=24, rings=12, mat="Plaster")
        bm = bmesh.new(); bm.from_mesh(dome.data)
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.x > -CL / 2 + 0.01 or v.co.z < 3.79], context='VERTS')
        bm.to_mesh(dome.data); bm.free()
        # inner shell of the apse so the inside is visible from the nave
        ap2 = ap.copy(); ap2.data = ap.data.copy(); bpy.context.collection.objects.link(ap2)
        for p in ap2.data.polygons: p.flip()
        d2 = dome.copy(); d2.data = dome.data.copy(); bpy.context.collection.objects.link(d2)
        for p in d2.data.polygons: p.flip()
        o = B.join([w, tri, ap, dome, ap2, d2], "SM_ChurchWallW")
        B.finish(o, OUT, center_xy=False, dark_base=0.62, height=1.5)
    if want("SM_ChurchFloor"):
        parts = []
        for i in range(11):
            for j in range(6):
                t = B.box((0.96, 0.96, 0.08), loc=(-CL / 2 + CT + 0.5 + i * 0.98, -CW / 2 + CT + 0.5 + j * 0.98 - 0.15, -0.02), mat="Stone")
                B.jitter(t, 0.008, i * 7 + j)
                parts.append(t)
        parts.append(B.cyl(2.0, 0.06, loc=(-CL / 2, 0, -0.02), verts=24, mat="Stone"))
        # one step up into the sanctuary behind the screen
        parts.append(B.box((2.6, CW - 2 * CT, 0.14), loc=(-CL / 2 + CT + 1.3, 0, 0), mat="Stone"))
        o = B.join(parts, "SM_ChurchFloor")
        B.finish(o, OUT, center_xy=False, dark_base=0.85, height=0.1)
    if want("SM_ChurchRoof"):
        parts = []
        half = CW / 2 + 0.35
        rise = 1.9
        ang = math.atan2(rise, CW / 2)
        slab_w = math.hypot(half, rise * half / (CW / 2)) + 0.05
        for s in (-1, 1):
            sl = B.box((CL + 0.5, slab_w, 0.22), loc=(0, 0, 0), mat="RoofTile", base=False)
            sl.rotation_euler = (-s * ang, 0, 0)  # outer (eave) end must be the low end
            sl.location = (0, s * (half / 2), CH + rise - rise * 0.5 * (half / (CW / 2)) + 0.12)
            B.apply(sl)
            B.subdivide_edges(sl, 2)
            B.jitter(sl, 0.01, s + 3)
            parts.append(sl)
        # inner ceiling (two flipped slabs) so the interior is not open to the sky when the roof is shown
        o = B.join(parts, "SM_ChurchRoof")
        B.finish(o, OUT, keep_origin=True, dark_base=0.9, height=6)
    if want("SM_ChurchDome"):
        parts = [B.cyl(1.25, 1.5, loc=(0, 0, 0), verts=24, mat="Plaster")]
        for i in range(8):
            a = 2 * pi * i / 8
            parts.append(B.box((0.08, 0.28, 0.65), loc=(1.25 * math.cos(a), 1.25 * math.sin(a), 0.45), rot=(0, 0, a), mat="Dark"))
        dm = B.sphere(1.3, loc=(0, 0, 1.5), seg=24, rings=12, mat="Plaster")
        bm = bmesh.new(); bm.from_mesh(dm.data)
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < 1.49], context='VERTS')
        bm.to_mesh(dm.data); bm.free()
        parts.append(dm)
        parts.append(B.cyl(0.04, 1.3, loc=(0, 0, 2.75), verts=6, mat="Bronze"))
        parts.append(B.box((0.05, 0.7, 0.05), loc=(0, 0, 3.6), mat="Bronze"))
        parts.append(B.box((0.05, 0.38, 0.05), loc=(0, 0, 3.82), mat="Bronze"))
        parts.append(B.box((0.05, 0.45, 0.05), loc=(0, 0, 3.25), rot=(0.35, 0, 0), mat="Bronze"))
        o = B.join(parts, "SM_ChurchDome")
        B.finish(o, OUT, center_xy=False, dark_base=0.9, height=1)
    if want("SM_Lattice"):
        parts = [B.box((0.08, 0.64, 0.06), loc=(0, 0, 0), mat="DarkWood"), B.box((0.08, 0.64, 0.06), loc=(0, 0, 0.98), mat="DarkWood")]
        for i in range(5):
            parts.append(B.box((0.05, 0.03, 1.0), loc=(0, -0.24 + i * 0.12, 0), mat="DarkWood"))
        for i in range(7):
            parts.append(B.box((0.05, 0.62, 0.03), loc=(0, 0, 0.1 + i * 0.13), mat="DarkWood"))
        o = B.join(parts, "SM_Lattice")
        B.finish(o, OUT, center_xy=True, vc=False)
    if want("SM_ChurchDoor"):
        # one leaf, hinge at local y=0, spans +Y 0.75 m, faces +X
        parts = []
        for i in range(4):
            parts.append(B.box((0.08, 0.18, 2.62), loc=(0, 0.1 + i * 0.185, 0), mat="DarkWood"))
        for z in (0.4, 1.3, 2.2):
            parts.append(B.box((0.05, 0.72, 0.1), loc=(0.06, 0.375, z), mat="Iron"))
        parts.append(B.torus(0.07, 0.012, loc=(0.1, 0.62, 1.25), rot=(0, pi / 2, 0), mat="Iron", maj=12, mino=4))
        o = B.join(parts, "SM_ChurchDoor")
        B.finish(o, OUT, center_xy=False, dark_base=0.7, height=1.2)
    if want("SM_Chain"):
        parts = []
        for i in range(12):
            a = i * 0.3
            parts.append(B.torus(0.045, 0.012, loc=(0, -0.72 + i * 0.12, 1.25 - 0.06 * math.sin(i / 11 * pi)), rot=(0 if i % 2 else pi / 2, pi / 2, 0), mat="Iron", maj=8, mino=4))
        parts.append(B.box((0.05, 0.14, 0.16), loc=(0.02, 0, 1.0), mat="Iron"))
        parts.append(B.torus(0.05, 0.014, loc=(0.02, 0, 1.17), rot=(0, pi / 2, 0), mat="Iron", maj=10, mino=4))
        o = B.join(parts, "SM_Chain")
        B.finish(o, OUT, center_xy=False, vc=False)


church()

# ------------------------------------------------------------------ church interior props
if want("SM_Templon"):
    # icon screen across the nave; four icon places at y = -2.0, -1.0, 1.0, 2.0 (Blender y), opening in the middle
    parts = []
    W2 = 5.6
    parts.append(B.box((0.18, W2, 0.18), loc=(0, 0, 2.55), mat="DarkWood"))
    parts.append(B.box((0.14, W2, 0.1), loc=(0, 0, 0.0), mat="DarkWood"))
    for y in (-2.75, -1.5, -0.55, 0.55, 1.5, 2.75):
        p = B.box((0.14, 0.12, 2.6), loc=(0, y, 0), mat="DarkWood")
        parts.append(p)
    for y in (-2.1, 2.1):
        parts.append(B.box((0.06, 1.2, 0.8), loc=(0, y, 0.05), mat="Wood"))
    for y in (-1.02, 1.02):
        parts.append(B.box((0.06, 0.9, 0.55), loc=(0, y, 0.05), mat="Wood"))
    # small cross atop the beam
    parts.append(B.box((0.06, 0.08, 0.6), loc=(0, 0, 2.73), mat="Gold"))
    parts.append(B.box((0.06, 0.4, 0.07), loc=(0, 0, 3.1), mat="Gold"))
    o = B.join(parts, "SM_Templon")
    B.finish(o, OUT, center_xy=False, dark_base=0.6, height=1.5)

if want("SM_IconBoard"):
    # 0.62 x 0.92 board; front faces +X with UV 0..1 on slot IconFront; origin at bottom centre
    w, h, t = 0.62, 0.92, 0.05
    back = B.box((t, w, h), loc=(-t / 2 - 0.002, 0, 0), mat="Wood")
    front = B.from_pydata("front", [(0, w / 2, 0), (0, -w / 2, 0), (0, -w / 2, h), (0, w / 2, h)], [(0, 1, 2, 3)], "IconFront")
    me = front.data
    if me.polygons[0].normal.x < 0:
        me.polygons[0].flip()
    uv = me.uv_layers.new(name="UV")
    # Blender +y is UE -y: as seen from +X in Unreal, Blender +y is on the viewer's left
    for li in me.polygons[0].loop_indices:
        co = me.vertices[me.loops[li].vertex_index].co
        uv.data[li].uv = ((w / 2 - co.y) / w if False else (co.y + w / 2) / w, co.z / h)
    B.uv_box(back, 1.0)
    frame = []
    for z in (0.0, h - 0.04):
        frame.append(B.box((0.07, w + 0.04, 0.04), loc=(0.0, 0, z), mat="Gold"))
    for y in (-w / 2, w / 2):
        frame.append(B.box((0.07, 0.04, h), loc=(0.0, y, 0), mat="Gold"))
    for f in frame:
        B.uv_box(f, 1.0)
    o = B.join([back, front] + frame, "SM_IconBoard")
    B.grime(o, dark_base=1.0, height=0, noise=0.05)
    B.smooth(o)
    B.origin_base(o)
    B.export(o, os.path.join(OUT, "SM_IconBoard.fbx"))
    print("EXPORT SM_IconBoard")
    bpy.data.objects.remove(o)

if want("SM_Altar"):
    a = B.box((1.0, 1.3, 0.98), mat="Stone")
    B.bevel(a, 0.03, 2)
    top = B.box((1.1, 1.4, 0.08), loc=(0, 0, 0.98), mat="Stone")
    B.bevel(top, 0.02, 2)
    o = B.join([a, top], "SM_Altar")
    B.finish(o, OUT, dark_base=0.7, height=0.6)

if want("SM_AltarCloth"):
    # cloth draped over the altar top (1.12 x 1.42) hanging 0.45 on all sides; top face UV for the closed-eye design
    c = B.box((1.16, 1.46, 0.5), loc=(0, 0, 0.6), mat="ClosedEyeCloth")
    bm = bmesh.new(); bm.from_mesh(c.data)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.normal.z < -0.5], context='FACES')
    bm.to_mesh(c.data); bm.free()
    B.subdivide_edges(c, 3)
    for v in c.data.vertices:
        if v.co.z < 1.05:
            v.co.x *= 1.0 + 0.03 * math.sin(v.co.y * 14)
            v.co.y *= 1.0 + 0.03 * math.sin(v.co.x * 14)
    me = c.data
    uv = me.uv_layers.new(name="UV")
    for p in me.polygons:
        for li in p.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            if p.normal.z > 0.5:
                uv.data[li].uv = (co.y / 1.46 + 0.5, co.x / 1.16 + 0.5)
            else:
                uv.data[li].uv = (0.02, 0.02)
    B.finish(c, OUT, "SM_AltarCloth", vc=False)

if want("SM_Bowl"):
    b = B.sphere(0.12, scale=(1, 1, 0.5), mat="Clay")
    bm = bmesh.new(); bm.from_mesh(b.data)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z > 0.01], context='VERTS')
    bm.to_mesh(b.data); bm.free()
    ash = B.cyl(0.1, 0.01, loc=(0, 0, -0.02), verts=12, mat="Ash")
    o = B.join([b, ash], "SM_Bowl")
    B.finish(o, OUT, vc=False)

if want("SM_Lampada"):
    # hanging glass oil lamp with a chain, origin at the bottom of the cup; the chain runs 1.2 m up
    parts = [B.sphere(0.08, scale=(1, 1, 0.9), mat="GlassRed"), B.cyl(0.05, 0.03, loc=(0, 0, -0.08), verts=10, mat="Bronze")]
    for i in range(3):
        a = 2 * pi * i / 3
        parts.append(B.cyl(0.004, 0.5, loc=(0.07 * math.cos(a), 0.07 * math.sin(a), 0.02), rot=(0.12 * math.sin(a), -0.12 * math.cos(a), 0), verts=4, mat="Bronze"))
    parts.append(B.cyl(0.006, 0.9, loc=(0, 0, 0.5), verts=4, mat="Bronze"))
    o = B.join(parts, "SM_Lampada")
    B.finish(o, OUT, vc=False)

if want("SM_Brazier"):
    parts = [B.sphere(0.32, loc=(0, 0, 0.75), scale=(1, 1, 0.45), mat="Bronze")]
    bm = bmesh.new(); bm.from_mesh(parts[0].data)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z > 0.78], context='VERTS')
    bm.to_mesh(parts[0].data); bm.free()
    parts.append(B.cyl(0.3, 0.02, loc=(0, 0, 0.7), verts=16, mat="Ash"))
    for i in range(3):
        a = 2 * pi * i / 3
        parts.append(B.cyl(0.02, 0.8, loc=(0.18 * math.cos(a), 0.18 * math.sin(a), 0), rot=(0.25 * math.sin(a), -0.25 * math.cos(a), 0), verts=5, mat="Bronze"))
    o = B.join(parts, "SM_Brazier")
    B.finish(o, OUT, vc=False)

if want("SM_ReadingStand"):
    parts = [B.cyl(0.05, 1.05, verts=8, mat="DarkWood"), B.box((0.5, 0.5, 0.05), mat="DarkWood"),
             B.box((0.45, 0.55, 0.04), loc=(0, 0, 1.08), rot=(0, 0.45, 0), mat="DarkWood")]
    o = B.join(parts, "SM_ReadingStand")
    B.finish(o, OUT, dark_base=0.8, height=1)

if want("SM_Book"):
    parts = [B.box((0.24, 0.32, 0.06), mat="Leather"), B.box((0.22, 0.3, 0.05), loc=(0.012, 0, 0.005), mat="Parchment")]
    o = B.join(parts, "SM_Book")
    B.finish(o, OUT, vc=False)

if want("SM_Chest"):
    parts = [B.box((0.6, 1.0, 0.5), mat="DarkWood"), B.box((0.62, 1.02, 0.08), loc=(0, 0, 0.5), mat="DarkWood")]
    for y in (-0.35, 0.35):
        parts.append(B.box((0.64, 0.05, 0.6), loc=(0, y, -0.01), mat="Iron"))
    parts.append(B.box((0.03, 0.1, 0.12), loc=(0.31, 0, 0.38), mat="Iron"))
    o = B.join(parts, "SM_Chest")
    B.finish(o, OUT, dark_base=0.7, height=0.4)

if want("SM_Trapdoor"):
    parts = [B.box((1.0, 1.0, 0.06), mat="DarkWood")]
    for y in (-0.3, 0.3):
        parts.append(B.box((1.02, 0.06, 0.07), loc=(0, y, 0.0), mat="Iron"))
    parts.append(B.torus(0.06, 0.012, loc=(0.35, 0, 0.07), rot=(0, 0, 0), mat="Iron", maj=10, mino=4))
    o = B.join(parts, "SM_Trapdoor")
    B.finish(o, OUT, vc=False)

if want("SM_Censer"):
    parts = [B.sphere(0.08, scale=(1, 1, 0.8), mat="Bronze"), B.cyl(0.04, 0.05, loc=(0, 0, -0.1), verts=10, mat="Bronze")]
    for i in range(4):
        a = 2 * pi * i / 4
        parts.append(B.cyl(0.003, 0.7, loc=(0.06 * math.cos(a), 0.06 * math.sin(a), 0.05), rot=(0.08 * math.sin(a), -0.08 * math.cos(a), 0), verts=3, mat="Bronze"))
    parts.append(B.torus(0.03, 0.006, loc=(0, 0, 0.75), rot=(pi / 2, 0, 0), mat="Bronze", maj=10, mino=4))
    o = B.join(parts, "SM_Censer")
    B.finish(o, OUT, vc=False)

if want("SM_CandleStand"):
    parts = [B.cyl(0.04, 1.05, verts=8, mat="Bronze"), B.cyl(0.2, 0.05, verts=12, mat="Bronze"),
             B.cyl(0.28, 0.08, loc=(0, 0, 1.05), verts=16, mat="Bronze"), B.cyl(0.25, 0.03, loc=(0, 0, 1.11), verts=16, mat="Sand")]
    o = B.join(parts, "SM_CandleStand")
    B.finish(o, OUT, vc=False)

if want("SM_Candle"):
    c = B.cyl(0.012, 0.26, verts=8, mat="Candle")
    B.finish(c, OUT, "SM_Candle", vc=False)

if want("SM_Flame"):
    f = B.sphere(0.02, scale=(1, 1, 2.4), seg=8, rings=6, mat="Flame")
    B.finish(f, OUT, "SM_Flame", vc=False)

if want("SM_Codex"):
    parts = [B.box((0.15, 0.2, 0.04), mat="Leather"), B.box((0.14, 0.19, 0.03), loc=(0.006, 0, 0.005), mat="Parchment")]
    o = B.join(parts, "SM_Codex")
    B.finish(o, OUT, vc=False)

if want("SM_SemantronFrame"):
    parts = []
    for y in (-1.25, 1.25):
        parts.append(B.cyl(0.08, 2.4, loc=(0, y, 0), verts=8, mat="DarkWood"))
    parts.append(B.box((0.14, 2.8, 0.14), loc=(0, 0, 2.35), mat="DarkWood"))
    for y in (-0.8, 0.8):
        parts.append(B.cyl(0.008, 0.6, loc=(0, y, 1.75), verts=4, mat="Rope"))
    o = B.join(parts, "SM_SemantronFrame")
    B.finish(o, OUT, center_xy=False, dark_base=0.6, height=1)

if want("SM_Semantron"):
    s = B.box((0.06, 2.0, 0.24), mat="Wood")
    B.bevel(s, 0.01, 1)
    B.finish(s, OUT, "SM_Semantron", dark_base=0.8, height=0.2)

if want("SM_SemantronWrapped"):
    s = B.box((0.3, 2.1, 0.32), mat="Sacking")
    B.subdivide_edges(s, 3)
    B.jitter(s, 0.03, 8)
    B.finish(s, OUT, "SM_SemantronWrapped", dark_base=0.8, height=0.2)

if want("SM_Mallet"):
    parts = [B.cyl(0.015, 0.35, rot=(0, pi / 2, 0), verts=6, mat="Wood"), B.cyl(0.04, 0.12, loc=(0.35, -0.06, 0), rot=(pi / 2, 0, 0), verts=8, mat="Wood")]
    o = B.join(parts, "SM_Mallet")
    B.finish(o, OUT, vc=False)

if want("SM_Jar"):
    j = B.sphere(0.22, loc=(0, 0, 0.3), scale=(1, 1, 1.4), mat="Clay")
    n = B.cyl(0.08, 0.18, loc=(0, 0, 0.55), verts=12, mat="Clay")
    seal = B.cyl(0.085, 0.04, loc=(0, 0, 0.72), verts=12, mat="Wax")
    o = B.join([j, n, seal], "SM_Jar")
    B.finish(o, OUT, dark_base=0.7, height=0.4)

if want("SM_Pot"):
    j = B.sphere(0.3, loc=(0, 0, 0.3), scale=(1, 1, 0.9), mat="Clay")
    B.jitter(j, 0.008, 2)
    B.finish(j, OUT, "SM_Pot", dark_base=0.7, height=0.4)

if want("SM_Basket"):
    b = B.cyl(0.28, 0.3, r2=0.34, verts=14, mat="Reed")
    B.finish(b, OUT, "SM_Basket", dark_base=0.7, height=0.3)

if want("SM_Mat"):
    m = B.box((1.9, 0.9, 0.02), mat="Reed")
    B.finish(m, OUT, "SM_Mat", vc=False)

if want("SM_Key"):
    parts = [B.torus(0.015, 0.004, rot=(pi / 2, 0, 0), mat="Iron", maj=8, mino=4), B.cyl(0.003, 0.06, loc=(0, 0, -0.07), verts=4, mat="Iron")]
    o = B.join(parts, "SM_Key")
    B.finish(o, OUT, vc=False)

if want("SM_Stole"):
    # priest's stole (epitrachelion) hanging from a nail, torn
    s = B.box((0.02, 0.22, 1.3), mat="StoleRed")
    B.subdivide_edges(s, 3)
    for v in s.data.vertices:
        v.co.x += 0.02 * math.sin(v.co.z * 7)
    for i in range(4):
        parts_ = B.box((0.025, 0.18, 0.02), loc=(0.0, 0, 0.2 + i * 0.3), mat="Gold")
        s = B.join([s, parts_], "SM_Stole")
    nail = B.cyl(0.005, 0.06, loc=(-0.03, 0, 1.3), rot=(0, pi / 2, 0), verts=4, mat="Iron")
    o = B.join([s, nail], "SM_Stole")
    B.finish(o, OUT, vc=False)

# ------------------------------------------------------------------ burial ground
if want("SM_Stele"):
    s = B.box((0.18, 0.55, 1.0), mat="Stone")
    B.subdivide_edges(s, 1)
    B.bevel(s, 0.04, 2)
    for v in s.data.vertices:
        if v.co.z > 0.9:
            v.co.z += 0.12 * (1 - (2 * v.co.y / 0.55) ** 2)
    B.jitter(s, 0.012, 3)
    cr = [B.box((0.03, 0.08, 0.36), loc=(0.09, 0, 0.45), mat="Stone"), B.box((0.03, 0.26, 0.07), loc=(0.09, 0, 0.66), mat="Stone")]
    o = B.join([s] + cr, "SM_Stele")
    B.finish(o, OUT, dark_base=0.5, height=0.6)

if want("SM_GraveCross"):
    parts = [B.box((0.08, 0.08, 1.3), mat="DarkWood"), B.box((0.08, 0.6, 0.08), loc=(0, 0, 0.95), mat="DarkWood")]
    o = B.join(parts, "SM_GraveCross")
    B.jitter(o, 0.006, 4)
    B.finish(o, OUT, dark_base=0.6, height=0.8)

if want("SM_GraveMound"):
    m = B.sphere(0.6, scale=(1.6, 0.8, 0.35), mat="Ground")
    bm = bmesh.new(); bm.from_mesh(m.data)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < -0.02], context='VERTS')
    bm.to_mesh(m.data); bm.free()
    B.jitter(m, 0.03, 6)
    B.finish(m, OUT, "SM_GraveMound", dark_base=1.0, height=0, noise=0.2)

if want("SM_BoneHouseEntrance"):
    # small stone hut over a stair going down; opening faces +X
    parts = []
    L, W, H = 2.6, 2.4, 2.2
    parts.append(B.box((L, 0.4, H), loc=(0, W / 2 - 0.2, 0), mat="Stone"))
    parts.append(B.box((L, 0.4, H), loc=(0, -W / 2 + 0.2, 0), mat="Stone"))
    parts.append(B.box((0.4, W, H), loc=(-L / 2 + 0.2, 0, 0), mat="Stone"))
    parts.append(B.box((L + 0.3, W + 0.3, 0.3), loc=(0, 0, H), mat="Stone"))
    parts.append(B.box((0.35, W - 0.6, 0.45), loc=(L / 2 - 0.1, 0, H - 0.45), mat="Stone"))
    for i in range(5):
        parts.append(B.box((0.35, W - 0.85, 0.2), loc=(L / 2 - 0.5 - i * 0.35, 0, -0.2 * (i + 1)), mat="Stone"))
    parts.append(B.box((0.1, W - 0.8, H), loc=(-L / 2 + 0.45, 0, -1.2), mat="Dark"))
    parts.append(B.box((L - 0.8, W - 0.8, 0.05), loc=(-0.1, 0, -1.2), mat="Dark"))
    # cross carved over the door
    parts.append(B.box((0.05, 0.08, 0.35), loc=(L / 2 + 0.05, 0, H - 0.05 + 0.0), mat="Stone"))
    o = B.join(parts, "SM_BoneHouseEntrance")
    for p in o.data.polygons:
        pass
    B.jitter(o, 0.01, 2)
    B.finish(o, OUT, keep_origin=True, dark_base=0.45, height=1.2)

if want("SM_TombRidge"):
    # long rock ridge with rock-cut doorways, faces +X; placed far west as a backdrop
    verts, faces = [], []
    NX, NY = 10, 40
    for j in range(NY + 1):
        for i in range(NX + 1):
            u, v = i / NX, j / NY
            x = -6 + 12 * u
            y = -45 + 90 * v
            z = 9.0 * (1 - u) ** 0.8 * (0.85 + 0.15 * math.sin(v * 13)) + 0.6 * math.sin(v * 31 + u * 4)
            verts.append((x, y, max(0, z)))
    for j in range(NY):
        for i in range(NX):
            a = j * (NX + 1) + i
            faces.append((a, a + 1, a + NX + 2, a + NX + 1))
    rg = B.from_pydata("ridge", verts, faces, "Rock")
    if rg.data.polygons[0].normal.z < 0:
        for p in rg.data.polygons: p.flip()
    parts = [rg]
    for k in range(9):
        y = -36 + k * 9 + R.uniform(-2, 2)
        z = R.uniform(1.0, 4.0)
        u = 1 - (z / 9.0) ** 1.25
        x = -6 + 12 * u
        parts.append(B.box((0.5, 1.0, 1.6), loc=(x + 0.1, y, z), mat="Dark"))
    o = B.join(parts, "SM_TombRidge")
    B.finish(o, OUT, center_xy=False, dark_base=0.7, height=6)

# ------------------------------------------------------------------ the bone house (ossuary) interior
# Main passage along -X from the stair (x=0) to x=-26; side passage north (Blender +y) at x=-9 to a chamber.
if want("SM_Ossuary"):
    parts = []
    W, H = 2.4, 2.7

    def corridor(x0, x1, y0, y1, along):
        # floor, two walls and a barrel vault
        if along == 'x':
            L = abs(x1 - x0); cx = (x0 + x1) / 2
            parts.append(B.box((L, W + 1.0, 0.2), loc=(cx, (y0 + y1) / 2, -0.2), mat="Stone"))
            for s in (-1, 1):
                # the wall on the camera side (Unreal +Y = Blender -y) is drawn cut away
                w = B.box((L, 0.5, H), loc=(cx, (y0 + y1) / 2 + s * (W / 2 + 0.25), 0), mat="RockWall" if s > 0 else "Cutaway")
                B.subdivide_edges(w, 3); B.jitter(w, 0.05, int(cx + s))
                parts.append(w)
            v = B.cyl(W / 2 + 0.25, L, loc=(x0 if x0 < x1 else x1, (y0 + y1) / 2, H), rot=(0, pi / 2, 0), verts=16, mat="Cutaway", cap=False)
            bm = bmesh.new(); bm.from_mesh(v.data)
            bmesh.ops.delete(bm, geom=[q for q in bm.verts if q.co.z < H - 0.01], context='VERTS')
            bm.to_mesh(v.data); bm.free()
            for p in v.data.polygons: p.flip()
            parts.append(v)
        else:
            L = abs(y1 - y0); cy = (y0 + y1) / 2
            parts.append(B.box((W + 1.0, L, 0.2), loc=((x0 + x1) / 2, cy, -0.2), mat="Stone"))
            for s in (-1, 1):
                w = B.box((0.5, L, H), loc=((x0 + x1) / 2 + s * (W / 2 + 0.25), cy, 0), mat="Cutaway" if s > 0 else "RockWall")
                B.subdivide_edges(w, 3); B.jitter(w, 0.05, int(cy + s))
                parts.append(w)
            v = B.cyl(W / 2 + 0.25, L, loc=((x0 + x1) / 2, y0 if y0 < y1 else y1, H), rot=(-pi / 2, 0, 0), verts=16, mat="Cutaway", cap=False)
            bm = bmesh.new(); bm.from_mesh(v.data)
            bmesh.ops.delete(bm, geom=[q for q in bm.verts if q.co.z < H - 0.01], context='VERTS')
            bm.to_mesh(v.data); bm.free()
            for p in v.data.polygons: p.flip()
            parts.append(v)

    # main passage: two runs leaving a gap in the north wall for the side passage at x=-9
    corridor(1.5, -7.8, 0, 0, 'x')
    corridor(-10.2, -27.0, 0, 0, 'x')
    # junction: floor + south wall + vault cover
    parts.append(B.box((2.4, W + 1.0, 0.2), loc=(-9.0, 0, -0.2), mat="Stone"))
    jw = B.box((2.4, 0.5, H), loc=(-9.0, -(W / 2 + 0.25), 0), mat="Cutaway"); B.jitter(jw, 0.04, 5); parts.append(jw)
    parts.append(B.box((2.4, W + 1.0, 0.5), loc=(-9.0, 0, H + 0.9), mat="Cutaway"))
    # side passage north and chamber
    corridor(-9.0, -9.0, W / 2 + 0.5, 8.0, 'y')
    cx, cy = -9.0, 10.2
    parts.append(B.box((5.0, 4.6, 0.2), loc=(cx, cy, -0.2), mat="Stone"))
    for (lx, ly, sx, sy, mm) in [(cx, cy + 2.3, 5.0, 0.5, "RockWall"), (cx - 2.5, cy, 0.5, 4.6, "RockWall"), (cx + 2.5, cy, 0.5, 4.6, "Cutaway")]:
        w = B.box((sx, sy, H + 0.6), loc=(lx, ly, 0), mat=mm); B.subdivide_edges(w, 3); B.jitter(w, 0.05, int(lx)); parts.append(w)
    for sx in (-1, 1):
        w = B.box((1.3, 0.5, H + 0.6), loc=(cx + sx * 1.85, cy - 2.3, 0), mat="Cutaway"); B.jitter(w, 0.04, sx); parts.append(w)
    parts.append(B.box((5.0, 4.6, 0.4), loc=(cx, cy, H + 0.6), mat="Cutaway"))
    # end walls
    parts.append(B.box((0.5, W + 1.0, H + 1.4), loc=(-27.2, 0, 0), mat="RockWall"))
    parts.append(B.box((0.5, W + 1.0, H + 1.4), loc=(1.7, 0, 0), mat="Cutaway"))
    # the stair down from the burial ground arrives at the east end (steps rising towards +X, cut off by the ceiling)
    for i in range(6):
        # step i sits at x = -0.7 + 0.4 i and gets taller towards the east end wall
        parts.append(B.box((0.4, 1.6, 0.22 * (i + 1)), loc=(-0.7 + 0.4 * i + 0.2, 0, 0), mat="Stone"))
    # stair up to the trapdoor in the chamber's north-west corner
    for i in range(8):
        parts.append(B.box((1.2, 0.4, 0.3 * (i + 1)), loc=(cx - 1.6, cy - 1.6 + i * 0.4, 0), mat="Stone"))
    # bone niches along the main passage walls (shelves)
    for s in (-1, 1):
        for x in range(-25, 0, 2):
            if -11 < x < -7 and s == 1:
                continue
            for z in (0.5, 1.2, 1.9):
                parts.append(B.box((1.6, 0.35, 0.08), loc=(x + 0.0, s * (W / 2 - 0.05), z), mat="Stone"))
    o = B.join(parts, "SM_Ossuary")
    B.finish(o, OUT, keep_origin=True, dark_base=0.5, height=1.5, uv_scale=0.6)

if want("SM_OssSkulls"):
    # skulls on the niche shelves, plus a heap of bones in the chamber
    parts = []
    rr = random.Random(77)
    W = 2.4
    count = 0
    for s in (-1, 1):
        for x in range(-25, 0, 2):
            if -11 < x < -7 and s == 1:
                continue
            for z in (0.58, 1.28, 1.98):
                for k in range(5):
                    if rr.random() < 0.18:
                        continue
                    px = x - 0.6 + k * 0.3 + rr.uniform(-0.03, 0.03)
                    py = s * (W / 2 - 0.08)
                    sk = B.sphere(0.09, loc=(px, py, z + 0.085), scale=(0.85, 1.05, 0.95), seg=10, rings=7, mat="Bone")
                    jaw = B.box((0.1, 0.09, 0.05), loc=(px, py - s * 0.05, z), mat="Bone")
                    e1 = B.sphere(0.022, loc=(px - 0.035, py - s * 0.085, z + 0.09), seg=6, rings=4, mat="Dark")
                    e2 = B.sphere(0.022, loc=(px + 0.035, py - s * 0.085, z + 0.09), seg=6, rings=4, mat="Dark")
                    parts += [sk, jaw, e1, e2]
                    count += 1
    o = B.join(parts, "SM_OssSkulls")
    B.finish(o, OUT, keep_origin=True, dark_base=0.6, height=2.0, seed=4)
    print("SKULLS", count)

if want("SM_BonePile"):
    parts = []
    rr = random.Random(12)
    for i in range(40):
        a = rr.uniform(0, 2 * pi); d = rr.uniform(0, 0.8)
        b = B.cyl(0.025, rr.uniform(0.3, 0.5), loc=(d * math.cos(a), d * math.sin(a), rr.uniform(0, 0.4) * (1 - d)), rot=(rr.uniform(0, pi), rr.uniform(0, pi), 0), verts=6, mat="Bone", base=False)
        parts.append(b)
    for i in range(6):
        a = rr.uniform(0, 2 * pi); d = rr.uniform(0, 0.6)
        parts.append(B.sphere(0.09, loc=(d * math.cos(a), d * math.sin(a), 0.1 + rr.uniform(0, 0.3)), seg=10, rings=7, mat="Bone"))
    o = B.join(parts, "SM_BonePile")
    B.finish(o, OUT, dark_base=0.6, height=0.5)

if want("SM_ScratchPanel"):
    # 2.4 x 1.8 decal panel for the scratched "Lord have mercy" wall, faces +X, UV 0..1
    o = B.from_pydata("SM_ScratchPanel", [(0, 1.2, 0), (0, -1.2, 0), (0, -1.2, 1.8), (0, 1.2, 1.8)], [(0, 1, 2, 3)], "Scratch")
    me = o.data
    if me.polygons[0].normal.x < 0:
        me.polygons[0].flip()
    uv = me.uv_layers.new(name="UV")
    for li in me.polygons[0].loop_indices:
        co = me.vertices[me.loops[li].vertex_index].co
        uv.data[li].uv = ((co.y + 1.2) / 2.4, co.z / 1.8)
    B.finish(o, OUT, vc=False)

if want("SM_Shelf"):
    parts = [B.box((0.35, 1.2, 0.05), loc=(0, 0, 1.0), mat="DarkWood"), B.box((0.35, 1.2, 0.05), loc=(0, 0, 0.5), mat="DarkWood")]
    for y in (-0.55, 0.55):
        parts.append(B.box((0.35, 0.05, 1.1), loc=(0, y, 0), mat="DarkWood"))
    o = B.join(parts, "SM_Shelf")
    B.finish(o, OUT, dark_base=0.6, height=1)

if want("SM_Table"):
    parts = [B.box((1.8, 0.9, 0.06), loc=(0, 0, 0.72), mat="DarkWood")]
    for sx in (-0.8, 0.8):
        for sy in (-0.35, 0.35):
            parts.append(B.box((0.07, 0.07, 0.72), loc=(sx, sy, 0), mat="DarkWood"))
    o = B.join(parts, "SM_Table")
    B.finish(o, OUT, dark_base=0.7, height=0.6)

# ------------------------------------------------------------------ convent of the Theotokos (far bank)
if want("SM_ConventWall"):
    parts = []
    L, W, H = 16.0, 14.0, 3.4
    gate = 1.8
    for (lx, ly, sx, sy) in [(0, W / 2, L, 0.6), (0, -W / 2, L, 0.6), (L / 2, 0, 0.6, W)]:
        w = B.box((sx, sy, H), loc=(lx, ly, 0), mat="MudBrick"); B.subdivide_edges(w, 3); B.jitter(w, 0.03, int(lx + ly)); parts.append(w)
    for s in (-1, 1):
        seg = (W - gate) / 2
        w = B.box((0.6, seg, H), loc=(-L / 2, s * (gate / 2 + seg / 2), 0), mat="MudBrick"); B.subdivide_edges(w, 3); B.jitter(w, 0.03, s + 9); parts.append(w)
        parts.append(B.box((0.9, 0.5, H + 0.6), loc=(-L / 2, s * (gate / 2 + 0.25), 0), mat="Plaster"))
    parts.append(B.box((0.9, gate + 1.0, 0.5), loc=(-L / 2, 0, H + 0.1), mat="Plaster"))
    parts.append(B.box((0.08, 0.1, 0.6), loc=(-L / 2 - 0.45, 0, H + 0.6), mat="Wood"))
    parts.append(B.box((0.08, 0.4, 0.08), loc=(-L / 2 - 0.45, 0, H + 1.0), mat="Wood"))
    # the house inside, with a high window that holds the lamp
    hb = B.box((6.0, 7.0, 6.0), loc=(2.0, 0, 0), mat="MudBrick"); B.subdivide_edges(hb, 3); B.jitter(hb, 0.03, 31); parts.append(hb)
    parts.append(B.box((0.06, 0.6, 0.8), loc=(-1.01, 1.5, 4.4), mat="Dark"))
    parts.append(B.box((6.2, 7.2, 0.2), loc=(2.0, 0, 6.0), mat="MudBrick"))
    parts.append(B.box((0.06, 1.0, 2.0), loc=(-1.01, -1.5, 0), mat="DarkWood"))
    o = B.join(parts, "SM_ConventWall")
    B.finish(o, OUT, center_xy=False, dark_base=0.5, height=1.2)

if want("SM_Gate"):
    parts = []
    for i in range(5):
        parts.append(B.box((0.08, 0.17, 2.4), loc=(0, 0.09 + i * 0.18, 0), mat="DarkWood"))
    parts.append(B.box((0.05, 0.9, 0.12), loc=(0.05, 0.45, 0.5), mat="Wood"))
    parts.append(B.box((0.05, 0.9, 0.12), loc=(0.05, 0.45, 1.8), mat="Wood"))
    o = B.join(parts, "SM_Gate")
    B.finish(o, OUT, center_xy=False, dark_base=0.7, height=1)

# ------------------------------------------------------------------ hand props
if want("SM_HandLamp"):
    # clay oil lamp, nozzle along +X
    b = B.sphere(0.06, scale=(1.3, 1, 0.5), mat="Clay")
    nz = B.cyl(0.018, 0.06, loc=(0.06, 0, 0.0), rot=(0, pi / 2, 0), verts=8, mat="Clay")
    hd = B.torus(0.02, 0.006, loc=(-0.08, 0, 0.01), rot=(pi / 2, 0, 0), mat="Clay", maj=8, mino=4)
    o = B.join([b, nz, hd], "SM_HandLamp")
    B.finish(o, OUT, vc=False)

if want("SM_CrookLamp"):
    # bronze lamp with a handle curled like a shepherd's crook
    parts = [B.sphere(0.06, scale=(1.3, 1, 0.55), mat="Bronze"), B.cyl(0.016, 0.06, loc=(0.06, 0, 0), rot=(0, pi / 2, 0), verts=8, mat="Bronze")]
    parts.append(B.cyl(0.008, 0.22, loc=(-0.06, 0, 0.0), verts=6, mat="Bronze"))
    for i in range(12):
        a = pi * i / 11 * 1.4
        parts.append(B.sphere(0.009, loc=(-0.06 + 0.05 * math.sin(a), 0, 0.22 + 0.05 - 0.05 * math.cos(a)), seg=6, rings=4, mat="Bronze"))
    o = B.join(parts, "SM_CrookLamp")
    B.finish(o, OUT, vc=False)

if want("SM_HandCross"):
    # hand cross with the relic of Saint Menas: three-barred, 25 cm
    parts = [B.box((0.02, 0.025, 0.25), loc=(0, 0, 0), mat="Gold"), B.box((0.02, 0.14, 0.022), loc=(0, 0, 0.17), mat="Gold"),
             B.box((0.02, 0.07, 0.016), loc=(0, 0, 0.215), mat="Gold"), B.box((0.02, 0.09, 0.016), loc=(0, 0, 0.08), rot=(0.35, 0, 0), mat="Gold"),
             B.sphere(0.012, loc=(0.012, 0, 0.17), mat="GlassRed")]
    o = B.join(parts, "SM_HandCross")
    B.finish(o, OUT, vc=False)

if want("SM_Flask"):
    # Menas ampulla: flat round clay flask with two handles
    b = B.cyl(0.07, 0.04, rot=(pi / 2, 0, 0), verts=16, mat="Clay", base=False)
    nk = B.cyl(0.015, 0.04, loc=(0, 0, 0.07), verts=8, mat="Clay")
    h1 = B.torus(0.02, 0.006, loc=(0.045, 0, 0.065), rot=(pi / 2, 0, 0), mat="Clay", maj=8, mino=4)
    h2 = B.torus(0.02, 0.006, loc=(-0.045, 0, 0.065), rot=(pi / 2, 0, 0), mat="Clay", maj=8, mino=4)
    o = B.join([b, nk, h1, h2], "SM_Flask")
    B.finish(o, OUT, vc=False, center_xy=True)

if want("SM_LetterScroll"):
    s = B.cyl(0.018, 0.22, rot=(pi / 2, 0, 0), verts=10, mat="Parchment", base=False)
    seal = B.cyl(0.02, 0.008, loc=(0.018, 0, 0), rot=(0, pi / 2, 0), verts=10, mat="Wax", base=False)
    o = B.join([s, seal], "SM_LetterScroll")
    B.finish(o, OUT, vc=False)

if want("SM_Table") or want("SM_Feast"):
    pass

print("BUILD_ENV DONE")
