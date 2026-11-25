# Builds character clothing on Epic's UE5 mannequin skeleton (SK_Mannequin) and exports one FBX per character.
# The clothes are fitted by measuring the mannequin body at every height, then skinned to Epic's bones,
# so the stock idle / walk / jog animations drive them with no retargeting.
# Run: blender -b --factory-startup --python Art/build_chars.py
# Units: centimetres (scene unit scale 0.01), the mannequin faces -Y, its left is +X.
import bpy, bmesh, math, os, sys, random
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "mannequin", "SKM_Manny_Simple.fbx")
OUT = os.path.join(HERE, "FBX", "Chars")
os.makedirs(OUT, exist_ok=True)
ONLY = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
pi = math.pi

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.unit_settings.system = 'METRIC'
sc.unit_settings.scale_length = 0.01
bpy.ops.import_scene.fbx(filepath=SRC)
ARM = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
BODY = [o for o in bpy.data.objects if o.type == 'MESH'][0]
EMP = [o for o in bpy.data.objects if o.type == 'EMPTY']
mw = ARM.matrix_world.copy()
ARM.parent = None
ARM.matrix_world = mw
for e in EMP:
    bpy.data.objects.remove(e)
for o in bpy.data.objects:
    o.select_set(False)


def bone(name):
    b = ARM.data.bones[name]
    return ARM.matrix_world @ b.head_local, ARM.matrix_world @ b.tail_local


def bh(name):
    return bone(name)[0]


# ------------------------------------------------------------------ body measurement
ARM_GROUPS = ("upperarm", "lowerarm", "hand", "thumb", "index", "middle", "ring", "pinky")
LEG_GROUPS = ("thigh", "calf", "foot", "ball")
bw = BODY.matrix_world
vg_names = {g.index: g.name for g in BODY.vertex_groups}
BODY_PTS = []   # (co, dominant group name)
for v in BODY.data.vertices:
    if not v.groups:
        continue
    g = max(v.groups, key=lambda x: x.weight)
    BODY_PTS.append((bw @ v.co, vg_names[g.group]))
TORSO = [p for p, g in BODY_PTS if not g.startswith(ARM_GROUPS) and not g.startswith(("clavicle",))]
NOARM = [p for p, g in BODY_PTS if not g.startswith(ARM_GROUPS)]
print("BODY verts", len(BODY_PTS), "torso", len(TORSO))

PELVIS_Z = bh("pelvis").z
KNEE_Z = bh("calf_l").z
NECK_Z = bh("neck_01").z
HEAD = bh("head")
CENTER_Y = bh("spine_03").y
HEADPTS = [p for p, g in BODY_PTS if g.startswith(("head", "neck_02"))]
HEAD_TOP = max(p.z for p in HEADPTS)
FACE_Y = min(p.y for p in HEADPTS)                       # tip of the nose (most -Y point)
CHIN_Z = min(p.z for p in HEADPTS if p.y < FACE_Y + 6.0 and p.z > HEAD.z - 12)
BROW_Z = CHIN_Z + 0.62 * (HEAD_TOP - CHIN_Z)
HEAD_BACK_Y = max(p.y for p in HEADPTS)
HEAD_HALF_X = max(abs(p.x) for p in HEADPTS)
HEAD_CY = (FACE_Y + HEAD_BACK_Y) / 2
HEAD_CZ = (CHIN_Z + HEAD_TOP) / 2 + 2
SHOULDER_PTS = [p for p, g in BODY_PTS if g.startswith(("clavicle", "upperarm")) and abs(p.x) < bh("upperarm_l").x + 1.5]
SHOULDER_TOP_Z = max(p.z for p in SHOULDER_PTS)
ARMPIT_Z = bh("upperarm_l").z - 12
print("LANDMARKS head_top=%.1f chin=%.1f face_y=%.1f back_y=%.1f half_x=%.1f shoulder_top=%.1f neck=%.1f pelvis=%.1f" % (HEAD_TOP, CHIN_Z, FACE_Y, HEAD_BACK_Y, HEAD_HALF_X, SHOULDER_TOP_Z, NECK_Z, PELVIS_Z))


