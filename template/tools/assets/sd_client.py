#!/usr/bin/env python3
"""sd_client.py - Stable Diffusion REST client for the Litt asset pipeline.

Talks the AUTOMATIC1111 WebUI API contract (/sdapi/v1/*), which is the
de-facto standard also spoken by SD.Next and several bridges:

    ./webui.sh --api          # or webui-user.bat: set COMMANDLINE_ARGS=--api

The endpoint can be a local WebUI (http://127.0.0.1:7860), a LAN box,
or a tunnel. Cloud endpoints that mirror /sdapi/v1/txt2img work too.

Every call returns raw PNG bytes or raises SDError with an actionable
message. No server? Callers use their procedural fallback instead.
"""
import base64
import json
import urllib.error
import urllib.request


class SDError(Exception):
    pass


class SDClient:
    def __init__(self, base_url="http://127.0.0.1:7860", api_key=None,
                 timeout=180):
        self.base = base_url.rstrip("/")
        self.timeout = timeout
        self.headers = {"Content-Type": "application/json"}
        if api_key:
            self.headers["Authorization"] = "Bearer " + api_key

    # -- low level ---------------------------------------------------------
    def _post(self, path, payload):
        req = urllib.request.Request(
            self.base + path,
            data=json.dumps(payload).encode("utf-8"),
            headers=self.headers, method="POST")
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as r:
                return json.loads(r.read().decode("utf-8"))
        except urllib.error.HTTPError as e:
            detail = e.read().decode("utf-8", "replace")[:300]
            raise SDError("%s -> HTTP %s: %s" % (path, e.code, detail)) from None
        except urllib.error.URLError as e:
            raise SDError(
                "%s unreachable at %s (%s). Start the WebUI with --api, "
                "or pass --server http://<host>:7860."
                % (path, self.base, e.reason)) from None

    def _get(self, path):
        req = urllib.request.Request(self.base + path, headers=self.headers)
        try:
            with urllib.request.urlopen(req, timeout=min(self.timeout, 20)) as r:
                return json.loads(r.read().decode("utf-8"))
        except urllib.error.HTTPError as e:
            raise SDError("%s -> HTTP %s" % (path, e.code)) from None
        except urllib.error.URLError as e:
            raise SDError(
                "%s unreachable at %s (%s)" % (path, self.base, e.reason)) from None

    # -- API surface -------------------------------------------------------
    def health(self):
        """True when a live A1111-compatible server answers."""
        try:
            self._get("/sdapi/v1/options")
            return True
        except SDError:
            return False

    def models(self):
        data = self._get("/sdapi/v1/sd-models")
        return [m.get("title", "?") for m in data]

    def txt2img(self, prompt, negative="", width=512, height=512,
                steps=20, cfg_scale=7.0, seed=-1, sampler="DPM++ 2M Karras",
                model=None):
        """Generate one image; returns PNG bytes."""
        payload = {
            "prompt": prompt,
            "negative_prompt": negative,
            "width": int(width), "height": int(height),
            "steps": int(steps), "cfg_scale": float(cfg_scale),
            "seed": int(seed),
            "sampler_name": sampler,
            "batch_size": 1, "n_iter": 1,
        }
        if model:
            self._post("/sdapi/v1/txt2img", {})  # probe reachability first
            opts = self._get("/sdapi/v1/options")
            if opts.get("sd_model_checkpoint") != model:
                self._post("/sdapi/v1/options", {"sd_model_checkpoint": model})
        data = self._post("/sdapi/v1/txt2img", payload)
        images = data.get("images") or []
        if not images:
            raise SDError("txt2img returned no images (prompt rejected?)")
        return base64.b64decode(images[0])

    def img2img(self, prompt, png_bytes, denoise=0.55, width=512, height=512,
                steps=20, cfg_scale=7.0, seed=-1):
        """Variation of an existing image; returns PNG bytes."""
        payload = {
            "init_images": [base64.b64encode(png_bytes).decode("ascii")],
            "prompt": prompt,
            "denoising_strength": float(denoise),
            "width": int(width), "height": int(height),
            "steps": int(steps), "cfg_scale": float(cfg_scale),
            "seed": int(seed), "batch_size": 1, "n_iter": 1,
        }
        data = self._post("/sdapi/v1/img2img", payload)
        images = data.get("images") or []
        if not images:
            raise SDError("img2img returned no images")
        return base64.b64decode(images[0])
