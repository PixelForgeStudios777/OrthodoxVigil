# UE Python: export Epic's mannequin (mesh + skeleton) to FBX for Blender to bind clothes to.
import os, unreal
PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(PROJECT, "Art", "mannequin")
os.makedirs(OUT, exist_ok=True)
for name in ("SKM_Manny_Simple",):
    mesh = unreal.EditorAssetLibrary.load_asset("/Game/Characters/Mannequins/Meshes/" + name)
    unreal.log("VIGILEXPORT %s loaded=%s skeleton=%s" % (name, mesh is not None, mesh.skeleton.get_path_name() if mesh else None))
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = os.path.join(OUT, name + ".fbx")
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    opts = unreal.FbxExportOption()
    opts.ascii = False
    opts.export_morph_targets = False
    opts.level_of_detail = False
    opts.collision = False
    task.options = opts
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log("VIGILEXPORT %s ok=%s" % (name, ok))
unreal.SystemLibrary.quit_editor()
