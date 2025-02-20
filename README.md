<<<<<<< HEAD
![Server Binaries](https://i.imgur.com/xP3IFZh.jpeg)

## As of 2/18/2025, a large update has been pushed to both the Source SDK and Source 1 games. As such, the binaries may not work anymore. An updated version of this repository here will be pushed in the future. No ETA as of now as a lot of work may be needed. Thanks for understanding.

### Frequently Asked Questions

## Will Sourcemod work with the binaries?

It should work fine for the most part with those binaries. However, some specific plugins requiring specific gamedata might not work. Because this is outside the scope of providing bug fixes, I cannot provide assistance there.

## How do I raise an issue?

Use the **Issues** tab above to help me keep tabs of ongoing issues.

## I would like to report a bug. How do I do that?

Use the **Discussions** tab at the top. I will more than happy to take a look.

# Preamble
You **must** apply the loose fixes explained here by Adrian: https://gist.github.com/Adrianilloo/6359896e79b6b135d3c925c627c9554b#loose-fixes
This is to stop a crash from happening on server boot up. The following is the gist of it to at least get the server to boot. Again, fixes provided by Adrian.
## Fix bad pointed library errors on SRCDS startup (Linux 32/64 bit)
- Problem crashes SRCDS. Fix: Create symbolic links. From server root folder:

```
ln -s soundemittersystem_srv.so soundemittersystem.so
ln -s scenefilecache_srv.so scenefilecache.so
```

- Another problem which does not crash is SSDK2013 binaries pointing to outdated library names. Fix:
```
mv libtier0.so libtier0.so.bak
ln -s libtier0_srv.so libtier0.so
mv libvstdlib.so libvstdlib.so.bak
ln -s libvstdlib_srv.so libvstdlib.so
```

# Installing the Server Binaries

## Installation Instructions

> **Note:** The following applies if you run the game on the latest branch of the game.

### Extracting Files:
Extract the contents of the **srcds** folder into your SRCDS installation directory (the location of your **srcds.exe**).

### Setting Up SRCDS for Prerelease:
Ensure your SRCDS installation is set to the **prerelease** branch. Follow these steps:
1. **Backup** your existing SRCDS installation.
2. Open **SteamCMD**.
3. Use the command `force_install_dir <path/to/srcds>` if you have a custom installation directory.
4. Login anonymously with the command:  
   `login anonymous`
5. Update SRCDS to the prerelease branch:  
   - If **already installed**:  
     `app_update 232370 -beta prerelease`
   - If this is your **first time** installing:  
     `app_update 232370 -beta prerelease validate`

---

## Important Notes

### Sound Fixes:
This release includes fixes for missing sounds, such as weapon and item audio cues.

---

## Courtesey fixes already provided by:
- **Adrian**: https://github.com/Adrianilloo/SourceSDK2013
- **Hrgve**: https://github.com/hrgve/hl2dm-bugfix (forked)
- **Tyabus**: https://github.com/tyabus/source-sdk-2013-multiplayer-fixes
- **weaponcubemap**: https://github.com/weaponcubemap/source-sdk-2013

Additional thanks to **Hrgve** for providing assistance where needed.

Additional fixes were provided in complement of the above.

Not all fixes and updates are listed here, only the most important ones.

# Bug Fixes
- Fixed many broken sounds (server-side).
- Fixed numerous server-crashing bugs and issues.
- Fixed a glitch where grenades could clip through some corners.
- Fixed an issue where a player's model would remain on fire after dying.
- Fixed a bug causing flames to follow players if they switched to Spectator mode while burning.
- Fixed exploits that allowed players to gain frag points unfairly by switching to Spectator in teamplay mode.
- Fixed SLAM laser behavior to extend fully on larger maps.
- Fixed `impulse 101` to correctly provide crossbow ammo.
- Fixed death sound playing when a player disconnected.
- Fixed SLAMs remaining in the level if the thrower disconnected or switched to Spectator.
- Fixed sprint delay, allowing immediate sprinting.
- Fixed an exploit allowing `impulse 51` use without `sv_cheats`.
- Fixed `sv_autojump` (renamed to `sv_autobhop`).
- Fixed missing weapon sounds due to prediction errors.
- Fixed floating guns around a player when observed by a spectator in IN_EYE mode.
- Fixed guidance issues for the third rocket.
- Fixed rocket slowdown when in contact with water.
- Fixed shotgun secondary attack prediction bugs.
- Fixed grenade trail attachment to the correct position.
- Fixed weapon switching when a live rocket is in the air.
- Fixed AR2 alt-fire misfires.
- Fixed tickrate-related physics issues.
- Fixed items and weapons not respawning at designated points as set by level designers.
- Fixed various ladder-related issues, including respawning on ladders, broken ladders after player disconnection, speed climbing, and wall clipping.
- Fixed player death due to spectator presence and limited respawn points.
- Fixed respawnable oil drums exploding on respawn.
- Fixed props exploding regardless of handling method after being thrown.
- Fixed SLAM damage and radius.
- Fixed bolts slowing upon contact with satchels.
- Fixed death icon for `physboxes`.
- Fixed floating SLAMs.
- Fixed weapon switching while holding objects with the physcannon.
- Fixed rapid suit power drain.
- Fixed view angle snapping with the .357 at high pings.
- Fixed weapon respawn angles.
- Fixed ambiguous console warning: `Precache of sprites/redglow1 ambiguous (no extension specified)`.
- Fixed SLAM stacking exploit.
- Fixed satchel detonation being impossible when a satchel charge was in a planting animation.
- Fixed player model T-posing when switching weapons mid-air.
- Fixed unintended rocket explosions due to hull size issues.
- Fixed default weapon attribution upon player spawn being skipped when spawn points are congested.
- Fixed sprint cancelation when dropping items held with the physcannon.
- Fixed headcrab canister precache warning on `dm_runoff`.
- Fixed prediction issue when dying while holding an object.
- Fixed player model not matching team.
- Fixed missing reload animation for `weapon_crossbow`.
- Fixed missing spark effects for crossbow bolt impact.
- Fixed dead players being healed.
- Fixed server crash exploit using `ent_remove` and `ent_remove_all`.
- Fixed AR2 bullet collision with the skybox.
- Fixed sprinting underwater.
- Fixed auto weapon switch persistence when cl_autowepswitch was 0.
- Fixed AR2 view punch while firing.
- Fixed silent object holding with the physcannon.
- Fixed `func_rotating` entities stopping unexpectedly.
- Fixed third-person view switch exploit.
- Fixed `point_camera` path movement issues.
- Fixed `func_movelinear` movement errors.
- Fixed incorrect model spawning for physics objects.
- Fixed sound control issues with `ambient_generic`.
- Fixed tank train damage issues.
- Fixed `func_rot_button` “Start locked” setting.
- Fixed `mp_flashlight` enforcement settings.
- Fixed various NPC-related issues.
- Fixed bypassing of forced respawn.
- Fixed items flipping on the Z-axis when punted while looking down.
- Fixed collision issues for props held in the physcannon by players who switch to Spectator.
- Fixed ammo crate opening with explosives while holding a crowbar.
- Fixed proper team chat for Spectator and Unassigned players when teamplay is disabled.
- Fixed bolt-related issues when jumping onto a landed bolt.
- Fixed sound precaching issues for physcannon on Debian systems.
- Fixed missing footstep sounds when another player was within PVS due to prediction.
- Deprecated `mp_footsteps` in favor of `sv_footsteps`.
- Fixed attacker not being properly updated in specific circumstances.
- Fixed rocket damage across ceilings.
- Fixed immediate game restart with `mp_restartgame_immediate`.
- Fixed numerous team scoring bugs.
- Fixed `joingame` console command functionality.
- Fixed team auto-balance functionality.
- Fixed AR2 and shotgun firing while holding both `+ATTACK` and `+ATTACK2`.
- Fixed shotgun trace accuracy.
- Fixed grenade ownership transfer issue.
- Fixed satchel double-explosion bug.
- Fixed mines' trace lines inside brushes.
- Fixed bounding box issues for mines and satchel charges.
- Fixed item collision by using the model's collision model.
- Fixed "backpack" reload for SMG1 and AR2.
- Fixed fall damage sounds when no fall damage occurred.
- Fixed props clipping into the void on `dm_lockdown`.
- Fixed gameplay issues caused by `cl_updaterate 0`.
- Fixed excessive `FCVAR_SERVER_CAN_EXECUTE` messages.
- Fixed empty dropped weapon pickup issues causing switching problems.
- Fixed view kick for crowbar and stunstick.
- Fixed physcannon throw mass calculations in multiplayer.
- Fixed custom disconnect reason exploit on non-VAC secured servers.
- Fixed `trigger_push` not affecting moving players on the Z-axis properly.
- Fixed punt sounds for `func_breakable_surf` glass.
- Fixed orb speed upon satchel charge contact.
- Fixed "Interpenetrating entities!" console spam.
- Fixed SMG1 grenade vertical smoke trail issue.
- Fixed `game_ui` bug where `+USE` activated interactive objects.
- Fixed server crash from `trigger_weapon_dissolve`.
- Fixed game unpause issue when `sv_pausable` is set to 0.
- Fixed spectator mode within spectators.
- Fixed physcannon interaction with objects behind walls.
- Fixed a server crash caused by respawnable breakable objects touching a trigger_hurt brush.
=======
# Source SDK 2013

Source code for Source SDK 2013.

Contains the game code for Half-Life 2, HL2: DM and TF2.

**Now including Team Fortress 2! ✨**

## Build instructions

Clone the repository using the following command:

`git clone https://github.com/ValveSoftware/source-sdk-2013`

### Windows

Requirements:
 - Source SDK 2013 Multiplayer installed via Steam
 - Visual Studio 2022

Inside the cloned directory, navigate to `src`, run:
```bat
createallprojects.bat
```
This will generate the Visual Studio project `everything.sln` which will be used to build your mod.

Then, on the menu bar, go to `Build > Build Solution`, and wait for everything to build.

You can then select the `Client (Mod Name)` project you wish to run, right click and select `Set as Startup Project` and hit the big green `> Local Windows Debugger` button on the tool bar in order to launch your mod.

The default launch options should be already filled in for the `Release` configuration.

### Linux

Requirements:
 - Source SDK 2013 Multiplayer installed via Steam
 - podman

Inside the cloned directory, navigate to `src`, run:
```bash
./buildallprojects
```

This will build all the projects related to the SDK and your mods automatically against the Steam Runtime.

You can then, in the root of the cloned directory, you can navigate to `game` and run your mod by launching the build launcher for your mod project, eg:
```bash
./mod_tf
```

*Mods that are distributed on Steam MUST be built against the Steam Runtime, which the above steps will automatically do for you.*

## Distributing your Mod

There is guidance on distributing your mod both on and off Steam available at the following link:

https://partner.steamgames.com/doc/sdk/uploading/distributing_source_engine

## Additional Resources

- [Valve Developer Wiki](https://developer.valvesoftware.com/wiki/Source_SDK_2013)

## License

The SDK is licensed to users on a non-commercial basis under the [SOURCE 1 SDK LICENSE](LICENSE), which is contained in the [LICENSE](LICENSE) file in the root of the repository.

For more information, see [Distributing your Mod](#markdown-header-distributing-your-mod).
>>>>>>> 0759e2e8e179d5352d81d0d4aaded72c1704b7a9
