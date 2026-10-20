# Back-view cleanup v2

- 21 final attachments. The two upper hair loops are merged into the corresponding side locks because their crossing strokes cannot be isolated naturally. Updated side-lock pivots are recorded in layout.json.
- The complete existing rear-hair, coat, boots and tail paintings replace incomplete source fragments. No original fragments are composited over plain geometry or colour extrusion.
- Hair is split along strand curves. The garment snowflakes and star motifs are solely on coat_back.
- Fur hem is isolated from the complete coat using its continuous fur region plus the attachment contour. The body has no residual fur-outline ring.
- Original headband, whale ornament, bows and pompoms are separately cleaned. Thin hanging cords and the pompom attachment loops are redrawn. Side fins retain the original painted design, with matching mirrored art and smooth roots.
- All 21 PNGs have exactly 2 px transparent margins. Canvas and top-left placement remain in layout.json.
- Verified in dark and checker sheets, white/dark assembled previews, and 200% per-part previews under inspection/final-*-2x.png.

Limitations: completed hidden areas use the previously generated painting and therefore are not pixel-identical to the source sheet. The upper hair loops no longer have separate bones. Rig deformation and motion ranges still require verification after Spine import.
