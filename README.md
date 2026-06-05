# Super Mario 64 Nintendo 3DS Port

This repo does **not** include all assets necessary for compiling the game.
A prior copy of the game is required to extract the assets.

## Changes vs. Vanilla 3DS Port

 - Based off [Refresh 11](https://github.com/sm64-port/sm64-port/commit/9214dddabcce4723d9b6cda2ebccbac209f6447d)
 - Stereo 3D support; use the mini-menu to switch between 3D (400px) and 800px modes (800PX/AA disabled in 3D mode)
 - Multi-threaded; audio thread runs on Core 1 on O3DS and Core 2 on N3DS; needs [Luma v10.1.1](https://github.com/LumaTeam/Luma3DS/releases) or higher
 - Naïve frame-skip if frame takes longer than 33.3ms (1 / 30 FPS) to render. This option is no longer very useful, but is still available for posterity.
     - Enable by building with `ENABLE_N3DS_FRAMESKIP=1`
 - Enhanced RSP Audio emulation performance
     - Disable some minor performance enhancements by building with `DISABLE_ENHANCED_RSPA=1`. This should not impact quality, but may be useful for debugging.
     - Use the PC port's original audio emulation by building with `FORCE_REFERENCE_RSPA=1`. This should not impact quality, but may be useful for debugging, and will override `DISABLE_ENHANCED_RSPA`.
     - By default, 3DS audio uses some inaccurate math to increase performance with no perceptible loss in quality. To disable this, build with `AUDIO_USE_ACCURATE_MATH=1`. This may have no effect depending on the audio implementation being used; for example, Reference RSPA ignores this flag.
 - Configurable controls via `sm64config.txt`
     - Use [this](https://codepen.io/benoitcaron/full/abNZrbP) online editor from [BenoitCaron](https://github.com/BenoitCaron).
 - GFX_POOL_SIZE [fix](https://github.com/aboood40091/sm64-port/commit/6ae4f4687ed234291ac1e572b75d65191ca9f364) (support 60 FPS on 32bit platforms)
 - Mini-menu (tap touch-screen to trigger)
     - Enable/disable AA
     - Enable/disable 800px mode
     - Exit the game
 - Touch-screen camera controls similar to SM64DS
 - Support injection of [SMDH](https://www.3dbrew.org/wiki/SMDH) file into the .3dsx
     - Change the `3ds/icon.png` in the base of this repository before building.
 - To change the CIA banner, modify the `3ds/icon.png` and `3ds/banner.png` and use [bannertool](https://github.com/Steveice10/bannertool/releases/tag/1.2.0) to generate new `icon.icn` and `banner.bnr` respectively.
 - Patches updated for 3DS:
     - [60 FPS](enhancements/60fps.patch)
     - [Puppycam](enhancements/puppycam.patch)
     - [Show FPS](enhancements/fps.patch)
 - Choice to disable audio at build-time; add build flag `DISABLE_AUDIO=1`
 - Experimental Mini-Map; bottom screen displays an overview of the current level

## Building

After building, either install the `.cia` if you made one, or copy over the `sm64.us.f3dex2e.3dsx` into the `/3ds` directory on your SD card and load via [The Homebrew Launcher](https://smealum.github.io/3ds/).

After making any changes to your build flags, it is important to run `make clean` before rebuilding, or else the new flags will not be applied.

  - [Docker](#docker)
  - [Linux / WSL (Ubuntu 18.04 or higher)](#linux--wsl-ubuntu)
  - [Windows (MSYS2)](#windows-msys2)

Visit the [wiki](https://github.com/mkst/sm64-port/wiki) for information.

### Docker

The following assumes a basic understanding of [Docker](https://www.docker.com/); if you do not belong to the `docker` group, prefix those commands with `sudo`.

As of recently, using Docker Desktop for Windows and WSL2 has become the easiest way to build when using WSL. For more information, look [here.](https://docs.docker.com/desktop/wsl/)

**Clone Repository:**

```sh
git clone https://github.com/mkst/sm64-port.git
```

**Navigate into freshly checked out repo:**

```sh
cd sm64-port
```

**Copy in baserom.XX.z64:**

```sh
cp /path/to/your/baserom.us.z64 ./ # change 'us' to 'eu', 'jp' or 'sh' as appropriate
```

**Build with pre-baked image:**

Change `VERSION=us` if applicable.
```sh
docker run --rm -v $(pwd):/sm64 markstreet/sm64:3ds make --jobs 4 VERSION=us cia 
```

### Linux / WSL (Ubuntu)

As of recently, using Docker Desktop for Windows and WSL2 has become the easiest way to build when using WSL. For more information, look [here.](https://docs.docker.com/desktop/wsl/)

Tested successfully on **Ubuntu 18.04** and **20.04**. Does not work on **16.04**.

```sh
sudo su -

apt-get update && \
    apt-get install -y \
        binutils-mips-linux-gnu \
        bsdmainutils \
        build-essential \
        libaudiofile-dev \
        pkg-config \
        python3 \
        wget \
        unzip \
        zlib1g-dev

wget https://github.com/devkitPro/pacman/releases/download/v1.0.2/devkitpro-pacman.amd64.deb \
  -O devkitpro.deb && \
  echo ebc9f199da9a685e5264c87578efe29309d5d90f44f99f3dad9dcd96323fece3 devkitpro.deb | sha256sum --check && \
  apt install -y ./devkitpro.deb && \
  rm devkitpro.deb

dkp-pacman -Syu 3ds-dev --noconfirm

wget https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.17/makerom-v0.17-ubuntu_x86_64.zip \
  -O makerom.zip && \
  echo 976c17a78617e157083a8e342836d35c47a45940f9d0209ee8fd210a81ba7bc0  makerom.zip | sha256sum --check && \
  unzip -d /opt/devkitpro/tools/bin/ makerom.zip && \
  chmod +x /opt/devkitpro/tools/bin/makerom && \
  rm makerom.zip

exit

cd

git clone https://github.com/mkst/sm64-port.git

cd sm64-port

# go and copy the baserom to c:\temp (create that directory in Windows Explorer)
cp /mnt/c/temp/baserom.us.z64 ./

sudo chmod 644 ./baserom.us.z64

export PATH="/opt/devkitpro/tools/bin/:~/sm64-port/tools:${PATH}"
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=/opt/devkitpro/devkitARM
export DEVKITPPC=/opt/devkitpro/devkitPPC

make --jobs 4
make cia # optional if you want a .cia
```

### Windows (MSYS2)

WSL is the preferred route, but you can also use MSYS2 (MINGW64) to compile.

For each instruction copy and paste the contents into the **MING64** console.

**Get MSYS2:**

Navigate to https://www.msys2.org/ and download the installer.

**Install and Run MINGW64:**

```
Next, Next, Next, Finish (keep the box checked to "Run MSYS 64bit now").
```

**Add keyserver for package validation:**

```sh
pacman-key --recv BC26F752D25B92CE272E0F44F7FD5492264BB9D0 --keyserver keyserver.ubuntu.com
pacman-key --lsign BC26F752D25B92CE272E0F44F7FD5492264BB9D0
```

**Add DevKitPro keyring:**

```sh
pacman -U --noconfirm https://downloads.devkitpro.org/devkitpro-keyring.pkg.tar.xz
```

**Add DevKitPro package repositories:**

```sh
cat <<EOF >> /etc/pacman.conf
[dkp-libs]
Server = https://downloads.devkitpro.org/packages
[dkp-windows]
Server = https://downloads.devkitpro.org/packages/windows
EOF
```

**Update dependencies:**

```sh
pacman -Syu --noconfirm
```

MINGW64 may close itself when done, if it does, find `MSYS2 MinGW 64bit` in your Start Menu and open again.

**Install Dependencies:**

```sh
pacman -S 3ds-dev git make python3 mingw-w64-x86_64-gcc unzip --noconfirm
```

**Download makerom:**

```sh
wget https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.17/makerom-v0.17-win_x86_64.zip
```

**Extract makerom:**

```sh
unzip -d /opt/devkitpro/tools/bin/ makerom-v0.17-win_x86_64.zip
```

**Setup Environment Variables:**

```sh
export PATH="$PATH:/opt/devkitpro/tools/bin" && echo "OK!"
export DEVKITPRO=/opt/devkitpro && echo "OK!"
export DEVKITARM=/opt/devkitpro/devkitARM && echo "OK!"
export DEVKITPPC=/opt/devkitpro/devkitPPC && echo "OK!"
```

**Clone Repository:**

```sh
git clone https://github.com/mkst/sm64-port.git
```

**Navigate into freshly checked out repo:**

```sh
cd sm64-port && echo "OK!"
```

**Copy in baserom.XX.z64:**

This assumes that you have create the directory `c:\temp` via Windows Explorer and copied the Super Mario 64 `baserom.XX.z64` to it.
```sh
cp /c/temp/baserom.us.z64 ./ && echo "OK!" # change 'us' to 'eu', 'jp' or 'sh' as appropriate
```

**Compile 3dsx:**

```sh
make VERSION=us --jobs 4 # Change 'us' to 'eu', 'jp' or 'sh' as appropriate.
```

**Create .cia:**

```sh
make VERSION=us cia # Change 'us' to 'eu', 'jp' or 'sh' as appropriate.
```

### Other Operating Systems

TBD; feel free to submit a PR.

## Project Structure

```
sm64
├── actors: object behaviors, geo layout, and display lists
├── asm: handwritten assembly code, rom header
│   └── non_matchings: asm for non-matching sections
├── assets: animation and demo data
│   ├── anims: animation data
│   └── demos: demo data
├── bin: C files for ordering display lists and textures
├── build: output directory
├── data: behavior scripts, misc. data
├── doxygen: documentation infrastructure
├── enhancements: example source modifications
├── include: header files
├── levels: level scripts, geo layout, and display lists
├── lib: SDK library code
├── rsp: audio and Fast3D RSP assembly code
├── sound: sequences, sound samples, and sound banks
├── src: C source code for game
│   ├── audio: audio code
│   ├── buffers: stacks, heaps, and task buffers
│   ├── engine: script processing engines and utils
│   ├── game: behaviors and rest of game source
│   ├── goddard: Mario intro screen
│   ├── menu: title screen and file, act, and debug level selection menus
│   └── pc: port code, audio and video renderer
├── text: dialog, level names, act names
├── textures: skybox and generic texture data
└── tools: build tools
```

## Credits

 - Credits go to [Gericom](https://github.com/Gericom) for the [sm64_3ds](https://github.com/sm64-port/sm64_3ds) port that this flavour is based off.
 - Minimap models:
   - [alecpike](https://www.models-resource.com/submitter/alecpike/); BOB
   - [Turtle Boy](https://www.models-resource.com/submitter/Turtle+Boy/); LLL; WF
   - SlyP54; Castle Grounds
   - Bruz; Castle Interior

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.
