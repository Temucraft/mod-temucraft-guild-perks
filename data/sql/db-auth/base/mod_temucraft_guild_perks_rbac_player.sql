-- mod-temucraft-guild-perks: its commands (all under .temucraft) as RBAC permissions 100750-100799. Every command is admin only by
-- default: linked to the administrator roles 192 and 1014. To give one to players, link it to 195 and 1010 here (or
-- grant it per account with .rbac account grant <account> <id>). "Temucraft: use ..." permissions are not
-- commands: they stay linked to the player roles so the items keep working.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` BETWEEN 100750 AND 100799;
DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 100750 AND 100799;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(100750, 'Command: temucraft guildperks on'),
(100751, 'Command: temucraft guildperks off'),
(100752, 'Command: temucraft guildperks'),
(100753, 'Command: temucraft guildsummon');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(192, 100750),
(1014, 100750),
(192, 100751),
(1014, 100751),
(192, 100752),
(1014, 100752),
(192, 100753),
(1014, 100753);
