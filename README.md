# Super Mario 64 3DS Port Ultimate / 超级马力欧64 3DS Port Ultimate

Super Mario 64 3DS Port Ultimate is a Nintendo 3DS-focused build of the Super Mario 64 decompilation port, based on the 3DS port lineage and expanded with a bottom-screen minimap, persistent 3DS display settings, dynamic shadows, improved presentation work, and an optional debug toolkit for development and testing.

Super Mario 64 3DS Port Ultimate 是一个面向 Nintendo 3DS 的《超级马力欧64》反编译移植版本，基于既有 3DS 移植分支继续扩展，加入了下屏小地图、可保存的 3DS 显示设置、动态阴影、表现层优化，以及用于开发和测试的可选 debug 工具。

This repository does not include copyrighted game assets. A legally obtained prior copy of the game is required, and the matching `baserom.XX.z64` must be provided before building.

本仓库不包含受版权保护的游戏资源。构建前需要准备一份合法取得的原版游戏，并将对应区域版本的 `baserom.XX.z64` 放入工程目录。

The project can produce both Homebrew Launcher `.3dsx` builds and installable `.cia` builds. The CIA package metadata currently uses Product Code `CTR-P-S64U` and Unique ID `0xae564`, so it is separated from older branch installs that use different title IDs.

本项目可以构建 Homebrew Launcher 使用的 `.3dsx`，也可以构建可安装的 `.cia`。当前 CIA 包信息使用 Product Code `CTR-P-S64U` 和 Unique ID `0xae564`，用于和旧分支版本区分，避免互相覆盖。

## Asset Policy / 资产策略

This repository is intended to be distributed without the full set of copyrighted ROM-extracted game assets. Builders must supply their own legal `baserom.XX.z64` and regenerate extracted content locally before compiling. A small number of generated asset-side files may remain tracked when they carry project-specific adjustments.

本仓库的发布形态应当不包含完整的、从原版 ROM 提取出的受版权保护游戏资源。构建者需要自行准备合法取得的 `baserom.XX.z64`，并在本地重新提取相关内容后再进行编译。若少量生成资产文件承载了本项目特有的修正，它们可以作为例外保留。

During development, a small number of generated asset-side files were intentionally patched for project behavior or presentation. If extracted assets are cleaned, these adjustments must be recreated or re-applied in the local working tree, or those patched files should be kept as explicit exceptions:

在开发过程中，少量“生成资产侧”文件被有意补丁化，用于本项目的行为或表现修正。如果清理提取资产，这些调整需要在本地重新生成后再次应用，或者将这些补丁化文件明确作为例外保留：

- `actors/blue_coin_switch/geo.inc.c`
- `actors/bobomb/geo.inc.c`
- `actors/bobomb/model.inc.c`
- `actors/cannon_barrel/geo.inc.c`
- `actors/cannon_base/geo.inc.c`
- `actors/capswitch/geo.inc.c`
- `actors/mario/geo.inc.c`
- `actors/poundable_pole/geo.inc.c`
- `actors/water_bubble/geo.inc.c`

The bottom-screen minimap image set under `src/minimap/textures/` is also project-specific generated content. It is not treated as vanilla upstream source and should be regenerated locally by the project tooling instead of being assumed to come from the base decompilation tree.

`src/minimap/textures/` 下的下屏小地图图像集同样属于本项目特有的生成内容。它不应被视为原始上游源码的一部分，而应通过本项目工具链在本地重新生成。

## Feature Highlights / 功能概览

- **Nintendo 3DS renderer and packaging.** The port targets the 3DS graphics stack, supports SMDH injection for `.3dsx`, and supports CIA icon/banner packaging through the standard 3DS build pipeline.
  **Nintendo 3DS 渲染与打包。** 本移植面向 3DS 图形栈，支持向 `.3dsx` 注入 SMDH 信息，也支持通过标准 3DS 构建流程生成带图标与横幅的 CIA。

