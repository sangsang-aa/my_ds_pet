# Canvas migration (384x456 -> 384x512)

The desktop pet now uses a **384x512 (3:4)** canvas so generated video frames can be
downscaled 0.8x from 480x640 without cropping.

`migrate_canvas.py` top-pads existing actions, so the character keeps its original
feet position and subject size:

| input | padding | result |
| --- | --- | --- |
| 384x456 | +56 px on top | 384x512 |
| 384x384 | +128 px on top | 384x512 |
| 384x512 | skip | idempotent |

`bounds.txt` rows are shifted by the same padding; if the row count does not match
the frame count, bounds are regenerated from the alpha channel.

## Usage

```powershell
python tools/migrate_canvas.py --dry-run   # report only
python tools/migrate_canvas.py             # write changes
```

Skipped directories: `emoji/`, `tmp/`, `视频文件/`, anything ending in `.bak`.

## Baseline

The legacy `idle` frame has its feet at `y=460` on a 512-high canvas (89.8%).
New clips should be anchored to **460**, not 0.94H (481), so switching actions
does not make the pet jump.