def profile(z, pts, n=24, band=3.0, cy=None, minr=6.0):
    """Radius per angle around (0, cy) for body points within +-band of z."""
    cy = CENTER_Y if cy is None else cy
    rad = [minr] * n
    for p in pts:
        if abs(p.z - z) > band:
            continue
        dx, dy = p.x, p.y - cy
        a = math.atan2(dy, dx) % (2 * pi)
        k = int(a / (2 * pi) * n) % n
        r = math.hypot(dx, dy)
        for kk in (k - 1, k, k + 1):
            rad[kk % n] = max(rad[kk % n], r * (0.92 if kk != k else 1.0))
    # smooth
    return [(rad[(i - 1) % n] + 2 * rad[i] + rad[(i + 1) % n]) / 4 for i in range(n)]


def ring(z, radii, cy=None, add=0.0, scale=(1.0, 1.0)):
    cy = CENTER_Y if cy is None else cy
    n = len(radii)
    return [Vector(((radii[i] + add) * math.cos(2 * pi * i / n) * scale[0], cy + (radii[i] + add) * math.sin(2 * pi * i / n) * scale[1], z)) for i in range(n)]


def loft(rings_, name, mat, close_top=False):
    n = len(rings_[0])
    verts = [tuple(v) for r in rings_ for v in r]
    faces = []
    for i in range(len(rings_) - 1):
        for j in range(n):
            a = i * n + j; b = i * n + (j + 1) % n
            faces.append((a, b, b + n, a + n))
    if close_top:
        faces.append(tuple((len(rings_) - 1) * n + j for j in range(n)))
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    o = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(o)
    set_mat(o, mat)
    # outward normals
    bm = bmesh.new(); bm.from_mesh(me)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(me); bm.free()
    return o


def set_mat(o, name):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    o.data.materials.clear()
    o.data.materials.append(m)


def solidify(o, t=0.8):
    m = o.modifiers.new("solid", 'SOLIDIFY')
    m.thickness = t
    m.offset = -1
    apply_mods(o)


def apply_mods(o):
    for ob in bpy.context.selected_objects:
        ob.select_set(False)
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    for m in list(o.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)


def prim(kind, name, mat, **kw):
    if kind == "cyl":
        bpy.ops.mesh.primitive_cylinder_add(**kw)
    elif kind == "cone":
        bpy.ops.mesh.primitive_cone_add(**kw)
    elif kind == "sphere":
        bpy.ops.mesh.primitive_uv_sphere_add(**kw)
    elif kind == "cube":
        bpy.ops.mesh.primitive_cube_add(**kw)
    elif kind == "torus":
        bpy.ops.mesh.primitive_torus_add(**kw)
    o = bpy.context.view_layer.objects.active
    o.name = name
    for ob in bpy.context.selected_objects:
        ob.select_set(False)
    o.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    set_mat(o, mat)
    return o


# ------------------------------------------------------------------ skinning
def seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(1e-6, ab.length_squared)))
    return (p - (a + ab * t)).length


def skin(o, weight_fn):
    """weight_fn(co) -> dict bone->weight. Adds groups + armature modifier, parents to ARM."""
    o.vertex_groups.clear()
    groups = {}
    for v in o.data.vertices:
        w = weight_fn(v.co)
        tot = sum(w.values()) or 1.0
        for bname, val in w.items():
            if val <= 1e-4:
                continue
            if bname not in groups:
                groups[bname] = o.vertex_groups.new(name=bname)
            groups[bname].add([v.index], val / tot, 'REPLACE')
    o.parent = ARM
    m = o.modifiers.new("Armature", 'ARMATURE')
    m.object = ARM
    return o


def nearest(bones, power=4.0, top=3):
    segs = {b: bone(b) for b in bones}
    def fn(co):
        ds = sorted(((seg_dist(co, *segs[b]) + 0.5, b) for b in bones))[:top]
        return {b: 1.0 / d ** power for d, b in ds}
    return fn


def rigid(b):
    return lambda co: {b: 1.0}


