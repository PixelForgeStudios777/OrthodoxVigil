# UE Python: imports everything Blender made, builds the material library and the empty map.
# Run through Tools/run_editor.ps1 (needs a rendering editor session for skeletal meshes).
import os, unreal

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(PROJECT, "Art")
at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
ROOT = "/Game/Vigil"

for cmd in ("Interchange.FeatureFlags.Import.FBX 0", "Interchange.FeatureFlags.Import.FBX.ToLevel 0"):
    unreal.SystemLibrary.execute_console_command(None, cmd)


def log(m):
    unreal.log("VIGILIMPORT " + str(m))


def import_files(files, dest, options_fn=None):
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.filename = f
        t.destination_path = dest
        t.destination_name = os.path.splitext(os.path.basename(f))[0]
        t.automated = True
        t.replace_existing = True
        t.save = True
        if options_fn:
            t.options = options_fn(f)
        tasks.append(t)
    at.import_asset_tasks(tasks)
    ok = [t for t in tasks if t.imported_object_paths]
    log("imported %d/%d into %s" % (len(ok), len(tasks), dest))
    return tasks


def listdir(d, ext):
    return sorted(os.path.join(d, f) for f in os.listdir(d) if f.lower().endswith(ext))


# ------------------------------------------------------------------ textures
NORMALS = set()
tex_tasks = import_files(listdir(os.path.join(ART, "Textures"), ".png"), ROOT + "/Textures")
for f in listdir(os.path.join(ART, "Textures"), ".png"):
    n = os.path.splitext(os.path.basename(f))[0]
    tex = eal.load_asset(ROOT + "/Textures/" + n)
    if not tex:
        log("MISSING texture " + n); continue
    if n.endswith("_N"):
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", True)
    if n.startswith(("T_Portrait", "T_Cover", "T_Ring", "T_Glow", "T_Vignette", "T_Parchment")):
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("never_stream", True)
    eal.save_loaded_asset(tex)


def T(n):
    return eal.load_asset(ROOT + "/Textures/" + n)


# ------------------------------------------------------------------ materials
MAT = ROOT + "/Materials"


def new_asset(name, path, cls, factory):
    full = path + "/" + name
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    return at.create_asset(name, path, cls, factory)


def expr(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def vparam(m, name, d, x, y):
    return expr(m, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*d))


def sparam(m, name, d, x, y):
    return expr(m, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=d)


def mul(m, a, b, x, y, ao="", bo=""):
    e = expr(m, unreal.MaterialExpressionMultiply, x, y)
    mel.connect_material_expressions(a, ao, e, "A"); mel.connect_material_expressions(b, bo, e, "B")
    return e


def add(m, a, b, x, y):
    e = expr(m, unreal.MaterialExpressionAdd, x, y)
    mel.connect_material_expressions(a, "", e, "A"); mel.connect_material_expressions(b, "", e, "B")
    return e