- **Stereo 3D and display modes.** The game supports stereo 3D mode at 400px and a high-resolution 800px mode. Anti-aliasing and 800px mode are disabled while stereo 3D is active, matching the hardware constraints.
  **立体 3D 与显示模式。** 游戏支持 400px 立体 3D 模式与 800px 高分辨率模式。开启立体 3D 时会禁用抗锯齿和 800px 模式，以符合硬件限制。

- **Persistent 3DS video settings.** Anti-aliasing and 400px/800px mode can be changed from the touch menu and are saved to `sm64config.txt`, so players can keep performance-friendly settings across launches.
  **可保存的 3DS 显示设置。** 抗锯齿与 400px/800px 模式可以在触摸菜单中切换，并保存到 `sm64config.txt`，玩家关闭高负载选项后，下次启动仍会保留设置。

- **Bottom-screen minimap.** The lower screen displays course maps with Mario position and heading, including multi-area courses and special states such as drained Castle Grounds where supported.
  **下屏小地图。** 下屏会显示关卡地图、马力欧位置和朝向，并支持多区域关卡以及城堡外水位排空等特殊状态。

- **Bottom-screen HUD layout.** Lives, stars, coins, red coin sprites, and the current BGM title are presented on the lower screen with a stable layout designed for quick glance reading during play.
  **下屏 HUD 布局。** 生命数、星星数、金币数、红币精灵图和当前 BGM 标题会常驻显示在下屏，并以适合游玩中快速读取的布局呈现。

- **Scene-aware bottom-screen presentation.** Title screens, star select screens, transitions, dialogs, and file-select scenes receive dedicated lower-screen handling so the minimap does not appear in scenes where it would be visually incorrect.
  **场景感知的下屏表现。** 标题画面、星星选择、转场、对话和存档选择等场景都有专门的下屏处理，避免小地图在不合适的画面中出现。

- **Synchronized transition rendering.** The lower screen mirrors the original upper-screen transition styles, including circle, star, Mario head, Bowser head, and color fade transitions, while preserving the previous lower-screen frame during scene changes.
  **同步转场渲染。** 下屏会同步使用上屏原版转场样式，包括圆形、星星、马力欧头、库巴头以及纯色淡入淡出，并在场景切换期间保留上一帧下屏画面。

- **Touch-screen mini-menu.** A full-screen touch trigger opens a centered menu with anti-aliasing, resolution, debug, hide-menu, and exit controls. The menu is disabled or automatically closed during title, file-select, and transition states where rendering conflicts can occur.
  **触摸屏迷你菜单。** 通过全屏触摸触发居中的菜单，可切换抗锯齿、分辨率、debug，或隐藏菜单、退出游戏。在标题、存档选择和转场等容易发生渲染冲突的状态下，菜单会被禁用或自动关闭。

- **Dynamic shadows.** Supported objects can use dynamic projected shadows through the 3DS renderer. The feature is controlled by `dynamic_shadows_enabled` in `sm64config.txt`, and the original shadow behavior remains available as a fallback.
  **动态阴影。** 支持的对象可以通过 3DS 渲染器显示动态投影阴影。该功能由 `sm64config.txt` 中的 `dynamic_shadows_enabled` 控制，同时保留原版阴影作为回退方案。

- **Death ragdoll system.** Fatal damage can transition Mario into a physics-driven death ragdoll with collision response, wall braking, ledge behavior, squish handling, and a delayed high-angle death camera.
  **死亡布娃娃系统。** 马力欧受到致命伤害后可以进入基于物理的死亡布娃娃状态，包含碰撞响应、墙面制动、边缘行为、挤压处理，以及带延迟的高角度死亡镜头。

- **Optional debug mode.** Debug mode is off by default and is not persisted. When enabled from the touch menu, it provides development/test utilities such as ragdoll visualization, life recovery, manual ragdoll triggering, and an in-place BLJ launcher for verifying high-speed behavior.
  **可选 debug 模式。** Debug 默认关闭且不会保存。通过触摸菜单开启后，可使用布娃娃可视化、生命恢复、手动触发布娃娃，以及用于验证高速行为的原地 BLJ 发射器等开发/测试功能。

