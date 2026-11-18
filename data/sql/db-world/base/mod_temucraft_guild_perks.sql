-- mod-temucraft-guild-perks: the perks, what the server allows and each perk's default for every guild. Re-runnable
-- (edit `allowed`, `unlocked`, `enabled` here or in the table; .reload config reads it again).
--   allowed:  the perk exists on this server
--   unlocked: every guild has it unlocked (0 = only guilds a GM unlocks it for, .guildperks unlock <perk> <guild>)
--   enabled:  an unlocked perk is on until the guild master or an officer switches it off (.guildperks off <perk>)
CREATE TABLE IF NOT EXISTS `mod_temucraft_guild_perk` (
    `id`       INT UNSIGNED NOT NULL,
    `name`     VARCHAR(64) NOT NULL,
    `spell`    INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'spellbook entry (0 = none)',
    `allowed`  TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `unlocked` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `enabled`  TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `effect`   VARCHAR(255) NOT NULL DEFAULT '',
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DELETE FROM `mod_temucraft_guild_perk`;
INSERT INTO `mod_temucraft_guild_perk` (`id`, `name`, `spell`, `allowed`, `unlocked`, `enabled`, `effect`) VALUES
(1,  'Fast Track',              78632,  1, 1, 1, '+10% experience from kills and quests'),
(2,  'Mr. Popularity',          78635,  1, 1, 1, '+10% reputation from kills and quests'),
(3,  'Mount Up',                78633,  1, 1, 1, '+10% mounted speed, not in battlegrounds or arenas'),
(4,  'Hasty Hearth',            83944,  1, 1, 1, 'hearthstone cooldown 15 minutes shorter'),
(5,  'Cash Flow',               83941,  1, 1, 1, 'an extra 10% of looted money goes to the guild bank'),
(6,  'Reinforce',               83943,  1, 1, 1, '20% less durability loss when dying'),
(7,  'Working Overtime',        118021, 1, 1, 1, '+20% chance of a skill-up on crafting and gathering'),
(8,  'The Quick and the Dead',  83950,  1, 1, 1, '+100% speed while dead, +50% health when a guild member resurrects you'),
(9,  'Guild Mail',              83951,  1, 1, 1, 'mail between guild members arrives at once'),
(10, 'Mobile Banking',          83958,  1, 1, 1, 'summon a guild chest for 5 minutes (1 hour cooldown)'),
(11, 'Honorable Mention',       83960,  1, 1, 1, '+10% honor from kills'),
(12, 'Bartering',               83964,  1, 1, 1, '10% lower vendor and trainer prices'),
(13, 'Bountiful Bags',          83966,  1, 1, 1, '15% chance of an extra herb, ore, leather or enchanting material'),
(14, 'The Doctor Is In',        118076, 1, 1, 1, '+25% healing from bandages'),
(15, 'Chug-A-Lug',              0,      1, 1, 1, 'flasks and elixirs last twice as long'),
(16, 'For Great Justice',       118483, 1, 1, 1, '20% chance of an extra emblem or badge when looting one'),
(17, 'Happy Hour',              0,      1, 1, 1, 'cauldrons give an extra potion'),
(18, 'Have Group, Will Travel', 0,      1, 1, 1, '.guildsummon: summon your party or raid (2 hour cooldown)'),
(19, 'Mass Resurrection',       32841,  1, 1, 1, 'resurrect dead party members nearby at 35% health, out of combat, each once per 10 minutes'),
(20, 'Ride Like the Wind',      117983, 1, 1, 1, 'flight paths 25% faster (sped up by mod-temucraft-travel)');

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_temucraft_mobile_banking', 'spell_temucraft_guild_mass_resurrection');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(83958, 'spell_temucraft_mobile_banking'),
(32841, 'spell_temucraft_guild_mass_resurrection');

-- Player-facing texts. Translations go in module_string_locale (same module and id, plus the locale).
DELETE FROM `module_string` WHERE `module` = 'mod-temucraft-guild-perks';
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-temucraft-guild-perks', 1, '|cff40ff40Guild perks|r (.guildperks on|off <perk> for the guild master and officers):'),
('mod-temucraft-guild-perks', 2, '  {}. {}: {}'),
('mod-temucraft-guild-perks', 3, 'You are not in a guild.'),
('mod-temucraft-guild-perks', 4, 'Your guild hasn''t unlocked that perk.'),
('mod-temucraft-guild-perks', 5, 'Only the guild master and officers can switch guild perks.'),
('mod-temucraft-guild-perks', 6, '|cff40ff40Guild perks:|r {} is now {} for your guild.'),
('mod-temucraft-guild-perks', 7, '|cff40ff40Guild perks:|r {} {} for {}.'),
('mod-temucraft-guild-perks', 8, 'There''s no such guild perk (.guildperks lists them).'),
('mod-temucraft-guild-perks', 9, 'No such guild.'),
('mod-temucraft-guild-perks', 10, 'You can summon your group again in {} min.'),
('mod-temucraft-guild-perks', 11, 'There''s nobody in your group to summon (out of combat, outside battlegrounds).'),
('mod-temucraft-guild-perks', 12, 'Summoning {} group member(s).'),
('mod-temucraft-guild-perks', 13, '.guildperks | .guildperks on|off <perk> | .guildsummon'),
('mod-temucraft-guild-perks', 14, '|cff40ff40on|r'),
('mod-temucraft-guild-perks', 15, '|cffffff40off|r'),
('mod-temucraft-guild-perks', 16, '|cffff8040locked|r'),
('mod-temucraft-guild-perks', 17, '|cff808080not on this server|r');