SPINE = ["pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "neck_01", "clavicle_l", "clavicle_r"]


def torso_weights():
    near = nearest(SPINE + ["upperarm_l", "upperarm_r"], power=3.0)
    def fn(co):
        w = near(co)
        # keep the upper arms from tugging the chest; only the outer shoulder follows them
        for side in ("l", "r"):
            k = "upperarm_" + side
            if k in w:
                sx = co.x if side == "l" else -co.x
                if sx < 16:
                    w[k] *= 0.15
        return w
    return fn


def skirt_weights(hip_z, knee_z):
    def fn(co):
        t = max(0.0, min(1.0, (hip_z - co.z) / max(1.0, hip_z - knee_z)))
        r = math.hypot(co.x, co.y - CENTER_Y) or 1.0
        s = co.x / r                     # +1 = left side, -1 = right side
        wl = 0.62 * max(0.0, min(1.0, (s + 0.45) / 1.45)) * t
        wr = 0.62 * max(0.0, min(1.0, (-s + 0.45) / 1.45)) * t
        return {"pelvis": max(0.05, 1.0 - wl - wr), "thigh_l": wl, "thigh_r": wr}
    return fn


def sleeve_weights(side):
    bones = ["upperarm_" + side, "lowerarm_" + side, "clavicle_" + side, "spine_05"]
    near = nearest(bones, power=4.0, top=2)
    hand = bh("hand_" + side)
    lower = bh("lowerarm_" + side)
    def fn(co):
        w = near(co)
        # bell of the sleeve rides on the forearm, never the hand
        if (co - hand).length < (lower - hand).length * 0.6:
            return {"lowerarm_" + side: 1.0}
        return w
    return fn


def head_weights(neck_blend_z=None, chest=False):
    def fn(co):
        if neck_blend_z is None or co.z >= neck_blend_z:
            return {"head": 1.0}
        t = max(0.0, min(1.0, (neck_blend_z - co.z) / 14.0))
        if chest and t >= 1.0:
            return {"spine_05": 1.0}
        return {"head": 1.0 - t, "neck_01": t * 0.5, "spine_05": t * 0.5}
    return fn


# ------------------------------------------------------------------ garment builders
def robe(name, mat, hem_z=6.0, top_z=None, flare=11.0, ease=2.6, hip_ease=5.0, length_rings=22, train=0.0):
    """Long robe from the collar to hem_z. Torso fitted to the body, skirt flares out to the hem."""
    top_z = NECK_Z - 4.0 if top_z is None else top_z
    rings_ = []
    zs = []
    # torso part: from collar down to hips
    for i in range(9):
        zs.append(top_z - (top_z - (PELVIS_Z - 4)) * i / 8)
    # skirt
    for i in range(1, length_rings + 1):
        zs.append((PELVIS_Z - 4) - ((PELVIS_Z - 4) - hem_z) * i / length_rings)
    pelvis_prof = profile(PELVIS_Z - 6, NOARM, band=6)
    # neckline ring, then the shoulder line (yoke), then down the torso
    if top_z >= NECK_Z - 6:
        neck_pr = profile(NECK_Z - 2, TORSO, band=2.5)
        rings_.append(ring(NECK_Z - 1.0, neck_pr, add=1.4))
        zs = [z for z in zs if z < SHOULDER_TOP_Z - 1.5]
        zs.insert(0, SHOULDER_TOP_Z + 0.8)
    for z in zs:
        if z > PELVIS_Z - 4:
            pts = TORSO + (SHOULDER_PTS if z > ARMPIT_Z else [])
            pr = profile(min(z, SHOULDER_TOP_Z - 2), pts, band=3.5)
            rings_.append(ring(z, pr, add=ease))
        else:
            u = ((PELVIS_Z - 4) - z) / ((PELVIS_Z - 4) - hem_z)
            pr = [max(a, b) for a, b in zip(pelvis_prof, profile(z, NOARM, band=4))]
            add = hip_ease + flare * (u ** 1.3)
            zz = z - train * u * u
            rr = ring(zz, pr, add=add)
            # robes hang a little forward of the legs mid-stride: give the front extra room
            for v in rr:
                if v.y < CENTER_Y:
                    v.y -= 4.0 * u
            rings_.append(rr)
    o = loft(rings_, name, mat)
    solidify(o, 0.9)
    return o


def collar(name, mat, h=4.0, add=1.6):
    z = NECK_Z - 3.0
    pr = profile(z + 2, TORSO, band=3.0)
    rs = [ring(z, pr, add=add + 0.6), ring(z + h, pr, add=add)]
    o = loft(rs, name, mat)
    solidify(o, 0.6)
    return o


def sleeve(name, mat, side, bell=13.0, cuff_back=6.0, ease=2.8, short=False):
    """Tube following clavicle->upperarm->lowerarm, bell-flared at the wrist."""
    sh = bh("upperarm_" + side)
    el = bh("lowerarm_" + side)
    wr = bh("hand_" + side)
    pts_arm = [p for p, g in BODY_PTS if g.startswith(("upperarm_" + side, "lowerarm_" + side))]
    path = []
    steps = 10 if not short else 4
    end = 0.55 if short else 1.0
    for i in range(steps + 1):
        u = i / steps * end
        if u < 0.5:
            p = sh.lerp(el, u / 0.5)
            d = (el - sh).normalized()
        else:
            p = el.lerp(wr, (u - 0.5) / 0.5)
            d = (wr - el).normalized()
        if u >= 0.98:
            p = p - d * cuff_back
        path.append((u, p, d))
    rings_ = []
    n = 14
    for idx, (u, p, d) in enumerate(path):
        # radius from arm points near this cross-section
        rr = 4.0
        for q in pts_arm:
            off = q - p
            if abs(off.dot(d)) < 2.5:
                rr = max(rr, (off - d * off.dot(d)).length)
        rr += ease
        if not short:
            rr += bell * max(0.0, (u - 0.55) / 0.45) ** 1.6
        else:
            rr += 1.5
        # sleeve opening near the shoulder is wider and sinks into the torso
        if idx == 0:
            rr += 1.0
            p = p - d * 6.0 + Vector((0, 0, 1.5))
        up = Vector((0, 0, 1))
        a1 = d.cross(up).normalized()
        a2 = d.cross(a1).normalized()
        sag = Vector((0, 0, -1)) * (bell * 0.45 * max(0.0, (u - 0.6) / 0.4) if not short else 0)
        rings_.append([p + sag * (0.5 + 0.5 * math.sin(2 * pi * j / n)) + (a1 * math.cos(2 * pi * j / n) + a2 * math.sin(2 * pi * j / n)) * rr for j in range(n)])
    o = loft(rings_, name, mat)
    solidify(o, 0.7)
    return o


def kamilavka(name, mat):
    h = HEAD
    top = h.z + 13.0
    base_z = BROW_Z + 2.0
    rad = HEAD_HALF_X + 1.6
    o = prim("cone", name, mat, vertices=24, radius1=rad, radius2=rad + 0.9, depth=14.0, location=(0, HEAD_CY + 1.0, base_z + 7.0))
    rim = prim("cyl", name + "_rim", mat, vertices=24, radius=rad + 1.8, depth=0.8, location=(0, HEAD_CY + 1.0, base_z + 13.8))
    return [o, rim]


def hair_cap(name, mat, h_add=0.0, back=True, z_off=8.0, scale=(1.0, 1.0, 1.0), r=11.0, puff=1.2):
    """Hair as a shell just outside the measured skull, cut away over the face."""
    o = prim("sphere", name, mat, segments=22, ring_count=14, radius=1.0, location=(0, 0, 0))
    rx = HEAD_HALF_X + puff
    ry = (HEAD_BACK_Y - FACE_Y) / 2 * 0.92 + puff
    rz = (HEAD_TOP - CHIN_Z) / 2 * 0.85 + puff
    for v in o.data.vertices:
        x, y, z = v.co
        v.co = Vector((x * rx, HEAD_CY + 1.5 + y * ry, HEAD_CZ + 1.0 + z * rz))
    bm = bmesh.new(); bm.from_mesh(o.data)
    kill = [v for v in bm.verts if (v.co.y < HEAD_CY - 1.0 and v.co.z < BROW_Z + 1.5) or v.co.z < CHIN_Z + 4 + (0 if v.co.y > HEAD_CY + 3 else 6)]
    bmesh.ops.delete(bm, geom=kill, context='VERTS')
    bm.to_mesh(o.data); bm.free()
    solidify(o, 0.8)
    return o


def beard(name, mat, length=9.0, width=8.5, z0=None):
    """Beard hugging the measured jaw: from below the mouth down past the chin."""
    top = CHIN_Z + 6.0
    bot = CHIN_Z - length
    o = prim("sphere", name, mat, segments=18, ring_count=12, radius=1.0, location=(0, 0, 0))
    cy = FACE_Y + 5.0
    for v in o.data.vertices:
        x, y, z = v.co
        zz = bot + (top - bot) * (z + 1) / 2
        w = width * (0.55 + 0.45 * math.sqrt(max(0.0, (z + 1) / 2)))
        v.co = Vector((x * w, cy + y * 7.5, zz))
    bm = bmesh.new(); bm.from_mesh(o.data)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.y > cy + 2.0], context='VERTS')
    bm.to_mesh(o.data); bm.free()
    solidify(o, 0.5)
    return o