- **Puppycam and camera improvements.** The project includes the 3DS-updated Puppycam patch, with configurable sensitivity, inversion, stopping speed, centering aggression, and pan amount through `sm64config.txt`.
  **Puppycam 与镜头改进。** 项目包含适配 3DS 的 Puppycam 补丁，并可通过 `sm64config.txt` 调整灵敏度、反转、停止速度、回中强度和横移量。

- **60 FPS and FPS display patches.** The included enhancement patches cover 60 FPS support and optional frame-rate display behavior, updated for the 3DS target.
  **60 FPS 与帧数显示补丁。** 项目包含面向 3DS 更新过的 60 FPS 支持与帧数显示相关补丁。

- **Multi-threaded audio.** Audio runs on Core 1 on Old 3DS and Core 2 on New 3DS. This requires Luma3DS v10.1.1 or newer.
  **多线程音频。** 音频线程在 Old 3DS 上运行于 Core 1，在 New 3DS 上运行于 Core 2。该功能需要 Luma3DS v10.1.1 或更新版本。

- **Enhanced RSP audio emulation.** The 3DS audio mixer includes enhanced RSPA paths for performance, with build flags available for reference behavior or accurate math when testing.
  **增强 RSP 音频模拟。** 3DS 音频混音器包含用于提升性能的增强 RSPA 路径，并提供构建参数用于测试参考实现或更精确的数学行为。

- **Naive frame-skip option.** A legacy 3DS frame-skip path remains available with `ENABLE_N3DS_FRAMESKIP=1` when building, although it is usually not necessary for current builds.
  **朴素跳帧选项。** 项目仍保留旧版 3DS 跳帧路径，可在构建时使用 `ENABLE_N3DS_FRAMESKIP=1` 开启，不过当前版本通常不需要它。

- **Configurable controls.** Player controls are read from `sm64config.txt`, including 3DS button bindings and camera-related options.
  **可配置按键。** 玩家按键会从 `sm64config.txt` 读取，包括 3DS 按键绑定和镜头相关选项。

- **Build-time audio toggle.** Audio can be disabled at build time with `DISABLE_AUDIO=1`, useful for narrow rendering or platform tests.
  **构建时关闭音频。** 可以通过 `DISABLE_AUDIO=1` 在构建阶段关闭音频，适合进行单独的渲染或平台测试。

- **Graphics pool fix.** The port includes the `GFX_POOL_SIZE` fix used by 32-bit platforms to support heavier rendering paths such as 60 FPS builds.
  **图形池修复。** 本移植包含 32 位平台常用的 `GFX_POOL_SIZE` 修复，以支持 60 FPS 等更重的渲染路径。

## Controls and Touch Screen / 操作与触摸屏

Tap the lower screen to open the mini-menu when the current scene allows it. The menu provides anti-aliasing, resolution mode, debug mode, hide-menu, and exit controls.

在当前场景允许时，点击下屏即可打开迷你菜单。菜单提供抗锯齿、分辨率模式、debug 模式、隐藏菜单和退出游戏控制。

The anti-aliasing and resolution buttons are unavailable during stereo 3D because those modes conflict with the 3DS 3D display path.

由于抗锯齿和 800px 模式与 3DS 立体显示路径冲突，开启立体 3D 时相关按钮不可用。

Debug mode is intentionally a temporary runtime switch. It resets to off on each launch and is meant for testing rather than normal play.

Debug 模式有意设计为临时运行时开关。每次启动都会恢复关闭状态，主要用于测试，而不是普通游玩。

## Configuration and Save Data / 配置与存档

The main configuration file is `sm64config.txt`. It stores controls, Puppycam values, dynamic shadow preference, and the 3DS anti-aliasing / wide-mode choices.

主配置文件为 `sm64config.txt`。它会保存按键、Puppycam 参数、动态阴影偏好，以及 3DS 抗锯齿和宽屏模式选择。

