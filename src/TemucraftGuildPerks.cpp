/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

// Guild perks, as guilds had them in the time of guild levels, all of them. Which perks exist on the server, which
// a guild has unlocked and which the guild has switched on are three different things:
//   - the server allows a perk (world table mod_temucraft_guild_perk, `allowed`);
//   - a guild has it unlocked: every guild by default (`unlocked`), or only when a GM unlocks it for that guild;
//   - an unlocked perk is on unless the guild master or an officer switches it off (`enabled` is the default).
// Perks with a spell the client knows are taught as that spell (a passive in the spellbook with its own tooltip) to
// every member while the perk is on, and taken away when it isn't. The effects are done here.
//
//   1 Fast Track (78632)          +10% experience
//   2 Mr. Popularity (78635)      +10% reputation
//   3 Mount Up (78633)            +10% mounted speed, not in battlegrounds or arenas
//   4 Hasty Hearth (83944)        hearthstone cooldown 15 minutes shorter
//   5 Cash Flow (83941)           an extra 10% of looted money goes to the guild bank
//   6 Reinforce (83943)           20% less durability loss when dying
//   7 Working Overtime (118021)   +20% chance of a skill-up on crafting and gathering
//   8 The Quick and the Dead      +100% speed while dead, +50% health when a guild member resurrects you
//     (83950)
//   9 Guild Mail (83951)          mail between guild members arrives at once
//  10 Mobile Banking (83958)      summons a guild chest for 5 minutes (1 hour cooldown)
//  11 Honorable Mention (83960)   +10% honor from kills
//  12 Bartering (83964)           10% lower vendor prices
//  13 Bountiful Bags (83966)      15% chance of an extra herb, ore, leather or enchanting material
//  14 The Doctor Is In (118076)   +25% healing from bandages
//  15 Chug-A-Lug                  flasks and elixirs last twice as long
//  16 For Great Justice (118483)  20% chance of an extra emblem or badge when looting one
//  17 Happy Hour                  cauldrons give an extra potion
//  18 Have Group, Will Travel     .guildsummon: summon the party or raid (2 hour cooldown)
//  19 Mass Resurrection (32841)   resurrect dead party members nearby at 35% (each once per 10 minutes)
//  20 Ride Like the Wind (117983) faster flight paths: members know the perk spell; mod-temucraft-travel's Swift
//     flights speeds their flights up (25% by default) for as long as they know it
//
// .guildperks lists them; the guild master and officers switch them with .guildperks on|off <perk>; GMs unlock and
// lock them per guild with .guildperks unlock|lock <perk> [guild name].

#include "CharacterCache.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Item.h"
#include "LootMgr.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "StringConvert.h"
#include "TemucraftModuleText.h"
#include "WorldPacket.h"
#include <algorithm>
#include <map>
#include <unordered_map>

using namespace Acore::ChatCommands;

namespace
{
    constexpr char const* MODULE = "mod-temucraft-guild-perks";

    enum Perk : uint32
    {
        FAST_TRACK = 1, MR_POPULARITY, MOUNT_UP, HASTY_HEARTH, CASH_FLOW, REINFORCE, WORKING_OVERTIME, QUICK_AND_DEAD,
        GUILD_MAIL, MOBILE_BANKING, HONORABLE_MENTION, BARTERING, BOUNTIFUL_BAGS, DOCTOR_IS_IN, CHUG_A_LUG,
        FOR_GREAT_JUSTICE, HAPPY_HOUR, HAVE_GROUP, MASS_RESURRECTION, RIDE_LIKE_THE_WIND
    };

    struct PerkDef
    {
        std::string name;
        uint32 spell = 0;
        bool allowed = true;
        bool unlocked = true;   // for every guild without its own row
        bool enabled = true;    // once unlocked, unless the guild switched it off
    };
    std::map<uint32, PerkDef> perks;

    struct GuildState { int8 unlocked = -1; int8 enabled = -1; };   // -1: the perk's default
    std::unordered_map<uint32, std::map<uint32, GuildState>> guildStates;   // guild id -> perk -> state

    // Lower ranks taken away so only the best rank shows (Fast Track, Mr. Popularity, Cash Flow, Reinforce,
    // Working Overtime, Honorable Mention, For Great Justice).
    uint32 const LOWER_RANKS[] = { 78631, 78634, 83940, 83942, 83949, 83959, 83953 };