def pectoral_cross(name, mat, cord_mat, wood=True, z=None):
    chest = bh("spine_05")
    z = chest.z - 16 if z is None else z
    pr = profile(z, TORSO, band=3)
    front_r = pr[18]  # angle 270 deg = -Y
    y = CENTER_Y - front_r - 3.6
    parts = [prim("cube", name, mat, size=1, location=(0, y, z), scale=(1.4, 0.6, 8.0)),
             prim("cube", name + "b", mat, size=1, location=(0, y, z + 1.6), scale=(5.0, 0.6, 1.2)),
             prim("cube", name + "c", mat, size=1, location=(0, y, z + 3.2), scale=(2.6, 0.6, 0.8)),
             prim("cube", name + "d", mat, size=1, location=(0, y, z - 2.0), scale=(3.2, 0.6, 0.7))]
    # cord around the neck
    cord = prim("torus", name + "_cord", cord_mat, major_radius=12.0, minor_radius=0.35, major_segments=24, minor_segments=4,
                location=(0, CENTER_Y - 3, NECK_Z - 9), rotation=(1.05, 0, 0))
    return parts + [cord]


def hood(name, mat, deep=True, void_mat="Dark", drape=True):
    """Deep hood over the head, opening facing -Y, draping onto the shoulders."""
    h = HEAD
    parts = []
    o = prim("sphere", name, mat, segments=24, ring_count=14, radius=1.0)
    rx, ry, rz = HEAD_HALF_X + 4.5, (HEAD_BACK_Y - FACE_Y) / 2 + 5.5, (HEAD_TOP - CHIN_Z) / 2 + 5.0
    for v in o.data.vertices:
        x, y, z = v.co
        k = 1.0 - 0.25 * max(0.0, z) ** 2          # narrows towards a soft point
        v.co = Vector((x * rx * k, HEAD_CY - 1.0 + y * ry + 3.0 * max(0.0, z), HEAD_CZ + z * rz + 2.5 * max(0.0, z) ** 3))
    bm = bmesh.new(); bm.from_mesh(o.data)
    opening = lambda c: c.y < HEAD_CY - ry * 0.55 and abs(c.x) < HEAD_HALF_X * 0.85 and CHIN_Z - 3 < c.z < BROW_Z + 4
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if opening(f.calc_center_median())], context='FACES')
    bm.to_mesh(o.data); bm.free()
    solidify(o, 1.2)
    parts.append(o)
    if deep:
        # a dark plate just in front of the face fills the opening: the face is never seen
        v_ = prim("sphere", name + "_void", void_mat, segments=16, ring_count=10, radius=1.0)
        for v in v_.data.vertices:
            x, y, z = v.co
            v.co = Vector((x * HEAD_HALF_X * 0.95, FACE_Y + 1.5 + y * 3.0, (CHIN_Z + BROW_Z) / 2 + 1 + z * ((BROW_Z - CHIN_Z) / 2 + 3)))
        parts.append(v_)
    if drape:
        z = NECK_Z - 2
        pr = profile(z - 6, TORSO + [p for p, g in BODY_PTS if g.startswith("clavicle")], band=5)
        rs = [ring(z + 2, pr, add=4.0, scale=(0.75, 0.9)), ring(z - 8, pr, add=6.5), ring(z - 20, pr, add=7.5, scale=(1.05, 1.0))]
        cape = loft(rs, name + "_cape", mat)
        solidify(cape, 0.9)
        parts.append(cape)
    return parts


