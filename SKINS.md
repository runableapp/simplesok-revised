# Adding Custom Skins to Simple Sokoban

Simple Sokoban supports custom skins (visual themes) for the game. Skins are image files containing sprite maps in the "Common skin format".

## Skin File Format

- **Format**: PNG or BMP image file
- **Layout**: 4 columns × minimum 6 rows (can be more, e.g., 4×10)
- **Tilesize**: Automatically calculated as (image_width / 4)
- **Transparency**: Supported (RGBA PNG recommended for smooth animations)

## Skin Tile Layout

The skin uses a 4-column grid with the following tile arrangement:

| Column 0 | Column 1 | Column 2 | Column 3 |
|----------|----------|----------|----------|
| **Row 0** (tiles 00-03): Floor, Pusher, Box, (unused) |
| **Row 1** (tiles 04-07): Goal, Pusher on Goal, Box on Goal, (unused) |
| **Row 2** (tiles 08-11): Wall corner, Wall horizontal, Wall cap, Background |
| **Row 3** (tiles 12-15): Wall vertical, Wall island, Copyright, Settings |
| **Row 4** (tiles 16-19): Player ↑, Player ←, Player ↓, Player → |
| **Row 5** (tiles 20-23): Player ↑ on Goal, Player ← on Goal, Player ↓ on Goal, Player → on Goal |
| **Rows 6+** (tiles 24+): Animation frames (optional) |

**Note**: 
- Tiles 01, 05, 14, 15, and 24+ are not used by Simple Sokoban
- Tiles 17, 18, 19, 21, 22, 23 can be transparent for rotation animation
- Tile 10 (Wall cap) is optional - if transparent, tile 08 is used instead

## Where to Place Skins

Simple Sokoban searches for skins in these directories **in order** (first match wins):

1. **User preferences directory**: `~/.local/share/simplesok/skins/`
   - This is the **recommended location** for user-installed skins
   - Uses SDL's `SDL_GetPrefPath()` which returns this path on Linux
   - Persists across application updates
   - User-specific (each user has their own skins)

2. **Application directory**: `./skins/` (where simplesok executable is located)
   - Good for portable installations
   - Skins travel with the executable

3. **Package data directory**: `/usr/share/simplesok/skins/` (or `PKGDATADIR/skins/`)
   - System-wide installation location
   - Used when installed via package manager

4. **System directory**: `/usr/share/simplesok/skins/`
   - Fallback system location

## Steps to Add a Skin

1. **Create or obtain a skin image file** (PNG or BMP format)
   - Must be 4 columns wide
   - Height must be at least 6 tiles (tilesize × 6)
   - Recommended: Use PNG with transparency for smooth animations

2. **Place it in one of the skin directories above**
   ```bash
   # Recommended: User directory
   mkdir -p ~/.local/share/simplesok/skins/
   cp myskin.png ~/.local/share/simplesok/skins/
   ```

3. **The skin name is the filename without extension**
   - Example: `myskin.png` → use with `--skin=myskin`
   - Example: `boxworld.png` → use with `--skin=boxworld`

4. **Run simplesok with your skin:**
   ```bash
   ./simplesok --skin=myskin
   ```

## List Available Skins

To see all available skins and their locations:

```bash
./simplesok --skinlist
```

## Default Skin

If no skin is specified, `antique3` is used by default.

## Example Skin Sizes

From included skins:
- `antique3.png`: 200×300 pixels (50×50 tiles, 4×6 grid)
- `yoshi.png`: 128×192 pixels (32×32 tiles, 4×6 grid)
- `ibm-cga.png`: 80×120 pixels (20×20 tiles, 4×6 grid)
- `heavymetal2.png`: 200×350 pixels (50×50 tiles, 4×7 grid)
- `mikrus.png`: 32×48 pixels (8×8 tiles, 4×6 grid)
- `boxworld.png`: 240×600 pixels (60×60 tiles, 4×10 grid)

## Animated Skins

Simple Sokoban supports animated pushers (player character) for smooth movement animations. For best results, use skins with transparent backgrounds.

### Animated Pusher Types

1. **Static pusher**: Uses tile 01 only (no animation)
2. **Directional pusher**: Uses tiles 16-19 (different sprite per direction)
3. **Rotating pusher**: Uses tile 16 (or 20) with transparent tiles 17-19 (or 21-23). Simple Sokoban will rotate the sprite smoothly.

### For Smooth Animations

- **Use transparent backgrounds** on moving tiles (pusher, box)
- This enables the "chameleon effect" for seamless movement
- Wall tiles can also have transparency (floor is drawn underneath)
- Tiles 17, 18, 19, 21, 22, 23 can be completely transparent to enable rotation-based animation

### Example Animated Skin

The Boxworld skin (4×10 grid, 240×600 pixels) is an example of an animated skin:

```bash
./simplesok --skin=boxworld
```

Download: https://usercontent.one/wp/sokoban.dk/wp-content/uploads/2016/05/Boxworld-2016-4x10.png

## Technical Notes

- The skin width must be exactly divisible by 4 (4 columns)
- The height should be divisible by the tilesize (width/4), but the code can handle variations
- The code expects at least 6 rows of tiles
- More rows are fine (e.g., 4×10 for Boxworld)
- More columns might work, but simplesok makes a best-effort guess at geometry

## References

For detailed skin format documentation, see:
- Official format specification: https://mateusz.fr/simplesok/skinfmt/
- Common skin format (cross-program standard)

