# Renders front and side previews of each character (mannequin body + clothes) to Art/previews.
import bpy, os, sys, math
from mathutils import Vector
HERE = os.path.dirname(os.path.abspath(__file__))
OUTD = os.path.join(HERE, "previews"); os.makedirs(OUTD, exist_ok=True)
COL = {"Cassock": (0.03, 0.03, 0.035), "Hair": (0.05, 0.035, 0.025), "CrossWood": (0.35, 0.18, 0.08), "Cord": (0.25, 0.15, 0.08),
       "Cloak": (0.22, 0.17, 0.12), "Dark": (0, 0, 0), "BeardWhite": (0.8, 0.8, 0.78), "DressBlue": (0.25, 0.35, 0.5), "Headcloth": (0.08, 0.07, 0.07),
       "Tunic": (0.62, 0.56, 0.45), "HairWet": (0.02, 0.02, 0.02), "BrightRobe": (0.95, 0.93, 0.88), "Void": (0, 0, 0), "NunRobe": (0.04, 0.04, 0.045),
       "Horn": (0.3, 0.27, 0.22), "DemonEye": (1, 0.2, 0.05), "Wing": (0.12, 0.1, 0.1), "Loincloth": (0.25, 0.22, 0.16)}
names = sys.argv[sys.argv.index("--") + 1:]
for name in names:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.scale_length = 0.01
    bpy.ops.import_scene.fbx(filepath=os.path.join(HERE, "mannequin", "SKM_Manny_Simple.fbx"))
    for o in bpy.data.objects:
        if o.type == 'MESH':
            o.data.materials.clear()
            m = bpy.data.materials.new("skin"); m.diffuse_color = (0.45, 0.3, 0.22, 1) if name != "SK_Guest" else (0.07, 0.065, 0.06, 1)
            o.data.materials.append(m)
            if name == "SK_BrightOne":
                o.hide_render = True
    bpy.ops.import_scene.fbx(filepath=os.path.join(HERE, "FBX", "Chars", name + ".fbx"))
    for m in bpy.data.materials:
        base = m.name.split(".")[0]
        if base in COL:
            m.diffuse_color = (*COL[base], 1)
    cam = bpy.data.cameras.new("c"); cam.type = 'ORTHO'; cam.ortho_scale = 230
    co = bpy.data.objects.new("c", cam); sc.collection.objects.link(co); sc.camera = co
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.display.shading.light = 'STUDIO'; sc.display.shading.color_type = 'MATERIAL'
    sc.display.shading.show_shadows = False; sc.display.shading.show_cavity = True
    sc.render.resolution_x = 640; sc.render.resolution_y = 720
    sc.world = bpy.data.worlds.new("w"); sc.display.shading.background_type = 'VIEWPORT'
    for tag, loc, rot in (("front", (0, -500, 100), (math.pi / 2, 0, 0)), ("side", (500, 0, 100), (math.pi / 2, 0, math.pi / 2))):
        co.location = loc; co.rotation_euler = rot
        sc.render.filepath = os.path.join(OUTD, "%s_%s.png" % (name, tag))
        bpy.ops.render.render(write_still=True)
    print("PREVIEW", name)
