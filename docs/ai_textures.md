# AI-Generated Assets (Stable Diffusion)

Litt's asset pipeline can use **Stable Diffusion** — locally or on any server
you provide — through the standard AUTOMATIC1111 WebUI REST contract. When no
server is reachable, every tool falls back to deterministic procedural output,
so AI build pipelines never stall.

## 1. Provide a server

Local (recommended, free):
```
git clone https://github.com/AUTOMATIC1111/stable-diffusion-webui
cd stable-diffusion-webui && ./webui.sh --api      # Windows: --api in webui-user.bat
```
Anything speaking `/sdapi/v1/txt2img` works: SD.Next, Fooocus bridges, LAN
boxes, tunnels. Point litt at it once:

`litt_engine.json` (next to the engine, or `~/.litt/litt_engine.json`):
```json
{
  "ai_assets": {
    "enabled": true,
    "endpoint": "http://127.0.0.1:7860",
    "model": "dreamshaper_8"
  }
}
```

## 2. What agents run

| tool | purpose |
|---|---|
| `template/tools/assets/sd_client.py` | shared A1111 client (txt2img / img2img / health) |
| `gen_texture.py` | prompt → PNG into `assets/textures/`, budget-enforced, index-registered, optional `--mtl mat1,mat2` binding |
| `gen_heightfield.py` | any image → luminance-displaced terrain OBJ (`res²·2` faces) |
| `selftest.py` | mock-server proof of the whole chain (no GPU needed) |

Examples:
```bash
python template/tools/assets/gen_texture.py --game-dir Project/kingsfall-hollow \
  --name crypt_stone --prompt "seamless dark gothic stone wall with moss" \
  --mtl crypt_wall,paving --seed 7

python template/tools/assets/gen_heightfield.py --game-dir Project/ember-depths \
  --image textures/ash_terrain.png --name ash_terrain --res 96
```

## 3. Rules

- **Provenance**: every generation is logged to the game's LIVE_LOG.md and
  registered in `asset_index.json` as `kind=texture`.
- **Budgets**: textures auto-downscale under 256 KB (asset_guidelines).
- **Determinism**: procedural fallback is seed-stable; SD seeds are recorded.
- **Fallback**: unreachable server → clearly-labeled procedural texture;
  pipelines continue. Never hard-fail a build on art.

## 4. Where textures show up today

- Browser runtime (three.js): `map_Kd` renders automatically via MTL.
- Native pygame player & Rust pathtracer: consume flat albedo; texture-aware
  sampling is an open renderer milestone (tracked in docs/ARCHITECTURE.md).