def headcloth(name, mat, drop=18):
    """Coptic headcloth / nun's apostolnik: covers head and neck, open at the face, falls to the chest."""
    h = HEAD
    o = prim("sphere", name, mat, segments=24, ring_count=14, radius=1.0)
    rx, ry, rz = HEAD_HALF_X + 2.2, (HEAD_BACK_Y - FACE_Y) / 2 + 2.0, (HEAD_TOP - CHIN_Z) / 2 + 2.5
    for v in o.data.vertices:
        x, y, z = v.co
        v.co = Vector((x * rx, HEAD_CY + 0.5 + y * ry, HEAD_CZ - 1.0 + z * rz))
    bm = bmesh.new(); bm.from_mesh(o.data)
    opening = lambda c: c.y < HEAD_CY - ry * 0.4 and abs(c.x) < HEAD_HALF_X * 0.72 and CHIN_Z - 1 < c.z < BROW_Z + 2
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if opening(f.calc_center_median())], context='FACES')
    bm.to_mesh(o.data); bm.free()
    solidify(o, 0.8)
    pr = profile(SHOULDER_TOP_Z - 3, TORSO + SHOULDER_PTS, band=4)
    neck = profile(NECK_Z, TORSO, band=3)
    rs = [ring(CHIN_Z - 1, neck, add=2.2), ring(SHOULDER_TOP_Z + 1.5, pr, add=2.5, scale=(0.85, 0.95)), ring(SHOULDER_TOP_Z - 8, pr, add=4.0), ring(SHOULDER_TOP_Z - drop, pr, add=4.5)]
    cape = loft(rs, name + "_cape", mat)
    solidify(cape, 0.8)
    return [o, cape]


