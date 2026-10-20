# Side view review

- 28 transparent PNG parts. Every part has exactly 2 px of empty transparent margin; zero RGB data in fully transparent pixels; dimensions match layout.json.
- White, dark and checkerboard assemblies reviewed. All 28 parts reviewed on the seven paired white/dark pages in this folder.
- Replaced the coat torso, collar and headband with complete illustrations from one new Sudorelay gpt-image-2.5-sunburst API edit. Prompt: ../../prompts/side-repair-clothing.txt.
- Reused clean complete painted face, posterior hair, hand and tail from the earlier generated restoration. The coat no longer has a duplicate sleeve or snowflake sleeve emblem. Posterior hair no longer uses an oval color fill. The blank face has one ear and no eye or mouth marks.
- Original small parts were cleaned against the original reference. Recovered the eye and snowflake from original pixels to remove owner-mask holes. The back pompom uses the complete painted pompom source to replace its formerly missing half; removed coat-fabric fragments between the cords.
- Face draw order is now below posterior hair. Posterior hair leaves space for the visible ear. Collar meets the jaw, torso overlaps its fur hem, tail roots overlap, and the ahoge meets the headband.

Limitations: source-derived small accessories retain their original low resolution and may look pixelated at several-times enlargement. Hair and fin roots intentionally have hidden cuts for layered assembly. This is a verified static setup; large rotations or deformation still need an actual Spine pose pass. The replacement hair, collar and garment follow the reference but are not pixel-identical reconstructions.