    constexpr uint32 SPELL_DEAD_SPEED     = 84559;
    constexpr uint32 SPELL_RECENT_BANDAGE = 11196;
    constexpr uint32 HEARTH_CATEGORY      = 1176;
    uint32 const EMBLEMS[] = { 29434, 40752, 40753, 45624, 47241, 49426 };
    uint32 const CAULDRON_POTIONS[] = { 22841, 22842, 22844, 22845, 22846, 22847, 22848 };

    enum Text : uint32
    {
        T_HEAD = 1, T_ROW, T_NO_GUILD, T_NOT_ALLOWED, T_NO_RIGHTS, T_SWITCHED, T_UNLOCKED, T_UNKNOWN_PERK, T_UNKNOWN_GUILD,
        T_SUMMON_WAIT, T_SUMMON_NONE, T_SUMMON_SENT, T_HELP, T_STATE_ON, T_STATE_OFF, T_STATE_LOCKED, T_STATE_DISALLOWED
    };
    char const* Say(Player* player, uint32 id, char const* fallback) { return TemucraftModuleText(MODULE, player, id, fallback); }

    struct Settings
    {
        bool enable = true;
        uint32 minMembers = 1;
        uint8 manageRank = 1;
        uint32 bankChest = 187299;
    } cfg;

    std::unordered_map<uint32, uint32> summonReady;   // player guid -> game time when .guildsummon is ready again
    std::unordered_map<uint32, uint32> massResLock;   // player guid -> game time until they can be mass-resurrected again

    void LoadSettings()
    {
        cfg.enable = sConfigMgr->GetOption<bool>("Temucraft.GuildPerks.Enable", true);
        cfg.minMembers = sConfigMgr->GetOption<uint32>("Temucraft.GuildPerks.MinMembers", 1);
        cfg.manageRank = uint8(sConfigMgr->GetOption<uint32>("Temucraft.GuildPerks.ManageRank", 1));
        cfg.bankChest = sConfigMgr->GetOption<uint32>("Temucraft.GuildPerks.MobileBanking.Chest", 187299);
    }

    void LoadPerks()
    {
        perks.clear();
        if (QueryResult r = WorldDatabase.Query("SELECT `id`, `name`, `spell`, `allowed`, `unlocked`, `enabled` FROM `mod_temucraft_guild_perk`"))
            do
            {
                Field* f = r->Fetch();
                PerkDef d;
                d.name = f[1].Get<std::string>();
                d.spell = f[2].Get<uint32>();
                if (d.spell && !sSpellMgr->GetSpellInfo(d.spell))
                    d.spell = 0;   // not in this server's spell data: the perk works without a spellbook entry
                d.allowed = f[3].Get<uint8>() != 0;
                d.unlocked = f[4].Get<uint8>() != 0;
                d.enabled = f[5].Get<uint8>() != 0;
                perks[f[0].Get<uint32>()] = d;
            } while (r->NextRow());
        guildStates.clear();
        if (QueryResult r = CharacterDatabase.Query("SELECT `guild`, `perk`, `unlocked`, `enabled` FROM `mod_temucraft_guild_perk_state`"))
            do
            {
                Field* f = r->Fetch();
                GuildState& st = guildStates[f[0].Get<uint32>()][f[1].Get<uint32>()];
                st.unlocked = f[2].IsNull() ? int8(-1) : int8(f[2].Get<uint8>());
                st.enabled = f[3].IsNull() ? int8(-1) : int8(f[3].Get<uint8>());
            } while (r->NextRow());
        LOG_INFO("server.loading", "{}: {} guild perks.", MODULE, perks.size());
    }

    GuildState StateOf(uint32 guildId, uint32 perk)
    {
        auto g = guildStates.find(guildId);
        if (g != guildStates.end())
            if (auto p = g->second.find(perk); p != g->second.end())
                return p->second;
        return {};
    }

    bool IsUnlocked(uint32 guildId, uint32 perk)
    {
        auto d = perks.find(perk);
        if (d == perks.end() || !d->second.allowed)
            return false;
        GuildState st = StateOf(guildId, perk);
        return st.unlocked < 0 ? d->second.unlocked : st.unlocked != 0;
    }

