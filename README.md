# mod-temucraft-guild-perks

Guild perks as guilds had them when they had levels, all of them. Three things decide whether a member has one:

- **The server allows it** (`allowed` in the world table `mod_temucraft_guild_perk`).
- **The guild has it unlocked**: every guild by default (`unlocked`), or only guilds a GM unlocks it for
  (`.guildperks unlock <perk> [guild name]`, `.guildperks lock ...`).
- **The guild has it on**: unlocked perks are on (`enabled`) until the guild master or an officer switches one off
  (`.guildperks off <perk>`, `.guildperks on <perk>`; who may is the `ManageRank` setting).

Perks the client has a spell for show in the spellbook with their own icon and tooltip; members learn them while the
perk is on and lose them when it isn't. `.guildperks` lists every perk with its state for your guild.

| # | Perk | Effect |
|---|---|---|
| 1 | Fast Track | +10% experience from kills and quests |
| 2 | Mr. Popularity | +10% reputation from kills and quests |
| 3 | Mount Up | +10% mounted speed, not in battlegrounds or arenas |
| 4 | Hasty Hearth | hearthstone cooldown 15 minutes shorter |
| 5 | Cash Flow | an extra 10% of looted money goes to the guild bank (you lose nothing) |
| 6 | Reinforce | 20% less durability loss from dying and from spirit healers |
| 7 | Working Overtime | +20% chance of a skill-up on crafting and gathering (never on grey recipes or nodes) |
| 8 | The Quick and the Dead | +100% speed while dead (not in battlegrounds or arenas), +50% health and mana when a guild member resurrects you |
| 9 | Guild Mail | mail between guild members arrives at once |
| 10 | Mobile Banking | summon a guild chest for 5 minutes (1 hour cooldown) |
| 11 | Honorable Mention | +10% honor from kills |
| 12 | Bartering | 10% lower vendor and trainer prices |
| 13 | Bountiful Bags | 15% chance of one more herb, ore, leather or enchanting material |
| 14 | The Doctor Is In | +25% healing from bandages, not in battlegrounds or arenas |
| 15 | Chug-A-Lug | flasks and elixirs last twice as long (no spellbook entry) |
| 16 | For Great Justice | 20% chance of an extra emblem or badge when looting one |
| 17 | Happy Hour | cauldrons give an extra potion (no spellbook entry) |
| 18 | Have Group, Will Travel | `.guildsummon`: summon your party or raid, out of combat (2 hour cooldown) |
| 19 | Mass Resurrection | resurrect dead party and raid members nearby at 35% health, out of combat; each person once per 10 minutes |
| 20 | Ride Like the Wind | flight paths 25% faster (needs [mod-temucraft-travel](https://github.com/Temucraft/mod-temucraft-travel), which speeds the flights up) |

## Install

1. Put this folder in `modules/` of your AzerothCore source, re-run CMake and build.
2. The SQL in `data/sql/` is applied by the worldserver on its next start: the perk list `mod_temucraft_guild_perk`
   and its texts (world), the scripts of Mobile Banking and Mass Resurrection (`spell_script_names` 83958, 32841),
   and `mod_temucraft_guild_perk_state` (characters, each guild's unlocks and switches).
3. Copy `conf/mod-temucraft-guild-perks.conf.dist` to `configs/modules/` as `mod-temucraft-guild-perks.conf` to
   change the settings. `.reload config` applies them and reads the perk list again (perk spells follow at the
   next login or zone change).

## Requirements

The perk spells (78632, 78633, 78635, 83941, 83943, 83944, 83950, 83951, 83958, 83960, 83964, 83966, 118021,
118076, 118483, 32841, 84559) must be in the server's and the client's Spell.dbc. They are in the Temucraft client data; a stock
3.3.5a Spell.dbc doesn't have them (the module then skips the missing ones).

## Works with other modules

- [mod-temucraft-travel](https://github.com/Temucraft/mod-temucraft-travel)'s hearthstone choice and Hasty Hearth add up (a 30 minute choice becomes 15, 15 becomes 0).
- Bartering lowers the same price discount reputation gives, so trainers (and [mod-temucraft-auto-learn](https://github.com/Temucraft/mod-temucraft-auto-learn)'s prices at
  trainers) are cheaper too.

## Independence

This module doesn't need any other module, nor any other Temucraft module.

Part of the Temucraft modules for AzerothCore 3.3.5a.
