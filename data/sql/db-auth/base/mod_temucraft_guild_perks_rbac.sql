-- mod-temucraft-guild-perks: its game master commands as RBAC permissions (100701-100702). Each is linked to the
-- stock security role of its old level (194 moderator, 193 game master, 192 administrator) and to the matching
-- Temucraft GM tier (1011 Triager, 1012 Entertainer, 1014 Administrator). Grant or deny them per account with
-- .rbac account grant / deny, or link them to other roles.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` BETWEEN 100701 AND 100749;
DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 100701 AND 100749;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(100701, 'Command: temucraft guildperks unlock'),
(100702, 'Command: temucraft guildperks lock');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(192, 100701),
(1014, 100701),
(192, 100702),
(1014, 100702);