    bool IsActive(uint32 guildId, uint32 perk)
    {
        if (!IsUnlocked(guildId, perk))
            return false;
        GuildState st = StateOf(guildId, perk);
        return st.enabled < 0 ? perks[perk].enabled : st.enabled != 0;
    }

    bool Qualifies(Player* player)
    {
        if (!cfg.enable || !player || !player->GetGuildId())
            return false;
        Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId());
        return guild && guild->GetMemberSize() >= cfg.minMembers;
    }

    bool Has(Player* player, uint32 perk)
    {
        return Qualifies(player) && IsActive(player->GetGuildId(), perk);
    }

    // Teach the perk spells a member should have, take away the others.
    void Sync(Player* player)
    {
        bool const member = Qualifies(player);
        for (auto const& [id, d] : perks)
        {
            if (!d.spell)
                continue;
            bool const want = member && IsActive(player->GetGuildId(), id);
            if (want && !player->HasSpell(d.spell))
                player->learnSpell(d.spell);
            else if (!want && player->HasSpell(d.spell))
                player->removeSpell(d.spell, SPEC_MASK_ALL, false);
        }
        for (uint32 spell : LOWER_RANKS)
            if (player->HasSpell(spell))
                player->removeSpell(spell, SPEC_MASK_ALL, false);
        uint32 const mountUp = perks.count(MOUNT_UP) ? perks[MOUNT_UP].spell : 0;
        if (mountUp && player->HasSpell(mountUp))
        {
            if (player->InBattleground() || player->InArena())
                player->RemoveAurasDueToSpell(mountUp);
            else if (!player->HasAura(mountUp))
                player->CastSpell(player, mountUp, true);
        }
    }

    void SyncGuild(uint32 guildId)
    {
        for (auto const& [guid, player] : ObjectAccessor::GetPlayers())
            if (player && player->IsInWorld() && player->GetGuildId() == guildId)
                Sync(player);
    }

    void SaveState(uint32 guildId, uint32 perk)
    {
        GuildState st = StateOf(guildId, perk);
        CharacterDatabase.Execute("REPLACE INTO `mod_temucraft_guild_perk_state` (`guild`, `perk`, `unlocked`, `enabled`) VALUES ({}, {}, {}, {})",
            guildId, perk, st.unlocked < 0 ? std::string("NULL") : std::to_string(st.unlocked), st.enabled < 0 ? std::string("NULL") : std::to_string(st.enabled));
    }

    uint32 FindPerk(std::string text)
    {
        if (Optional<uint32> id = Acore::StringTo<uint32>(text); id && perks.count(*id))
            return *id;
        auto simplify = [](std::string s)
        {
            std::string out;
            for (char c : s)
                if (std::isalnum(static_cast<unsigned char>(c)))
                    out += char(std::tolower(static_cast<unsigned char>(c)));
            return out;
        };
        std::string const want = simplify(text);
        for (auto const& [id, d] : perks)
            if (!want.empty() && simplify(d.name).compare(0, want.size(), want) == 0)
                return id;
        return 0;
    }

    // Reinforce: durability before a death or a spirit healer's resurrection, to give part of the loss back
    std::unordered_map<uint32, std::unordered_map<uint8, uint32>> durability;

    void Snapshot(Player* player)
    {
        auto& map = durability[player->GetGUID().GetCounter()];
        map.clear();
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                if (item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY))
                    map[slot] = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
    }

    void GiveBackDurability(Player* player)
    {
        auto itr = durability.find(player->GetGUID().GetCounter());
        if (itr == durability.end())
            return;
        for (auto const& [slot, before] : itr->second)
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            {
                uint32 const now = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
                if (now < before)
                    if (int32 back = int32(before - now) * 20 / 100)
                        player->DurabilityPointsLoss(item, -back);
            }
        durability.erase(itr);
    }

    void AfterLoss(Player* player)
    {
        player->m_Events.AddEventAtOffset([player]() { GiveBackDurability(player); }, 500ms);
    }

    bool IsGatheredMaterial(ItemTemplate const* proto)
    {
        if (proto->Class != ITEM_CLASS_TRADE_GOODS)
            return false;
        switch (proto->SubClass)
        {
            case ITEM_SUBCLASS_LEATHER: case ITEM_SUBCLASS_METAL_STONE: case ITEM_SUBCLASS_HERB: case ITEM_SUBCLASS_ENCHANTING:
                return true;
            default:
                return false;
        }
    }

    template <std::size_t N>
    bool In(uint32 const (&list)[N], uint32 id) { return std::find(std::begin(list), std::end(list), id) != std::end(list); }
}

