-- mod-temucraft-guild-perks: its player commands (all under .temucraft) as RBAC permissions 100750-100799. Linked to the stock player
-- role 195 and the Temucraft player role 1010, so every account has them by default. To take one away, remove its
-- link here (or deny it per account with .rbac account deny <account> <id>); to give it to staff only, link it to
-- 1011-1014 instead.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` BETWEEN 100750 AND 100799;
DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 100750 AND 100799;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(100750, 'Command: temucraft guildperks on'),
(100751, 'Command: temucraft guildperks off'),
(100752, 'Command: temucraft guildperks'),
(100753, 'Command: temucraft guildsummon');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(195, 100750),
(1010, 100750),
(195, 100751),
(1010, 100751),
(195, 100752),
(1010, 100752),
(195, 100753),
(1010, 100753);