def tunic(name, mat, hem_z=None, belt_mat=None):
    hem_z = KNEE_Z + 4 if hem_z is None else hem_z
    o = robe(name, mat, hem_z=hem_z, flare=5.0, ease=2.4, hip_ease=4.5, length_rings=10)
    parts = [o]
    if belt_mat:
        z = PELVIS_Z + 8
        pr = profile(z, TORSO, band=2.5)
        b = loft([ring(z - 1.5, pr, add=3.6), ring(z + 1.5, pr, add=3.6)], name + "_belt", belt_mat)
        solidify(b, 0.6)
        parts.append(b)
    return parts


# ------------------------------------------------------------------ export
def export_char(name, objs):
    for ob in bpy.context.selected_objects:
        ob.select_set(False)
    ARM.select_set(True)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = ARM
    path = os.path.join(OUT, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False,
                             bake_anim=False, mesh_smooth_type='FACE', use_armature_deform_only=False, primary_bone_axis='Y', secondary_bone_axis='X')
    tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in objs)
    print("EXPORT %-18s parts=%d tris=%d" % (name, len(objs), tris))
    for o in objs:
        bpy.data.objects.remove(o, do_unlink=True)


def smooth_all(objs, angle=1.0):
    for o in objs:
        for ob in bpy.context.selected_objects:
            ob.select_set(False)
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
        try:
            bpy.ops.object.shade_smooth_by_angle(angle=angle)
        except Exception:
            bpy.ops.object.shade_smooth()


def finish_parts(spec):
    """spec: list of (obj or list, weight_fn). Returns flat list of skinned objects."""
    out = []
    for objs, fn in spec:
        if not isinstance(objs, list):
            objs = [objs]
        for o in objs:
            skin(o, fn)
            out.append(o)
    smooth_all(out)
    return out


def want(n):
    return not ONLY or n in ONLY


def priest_like(prefix, robe_mat, hem=6.0, flare=11.0, bell=13.0, train=0.0):
    r = robe(prefix + "_robe", robe_mat, hem_z=hem, flare=flare, train=train)
    c = collar(prefix + "_collar", robe_mat)
    sl = sleeve(prefix + "_sl_l", robe_mat, "l", bell=bell)
    sr = sleeve(prefix + "_sl_r", robe_mat, "r", bell=bell)
    return [(r, torso_skirt()), (c, nearest(["neck_01", "spine_05"], 2)), (sl, sleeve_weights("l")), (sr, sleeve_weights("r"))]


