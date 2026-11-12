# OpenBor Visual Studio

A native Windows editor for OpenBOR character models and side-scrolling stages. Built with C++20, Dear ImGui, Direct3D 11, and Windows Imaging Component.

## Start here

1. Unzip the Windows package and launch **OpenBOR Studio.exe**. It starts with a blank project. Click **+ New model** or use an import button to start a character, or switch to **Stages** to create a stage.
2. Choose **Project → Open demo** to try the included **Training Ground** project. Press **Space** to play Goku’s attack animation. The cyan marker shows the projectile origin and the amber marker shows the item origin. Click **Stages** to open the canyon stage. Keep the **Demo** folder beside the executable to use this example.
3. Use **Project → Open project** to load an existing project. **Save** stores a **.borstudio** file and a sibling **.assets** directory. Keep them together when moving a project.
4. Use **Export** and select **Playable module** or **Content for an existing module**. Choose a destination. The dialog shows the full path of the new **OpenBOR_Export** directory and any errors that need attention.

Drag the dotted divider beside the right inspector to resize it. Double-click the divider to reset its width. When the tabs extend past the panel, use the arrows or the tab list to switch tabs.

## Open an existing game

- Use **Project → Open OpenBOR PAK** to unpack a PAK32 or PAK64 into a new working folder. Use **Open OpenBOR data folder** to open an unpacked module. Select either its **data** folder or the folder that contains it.
- Registered models with readable sprites and the stages listed in **levels.txt** appear in their workspaces. **Native TXT** opens the imported configuration, models, levels, and scripts directly. Apply a text edit to use that version of the file on export. To apply visual edits to that file again, revert its text override.
- Save a **.borstudio** project to keep a portable copy of the module and your edits. Export writes a separate data folder and can also create a new PAK. Audio edits update the relevant model or level commands in place. Broader visual edits can rebuild an affected model or level TXT from Studio's supported settings. The export README lists those files, and their original text stays available in the project.

## Characters

- **New model** creates a model with an Idle animation. Set its ID, type, health and speed under **Inspector → Entity**. This tab also provides MP, aggression, score, jump height, lifespan, collision defaults and the HUD portrait. Use OpenBOR animation IDs such as `idle`, `walk`, `attack1`, and `freespecial1`.
- **+ Frames** imports multiple images into the selected clip in filename order. Use zero-padded numbers to sort correctly. Click frames to scrub, drag the thumbnails to reorder, and set each delay in centiseconds. Enable **Onion skin** to overlay the previous frame at its own pivot. On the first frame of a looping clip, the overlay uses the last frame. 
- **Import sprite pack** accepts an extracted DBZ character folder such as **Goku [060]**. Select the source facing for right-facing OpenBOR artwork. Studio reads the parent **frames.csv** when available to retain the original pivots. It maps the first group to Idle, group 1 to Walk if present, and remaining groups to Free Special slots. Review clip names and timing for the character you are building.
- **Import OpenBOR model** reads the supported model and animation commands into a new editor model. The status line counts skipped commands. Import preserves basic frames, timing, pivots, boxes and the supported spawn cues. Custom scripts, command strings, complex collision properties and other unsupported directives require manual integration.
- Use **Origin → Place pivot** to click a point on the sprite, or **Origin → Drag sprite** to slide the current frame around the fixed pivot. A drag can be undone in one step. Use **Body** or **Attack** and drag a rectangle. The inspector has numeric inputs for precise edits.
- In **Inspector → Frame**, choose **Copy previous boxes** and select **Body**, **Attack**, or **Both**. Each copied box includes its enabled state and settings. On the first frame of a looping clip, the button copies from the last frame. Undo restores the prior settings in one step.
- In the preview, use **Select** and click the sprite, pivot, a collision box, or a spawn marker. With the preview focused, the arrow keys nudge the selection one pixel at a time. Nudging a pivot changes its position on the sprite. Nudging the sprite moves it around the fixed pivot. Use the small up and down arrows beside a number to change it by one.
- **Inspector → Spawns** has one projectile cue and one relative item or subentity cue per clip. Pick a model and trigger frame. **Place in preview** lets you set the X and altitude visually at the selected Z. Positive X is forward, positive Z moves down the screen, and positive altitude moves up. **Facing left/right** mirrors the sprite and the relative X positions. **Spawn ghost** shows the referenced model and a visual preview of projectile travel.
- **Effects** sets scale, flip, rotation, and the OpenBOR blend mode for the current frame. The preview shows transforms and alpha masks. Blend modes are applied by OpenBOR. Collision boxes stay in gameplay coordinates. Use a matching grayscale mask file or generate a mask from the sprite's own alpha channel.
- **Palettes** builds a shared palette from the model's frames. Select a swatch to edit it or create an alternate from the current palette. The selected palette previews on the active frame. Palette entry zero is reserved for transparency.

