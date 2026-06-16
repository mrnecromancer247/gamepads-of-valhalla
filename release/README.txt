==============================================================
            G A M E P A D S   O F   V A L H A L L A
        gamepad support for Rune Classic (PS2-style layout)
==============================================================
  A small mod for improved gamepad/controller support
  Works on Steam, GoG (1.10 / 1.11), Rune Gold (Rune 1.07/Halls of Valhalla)
==============================================================

WHAT THIS IS
------------
A proxy winmm.dll intercepts the engine's joystick input (Unreal
Engine 1) and reads the gamepad through SDL2 - with correct dead
zones and, crucially, with triggers you can actually bind (Rune's
native input can't do that).


WHAT'S IN THE PACK
------------------
  winmm.dll          - the shim (32-bit)
  SDL2.dll           - 32-bit SDL2 runtime
  sdljoy.ini         - controller profile 
  copy_this_to_User_ini.txt  - assigns for controller actions
 

NOTE: winmm.dll and SDL2.dll are both 32-bit (x86)


INSTALL
-------
1. Open the game folder: <Rune Classic>\System\

2. Back up User.ini and Rune.ini.

3. Copy winmm.dll, SDL2.dll, sdljoy.ini into System\ 

4. In Rune.ini, section [WinDrv.WindowsClient], set:
	UseJoystick=True       (OFF by default - nothing works without it)
	InvertVertical=False   (otherwise vertical look is flipped)
	DeadZoneXYZ=True
	DeadZoneRUV=True
	ScaleXYZ=1000.000000   (1000 is default value for Classic. For Rune Gold the default is 2000, so change it to 1000)
	ScaleRUV=1000.000000   (1000 is default value for Classic. For Rune Gold the default is 2000, so change it to 1000)
	UseDirectInput=False

5. Open User.ini, find the [Engine.Input] section and replace the
   Joy1..Joy16 / JoyX..JoyV / JoyPov* lines with the block from
   copy_this_to_User_ini.txt

6. Connect a gamepad and launch the game.

Make all config edits with the game CLOSED - UE1 rewrites
User.ini and Rune.ini on exit.
Gamepad should be connected (or switched ON) before you start Rune, otherwise SDL won't initialize it


CONTROL LAYOUT
--------------
  Xbox/Playstation scheme
  A/cross           jump
  B/circle          rune power
  X/square          use / pick up
  Y/triangle        throw weapon
  LB/L1             block
  RB/R1             attack
  LT-RT/L2-R2       camera out / in
  LS/L3             crouch
  RS/R3             center view
  Back/Select       quick save
  Start             quick load
  D-pad             weapon select (down = holster weapon, other = next weapon in class sword-blunt-axe)
  Left stick        move
  Right stick       look

I added QuickSave/Load to Back/Start buttons for convenience. If you don't like them, you can change or delete the Joy button assigns in User.ini
I didn't map pause/menu button, because Rune crashes if you try to go into menu from controller, since Controller doesn't work in Menu anyway. Use ESC and KB+M button to access the menu
 


CONTROLLERS
-----------
Any gamepad SDL2 can see works (Xbox, PlayStation, Switch, generic)
Connect the gamepad BEFORE launching the game.
Keep ONLY ONE gamepad connected - two at once can cause input
conflicts (an individual button may "drop out"). If something
sticks, disconnect the extra pad and/or reboot the PC.


TUNING
------
  Right-stick sensitivity - right_sensitivity parameter in sdljoy.ini

  (75 is default value, 100 is max). Restart the game after editing.
  ScaleRUV in Rune.ini also effects sensitivity, so keep it 1000 and adjust the sensitivity of right stick only in sdljoy.ini

IMPORTANT - WHERE THE GAME IS INSTALLED
---------------------------------------
If the game lives in C:\Program Files\... (common with Steam),
Windows redirects the configs to %LOCALAPPDATA%\VirtualStore and
your edits to User.ini / Rune.ini won't apply. Fix: run the game
as administrator OR install it outside Program Files (e.g. C:\Games\).

Steam: in Rune Classic's properties disable Steam Input
(Controller -> Disable) so it doesn't conflict with the shim.
