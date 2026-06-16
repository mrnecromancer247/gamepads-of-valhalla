# Cutting a release

The repo holds **source only**. Binaries (`winmm.dll`, `SDL2.dll`) are attached to a
GitHub Release, never committed. Steps:

1. **Build** the shim (see README → *Build from source*):
   ```bat
   cmake -A Win32 -DSDL2_DIR=C:/path/to/SDL2/cmake -S . -B build
   cmake --build build --config Release
   ```

2. **Assemble the download.** Copy into `release/`:
   - `build\Release\winmm.dll`  (your freshly built shim)
   - `SDL2.dll`  (the **32-bit** runtime from https://libsdl.org , in `lib\x86`)
   - *(optional)* `gamecontrollerdb.txt` from https://github.com/mdqinc/SDL_GameControllerDB

   `release/` already contains `sdljoy.ini`, `copy_this_to_User_ini.txt`, `README.txt` and
   `THIRD_PARTY.txt`. Delete the `_PUT-DLLS-HERE.txt` placeholder.

3. **Zip** the *contents* of `release/` (not the folder itself) as
   `Gamepads-of-Valhalla-vX.Y.zip`.

4. **Checksum** so users can verify the unsigned DLL:
   ```bat
   certutil -hashfile Gamepads-of-Valhalla-vX.Y.zip SHA256
   ```

5. **Publish on GitHub:** Releases → *Draft a new release* → create a tag
   (`v1.0`), title `Gamepads of Valhalla v1.0`, attach the zip, and paste release
   notes including the SHA-256 and the antivirus false-positive note.

> Keep `config/sdljoy.ini` + `config/copy_this_to_User_ini.txt` (source of truth) and the
> copies in `release/` in sync when you change bindings.
