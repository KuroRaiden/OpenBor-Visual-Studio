# OpenBor Visual Studio

A native Windows editor for OpenBOR character models and side-scrolling stages. Built with C++20, Dear ImGui, Direct3D 11, and Windows Imaging Component.

## Start here

1. Unzip the Windows package and launch **OpenBOR Studio.exe**. It starts with a blank project. Click **+ New model** or use an import button to start a character, or switch to **Stages** to create a stage.
2. Choose **Project → Open demo** to try the included **Training Ground** project. Press **Space** to play Goku’s attack animation. The cyan marker shows the projectile origin and the amber marker shows the item origin. Click **Stages** to open the canyon stage. Keep the **Demo** folder beside the executable to use this example.
3. Use **Project → Open project** to load an existing project. **Save** stores a **.borstudio** file and a sibling **.assets** directory. Keep them together when moving a project.
4. Use **Export** and choose an output directory. Studio creates a new **OpenBOR_Export** directory containing the **data** subtree, model registrations and stage registrations.

## Characters

- **New model** creates a model with an Idle animation. Set its ID, type, health and speed under **Inspector → Model**. Use OpenBOR animation IDs such as `idle`, `walk`, `attack1`, and `freespecial1`.
- **+ Frames** imports multiple images into the selected clip in filename order. Use zero-padded numbers to sort correctly. Click frames to scrub, drag the thumbnails to reorder, and set each delay in centiseconds. Enable **Onion skin** to overlay the previous frame at its own pivot. On the first frame of a looping clip, the overlay uses the last frame. 
- **Import sprite pack** accepts an extracted DBZ character folder such as **Goku [060]**. Select the source facing for right-facing OpenBOR artwork. Studio reads the parent **frames.csv** when available to retain the original pivots. It maps the first group to Idle, group 1 to Walk if present, and remaining groups to Free Special slots. Review clip names and timing for the character you are building.
- **Import OpenBOR model** reads the supported model and animation commands into a new editor model. The status line counts skipped commands. Import preserves basic frames, timing, pivots, boxes and the supported spawn cues. Custom scripts, command strings, complex collision properties and other unsupported directives require manual integration.
- Use **Origin → Place pivot** to click a point on the sprite, or **Origin → Drag sprite** to slide the current frame around the fixed pivot. A drag can be undone in one step. Use **Body** or **Attack** and drag a rectangle. The inspector has numeric inputs for precise edits.
- In **Inspector → Frame**, choose **Copy previous boxes** and select **Body**, **Attack**, or **Both**. Each copied box includes its enabled state and settings. On the first frame of a looping clip, the button copies from the last frame. Undo restores the prior settings in one step.
- **Inspector → Spawns** has one projectile cue and one relative item or subentity cue per clip. Pick a model and trigger frame. **Place in preview** lets you set the X and altitude visually at the selected Z. Positive X is forward, positive Z moves down the screen, and positive altitude moves up. **Facing left/right** mirrors the sprite and the relative X positions. **Spawn ghost** shows the referenced model and a visual preview of projectile travel.

## Stages

- Add a stage, then choose a background and add one or more panels. Panels define stage width and are stitched left to right. The stage preview shows a 320 × 240 camera window, the walkable Z band and player start.
- Move **Camera X**, add a spawn, and click the canvas to place it. Existing spawn markers can also be dragged. **Trigger at** controls when the event runs. Screen X is relative to that trigger, so the overview marker is plotted at **at + X**. Z is measured from the top of the screen and altitude is measured above ground.
- Set model, coordinates, facing flip, boss flag or carried item in the Spawn inspector. **+ Wait at camera** inserts a combat wait at the current scroll position.
- **Stage** settings include walkable depth and player start. The current stage format targets right-scrolling levels. Preview TXT lets you inspect and copy the generated commands.

## Export and compatibility

Studio creates indexed 8-bit PNGs with palette index zero reserved for transparency. A model uses one shared palette across its frames. Images with over 255 opaque colors are quantized, and partially transparent pixels are thresholded. The editor previews the source images.

Export targets OpenBOR 4.x, using `custknife`, `throwframe`, and the `projectile_position_*` and `projectile_velocity_*` commands for the projectile, and `subentity` / `spawnframe` for a relative item spawn. Projectile ghost motion and stage camera rendering are editor previews. Physics, AI, scripts, and gameplay are handled by OpenBOR.

The export supplies models, stage graphics, spawn definitions, and registration files. Merge the generated registrations into your mod's **models.txt** and **levels.txt**. Keep your existing module's title screens, menu assets, fonts, HUD, and other shared resources. **video.txt** is provided as a 320 × 240 reference configuration.

## Build from C++ source

Install Visual Studio 2022 with the **Desktop development with C++** workload and CMake 3.21 or later. From the source directory:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Run **build/Release/OpenBOR Studio.exe**. CMake copies the demo beside the executable. The Dear ImGui and nlohmann/json sources and licenses are included under **vendor**. No package downloads are required to build.

## References

- [OpenBOR engine](https://github.com/DCurrent/openbor)
- [OpenBOR Legacy Manual](https://chronocrash.com/openbor/wiki/index.php/Legacy_Manual)
- [OpenBOR projectile commands](https://chronocrash.com/obor/wiki/projectile/)
- [Dear ImGui](https://github.com/ocornut/imgui/tree/v1.92.5)
- [nlohmann/json](https://github.com/nlohmann/json/tree/v3.12.0)

The Training Ground example uses a selection of sprites from the ROMs provided for this project. The stage, capsule, and projectile example art are included to demonstrate editing and preview.
