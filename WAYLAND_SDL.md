# Wayland + SDL2 Hardware Rendering Guide

## Why the Error Occurred

Error message:
```
X Error of failed request: BadValue (integer parameter out of range for operation)
Major opcode of failed request: 149 (GLX)
```

This means SDL2 tried to use **GLX** (X11's OpenGL interface) instead of **EGL** (Wayland's OpenGL interface).

### The Problem

1. **GLX vs EGL:**
   - **GLX** = X11's OpenGL extension (doesn't work on native Wayland)
   - **EGL** = Wayland's OpenGL interface (required for hardware acceleration on Wayland)

2. **Why it happened:**
   - SDL2 detected it could use hardware acceleration
   - But it defaulted to GLX (X11) instead of EGL (Wayland)
   - This happens when SDL2's backend detection doesn't properly identify Wayland

3. **Software rendering works because:**
   - It doesn't use OpenGL at all
   - It renders directly to a software buffer
   - No GLX/EGL dependency

## What's Needed for Wayland + Hardware Rendering

✅ **You already have everything needed:**
- SDL2 2.30.0 (supports Wayland + EGL)
- EGL libraries installed (`libegl1-mesa`, `libegl-dev`)
- Wayland compositor (your desktop environment)

## How to Enable Hardware Rendering on Wayland

### Method 1: Force SDL to Use Wayland Backend (Recommended)

Set the SDL video driver environment variable:

```bash
SDL_VIDEODRIVER=wayland ./simplesok
```

This forces SDL2 to use the Wayland backend, which will automatically use EGL for hardware acceleration.

### Method 2: Rebuild Without Software Renderer + Force Wayland

```bash
cd simplesok-1.0.7
./configure  # Don't use --with-software-renderer
make
SDL_VIDEODRIVER=wayland ./simplesok
```

### Method 3: Create a Wrapper Script

Create a script to always use Wayland:

```bash
#!/bin/bash
export SDL_VIDEODRIVER=wayland
exec /path/to/simplesok "$@"
```

### Method 4: Set Environment Variable Permanently

Add to your `~/.bashrc` or `~/.profile`:

```bash
export SDL_VIDEODRIVER=wayland
```

## Why SDL2 Doesn't Auto-Detect Wayland

SDL2 should auto-detect Wayland, but sometimes:
1. **XWayland is present** - SDL2 might prefer X11/XWayland over native Wayland
2. **Environment variables** - `DISPLAY` might be set, making SDL2 think X11 is available
3. **Backend priority** - SDL2 tries backends in order, and X11 might be checked first

## Verify Hardware Acceleration is Working

After running with `SDL_VIDEODRIVER=wayland`, check:

```bash
# Check if using EGL (should show wayland-egl)
ldd simplesok | grep egl

# Check SDL video driver in use
SDL_VIDEODRIVER=wayland ./simplesok
# Then check SDL logs or use SDL_VIDEODRIVER environment variable
```

## Summary

- **Software rendering** = Works everywhere, no OpenGL needed
- **Hardware rendering on Wayland** = Requires EGL (which you have) + forcing Wayland backend
- **The fix** = Use `SDL_VIDEODRIVER=wayland` environment variable


