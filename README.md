# Super Mario 64 3DS Port Ultimate / スーパーマリオ64 3DS ポート アルティメット

<p>
  <img src="https://img.shields.io/badge/platform-Nintendo%203DS-1f6feb?style=for-the-badge" alt="Nintendo 3DS">
  <img src="https://img.shields.io/badge/output-.3dsx%20%7C%20.cia-6f42c1?style=for-the-badge" alt="3DSX and CIA">
</p>

<table>
  <tr>
    <td valign="top">

Super Mario 64 3DS Port Ultimate is a Nintendo 3DS-focused build of the Super Mario 64 decompilation port, based on the 3DS port lineage and expanded with a bottom-screen minimap, persistent 3DS display settings, dynamic shadows, improved presentation work, ragdoll system and optional debug options for development and testing.

Super Mario 64 3DS Port Ultimate は、Nintendo 3DS 向けに調整された『スーパーマリオ64』デコンパイルポート版です。既存の 3DS ポート系統をベースに、下画面ミニマップ、保存可能な 3DS 表示設定、動的シャドウ、表示演出の改善、ラグドールシステム、開発・テスト用の任意で使える debug オプションを追加しています。

  </td>
    <td align="right" valign="top" width="360">
      <img src="indevscreenshots/IMG_9021.jpeg" alt="Super Mario 64 3DS Port Ultimate preview" width="320">
    </td>
  </tr>
</table>

## ⬇️Clone without development screenshots / ダウンロード：
```sh
git clone --filter=blob:none --sparse https://github.com/Epic0522/Super-Mario-64-3ds-port---Ultimate.git && cd Super-Mario-64-3ds-port---Ultimate && git sparse-checkout set --no-cone '/*' '!/indevscreenshots/'
```
## 🔗Related Repositories / 関連リポジトリ

