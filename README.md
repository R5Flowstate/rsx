# RSX (R5Flowstate / S21)

reSource Xtractor — extract and preview Respawn RPak / model assets. This fork
is the S21 map-port extractor: it dumps **raw, re-packable** assets that RePak
can feed back into an S21 pak. Upstream RSX is a general extractor/previewer
and defaults to processed (not re-packable) output.

Upstream: [r-ex/rsx](https://github.com/r-ex/rsx).

## What this fork adds

- **Raw export by default** for models, textures, materials, aseq, arig so
  RePak can rebuild the same asset.
- Shader export: MSW by default; v15/v16 feature bytes preserved; v16
  env-combo count uses the v16 scale indices; SM5.1 permutations replaced
  with a sibling SM5.0 buffer for the S21 DX11 device.
- efct: version-dispatched operator-list export + raw `.efct_def` /
  `.efct_childrefs` / `.efct_assetrefs` sidecars.
- uiia: raw container export with self-pointer reloc list.
- `--exportpak` / `--exportguids` for a map's dependency closure instead of
  dumping an entire pak set.
- Headless `-nogui` single-instance mutex (concurrent pak loads no longer
  lock each other).
- Anim parse: bone arrays sized by bone count (no 256-bone stack overrun);
  materials null-check an unparsed newer shaderset instead of crashing.

## Building

Open `rsx.sln` and build x64 Release. Output: `bin/Release/rsx.exe`.

By using this software, you acknowledge that the software is provided "as is",
without any representations, warranties, conditions, or liabilities, to the
extent permitted by law.