For `.3dsx` builds, configuration and save data are stored beside the `.3dsx` file. For `.cia` builds, configuration and save data are stored at the SD card root.

对于 `.3dsx` 版本，配置文件和存档会保存在 `.3dsx` 文件同目录。对于 `.cia` 版本，配置文件和存档会保存在 SD 卡根目录。

Useful configuration keys include `dynamic_shadows_enabled`, `n3ds_anti_aliasing`, `n3ds_wide_mode`, `puppycam_sensitivity_x`, `puppycam_sensitivity_y`, `puppycam_invert_x`, `puppycam_invert_y`, `puppycam_stopping_speed`, `puppycam_centre_aggression`, and `puppycam_pan_amount`.

常用配置项包括 `dynamic_shadows_enabled`、`n3ds_anti_aliasing`、`n3ds_wide_mode`、`puppycam_sensitivity_x`、`puppycam_sensitivity_y`、`puppycam_invert_x`、`puppycam_invert_y`、`puppycam_stopping_speed`、`puppycam_centre_aggression` 和 `puppycam_pan_amount`。

## Building / 构建

Place a matching baserom in the repository root before building, for example `baserom.us.z64` for the US version. Change `VERSION=us` to `eu`, `jp`, or `sh` when building another supported region.

构建前，请将匹配的 baserom 放在仓库根目录，例如美版使用 `baserom.us.z64`。如果构建其他支持的区域版本，请将 `VERSION=us` 改为 `eu`、`jp` 或 `sh`。

After changing build flags, run `make clean` before rebuilding so the new flags are applied consistently.

修改构建参数后，请先运行 `make clean` 再重新构建，以确保新参数被完整应用。

Build a Homebrew Launcher `.3dsx`:

构建 Homebrew Launcher 使用的 `.3dsx`：

```sh
make VERSION=us --jobs 4
```

Build an installable `.cia`:

构建可安装的 `.cia`：

```sh
make VERSION=us cia
```

Build with the legacy 3DS frame-skip option:

构建时开启旧版 3DS 跳帧选项：

```sh
make VERSION=us ENABLE_N3DS_FRAMESKIP=1 --jobs 4
```

Disable audio for a test build:

构建测试版本时关闭音频：

```sh
make VERSION=us DISABLE_AUDIO=1 --jobs 4
```

Use the PC port's reference RSP audio path:

使用 PC 移植版的参考 RSP 音频路径：

```sh
make VERSION=us FORCE_REFERENCE_RSPA=1 --jobs 4
```

Disable the enhanced RSP audio performance path while keeping the 3DS mixer:

在保留 3DS 混音器的同时关闭增强 RSP 音频性能路径：

```sh
make VERSION=us DISABLE_ENHANCED_RSPA=1 --jobs 4
```

Use accurate audio math where supported:

在支持的音频实现中使用更精确的数学行为：

```sh
make VERSION=us AUDIO_USE_ACCURATE_MATH=1 --jobs 4
```

The 3DS toolchain expects devkitPro/devkitARM and the usual 3DS build tools such as `3dsxtool`, `smdhtool`, `tex3ds`, and `makerom`. Docker, Linux/WSL, and MSYS2 setups are all viable if the devkitPro environment is installed correctly.

3DS 工具链需要 devkitPro/devkitARM，以及 `3dsxtool`、`smdhtool`、`tex3ds`、`makerom` 等常见 3DS 构建工具。只要 devkitPro 环境配置正确，Docker、Linux/WSL 和 MSYS2 都可以用于构建。

After building, copy the generated `.3dsx` to the `/3ds` directory on the SD card for Homebrew Launcher usage, or install the generated `.cia` with a compatible title installer.

构建完成后，可以将生成的 `.3dsx` 复制到 SD 卡 `/3ds` 目录并通过 Homebrew Launcher 启动，也可以使用兼容的 title 安装器安装生成的 `.cia`。

## Package Assets / 打包资源

