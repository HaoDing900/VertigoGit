# Transparent plastic quality option

Open a Material Instance of `/Game/EnvArt/EnvMaterial/Glass/T_TransparentPlastic`.
Under **Performance**, enable the override checkbox beside **HighQuality**, then
set its value:

- **On** (default): original material calculations.
- **Off**: constant opacity/specular, simplified glow, and neutral index of refraction.

With HighQuality off, adjust **LowQualityOpacity** (default 0.10) and
**LowQualityGlowScale** (default 0.20). BaseColor, GlowAmount, and exposure
compensation remain available. Low quality loses angle-dependent edge effects.

`MI_TransparentPlastic_High` and `MI_TransparentPlastic_Low` in the same folder are
ready to compare or assign. Existing instances inherit HighQuality on. To keep
an existing instance's color and other overrides, toggle HighQuality on that
instance. To switch during gameplay, prepare instances and use Set Material;
HighQuality is a compile-time static parameter.

## Validation and limits

UE 5.3.2, D3D SM6, current project configuration:

| Version | Reported pixel instructions | Vertex instructions | Samplers |
|---|---:|---:|---:|
| Original | 1023 | 423 | 5 |
| High | 1023 | 423 | 5 |
| Low | 1011 | 423 | 5 |

This is a small instruction reduction, not a measured FPS improvement. The
material remains Default Lit, Translucent, Surface lighting, and retains its
refraction configuration. Neutral IOR does not guarantee elimination of the
refraction pass. Transparency overlap and lighting costs remain. Profile the
actual scene on target hardware; a much cheaper option would require a separate
simpler material, for example Unlit if scene lighting is unnecessary.

The original asset backup and compile report are in `Saved/MaterialQuality/`.
`Tools/add_transparent_plastic_quality.py` documents the one-time installation;
it refuses to run again over the installed quality option or existing presets.