class TemucraftGuildPerksPlayer : public PlayerScript
{
public:
    TemucraftGuildPerksPlayer() : PlayerScript("TemucraftGuildPerksPlayer", {
        PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_UPDATE_ZONE, PLAYERHOOK_ON_SPELL_CAST,
        PLAYERHOOK_ON_BEFORE_LOOT_MONEY, PLAYERHOOK_ON_PLAYER_JUST_DIED, PLAYERHOOK_ON_PLAYER_RESURRECT,
        PLAYERHOOK_ON_PLAYER_RELEASED_GHOST, PLAYERHOOK_ON_UPDATE_CRAFTING_SKILL, PLAYERHOOK_ON_UPDATE_GATHERING_SKILL,
        PLAYERHOOK_ON_VICTIM_REWARD_AFTER, PLAYERHOOK_ON_GET_REPUTATION_PRICE_DISCOUNT, PLAYERHOOK_ON_LOOT_ITEM }) { }

    void OnPlayerLogin(Player* player) override { Sync(player); }
    void OnPlayerLogout(Player* player) override { durability.erase(player->GetGUID().GetCounter()); }
    void OnPlayerUpdateZone(Player* player, uint32 /*newZone*/, uint32 /*newArea*/) override { Sync(player); }

    // Hasty Hearth: 15 minutes off the hearthstone's cooldown (and that of everything sharing it)
    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!spell || !spell->GetSpellInfo() || spell->GetSpellInfo()->GetCategory() != HEARTH_CATEGORY || !Has(player, HASTY_HEARTH))
            return;
        player->m_Events.AddEventAtOffset([player]()
        {
            auto itr = sSpellsByCategoryStore.find(HEARTH_CATEGORY);
            if (itr == sSpellsByCategoryStore.end())
                return;
            for (auto const& [byItem, spellId] : itr->second)
                if (player->HasSpellCooldown(spellId))
                    player->ModifySpellCooldown(spellId, -int32(15 * MINUTE * IN_MILLISECONDS));
        }, 400ms);
    }

    // Cash Flow: an extra 10% of looted money for the guild bank (the looter loses nothing)
    void OnPlayerBeforeLootMoney(Player* player, Loot* loot) override
    {
        if (!loot || !loot->gold || !Has(player, CASH_FLOW))
            return;
        uint32 const extra = loot->gold / 10;
        if (!extra)
            return;
        if (Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
        {
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            if (guild->ModifyBankMoney(trans, extra, true))
                CharacterDatabase.CommitTransaction(trans);
        }
    }

    // Reinforce: give back 20% of what a death or a spirit healer took
    void OnPlayerJustDied(Player* player) override
    {
        if (Has(player, REINFORCE))
        {
            Snapshot(player);
            AfterLoss(player);
        }
    }

    void OnPlayerResurrect(Player* player, float /*restorePercent*/, bool& /*applySickness*/) override
    {
        player->RemoveAurasDueToSpell(SPELL_DEAD_SPEED);
        if (Has(player, REINFORCE))
        {
            Snapshot(player);
            AfterLoss(player);
        }
        // The Quick and the Dead: +50% health and mana when a guild member brings you back
        if (Has(player, QUICK_AND_DEAD) && player->isResurrectRequested())
            for (auto const& [guid, other] : ObjectAccessor::GetPlayers())
                if (other && other != player && other->GetGuildId() == player->GetGuildId() && player->isResurrectRequestedBy(other->GetGUID()))
                {
                    player->m_Events.AddEventAtOffset([player]()
                    {
                        if (!player->IsAlive())
                            return;
                        player->SetHealth(std::min(player->GetMaxHealth(), player->GetHealth() * 3 / 2));
                        if (player->getPowerType() == POWER_MANA)
                            player->SetPower(POWER_MANA, std::min(player->GetMaxPower(POWER_MANA), player->GetPower(POWER_MANA) * 3 / 2));
                    }, 200ms);
                    break;
                }
    }

    // The Quick and the Dead: +100% speed as a ghost
    void OnPlayerReleasedGhost(Player* player) override
    {
        if (!Has(player, QUICK_AND_DEAD) || player->InBattleground() || player->InArena())
            return;
        if (Aura* aura = player->AddAura(SPELL_DEAD_SPEED, player))
            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                if (AuraEffect* effect = aura->GetEffect(i))
                    effect->ChangeAmount(100);
    }

    // Working Overtime: +20% chance of a skill-up, never on grey recipes or nodes
    void OnPlayerUpdateCraftingSkill(Player* player, SkillLineAbilityEntry const* skill, uint32 currentLevel, uint32& gain) override
    {
        if (!gain && skill && currentLevel < skill->TrivialSkillLineRankHigh && Has(player, WORKING_OVERTIME) && roll_chance_i(20))
            gain = 1;
    }

    void OnPlayerUpdateGatheringSkill(Player* player, uint32 /*skillId*/, uint32 current, uint32 gray, uint32 /*green*/, uint32 /*yellow*/, uint32& gain) override
    {
        if (!gain && current < gray && Has(player, WORKING_OVERTIME) && roll_chance_i(20))
            gain = 1;
    }

    // Honorable Mention: +10% honor from kills
    void OnPlayerVictimRewardAfter(Player* player, Player* /*victim*/, uint32& /*killerTitle*/, int32& /*victimRank*/, float& honor) override
    {
        if (Has(player, HONORABLE_MENTION))
            honor *= 1.10f;
    }

    // Bartering: 10% lower vendor (and trainer) prices
    void OnPlayerGetReputationPriceDiscount(Player const* player, Creature const* /*creature*/, float& discount) override
    {
        if (Has(const_cast<Player*>(player), BARTERING))
            discount *= 0.90f;
    }
    void OnPlayerGetReputationPriceDiscount(Player const* player, FactionTemplateEntry const* /*faction*/, float& discount) override
    {
        if (Has(const_cast<Player*>(player), BARTERING))
            discount *= 0.90f;
    }

    // Bountiful Bags, For Great Justice, Happy Hour: sometimes one more of what was looted
    void OnPlayerLootItem(Player* player, Item* item, uint32 /*count*/, ObjectGuid lootGuid) override
    {
        if (!item || !Qualifies(player))
            return;
        uint32 const entry = item->GetEntry();
        if (IsGatheredMaterial(item->GetTemplate()) && (lootGuid.IsGameObject() || lootGuid.IsCreature() || lootGuid.IsItem())
            && Has(player, BOUNTIFUL_BAGS) && roll_chance_i(15))
            player->AddItem(entry, 1);
        else if (In(EMBLEMS, entry) && Has(player, FOR_GREAT_JUSTICE) && roll_chance_i(20))
            player->AddItem(entry, 1);
        else if (In(CAULDRON_POTIONS, entry) && lootGuid.IsGameObject() && Has(player, HAPPY_HOUR))
            player->AddItem(entry, 1);
    }
};