To change the `.3dsx` icon, replace `3ds/icon.png` before building. The build process generates the SMDH file used by the final `.3dsx`.

如需更换 `.3dsx` 图标，请在构建前替换 `3ds/icon.png`。构建流程会生成最终 `.3dsx` 使用的 SMDH 文件。

To change CIA presentation assets, update `3ds/icon.png` and `3ds/banner.png`, then regenerate the CIA icon and banner assets with `bannertool`.

如需更换 CIA 展示资源，请更新 `3ds/icon.png` 和 `3ds/banner.png`，然后使用 `bannertool` 重新生成 CIA 图标和横幅资源。

## Project Structure / 项目结构

The repository follows the standard Super Mario 64 decompilation layout, with 3DS-specific platform code and project enhancements layered on top.

本仓库沿用标准《超级马力欧64》反编译工程结构，并在其上加入 3DS 平台代码和本项目增强功能。

```text
sm64
├── actors: object behaviors, geo layout, and display lists
├── asm: handwritten assembly code and non-matching sections
├── assets: animation and demo data
├── bin: C files for ordering display lists and textures
├── build: generated build output
├── data: behavior scripts and miscellaneous data
├── doxygen: documentation infrastructure
├── enhancements: optional and project-specific gameplay/rendering changes
├── include: header files
├── levels: level scripts, geo layout, and display lists
├── lib: SDK library code
├── rsp: audio and Fast3D RSP assembly code
├── sound: sequences, sound samples, and sound banks
├── src: C source code for the game and platform layer
│   ├── audio: audio code
│   ├── buffers: stacks, heaps, and task buffers
│   ├── engine: script processing engines and utilities
│   ├── game: gameplay behaviors and core game source
│   ├── goddard: Mario intro screen
│   ├── menu: title, file, act, and level-selection menus
│   └── pc: port code, audio, video, and 3DS renderer code
├── text: dialog, level names, and act names
├── textures: skybox and generic texture data
└── tools: build and asset tools
```

## Credits / 致谢

This project builds on the Super Mario 64 decompilation and 3DS port work by the broader SM64 port community.

本项目建立在《超级马力欧64》反编译工程和 SM64 port 社区的 3DS 移植工作之上。

Credits go to Gericom for the `sm64_3ds` port lineage that this 3DS flavor is based on.

感谢 Gericom 以及 `sm64_3ds` 移植分支，本项目的 3DS 基础来自该移植路线。

The 3DS branch is based on the Refresh 11-era port work and keeps the original 3DS-specific improvements such as stereo 3D, multi-threaded audio, enhanced RSPA, SMDH support, and 3DS build packaging.

本 3DS 分支基于 Refresh 11 时期的移植工作，并保留了原有 3DS 专属改进，包括立体 3D、多线程音频、增强 RSPA、SMDH 支持和 3DS 构建打包流程。

Minimap model and map resource credits include alecpike for Bob-omb Battlefield, Turtle Boy for Lethal Lava Land and Whomp's Fortress, SlyP54 for Castle Grounds, and Bruz for Castle Interior.

小地图模型和地图资源致谢包括：alecpike 提供 Bob-omb Battlefield，Turtle Boy 提供 Lethal Lava Land 与 Whomp's Fortress，SlyP54 提供 Castle Grounds，Bruz 提供 Castle Interior。

Additional enhancement work in this branch includes the bottom-screen minimap/HUD system, synchronized lower-screen transitions, persistent 3DS menu settings, dynamic shadows, death ragdoll behavior, and 3DS presentation polish.

本分支的额外增强包括下屏小地图/HUD 系统、同步下屏转场、可保存的 3DS 菜单设置、动态阴影、死亡布娃娃行为，以及 3DS 表现层打磨。

## Contributing / 贡献

Pull requests are welcome. For major changes, please open an issue first so implementation scope, compatibility impact, and build targets can be discussed clearly.

欢迎提交 Pull Request。若改动较大，请先开启 Issue，以便明确实现范围、兼容性影响和目标构建平台。
