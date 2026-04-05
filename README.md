# Features

- Automatically gets players in your game and fetches stats (real fast)
- Command Interception: Enable/disable client commands

## Usage

- Inject `ovson.dll` into Minecraft yourself  
**or**  
- Run `OVsonLoader.exe` when Minecraft is fully loaded
- You DON'T need to download both of the files
- If you're stuck with chat stats do .ovmode gui

## Toggle Key
- Default: `Insert`
- Fully changeable

---

## Tab Stats
- Sortable (**ascending / descending**)
- Mode changeable  
- Sort by: `Team` · `Stars` · `FK` · `FKDR` · `Wins` · `WLR` · `WS`

### Multiple Tags
- Supports displaying multiple tags at once (Seraph / Urchin)  
- Fully toggleable/selectable in Tab  
- Works together with stats display  

### Previews
**FKDR Sort (FK Mode)**  
![Tab FKDR Sort](https://nigga.tr/tab_fkdr_sort.png)

**Teams (Normal)**  
![Tab Teams Normal](https://nigga.tr/tab_teams_normal.png)

**Multiple Tags Preview**  
![Tab Multiple Tags](https://nigga.tr/tab_multiple_tags.png)

---

## GUI Stats
- Auto remove
- Sortable (**ascending / descending**)
- Toggleable columns
- Sort by: `Team` · `Stars` · `FK` · `FKDR` · `Wins` · `WLR` · `WS`

### Preview
**FKDR Sort**  
![GUI FKDR Sort](https://nigga.tr/gui_fkdr_sort.png)

**Stars Sort**  
![GUI Stars Sort](https://nigga.tr/gui_stars_sort.png)

**Toggled Off Columns (Tags, Wins, WLR)**  
![GUI Toggled Off Columns](https://nigga.tr/gui_toggled_off.png)

---

## GUI Player Search
- Player stats support
- Urchin / Seraph tag detection

### Preview
![GUI Player Search](https://nigga.tr/gui_player_search.png)

---

## Click GUI
- Toggleable
- Clean layout
- Customizable

### Preview
![Click GUI](https://nigga.tr/clickgui_normal.png)

---

## Tags
- Seraph & Urchin support
- Can be displayed on **Tab** and **GUI**
- Fully toggleable / selectable
- Notifies when a tagged player is in your game

---

## Bedplates
- Works on Lunar and Badlion
- Currently disabled on Forge
- Needs improvements (WIP)

## Chat Bypasser
- Basic implementation
- Currently breaks commands (will be fixed)

## Replay Report Spammer
- Functional but slow
- Optimization planned

## Debug
- Toggleable master debug switch
- Debug sections available
- Structure is temporary (will be cleaned up soon)

## Command Interception
- Enable/disable client commands
- Useful to prevent accidental execution or enforce specific behavior

---

# Chat Commands
All commands use the `.` prefix in-game.

- `.help` — Shows all available commands  
- `.api <key>` — Save your Hypixel API key  
- `.stats <player>` — Show Bedwars stats for a player  

### Preview
![Stats Command Preview](https://nigga.tr/stats_command.png)

- `.tab <setting>` — Configure tab list  
- `.clearcache` — Clear cached player stats  
- `.clickgui <on|off>` — Toggle ClickGUI  
- `.tech <on|off>` — Toggle tech overlay  
- `.reportspam <p|start|stop>` — Replay report spammer  
- `.bedplates <on|off>` — Toggle bedplates  
- `.bedscan` — Manual bed defense scan  
- `.lookat` — Debug look-at test  
- `.mode <mode>` — not working atm  
- `.ovmode <mode>` — OVson submode switch  
- `.debugging <on|off>` — Toggle debug messages  
- `.echo <text>` — Client-side echo test  

---

## Work In Progress
- Colors will be reworked
- General UI improvements planned
- Some modules still experimental
- Tags will be shown on `.stats` command
- Tab stats will be fully reworked

---

## Known Issues
- On game start, the first second may cause lag due to rapid stat fetching  
- Threading is currently optimized for speed; this can make stats load very fast initially  
- Solutions for smoother initial performance are being worked on

---

## Supported Clients
- Lunar — Tested  
- Badlion — Beta  
- Forge — Beta  
- Vanilla — Beta

## Special Thanks
- pree