class TemucraftGuildPerksUnit : public UnitScript
{
public:
    TemucraftGuildPerksUnit() : UnitScript("TemucraftGuildPerksUnit", true, { UNITHOOK_MODIFY_HEAL_RECEIVED, UNITHOOK_ON_AURA_APPLY }) { }

    // The Doctor Is In: +25% healing from bandages
    void ModifyHealReceived(Unit* target, Unit* /*healer*/, uint32& heal, SpellInfo const* spellInfo) override
    {
        Player* player = target ? target->ToPlayer() : nullptr;
        if (!player || !spellInfo || spellInfo->ExcludeTargetAuraSpell != SPELL_RECENT_BANDAGE)
            return;
        if (Has(player, DOCTOR_IS_IN) && !player->InBattleground() && !player->InArena())
            heal = heal * 125 / 100;
    }

    // Chug-A-Lug: flasks and elixirs last twice as long
    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        Player* player = unit ? unit->ToPlayer() : nullptr;
        if (!player || !aura || aura->GetCasterGUID() != player->GetGUID() || !Has(player, CHUG_A_LUG))
            return;
        uint32 const id = aura->GetId();
        if (!sSpellMgr->IsSpellMemberOfSpellGroup(id, SPELL_GROUP_ELIXIR_BATTLE) && !sSpellMgr->IsSpellMemberOfSpellGroup(id, SPELL_GROUP_ELIXIR_GUARDIAN))
            return;
        if (aura->GetMaxDuration() <= 0 || aura->GetDuration() != aura->GetMaxDuration())
            return;   // only freshly applied ones (not reloaded at login)
        aura->SetMaxDuration(aura->GetMaxDuration() * 2);
        aura->SetDuration(aura->GetMaxDuration());
    }
};

