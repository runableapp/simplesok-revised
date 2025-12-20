# Building Packages for Simple Sokoban

This document explains how to build different package formats for Simple Sokoban.

## Prerequisites

First, build the source distribution tarball:

```bash
cd simplesok-1.0.7
autoreconf -fi
./configure
make dist-xz
```

This creates `simplesok-1.0.7.tar.xz` in the current directory.

## Standalone Package

**Location**: `pkgbuild/standalone/`

**Build**:
```bash
make standalone
```

**Result**: Creates `pkgbuild/standalone/simplesok-1.0.7-standalone/` directory containing:
- `simplesok` (executable)
- `skins/` (directory with PNG skin files)
- `README.txt` (usage instructions)

**Usage**: Move the directory anywhere (e.g., `/opt/games/simplesok/`) and run `./simplesok`

## RPM Package (openSUSE/RHEL/CentOS)

**Location**: `pkgbuild/rpm/`

**Files**:
- `simplesok.spec.in` - Template (processed by configure)
- `simplesok.spec` - Generated spec file

**Build**:
```bash
# 1. Generate the spec file (done automatically by ./configure)
./configure

# 2. Copy source tarball to rpmbuild SOURCES
cp simplesok-1.0.7.tar.xz ~/rpmbuild/SOURCES/

# 3. Build RPM package
rpmbuild -ba pkgbuild/rpm/simplesok.spec
```

**Result**: RPM packages in `~/rpmbuild/RPMS/` and `~/rpmbuild/SRPMS/`

**Note**: Requires `rpmbuild` and development packages installed.

## RPM Package (Fedora)

**Location**: `pkgbuild/rpm_fedora/`

**Files**:
- `simplesok.spec.in` - Template (processed by configure)
- `simplesok.spec` - Generated spec file

**Build**:
```bash
# 1. Generate the spec file (done automatically by ./configure)
./configure

# 2. Copy source tarball to rpmbuild SOURCES
cp simplesok-1.0.7.tar.xz ~/rpmbuild/SOURCES/

# 3. Build RPM package
rpmbuild -ba pkgbuild/rpm_fedora/simplesok.spec
```

**Result**: RPM packages in `~/rpmbuild/RPMS/` and `~/rpmbuild/SRPMS/`

**Differences from standard RPM**:
- Uses Fedora-specific macros (`%autosetup`, `%{make_build}`)
- Includes desktop file installation
- Optional libcurl support via `--without libcurl`

## Debian Package (.deb)

**Location**: `pkgbuild/deb/`

**Files**:
- `simplesok.dsc` - Debian source package description
- `simplesok_1.0.2-1.debian.tar.xz` - Debian packaging files (old version)

**Build**:
```bash
# 1. Install build dependencies
sudo apt install build-essential devscripts debhelper

# 2. Create source package structure
mkdir -p ../simplesok-deb
cd ../simplesok-deb
tar xf ../simplesok-1.0.7/simplesok-1.0.7.tar.xz
cd simplesok-1.0.7

# 3. Create debian/ directory with packaging files
# (You'll need to create debian/control, debian/rules, etc.)

# 4. Build package
debuild -us -uc
```

**Result**: `.deb` package in parent directory

**Note**: The debian packaging files in `pkgbuild/deb/` appear to be from an older version (1.0.2). You may need to update them or create new ones for version 1.0.7.

## Flatpak Package

**Location**: `pkgbuild/flatpak/`

**Files**:
- `io.osdn.simplesok.desktop` - Desktop entry file
- `io.osdn.simplesok.metainfo.xml` - AppStream metadata
- `io.osdn.simplesok.svg` - Application icon

**Build**:
```bash
# 1. Install Flatpak and build tools
sudo apt install flatpak flatpak-builder

# 2. Create manifest file (flatpak manifest.json)
# You'll need to create a manifest.json file that references these files

# 3. Build Flatpak
flatpak-builder --repo=repo build-dir manifest.json

# 4. Build bundle
flatpak build-bundle repo simplesok.flatpak fr.mateusz.simplesok
```

**Result**: `simplesok.flatpak` bundle file

**Note**: These are just the metadata files. You'll need to create a complete Flatpak manifest.json that includes:
- Build dependencies (SDL2, SDL2_image, zlib, libcurl)
- Build instructions
- Runtime dependencies

## Summary

| Package Type | Command | Output Location |
|-------------|---------|----------------|
| **Standalone** | `make standalone` | `pkgbuild/standalone/simplesok-1.0.7-standalone/` |
| **RPM** | `rpmbuild -ba pkgbuild/rpm/simplesok.spec` | `~/rpmbuild/RPMS/` |
| **RPM (Fedora)** | `rpmbuild -ba pkgbuild/rpm_fedora/simplesok.spec` | `~/rpmbuild/RPMS/` |
| **Debian** | `debuild -us -uc` | Parent directory (requires debian/ files) |
| **Flatpak** | `flatpak-builder` | Requires manifest.json |

## Common Requirements

All package builds require:
- Source tarball: `simplesok-1.0.7.tar.xz` (created with `make dist-xz`)
- Build dependencies installed (SDL2, SDL2_image, zlib, libcurl development packages)
- Configure script run: `./configure` (generates spec files from `.in` templates)

