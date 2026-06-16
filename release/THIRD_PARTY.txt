# Third-party components

Gamepads of Valhalla's own code is MIT-licensed (see `LICENSE`). The release
archive additionally redistributes the components below. Both are permissively
licensed (zlib), which allows redistribution provided the notices are kept intact.

The shim does **not** contain or redistribute any Microsoft code or any files from
the game. The `winmm.def` / forwarder table is generated at build time from the
*user's own* system `winmm.dll`; only Win32 export **names** (uncopyrightable API
identifiers) are referenced.

---

## SDL2 (Simple DirectMedia Layer)

Project: https://www.libsdl.org/ — License: **zlib**
Redistributed as `SDL2.dll` in release archives. The authoritative license text
ships inside the SDL2 package; the zlib license reads:

```
Simple DirectMedia Layer
Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

## SDL_GameControllerDB

Project: https://github.com/mdqinc/SDL_GameControllerDB — License: **zlib**
The optional `gamecontrollerdb.txt` is the community controller-mapping database,
distributed under the same zlib license as SDL. Attribution: Gabriel Manzano
(mdqinc) and contributors.