## OpenBOR3D characters

- Click **Import GLB model** to create a character from a self-contained, skinned GLB, or use **Inspector → GLB → Attach GLB** on an existing character. A new import maps an `idle` or `Battle_Idle` clip to `idle` when available and otherwise uses the first clip.
- In **GLB**, choose the source **Up** and **Forward** axes, then set pitch, yaw, and roll in degrees. The preview follows the fork's planar or stage-depth projection. **Target height** sets a uniform scale, and the inspector shows the rotated rest-pose width, height, and depth in OpenBOR units. Animated poses can extend beyond those dimensions.
- Use **+ OpenBOR animation** to select an engine slot, then choose a **GLB clip** for it. Play the result or scrub the normal animation timeline. For a slot with no frames, Studio creates editable timing frames based on the source clip duration. You can set delays, collision boxes, and spawn cues on those frames. Existing sprite frames and timing are kept when assigning a clip to an existing animation.
- The export contains `meshmodel`, per-animation `meshclip` bindings, and a GLB with the source-axis, pitch, and roll adjustments. It requires a compatible OpenBOR3D build. The fork renders meshes during gameplay. Character Select uses the ordinary sprite frames. Supply a sprite for that screen or configure `skipselect` in the level set.
- The fork accepts triangle skins with up to four joint influences, LINEAR or STEP joint animation, and embedded PNG base-color textures. It currently has a 32 MiB GLB limit and does not render morph targets or blended materials.

## Stages

- Add a stage, then choose a background and add one or more panels. Panels define stage width and are stitched left to right. The stage preview shows a 320 × 240 camera window, the walkable Z band and player start.
- Select the background or a panel in the Artwork list, then drag in the preview to move it. Use the arrow keys with the preview focused or set pixel offsets under **Inspector → Artwork**. Panel X offsets are measured from the panel's stitched position. Use **Reset position** to return to that position, or click **Move artwork** to return to editing spawn markers.
- Backgrounds can repeat on each axis. Studio exports their offsets as OpenBOR background settings. OpenBOR places panels in a fixed grid, so Studio composites custom panel arrangements into native panels at export. Later panels draw over earlier ones in overlaps.
- Move **Camera X**, add a spawn, and click the canvas to place it. Existing spawn markers can also be dragged. **Trigger at** controls when the event runs. Screen X is relative to that trigger, so the overview marker is plotted at **at + X**. Z is measured from the top of the screen and altitude is measured above ground.
- Click a spawn or **Player start** marker in the stage preview to select it, then nudge it with the arrow keys. Spawn nudges adjust X and Z while keeping the trigger position and altitude.
- Set model, coordinates, facing flip, boss flag or carried item in the Spawn inspector. **+ Wait at camera** inserts a combat wait at the current scroll position.
- **Stage** settings include walkable depth and player start. The current stage format targets right-scrolling levels. Preview TXT lets you inspect and copy the generated commands.
- **Spawn → Entity overrides** sets health, MP, and palette for a stage spawn.

## Cutscenes and audio

- **Stages → Inspector → Media** sets looping stage music and cutscenes that play before or after the stage. A loop offset of zero restarts the music at the beginning. Other offsets are in sample frames.
- **Module → Screens** sets the startup, idle intro, and How To Play scenes. Choose an animated GIF or a WebM with VP8 video and optional Vorbis audio. Menu GIFs play in Studio's canvas and support X/Y positioning. WebMs play in a compatible OpenBOR build or your media player. Use **Allow skipping** to set whether a player can dismiss each scene.
- **Characters → Inspector → Frame** assigns a sound to the selected animation frame. **Entity** assigns a death sound. Stage music and character sounds accept WAV and Ogg Vorbis. Use **Play WAV** to audition a WAV in Studio. Save and export carry the selected media with the project.

