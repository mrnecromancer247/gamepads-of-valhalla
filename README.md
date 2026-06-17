# Gamepads of Valhalla

**Improved gamepad / controller support for *Rune Classic* (Unreal Engine 1) — analog sticks and bindable triggers, via a `winmm` → SDL2 shim.**

![platform](https://img.shields.io/badge/platform-Windows%20x86-blue)
![steamdeck](https://img.shields.io/badge/Steam%20Deck-Proton-1a9fff)
![license](https://img.shields.io/badge/license-MIT-green)
![built with SDL2](https://img.shields.io/badge/built%20with-SDL2-orange)

Works on **Steam**, **GoG** (1.10 / 1.11) and **Rune Gold** (Rune 1.07 / *Halls of Valhalla*).

Rune runs on Unreal Engine 1 and reads the gamepad through the legacy WinMM
joystick API. Its native XInput path is broken: the camera spins, deadzones are
wrong, and the triggers collapse onto a single shared axis so **LT/RT can't be
bound at all.** Gamepads of Valhalla replaces `winmm.dll` with a thin *shim* that
forwards everything unrelated to the real system DLL and re-implements only the
joystick functions — feeding them from a real controller through SDL2. You get
correct deadzones, **triggers exposed as bindable buttons**, and a PS2-style
"Viking" layout out of the box.

> **Unofficial fan project.** Ships **no game files** and modifies nothing in your
> system. No DRM or anti-cheat is touched (these games have none) — it's the same
> proxy-DLL technique used by countless renderer wrappers. Not affiliated with the
> rights holders of Rune.

---

## Features

- Analog left stick (move) and right stick (look) with proper deadzones.
- **Triggers as real buttons** — the thing native Rune can't do.
- PS2-style "Viking Warlord" control scheme, ready to paste in.
- Works with any controller SDL sees (Xbox, PlayStation, Switch, generic).
- Genuine legacy joysticks still work (the shim only virtualizes pad id 0).
- The underlying shim is engine-generic and also works on other UE1 titles
  (Unreal, UT99, Deus Ex, …) with different axis names — see notes below.

## How it works

```
game / WinDrv  ──calls──▶  winmm.dll  (this shim)
                              ├─ joy*  ─────────────▶  SDL2 GameController ──▶ your pad
                              └─ everything else ───▶  C:\Windows\System32\winmm.dll
```

Joystick **id 0** becomes a virtual SDL controller; ids 1–15 are delegated to the
real winmm, so any genuine legacy joystick keeps working.

## Control layout

| Button (Xbox / PS) | Action | | Button (Xbox / PS) | Action |
|---|---|---|---|---|
| **A / Cross** | Jump | | **LT-RT / L2-R2** | Camera out / in |
| **B / Circle** | Rune power | | **LS / L3** | Crouch |
| **X / Square** | Use / pick up | | **RS / R3** | Center view |
| **Y / Triangle** | Throw weapon | | **Back / Select** | Quick save |
| **LB / L1** | Block | | **Start** | Quick load |
| **RB / R1** | Attack | | **D-pad** | Weapon select |
| Left stick | Move | | Right stick | Look |

D-pad: down = holster weapon; the others cycle the next weapon within a class
(sword → blunt → axe).

- Quick save / load are mapped to Back / Start for convenience — change or delete
  those `Joy*` lines in `User.ini` if you'd rather not have them.
- The pause/menu button is intentionally **not** mapped: the controller doesn't work
  in Rune's menus, and trying to open the menu from the pad can crash the game. Use
  **Esc + keyboard/mouse** for the menu.

---

## Install (download)

Grab the latest zip from [**Releases**](../../releases) and drop these into the
game's `System\` folder (next to `Rune.exe`):

```
winmm.dll                  ← the shim (x86)
SDL2.dll                   ← 32-bit SDL2 runtime
sdljoy.ini                 ← controller profile
copy_this_to_User_ini.txt  ← controller bindings (to paste into User.ini)
```
*(Optional: `gamecontrollerdb.txt` for exotic pads.)*

**Easiest — run the patcher.** With the game **closed**, double-click
**`apply-gamepad-config.bat`**. It backs up `User.ini`/`Rune.ini` (as `*.gov-bak`)
and writes every required setting plus all the gamepad bindings for you.
`restore-original-config.bat` undoes it. Then connect one gamepad and launch.

<details><summary><b>Manual setup</b> (if you'd rather not use the patcher)</summary>

In `Rune.ini`, section `[WinDrv.WindowsClient]`, set:

```
UseJoystick=True        ; OFF by default — nothing works without it
InvertVertical=False    ; otherwise vertical look is flipped
DeadZoneXYZ=True
DeadZoneRUV=True
ScaleXYZ=1000.000000    ; 1000 = Classic default. Rune Gold defaults to 2000 — set 1000.
ScaleRUV=1000.000000    ; same: keep 1000 and tune the right stick in sdljoy.ini
UseDirectInput=False    ; force the WinMM path the shim hooks
```

Then paste the contents of **`copy_this_to_User_ini.txt`** into `User.ini`'s
`[Engine.Input]` section (replacing the existing `Joy1..Joy16 / JoyX..JoyV /
JoyPov*` lines).
</details>

Edit configs with the **game closed** — UE1 rewrites `User.ini`/`Rune.ini` on exit.
Connect **one** gamepad (powered on) **before** launching, or SDL won't pick it up.
Full step-by-step is in the zip's `README.txt`.

> **Heads-up:** an unsigned `winmm.dll` is a common false-positive for antivirus /
> SmartScreen (the proxy-DLL pattern). The release notes publish a SHA-256 so you
> can verify the file, or build it yourself below.

## Build from source (Windows, x86)

Requirements: MSVC (VS 2019/2022/later), CMake ≥ 3.21, Python 3
(`pip install pefile`), and 32-bit SDL2 **development** libraries.

```bat
cmake -A Win32 -DSDL2_DIR=C:/path/to/SDL2/cmake -S . -B build
cmake --build build --config Release
```

`winmm.dll` lands in `build\Release\`. At configure time
`scripts/gen_forwarders.py` reads *your* system `winmm.dll` and generates the
complete forwarder table, so the export set always matches your OS.

> **x86 only.** The forwarders are `__declspec(naked)` `jmp` thunks, which MSVC
> only supports for 32-bit — which is fine, UE1 is 32-bit.

**SDL2 version:** built against SDL2 **2.30**, but the entire SDL2 2.x line is
ABI-compatible, so you can ship any newer 2.x runtime (e.g. **2.32.0**, the last
2.x release) without rebuilding — the runtime just has to be ≥ the version you
built against. Do **not** use SDL3; it's a different API and would need code changes.

## Steam Deck / Proton

Rune runs through Proton, which executes the Windows binaries — so the **same
32-bit `winmm.dll` + `SDL2.dll`** work, no Linux build needed. Two settings:

1. **DLL override** so Proton loads our native winmm instead of its builtin. In the
   game's Steam **Launch Options**:
   ```
   WINEDLLOVERRIDES="winmm=n,b" %command%
   ```
   (The shim loads the real winmm by full `System32` path, so under Wine it grabs
   the builtin one and never recurses — the override is safe.)
2. **Steam Input:** use a **Gamepad** template (pass-through as a standard pad), not
   a keyboard/mouse layout, so SDL inside Wine sees a real controller with analog
   triggers.

The `Program Files` / VirtualStore caveat doesn't exist on Linux; edit
`User.ini`/`Rune.ini` directly in the install's `System\` folder.

## Configuration & tuning

Everything lives in **`sdljoy.ini`** (commented): controller index, per-stick
deadzones, invert, **right-stick sensitivity**, trigger mode (`buttons` vs `axes`)
and threshold, and the SDL-button → Joy-index map. No rebuild needed.

- **Camera too fast/slow?** Adjust `right_sensitivity` (default **75**, max **100**).
  Restart the game after editing. Keep `ScaleRUV=1000` in `Rune.ini` and tune the
  right stick *only* here, so the two settings don't fight.
- **Vertical look inverted?** Flip `invert_right_y` (`0` ⇄ `1`).
- **An axis goes the wrong way?** Flip the `Speed=` sign on its `Joy*` line in `User.ini`.

### Other UE1 games

The shim is generic. For Unreal/UT99 the right-stick horizontal is `aTurn` instead
of `aBaseX`; otherwise the binding approach is identical.

## Troubleshooting

- **Game won't launch / missing winmm export** — your forwarder table is incomplete;
  rebuild so `gen_forwarders.py` runs against *your* winmm.
- **No controller detected** — confirm `SDL2.dll` is the **32-bit** build and sits in
  `System\`; make sure the pad is connected/on *before* launch; try a different
  `controller_index`; drop in `gamecontrollerdb.txt`.
- **A button (or any) "drops out" on launch** — (1) close Rune; (2) switch the gamepad
  off and back on (unplug/plug it back if wired), then start the game again; (3) if that
  doesn't help, restart your PC (or re-login to your Windows user on Win10/11) and it
  will remedy the bug. Keep only **one** controller connected. The binding itself is
  fine — it's the controller's session state, which reconnecting resets.
- **Edits don't stick** — close the game before editing; if the game is under
  `C:\Program Files\`, Windows redirects configs to `%LOCALAPPDATA%\VirtualStore`
  (run as admin or install outside Program Files).
- **Steam grabs the pad** — on desktop, disable Steam Input for Rune; on Deck, do the
  opposite (use the Gamepad template).

## License & credits

MIT — see [`LICENSE`](LICENSE). Bundles nothing from Microsoft or the games.
Third-party components (SDL2, SDL_GameControllerDB) and their licenses are listed in
[`THIRD_PARTY.md`](THIRD_PARTY.md). The name nods to the classic *Halls of Valhalla*
add-on (Rune Gold), which is one of the supported targets.
