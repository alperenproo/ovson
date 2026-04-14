# OVson

## Features
- Api keyless stat fetching
- Automatically gets players in your game and fetches stats (real fast)
- Command Interception: Enable/disable client commands
- Lobby Pre-Game Stat Checker (auto stats when someone chats)
- Smart Chat Bypasser (does not break commands)
- Quick Queue commands
- Multiple tag detection (Seraph / Urchin)
- GUI, Tab and Search stats
- Bedplates detection
- Replay report spammer
- Debug system

---

## Usage
- Inject `ovson.dll` into Minecraft yourself  
**or**  
- Run `OVsonLoader.exe` when Minecraft is fully loaded  
- You DON'T need to download both files  
- If you're stuck with chat stats do `.ovmode gui`

---

## Toggle Key
- Default: `Insert`
- Fully changeable

---

# Lobby Pre-Game Stat Checker
- Automatically detects players chatting in lobby
- Instantly fetches their Bedwars stats
- Sends stats directly to chat
- Works in pre-game phase

### Preview
![Lobby Pre Game Stats](https://nigga.tr/pre_game_stats.png)

---

# Chat Bypasser
- Smart keyword-based bypass
- Commands are no longer broken
- Automatic handling
- Safer chat sending

---

# Tab Stats
- Sortable (ascending / descending)
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

# GUI Stats
- Auto remove
- Sortable (ascending / descending)
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

# GUI Player Search
- Player stats support
- Urchin / Seraph tag detection

### Preview
![GUI Player Search](https://nigga.tr/gui_player_search.png)

---

# Click GUI
- Toggleable
- Clean layout
- Customizable

### Preview
![Click GUI](https://nigga.tr/clickgui_normal.png)

---

# Tags
- Seraph & Urchin support
- Can be displayed on Tab, GUI and `.stats`
- Fully toggleable / selectable
- Notifies when a tagged player is in your game

---

# Bedplates
- Works on Lunar and Badlion
- Currently disabled on Forge
- Needs improvements (WIP)

---

# Replay Report Spammer
- Functional but slow
- Optimization planned

---

# Debug
- Toggleable master debug switch
- Debug sections available
- Structure is temporary (will be cleaned up soon)

---

# Command Interception
- Enable/disable client commands
- Prevent accidental execution
- Enforce custom behavior

---

# Chat Commands
All commands use the `.` prefix in-game.

- `.help` — Shows all available commands  
- `.api <key>` — Save your Hypixel API key  
- `.stats <player>` — Show Bedwars stats for a player  

### Quick Queue
- `.1s` — Solo queue  
- `.2s` — Doubles queue  
- `.3s` — 3v3v3v3 queue  
- `.4s` — 4v4v4v4 queue  
- `.4v4` — 4v4 queue  

### Other Commands
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

# Work In Progress
- Colors will be reworked
- Some modules still experimental
- Tab stats will be fully reworked

---

# Known Issues
- Threading is optimized for speed, which may cause initial burst
- Smoother loading improvements planned
- Breaks Vape's antibot

---

# Supported Clients
- Lunar — Tested  
- Badlion — Tested  
- Forge — Beta  
- Vanilla — Beta  

---

# Special Thanks
- pree
