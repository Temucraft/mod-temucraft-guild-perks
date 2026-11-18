-- mod-temucraft-guild-perks: its game master commands as RBAC permissions (100701-100702). Each is linked to the
-- stock security role of its old level (194 moderator, 193 game master, 192 administrator) and to the matching
-- Temucraft GM tier (1011 Triager, 1012 Entertainer, 1014 Administrator). Grant or deny them per account with
-- .rbac account grant / deny, or link them to other roles.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` BETWEEN 100701 AND 100799;
DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 100701 AND 100799;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(100701, 'Command: guildperks unlock'),
(100702, 'Command: guildperks lock');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(193, 100701),
(1012, 100701),
(193, 100702),
(1012, 100702);
