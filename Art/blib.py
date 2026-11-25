# Shared Blender helpers for the Orthodox Vigil asset scripts.
# Units are metres. Every exported mesh has its origin at the centre of its base.
import bpy, bmesh, math, random, os
from mathutils import Vector, Matrix

RNG = random.Random(543)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def _active():
    return bpy.context.view_layer.objects.active


def mat_slot(obj, name):
    """Give every face of obj the material slot called name (materials are mapped in Unreal by slot name)."""
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    obj.data.materials.clear()
    obj.data.materials.append(m)
    for p in obj.data.polygons:
        p.material_index = 0
    return obj


def box(size, loc=(0, 0, 0), rot=(0, 0, 0), mat="Default", base=True):
    """Box of size (x, y, z). With base=True, loc is the centre of the bottom face."""
    bpy.ops.mesh.primitive_cube_add(size=1)
    o = _active()
    o.scale = size
    z = loc[2] + (size[2] / 2 if base else 0)
    o.location = (loc[0], loc[1], z)
    o.rotation_euler = rot
    apply(o)
    return mat_slot(o, mat)


def cyl(r, h, loc=(0, 0, 0), rot=(0, 0, 0), verts=16, mat="Default", r2=None, cap=True, base=True):
    if r2 is None:
        bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=h, end_fill_type='NGON' if cap else 'NOTHING')
    else:
        bpy.ops.mesh.primitive_cone_add(vertices=verts, radius1=r, radius2=r2, depth=h, end_fill_type='NGON' if cap else 'NOTHING')
    o = _active()
    if base:
        # base of the cylinder sits at loc, along its own (possibly rotated) axis
        o.data.transform(Matrix.Translation((0, 0, h / 2)))
    o.rotation_euler = rot
    o.location = loc
    apply(o)
    return mat_slot(o, mat)


def sphere(r, loc=(0, 0, 0), scale=(1, 1, 1), seg=16, rings=10, mat="Default"):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=rings, radius=r)
    o = _active()
    o.location = loc
    o.scale = scale
    apply(o)
    return mat_slot(o, mat)


def torus(R, r, loc=(0, 0, 0), rot=(0, 0, 0), mat="Default", maj=24, mino=8):
    bpy.ops.mesh.primitive_torus_add(major_radius=R, minor_radius=r, major_segments=maj, minor_segments=mino)
    o = _active()
    o.location = loc
    o.rotation_euler = rot
    apply(o)
    return mat_slot(o, mat)


def plane(sx, sy, loc=(0, 0, 0), rot=(0, 0, 0), mat="Default"):
    bpy.ops.mesh.primitive_plane_add(size=1)
    o = _active()
    o.scale = (sx, sy, 1)
    o.location = loc
    o.rotation_euler = rot
    apply(o)
    return mat_slot(o, mat)


def from_pydata(name, verts, faces, mat="Default"):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    o = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(o)
    return mat_slot(o, mat)


def apply(o):
    for ob in bpy.context.selected_objects:
        ob.select_set(False)
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return o


def join(objs, name):
    objs = [o for o in objs if o is not None]
    for ob in bpy.context.selected_objects:
        ob.select_set(False)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    o = _active()
    o.name = name
    o.data.name = name
    # merge duplicate material slots by name
    seen = {}
    remap = []
    mats = list(o.data.materials)
    for m in mats:
        if m.name not in seen:
            seen[m.name] = len(seen)
        remap.append(seen[m.name])
    # read every face's material first: clearing the slot list resets the indices to 0
    new_index = [remap[p.material_index] for p in o.data.polygons]
    o.data.materials.clear()
    for n in sorted(seen, key=lambda k: seen[k]):
        o.data.materials.append(bpy.data.materials[n])
    for p, k in zip(o.data.polygons, new_index):
        p.material_index = k
    return o


def bevel(o, width=0.02, segs=2, angle=0.6):
    m = o.modifiers.new("bevel", 'BEVEL')
    m.width = width
    m.segments = segs
    m.limit_method = 'ANGLE'
    m.angle_limit = angle
    apply_mods(o)
    return o


def subdiv(o, levels=1):
    m = o.modifiers.new("sub", 'SUBSURF')
    m.levels = levels
    m.render_levels = levels
    apply_mods(o)
    return o


def apply_mods(o):
    for ob in bpy.context.selected_objects:
        ob.select_set(False)
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    for m in list(o.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)


def jitter(o, amt=0.01, seed=0, zmin=None):
    r = random.Random(seed)
    for v in o.data.vertices:
        if zmin is not None and v.co.z < zmin:
            continue
        v.co += Vector((r.uniform(-amt, amt), r.uniform(-amt, amt), r.uniform(-amt, amt)))
    return o


