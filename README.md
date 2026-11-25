# Orthodox Vigil: vertical slice (UE 5.8 + Blender 4.5)

The first night in Tarsenis, from "Orthodox Vigil: Story Script" (8 Nov 2026): a condensed Prologue (P2, P3),
all of Act I (I-1 to I-7), and the opening lines of Act II (II-1, Anastasia at the convent gate).
About 25 to 35 minutes to play.

## Play

Packaged build: `Build/Windows/OrthodoxVigil.exe` (keep the whole `Build/Windows` folder together, about 0.9 GB).
Rebuild it with `RunUAT BuildCookRun -project=... -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory=Build`.

From the project: open `OrthodoxVigil.uproject` in UE 5.8 and press Play, or run:

`UnrealEditor.exe OrthodoxVigil.uproject -game -windowed -ResX=1600 -ResY=900`

| Action | Keyboard and mouse | Gamepad |
|---|---|---|
| Walk / hurry | WASD or arrows / hold Shift | Left stick / L3 |
| Use, talk, advance text | E, Space or left click | A |
| Prayer rope (hold), complete a knot | Hold Q, press Space as the ring meets the knots | Hold LT, press A |
| Raise the cross (hold) | F or right mouse | RT |
| Holy Water | G | X |
| Journal and inventory | J or Tab | Y |
| Pause | Esc | Start |

## What is in it

- **Prologue.** The Patriarch's commission over black, with portraits cut from the concept art; Ioannes's letter as a
  document; the four objects. Then the last evening on the river with Hor, the voices in the reeds, and the Composure
  tutorial with the prayer rope.
- **I-1 Empty streets.** Ten sealed mud-brick houses, the closed-eye sign in ash on every lintel, half loaves on the
  thresholds, journal entries, the hooded man with the crook-handled lamp (you can choose to look at the lamp: `noticed_crook_lamp`).
- **I-2 The well.** The reversed Trisagion, the reflection with the bright figure behind it, turn around, look back,
  Holy Water, the second light and the scrape. Thecla runs out; Pakhom watches with wet hair; she sends you to Besa.
- **I-3 The locked church.** Chained doors, the view through the lattice of icons turned to the wall, the empty frame.
- **I-4 Besa.** The knife, the cross tutorial, his account of Ioannes, the Shepherd and Petbe, and the question menu.
- **I-5 The bone house.** Darkness with only the lamp, the bright one ("Little priest. You came the long way round."),
  the crucifix standoff with the shrinking flame ring, backing away into the side passage. Failure is not death: if it
  reaches you it passes through and you try again. The scratched Kyrie eleison wall, Ioannes's stole and key, the
  semantron under the bones, the lamp oil, the trapdoor up into the defiled church, and the choice to pray or sleep (the dream).
- **I-6 The cleansing.** Day 2: gather incense (sacristy chest, key), lamp oil, candles (on the step), the Psalter.
  At sunset the seven steps in order, with all four interruptions from the script: the wind on the candles (shield
  with prayer or relight), Ioannes's false voice from the trapdoor (going to it breaks the step, `heeded_false_voice`),
  the Theotokos turning back to the wall (Holy Water), and the bright one at the lattice (looking drains Composure).
  Then the semantron: twelve strikes, the lamp that comes on and goes out, the answering lamp across the canal.
- **I-7.** Ioannes's journal from behind the Archangel's board, the lamp passing the windows twice, the end of Act I.
- **II-1 (opening).** "They sent a boy." Then the Guest's true form for one frame, and the end card.

Composure is never shown as a number: the screen edges close in, a heartbeat rises, and the prayer rope in the corner
trembles. At zero Alexios falters and must pray. The journal keeps every entry and item.

## Characters and concept art

Faces like the concept art cannot be built from script, so every figure is **Epic's UE5 mannequin** (as you asked)
wearing clothes modelled in Blender to the concept sheets and skinned to Epic's skeleton:
Alexios (cassock, kamilavka, beard, wooden cross), Anastasia (rason, apostolnik, cross), the hooded Theodotos (face
never shown), Thecla, Besa (also used for Hor), Pakhom, the bright ones (scorched white robes, light that falls on
nothing), and the Guest from the "Demon" sheet (horns, wings, glowing eyes). The Patriarch appears as a portrait.
The cover art is the title screen. Portraits for Alexios, Anastasia and the Patriarch are cropped from the concept art.

## How it is built (everything regenerates from scripts)

| Step | Command |
|---|---|
| Environment meshes (80) | `blender -b --factory-startup --python Art/build_env.py` |
| Character clothing (8) on the mannequin skeleton | `blender -b --factory-startup --python Art/build_chars.py` |
| Textures and portrait crops | `blender -b --factory-startup --python Art/build_textures.py` |
| Icons, scratched wall, silhouette portraits | `blender -b --factory-startup --python Art/render_icons.py` |
| Sounds (34, synthesised) | `blender -b --factory-startup --python Art/build_audio.py` |
| Import into Unreal, materials, map | `powershell Tools/run_editor.ps1 -Script Tools/import_assets.py` |
| Gameplay | C++ in `Source/OrthodoxVigil` (build with Build.bat or the editor) |

- `VigilWorld.cpp` places the whole level from code (metres, +X east, +Y south).
- `VigilStory.cpp` is the script: every scene, line and choice. Lines marked `ADDED` are mine, not the script's.
- `VigilGameMode.cpp` holds the story queue, Composure, prayer rope, cross, Holy Water, audio and menus.
- `VigilAnim.cpp` plays Epic's idle, walk and jog by speed (stride speed measured from the clips) and bends arm bones
  for the cross, the rope, sprinkling and censing. There is no Animation Blueprint.
- `VigilAuto.cpp` is a development robot: `-game -VigilAutoPlay` plays the whole slice and logs `VIGILAUTO` lines with
  screenshots in `Saved/Screenshots`. `-VigilJump=ossuary|chamber|day2|rite|convent|guest|village -VigilShotAt=8`
  jumps to a chapter and takes one screenshot.

## Placeholders and known gaps

- **Lines I wrote** (marked `ADDED` in `VigilStory.cpp`): Besa's answers about the bread, the bright ones, Ioannes and
  the tombs (the script lists the questions but not the answers), and how the church doors come open on day 2.
- The ritual camera is a close isometric camera, not the first-person view the script describes.
- Hor, Kosmas, Thecla, Besa and the hooded man have silhouette portraits rendered from their models.
- The mannequin's face is featureless. Real character models (or MetaHumans) would replace the body, keeping the skeleton.
- Sounds are synthesised stand-ins; the chant and the voices want real recordings.
- **Font licence:** dialogue uses Monotype Garamond copied from Windows. Fine for a private build, not for distribution.
  Replace `Art/Fonts/*.ttf` with an open font (for example EB Garamond) before sharing a build.
- Epic's mannequin and animations are licensed for use in Unreal projects.