// Guild Mail: mail between guild members arrives at once
class TemucraftGuildPerksMail : public MailScript
{
public:
    TemucraftGuildPerksMail() : MailScript("TemucraftGuildPerksMail", { MAILHOOK_ON_BEFORE_MAIL_DRAFT_SEND_MAIL_TO }) { }

    void OnBeforeMailDraftSendMailTo(MailDraft* /*draft*/, MailReceiver const& receiver, MailSender const& sender, MailCheckMask& /*checked*/,
        uint32& deliverDelay, uint32& /*expiration*/, bool& /*deleteItems*/, bool& /*send*/) override
    {
        if (!deliverDelay || !cfg.enable || sender.GetMailMessageType() != MAIL_NORMAL)
            return;
        ObjectGuid const from = ObjectGuid::Create<HighGuid::Player>(sender.GetSenderId());
        ObjectGuid const to = ObjectGuid::Create<HighGuid::Player>(receiver.GetPlayerGUIDLow());
        uint32 const guild = sCharacterCache->GetCharacterGuildIdByGuid(from);
        if (guild && guild == sCharacterCache->GetCharacterGuildIdByGuid(to) && IsActive(guild, GUILD_MAIL))
            deliverDelay = 0;
    }
};

class TemucraftGuildPerksGuild : public GuildScript
{
public:
    TemucraftGuildPerksGuild() : GuildScript("TemucraftGuildPerksGuild", { GUILDHOOK_ON_ADD_MEMBER, GUILDHOOK_ON_REMOVE_MEMBER, GUILDHOOK_ON_DISBAND }) { }

    void OnAddMember(Guild* /*guild*/, Player* player, uint8& /*rank*/) override
    {
        if (player)
            player->m_Events.AddEventAtOffset([player]() { Sync(player); }, 1s);
    }

    void OnRemoveMember(Guild* /*guild*/, Player* player, bool /*disbanding*/, bool /*kicked*/) override
    {
        if (player)
            player->m_Events.AddEventAtOffset([player]() { Sync(player); }, 1s);
    }

    void OnDisband(Guild* guild) override
    {
        if (!guild)
            return;
        guildStates.erase(guild->GetId());
        CharacterDatabase.Execute("DELETE FROM `mod_temucraft_guild_perk_state` WHERE `guild` = {}", guild->GetId());
    }
};

class TemucraftGuildPerksWorld : public WorldScript
{
public:
    TemucraftGuildPerksWorld() : WorldScript("TemucraftGuildPerksWorld", { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_STARTUP }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        LoadSettings();
        if (reload)
            LoadPerks();
    }

    void OnStartup() override { LoadPerks(); }
};

// Mobile Banking: a guild chest beside the caster for 5 minutes
class spell_temucraft_mobile_banking : public SpellScript
{
    PrepareSpellScript(spell_temucraft_mobile_banking);