def torso_skirt():
    tw = torso_weights()
    sw = skirt_weights(PELVIS_Z - 2, KNEE_Z)
    def fn(co):
        if co.z > PELVIS_Z + 2:
            return tw(co)
        if co.z > PELVIS_Z - 6:
            t = (PELVIS_Z + 2 - co.z) / 8.0
            a, b = tw(co), sw(co)
            out = {k: v * (1 - t) for k, v in a.items()}
            tot_b = sum(b.values())
            for k, v in b.items():
                out[k] = out.get(k, 0) + v / tot_b * t * sum(a.values())
            return out
        return sw(co)
    return fn


# Father Alexios: black cassock with wide sleeves, kamilavka, short dark beard, wooden pectoral cross.
if want("SK_Alexios"):
    spec = priest_like("alx", "Cassock")
    spec.append((kamilavka("alx_hat", "Cassock"), rigid("head")))
    spec.append((hair_cap("alx_hair", "Hair", z_off=6.5, scale=(1.05, 1.1, 0.95), r=11.2), rigid("head")))
    spec.append((beard("alx_beard", "Hair", length=10.0, width=8.0), rigid("head")))
    spec.append((pectoral_cross("alx_cross", "CrossWood", "Cord"), nearest(["spine_05", "neck_01"], 2)))
    export_char("SK_Alexios", finish_parts(spec))

# Theodotos as the hooded man: dusty brown cloak, deep hood, face never shown.
if want("SK_Hooded"):
    spec = priest_like("hd", "Cloak", hem=3.0, flare=14.0, bell=9.0)
    spec.append((hood("hd_hood", "Cloak"), head_weights(NECK_Z + 4)))
    spec.append((beard("hd_beard", "BeardWhite", length=14.0, width=7.0), rigid("head")))
    export_char("SK_Hooded", finish_parts(spec))

# Thecla, keeper of the well: faded blue dress, dark headcloth.
if want("SK_Thecla"):
    r = robe("th_robe", "DressBlue", hem_z=7.0, flare=8.0)
    spec = [(r, torso_skirt()),
            (sleeve("th_sl_l", "DressBlue", "l", bell=3.0, cuff_back=4.0), sleeve_weights("l")),
            (sleeve("th_sl_r", "DressBlue", "r", bell=3.0, cuff_back=4.0), sleeve_weights("r")),
            (headcloth("th_head", "Headcloth"), head_weights(NECK_Z + 2))]
    export_char("SK_Thecla", finish_parts(spec))

# Besa the fisherman (also used for Hor and the boy Pakhom): knee tunic, short sleeves, rope belt, short hair and beard.
if want("SK_Besa"):
    spec = [(tunic("bs_tunic", "Tunic", belt_mat="Cord"), torso_skirt()),
            (sleeve("bs_sl_l", "Tunic", "l", short=True), sleeve_weights("l")),
            (sleeve("bs_sl_r", "Tunic", "r", short=True), sleeve_weights("r")),
            (hair_cap("bs_hair", "Hair", z_off=7.5, scale=(1.0, 1.05, 0.8), r=10.8), rigid("head")),
            (beard("bs_beard", "Hair", length=6.0, width=8.0), rigid("head"))]
    export_char("SK_Besa", finish_parts(spec))

if want("SK_Pakhom"):
    spec = [(tunic("pk_tunic", "Tunic"), torso_skirt()),
            (sleeve("pk_sl_l", "Tunic", "l", short=True), sleeve_weights("l")),
            (sleeve("pk_sl_r", "Tunic", "r", short=True), sleeve_weights("r")),
            (hair_cap("pk_hair", "HairWet", z_off=7.0, scale=(1.05, 1.1, 0.95), r=11.0), rigid("head"))]
    export_char("SK_Pakhom", finish_parts(spec))

# A bright one: tall figure in scorched white robes, hood deep and empty, sleeves past the hands.
if want("SK_BrightOne"):
    r = robe("br_robe", "BrightRobe", hem_z=-2.0, flare=16.0, train=4.0)
    spec = [(r, torso_skirt()),
            (collar("br_collar", "BrightRobe", h=5), nearest(["neck_01", "spine_05"], 2)),
            (sleeve("br_sl_l", "BrightRobe", "l", bell=8.0, cuff_back=-9.0), sleeve_weights("l")),
            (sleeve("br_sl_r", "BrightRobe", "r", bell=8.0, cuff_back=-9.0), sleeve_weights("r")),
            (hood("br_hood", "BrightRobe", void_mat="Void"), head_weights(NECK_Z + 4))]
    export_char("SK_BrightOne", finish_parts(spec))

