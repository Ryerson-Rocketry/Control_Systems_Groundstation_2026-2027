# Sprites

Drop a PNG here named after a sprite and rebuild. It replaces the pink
placeholder box wherever that sprite is drawn. Images are scaled to fit
with nearest-neighbour scaling (crisp pixels) and keep their aspect ratio,
so use a transparent background.

| File          | Where it shows                         | Drawn at (px) |
|---------------|----------------------------------------|---------------|
| `background.png` | Behind the whole dashboard (Peach's castle), scaled to cover the window; without it a pixel sunset is drawn | any |
| `logo.png`    | Header, top left                       | 36 × 36       |
| `runner.png`  | Pac-Man, just left of the current state's ghost on the phase track | 28 × 28 |
| `avatar.png`  | Photo of the rocket, tall column on the left of the Game & Watch LCD | 28 × 140 |
| `marker.png`  | Inside the red rocket disc on the GPS tracker | 16 × 16 |
| `tracker_head.png` | Mascot head on top of the GPS tracker's orange plaque | 26 × 26 |
| `tracker_icon.png` | Orange box, top-right of the GPS tracker | 26 × 26 |
| `tracker_buddy.png` | Bottom-left corner of the GPS tracker | 28 × 28 |
| `kirby_ground_0…9.png` | Kirby walking on the ground: on the pad before launch and after landing (12 fps) | 36 × 36 |
| `kirby_float_0…2.png` | Kirby puffed up, floating up the altitude scene (10 fps) | 36 × 36 |
| `kirby_parasol_0…19.png` | Kirby under his parasol on the way down (10 fps, plays forwards then back) | 34 × 50 |
| `ghost_red.png`, `ghost_pink.png`, `ghost_cyan.png`, `ghost_orange.png` | Above each state on the phase track (IDLE → APOGEE) | 28 × 28 |
| `ghost_dead.png` | Replaces a state's ghost once that state has passed | 28 × 28 |
| `bird.png`    | Flies at the head of each graph line while in flight | 32 × 18 |
| `dog.png`     | Pops up on the graphs at each phase change | 24 × 20 |

Animations are numbered frames: `name_0.png`, `name_1.png`, ...

Names are defined in `src/ui/include/sprites.h`.
