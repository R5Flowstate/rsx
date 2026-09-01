# RSX — agent notes

Human overview: `README.md`.

Default export is **raw / re-packable** for RePak. Oodle is statically linked.

## Build

`rsx.sln`, Release x64. Output: `bin/Release/rsx.exe`.

## CLI

**All flags first, then all input paths.** First non-`-` argument ends flag
scan. `-nogui` without `-export` / `--list` / `--depfilepath` / `-validateshaders`
/ `-decompresspak` loads and exits.

```
rsx.exe -nogui -export [flags] <pak> [pak2 ...]
```

Boolean: `-nogui` `-export` `-exportfullpaths` `-exportdependencies`
`-matltextures` `-nocachedb` `-exportrigsequences` `-validateshaders`
`-decompresspak`

Valued:

| flag | use |
|------|-----|
| `--exporttypes` | comma 4cc (`mdl_,txtr,matl,wrap,…`) |
| `--exportpak` | substring of **source pak filename**; other paks resolve only |
| `--exportguids` | txt, one hex GUID/line; export only those |
| `--exportdir` | default `.\exported_files` |
| `--list` / `--listformat` | `txt` or `csv` |
| `--depfilepath` / `--depfileformat` | adjlist `node,dep1,…` |
| `--texturenames` | `guid` / `stored` / `text` / `semantic` |
| `--parsethreads` `--exportthreads` | `-nogui` only |
| `--validateout` | CSV path for `-validateshaders` (default `shader_validate.csv`) |

`--list` CSV column `type` is the authoritative `--exporttypes` code.
Exported files are named **unpadded** (`0x101F…` not `0x0101…`).

`-nogui` takes mutex `Local\RSX_SingleInstance` (second instance waits).
imgui.ini `[AssetSettings]` is **GUI only**. Headless format = code default
in `InitXxxAssetType`. Change a format → edit source and rebuild.

## `--exporttypes` 4cc

Each code is ≤ 4 chars (`MAKEFOURCC`). Not sure? `--list --listformat csv`
and use column `type`.

| 4cc | asset |
|-----|-------|
| `wrap` | BSP + `.bsp_lump` |
| `mdl_` | model |
| `matl` | material |
| `txtr` | texture |
| `txan` | texture anim |
| `txtx` | texture extra |
| `shdr` / `shds` | shader / shaderset |
| `uiia` | UI image (raw container for RePak) |
| `uimg` | UI atlas (legacy) |
| `font` | font atlas |
| `arig` / `aseq` | anim rig / sequence |
| `dtbl` | datatable |
| `stgs` / `stlt` | settings / layout |
| `rmap` | map asset |
| `efct` | particle |
| `locl` | localization |
| `rson` | RSON |

## `-validateshaders`

Offline DX11 gate. Headless D3D11 **debug-layer** device; `Create*Shader` on
every real (non-ref) bytecode buffer after post-load conversions. CSV via
`--validateout`. 0 flagged = creatable on S21 DX11 without launching the
client. Load the map plus `common*` / `startup*` so child shaders resolve
parents. Does not export.

```
rsx.exe -nogui -validateshaders --validateout out.csv <map.rpak> <common.rpak> ...
```

## `-decompresspak`

Standalone: no asset load, no export. Decompresses oodle/zstd/rtech and writes
`<path>.dec.rpak` (`foo.rpak` -> `foo.rpak.dec.rpak`; flags cleared,
`cmpSize = dcmpSize`).

```
rsx.exe -nogui -nocachedb -decompresspak <file.rpak>
```

Decompressed Respawn paks are **segment-aligned** pages, not contiguous.
Prefer RSX JSON / `--depfilepath` over re-reading that file by offset.

## Re-packable extract

- Always `-exportfullpaths`.
- Materials: `--texturenames guid` or `stored`. Default `text` invents
  semantic names that StringToGuid wrong.
- Models: raw `.rmdl` + `.vg` / `.vg_static` / `.phy` / `.rson`.
- aseq/arig: raw `.rseq` / `.rrig` (not `.cast`).
- Shaders: MSW. v16 env-combo uses v16 scale indices. SM5.1 buffers are
  replaced with a sibling SM5.0 for the S21 DX11 device.
- `--exportpak <map>.rpak -exportdependencies` = map + closure, not every
  loaded pak.
- `--exportguids` for a computed closure (do not `--exporttypes txtr` on
  common).

## Maps / wrap

- Streamed lumps need the `.starpak` next to the `.rpak`.
- Disk-load: strip `.client`/`.server` from lump names. RSX writes
  `….bsp_lump.client`; the loader wants `….bsp_lump`.
- Lump `0x69` (`LUMP_LIGHTMAP_DATA_REAL_TIME_LIGHTS`) in-rpak size is `N * 6`;
  disk-load wants `N * 6.5` = `rpakSize * 13/12`. Append zeros (`rpakSize / 12`).
  Skip the pad and map-load aborts on odd lump size. Other lightmap lumps are
  passthrough (`0x61 = N*2`, `0x62 = N*8`, `0x7A`).
- Sidecar hex: RSX writes uppercase `%X`; bspconv/VPK want lowercase.

## Do not

- Blame RSX when guid-mode material JSON is clean and the packer has
  garbage refs. That is almost always stem-collision in the manifest
  generator (match materials by **full path**).
- Treat decoded uiia/txtx (PNG/DDS) as RePak input. Need the raw
  container (self-pointer reloc list).
- Re-read a decompressed pak with a **contiguous** page model.
  Respawn pages are **segment-aligned**. Prefer RSX JSON / `--depfilepath`.
- Put a file path before a `--valued` flag.