## Module layout

- Open **Module → HUD** to position portraits, names and scores, health and MP gauges, opponent gauges, the timer, and optional overlay art. Drag an element, nudge it with the arrow keys, or enter its X and Y. Enable **Include in export** to write the HUD settings to **levels.txt**.
- **Module → Character Select** edits each player slot, facing, Ready text, background image, and palette selection. Enable **Include in export** to write the position and facing commands and include custom background art. The preview shows the first player's Waiting or Idle sprite.
- **Module → Screens** previews the title, main menu, and Options at 320 × 240. Click a menu row or press Up and Down in the preview to check the selected font against the background. Choose background art, replace or tint the bitmap fonts, and set menu music. Main Menu and Options share the same background in OpenBOR.
- Drag a scene GIF in the **Screens** preview or scrub it with the timeline. OpenBOR supplies the built-in menu labels and layout.
- **Use main-openbor 1.1 assets** applies the supplied title and menu art, fonts, menu music, scene GIFs, and Character Select background. The template is in **ScreenTemplate** beside the executable. Saving the project copies the chosen assets into its portable **.assets** folder.
- **Module → Commands** accepts native OpenBOR configuration commands for advanced settings.

## Native TXT editor

Click **TXT** to add OpenBOR commands in the context of the selected model, animation, frame, stage, module configuration, or level set. The reference pane follows the command under the cursor and shows its parameters. Type a prefix and press **Tab** to complete a command, or select one in the reference pane and insert it. **Generated TXT** shows how the draft combines with the visually authored content. **Apply commands** saves the draft to the project and adds one undo step.

## Export and compatibility

Studio creates indexed 8-bit PNGs with palette index zero reserved for transparency. A model uses one shared palette across its frames. Images with over 255 opaque colors are quantized, and partially transparent pixels are thresholded. The editor previews the source images.

Export targets OpenBOR 4.x, using `custknife`, `throwframe`, and the `projectile_position_*` and `projectile_velocity_*` commands for the projectile, and `subentity` / `spawnframe` for a relative item spawn. Projectile ghost motion and stage camera rendering are editor previews. Physics, AI, scripts, and gameplay are handled by OpenBOR.

**Playable module** adds starter art and distinct regular and selected menu fonts wherever you have left the defaults. It creates a PAK for a project with a playable character and stage. Copy the generated PAK into the **Paks** directory of OpenBOR 4.x. The export also includes the editable **data** folder. **Content for an existing module** includes your edited screen assets along with models, stage graphics, settings, and registrations. The default video configuration is 320 × 240.

## Build from C++ source

Install Visual Studio 2022 with the **Desktop development with C++** workload and CMake 3.21 or later. From the source directory:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Run **build/Release/OpenBOR Studio.exe**. CMake copies **Demo** and **ScreenTemplate** beside the executable. The Dear ImGui, nlohmann/json, and cgltf sources and licenses are included under **vendor**. No package downloads are required to build.

## References

- [OpenBOR engine](https://github.com/DCurrent/openbor)
- [OpenBOR Legacy Manual](https://chronocrash.com/openbor/wiki/index.php/Legacy_Manual)
- [OpenBOR projectile commands](https://chronocrash.com/obor/wiki/projectile/)
- [HUD](https://chronocrash.com/obor/wiki/heads-up-display-hud/)
- [Character Select](https://chronocrash.com/obor/wiki/select-screen/)
- [Draw effects](https://chronocrash.com/openbor/wiki/index.php/Graphics_Overview#Draw_Effects)
- [Alpha masks](https://chronocrash.com/obor/wiki/alpha-mask/)
- [Palettes](https://chronocrash.com/obor/wiki/palette/)
- [Movie Player](https://chronocrash.com/openbor/wiki/index.php/Movie_Player)
- [Audio Overview](https://chronocrash.com/openbor/wiki/index.php/Audio_Overview)
- [Dear ImGui](https://github.com/ocornut/imgui/tree/v1.92.5)
- [nlohmann/json](https://github.com/nlohmann/json/tree/v3.12.0)

The Training Ground example uses a selection of sprites from the ROMs provided for this project. The stage, capsule, and projectile example art are included to demonstrate editing and preview.