    SpellCastResult CheckGuild()
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        return player && Has(player, MOBILE_BANKING) ? SPELL_CAST_OK : SPELL_FAILED_NOT_READY;
    }

    void HandleSummon(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!player)
            return;
        float x, y, z;
        player->GetClosePoint(x, y, z, player->GetCombatReach(), 2.0f);
        player->SummonGameObject(cfg.bankChest, x, y, z, player->GetOrientation() + float(M_PI), 0, 0, 0, 0, 5 * MINUTE);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_temucraft_mobile_banking::CheckGuild);
        OnEffectHit += SpellEffectFn(spell_temucraft_mobile_banking::HandleSummon, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// Mass Resurrection: dead party and raid members nearby come back at 35% health, each once per 10 minutes
class spell_temucraft_guild_mass_resurrection : public SpellScript
{
    PrepareSpellScript(spell_temucraft_guild_mass_resurrection);

    SpellCastResult CheckCast()
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!player)
            return SPELL_CAST_OK;   // creatures use the spell as it is
        if (!Has(player, MASS_RESURRECTION))
            return SPELL_FAILED_NOT_READY;
        return player->IsInCombat() ? SPELL_FAILED_AFFECTING_COMBAT : SPELL_CAST_OK;
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        if (!GetCaster() || !GetCaster()->IsPlayer())
            return;
        uint32 const now = uint32(GameTime::GetGameTime().count());
        targets.remove_if([now](WorldObject* target)
        {
            auto itr = massResLock.find(target->GetGUID().GetCounter());
            return itr != massResLock.end() && itr->second > now;
        });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (!GetCaster() || !GetCaster()->IsPlayer())
            return;
        if (Player* target = GetHitPlayer())
        {
            massResLock[target->GetGUID().GetCounter()] = uint32(GameTime::GetGameTime().count()) + 10 * MINUTE;
            SetEffectValue(int32(target->GetMaxHealth() * 35 / 100));
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_temucraft_guild_mass_resurrection::CheckCast);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_temucraft_guild_mass_resurrection::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_PARTY);
        OnEffectHitTarget += SpellEffectFn(spell_temucraft_guild_mass_resurrection::HandleHit, EFFECT_0, SPELL_EFFECT_RESURRECT_NEW);
    }
};