- **SM64 decompilation / SM64 デコンパイル：** [n64decomp/sm64](https://github.com/n64decomp/sm64)
- **SM64 port / SM64 ポート：** [sm64-port/sm64-port](https://github.com/sm64-port/sm64-port)
- **SM64 3DS / SM64 3DS：** [sm64-port/sm64_3ds](https://github.com/sm64-port/sm64_3ds) 
- **SM64 3DS minimap prototype / SM64 3DS ミニマッププロトタイプ：** [mkst/sm64-port](https://github.com/mkst/sm64-port)

## 👀At a Glance / 概要

- **Platform target:** Nintendo 3DS, with both `.3dsx` and `.cia` outputs.
- **Signature additions:** bottom-screen minimap and HUD, synchronized lower-screen transitions, hybrid official/free camera control, dynamic shadows, death ragdoll behavior, and optional debug utilities.
- **Persistent options:** 3DS anti-aliasing, 400px/800px display mode, dynamic shadows, death ragdoll, hit ragdoll, and camera tuning are saved in `sm64config.txt`.

- **対象プラットフォーム：** Nintendo 3DS。`.3dsx` と `.cia` の両方を出力できます。
- **主な追加要素：** 下画面ミニマップと HUD、同期した下画面トランジション、公式/自由カメラのハイブリッド操作、動的シャドウ、ラグドール。
- **保存される設定：** 3DS アンチエイリアス、400px/800px 表示モード、動的シャドウ、死亡ラグドール、受撃ラグドール、カメラ設定は `sm64config.txt` に保存されます。


During development, a small number of generated asset-side files were intentionally patched for project behavior or presentation. Because `extract_assets.py` can overwrite these files, the project keeps mirrored replacements under `project_asset_overrides/`. After local extraction, copy that directory back into the repository root so the patched versions replace the freshly generated ones:

開発中に、一部の生成済みアセット側ファイルを本プロジェクト用の挙動・表示調整のために変更しています。`extract_assets.py` によってこれらのファイルが上書きされる可能性があるため、`project_asset_overrides/` に同じ構成の上書き用ファイルを保持しています。ローカルでアセット抽出を行った後、このディレクトリの内容をリポジトリのルートへコピーし、生成されたファイルをパッチ版で上書きしてください：

```sh
cp -R project_asset_overrides/* .
```

The files currently preserved in that override pack are:
現在この上書きパックに含まれているファイル：

- `actors/blue_coin_switch/geo.inc.c`
- `actors/cannon_barrel/geo.inc.c`
- `actors/cannon_base/geo.inc.c`
- `actors/capswitch/geo.inc.c`
- `actors/mario/geo.inc.c`
- `actors/poundable_pole/geo.inc.c`
- `actors/water_bubble/geo.inc.c`


## ⚙️Feature Highlights / 機能概要

### 3DS Presentation / 3DS 表示演出

- **Stereo 3D and display modes.** The game supports 400px stereoscopic 3D and an 800px high-resolution mode. The renderer automatically disables anti-aliasing and 800px rendering while stereoscopic 3D is active so the display path stays within hardware limits.
  **立体 3D と表示モード。** ゲームは 400px の立体 3D と 800px の高解像度モードに対応しています。立体 3D 有効時は、ハードウェア制限内で動作するように、レンダラーがアンチエイリアスと 800px レンダリングを自動的に無効化します。

- **Persistent 3DS video settings.** Anti-aliasing and 400px/800px mode can be changed from the touch menu and are saved to `sm64config.txt`, so performance-oriented choices remain in effect after rebooting the game.
  **保存可能な 3DS 表示設定。** アンチエイリアスと 400px/800px モードはタッチメニューから切り替えられ、`sm64config.txt` に保存されます。そのため、パフォーマンス重視の設定はゲーム再起動後も維持されます。

- **Transition-matched lower-screen compositing.** The lower screen mirrors the original upper-screen transition family, including circle, star, Mario head, Bowser head, and full-color fades.
  **上画面と同期する下画面トランジション。** 下画面は、円形、星、マリオの顔、クッパの顔、単色フェードなど、オリジナル上画面のトランジション表現に同期します。

### Bottom-Screen Interface / 下画面インターフェース

- **Bottom-screen minimap.** The lower screen displays course maps with Mario position and heading, including support for multi-area stages and special map states such as drained Castle Grounds where applicable.
  **下画面ミニマップ。** 下画面にはコースマップ、マリオの位置と向きが表示されます。複数エリアを持つステージや、城外の排水状態のような特殊なマップ状態にも対応しています。

- **Integrated lower-screen HUD.** Lives, stars, coins, red coin sprites, and the current BGM title are laid out as a persistent bottom-screen HUD for quick reference without occupying the main gameplay view.
  **統合された下画面 HUD。** 残機数、スター数、コイン数、赤コインのスプライト、現在の BGM タイトルを常時表示の下画面 HUD として配置し、メインのゲーム画面を占有せずに素早く確認できます。

- **Touch mini-menu.** A full-screen touch trigger opens a centered lower-screen control panel. The root menu provides anti-aliasing, resolution, debug, `Enh`, hide-menu, and exit actions; `Enh` opens a second page for dynamic shadows, death ragdoll, and hit ragdoll toggles. The menu is forcibly closed or disabled in scenes where mixed rendering would cause conflicts, and every reopen starts from the root page.
  **タッチメニュー。** 下画面全体のタッチ操作で中央配置の操作パネルを開きます。ルートメニューではアンチエイリアス、解像度、debug、`Enh`、メニュー非表示、終了を操作でき、`Enh` からは動的シャドウ、死亡ラグドール、受撃ラグドールの切り替えページに入れます。混合レンダリングの競合が起きやすい場面ではメニューは強制的に閉じられるか無効化され、開き直したときは常にルートページから始まります。

### Camera and Controls / カメラと操作

- **Hybrid official/free camera system.** The project combines the original official SM64 camera with a 3DS-adapted free-camera mode. Players can switch between them at runtime instead of committing to only one camera style for the whole play session.
  **公式/自由カメラのハイブリッドシステム。** 本プロジェクトは、オリジナル SM64 の公式カメラと 3DS 向けに調整した自由カメラを組み合わせています。プレイヤーは実行中にいつでも切り替えられます。

- **Quick recenter behavior.** A dedicated recenter input snaps the free camera behind Mario immediately, and in official mode it restores a stable Lakitu trailing camera. This gives the 3DS build a faster and more reliable recovery path than the default camera flow.
  **クイック再センタリング。** 専用の再センタリング入力により、自由カメラでは視点をすぐマリオの背後へ戻し、公式カメラでは安定した Lakitu 追従視点へ戻します。オリジナルの標準カメラ操作よりも、3DS 版では素早く確実に視点を立て直せます。

- **Analogue camera input on C-Stick.** When free camera is active, the C-Stick acts as analogue camera input. When it is idle, the game can still fall back to the original digital C-button style expected by SM64.
  **C-Stick のアナログカメラ入力。** 自由カメラ有効時は C-Stick をアナログカメラ入力として使用します。入力がない場合でも、SM64 オリジナルが想定するデジタル C ボタン形式に戻せます。

- **Saved Puppycam tuning.** Sensitivity, inversion, stopping speed, centering aggression, and pan amount are all stored in `sm64config.txt`.
  **保存可能な Puppycam 設定。** 感度、反転、停止速度、中央復帰の強さ、パン量はすべて `sm64config.txt` に保存されます。

- **Configurable controls.** Player controls are read from `sm64config.txt`, including 3DS button bindings and camera-related behavior.
  **設定可能な操作。** 3DS ボタン割り当てやカメラ関連の挙動を含むプレイヤー操作は、`sm64config.txt` から読み込まれます。

### Gameplay and Debugging / ゲームプレイ強化とデバッグ

- **Dynamic shadows.** Supported objects can use dynamic projected shadows through the 3DS renderer. The feature is controlled by `dynamic_shadows_enabled` in `sm64config.txt`, and the original shadow behavior remains available as a fallback.
  **動的シャドウ。** 対応オブジェクトは 3DS レンダラーを通じて動的な投影シャドウを表示できます。この機能は `sm64config.txt` の `dynamic_shadows_enabled` で制御され、オリジナルのシャドウ挙動もフォールバックとして残しています。

- **Death ragdoll system.** Fatal damage can transition Mario into a physics-driven death ragdoll with collision response, wall braking, ledge behavior, squish handling, and a high-angle death camera designed to frame the scene more cleanly.
  **死亡ラグドールシステム。** マリオが致命的なダメージを受けると、物理ベースのラグドール状態へ移行できます。衝突応答、壁ブレーキ、足場端での挙動、押しつぶし処理、シーンを見やすく映す俯瞰カメラを含みます。

- **Hit ragdoll toggle (destructive to vanilla behavior).** The `Enh` submenu can also redirect supported non-lethal knockback reactions into a recoverable ragdoll state. This option is intentionally destructive relative to original SM64 timing and feel, depends on death ragdoll being enabled, and defaults to off.
  **受撃ラグドール切り替え（原作挙動に対して破壊的）。** `Enh` サブメニューでは、対応する非致死ノックバックを復帰可能なラグドール状態へ置き換える設定も切り替えられます。この機能はオリジナル SM64 の受撃タイミングや感触を意図的に崩す破壊的な変更であり、死亡ラグドール有効時のみ使用でき、初期状態ではオフです。

- **Optional debug mode.** Debug mode is off by default and is not persisted. When enabled from the touch menu, it exposes development/test utilities such as ragdoll visualization, life recovery, manual ragdoll triggering, FPS display support, and an in-place BLJ for high-speed behavior testing.
  **任意で使える debug モード。** Debug は標準でオフで、保存されません。タッチメニューから有効化すると、ラグドール可視化、体力回復、手動ラグドール起動、FPS 表示対応、高速挙動テスト用のその場 BLJ などのテスト機能を利用できます。


### Performance and Audio / パフォーマンスと音声

- **60 FPS and FPS display patches.** The included enhancement patches cover 60 FPS support and optional frame-rate display behavior, updated for the 3DS target.
  **60 FPS と FPS 表示パッチ。** 3DS 向けに更新された 60 FPS 対応と、任意の FPS 表示に関する拡張パッチを含みます。

- **Multi-threaded audio.** Audio runs on Core 1 on Old 3DS and Core 2 on New 3DS. This requires Luma3DS v10.1.1 or newer.
  **マルチスレッド音声。** 音声スレッドは Old 3DS では Core 1、New 3DS では Core 2 で動作します。この機能には Luma3DS v10.1.1 以降が必要です。

- **Enhanced RSP audio emulation.** The 3DS audio mixer includes enhanced RSPA paths for performance, with build flags available for reference behavior or accurate math when testing.
  **強化 RSP 音声エミュレーション。** 3DS 音声ミキサーにはパフォーマンス向上用の強化 RSPA 経路が含まれ、参照実装やより正確な演算挙動をテストするためのビルドフラグも用意されています。

- **Naive frame-skip option.** A legacy 3DS frame-skip path remains available with `ENABLE_N3DS_FRAMESKIP=1` when building, although it is usually not necessary for current builds.
  **簡易フレームスキップオプション。** 旧来の 3DS フレームスキップ経路も残っており、ビルド時に `ENABLE_N3DS_FRAMESKIP=1` で有効化できます。ただし現在のビルドでは通常必要ありません。

- **Build-time audio toggle and graphics pool fix.** Audio can be disabled with `DISABLE_AUDIO=1` for targeted testing, and the `GFX_POOL_SIZE` fix is included to support heavier rendering paths such as 60 FPS builds.
  **ビルド時の音声無効化とグラフィックプール修正。** `DISABLE_AUDIO=1` によりテスト用に音声を無効化できます。また、60 FPS など重いレンダリング経路を支えるための `GFX_POOL_SIZE` 修正も含まれています。

## 🎮Controls and Touch Screen / 操作とタッチ画面

Tap the lower screen to open the mini-menu when the current scene allows it. The root page provides anti-aliasing, resolution mode, debug mode, `Enh`, hide-menu, and exit controls. The `Enh` page contains dynamic shadows, death ragdoll, and hit ragdoll toggles; hit ragdoll is forced off whenever death ragdoll is off.

現在の場面で許可されている場合、下画面をタッチするとミニメニューを開けます。ルートページではアンチエイリアス、解像度モード、debug モード、`Enh`、メニュー非表示、ゲーム終了を操作できます。`Enh` ページでは動的シャドウ、死亡ラグドール、受撃ラグドールを切り替えられ、死亡ラグドールがオフのときは受撃ラグドールも強制的にオフになります。

On Nintendo 3DS, `X` toggles between the original official camera and the free Puppycam-style camera. `Y` performs a quick recenter action: in free camera it snaps the view back behind Mario, and in official camera it restores a stable Lakitu-style trailing view.

Nintendo 3DS では、`X` でオリジナルの公式カメラと Puppycam 風の自由カメラを切り替えます。`Y` はクイック再センタリングです。自由カメラでは視点をすばやくマリオの背後へ戻し、公式カメラでは安定した追従視点へ戻します。

When the free camera is active, the C-Stick is used as analogue camera input. When using Dpads, the camera can act as standard digital C-button behavior expected by the original game.

自由カメラ有効時は、C-Stick がアナログカメラ入力として使われます。十字ボタン入力時は、オリジナルと同じデジタル C ボタン挙動として扱われます。

The anti-aliasing and resolution buttons are unavailable during stereo 3D because those modes conflict with the 3DS 3D display path.

アンチエイリアスと 800px モードは 3DS の立体表示経路と競合するため、立体 3D 有効時は関連ボタンを使用できません。

When debug mode is enabled from the lower-screen menu, these 3DS shortcuts become active:

下画面メニューから debug モードを有効化すると、次の 3DS ショートカットが有効になります：

| Shortcut | Function |
| --- | --- |
| `SELECT + ZL + ZR` | Open the level selector. |
| `START + ZL + ZR` in the final Bowser fight | Trigger the ending sequence and staff roll. |
| Double-tap `ZR` | Manually trigger Mario's death ragdoll action. |
| Hold `ZR` | Restore Mario's health while held. |
| Double-tap `ZL` | Drop Mario to low health. |
| Hold `ZL` | Lock Mario in place and repeatedly perform BLJ. |

| ショートカット | 機能 |
| --- | --- |
| `SELECT + ZL + ZR` | レベルセレクトを開きます。 |
| 最終クッパ戦で `START + ZL + ZR` | エンディングとスタッフロールへ進みます。 |
| `ZR` をダブルタップ | マリオの死亡ラグドール動作を手動で発動します。 |
| `ZR` を長押し | マリオの体力を回復します。 |
| `ZL` をダブルタップ | マリオの体力を 1 まで下げます。 |
| `ZL` を長押し | マリオの位置を固定し、BLJ を繰り返し実行します。 |

## 🔧Configuration and Save Data / 設定とセーブデータ

The main configuration file is `sm64config.txt`. It stores controls, Puppycam values, dynamic shadow preference, death ragdoll preference, hit ragdoll preference, and the 3DS anti-aliasing / wide-mode choices.

メイン設定ファイルは `sm64config.txt` です。操作設定、Puppycam の値、動的シャドウ設定、死亡ラグドール設定、受撃ラグドール設定、3DS のアンチエイリアス/ワイドモード選択を保存します。

For `.3dsx` builds, configuration and save data are stored beside the `.3dsx` file. For `.cia` builds, configuration and save data are stored at the SD card root.

`.3dsx` ビルドでは、設定ファイルとセーブデータは `.3dsx` ファイルと同じディレクトリに保存されます。`.cia` ビルドでは、設定ファイルとセーブデータは SD カードのルートに保存されます。

Useful configuration keys include `dynamic_shadows_enabled`, `death_ragdoll_enabled`, `hit_ragdoll_enabled`, `n3ds_anti_aliasing`, `n3ds_wide_mode`, `puppycam_sensitivity_x`, `puppycam_sensitivity_y`, `puppycam_invert_x`, `puppycam_invert_y`, `puppycam_stopping_speed`, `puppycam_centre_aggression`, and `puppycam_pan_amount`.

よく使う設定キーには、`dynamic_shadows_enabled`、`death_ragdoll_enabled`、`hit_ragdoll_enabled`、`n3ds_anti_aliasing`、`n3ds_wide_mode`、`puppycam_sensitivity_x`、`puppycam_sensitivity_y`、`puppycam_invert_x`、`puppycam_invert_y`、`puppycam_stopping_speed`、`puppycam_centre_aggression`、`puppycam_pan_amount` があります。

## 🏗️Building / ビルド

Place a matching baserom in the repository root before building, for example `baserom.us.z64` for the US version. Change `VERSION=us` to `eu`, `jp`, or `sh` when building another supported region.

ビルド前に、対応する baserom をリポジトリのルートに配置してください。たとえば US 版では `baserom.us.z64` を使用します。他の対応リージョンをビルドする場合は、`VERSION=us` を `eu`、`jp`、`sh` に変更してください。

From a clean release checkout, you do not need to run `extract_assets.py` manually. The first `make` automatically builds the required host tools and extracts ROM-backed assets into ignored local files before compiling.

クリーンな配布版チェックアウトから始める場合、`extract_assets.py` を手動で実行する必要はありません。最初の `make` が必要なホストツールのビルドと、ROM 由来アセットの抽出を自動で行ってからコンパイルします。

For a normal 3DS build environment, export `DEVKITPRO` and `DEVKITARM` first so the Makefile can find the devkitPro toolchain. If `bannertool` is not installed globally, also export `BANNERTOOL` to its full path. The examples below assume the common macOS/Linux install path:

通常の 3DS ビルド環境では、Makefile が devkitPro ツールチェーンを見つけられるように、先に `DEVKITPRO` と `DEVKITARM` を設定してください。`bannertool` がグローバルに入っていない場合は、そのフルパスを `BANNERTOOL` に設定してください。以下の例では、一般的な macOS/Linux のインストールパスを想定しています：

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=/opt/devkitpro/devkitARM
```

Recommended zero-start `.3dsx` build command on macOS:

macOS での「ゼロからの」推奨 `.3dsx` ビルドコマンド：

```sh
PATH="/opt/devkitpro/tools/bin:$PATH" \
DEVKITPRO=/opt/devkitpro \
DEVKITARM=/opt/devkitpro/devkitARM \
make -j$(sysctl -n hw.ncpu)
```

If you keep `bannertool` outside of `PATH`, append `BANNERTOOL=/path/to/bannertool` to the same command line.

`bannertool` を `PATH` 外に置いている場合は、同じコマンドラインに `BANNERTOOL=/path/to/bannertool` を追加してください。

After changing build flags, run `make clean` before rebuilding so the new flags are applied consistently.

ビルドフラグを変更した後は、新しいフラグを確実に反映するために、再ビルド前に `make clean` を実行してください。

Build a Homebrew Launcher `.3dsx` (output: `build/us_3ds/sm64.us.aev64u6.3dsx`):

Homebrew Launcher 用の `.3dsx` をビルドします（出力先：`build/us_3ds/sm64.us.aev64u6.3dsx`）：

```sh
PATH="/opt/devkitpro/tools/bin:$PATH" DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM make VERSION=us -j$(sysctl -n hw.ncpu)
```

Build an installable `.cia` (output: `build/us_3ds/sm64.us.aev64u6.cia`):

インストール可能な `.cia` をビルドします（出力先：`build/us_3ds/sm64.us.aev64u6.cia`）：

```sh
PATH="/opt/devkitpro/tools/bin:$PATH" DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM BANNERTOOL=/path/to/bannertool make VERSION=us cia -j$(sysctl -n hw.ncpu)
```

Clean previous outputs:

既存のビルド出力を削除します：

```sh
DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM make clean
```

Build with the legacy 3DS frame-skip option:

旧来の 3DS フレームスキップオプションを有効にしてビルドします：

```sh
PATH="/opt/devkitpro/tools/bin:$PATH" DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM make VERSION=us ENABLE_N3DS_FRAMESKIP=1 -j$(sysctl -n hw.ncpu)
```

Disable audio for a test build:

テストビルド用に音声を無効化します：

```sh
PATH="/opt/devkitpro/tools/bin:$PATH" DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM make VERSION=us DISABLE_AUDIO=1 -j$(sysctl -n hw.ncpu)
```

Use the PC port's reference RSP audio path:

PC ポート版の参照 RSP 音声経路を使用します：

```sh
PATH="/opt/devkitpro/tools/bin:$PATH" DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM make VERSION=us FORCE_REFERENCE_RSPA=1 -j$(sysctl -n hw.ncpu)
```

Disable the enhanced RSP audio performance path while keeping the 3DS mixer:

3DS ミキサーを維持したまま、強化 RSP 音声の高速経路を無効化します：

```sh
PATH="/opt/devkitpro/tools/bin:$PATH" DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM make VERSION=us DISABLE_ENHANCED_RSPA=1 -j$(sysctl -n hw.ncpu)
```

Use accurate audio math where supported:

対応している音声実装で、より正確な演算挙動を使用します：

```sh
PATH="/opt/devkitpro/tools/bin:$PATH" DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM make VERSION=us AUDIO_USE_ACCURATE_MATH=1 -j$(sysctl -n hw.ncpu)
```

The 3DS toolchain expects devkitPro/devkitARM and the usual 3DS build tools such as `3dsxtool`, `smdhtool`, `tex3ds`, and `makerom`. Docker, Linux/WSL, and MSYS2 setups are all viable if the devkitPro environment is installed correctly.

3DS ツールチェーンには devkitPro/devkitARM と、`3dsxtool`、`smdhtool`、`tex3ds`、`makerom` などの一般的な 3DS ビルドツールが必要です。devkitPro 環境が正しく設定されていれば、Docker、Linux/WSL、MSYS2 の環境でもビルドできます。


## 📦Package Assets / パッケージ用アセット

To change the `.3dsx` icon, replace `3ds/icon.png` before building. The build process generates the SMDH file used by the final `.3dsx`.

`.3dsx` アイコンを変更する場合は、ビルド前に `3ds/icon.png` を置き換えてください。ビルド処理によって、最終的な `.3dsx` で使用される SMDH ファイルが生成されます。

To change CIA presentation assets, update `3ds/icon.png` and `3ds/banner.png`, then regenerate the CIA icon and banner assets with `bannertool`.

CIA の表示用アセットを変更する場合は、`3ds/icon.png` と `3ds/banner.png` を更新し、`bannertool` で CIA アイコンとバナーアセットを再生成してください。

If `bannertool` is not installed globally, point the `BANNERTOOL` environment variable at any local `bannertool` executable before building `.cia`.

`bannertool` がグローバルにインストールされていない場合は、`.cia` をビルドする前に、任意のローカル `bannertool` 実行ファイルを `BANNERTOOL` 環境変数で指定してください。

## 🗺️Minimap Tooling / ミニマップ用ツール

The repository includes internal tools used to draft, calibrate, and regenerate minimap resources. 

このリポジトリには、ミニマップ素材の下書き、調整、再生成に使う内部ツールが含まれています。これらのツールは主にプロジェクトのアート作業やレイアウト調整のためのものです。

### Python prerequisites / Python 依存関係

The minimap helper scripts use Python 3. The texture text generators also require Pillow:

ミニマップ補助スクリプトは Python 3 を使用します。テキストテクスチャ生成スクリプトには Pillow も必要です：

```sh
python3 -m pip install pillow
```

### Collision draft maps / コリジョン下書きマップ

Generate a rough collision-based draft for one level area:

単一のレベルエリアについて、コリジョンを基にした大まかな下書きマップを生成します：

```sh
python3 tools/minimap_extract_collision.py --level bob --area 1 --grid --fit
```

This writes an image like `minimap_work/collision/bob_1.png`.

`minimap_work/collision/bob_1.png` のような画像が出力されます。

Generate collision drafts for every level area:

すべてのレベルエリアについて、コリジョン下書きマップを一括生成します：

```sh
python3 tools/minimap_extract_collision.py --all --grid --fit
```

### Textured draft maps / テクスチャ合成下書きマップ

Generate a composed textured draft for one area using the project’s usual composition layers:

プロジェクトで通常使用する合成レイヤーを使って、単一エリアのテクスチャ付き下書きマップを生成します：

```sh
python3 tools/minimap_extract_textured.py --level bob --area 1 --compose-map --grid --mark-mario-start
```

This writes an image like `minimap_work/textured/bob_area1_textured.png`.

`minimap_work/textured/bob_area1_textured.png` のような画像が出力されます。

Generate textured drafts for every area:

すべてのエリアについて、テクスチャ付き下書きマップを一括生成します：

```sh
python3 tools/minimap_extract_textured.py --all --compose-map
```

Useful flags for textured drafts include:

テクスチャ下書きマップでよく使う追加フラグ：

- `--include-walls`: include near-vertical geometry for debugging silhouettes.
- `--include-water-movtex`: also overlay water movtex regions.
- `--no-screen-anchor-calibration`: disable alignment against captured 3DS screenshots.
- `--output` or `--output-dir`: send renders to a custom path.

- `--include-walls`：ほぼ垂直なジオメトリを含め、輪郭のデバッグに使います。
- `--include-water-movtex`：水面 movtex 領域も重ねて描画します。
- `--no-screen-anchor-calibration`：3DS 実機スクリーンショットとの位置合わせ補正を無効化します。
- `--output` または `--output-dir`：結果を任意のパスへ出力します。

### Minimap text textures / ミニマップ用テキストテクスチャ

Regenerate title-related minimap textures such as `PRESS START` and the copyright texture:

`PRESS START` やコピーライト表示など、タイトル関連のミニマップ用テクスチャを再生成します：

```sh
python3 tools/generate_minimap_title_textures.py
```

Regenerate BGM-title textures used on the lower screen:

下画面で使用する BGM タイトルテクスチャを再生成します：

```sh
python3 tools/generate_minimap_music_textures.py
```

### Rebuild converted minimap textures / 変換済みミニマップテクスチャの再ビルド

The 3DS build converts `src/minimap/textures/*.png` into `.t3s`, `.t3x`, and generated headers automatically through `tex3ds`. In other words, once the PNG sources are updated, a normal `make` is enough to rebuild the converted minimap texture assets.

3DS ビルドでは、`src/minimap/textures/*.png` が `tex3ds` によって `.t3s`、`.t3x`、生成ヘッダーへ自動変換されます。つまり PNG ソースを更新した後は、通常どおり `make` を実行するだけで、変換済みのミニマップテクスチャアセットを再ビルドできます。

## 🌲Project Structure / プロジェクト構成

The repository follows the standard Super Mario 64 decompilation layout, with 3DS-specific platform code and project enhancements layered on top.

このリポジトリは標準的な『スーパーマリオ64』デコンパイルプロジェクトの構成に従い、その上に 3DS 固有のプラットフォームコードと本プロジェクトの拡張機能を重ねています。

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

## 🫡Credits / クレジット

This project builds on the Super Mario 64 decompilation and 3DS port work by the broader SM64 port community.

本プロジェクトは、『スーパーマリオ64』デコンパイルプロジェクトと、SM64 port コミュニティによる 3DS ポート作業を基盤としています。

Credits go to Gericom for the `sm64_3ds` port lineage that this 3DS flavor is based on.

本 3DS 版の基礎となった `sm64_3ds` ポート系統について、Gericom に感謝します。

The 3DS branch is based on the Refresh 11-era port work and keeps the original 3DS-specific improvements such as stereo 3D, multi-threaded audio, enhanced RSPA, SMDH support, and 3DS build packaging.

この 3DS ブランチは Refresh 11 時期のポート作業をベースにしており、立体 3D、マルチスレッド音声、強化 RSPA、SMDH 対応、3DS ビルドパッケージングなど、当時の 3DS 固有の改善を維持しています。

Minimap model and map resource credits include alecpike for Bob-omb Battlefield, Turtle Boy for Lethal Lava Land and Whomp's Fortress, SlyP54 for Castle Grounds, Bruz for Castle Interior, and SM64DS minimaps in Mario wiki.

ミニマップ用モデルおよびマップ素材については、Bob-omb Battlefield の alecpike、Lethal Lava Land と Whomp's Fortress の Turtle Boy、Castle Grounds の SlyP54、Castle Interior の Bruz、そして Mario Wiki の SM64DS ミニマップ素材に感謝します。

This is an unofficial fan project.
Super Mario 64 and related assets are owned by Nintendo.

これは非公式のファンプロジェクトです。
『スーパーマリオ64』および関連するキャラクター、商標、アセットは任天堂に帰属します。
