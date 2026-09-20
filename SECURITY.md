# Security notes

Material Response is a ReShade addon for Dark Souls Remastered. It runs inside the game process and works on renderer state and D3D11 resources.

## Expected behaviour

The addon may:

- receive ReShade renderer callbacks
- inspect/track D3D11 resources and views
- select verified material/resource routes
- temporarily replace shader-resource bindings for a draw
- restore the original bindings afterwards
- read DSRRL assets from the game directory
- change memory protection for the addon's own reserved resource area during local asset loading

## It does not intentionally

- connect to remote servers
- perform HTTP requests
- download or execute programs
- launch external processes
- install services or drivers
- create scheduled tasks or autorun persistence
- collect credentials or personal data
- inspect browsers, email or unrelated applications
- modify `DarkSoulsRemastered.exe` on disk

## External EnvSpec resource

The addon reads:

`DSRRL\EnvSpec\PackedGI\PTDE_GI_ENVSPEC_PACK_RGBA.bin`

Expected size:

`33,619,968 bytes`

Expected SHA-256:

`c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3`

If validation fails, the EnvSpec resource path does not activate and the game keeps the stock DSR path.

## Antivirus detections

The public 1.45 addon has produced generic heuristic detections from several antivirus engines, mainly the Barys family, with a Microsoft ML Wacatac label also observed.

The repository contains the release builder, exact loader bytes, readable loader logic and release hashes so the binary's behaviour can be inspected directly.
