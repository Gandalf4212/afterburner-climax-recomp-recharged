# Reference copies from mad-riders-recomp

Copied from github.com/Gandalf4212/mad-riders-recomp at commit b69b268 (the owner's project, MIT) so
this repo no longer needs that one. **Not compiled.** Use them as models:

| File | Use it for |
|---|---|
| `mad_riders_app.h` | Shader-cache wiring: `SetUpShaderCache()` (ShaderCacheSource, old-cache cleanup), `OnPostLaunchModule` (game thread held until `PrepareShaderCacheBeforeLaunch` finishes), `ShaderCacheScreen` in `OnCreateDialogs`, `native_x_renderer_scene_wait_ms` default. |
| `shader_scan.cpp/.h` | How Mad Riders finds every shipped shader (`.rpack` sections 0x30/0x31). After Burner needs its own scanner for its data format. |
| `native_x_renderer_mad_riders.cpp` | Hook-file model, incl. DrawIndexedVertices split for draws over 65532 indices and Swap reported as Present. |
| `native-x-renderer.md` | Mad Riders' NativeX notes (entry-point table, verify results, scene size/AA, shader cache). |

The four NativeXRenderer patches are in `patches/native_x_renderer-pending/` (written for NativeX 5aa5a05; not
applied by CI). See docs/HANDOFF.md, next step 3, for the rebase onto bf965e9. The shader cache's own guide is
`docs/shader-cache.md` inside patch 0002/0004.