def subdivide_edges(o, cuts=2):
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=cuts, use_grid_fill=True)
    bm.to_mesh(o.data)
    bm.free()
    return o


def taper(o, axis_z_top, factor):
    """Scale xy towards the top: at z=axis_z_top, xy are multiplied by factor."""
    for v in o.data.vertices:
        t = max(0.0, min(1.0, v.co.z / axis_z_top))
        s = 1.0 + (factor - 1.0) * t
        v.co.x *= s
        v.co.y *= s
    return o


def grime(o, dark_base=0.55, height=0.6, noise=0.12, seed=1, tint=(1, 1, 1)):
    """Vertex colour: darker towards the ground plus per-vertex variation. Unreal multiplies this into base colour."""
    me = o.data
    if "Col" in me.color_attributes:
        me.color_attributes.remove(me.color_attributes["Col"])
    ca = me.color_attributes.new("Col", 'BYTE_COLOR', 'CORNER')
    r = random.Random(seed)
    vcol = {}
    for v in me.vertices:
        t = max(0.0, min(1.0, v.co.z / height)) if height > 0 else 1.0
        k = dark_base + (1 - dark_base) * t
        k *= 1.0 - r.uniform(0, noise)
        vcol[v.index] = k
    for loop in me.loops:
        k = vcol[loop.vertex_index]
        ca.data[loop.index].color = (k * tint[0], k * tint[1], k * tint[2], 1.0)
    return o


def uv_box(o, scale=1.0):
    """Planar box mapping in metres, so textures keep the same density everywhere."""
    me = o.data
    if not me.uv_layers:
        me.uv_layers.new(name="UV")
    uv = me.uv_layers.active
    for p in me.polygons:
        n = p.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for li in p.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            if ax == 0:
                u, v = co.y, co.z
            elif ax == 1:
                u, v = co.x, co.z
            else:
                u, v = co.x, co.y
            uv.data[li].uv = (u * scale, v * scale)
    return o


def smooth(o, angle=0.6):
    for ob in bpy.context.selected_objects:
        ob.select_set(False)
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    try:
        bpy.ops.object.shade_smooth_by_angle(angle=angle)
    except Exception:
        bpy.ops.object.shade_smooth()
    return o


def origin_base(o, center_xy=True):
    """Move mesh data so the base centre (or the world origin in xy) is the object origin."""
    vs = [v.co for v in o.data.vertices]
    minz = min(v.z for v in vs)
    if center_xy:
        cx = (min(v.x for v in vs) + max(v.x for v in vs)) / 2
        cy = (min(v.y for v in vs) + max(v.y for v in vs)) / 2
    else:
        cx = cy = 0.0
    o.data.transform(Matrix.Translation((-cx, -cy, -minz)))
    o.location = (0, 0, 0)
    return o


def export(o, path):
    for ob in bpy.context.selected_objects:
        ob.select_set(False)
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={'MESH'}, mesh_smooth_type='FACE',
                             use_mesh_modifiers=True, add_leaf_bones=False, bake_anim=False, colors_type='SRGB')


def finish(o, out_dir, name=None, center_xy=True, uv_scale=0.5, vc=True, sm_angle=0.6, keep=False, keep_origin=False, **grime_kw):
    name = name or o.name
    o.name = name
    if not o.data.uv_layers:
        uv_box(o, uv_scale)
    if vc:
        grime(o, **grime_kw)
    smooth(o, sm_angle)
    if not keep_origin:
        origin_base(o, center_xy)
    export(o, os.path.join(out_dir, name + ".fbx"))
    tris = sum(len(p.vertices) - 2 for p in o.data.polygons)
    print("EXPORT %-26s tris=%6d dims=(%.2f, %.2f, %.2f)" % (name, tris, *o.dimensions))
    if not keep:
        bpy.data.objects.remove(o, do_unlink=True)


def ring_loft(rings, name, mat="Default", cap_bottom=False, cap_top=False):
    """rings: list of lists of Vector, same length each. Builds a quad strip skin through them."""
    n = len(rings[0])
    verts = [tuple(v) for ring in rings for v in ring]
    faces = []
    for i in range(len(rings) - 1):
        for j in range(n):
            a = i * n + j
            b = i * n + (j + 1) % n
            c = (i + 1) * n + (j + 1) % n
            d = (i + 1) * n + j
            faces.append((a, b, c, d))
    if cap_bottom:
        faces.append(tuple(reversed(range(n))))
    if cap_top:
        faces.append(tuple((len(rings) - 1) * n + j for j in range(n)))
    return from_pydata(name, verts, faces, mat)