def build_surface():
    """M_Surface: base = (UseTex ? Tex : 1) * Tint * VertexColor; normal map optional; two sided."""
    m = new_asset("M_Surface", MAT, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("two_sided", True)
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -1600, 0)
    tiling = sparam(m, "Tiling", 1.0, -1600, 120)
    uvt = mul(m, uv, tiling, -1400, 40)
    tex = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -1200, -100, parameter_name="BaseTex", texture=T("T_Plaster_D"))
    mel.connect_material_expressions(uvt, "", tex, "UVs")
    one = expr(m, unreal.MaterialExpressionConstant3Vector, -1200, 150, constant=unreal.LinearColor(1, 1, 1, 1))
    sw = expr(m, unreal.MaterialExpressionStaticSwitchParameter, -950, 0, parameter_name="UseTex", default_value=False)
    mel.connect_material_expressions(tex, "RGB", sw, "True"); mel.connect_material_expressions(one, "", sw, "False")
    tint = vparam(m, "Tint", (1, 1, 1, 1), -950, 200)
    vc = expr(m, unreal.MaterialExpressionVertexColor, -950, 350)
    vcamt = sparam(m, "VertexAO", 1.0, -950, 480)
    vcl = expr(m, unreal.MaterialExpressionLinearInterpolate, -750, 380)
    one1 = expr(m, unreal.MaterialExpressionConstant, -950, 560, r=1.0)
    mel.connect_material_expressions(one1, "", vcl, "A"); mel.connect_material_expressions(vc, "", vcl, "B"); mel.connect_material_expressions(vcamt, "", vcl, "Alpha")
    b1 = mul(m, sw, tint, -700, 100)
    b2 = mul(m, b1, vcl, -500, 150)
    mel.connect_material_property(b2, "", unreal.MaterialProperty.MP_BASE_COLOR)
    nt = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -1200, 700, parameter_name="NormalTex",
              texture=eal.load_asset("/Engine/EngineMaterials/DefaultNormal"), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_expressions(uvt, "", nt, "UVs")
    flat = expr(m, unreal.MaterialExpressionConstant3Vector, -1200, 900, constant=unreal.LinearColor(0, 0, 1, 1))
    ns = sparam(m, "NormalStrength", 1.0, -1000, 950)
    nl = expr(m, unreal.MaterialExpressionLinearInterpolate, -800, 800)
    mel.connect_material_expressions(flat, "", nl, "A"); mel.connect_material_expressions(nt, "RGB", nl, "B"); mel.connect_material_expressions(ns, "", nl, "Alpha")
    mel.connect_material_property(nl, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(sparam(m, "Roughness", 0.85, -500, 400), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(sparam(m, "Metallic", 0.0, -500, 480), "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(sparam(m, "Specular", 0.4, -500, 560), "", unreal.MaterialProperty.MP_SPECULAR)
    em = vparam(m, "Emissive", (0, 0, 0, 1), -500, 640)
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(m); eal.save_loaded_asset(m)
    return m


def build_unlit(name, blend=unreal.BlendMode.BLEND_OPAQUE, color=(1, 1, 1, 1), tex=None):
    m = new_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", blend)
    m.set_editor_property("two_sided", True)
    c = vparam(m, "Color", color, -600, 0)
    s = sparam(m, "Strength", 1.0, -600, 200)
    out = mul(m, c, s, -350, 50)
    if tex:
        t = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, 300, parameter_name="Tex", texture=tex)
        out = mul(m, out, t, -200, 150, bo="RGB")
        mel.connect_material_property(t, "A", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    m.set_editor_property("used_with_skeletal_mesh", True)
    mel.recompile_material(m); eal.save_loaded_asset(m)
    return m


def build_bright():
    """The bright ones: white robes that glow from inside, scorched towards the hem. Unlit, so they light nothing."""
    m = new_asset("M_Bright", MAT, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -1400, 0)
    op = expr(m, unreal.MaterialExpressionActorPositionWS, -1400, 150)
    sub = expr(m, unreal.MaterialExpressionSubtract, -1200, 50)
    mel.connect_material_expressions(wp, "", sub, "A"); mel.connect_material_expressions(op, "", sub, "B")
    mask = expr(m, unreal.MaterialExpressionComponentMask, -1000, 50, r=False, g=False, b=True, a=False)
    mel.connect_material_expressions(sub, "", mask, "")
    h = sparam(m, "ScorchHeight", 160.0, -1000, 200)
    div = expr(m, unreal.MaterialExpressionDivide, -850, 100)
    mel.connect_material_expressions(mask, "", div, "A"); mel.connect_material_expressions(h, "", div, "B")
    sat = expr(m, unreal.MaterialExpressionSaturate, -700, 100)
    mel.connect_material_expressions(div, "", sat, "")
    lo = vparam(m, "Scorch", (0.06, 0.045, 0.03, 1), -700, 250)
    hi = vparam(m, "Color", (0.95, 0.92, 0.84, 1), -700, 400)
    lr = expr(m, unreal.MaterialExpressionLinearInterpolate, -500, 250)
    mel.connect_material_expressions(lo, "", lr, "A"); mel.connect_material_expressions(hi, "", lr, "B"); mel.connect_material_expressions(sat, "", lr, "Alpha")
    fr = expr(m, unreal.MaterialExpressionFresnel, -700, 550)
    k = add(m, fr, expr(m, unreal.MaterialExpressionConstant, -700, 650, r=0.55), -500, 550)
    s = sparam(m, "Strength", 1.6, -500, 700)
    out = mul(m, mul(m, lr, k, -300, 350), s, -150, 400)
    mel.connect_material_property(out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    m.set_editor_property("used_with_skeletal_mesh", True)
    mel.recompile_material(m); eal.save_loaded_asset(m)


def build_decal():
    """Masked painted marks: ash signs and scratched letters."""
    m = new_asset("M_Mark", MAT, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", True)
    t = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -800, 0, parameter_name="Tex", texture=T("T_AshSign"))
    tint = vparam(m, "Tint", (1, 1, 1, 1), -800, 250)
    mel.connect_material_property(mul(m, t, tint, -500, 100, ao="RGB"), "", unreal.MaterialProperty.MP_BASE_COLOR)
    amt = sparam(m, "AlphaFrom", 0.0, -800, 400)  # 0 = use alpha, 1 = use red (white-on-black textures)
    lr = expr(m, unreal.MaterialExpressionLinearInterpolate, -500, 350)
    mel.connect_material_expressions(t, "A", lr, "A"); mel.connect_material_expressions(t, "R", lr, "B"); mel.connect_material_expressions(amt, "", lr, "Alpha")
    mel.connect_material_property(lr, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.connect_material_property(sparam(m, "Roughness", 0.95, -500, 500), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m); eal.save_loaded_asset(m)


def build_water():
    m = new_asset("M_Water", MAT, unreal.Material, unreal.MaterialFactoryNew())
    pan = expr(m, unreal.MaterialExpressionPanner, -900, 0, speed_x=0.01, speed_y=0.02)
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -1100, 0, u_tiling=6.0, v_tiling=40.0)
    mel.connect_material_expressions(uv, "", pan, "Coordinate")
    n = expr(m, unreal.MaterialExpressionTextureSample, -700, 0, texture=T("T_Ground_N"), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_expressions(pan, "", n, "UVs")
    flat = expr(m, unreal.MaterialExpressionConstant3Vector, -700, 250, constant=unreal.LinearColor(0, 0, 1, 1))
    lr = expr(m, unreal.MaterialExpressionLinearInterpolate, -450, 100)
    mel.connect_material_expressions(flat, "", lr, "A"); mel.connect_material_expressions(n, "RGB", lr, "B")
    mel.connect_material_expressions(expr(m, unreal.MaterialExpressionConstant, -700, 350, r=0.25), "", lr, "Alpha")
    mel.connect_material_property(lr, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(vparam(m, "Color", (0.012, 0.016, 0.014, 1), -450, -150), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(expr(m, unreal.MaterialExpressionConstant, -450, 300, r=0.06), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(expr(m, unreal.MaterialExpressionConstant, -450, 380, r=0.9), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m); eal.save_loaded_asset(m)


def build_cutaway():
    """Walls between the camera and the player underground: only the lowest metre is drawn."""
    m = new_asset("M_Cutaway", MAT, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", True)
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    t = expr(m, unreal.MaterialExpressionTextureSample, -1000, 0, texture=T("T_RockWall_D"))
    mel.connect_material_expressions(uv, "", t, "UVs")
    vc = expr(m, unreal.MaterialExpressionVertexColor, -1000, 250)
    mel.connect_material_property(mul(m, t, vc, -700, 100, ao="RGB"), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(expr(m, unreal.MaterialExpressionConstant, -700, 300, r=0.9), "", unreal.MaterialProperty.MP_ROUGHNESS)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -1400, 500)
    op = expr(m, unreal.MaterialExpressionActorPositionWS, -1400, 650)
    sub = expr(m, unreal.MaterialExpressionSubtract, -1200, 550)
    mel.connect_material_expressions(wp, "", sub, "A"); mel.connect_material_expressions(op, "", sub, "B")
    mask = expr(m, unreal.MaterialExpressionComponentMask, -1000, 550, r=False, g=False, b=True, a=False)
    mel.connect_material_expressions(sub, "", mask, "")
    cut = sparam(m, "Cut", 95.0, -1000, 700)
    d = expr(m, unreal.MaterialExpressionSubtract, -800, 600)
    mel.connect_material_expressions(cut, "", d, "A"); mel.connect_material_expressions(mask, "", d, "B")
    mel.connect_material_property(d, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.recompile_material(m); eal.save_loaded_asset(m)


surface = build_surface()
build_cutaway()
build_unlit("M_Emissive", color=(6, 3, 1, 1))
build_bright()
build_decal()
build_water()
log("master materials built")


def mi(name, parent, tex=None, normal=None, tint=(1, 1, 1, 1), rough=0.85, metal=0.0, spec=0.4, emissive=None, tiling=1.0, vao=1.0, nstr=1.0, extra=None):
    inst = new_asset(name, MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(inst, eal.load_asset(MAT + "/" + parent))
    if parent == "M_Surface":
        if tex:
            mel.set_material_instance_static_switch_parameter_value(inst, "UseTex", True)
            mel.set_material_instance_texture_parameter_value(inst, "BaseTex", T(tex))
        if normal:
            mel.set_material_instance_texture_parameter_value(inst, "NormalTex", T(normal))
            mel.set_material_instance_scalar_parameter_value(inst, "NormalStrength", nstr)
        else:
            mel.set_material_instance_scalar_parameter_value(inst, "NormalStrength", 0.0)
        mel.set_material_instance_vector_parameter_value(inst, "Tint", unreal.LinearColor(*tint))
        mel.set_material_instance_scalar_parameter_value(inst, "Roughness", rough)
        mel.set_material_instance_scalar_parameter_value(inst, "Metallic", metal)
        mel.set_material_instance_scalar_parameter_value(inst, "Specular", spec)
        mel.set_material_instance_scalar_parameter_value(inst, "Tiling", tiling)
        mel.set_material_instance_scalar_parameter_value(inst, "VertexAO", vao)
        if emissive:
            mel.set_material_instance_vector_parameter_value(inst, "Emissive", unreal.LinearColor(*emissive))
    for k, v in (extra or {}).items():
        if isinstance(v, tuple):
            mel.set_material_instance_vector_parameter_value(inst, k, unreal.LinearColor(*v))
        elif isinstance(v, str):
            mel.set_material_instance_texture_parameter_value(inst, k, T(v))
        else:
            mel.set_material_instance_scalar_parameter_value(inst, k, v)
    eal.save_loaded_asset(inst)
    return inst


S = "M_Surface"
LIB = {
    # environment (textured)
    "Ground": dict(tex="T_Ground_D", normal="T_Ground_N", rough=0.95, spec=0.3),
    "MudBrick": dict(tex="T_MudBrick_D", normal="T_MudBrick_N", rough=0.92),
    "Plaster": dict(tex="T_Plaster_D", normal="T_Plaster_N", rough=0.9, tint=(0.95, 0.93, 0.88, 1)),
    "Stone": dict(tex="T_Stone_D", normal="T_Stone_N", rough=0.85),
    "RockWall": dict(tex="T_RockWall_D", normal="T_RockWall_N", rough=0.9, nstr=1.0),
    "Rock": dict(tex="T_RockWall_D", normal="T_RockWall_N", rough=0.9, tiling=0.25, tint=(0.8, 0.72, 0.6, 1)),
    "Wood": dict(tex="T_Wood_D", normal="T_Wood_N", rough=0.75),
    "DarkWood": dict(tex="T_Wood_D", normal="T_Wood_N", rough=0.7, tint=(0.45, 0.42, 0.4, 1)),
    "PalmTrunk": dict(tex="T_Thatch_D", normal="T_Thatch_N", rough=0.9, tint=(0.75, 0.62, 0.5, 1), tiling=0.5),
    "Thatch": dict(tex="T_Thatch_D", normal="T_Thatch_N", rough=0.95),
    "Reed": dict(tex="T_Thatch_D", rough=0.9, tint=(0.95, 0.9, 0.7, 1)),
    "RoofTile": dict(tex="T_Stone_D", normal="T_Stone_N", rough=0.8, tint=(0.62, 0.42, 0.32, 1)),
    "ClosedEyeCloth": dict(tex="T_AltarCloth", rough=0.9, vao=0.0),
    # flat colours
    "Dark": dict(tint=(0.004, 0.004, 0.004, 1), rough=1.0, spec=0.0, vao=0.0),
    "Default": dict(tint=(0.5, 0.5, 0.5, 1)),
    "Net": dict(tint=(0.3, 0.26, 0.19, 1), rough=0.95),
    "Palm": dict(tint=(0.1, 0.14, 0.06, 1), rough=0.7, vao=0.6),
    "Cloth": dict(tint=(0.62, 0.58, 0.5, 1), rough=0.95, vao=0.0),
    "ClothBlue": dict(tint=(0.18, 0.27, 0.4, 1), rough=0.95, vao=0.0),
    "ClothRed": dict(tint=(0.4, 0.12, 0.08, 1), rough=0.95, vao=0.0),
    "Rope": dict(tint=(0.36, 0.28, 0.17, 1), rough=0.95, vao=0.0),
    "Cord": dict(tint=(0.22, 0.14, 0.08, 1), rough=0.9, vao=0.0),
    "Bronze": dict(tint=(0.55, 0.37, 0.17, 1), rough=0.42, metal=1.0, vao=0.0),
    "Gold": dict(tint=(0.85, 0.62, 0.25, 1), rough=0.32, metal=1.0, vao=0.0),
    "Iron": dict(tint=(0.12, 0.115, 0.11, 1), rough=0.6, metal=0.8, vao=0.0),
    "Rust": dict(tint=(0.36, 0.17, 0.07, 1), rough=0.8, metal=0.3),
    "Clay": dict(tint=(0.5, 0.29, 0.16, 1), rough=0.85),
    "Bread": dict(tint=(0.6, 0.4, 0.2, 1), rough=0.9),
    "Bone": dict(tint=(0.66, 0.6, 0.48, 1), rough=0.75),
    "Ash": dict(tint=(0.1, 0.095, 0.09, 1), rough=1.0, vao=0.0),
    "Sand": dict(tint=(0.6, 0.52, 0.38, 1), rough=1.0, vao=0.0),
    "Candle": dict(tint=(0.88, 0.82, 0.66, 1), rough=0.5, vao=0.0, emissive=(0.02, 0.012, 0.004, 1)),
    "Wax": dict(tint=(0.45, 0.06, 0.04, 1), rough=0.5, vao=0.0),
    "Parchment": dict(tint=(0.75, 0.68, 0.5, 1), rough=0.9, vao=0.0),
    "Leather": dict(tint=(0.22, 0.12, 0.06, 1), rough=0.7, vao=0.0),
    "Sacking": dict(tint=(0.42, 0.35, 0.24, 1), rough=1.0),
    "StoleRed": dict(tint=(0.38, 0.05, 0.04, 1), rough=0.8, vao=0.0),
    "Goat": dict(tint=(0.32, 0.27, 0.22, 1), rough=1.0),
    "Sail": dict(tint=(0.68, 0.62, 0.52, 1), rough=0.95, vao=0.0),
    "GlassRed": dict(tint=(0.4, 0.04, 0.02, 1), rough=0.15, vao=0.0, emissive=(0.0, 0.0, 0.0, 1)),
    # characters
    "Skin": dict(tint=(0.4, 0.26, 0.17, 1), rough=0.6, vao=0.0),
    "DemonSkin": dict(tint=(0.045, 0.04, 0.036, 1), rough=0.55, vao=0.0, spec=0.6),
    "Cassock": dict(tint=(0.022, 0.022, 0.026, 1), rough=0.8, vao=0.0),
    "Hair": dict(tint=(0.03, 0.02, 0.014, 1), rough=0.6, vao=0.0),
    "HairWet": dict(tint=(0.01, 0.01, 0.01, 1), rough=0.15, vao=0.0, spec=0.8),
    "CrossWood": dict(tint=(0.3, 0.17, 0.08, 1), rough=0.6, vao=0.0),
    "Cloak": dict(tint=(0.17, 0.13, 0.09, 1), rough=0.95, vao=0.0),
    "BeardWhite": dict(tint=(0.7, 0.68, 0.64, 1), rough=0.8, vao=0.0),
    "DressBlue": dict(tint=(0.16, 0.24, 0.34, 1), rough=0.95, vao=0.0),
    "Headcloth": dict(tint=(0.05, 0.045, 0.045, 1), rough=0.95, vao=0.0),
    "Tunic": dict(tint=(0.55, 0.49, 0.38, 1), rough=0.95, vao=0.0),
    "NunRobe": dict(tint=(0.025, 0.025, 0.028, 1), rough=0.85, vao=0.0),
    "Horn": dict(tint=(0.22, 0.19, 0.15, 1), rough=0.5, vao=0.0),
    "Wing": dict(tint=(0.06, 0.05, 0.05, 1), rough=0.6, vao=0.0),
    "Loincloth": dict(tint=(0.2, 0.17, 0.12, 1), rough=1.0, vao=0.0),
}
for k, v in LIB.items():
    mi("MI_" + k, S, **v)
mi("MI_Flame", "M_Emissive", extra={"Color": (9.0, 4.2, 1.2, 1)})
mi("MI_DemonEye", "M_Emissive", extra={"Color": (40.0, 3.0, 0.4, 1)})
mi("MI_Void", "M_Emissive", extra={"Color": (0.0, 0.0, 0.0, 1)})
mi("MI_BrightRobe", "M_Bright", extra={"Scorch": (0.42, 0.37, 0.3, 1), "ScorchHeight": 90.0})   # scorched, never black: the robe must still read as white
mi("MI_AshSign", "M_Mark", extra={"Tex": "T_AshSign"})
mi("MI_Scratch", "M_Mark", extra={"Tex": "T_Scratch", "AlphaFrom": 1.0, "Tint": (0.55, 0.5, 0.42, 1)})
mi("MI_Water", "M_Water")
mi("MI_Cutaway", "M_Cutaway")
for n in ("Christ", "Theotokos", "Michael", "Forerunner"):
    mi("MI_Icon_" + n, S, tex="T_Icon_" + n, rough=0.45, vao=0.0)
ALIAS = {"IconFront": "MI_Icon_Christ", "AshSign": "MI_AshSign", "Scratch": "MI_Scratch", "Water": "MI_Water", "Flame": "MI_Flame",
         "DemonEye": "MI_DemonEye", "Void": "MI_Void", "BrightRobe": "MI_BrightRobe", "Cutaway": "MI_Cutaway"}
log("material instances built: %d" % (len(LIB) + 12))


def material_for(slot):
    slot = str(slot)
    base = slot.split(".")[0]
    name = ALIAS.get(base, "MI_" + base)
    m = eal.load_asset(MAT + "/" + name)
    if not m:
        log("WARN no material for slot %s" % slot)
        m = eal.load_asset(MAT + "/MI_Default")
    return m


# ------------------------------------------------------------------ static meshes
def sm_opts(f):
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    d = ui.static_mesh_import_data
    d.combine_meshes = True
    d.auto_generate_collision = False
    d.generate_lightmap_u_vs = False
    d.vertex_color_import_option = unreal.VertexColorImportOption.REPLACE
    d.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    return ui


env = listdir(os.path.join(ART, "FBX", "Env"), ".fbx")
import_files(env, ROOT + "/Meshes", sm_opts)
for f in env:
    n = os.path.splitext(os.path.basename(f))[0]
    sm = eal.load_asset(ROOT + "/Meshes/" + n)
    if not sm:
        log("MISSING mesh " + n); continue
    mats = sm.get_editor_property("static_materials")
    for i, smat in enumerate(mats):
        sm.set_material(i, material_for(smat.get_editor_property("material_slot_name")))
    bs = sm.get_editor_property("body_setup")
    if bs:
        bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(sm)
log("static meshes ready: %d" % len(env))

# ------------------------------------------------------------------ characters on Epic's skeleton
SKEL = eal.load_asset("/Game/Characters/Mannequins/Meshes/SK_Mannequin")


def sk_opts(f):
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = True
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    ui.skeleton = SKEL
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.create_physics_asset = False
    return ui


chars = listdir(os.path.join(ART, "FBX", "Chars"), ".fbx")
import_files(chars, ROOT + "/Characters", sk_opts)
for f in chars:
    n = os.path.splitext(os.path.basename(f))[0]
    sk = eal.load_asset(ROOT + "/Characters/" + n)
    if not sk:
        log("MISSING character " + n); continue
    mats = sk.get_editor_property("materials")
    for smat in mats:
        smat.set_editor_property("material_interface", material_for(smat.get_editor_property("material_slot_name")))
    sk.set_editor_property("materials", mats)
    eal.save_loaded_asset(sk)
    log("character %s skeleton=%s slots=%s" % (n, sk.skeleton.get_name() if sk.skeleton else None, [str(m.get_editor_property("material_slot_name")) for m in mats]))

# ------------------------------------------------------------------ sounds
LOOPS = ("amb_", "mus_", "chant_reverse", "whisper", "cross_hum", "heartbeat", "fire_loop")
wavs = listdir(os.path.join(ART, "Audio"), ".wav")
import_files(wavs, ROOT + "/Audio")
for f in wavs:
    n = os.path.splitext(os.path.basename(f))[0]
    s = eal.load_asset(ROOT + "/Audio/" + n)
    if s and n.startswith(LOOPS):
        s.set_editor_property("looping", True)
        eal.save_loaded_asset(s)

# ------------------------------------------------------------------ animation facts (for C++ speed matching)
for p in ("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle", "/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd",
          "/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd"):
    a = eal.load_asset(p)
    if a:
        log("ANIM %s length=%.3f rootmotion=%s" % (a.get_name(), a.get_play_length(), a.get_editor_property("enable_root_motion")))
bs = eal.load_asset("/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run")
if bs:
    try:
        for smp in bs.get_editor_property("sample_data"):
            log("BS sample %s at %s" % (smp.get_editor_property("animation").get_name(), smp.get_editor_property("sample_value")))
    except Exception as e:
        log("BS read failed %s" % e)

# ------------------------------------------------------------------ fonts (serif for dialogue and titles)
for ttf, fname in (("Garamond.ttf", "F_Garamond"), ("GaramondBold.ttf", "F_GaramondBold")):
    try:
        tasks = import_files([os.path.join(ART, "Fonts", ttf)], ROOT + "/Fonts")
        faces = [eal.load_asset(p) for t in tasks for p in t.imported_object_paths]
        face = next((f for f in faces if isinstance(f, unreal.FontFace)), None)
        if eal.does_asset_exist(ROOT + "/Fonts/" + fname):
            eal.delete_asset(ROOT + "/Fonts/" + fname)
        ff = unreal.FontFactory()
        if face:
            ff.set_editor_property("initial_font_faces", [face])
        font = at.create_asset(fname, ROOT + "/Fonts", unreal.Font, ff)
        font.set_editor_property("font_cache_type", unreal.FontCacheType.RUNTIME)
        eal.save_loaded_asset(font)
        log("font %s face=%s font=%s" % (fname, face, font))
    except Exception as e:
        log("font import failed %s: %s" % (ttf, e))

# ------------------------------------------------------------------ map
map_path = ROOT + "/Maps/Tarsenis"
if not eal.does_asset_exist(map_path):
    w = at.create_asset("Tarsenis", ROOT + "/Maps", unreal.World, unreal.WorldFactory())
    eal.save_loaded_asset(w)
log("map ready: %s" % eal.does_asset_exist(map_path))
log("DONE")
unreal.SystemLibrary.quit_editor()