class TemucraftGuildPerksCommands : public CommandScript
{
public:
    TemucraftGuildPerksCommands() : CommandScript("TemucraftGuildPerksCommands") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable perksTable =
        {
            { "on",     HandleOn,     SEC_PLAYER,     Console::No },
            { "off",    HandleOff,    SEC_PLAYER,     Console::No },
            { "unlock", HandleUnlock, 100701 /* SEC_GAMEMASTER -> RBAC */, Console::No },
            { "lock",   HandleLock,   100702 /* SEC_GAMEMASTER -> RBAC */, Console::No },
            { "",       HandleList,   SEC_PLAYER,     Console::No },
        };
        static ChatCommandTable root =
        {
            { "guildperks",  perksTable },
            { "guildsummon", HandleSummon, SEC_PLAYER, Console::No },
        };
        return root;
    }

    static bool HandleList(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;
        uint32 const guild = player->GetGuildId();
        handler->SendSysMessage(Say(player, T_HEAD, "|cff40ff40Guild perks|r (.guildperks on|off <perk> for the guild master and officers):"));
        for (auto const& [id, d] : perks)
        {
            char const* state = !d.allowed ? Say(player, T_STATE_DISALLOWED, "|cff808080not on this server|r")
                : !guild || !IsUnlocked(guild, id) ? Say(player, T_STATE_LOCKED, "|cffff8040locked|r")
                : IsActive(guild, id) ? Say(player, T_STATE_ON, "|cff40ff40on|r") : Say(player, T_STATE_OFF, "|cffffff40off|r");
            handler->PSendSysMessage(Say(player, T_ROW, "  {}. {}: {}"), id, d.name, state);
        }
        if (!guild)
            handler->SendSysMessage(Say(player, T_NO_GUILD, "You are not in a guild."));
        return true;
    }

    static bool Switch(ChatHandler* handler, std::string const& perkText, bool on)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;
        Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId());
        if (!guild)
        {
            handler->SendSysMessage(Say(player, T_NO_GUILD, "You are not in a guild."));
            return true;
        }
        Guild::Member const* member = guild->GetMember(player->GetGUID());
        if (!member || (member->GetRankId() > cfg.manageRank && guild->GetLeaderGUID() != player->GetGUID()))
        {
            handler->SendSysMessage(Say(player, T_NO_RIGHTS, "Only the guild master and officers can switch guild perks."));
            return true;
        }
        uint32 const perk = FindPerk(perkText);
        if (!perk)
        {
            handler->SendSysMessage(Say(player, T_UNKNOWN_PERK, "There's no such guild perk (.guildperks lists them)."));
            return true;
        }
        if (!IsUnlocked(guild->GetId(), perk))
        {
            handler->SendSysMessage(Say(player, T_NOT_ALLOWED, "Your guild hasn't unlocked that perk."));
            return true;
        }
        guildStates[guild->GetId()][perk].enabled = on ? 1 : 0;
        SaveState(guild->GetId(), perk);
        SyncGuild(guild->GetId());
        handler->PSendSysMessage(Say(player, T_SWITCHED, "|cff40ff40Guild perks:|r {} is now {} for your guild."), perks[perk].name,
            on ? Say(player, T_STATE_ON, "|cff40ff40on|r") : Say(player, T_STATE_OFF, "|cffffff40off|r"));
        return true;
    }
    static bool HandleOn(ChatHandler* handler, Tail perk) { return Switch(handler, std::string(perk), true); }
    static bool HandleOff(ChatHandler* handler, Tail perk) { return Switch(handler, std::string(perk), false); }

    // .guildperks unlock|lock <perk> [guild name] (without a name: the GM's own guild)
    static bool Unlock(ChatHandler* handler, std::string text, bool unlock)
    {
        Player* player = handler->GetPlayer();
        std::string perkText = text, guildName;
        if (std::size_t space = text.find(' '); space != std::string::npos)
        {
            perkText = text.substr(0, space);
            guildName = text.substr(space + 1);
        }
        uint32 const perk = FindPerk(perkText);
        Guild* guild = guildName.empty() ? (player ? sGuildMgr->GetGuildById(player->GetGuildId()) : nullptr) : sGuildMgr->GetGuildByName(guildName);
        if (!perk)
        {
            handler->SendSysMessage(Say(player, T_UNKNOWN_PERK, "There's no such guild perk (.guildperks lists them)."));
            return true;
        }
        if (!guild)
        {
            handler->SendSysMessage(Say(player, T_UNKNOWN_GUILD, "No such guild."));
            return true;
        }
        guildStates[guild->GetId()][perk].unlocked = unlock ? 1 : 0;
        SaveState(guild->GetId(), perk);
        SyncGuild(guild->GetId());
        handler->PSendSysMessage(Say(player, T_UNLOCKED, "|cff40ff40Guild perks:|r {} {} for {}."), perks[perk].name, unlock ? "unlocked" : "locked", guild->GetName());
        return true;
    }
    static bool HandleUnlock(ChatHandler* handler, Tail text) { return Unlock(handler, std::string(text), true); }
    static bool HandleLock(ChatHandler* handler, Tail text) { return Unlock(handler, std::string(text), false); }

    // Have Group, Will Travel: summon the party or raid (they accept like a summoning stone's)
    static bool HandleSummon(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;
        if (!Has(player, HAVE_GROUP))
        {
            handler->SendSysMessage(Say(player, T_NOT_ALLOWED, "Your guild hasn't unlocked that perk."));
            return true;
        }
        uint32 const now = uint32(GameTime::GetGameTime().count());
        uint32& ready = summonReady[player->GetGUID().GetCounter()];
        if (ready > now)
        {
            handler->PSendSysMessage(Say(player, T_SUMMON_WAIT, "You can summon your group again in {} min."), (ready - now) / MINUTE + 1);
            return true;
        }
        Group* group = player->GetGroup();
        uint32 sent = 0;
        if (group && !player->IsInCombat() && !player->GetMap()->IsBattlegroundOrArena())
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (Player* member = ref->GetSource(); member && member != player && member->IsAlive() && !member->IsInCombat())
                {
                    member->SetSummonPoint(player->GetMapId(), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());
                    WorldPacket data(SMSG_SUMMON_REQUEST, 8 + 4 + 4);
                    data << player->GetGUID();
                    data << uint32(player->GetZoneId());
                    data << uint32(MAX_PLAYER_SUMMON_DELAY * IN_MILLISECONDS);
                    member->SendDirectMessage(&data);
                    ++sent;
                }
        if (!sent)
        {
            handler->SendSysMessage(Say(player, T_SUMMON_NONE, "There's nobody in your group to summon (out of combat, outside battlegrounds)."));
            return true;
        }
        ready = now + 2 * HOUR;
        handler->PSendSysMessage(Say(player, T_SUMMON_SENT, "Summoning {} group member(s)."), sent);
        return true;
    }
};

void AddTemucraftGuildPerksScripts()
{
    new TemucraftGuildPerksPlayer();
    new TemucraftGuildPerksUnit();
    new TemucraftGuildPerksMail();
    new TemucraftGuildPerksGuild();
    new TemucraftGuildPerksWorld();
    new TemucraftGuildPerksCommands();
    RegisterSpellScript(spell_temucraft_mobile_banking);
    RegisterSpellScript(spell_temucraft_guild_mass_resurrection);
}
