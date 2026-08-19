# Browser Asset Ingest

## Steps
1. Read edge_all_open_tabs metadata
2. Scan for models (.glb/.gltf/.obj), textures (.png/.ktx2), shaders (.glsl/.spv)
3. Validate license: CC0/CC-BY/MIT/Apache-2.0 OK, GPL/proprietary/unclear needs approval
4. If >1MB or unclear: PR with source URL, request human approval
5. Copy to template/assets/external/, update asset_index.json and ATTRIBUTION.md
6. Log: [ts] INGEST asset=<name> source=browser:<tabId> license=<license>
