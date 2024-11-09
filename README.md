![Server Binaries](https://i.imgur.com/xP3IFZh.jpeg)

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

## Courtesey fixes already provided by:
**Adrian**: https://github.com/Adrianilloo/SourceSDK2013
**Hrgve**: https://github.com/hrgve/hl2dm-bugfix (forked)
**Tyabus**: https://github.com/tyabus/source-sdk-2013-multiplayer-fixes
**weaponcubemap**: https://github.com/weaponcubemap/source-sdk-2013

Additional thanks to **No Air** for providing assistance where needed.

Additional fixes were provided in complement of the above.

Not all fixes and updates are listed here, only the most important ones.

# Bug Fixes
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

---

# Quality of Life Improvements
- Removed `FCVAR_DEVELOPMENTONLY` from many commands, restoring compatibility without Sourcemod's sm_cvar command.
- Immediately spawn when switching from Spectator to a team.
- Removed black screen for fall damage deaths.
- Made Spectators friendly to all players.
- Restored speed crawl from the pre-Orange Box era
- Increased max player count to 32.
- Track SLAM ownership by SteamID.
- Added custom suit charger limits.
- Adjusted player model update frequency to 0.1 second.
- Enhanced time formatting to `DD:HH:MM:SS`.
- Added additional outputs for `func_healthcharger`.
- Improved bolt pass-through for glass.
- Allowed SLAM detonation upon throw.
- Announce new player connections in chat.
- Colorized specific chat messages.
- Increased shotgun spread and damage.
- Defaulted tickrate to 100.
- Added control over connection, team switch, and disconnect messages.
- Restored god mode.
- Enabled custom HUD options for player scores and team stats.
- Restored `impulse 101` to give 100 armor points.
- Added Steam authentication enforcement.
- Updated lag compensation.
- Disabled viewmodels when zoomed in to prevent upside-down model issues.
- Enhanced spawn system to prevent spawning near active players.
- Set pause time limit to 5 minutes.

---

# New Features
- Added options to spawn weaponless or with custom loadouts.
- Displayed remaining time as a HUD element.
- Added HEV suit voice.
- Enabled item pickup through glass.
- Added custom sprint drain rate options.
- Allowed game description customization.
- Added new chat triggers.
- Enabled ear ringing and DSP toggle.
- Added `trigger_catapult` and `trigger_userinput`.
- Defaulted `sv_allow_point_servercommand` to false.
- Added custom respawn times for weapons.
- Enabled team score and player score HUD elements.
- Allowed SMG1 and orb projectiles to break glass.
- Enabled automatic spectator assignment upon connection.
- Added custom forced respawn times.
- Introduced custom sounds.
- Enabled fall damage toggle.
- HUD target ID improvements.
- Enhanced player visibility through equalizer.
- Allowed spectators to view team chats.
- Added FOV options and zoom for .357.
- Added infinite sprint, oxygen, and flashlight options.
- Added weapon respawn controls.
- Introduced AFK tracking and team kill penalties.
- Enabled instant secondary attack on physcannon.
- Provided control over shotgun pump on deploy.
- Allowed `ent_fire` use for non-host users (with permissions).
- Enabled IP masking for client connections.
- Added save/restore system for player scores.
- Introduced a new admin interface.
- Enabled MOTD display toggle.
- Added armor sparks for armored players.

---

# Console Commands
These existing commands, previously restricted to Debug mode, are now accessible:

- `sv_stopspeed`
- `sv_maxspeed`
- `sv_accelerate`
- `sv_airaccelerate`
- `sv_wateraccelerate`
- `sv_waterfriction`
- `sv_footsteps`
- `sv_rollspeed`
- `sv_rollangle`
- `sv_friction`
- `sv_bounce`
- `sv_maxvelocity`
- `sv_stepsize`
- `sv_backspeed`
- `sv_waterdist`

**Recommended for advanced users only.**

**New Console Commands:**

- `sv_crowbar_respawn_time`
- `sv_stunstick_respawn_time`
- `sv_pistol_respawn_time`
- `sv_357_respawn_time`
- `sv_smg1_respawn_time`
- `sv_ar2_respawn_time`
- `sv_shotgun_respawn_time`
- `sv_crossbow_respawn_time`
- `sv_frag_respawn_time`
- `sv_rpg_respawn_time`
- `sv_speedcrawl`
- `sv_show_client_put_in_server_msg`
- `sv_show_client_connect_msg`
- `sv_show_client_disconnect_msg`
- `sv_show_team_change_msg`
- `mp_allow_immediate_slam_detonation`
- `mp_noblock`
- `sv_timeleft_teamscore`
- `sv_timeleft_color_override`
- `mp_smg_alt_fire_glass`
- `mp_ar2_alt_fire_glass`
- `mp_rpg_ceiling`
- `sv_join_spec_on_connect`
- `mp_forcerespawn_time`
- `sv_spec_can_read_teamchat`
- `sv_silence_chatcmds`
- `sv_showhelpmessages`
- `mp_restartgame_notimelimitreset`
- `mp_spawnprotection`
- `mp_spawnprotection_time`
- `mp_afk`
- `mp_afk_time`
- `mp_afk_warnings`
- `sv_teamkill_kick`
- `sv_teamkill_kick_threshold`
- `sv_teamkill_kick_warning`
- `sv_domination_messages`
- `sv_keybind_spam`
- `sv_jump_spam_protection`
- `sv_use_spam_protection`
- `sv_jump_threshold`
- `sv_jump_threshold_reset`
- `sv_use_threshold`
- `sv_use_threshold_reset`
- `physcannon_secondaryrefire_instantaneous`
- `sv_infinite_sprint`
- `sv_infinite_oxygen`
- `sv_infinite_flashlight`
- `sv_flashlight_drain_rate`
- `sv_allow_client_ent_fire`
- `sv_showbullet_tracers`
- `net_maskclientipaddress`
- `sv_hudtargetid`
- `sv_hudtargetid_channel`
- `sv_hudtargetid_delay`
- `sv_equalizer_allow_toggle`
- `sv_chat_trigger`
- `sv_logchat`
- `sv_allow_point_servercommand`
- `sv_equalizer`
- `sv_show_client_connect_msg`
- `sv_show_client_disconnect_msg`
- `sv_savescores`
- `hl2_walkspeed`
- `hl2_normspeed`
- `hl2_sprintspeed`
- `sv_sprint_drain_rate`
- `mp_ggun_punt_slam_damage`
- `sv_show_motd_on_connect`
- `mp_skipdefaults`
- `mp_spawnweapons*`
- `mp_suitvoice`
- `mp_ear_ringing`
- `sv_custom_sounds**`
- `sv_lockteams`
- `sv_teamsmenu`
- `sv_propflying`
- `sv_spawnpoint_lineofsight`
- `sv_spawnradius`
- `sv_hl2mp_item_pickup_through_glass`
- `mp_armor_sparks`
- `sv_new_shotgun`
- `sv_shotgun_hullsize`
- `sv_autobhop`
- `sv_pause_timelimit`
- `sv_timeleft_enable`
- `sv_timeleft_teamscore`
- `sv_timeleft_color_override`
- `sv_timeleft_r`
- `sv_timeleft_g`
- `sv_timeleft_b`
- `sv_timeleft_channel`
- `sv_timeleft_x`
- `sv_timeleft_y`
- `sv_equalizer_combine_red`
- `sv_equalizer_combine_green`
- `sv_equalizer_combine_blue`
- `sv_equalizer_rebels_red`
- `sv_equalizer_rebels_green`
- `sv_equalizer_rebels_blue`
- `sv_overtime`
- `sv_overtime_limit`
- `sv_overtime_time`
- `mp_autoteambalance`
- `mp_melee_viewkick`
- `mp_held_fragnade_punt`
- `sv_physcannon_default_pollrate`
- `sv_physcannon_obstructiondrop`

> *To use `mp_spawnweapons`, set your weapon spawns in `weapon_spawns.txt` and set `mp_skipdefaults` to `1`.*
> *This command must be added in the server command line!*
---

# Chat Commands
The following chat commands have been added:

- `timeleft`
- `!timeleft`
- `!e`
- `!eq`
- `!equalizer`
- `nextmap`
- `!nextmap`
- `!tp`
- `!teamplay`
- `!fov`
- `!mzl`
- `!czl`
- `!mz`
- `!cz`
- `!ks`
- `!hs`
- `!switch`
- `!cmds`
- `!motd`
- `!1`
- `!2`
- `!3`
- `!spec`
- `!spectate`
- `!comb`
- `!combine`
- `!blue`
- `!reb`
- `!rebs`
- `!rebels`
- `!red`
- `!rebel`

> *The above commands are automatically suppressed from the chat.*

# Server Administration Interface

## Introduction
As of version 1.1.0, a new server administration interface was implemented to provide an all in one package and provide server owners with the tools that they need to administrate their servers. The advantage is that it is built within the binaries, tailored for HL2MP and loads fast and does not come with any useless features. If you are coming from Sourcemod, a lot of the new system is built around familiarity so that if you wish to make the transition, you will not feel lost.

## Admin Commands & Levels
### Admin Commands
> Usage<br>
The server admin interface comes with a number of commands, all of which used to keep players in line with your rules or to maintain your server.

To use an admin command, you must follow the following syntax:
- **[Server Admin] Usage: sa \<command\> [argument]**

If you use the console to input commands, you must always prepend **sa** before the command: **sa map dm_lockdown**
If you use the chat to input commands, you do <ins>_not</ins> need **sa** at all: **!map dm_lockdown**
Additionally, you can silence chat commands by using a forward slash: **/map dm_lockdown**

> Targeting<br>
Multiple options are available to target a player or a group:<br>
**NOTE**: Not all commands may support this:<br>

| General Targets | Purpose                                                                                                                          |
|-----------------|----------------------------------------------------------------------------------------------------------------------------------|
| `name`          | Targets a player by their name. Partial names are supported; full names may require quotes if they contain spaces or need exact matching. |
| `#userid`       | Targets a player by their user ID. Use the `status` command in the console to retrieve a player's ID. The `#` symbol is required. |
| `@all`          | Targets all players on the server.                                                                                               |
| `@me`           | Targets yourself as the admin executing the command.                                                                             |
| `@blue`         | Targets all players on the Combine team.                                                                                         |
| `@red`          | Targets all players on the Rebels team.                                                                                          |
| `@!me`          | Targets all players except yourself.                                                                                             |
| `@alive`        | Targets all players who are currently alive.                                                                                     |
| `@dead`         | Targets all players who are currently dead.                                                                                      |
| `@bots`         | Targets all bot players.                                                                                                         |
| `@humans`       | Targets all human (non-bot) players.                                                                                             |


Below is a table listing all available commands. Please note that non-root admins may only see commands they have access to.

| Command       | Format                              | Admin Access Level | Description                                                                                   |
|---------------|-------------------------------------|---------------------|-----------------------------------------------------------------------------------------------|
| `say`         | `<message>`                         | Chat (`j`)         | Sends an admin formatted message to all players in the chat                                   |
| `csay`        | `<message>`                         | Chat (`j`)         | Sends a centered message to all players                                                       |
| `psay`        | `<name\|#userID> <message>`          | Chat (`j`)         | Sends a private message to a player                                                           |
| `chat`        | `<message>`                         | Chat (`j`)         | Sends a chat message to connected admins only                                                 |
| `ban`         | `<name\|#userID> <time> [reason]`    | Ban (`d`)          | Ban a player                                                                                  |
| `kick`        | `<name\|#userID> [reason]`           | Kick (`c`)         | Kick a player                                                                                 |
| `addban`      | `<time> <SteamID3> [reason]`        | RCON (`m`)         | Add a manual ban to `banned_user.cfg`                                                         |
| `unban`       | `<SteamID3>`                        | Unban (`e`)        | Remove a banned SteamID from `banned_user.cfg`                                                |
| `slap`        | `<name\|#userID> [amount]`           | Slay (`f`)         | Slap a player with damage if defined                                                          |
| `slay`        | `<name\|#userID>`                    | Slay (`f`)         | Slay a player                                                                                 |
| `noclip`      | `<name\|#userID>`                    | Slay (`f`)         | Toggle noclip mode for a player                                                               |
| `team`        | `<name\|#userID> <team index>`       | Slay (`f`)         | Move a player to another team                                                                 |
| `gag`         | `<name\|#userID>`                    | Chat (`j`)         | Gag a player                                                                                  |
| `ungag`       | `<name\|#userID>`                    | Chat (`j`)         | Ungag a player                                                                                |
| `mute`        | `<name\|#userID>`                    | Chat (`j`)         | Mute a player                                                                                 |
| `unmute`      | `<name\|#userID>`                    | Chat (`j`)         | Unmute a player                                                                               |
| `bring`       | `<name\|#userID>`                    | Slay (`f`)         | Teleport a player to where an admin is aiming                                                 |
| `goto`        | `<name\|#userID>`                    | Slay (`f`)         | Teleport yourself to a player                                                                 |
| `map`         | `<map name>`                        | Change Map (`g`)   | Change the map                                                                               |
| `cvar`        | `<cvar name> [new value]`           | CVAR (`h`)         | Modify any cvar's value                                                                       |
| `exec`        | `<filename>`                        | Config (`i`)       | Executes a configuration file                                                                 |
| `rcon`        | `<command> [value]`                 | RCON (`m`)         | Send a command as if it was written in the server console                                     |
| `reloadadmins`| *No arguments*                      | Config (`i`)       | Refresh the admin cache                                                                       |
| `help`        | *No arguments*                      | Generic (`b`)      | Provide instructions on how to use the admin interface                                        |
| `credits`     | *No arguments*                      | Generic (`b`)      | Display credits                                                                               |
| `version`     | *No arguments*                      | Generic (`b`)      | Display version                                                                               |

Please note that more commands will be added as new versions come out.

### Admin Levels
Different commands require different levels. It is up to the server owner to define what an admin has access to.

| Name        | Flag | Description                                          |
|-------------|------|------------------------------------------------------|
| Generic     | b    | Basic admin functions (help, credits, version).      |
| Kick        | c    | Permission to kick players.                          |
| Ban         | d    | Permission to ban players.                           |
| Unban       | e    | Permission to unban players.                         |
| Slay        | f    | Player management (slap, slay, noclip).              |
| Change Map  | g    | Permission to change the map.                        |
| CVAR        | h    | Permission to modify server variables (cvars).       |
| Config      | i    | Permission to execute configuration files and refresh the admin cache. |
| Chat        | j    | Chat functions (gag, mute, psay).                    |
| Vote        | k    | Start or create votes.                               |
| Password    | l    | Set a password on the server.                        |
| RCON        | m    | Server console access without password.              |
| Root        | z    | Full permissions and immunity.                       |


There is no notion of immunity. It is expected and assumed that selected admins are fit for the job they were assigned. Only root admins are protected from being kicked, banned, gagged and muted.
Finally, there is no admin menus available. After surveying a number of people, it is clear the admin menu has absolutely no use and therefore was not included.

## Adding Admins
Adding admins is straightforward. All admin data is handled in a single file called *admins.txt*. Navigate to `hl2mp\cfg\admin` and open `admins.txt`. This file also includes instructions on adding admins in case you need a reminder. At the bottom of the file, use the KeyValues format to add your admins:

```
"Admins"
{
    "STEAMID3" "ACCESS"
}
```
> **Note:** You must use the SteamID3 format. Any other format will be ignored.

**Example:**
```
"Admins"
{
    "[U:1:400275221]" "z" // Full root access for this SteamID
}
```
Admin data loads on every level initialization. You can also use `sa reloadadmins` to manually refresh the admin cache.

---

## Admin & Chat Logs

### Admin Logs
All admin commands are logged in the `hl2mp\cfg\admin\logs` directory. Admin log files are prefixed with `adminlogs_`, followed by the current date. Each log entry is formatted as follows:

```
[MAP] DATE @ TIME => Admin ADMIN_NAME <SteamID3> ACTION
```
**Example:**
```
[dm_overwatch] 2024/11/01 @ 09:34:35 => Admin Console <Console> changed map to dm_powerhouse
[dm_overwatch] 2024/11/01 @ 09:47:18 => Admin Peter Brev <[U:1:400275221]> executed rcon: botrix bot add
[dm_overwatch] 2024/11/01 @ 09:47:21 => Admin Peter Brev <[U:1:400275221]> kicked Bart (fool) <BOT> (No reason provided)
[dm_overwatch] 2024/11/01 @ 09:52:21 => Admin Peter Brev <[U:1:400275221]> executed rcon: sv_noclipspeed 15
[dm_overwatch] 2024/11/01 @ 11:50:07 => Admin Console <Console> changed map to dm_runoff
```
> **Note:** Admin logs cannot be disabled.

### Chat Logs
Chat logs follow a similar format:
```
[dm_overwatch] 2024/11/08 @ 10:11:27 => Player Peter Brev <[U:1:400275221]> said "text"
[dm_overwatch] 2024/11/08 @ 10:11:37 => Player Peter Brev <[U:1:400275221]> said "this is logged"
[dm_overwatch] 2024/11/08 @ 10:19:41 => Player Peter Brev <[U:1:400275221]> said "amazing"
[dm_overwatch] 2024/11/08 @ 10:25:55 => Player Peter Brev <[U:1:400275221]> said "AAAAAAAAAAAAAAH"
```
> **Note:** Chat logging can be disabled using the `sv_logchat` command.

---

## Rock the Vote & Nominate

The Server Administration Interface includes **Rock the Vote (RTV)** and **Nominate** functionality, which are disabled by default. You can enable these features and customize RTV behavior with the following ConVars:

| ConVar Name            | Default Value | Description                                                                                              |
|------------------------|---------------|----------------------------------------------------------------------------------------------------------|
| `sv_rtv_enabled`       | `0`           | Enables or disables Rock the Vote (RTV) functionality.                                                   |
| `sv_rtv_needed`        | `0.60`        | Percentage of players needed to start a map vote. Range: 0.00 - 1.00                                     |
| `sv_rtv_mintime`       | `30`          | Time in seconds players must wait before they can start typing RTV.                                      |
| `sv_rtv_minmaprotation`| `15`          | Minimum number of maps before allowing a previously played map in RTV or nominations.                    |
| `sv_rtv_showmapvotes`  | `0`           | If non-zero, displays which map a player has voted for during RTV.                                      |

To enable RTV and Nominate, set `sv_rtv_enabled` to `1`.

### Map List Requirements
To populate the map list for `nominate` and `rtv`, add maps to `mapcycle.txt`. A minimum of 5 maps is required for RTV to function correctly, but it is recommended to include more to keep the system engaging. If you have fewer than 15 maps, adjust `sv_rtv_minmaprotation` to a number lower than the total maps in `mapcycle.txt`.

- `sv_rtv_needed` requires a value between `0.00` and `1.00`, representing the percentage of players required to start a map vote. By default, `0.60` means 60% of players.
- `sv_rtv_mintime` specifies the time (in seconds) after the level loads before players can initiate RTV.
- `sv_rtv_showmapvotes` displays players' map votes during the RTV process.

### Usage
Players simply need to type `nominate` or `rtv` in the chat. Each player can nominate only one map per current map, and nominations can be updated any time. If a player disconnects, their nomination is automatically removed.
