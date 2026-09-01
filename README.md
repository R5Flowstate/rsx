# RSX ( R5Flowstate/S21 )

reSource Xtractor - extracts and previews Respawn RPak and model assets.

Agents view included: CLAUDE.md

## What this fork adds

- Raw export by default: models, textures, materials, `.rseq`, `.rrig`, and
  `.uiia` with its reloc list.
- Baked effects: raw `.efct_def` + child/asset GUID sidecars, and a
  version-dispatched operator list.
- Shaders: MSW export keeping the feature bytes; v16 headers normalized to
  the v15 arrangement. No bytecode is recompiled.
- `--exportpak` / `--exportguids` to export one pak or a GUID closure
  instead of every loaded pak; `-decompresspak`; `-validateshaders`.
- `-nogui` single-instance mutex, and post-load no longer busy-waits when a
  pak has no trailing non-prioritized assets.
- Crash fixes: bone arrays sized by bone count, unaligned quaternion loads,
  null-checked unparsed shadersets.

## Usage

```
rsx <file.rpak|file.mdl|...> [-nogui] [-export] [--exporttypes <4cc,...>]
    [--exportdir <dir>] [-exportfullpaths] [--exportpak <substr>]
    [--exportguids <file>] [-decompresspak] [-validateshaders] [-keepsm51]
```

```
# headless raw extract
rsx map.rpak -nogui -export -exportfullpaths

# only the assets a map pak owns
rsx common.rpak map.rpak -nogui -export --exportpak map

# a GUID closure, one guid per line
rsx common.rpak -nogui -export --exportguids closure.txt
```

Upstream: [r-ex/rsx](https://github.com/r-ex/rsx), by way of
[kralrindo/rsx](https://github.com/kralrindo/rsx).