# Sister Anastasia: black rason and apostolnik, wooden cross (from the concept art).
if want("SK_Anastasia"):
    spec = priest_like("an", "NunRobe", hem=5.0, flare=10.0, bell=6.0)
    spec.append((headcloth("an_head", "NunRobe", drop=34), head_weights(NECK_Z + 2, chest=True)))
    spec.append((pectoral_cross("an_cross", "CrossWood", "Cord", z=bh("spine_05").z - 24), nearest(["spine_05", "spine_04"], 2)))
    export_char("SK_Anastasia", finish_parts(spec))

# The Guest, true form (from the "Demon" concept): horns, membrane wings, glowing eyes, torn loincloth.
if want("SK_Guest"):
    parts = []
    for s in (-1, 1):
        horn = []
        base = Vector((HEAD.x + s * 5.5, HEAD.y + 1.0, HEAD.z + 15.0))
        for i in range(9):
            t = i / 8
            c = base + Vector((s * 6.0 * t, 3.0 * t * t, 12.0 * t - 4.0 * t * t * t))
            rr = 2.2 * (1 - t) + 0.2
            horn.append([c + Vector((rr * math.cos(2 * pi * j / 8), rr * math.sin(2 * pi * j / 8), 0)) for j in range(8)])
        parts.append((loft(horn, "gs_horn%d" % s, "Horn", close_top=True), rigid("head")))
        eye = prim("sphere", "gs_eye%d" % s, "DemonEye", segments=8, ring_count=6, radius=1.7, location=(s * 3.5, FACE_Y - 0.6, BROW_Z - 3.0))   # just in front of the mannequin's visor face
        parts.append((eye, rigid("head")))
        # wing: a fan of membrane panels from the upper back
        root = bh("spine_05") + Vector((s * 6.0, 10.0, 0))
        tips = [root + Vector((s * 16, 20, 42)), root + Vector((s * 34, 26, 18)), root + Vector((s * 40, 30, -28)), root + Vector((s * 30, 26, -80)), root + Vector((s * 12, 18, -108))]
        verts = [tuple(root)] + [tuple(t) for t in tips]
        mids = []
        for a, b in zip(tips[:-1], tips[1:]):
            m = (a + b) / 2 + (root - (a + b) / 2) * 0.28
            mids.append(tuple(m))
        verts += mids
        faces = []
        for k in range(4):
            faces.append((0, 1 + k, 6 + k))
            faces.append((0, 6 + k, 2 + k))
        me = bpy.data.meshes.new("gs_wing%d" % s)
        me.from_pydata(verts, [], faces)
        me.update()
        w = bpy.data.objects.new("gs_wing%d" % s, me)
        bpy.context.collection.objects.link(w)
        set_mat(w, "Wing")
        solidify(w, 0.6)
        parts.append((w, rigid("spine_05")))
        for t in tips[:4]:
            bone_ = [[root.lerp(t, k / 6) + Vector((math.cos(2 * pi * j / 5), math.sin(2 * pi * j / 5), 0)) * (1.2 * (1 - k / 6) + 0.3) for j in range(5)] for k in range(7)]
            parts.append((loft(bone_, "gs_wb", "Horn"), rigid("spine_05")))
    lc = robe("gs_loin", "Loincloth", top_z=PELVIS_Z + 6, hem_z=KNEE_Z + 6, flare=3.0, ease=1.5, hip_ease=2.0, length_rings=6)
    bm = bmesh.new(); bm.from_mesh(lc.data)
    rr = random.Random(3)
    for v in bm.verts:
        if v.co.z < KNEE_Z + 14:
            v.co.z += rr.uniform(0, 10)
    bm.to_mesh(lc.data); bm.free()
    parts.append((lc, torso_skirt()))
    hair = hair_cap("gs_hair", "Hair", z_off=5.0, scale=(1.1, 1.2, 1.1), r=11.5)
    parts.append((hair, rigid("head")))
    export_char("SK_Guest", finish_parts(parts))

print("BUILD_CHARS DONE")
