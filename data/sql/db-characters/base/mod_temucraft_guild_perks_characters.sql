-- mod-temucraft-guild-perks: what each guild unlocked and switched (NULL = the perk's default).
CREATE TABLE IF NOT EXISTS `mod_temucraft_guild_perk_state` (
    `guild`    INT UNSIGNED NOT NULL,
    `perk`     INT UNSIGNED NOT NULL,
    `unlocked` TINYINT UNSIGNED NULL DEFAULT NULL,
    `enabled`  TINYINT UNSIGNED NULL DEFAULT NULL,
    PRIMARY KEY (`guild`, `perk`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
