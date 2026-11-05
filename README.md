# GoldenEye 007: Recompiled, on the web

> [!WARNING]
> **This is n64hub/GoldenEye64Recomp-web, a fork, not GoldenEye64Recomp or GoldenRecomp.** Its `main` branch is
> podulator's [GoldenEye64Recomp](https://github.com/podulator/GoldenEye64Recomp), which carries on cblock85's
> [GoldenEye64Recomp](https://github.com/cblock85/GoldenEye64Recomp), with web support added: built into a web page
> (WebGPU, through [N64Recomp-browser](https://github.com/n64hub/N64Recomp-browser)). Everything this fork adds to
> podulator's main was written by AI (Claude). It is not made, endorsed or supported by GoldenEye64Recomp's or
> GoldenRecomp's authors, and please don't take issues with this fork to them. Only fixes for bugs the desktop build
> has too, the keyboard and mouse, a Windows build and the move onto RecompFrontend are offered upstream, to
> podulator's, each a pull request of its own on its code, without the web code, saying it is AI-written.
> Building it: [BUILDING.md](BUILDING.md).
>
> This port is here to build out and test N64Recomp-browser's web support on another game than the one it was written
> for: what it turns up is fixed there where it can be, so every recomp's page gets the fix.

**Play it in the browser: https://n64hub.com/goldeneye/**, with your own ROM, picked in the
launcher and kept in the browser. It needs WebGPU (Chrome or Edge 113 and up). The page is built from this branch
and published with N64Recomp-browser's `tools/publish_page.sh`, so it can be a commit or two behind.

GoldenEye 007 (N64, NTSC-U) statically recompiled with [N64Recomp](https://github.com/N64Recomp/N64Recomp) into C,
run by [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) and drawn by [RT64](https://github.com/rt64/rt64),
from the [GoldenEye decompilation](https://gitlab.com/kholdfuzion/goldeneye_src). It descends from kholdfuzion's
[GoldenRecomp](https://github.com/kholdfuzion/goldenrecomp) (Windows) through cblock85's GoldenEye64Recomp (macOS
and Linux) and podulator's fork of it, whose main this one merges: its launcher and menus on RecompFrontend, its
settings, keyboard and mouse, quicksaves and debug menu, its build scripts for Windows, Linux, macOS and the Steam
Deck and its release workflow are this fork's too. This fork adds the page, which runs in a browser with WebGPU, and
builds natively as podulator's does (checked on Windows), on upstream RT64, N64ModernRuntime and RecompFrontend at the
commits N64Recomp-browser's web forks are built on (What differs from podulator's main).

## What differs from podulator's main

The fork's diff from podulator's main is the web support and what it needs:

- `lib/`: podulator's copies of RT64, N64ModernRuntime, RecompFrontend and N64Recomp (`n64recomp-src`) are
  submodules here, at the upstream commits N64Recomp-browser's forks are built on, beside N64Recomp-browser itself.
  What podulator's copies change in them is in `lib-patches/` (its `apply.cmake` says what each patch holds), applied
  with N64Recomp-browser's `patches/` on top by `cmake -P lib-patches/apply.cmake lib`: CMakeLists.txt runs it when it
  configures, and the build scripts before they build N64Recomp from `lib/N64ModernRuntime/N64Recomp`. Not carried:
  RT64's ubershaders-only on the page, which builds the game's shaders before it draws with them
  (`src/rt64_shaders.txt`), and N64ModernRuntime's `--game` and `--game-mode`, newer than the runtime the forks are
  on. On Linux, CMakeLists.txt runs RT64's DXC as upstream names it, `dxc-linux`.
- The page's build: CMakeLists.txt's `EMSCRIPTEN` parts, the prebuilt shader list and how it is recorded
  (`src/rt64_shaders.txt`, `tools/record_shaders.sh`, `tools/shader_survey.js`, `patches/shader_survey.c`), and the
  plans that play the page (`tools/play_plan.json`, `tools/play_plan_extended.json`, `tools/shader_plan.json`).
- The page's code, behind `__EMSCRIPTEN__`: no native file dialogs and no Exit in the launcher, the canvas as the
  window, sound through N64Recomp-browser's AudioWorklet output, render to RAM's late copy, the page's own game id
  (`ge007.n64.us`, under which a browser keeps the player's ROM and saves), and a Mouse Sensitivity of 0% saved by an
  earlier page loading as 50% (`src/game/config.cpp`).
- The front end and the attract demos across a window wider than 4:3, which the native build takes too: the intro's
  dots cross the whole window, its gun barrel slides in from the window's edge, the barrel behind the file select is
  zoomed to fill the window, and the folder menus' cover runs to its edges (`patches/widescreen.c`, beside the
  intro's own `patches/workbench_theboy.c`). An attract demo draws the whole wider view and culls by it as the game
  does, so its replay leaves out the check of the game's random seed against the recording, which the guards' other
  random numbers tripped a few seconds in: its guards then act on their own while Bond plays the recording to its end.
- The in-game HUD to the sides of a window wider than 4:3, which the native build takes too: the ammo counters, the
  messages at the top and bottom and the health and armour gauges sit as far from the window's sides as they sit from
  the 4:3 view's, as far out as the HUD Placement setting lets them (`patches/hud.c`). The crosshair, the gun and the
  watch stay centred, and split screen draws as the game does.
- Fixes the page needed, which the native build takes too:
  - The patches yield through `recomp_yield_self_1ms`, a native whose signature matches the call (WebAssembly traps on
    one that doesn't), and the main loop's waits for its next tick yield as they poll (`patches/fps.c`; Status says
    why).
  - `get_frames_remaining` leaves out the buffer queued last (`src/main/main.cpp`; Status says why).
  - `dump.toml` gives `cosf` the size that takes in `sinf`, which it falls through into, so N64Recomp writes the whole
    function: the page's build doesn't run `tools_weaken_patched.py`, which repairs it after.
  - `patches.toml` and `us.toml` rename the game's `strtol` and the other functions with C library names, which
    clashed with the library's declarations, and the patches call the game's `strtol` rather than a copy of their own
    (N64Recomp's strict patch mode refuses a copy).
  - `patches/syms.ld` comments out with `/* */`: ld.lld took a `//` as part of the next symbol's name.
  - The launcher remembers a ROM picked in it (`src/main/launcher.cpp`): the runtime's `is_rom_valid` knows only the
    ROM stored when the program started, so Start game stayed Select ROM, and opened the file dialog again, until the
    program restarted.

- Ext, a control style for a modern pad, the one a new file plays (A modern pad: Ext, below).

## No Nintendo assets

This repository holds no game code, no game assets and no ROM data. You need your own GoldenEye 007 NTSC-U dump
(big-endian `.z64`, sha1 `abe01e4aeb033b6c0836819f549c791b26cfde83`).

What is committed is symbol data (function names, addresses and sizes in `dump.toml` and `data_dump.toml`), the
port's own code, and two patches that rearrange *your* dump into the "TLBFREE" layout the recompiled code was made
from: `vanilla_to_tlbfree.xdelta` to build with, and `vanilla_to_tlbfree.gep1`, which the program applies to the
ROM you load, so the launcher takes the retail dump as it is.

## Status

- Native (Windows, clang-cl): the launcher, the gun-barrel intro and the attract demo.
- The page: the launcher, Load ROM with the retail dump, the intro and its credits, the menus, and Dam walked on the
  keyboard, at the game's 60 retraces a second, with sound on the browser's audio thread. No real gamepad tried. How
  it plays, in which browser and on what graphics, is recorded in N64Recomp-browser's
  [plays.jsonl](https://github.com/n64hub/N64Recomp-browser/blob/main/plays.jsonl) and its
  [games table](https://github.com/n64hub/N64Recomp-browser#games).
- A steady 960 frames a second of the sound, about 2%, came up short in both browsers and natively: the game's audio
  manager made every buffer its minimum (`get_frames_remaining` in `src/main/main.cpp` says why). Since main 59dc8e8 it
  writes 48,000 frames a second and plays without a gap between the boot's shader wait and its first burst: on a
  3840x2000 canvas, a cold profile each, frames short per 5 s went from 4,800 to 0 in both browsers outside the bursts,
  and Firefox 158's from 161k in 45 s to 136k.
- The bursts were the main loop's wait for its next tick (`patches/fps.c`), which polled the count without an OS call:
  on the console the scheduler and the audio thread preempt it, and here they waited behind it. In Firefox that was
  about 0.4 s at a few fixed moments of the boot, and in Chrome, in some cold runs, ~20% for ~40 s while the shader list
  builds behind the game. The intro's copy of the wait lasts as long as the frame before took, so one slow frame (a
  shader built behind the game, a load) held every frame of the intro to that length, the main thread polling 85% of
  the time, ~1.5 million counts a second, and the audio thread getting in every ~85 ms instead of 33. Each poll yields
  now (main 7f40089). With and without it, cold, alone on the GPU: Chrome 155 headless at 1256x645 had 0 frames short
  in 85 s in each of three runs, against 630k (~20% for ~40 s) without it; Firefox 158 at 3840x2000 0 short in 95 s,
  against 27k, 67k and 28k in three 5 s windows.
- The page builds the game's shaders before it draws with them, so weak graphics don't draw with RT64's slow general
  shader: the 465 in `src/rt64_shaders.txt`, recorded by a script that plays the intro and the credits, shows every
  menu and starts every level (`tools/record_shaders.sh`), are built a part at a time: the intro's and the menus'
  before the game starts, and each level's when the level first draws. Chrome's pipeline cache holds only part of
  them, so a later visit builds most of a level's again. The script records each level's start, so a shader a level
  draws later is built when it first draws, and the page remembers it and builds it with that level on the next visit.
- Render to RAM stays on: the game reads a frame back 3-33 times a second (up to 120 in some scenes), each read
  waiting for the GPU. The page copies a frame into RDRAM a display list late (rt64-web 73e9ca0): in Dam 3.8-5.2 ms a
  list against 8.0-9.7 with the exact copy, the lamps' glare still drawn (headless Chrome 155, RTX 2070).
- Firefox looks for finished GPU work only after a submission or every 100 ms, so a frame that waits on its readback
  can wait that long. On a laptop's Intel UHD, before N64Recomp-browser v0.21.0, the intro's blood overlay, whose
  shaders the first section didn't hold then, drew ~180 draws a frame with RT64's ubershader, which halved the frame
  rate, and in 3 of 3 scripted first visits that reached it the game then fell to about a frame a second for good, each
  frame waiting on its readback. The page submits nothing once a millisecond while a readback waits (rt64-web
  d070e5c): those visits then kept 28-45 frames a second and a second visit never fell. On an RTX 2070 slowed by a
  heavy pass added to every present, d070e5c took Firefox from 33-37 frames a second to 42-45. The first section now
  holds the intro and the cast credits.
- Firefox builds the shaders one at a time and presents nothing meanwhile, so its waits are the WGSL's size. Since
  rt64-web b052049 (N64Recomp-browser v0.41.0) a pixel shader that picks its tile per pixel (mip LOD) loads a level's
  four taps in one switch over the draw's slots instead of one switch a tap, the list's WGSL 19.0 → 16.3 MB and the
  largest RasterPS 224 → 185 KB: Firefox 158 waited 8.7-9.4 s for the boot and 14.0-21.7 s for Dam with it, against
  10.4-11.6 and 17.7-23.3 without (RTX 2070, 1256x645, a fresh profile each, three runs each, interleaved, beside
  another program's game on the GPU). Since rt64-web 008f380 (v0.50.0) such a shader samples its levels and tiles in
  loops, and Firefox's waits for the boot and Dam together went from 18.6 s to 10.8 s.
- What holds Chrome below 60 in the levels on a big canvas is the levels' own drawing, not the shaders: in Chrome 155
  with a window (`play_page.mjs --headed`) at 3840x2000 it draws 52-58 while the list builds behind the game, 59-60 in
  the title and the credits once all 202 are built (88 s), and then 48-55 in the attract demo's levels with nothing
  left to build and the GPU 75-78% busy.
- In Dam, render to RAM's late copy had the thread that runs the display lists asking at every retrace whether the
  copy could land, and each question waited behind everything queued on the page's thread. Since plume-web 81344f5
  and ba93421 (rt64-web a5bc25d) the readback's answer, the swap chain's size and its refresh rate are published by
  the page's thread and read without waiting. CPU profiles over 13 s in Dam at 3840x2000, Chrome 155 with a window,
  RTX 2070, a busy host:
  - The display list thread's waits for the page's thread fell from 2,335 ms to 151 ms.
  - The present thread's waits fell from 963 ms to 106 ms.
  - The page's thread fell from 47% busy to 32%.

  Presents stayed at 57-59.5 a second.
- The 1.6 s with nothing presented after Start on a first visit is the game's own boot: the boss thread's first
  retraces (~0.6 s), then, with no save yet, `fileValidateSaves` resetting and writing every save to the EEPROM, 48
  EEPROM calls each between `joyDisablePoll` and `joyEnablePoll`, which wait a retrace for the controller thread
  (~0.77 s). A second visit has the save and waits 1.05 s (headless Chrome 155, 3840x2000, RTX 2070; N64Recomp-browser's
  [docs/findings.md](https://github.com/n64hub/N64Recomp-browser/blob/main/docs/findings.md), Present gaps after
  Start).
- Mods a player adds are off on the web (there is no recompiler that emits WebAssembly at run time); natively they
  are supported but untested.

## Keyboard and mouse

The page and the desktop build play the game with keyboard and mouse as the PC ports do (the design of
[goldeneye-pc-port](https://github.com/jkdansereau/goldeneye-pc-port)'s `port/src/input.c`, and the feel of the
[Perfect Dark port](https://github.com/fgsfdsfgs/perfect_dark)), as podulator's main does: `patches/mouse.c` is the
game's side, and `src/game/input.cpp` and `src/game/mouse.cpp` the program's.

- The mouse turns Bond and tilts his view, 0.055 degrees a count at the default 50% as Perfect Dark's port does, less
  as the view zooms, aiming or not. Right click aims and does nothing else: it shows the crosshair, which stays in the
  middle of the screen where the shots go, and zooms the weapons that zoom, as Perfect Dark's port's Locked mouse aim
  and goldeneye-pc-port's Centred aim do, while Bond walks, strafes and looks as he does without it. It isn't the
  game's aim mode (R), which would stop him: there the stick moves the crosshair instead of walking him and C-left and
  C-right lean. While it aims the sniper rifle or the camera, the wheel zooms it further, up in and down out. Mouse up
  looks up, whatever the game's Look Up/Down option says, unless Settings > General > Invert Mouse Y is on.
- The game's auto-aim (its Auto-Aim option) is off while the mouse plays, the pointer locked to the game: shots go
  where the mouse points, and right click's crosshair stays in the middle, rather than onto a guard near it. With a pad,
  or Mouse Sensitivity at 0, the saved option is as it was.
- In the tank (Runway, St. Petersburg) A and D turn it, as the arrows do, and W and S drive it, rather than A and D
  swinging its turret a degree a tick, which is what C-left and C-right do there; the mouse swings the turret and tilts
  the view instead (the tank works out Bond's view from its own angle and its turret's, so the mouse can't turn it as
  it turns Bond). In the game's aim mode (Left Shift) the tank's controls are the game's own.
- On the watch's inventory page W and S step through the items an item a press, and held, after 0.4 s at 7.5 a
  second, as a PC's list does, rather than an item each of the game's 60 ticks a second, which is how far the game
  moves its cursor with the stick pushed all the way, as the keys push it. The Up and Down arrows (C-up and C-down) step
  as the game has them. With a pad, or the pointer free, the stick is the game's own.
- In the menus' folders (file, mission and difficulty select) the mouse moves the game's cursor, as the stick does, left
  click selects and right click goes back, as B does.
- Left click fires (also F), right click aims, the wheel changes weapon, R reloads, and Left Ctrl or C crouches, a
  toggle aim mode keeps. Left Shift is R, the game's own aim mode. WASD walks and strafes, the arrows turn and look, Space or Q is A (weapon), E is
  B (use), X is L, Return or Tab is Start (the watch). Motion and keys while the watch is up do nothing.
- Settings > General > Mouse Sensitivity sets how far a count goes, 50% by default; 0 turns mouse look off and frees
  the pointer. On the page, a config saved before mouse look came holds the frontend's default then, 0, and loads as
  50% (src/game/config.cpp).
  Above 0 RecompFrontend asks for the browser's pointer lock once the game starts, which a browser grants only at a
  click or a key.
- Played in Dam in Chrome (headless and off the desktop) and Firefox, with synthetic mouse events and Bond's angles,
  crouch, aim mode, weapon, field of view and watch read from RDRAM: in both the mouse turns him the way the stick does,
  up looks up, in aim mode too (200 counts turn him 11 degrees aiming or not, about half that through a weapon zoomed
  to half), the crosshair stays in the middle, and the watch takes no motion; in Chrome the crouch lasts through aim
  mode and fire, reload and the wheel work, and with every gun aim mode zooms the KF7 to a 30 degree view, the AR33 to
  20 and the sniper rifle to 15, which C-up (the Up arrow) takes to 7. Neither browser gave those runs the pointer
  lock, so how it feels, and the lock itself, are left to a play by hand. In file select, in both, the cursor follows
  the mouse (0.25 menu units a count at 50%) and a left click opens the folder, and on the difficulty screen a right
  click goes back to mission select; with Invert Mouse Y on, mouse up looks down in Dam while the menu cursor still
  follows the mouse as it moves.
- Right click without the game's aim mode, played in Dam in Chrome and Firefox the same way, Bond's position, aim mode,
  crosshair, field of view and weapon read from RDRAM: with it held he stays out of aim mode with the crosshair shown,
  and W, D and S move him as far as without it (W for 1.5 s about 670 units either way, where the earlier aim mode
  moved him none); with the sniper rifle (All Guns on) it zooms to 15 degrees, three notches of the wheel up take that
  to 7.7 and one down to 9.6, he walks while zoomed, and letting go puts back 60 and hides the crosshair.
- Auto-aim with the mouse, played in Dam in Chrome and Firefox with right click held while Bond walks, strafes and turns
  past the guards, the player's auto-aim targets read from RDRAM 20 times a second: before, auto-aim had a target in 68
  of 970 samples in Chrome; now in none of 981 in Chrome and 918 in Firefox, with the option still saved on.
- The tank, played on Runway in Chrome and Firefox, Bond put onto the tank and in with E, the tank's and its turret's
  angles read from RDRAM: before, D held for 2 s left the tank where it was and swung its turret 123 degrees, the right
  arrow turned it, and the mouse turned nothing; now D turns the tank as far as the right arrow does (about 115 degrees
  in 2 s), A turns it back and the turret stays put, and 400 counts of the mouse to the right swing the turret 22
  degrees and 200 up tilt the view 11, in both browsers. Neither gave those runs the pointer lock either: a stand-in for
  it took the mouse's motion as a locked pointer's.
- The watch's inventory page, played in Dam in Chrome and Firefox with All Guns on (33 items), the page's item read from
  RDRAM while S, W and the Down arrow were held: before, S went through all 33 items in half a second, one each tick;
  now it steps one at once, the next 0.4 s later and then one every 133 ms, in both browsers, and the Down arrow steps
  as before (one at once, then about six a second). A stand-in took the place of the pointer lock here too.

## A modern pad: Ext

A ninth control style, Ext, after 2.4 on the watch's Control page, plays a modern pad as the PC ports do (Perfect Dark's
port's Ext, goldeneye-pc-port's modern scheme), and is the style a new file plays: `patches/ext_controls.c` is the
game's side and `src/game/ext_controls.cpp` the program's.

- The right stick turns and looks, the left stick walks and strafes, RT fires and LT aims (the game's aim mode, the
  stick moving the crosshair, a push to its edge turning). Up looks up, and moves the crosshair up, with the game's
  Look Up/Down option as it comes, which in the other styles has the stick and C-up look down, as a plane's stick
  does; its other setting turns Ext's round too. A or RB is B (use, open), LB the game's A (held with a shot,
  the previous weapon; tapped, the next), Y and B the next and previous weapon, X reloads, a click of the left stick
  crouches (a toggle, as the crouch key's), and Start is the watch. In aim mode the left stick down crouches and up
  stands, as 2.4 has it. The pad's buttons are these whatever the Controls tab binds it to, which the menus and the
  watch keep, as do the other styles.
- The keyboard plays on with the mouse as it does in 1.1: W and S walk, A and D strafe, the arrows turn and look (the
  Up arrow up, as the right stick), F and left click fire, Left Shift is aim mode and right click aims without it, and the keys for reload, crouch and the
  wheel are as above.
- It is the game's 2.4 (Goodhead), which plays those sticks with two N64 pads, the first's stick turning and its Z
  aiming, the second's walking and its Z firing: the game plays Ext as 2.4, and the program makes the two pads of one
  modern pad and the keyboard while Bond is the player's to move. The watch shows Ext's buttons as text where it draws
  the N64 pads, and Ext is listed only when one player plays.
- A save keeps the style in 3 bits of its options, which the eight styles fill, so it keeps Ext as a bit the game never
  sets, with 1.1 in the 3 bits (a console reads the file as 1.1), and every other style with another bit beside it. A
  file with neither bit (a console's, one saved before Ext, or one the game has just made) keeps its style once a
  mission is completed in it, and until then is new and plays Ext if its style is 1.1, the game's default. A file
  whose player picked 1.1 on the watch since keeps 1.1.
