/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "World.h"
#include "Player.h"
#include "Chat.h"
#include "Language.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "SystemConfig.h"
#include "revision.h"
#include "Util.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "TemporarySummon.h"
#include "Totem.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "WaypointManager.h"
#include "WaypointMovementGenerator.h"
#include "TargetedMovementGenerator.h"

#include <fstream>

bool ChatHandler::HandleNpcInfoCommand(char* /*args*/)
{
    Creature* target = GetSelectedCreature();

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 faction = target->GetFactionTemplateId();
    uint32 npcflags = target->GetUInt32Value(UNIT_NPC_FLAGS);
    uint32 displayid = target->GetDisplayId();
    uint32 nativeid = target->GetNativeDisplayId();
    uint32 Entry = target->GetEntry();
    CreatureInfo const* cInfo = target->GetCreatureInfo();

    time_t curRespawnDelay = target->GetRespawnTimeEx() - time(nullptr);
    if (curRespawnDelay < 0)
        curRespawnDelay = 0;
    std::string curRespawnDelayStr = secsToTimeString(curRespawnDelay, true);
    std::string defRespawnDelayStr = secsToTimeString(target->GetRespawnDelay(), true);

    PSendSysMessage(LANG_NPCINFO_CHAR, target->GetGuidStr().c_str(), faction, npcflags, Entry, displayid, nativeid);
    PSendSysMessage(LANG_NPCINFO_LEVEL, target->GetLevel());
    PSendSysMessage(LANG_NPCINFO_EQUIPMENT, target->GetCurrentEquipmentId());
    PSendSysMessage(LANG_NPCINFO_HEALTH, target->GetCreateHealth(), target->GetMaxHealth(), target->GetHealth());
    if (target->GetPowerType() == POWER_MANA)
        PSendSysMessage(LANG_NPCINFO_MANA, target->GetCreateMana(), target->GetMaxPower(POWER_MANA), target->GetPower(POWER_MANA));
    PSendSysMessage(LANG_NPCINFO_INHABIT_TYPE, cInfo->inhabit_type);
    PSendSysMessage(LANG_NPCINFO_FLAGS, target->GetUInt32Value(UNIT_FIELD_FLAGS), target->GetUInt32Value(UNIT_DYNAMIC_FLAGS), target->GetFactionTemplateId());
    PSendSysMessage(LANG_COMMAND_RAWPAWNTIMES, defRespawnDelayStr.c_str(), curRespawnDelayStr.c_str());
    PSendSysMessage(LANG_NPCINFO_LOOT, cInfo->loot_id, cInfo->pickpocket_loot_id, cInfo->skinning_loot_id);
    PSendSysMessage(LANG_NPCINFO_ARMOR, target->GetArmor());
    PSendSysMessage(LANG_NPCINFO_DUNGEON_ID, target->GetInstanceId());
    PSendSysMessage(LANG_NPCINFO_POSITION, float(target->GetPositionX()), float(target->GetPositionY()), float(target->GetPositionZ()));
    PSendSysMessage(LANG_NPCINFO_AIINFO, target->GetAIName().c_str(), target->GetScriptName().c_str());
    PSendSysMessage(LANG_NPCINFO_ACTIVE_VISIBILITY, target->isActiveObject(), target->GetVisibilityModifier());

    if ((npcflags & UNIT_NPC_FLAG_VENDOR))
        SendSysMessage(LANG_NPCINFO_VENDOR);

    if ((npcflags & UNIT_NPC_FLAG_TRAINER))
        SendSysMessage(LANG_NPCINFO_TRAINER);

    ShowNpcOrGoSpawnInformation<Creature>(target->GetGUIDLow());
    return true;
}

bool ChatHandler::HandleNpcAIInfoCommand(char* /*args*/)
{
    Creature* pTarget = GetSelectedCreature();

    if (!pTarget || !pTarget->AI())
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_NPC_AI_HEADER, pTarget->GetEntry());

    std::string strScript = pTarget->GetScriptName();
    std::string strAI = pTarget->GetAIName();
    char const* cstrAIClass = pTarget->AI() ? typeid(*pTarget->AI()).name() : " - ";

    PSendSysMessage(LANG_NPC_AI_NAMES,
                    strAI.empty() ? " - " : strAI.c_str(),
                    cstrAIClass ? cstrAIClass : " - ",
                    strScript.empty() ? " - " : strScript.c_str());
    PSendSysMessage(LANG_NPC_AI_MOVE, GetOnOffStr(pTarget->AI()->IsCombatMovementEnabled()));
    PSendSysMessage(LANG_NPC_AI_ATTACK, GetOnOffStr(pTarget->AI()->IsMeleeAttackEnabled()));
    PSendSysMessage(LANG_NPC_MOTION_TYPE, pTarget->GetMotionMaster()->GetCurrentMovementGeneratorType());
    pTarget->AI()->GetAIInformation(*this);

    return true;
}

bool ChatHandler::HandleNpcChangeEntryCommand(char *args)
{
    if (!*args)
        return false;

    uint32 newEntryNum = atoi(args);
    if (!newEntryNum)
        return false;

    Unit* unit = GetSelectedUnit();
    if (!unit || unit->GetTypeId() != TYPEID_UNIT)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }
    Creature* creature = (Creature*)unit;
    if (creature->UpdateEntry(newEntryNum))
        SendSysMessage(LANG_DONE);
    else
        SendSysMessage(LANG_ERROR);
    return true;
}

bool ChatHandler::HandleNpcChangeLevelCommand(char* args)
{
    if (!*args)
        return false;

    uint8 lvl = (uint8) atoi(args);
    if (lvl < 1 || lvl > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL) + 3)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    Creature* pCreature = GetSelectedCreature();
    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (pCreature->IsPet())
        ((Pet*)pCreature)->GivePetLevel(lvl);
    else
    {
        pCreature->SetMaxHealth(100 + 30 * lvl);
        pCreature->SetHealth(100 + 30 * lvl);
        pCreature->SetLevel(lvl);

        if (pCreature->HasStaticDBSpawnData())
            pCreature->SaveToDB();
    }

    return true;
}

bool ChatHandler::HandleNpcSetModelCommand(char* args)
{
    if (!*args)
        return false;

    uint32 displayId = (uint32) atoi(args);

    Creature* pCreature = GetSelectedCreature();

    if (!pCreature || pCreature->IsPet())
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->SetDisplayId(displayId);
    pCreature->SetNativeDisplayId(displayId);

    if (pCreature->HasStaticDBSpawnData())
        pCreature->SaveToDB();

    return true;
}

bool ChatHandler::HandleNpcFactionIdCommand(char* args)
{
    if (!*args)
        return false;

    uint32 factionId = (uint32) atoi(args);

    if (!sObjectMgr.GetFactionTemplateEntry(factionId))
    {
        PSendSysMessage(LANG_WRONG_FACTION, factionId);
        SetSentErrorMessage(true);
        return false;
    }

    Creature* pCreature = GetSelectedCreature();

    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->SetFactionTemplateId(factionId);

    // faction is set in creature_template - not inside creature

    // update in memory
    if (CreatureInfo const* cinfo = pCreature->GetCreatureInfo())
    {
        const_cast<CreatureInfo*>(cinfo)->faction = factionId;
    }

    // and DB
    WorldDatabase.PExecuteLog("UPDATE creature_template SET faction = '%u' WHERE entry = '%u'", factionId, pCreature->GetEntry());

    return true;
}

bool ChatHandler::HandleNpcFlagCommand(char* args)
{
    if (!*args)
        return false;

    uint32 npcFlags = (uint32) atoi(args);

    Creature* pCreature = GetSelectedCreature();

    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->SetUInt32Value(UNIT_NPC_FLAGS, npcFlags);

    WorldDatabase.PExecuteLog("UPDATE creature_template SET npc_flags = '%u' WHERE entry = '%u'", npcFlags, pCreature->GetEntry());

    SendSysMessage(LANG_VALUE_SAVED_REJOIN);

    return true;
}

bool ChatHandler::HandleNpcTameCommand(char* /*args*/)
{
    Creature* creatureTarget = GetSelectedCreature();

    if (!creatureTarget || creatureTarget->IsPet())
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* player = m_session->GetPlayer();

    if (player->GetPetGuid())
    {
        SendSysMessage(LANG_YOU_ALREADY_HAVE_PET);
        SetSentErrorMessage(true);
        return false;
    }

    player->CastSpell(creatureTarget, 13481, true);         // Tame Beast, triggered effect
    return true;
}

bool ChatHandler::HandleNpcSetDeathStateCommand(char* args)
{
    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    Creature* pCreature = GetSelectedCreature();
    if (!pCreature || !pCreature->HasStaticDBSpawnData())
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    CreatureData* pData = const_cast<CreatureData*>(sObjectMgr.GetCreatureData(pCreature->GetGUIDLow()));

    if (value)
        pData->spawn_flags |= SPAWN_FLAG_DEAD;
    else
        pData->spawn_flags &= ~SPAWN_FLAG_DEAD;

    pCreature->SaveToDB();
    pCreature->Respawn();

    return true;
}

bool ChatHandler::HandleNpcDespawnCommand(char* args)
{
    Creature* pCreature = GetSelectedCreature();

    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->DespawnOrUnsummon();

    return true;
}

bool ChatHandler::HandleRespawnCommand(char* /*args*/)
{
    Player* pl = m_session->GetPlayer();

    // accept only explicitly selected target (not implicitly self targeting case)
    Unit* target = GetSelectedUnit();
    if (pl->GetSelectionGuid() && target)
    {
        if (target->GetTypeId() != TYPEID_UNIT)
        {
            SendSysMessage(LANG_SELECT_CREATURE);
            SetSentErrorMessage(true);
            return false;
        }

        if (target->IsDead())
            ((Creature*)target)->Respawn();
        return true;
    }

    MaNGOS::RespawnDo u_do;
    MaNGOS::WorldObjectWorker<MaNGOS::RespawnDo> worker(u_do);
    Cell::VisitGridObjects(pl, worker, pl->GetMap()->GetVisibilityDistance());
    return true;
}

bool ChatHandler::HandleNpcSpawnDistCommand(char* args)
{
    if (!*args)
        return false;

    float option = (float)atof(args);
    if (option < 0.0f)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return false;
    }

    MovementGeneratorType mtype = IDLE_MOTION_TYPE;
    if (option > 0.0f)
        mtype = RANDOM_MOTION_TYPE;

    Creature* pCreature = GetSelectedCreature();
    uint32 u_guidlow = 0;

    if (pCreature)
        u_guidlow = pCreature->GetGUIDLow();
    else
        return false;

    pCreature->SetWanderDistance((float)option);
    pCreature->SetDefaultMovementType(mtype);
    pCreature->GetMotionMaster()->Initialize();
    if (pCreature->IsAlive())                               // dead creature will reset movement generator at respawn
    {
        pCreature->SetDeathState(JUST_DIED);
        pCreature->Respawn();
    }

    WorldDatabase.PExecuteLog("UPDATE creature SET spawndist=%f, MovementType=%i WHERE guid=%u", option, mtype, u_guidlow);
    PSendSysMessage(LANG_COMMAND_SPAWNDIST, option);
    return true;
}

bool ChatHandler::HandleNpcSpawnTimeCommand(char* args)
{
    uint32 stime;
    if (!ExtractUInt32(&args, stime))
        return false;

    Creature* pCreature = GetSelectedCreature();
    if (!pCreature)
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 u_guidlow = pCreature->GetGUIDLow();

    WorldDatabase.PExecuteLog("UPDATE creature SET spawntimesecsmin=%i WHERE guid=%u", stime, u_guidlow);
    pCreature->SetRespawnDelay(stime);
    PSendSysMessage(LANG_COMMAND_SPAWNTIME, stime);

    return true;
}

bool ChatHandler::HandleNpcEvadeCommand(char* /*args*/)
{
    Creature* pTarget = GetSelectedCreature();

    if (!pTarget)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    if (pTarget->AI())
        pTarget->AI()->EnterEvadeMode();

    return true;
}

bool ChatHandler::HandleNpcPlayEmoteCommand(char* args)
{
    uint32 emote = atoi(args);

    Creature* target = GetSelectedCreature();
    if (!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    target->HandleEmote(emote);

    return true;
}

bool ChatHandler::HandleNpcSayCommand(char* args)
{
    if (!*args)
        return false;

    Creature* pCreature = GetSelectedCreature();
    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->MonsterSay(args, LANG_UNIVERSAL);

    return true;
}

bool ChatHandler::HandleNpcYellCommand(char* args)
{
    if (!*args)
        return false;

    Creature* pCreature = GetSelectedCreature();
    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->MonsterYell(args, LANG_UNIVERSAL);

    return true;
}

bool ChatHandler::HandleNpcTextEmoteCommand(char* args)
{
    if (!*args)
        return false;

    Creature* pCreature = GetSelectedCreature();

    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->MonsterTextEmote(args, 0, false);

    return true;
}

bool ChatHandler::HandleNpcWhisperCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
        return false;

    ObjectGuid guid = m_session->GetPlayer()->GetSelectionGuid();
    if (!guid)
        return false;

    Creature* pCreature = m_session->GetPlayer()->GetMap()->GetCreature(guid);

    if (!pCreature || !target || !*args)
        return false;

    // check online security
    if (HasLowerSecurity(target))
        return false;

    pCreature->MonsterWhisper(args, target);

    return true;
}

bool ChatHandler::HandleNpcAddCommand(char* args)
{
    if (!*args)
        return false;

    uint32 id;
    if (!ExtractUint32KeyFromLink(&args, "Hcreature_entry", id))
        return false;

    CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(id);
    if (!cinfo)
    {
        PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, id);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = m_session->GetPlayer();
    CreatureCreatePos pos(chr, chr->GetOrientation());
    Map* map = chr->GetMap();

    Creature* pCreature = new Creature;

    // used guids from specially reserved range (can be 0 if no free values)
    uint32 lowguid = sObjectMgr.GenerateStaticCreatureLowGuid();
    if (!lowguid)
    {
        SendSysMessage(LANG_NO_FREE_STATIC_GUID_FOR_SPAWN);
        SetSentErrorMessage(true);
        return false;
    }

    if (!pCreature->Create(lowguid, pos, cinfo, TEAM_NONE, id))
    {
        delete pCreature;
        return false;
    }

    pCreature->SaveToDB(map->GetId());

    uint32 db_guid = pCreature->GetGUIDLow();

    // To call _LoadGoods(); _LoadQuests(); CreateTrainerSpells();
    pCreature->LoadFromDB(db_guid, map);

    map->Add(pCreature);
    sObjectMgr.AddCreatureToGrid(db_guid, sObjectMgr.GetCreatureData(db_guid));
    return true;
}

bool ChatHandler::HandleNpcSummonCommand(char* args)
{
    if (!*args)
        return false;

    uint32 id;
    if (!ExtractUint32KeyFromLink(&args, "Hcreature_entry", id))
        return false;

    CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(id);
    if (!cinfo)
    {
        PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, id);
        SetSentErrorMessage(true);
        return false;
    }

    Player* chr = m_session->GetPlayer();
    chr->SummonCreature(id, chr->GetPositionX(), chr->GetPositionY(), chr->GetPositionZ(), chr->GetOrientation());
    return true;
}

bool ChatHandler::HandleNpcDeleteCommand(char* args)
{
    Creature* unit = nullptr;

    if (*args)
    {
        // number or [name] Shift-click form |color|Hcreature:creature_guid|h[name]|h|r
        uint32 lowguid;
        if (!ExtractUint32KeyFromLink(&args, "Hcreature", lowguid))
            return false;

        if (!lowguid)
            return false;

        if (CreatureData const* data = sObjectMgr.GetCreatureData(lowguid))
            unit = m_session->GetPlayer()->GetMap()->GetCreature(data->GetObjectGuid(lowguid));
    }
    else
        unit = GetSelectedCreature();

    if (!unit)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    switch (unit->GetSubtype())
    {
        case CREATURE_SUBTYPE_GENERIC:
        {
            unit->CombatStop();
            if (CreatureData const* data = sObjectMgr.GetCreatureData(unit->GetGUIDLow()))
            {
                Creature::AddToRemoveListInMaps(unit->GetGUIDLow(), data);
                Creature::DeleteFromDB(unit->GetGUIDLow(), data);
            }
            else
                unit->AddObjectToRemoveList();
            break;
        }
        case CREATURE_SUBTYPE_PET:
            ((Pet*)unit)->Unsummon(PET_SAVE_AS_CURRENT);
            break;
        case CREATURE_SUBTYPE_TOTEM:
            ((Totem*)unit)->UnSummon();
            break;
        case CREATURE_SUBTYPE_TEMPORARY_SUMMON:
            ((TemporarySummon*)unit)->UnSummon();
            break;
        default:
            return false;
    }

    SendSysMessage(LANG_COMMAND_DELCREATMESSAGE);

    return true;
}

bool ChatHandler::HandleNpcAddEntryCommand(char* args)
{
    Creature* pCreature = GetSelectedCreature();

    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    uint32 uiCreatureId = 0;

    if (!ExtractUInt32(&args, uiCreatureId))
        return false;

    if (!ObjectMgr::GetCreatureTemplate(uiCreatureId))
    {
        PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, uiCreatureId);
        SetSentErrorMessage(true);
        return false;
    }

    CreatureData* pData = const_cast<CreatureData*>(pCreature->GetCreatureData());
    if (!pData)
    {
        SendSysMessage("Creature is not a permanent spawn.");
        SetSentErrorMessage(true);
        return false;
    }

    for (int i = 0; i < MAX_CREATURE_IDS_PER_SPAWN; i++)
    {
        if (pData->creature_id[i] == uiCreatureId)
        {
            SendSysMessage("Creature spawn already includes this entry.");
            SetSentErrorMessage(true);
            return false;
        }
    }

    if (pData->GetCreatureIdCount() >= MAX_CREATURE_IDS_PER_SPAWN)
    {
        SendSysMessage("Creature spawn has the maximum amount of entries already.");
        SetSentErrorMessage(true);
        return false;
    }

    int count = 0;
    std::array<uint32, MAX_CREATURE_IDS_PER_SPAWN> creatureIds = pData->creature_id;
    for (int i = 0; i < MAX_CREATURE_IDS_PER_SPAWN; i++)
    {
        if (!creatureIds[i])
        {
            count = i+1;
            creatureIds[i] = uiCreatureId;
            break;
        }
    }
    std::sort(creatureIds.begin(), creatureIds.begin()+count);
    pData->creature_id = creatureIds;

    WorldDatabase.PExecute("UPDATE `creature` SET `id`=%u, `id2`=%u, `id3`=%u, `id4`=%u WHERE `guid`=%u", creatureIds[0], creatureIds[1], creatureIds[2], creatureIds[3], pCreature->GetGUIDLow());
    PSendSysMessage("Creature entry %u added to guid %u.", uiCreatureId, pCreature->GetGUIDLow());
    return true;
}

bool ChatHandler::HandleNpcAddWeaponCommand(char* args)
{
    Creature* pCreature = GetSelectedCreature();

    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    uint32 uiItemId = 0;

    if (!ExtractUInt32(&args, uiItemId))
        return false;

    uint32 uiSlotId = 0;

    if (!ExtractUInt32(&args, uiSlotId))
        return false;

    ItemPrototype const* pItemProto = ObjectMgr::GetItemPrototype(uiItemId);

    if (!pItemProto)
    {
        PSendSysMessage(LANG_ITEM_NOT_FOUND, uiItemId);
        return true;
    }

    if (uiSlotId > VIRTUAL_ITEM_SLOT_2)
    {
        PSendSysMessage(LANG_ITEM_SLOT_NOT_EXIST, uiSlotId);
        return true;
    }

    pCreature->SetVirtualItem(VirtualItemSlot(uiSlotId), uiItemId);
    PSendSysMessage(LANG_ITEM_ADDED_TO_SLOT, uiItemId, pItemProto->Name1, uiSlotId);

    return true;
}

bool ChatHandler::HandleNpcAddVendorItemCommand(char* args)
{
    uint32 itemId;
    if (!ExtractUint32KeyFromLink(&args, "Hitem", itemId))
    {
        SendSysMessage(LANG_COMMAND_NEEDITEMSEND);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 maxcount;
    if (!ExtractOptUInt32(&args, maxcount, 0))
        return false;

    uint32 incrtime;
    if (!ExtractOptUInt32(&args, incrtime, 0))
        return false;

    uint32 itemflags;
    if (!ExtractOptUInt32(&args, itemflags, 0))
        return false;

    Creature* vendor = GetSelectedCreature();

    uint32 vendor_entry = vendor ? vendor->GetEntry() : 0;

    if (!sObjectMgr.IsVendorItemValid(false, "npc_vendor", vendor_entry, itemId, maxcount, incrtime, 0, m_session->GetPlayer()))
    {
        SetSentErrorMessage(true);
        return false;
    }

    sObjectMgr.AddVendorItem(vendor_entry, itemId, maxcount, incrtime, itemflags);

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(itemId);

    PSendSysMessage(LANG_ITEM_ADDED_TO_LIST, itemId, pProto->Name1, maxcount, incrtime);
    return true;
}

bool ChatHandler::HandleNpcDelVendorItemCommand(char* args)
{
    if (!*args)
        return false;

    Creature* vendor = GetSelectedCreature();
    if (!vendor || !vendor->IsVendor())
    {
        SendSysMessage(LANG_COMMAND_VENDORSELECTION);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 itemId;
    if (!ExtractUint32KeyFromLink(&args, "Hitem", itemId))
    {
        SendSysMessage(LANG_COMMAND_NEEDITEMSEND);
        SetSentErrorMessage(true);
        return false;
    }

    if (!sObjectMgr.RemoveVendorItem(vendor->GetEntry(), itemId))
    {
        PSendSysMessage(LANG_ITEM_NOT_IN_LIST, itemId);
        SetSentErrorMessage(true);
        return false;
    }

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(itemId);

    PSendSysMessage(LANG_ITEM_DELETED_FROM_LIST, itemId, pProto->Name1);
    return true;
}

bool ChatHandler::HandleNpcMoveCommand(char* args)
{
    uint32 lowguid = 0;
    Player* pPlayer = m_session->GetPlayer();
    Creature* pCreature = GetSelectedCreature();

    if (!pCreature)
    {
        // number or [name] Shift-click form |color|Hcreature:creature_guid|h[name]|h|r
        if (!ExtractUint32KeyFromLink(&args, "Hcreature", lowguid))
            return false;

        CreatureData const* data = sObjectMgr.GetCreatureData(lowguid);
        if (!data)
        {
            PSendSysMessage(LANG_COMMAND_CREATGUIDNOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        if (pPlayer->GetMapId() != data->position.mapId)
        {
            PSendSysMessage(LANG_COMMAND_CREATUREATSAMEMAP, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        pCreature = pPlayer->GetMap()->GetCreature(data->GetObjectGuid(lowguid));
    }
    else
        lowguid = pCreature->GetGUIDLow();

    float x = pPlayer->GetPositionX();
    float y = pPlayer->GetPositionY();
    float z = pPlayer->GetPositionZ();
    float o = pPlayer->GetOrientation();

    if (pCreature)
    {
        pCreature->SetHomePosition(x, y, z, o);
        if (pCreature->IsAlive())                           // dead creature will reset movement generator at respawn
        {
            pCreature->SetDeathState(JUST_DIED);
            pCreature->Respawn();
        }
    }

    WorldDatabase.PExecuteLog("UPDATE creature SET position_x = '%f', position_y = '%f', position_z = '%f', orientation = '%f' WHERE guid = '%u'", x, y, z, o, lowguid);
    PSendSysMessage(LANG_COMMAND_CREATUREMOVED);
    return true;
}

/**HandleNpcSetMoveTypeCommand
 * Set the movement type for an NPC.<br/>
 * <br/>
 * Valid movement types are:
 * <ul>
 * <li> stay - NPC wont move </li>
 * <li> random - NPC will move randomly according to the spawndist </li>
 * <li> way - NPC will move with given waypoints set </li>
 * </ul>
 * additional parameter: NODEL - so no waypoints are deleted, if you
 *                       change the movement type
 */
bool ChatHandler::HandleNpcSetMoveTypeCommand(char* args)
{
    // 3 arguments:
    // GUID (optional - you can also select the creature)
    // stay|random|way (determines the kind of movement)
    // NODEL (optional - tells the system NOT to delete any waypoints)
    //        this is very handy if you want to do waypoints, that are
    //        later switched on/off according to special events (like escort
    //        quests, etc)

    uint32 lowguid;
    Creature* pCreature;
    if (!ExtractUInt32(&args, lowguid))                     // case .setmovetype $move_type (with selected creature)
    {
        pCreature = GetSelectedCreature();
        if (!pCreature || !pCreature->HasStaticDBSpawnData())
            return false;
        lowguid = pCreature->GetGUIDLow();
    }
    else                                                    // case .setmovetype #creature_guid $move_type (with guid)
    {
        CreatureData const* data = sObjectMgr.GetCreatureData(lowguid);
        if (!data)
        {
            PSendSysMessage(LANG_COMMAND_CREATGUIDNOTFOUND, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        Player* player = m_session->GetPlayer();

        if (player->GetMapId() != data->position.mapId)
        {
            PSendSysMessage(LANG_COMMAND_CREATUREATSAMEMAP, lowguid);
            SetSentErrorMessage(true);
            return false;
        }

        pCreature = player->GetMap()->GetCreature(data->GetObjectGuid(lowguid));
    }

    MovementGeneratorType move_type;
    char* type_str = ExtractLiteralArg(&args);
    if (!type_str)
        return false;

    if (strncmp(type_str, "idle", strlen(type_str)) == 0)
        move_type = IDLE_MOTION_TYPE;
    else if (strncmp(type_str, "random", strlen(type_str)) == 0)
        move_type = RANDOM_MOTION_TYPE;
    else if (strncmp(type_str, "waypoint", strlen(type_str)) == 0)
        move_type = WAYPOINT_MOTION_TYPE;
    else
        return false;

    bool doNotDelete = ExtractLiteralArg(&args, "NODEL") != nullptr;
    if (!doNotDelete && *args)                              // need fail if false in result wrong literal
        return false;

    // now lowguid is low guid really existing creature
    // and pCreature point (maybe) to this creature or nullptr

    // update movement type
    if (!doNotDelete)
        sWaypointMgr.DeletePath(lowguid);

    if (pCreature)
    {
        pCreature->SetDefaultMovementType(move_type);
        pCreature->GetMotionMaster()->Initialize();
        if (pCreature->IsAlive())                           // dead creature will reset movement generator at respawn
        {
            pCreature->SetDeathState(JUST_DIED);
            pCreature->Respawn();
        }
        pCreature->SaveToDB();
    }

    if (doNotDelete)
        PSendSysMessage(LANG_MOVE_TYPE_SET_NODEL, type_str);
    else
        PSendSysMessage(LANG_MOVE_TYPE_SET, type_str);

    return true;
}

bool ChatHandler::HandleComeToMeCommand(char *args)
{
    Creature* caster = GetSelectedCreature();

    if (!caster)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    Player* pl = m_session->GetPlayer();

    caster->GetMotionMaster()->MovePoint(0, pl->GetPositionX(), pl->GetPositionY(), pl->GetPositionZ(), true);
    return true;
}

bool ChatHandler::HandleNpcFollowCommand(char* /*args*/)
{
    Player* player = m_session->GetPlayer();
    Creature* creature = GetSelectedCreature();

    if (!creature)
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    // Follow player - Using pet's default dist and angle
    creature->GetMotionMaster()->MoveFollow(player, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);

    PSendSysMessage(LANG_CREATURE_FOLLOW_YOU_NOW, creature->GetName());
    return true;
}

bool ChatHandler::HandleNpcUnFollowCommand(char* /*args*/)
{
    Player* pPlayer = m_session->GetPlayer();
    Creature* pCreature = GetSelectedCreature();

    if (!pCreature)
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    MotionMaster* creatureMotion = pCreature->GetMotionMaster();
    if (creatureMotion->empty() ||
        creatureMotion->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
    {
        PSendSysMessage(LANG_CREATURE_NOT_FOLLOW_YOU, pCreature->GetName());
        SetSentErrorMessage(true);
        return false;
    }

    FollowMovementGenerator<Creature> const* mgen
        = static_cast<FollowMovementGenerator<Creature> const*>((creatureMotion->top()));

    if (mgen->GetTarget() != pPlayer)
    {
        PSendSysMessage(LANG_CREATURE_NOT_FOLLOW_YOU, pCreature->GetName());
        SetSentErrorMessage(true);
        return false;
    }

    // reset movement
    creatureMotion->MovementExpired(true);

    PSendSysMessage(LANG_CREATURE_NOT_FOLLOW_YOU_NOW, pCreature->GetName());
    return true;
}

bool ChatHandler::HandleNpcAllowMovementCommand(char* args)
{
    Creature* pCreature = GetSelectedCreature();

    if (!pCreature)
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    if (pCreature->AI())
        pCreature->AI()->SetCombatMovement(value);

    return true;
}

bool ChatHandler::HandleNpcAllowAttackCommand(char* args)
{
    Creature* pCreature = GetSelectedCreature();

    if (!pCreature)
    {
        PSendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    if (pCreature->AI())
        pCreature->AI()->SetMeleeAttack(value);

    return true;
}

bool ChatHandler::HandleNpcGroupAddCommand(char* args)
{
    if (!*args)
        return false;

    Creature* target = GetSelectedCreature();
    SetSentErrorMessage(true);

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return false;
    }

    uint32 leaderGuidCounter = 0;
    uint32 options = OPTION_FORMATION_MOVE | OPTION_AGGRO_TOGETHER | OPTION_EVADE_TOGETHER | OPTION_RESPAWN_TOGETHER;
    if (!ExtractUInt32(&args, leaderGuidCounter))
        return false;
    ExtractUInt32(&args, options);
    Creature* leader = target->GetMap()->GetCreature(CreatureGroupsManager::ConvertDBGuid(leaderGuidCounter));
    if (!leader)
    {
        PSendSysMessage("Leader not found");
        return false;
    }
    if (target->GetCreatureGroup())
    {
        SendSysMessage("Selected creature is already member of a group.");
        return false;
    }

    bool dbsave = target->HasStaticDBSpawnData();
    Player* chr = m_session->GetPlayer();
    float angle = (chr->GetAngle(target) - target->GetOrientation()) + 2 * M_PI_F;
    float dist = sqrtf(pow(chr->GetPositionX() - target->GetPositionX(), int(2)) + pow(chr->GetPositionY() - target->GetPositionY(), int(2)));

    CreatureGroup* group = leader->GetCreatureGroup();
    if (!group)
        group = new CreatureGroup(leader->GetObjectGuid());
    group->AddMember(target->GetObjectGuid(), dist, angle, options);
    target->SetCreatureGroup(group);
    leader->SetCreatureGroup(group);
    target->GetMotionMaster()->Initialize();
    if (dbsave)
        group->SaveToDb();
    PSendSysMessage("Group added for creature %u. Leader %u, Angle %f, Dist %f", target->GetGUIDLow(), leader->GetGUIDLow(), angle, dist);
    return true;
}

bool ChatHandler::HandleNpcGroupAddRelCommand(char* args)
{
    if (!*args)
        return false;

    Creature* target = GetSelectedCreature();
    SetSentErrorMessage(true);

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return false;
    }

    uint32 leaderGuidCounter = 0;
    uint32 options = OPTION_FORMATION_MOVE | OPTION_AGGRO_TOGETHER | OPTION_EVADE_TOGETHER | OPTION_RESPAWN_TOGETHER;
    if (!ExtractUInt32(&args, leaderGuidCounter))
        return false;
    ExtractUInt32(&args, options);
    Creature* leader = target->GetMap()->GetCreature(CreatureGroupsManager::ConvertDBGuid(leaderGuidCounter));
    if (!leader)
    {
        PSendSysMessage("Leader not found");
        return false;
    }
    if (target->GetCreatureGroup())
    {
        SendSysMessage("Selected creature is already member of a group.");
        return false;
    }

    bool dbsave = target->HasStaticDBSpawnData();
    //Player* chr = m_session->GetPlayer();
    float angle = target->GetAngle(leader);//(chr->GetAngle(target) - target->GetOrientation()) + 2 * M_PI_F;
    float dist = sqrtf(pow(leader->GetPositionX() - target->GetPositionX(), int(2)) + pow(leader->GetPositionY() - target->GetPositionY(), int(2)));

    CreatureGroup* group = leader->GetCreatureGroup();
    if (!group)
        group = new CreatureGroup(leader->GetObjectGuid());
    group->AddMember(target->GetObjectGuid(), dist, angle, options);
    target->SetCreatureGroup(group);
    leader->SetCreatureGroup(group);
    target->GetMotionMaster()->Initialize();
    if (dbsave)
        group->SaveToDb();
    PSendSysMessage("Group added for creature %u. Leader %u, Angle %f, Dist %f", target->GetGUIDLow(), leader->GetGUIDLow(), angle, dist);
    return true;
}

bool ChatHandler::HandleNpcGroupDelCommand(char *args)
{
    Creature* target = GetSelectedCreature();
    SetSentErrorMessage(true);

    if (!target || !target->HasStaticDBSpawnData())
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return false;
    }

    CreatureGroup* g = target->GetCreatureGroup();
    if (!g)
    {
        PSendSysMessage("%s [GUID=%u] is not in a group.", target->GetName(), target->GetGUIDLow());
        return false;
    }

    g->RemoveMember(target->GetObjectGuid());
    g->SaveToDb();
    target->SetCreatureGroup(nullptr);
    target->GetMotionMaster()->Initialize();
    return true;
}

bool ChatHandler::HandleNpcGroupLinkCommand(char * args)
{
    if (!*args)
        return false;

    Creature* target = GetSelectedCreature();
    SetSentErrorMessage(true);

    if (!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return false;
    }

    uint32 options;
    uint32 leaderGuidCounter = 0;
    if (!ExtractUInt32(&args, leaderGuidCounter))
        return false;
    
    ExtractUInt32(&args, options);
    
    Creature* leader = target->GetMap()->GetCreature(CreatureGroupsManager::ConvertDBGuid(leaderGuidCounter));
    if (!leader)
    {
        PSendSysMessage("Leader not found");
        return false;
    }
    
    WorldDatabase.PExecute("DELETE FROM `creature_linking` WHERE `guid`=%u", target->GetGUIDLow());
        WorldDatabase.PExecute("INSERT INTO `creature_linking` SET `guid`=%u, `master_guid`=%u, `flag`='%u'",
            target->GetGUIDLow(), leaderGuidCounter, options);

    PSendSysMessage("creature_link for creature %u. Leader %u", target->GetGUIDLow(), leader->GetGUIDLow());
    return true;
}

/// Helper function
inline Creature* Helper_CreateWaypointFor(Creature* wpOwner, WaypointPathOrigin wpOrigin, int32 pathId, uint32 wpId, WaypointNode const* wpNode, CreatureInfo const* waypointInfo)
{
    TemporarySummonWaypoint* wpCreature = new TemporarySummonWaypoint(wpOwner->GetObjectGuid(), wpId+1, pathId, (uint32)wpOrigin);

    CreatureCreatePos pos(wpOwner->GetMap(), wpNode->x, wpNode->y, wpNode->z, wpNode->orientation != 100.0f ? wpNode->orientation : 0.0f);

    if (!wpCreature->Create(wpOwner->GetMap()->GenerateLocalLowGuid(HIGHGUID_UNIT), pos, waypointInfo, TEAM_NONE, waypointInfo->entry))
    {
        delete wpCreature;
        return nullptr;
    }

    wpCreature->SetVisibility(VISIBILITY_OFF);
    wpCreature->SetSummonPoint(pos);
    wpCreature->SetUInt32Value(UNIT_FIELD_LEVEL, wpId+1);
    wpCreature->SetActiveObjectState(true);

    wpCreature->Summon(TEMPSUMMON_TIMED_DESPAWN, 5 * MINUTE * IN_MILLISECONDS); // Also initializes the AI and MMGen
    return wpCreature;
}

inline void UnsummonVisualWaypoints(Player const* player, ObjectGuid ownerGuid)
{
    std::list<Creature*> waypoints;
    MaNGOS::AllCreaturesOfEntryInRange checkerForWaypoint(player, VISUAL_WAYPOINT, SIZE_OF_GRIDS);
    MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRange> searcher(waypoints, checkerForWaypoint);
    Cell::VisitGridObjects(player, searcher, SIZE_OF_GRIDS);

    for (const auto& waypoint : waypoints)
    {
        if (waypoint->GetSubtype() != CREATURE_SUBTYPE_TEMPORARY_SUMMON)
            continue;

        TemporarySummonWaypoint* wpTarget = dynamic_cast<TemporarySummonWaypoint*>(waypoint);
        if (!wpTarget)
            continue;

        if (wpTarget->GetSummonerGuid() == ownerGuid)
            wpTarget->UnSummon();
    }
}

/** Add a waypoint to a creature
 * .wp add [dbGuid] [pathId] [source]
 *
 * The user can either select an npc or provide its dbGuid.
 * Also the user can specify pathId and source if wanted.
 *
 * The user can even select a visual waypoint - then the new waypoint
 * is placed *after* the selected one - this makes insertion of new
 * waypoints possible.
 *
 * .wp add [pathId] [source]
 * -> adds a waypoint to the currently selected creature, to path pathId in source-storage
 *
 * .wp add guid [pathId] [source]
 * -> if no npc is selected, expect the creature provided with guid argument
 *
 * @return true - command did succeed, false - something went wrong
 */
bool ChatHandler::HandleWpAddCommand(char* args)
{
    DEBUG_LOG("DEBUG: HandleWpAddCommand");

    CreatureInfo const* waypointInfo = ObjectMgr::GetCreatureTemplate(VISUAL_WAYPOINT);
    if (!waypointInfo || waypointInfo->GetHighGuid() != HIGHGUID_UNIT)
        return false;                                       // must exist as normal creature in mangos.sql 'creature_template'

    Creature* targetCreature = GetSelectedCreature();
    WaypointPathOrigin wpDestination = PATH_NO_PATH;        ///< into which storage
    int32 wpPathId = 0;                                     ///< along which path
    uint32 wpPointId = 0;                                   ///< pointId if a waypoint was selected, in this case insert after
    Creature* wpOwner;

    if (targetCreature)
    {
        // Check if the user did specify a visual waypoint
        if (targetCreature->GetEntry() == VISUAL_WAYPOINT && targetCreature->GetSubtype() == CREATURE_SUBTYPE_TEMPORARY_SUMMON)
        {
            TemporarySummonWaypoint* wpTarget = dynamic_cast<TemporarySummonWaypoint*>(targetCreature);
            if (!wpTarget)
            {
                PSendSysMessage(LANG_WAYPOINT_VP_SELECT);
                SetSentErrorMessage(true);
                return false;
            }

            // Who moves along this waypoint?
            wpOwner = targetCreature->GetMap()->GetAnyTypeCreature(wpTarget->GetSummonerGuid());
            if (!wpOwner)
            {
                PSendSysMessage(LANG_WAYPOINT_NOTFOUND_NPC, wpTarget->GetSummonerGuid().GetString().c_str());
                SetSentErrorMessage(true);
                return false;
            }
            wpDestination = (WaypointPathOrigin)wpTarget->GetPathOrigin();
            wpPathId = wpTarget->GetPathId();
            wpPointId = wpTarget->GetWaypointId() + 1;      // Insert as next waypoint
        }
        else // normal creature selected
            wpOwner = targetCreature;
    }
    else //!targetCreature - first argument must be dbGuid
    {
        uint32 dbGuid;
        if (!ExtractUInt32(&args, dbGuid))
        {
            PSendSysMessage(LANG_WAYPOINT_NOGUID);
            SetSentErrorMessage(true);
            return false;
        }

        CreatureData const* data = sObjectMgr.GetCreatureData(dbGuid);
        if (!data)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }

        if (m_session->GetPlayer()->GetMapId() != data->position.mapId)
        {
            PSendSysMessage(LANG_COMMAND_CREATUREATSAMEMAP, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }

        wpOwner = m_session->GetPlayer()->GetMap()->GetAnyTypeCreature(data->GetObjectGuid(dbGuid));
        if (!wpOwner)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }
    }

    if (wpDestination == PATH_NO_PATH)                      // No Waypoint selected, parse additional params
    {
        if (ExtractOptInt32(&args, wpPathId, 0))            // Fill path-id and source
        {
            uint32 src = (uint32)PATH_NO_PATH;
            if (ExtractOptUInt32(&args, src, src))
                wpDestination = (WaypointPathOrigin)src;
            else // pathId provided but no destination
            {
                if (wpPathId != 0)
                    wpDestination = PATH_FROM_ENTRY;        // Multiple Paths must only be assigned by entry
            }
        }

        if (wpDestination == PATH_NO_PATH)                  // No overwrite params. Do best estimate
        {
            if (wpOwner->GetMotionMaster()->GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
                if (WaypointMovementGenerator<Creature> const* wpMMGen = dynamic_cast<WaypointMovementGenerator<Creature> const*>(wpOwner->GetMotionMaster()->GetCurrent()))
                    wpMMGen->GetPathInformation(wpPathId, wpDestination);

            // Get information about default path if no current path. If no default path, prepare data dependendy on uniqueness
            if (wpDestination == PATH_NO_PATH && !sWaypointMgr.GetDefaultPath(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), &wpDestination))
            {
                wpDestination = PATH_FROM_ENTRY;                // Default place to store paths
                if (wpOwner->HasStaticDBSpawnData())
                {
                    QueryResult* result = WorldDatabase.PQuery("SELECT COUNT(id) FROM creature WHERE id = %u", wpOwner->GetEntry());
                    if (result && result->Fetch()[0].GetUInt32() != 1)
                        wpDestination = PATH_FROM_GUID;
                    delete result;
                }
            }
        }
    }

    // All arguments parsed
    // wpOwner will get a new waypoint inserted into wpPath = GetPathFromOrigin(wpOwner, wpDestination, wpPathId) at wpPointId

    float x, y, z;
    m_session->GetPlayer()->GetPosition(x, y, z);
    if (!sWaypointMgr.AddNode(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpPointId, wpDestination, x, y, z))
    {
        PSendSysMessage(LANG_WAYPOINT_NOTCREATED, wpPointId, wpOwner->GetGuidStr().c_str(), wpPathId, WaypointManager::GetOriginString(wpDestination).c_str());
        SetSentErrorMessage(true);
        return false;
    }

    // Unsummon old visuals, summon new ones
    UnsummonVisualWaypoints(m_session->GetPlayer(), wpOwner->GetObjectGuid());
    WaypointPath const* wpPath = sWaypointMgr.GetPathFromOrigin(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpPathId, wpDestination);
    for (const auto& itr : *wpPath)
    {
        if (!Helper_CreateWaypointFor(wpOwner, wpDestination, wpPathId, itr.first, &itr.second, waypointInfo))
        {
            PSendSysMessage(LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
            SetSentErrorMessage(true);
            return false;
        }
    }

    PSendSysMessage(LANG_WAYPOINT_ADDED, wpPointId, wpOwner->GetGuidStr().c_str(), wpPathId, WaypointManager::GetOriginString(wpDestination).c_str());

    return true;
}                                                           // HandleWpAddCommand

/**
 * .wp modify waittime | scriptid | orientation | del | move [dbGuid, id] [value]
 *
 * waittime <Delay>
 *   User has selected a visual waypoint before.
 *   Delay <Delay> is added to this waypoint. Everytime the
 *   NPC comes to this waypoint, it will wait Delay millieseconds.
 *
 * waittime <DBGuid> <WPNUM> <Delay>
 *   User has not selected visual waypoint before.
 *   For the waypoint <WPNUM> for the NPC with <DBGuid>
 *   an delay Delay is added to this waypoint
 *   Everytime the NPC comes to this waypoint, it will wait Delay millieseconds.
 *
 * scriptid <scriptId>
 *   User has selected a visual waypoint before.
 *   <scriptId> is added to this waypoint. Everytime the
 *   NPC comes to this waypoint, the DBScript scriptId is executed.
 *
 * scriptid <DBGuid> <WPNUM> <scriptId>
 *   User has not selected visual waypoint before.
 *   For the waypoint <WPNUM> for the NPC with <DBGuid>
 *   an emote <scriptId> is added.
 *   Everytime the NPC comes to this waypoint, the DBScript scriptId is executed.
 *
 * orientation [DBGuid, WpNum] <Orientation>
 *   Set the orientation of the selected waypoint or waypoint given with DbGuid/ WpId
 *   to the value of <Orientation>.
 *
 * del [DBGuid, WpId]
 *   Remove the selected waypoint or waypoint given with DbGuid/ WpId.
 *
 * move [DBGuid, WpId]
 *   Move the selected waypoint or waypoint given with DbGuid/ WpId to player's current positiion.
 */
bool ChatHandler::HandleWpModifyCommand(char* args)
{
    DEBUG_LOG("DEBUG: HandleWpModifyCommand");

    if (!*args)
        { return false; }

    CreatureInfo const* waypointInfo = ObjectMgr::GetCreatureTemplate(VISUAL_WAYPOINT);
    if (!waypointInfo || waypointInfo->GetHighGuid() != HIGHGUID_UNIT)
        { return false; }                                       // must exist as normal creature in mangos.sql 'creature_template'

    // first arg: add del text emote spell waittime move
    char* subCmd_str = ExtractLiteralArg(&args);
    if (!subCmd_str)
    {
        return false;
    }

    std::string subCmd = subCmd_str;
    // Check
    // Remember: "show" must also be the name of a column!
    if ((subCmd != "waittime") && (subCmd != "scriptid") && (subCmd != "orientation") && (subCmd != "del") && (subCmd != "move"))
    {
        return false;
    }

    // Next arg is: <GUID> <WPNUM> <ARGUMENT>

    // Did user provide a GUID or did the user select a creature?
    Creature* targetCreature = GetSelectedCreature();       // Expect a visual waypoint to be selected
    Creature* wpOwner;                               // Who moves along the waypoint
    uint32 wpId = 0;
    WaypointPathOrigin wpSource = PATH_NO_PATH;
    int32 wpPathId = 0;

    if (targetCreature)
    {
        DEBUG_LOG("DEBUG: HandleWpModifyCommand - User did select an NPC");

        // Check if the user did specify a visual waypoint
        if (targetCreature->GetEntry() != VISUAL_WAYPOINT || targetCreature->GetSubtype() != CREATURE_SUBTYPE_TEMPORARY_SUMMON)
        {
            PSendSysMessage(LANG_WAYPOINT_VP_SELECT);
            SetSentErrorMessage(true);
            return false;
        }
        TemporarySummonWaypoint* wpTarget = dynamic_cast<TemporarySummonWaypoint*>(targetCreature);
        if (!wpTarget)
        {
            PSendSysMessage(LANG_WAYPOINT_VP_SELECT);
            SetSentErrorMessage(true);
            return false;
        }

        // Who moves along this waypoint?
        wpOwner = targetCreature->GetMap()->GetAnyTypeCreature(wpTarget->GetSummonerGuid());
        if (!wpOwner)
        {
            PSendSysMessage(LANG_WAYPOINT_NOTFOUND_NPC, wpTarget->GetSummonerGuid().GetString().c_str());
            SetSentErrorMessage(true);
            return false;
        }
        wpId = wpTarget->GetWaypointId()-1;

        wpPathId = wpTarget->GetPathId();
        wpSource = (WaypointPathOrigin)wpTarget->GetPathOrigin();
    }
    else
    {
        uint32 dbGuid = 0;
        // User did provide <GUID> <WPNUM>
        if (!ExtractUInt32(&args, dbGuid))
        {
            SendSysMessage(LANG_WAYPOINT_NOGUID);
            SetSentErrorMessage(true);
            return false;
        }

        if (!ExtractUInt32(&args, wpId))
        {
            SendSysMessage(LANG_WAYPOINT_NOWAYPOINTGIVEN);
            SetSentErrorMessage(true);
            return false;
        }

        CreatureData const* data = sObjectMgr.GetCreatureData(dbGuid);
        if (!data)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }

        wpOwner = m_session->GetPlayer()->GetMap()->GetAnyTypeCreature(data->GetObjectGuid(dbGuid));
        if (!wpOwner)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }
    }

    if (wpSource == PATH_NO_PATH)                           // No waypoint selected
    {
        if (wpOwner->GetMotionMaster()->GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
            if (WaypointMovementGenerator<Creature> const* wpMMGen = dynamic_cast<WaypointMovementGenerator<Creature> const*>(wpOwner->GetMotionMaster()->GetCurrent()))
                wpMMGen->GetPathInformation(wpPathId, wpSource);

        if (wpSource == PATH_NO_PATH)
            sWaypointMgr.GetDefaultPath(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), &wpSource);
    }

    WaypointPath const* wpPath = sWaypointMgr.GetPathFromOrigin(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpPathId, wpSource);
    if (!wpPath)
    {
        PSendSysMessage(LANG_WAYPOINT_NOTFOUNDPATH, wpOwner->GetGuidStr().c_str(), wpPathId, WaypointManager::GetOriginString(wpSource).c_str());
        SetSentErrorMessage(true);
        return false;
    }

    WaypointPath::const_iterator point = wpPath->find(wpId);
    if (point == wpPath->end())
    {
        PSendSysMessage(LANG_WAYPOINT_NOTFOUND, wpId, wpOwner->GetGuidStr().c_str(), wpPathId, WaypointManager::GetOriginString(wpSource).c_str());
        SetSentErrorMessage(true);
        return false;
    }

    // If no visual WP was selected, but we are not going to remove it
    if (!targetCreature && subCmd != "del")
    {
        targetCreature = Helper_CreateWaypointFor(wpOwner, wpSource, wpPathId, wpId, &(point->second), waypointInfo);
        if (!targetCreature)
        {
            PSendSysMessage(LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
            SetSentErrorMessage(true);
            return false;
        }
    }

    if (subCmd == "del")                                    // Remove WP, no additional command required
    {
        sWaypointMgr.DeleteNode(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpId, wpPathId, wpSource);

        if (TemporarySummonWaypoint* wpCreature = dynamic_cast<TemporarySummonWaypoint*>(targetCreature))
            wpCreature->UnSummon();

        if (wpPath->empty())
        {
            wpOwner->SetDefaultMovementType(RANDOM_MOTION_TYPE);
            wpOwner->GetMotionMaster()->Initialize();
            if (wpOwner->IsAlive())                         // Dead creature will reset movement generator at respawn
            {
                wpOwner->SetDeathState(JUST_DIED);
                wpOwner->Respawn();
            }
            wpOwner->SaveToDB();
        }

        PSendSysMessage(LANG_WAYPOINT_REMOVED);
        return true;
    }
    else if (subCmd == "move")                              // Move to player position, no additional command required
    {
        float x, y, z;
        m_session->GetPlayer()->GetPosition(x, y, z);

        // Move visual waypoint
        targetCreature->NearTeleportTo(x, y, z, targetCreature->GetOrientation());

        sWaypointMgr.SetNodePosition(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpId, wpPathId, wpSource, x, y, z);

        PSendSysMessage(LANG_WAYPOINT_CHANGED);
        return true;
    }
    else if (subCmd == "waittime")
    {
        uint32 waittime;
        if (!ExtractUInt32(&args, waittime))
            return false;

        sWaypointMgr.SetNodeWaittime(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpId, wpPathId, wpSource, waittime);
    }
    else if (subCmd == "scriptid")
    {
        uint32 scriptId;
        if (!ExtractUInt32(&args, scriptId))
            return false;

        if (!sWaypointMgr.SetNodeScriptId(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpId, wpPathId, wpSource, scriptId))
            PSendSysMessage(LANG_WAYPOINT_INFO_UNK_SCRIPTID, scriptId);
    }
    else if (subCmd == "orientation")
    {
        float ori;
        if (!ExtractFloat(&args, ori))
            return false;

        sWaypointMgr.SetNodeOrientation(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpId, wpPathId, wpSource, ori);
    }

    PSendSysMessage(LANG_WAYPOINT_CHANGED_NO, subCmd_str);
    return true;
}

/**
 * .wp show info | on | off | first | last [dbGuid] [pathId [wpOrigin] ]
 *
 * info -> User has selected a visual waypoint before
 *
 * on -> User has selected an NPC; all visual waypoints for this
 *       NPC are added to the world
 *
 * on <dbGuid> -> User did not select an NPC - instead the dbGuid of the
 *              NPC is provided. All visual waypoints for this NPC
 *              are added from the world.
 *
 * off -> User has selected an NPC; all visual waypoints for this
 *        NPC are removed from the world.
 */
bool ChatHandler::HandleWpShowCommand(char* args)
{
    DEBUG_LOG("DEBUG: HandleWpShowCommand");

    if (!*args)
        { return false; }

    CreatureInfo const* waypointInfo = ObjectMgr::GetCreatureTemplate(VISUAL_WAYPOINT);
    if (!waypointInfo || waypointInfo->GetHighGuid() != HIGHGUID_UNIT)
        { return false; }                                       // must exist as normal creature in mangos.sql 'creature_template'

    // first arg: info, on, off, first, last

    char* subCmd_str = ExtractLiteralArg(&args);
    if (!subCmd_str)
        return false;
    std::string subCmd = subCmd_str;                        ///< info, on, off, first, last

    uint32 dbGuid = 0;
    int32 wpPathId = 0;
    WaypointPathOrigin wpOrigin = PATH_NO_PATH;

    // User selected an npc?
    Creature* targetCreature = GetSelectedCreature();
    if (targetCreature)
    {
        if (ExtractOptInt32(&args, wpPathId, 0))            // Fill path-id and source
        {
            uint32 src;
            if (ExtractOptUInt32(&args, src, (uint32)PATH_NO_PATH))
                wpOrigin = (WaypointPathOrigin)src;
        }
    }
    else    // Guid must be provided
    {
        if (!ExtractUInt32(&args, dbGuid))                  // No creature selected and no dbGuid provided
            return false;

        if (ExtractOptInt32(&args, wpPathId, 0))            // Fill path-id and source
        {
            uint32 src = (uint32)PATH_NO_PATH;
            if (ExtractOptUInt32(&args, src, src))
                wpOrigin = (WaypointPathOrigin)src;
        }

        // Params now parsed, check them
        CreatureData const* data = sObjectMgr.GetCreatureData(dbGuid);
        if (!data)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }

        targetCreature = m_session->GetPlayer()->GetMap()->GetCreature(data->GetObjectGuid(dbGuid));
        if (!targetCreature)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }
    }

    Creature* wpOwner;                                      // Npc that is moving
    TemporarySummonWaypoint* wpTarget = nullptr;               // Define here for wp-info command

    // Show info for the selected waypoint (Step one: get moving npc)
    if (subCmd == "info")
    {
        // Check if the user did specify a visual waypoint
        if (targetCreature->GetEntry() != VISUAL_WAYPOINT || targetCreature->GetSubtype() != CREATURE_SUBTYPE_TEMPORARY_SUMMON)
        {
            PSendSysMessage(LANG_WAYPOINT_VP_SELECT);
            SetSentErrorMessage(true);
            return false;
        }
        wpTarget = dynamic_cast<TemporarySummonWaypoint*>(targetCreature);
        if (!wpTarget)
        {
            PSendSysMessage(LANG_WAYPOINT_VP_SELECT);
            SetSentErrorMessage(true);
            return false;
        }

        // Who moves along this waypoint?
        wpOwner = targetCreature->GetMap()->GetAnyTypeCreature(wpTarget->GetSummonerGuid());
        if (!wpOwner)
        {
            PSendSysMessage(LANG_WAYPOINT_NOTFOUND_NPC, wpTarget->GetSummonerGuid().GetString().c_str());
            SetSentErrorMessage(true);
            return false;
        }

        // Ignore params, use information of selected waypoint!
        wpOrigin = (WaypointPathOrigin)wpTarget->GetPathOrigin();
        wpPathId = wpTarget->GetPathId();
    }
    else
        wpOwner = targetCreature;

    // Get the path
    WaypointPath* wpPath = nullptr;
    if (wpOrigin != PATH_NO_PATH)                           // Might have been provided by param
        wpPath = sWaypointMgr.GetPathFromOrigin(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpPathId, wpOrigin);
    else
    {
        if (wpOwner->GetMotionMaster()->GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
            if (WaypointMovementGenerator<Creature> const* wpMMGen = dynamic_cast<WaypointMovementGenerator<Creature> const*>(wpOwner->GetMotionMaster()->GetCurrent()))
            {
                wpMMGen->GetPathInformation(wpPathId, wpOrigin);
                wpPath = sWaypointMgr.GetPathFromOrigin(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpPathId, wpOrigin);
            }

        if (wpOrigin == PATH_NO_PATH)
            wpPath = sWaypointMgr.GetDefaultPath(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), &wpOrigin);
    }

    if (!wpPath || wpPath->empty())
    {
        PSendSysMessage(LANG_WAYPOINT_NOTFOUNDPATH, wpOwner->GetGuidStr().c_str(), wpPathId, WaypointManager::GetOriginString(wpOrigin).c_str());
        SetSentErrorMessage(true);
        return false;
    }

    // Show info for the selected waypoint (Step two: Show actual info)
    if (subCmd == "info")
    {
        // Find the waypoint
        WaypointPath::const_iterator point = wpPath->find(wpTarget->GetWaypointId());
        if (point == wpPath->end())
        {
            PSendSysMessage(LANG_WAYPOINT_NOTFOUND, wpTarget->GetWaypointId(), wpOwner->GetGuidStr().c_str(), wpPathId, WaypointManager::GetOriginString(wpOrigin).c_str());
            SetSentErrorMessage(true);
            return false;
        }

        PSendSysMessage(LANG_WAYPOINT_INFO_TITLE, wpTarget->GetWaypointId(), wpOwner->GetGuidStr().c_str(), wpPathId, WaypointManager::GetOriginString(wpOrigin).c_str());
        PSendSysMessage(LANG_WAYPOINT_INFO_WAITTIME, point->second.delay);
        PSendSysMessage(LANG_WAYPOINT_INFO_ORI, point->second.orientation);
        PSendSysMessage(LANG_WAYPOINT_INFO_SCRIPTID, point->second.script_id);
        if (wpOrigin == PATH_FROM_SPECIAL)
            PSendSysMessage(LANG_WAYPOINT_INFO_AISCRIPT, wpOwner->GetScriptName().c_str());

        return true;
    }

    if (subCmd == "on")
    {
        UnsummonVisualWaypoints(m_session->GetPlayer(), wpOwner->GetObjectGuid());

        for (const auto& itr : *wpPath)
        {
            if (!Helper_CreateWaypointFor(wpOwner, wpOrigin, wpPathId, itr.first, &(itr.second), waypointInfo))
            {
                printf("error %s wpPathId %i", wpOwner->GetName(), wpPathId);
                PSendSysMessage(LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
                SetSentErrorMessage(true);
                return false;
            }
        }

        return true;
    }

    if (subCmd == "first")
    {
        if (!Helper_CreateWaypointFor(wpOwner, wpOrigin, wpPathId, wpPath->begin()->first, &(wpPath->begin()->second), waypointInfo))
        {
            PSendSysMessage(LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
            SetSentErrorMessage(true);
            return false;
        }

        // player->PlayerTalkClass->SendPointOfInterest(x, y, 6, 6, 0, "First Waypoint");
        return true;
    }

    if (subCmd == "last")
    {
        if (!Helper_CreateWaypointFor(wpOwner, wpOrigin, wpPathId, wpPath->rbegin()->first, &(wpPath->rbegin()->second), waypointInfo))
        {
            PSendSysMessage(LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
            SetSentErrorMessage(true);
            return false;
        }

        // player->PlayerTalkClass->SendPointOfInterest(x, y, 6, 6, 0, "Last Waypoint");
        return true;
    }

    if (subCmd == "off")
    {
        UnsummonVisualWaypoints(m_session->GetPlayer(), wpOwner->GetObjectGuid());
        PSendSysMessage(LANG_WAYPOINT_VP_ALLREMOVED);
        return true;
    }

    return false;
}                                                           // HandleWpShowCommand

                                                            /// [Guid if no selected unit] <filename> [pathId [wpOrigin] ]
bool ChatHandler::HandleWpExportCommand(char* args)
{
    if (!*args)
        return false;

    Creature* wpOwner;
    WaypointPathOrigin wpOrigin = PATH_NO_PATH;
    int32 wpPathId = 0;

    if (Creature* targetCreature = GetSelectedCreature())
    {
        // Check if the user did specify a visual waypoint
        if (targetCreature->GetEntry() == VISUAL_WAYPOINT && targetCreature->GetSubtype() == CREATURE_SUBTYPE_TEMPORARY_SUMMON)
        {
            TemporarySummonWaypoint* wpTarget = dynamic_cast<TemporarySummonWaypoint*>(targetCreature);
            if (!wpTarget)
            {
                PSendSysMessage(LANG_WAYPOINT_VP_SELECT);
                SetSentErrorMessage(true);
                return false;
            }

            // Who moves along this waypoint?
            wpOwner = targetCreature->GetMap()->GetAnyTypeCreature(wpTarget->GetSummonerGuid());
            if (!wpOwner)
            {
                PSendSysMessage(LANG_WAYPOINT_NOTFOUND_NPC, wpTarget->GetSummonerGuid().GetString().c_str());
                SetSentErrorMessage(true);
                return false;
            }
            wpOrigin = (WaypointPathOrigin)wpTarget->GetPathOrigin();
            wpPathId = wpTarget->GetPathId();
        }
        else // normal creature selected
            wpOwner = targetCreature;
    }
    else
    {
        uint32 dbGuid;
        if (!ExtractUInt32(&args, dbGuid))
        {
            PSendSysMessage(LANG_WAYPOINT_NOGUID);
            SetSentErrorMessage(true);
            return false;
        }

        CreatureData const* data = sObjectMgr.GetCreatureData(dbGuid);
        if (!data)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }

        if (m_session->GetPlayer()->GetMapId() != data->position.mapId)
        {
            PSendSysMessage(LANG_COMMAND_CREATUREATSAMEMAP, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }

        wpOwner = m_session->GetPlayer()->GetMap()->GetAnyTypeCreature(data->GetObjectGuid(dbGuid));
        if (!wpOwner)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }
    }

    // wpOwner is now known, in case of export by visual waypoint also the to be exported path
    char* export_str = ExtractLiteralArg(&args);
    if (!export_str)
    {
        PSendSysMessage(LANG_WAYPOINT_ARGUMENTREQ, "export");
        SetSentErrorMessage(true);
        return false;
    }

    if (wpOrigin == PATH_NO_PATH)                           // No WP selected, Extract optional arguments
    {
        if (ExtractOptInt32(&args, wpPathId, 0))            // Fill path-id and source
        {
            uint32 src = (uint32)PATH_NO_PATH;
            if (ExtractOptUInt32(&args, src, src))
                wpOrigin = (WaypointPathOrigin)src;
            else // pathId provided but no destination
            {
                if (wpPathId != 0)
                    wpOrigin = PATH_FROM_ENTRY;             // Multiple Paths must only be assigned by entry
            }
        }

        if (wpOrigin == PATH_NO_PATH)
        {
            if (wpOwner->GetMotionMaster()->GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
                if (WaypointMovementGenerator<Creature> const* wpMMGen = dynamic_cast<WaypointMovementGenerator<Creature> const*>(wpOwner->GetMotionMaster()->GetCurrent()))
                    wpMMGen->GetPathInformation(wpPathId, wpOrigin);
            if (wpOrigin == PATH_NO_PATH)
                sWaypointMgr.GetDefaultPath(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), &wpOrigin);
        }
    }

    WaypointPath const* wpPath = sWaypointMgr.GetPathFromOrigin(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpPathId, wpOrigin);
    if (!wpPath || wpPath->empty())
    {
        PSendSysMessage(LANG_WAYPOINT_NOTHINGTOEXPORT);
        SetSentErrorMessage(true);
        return false;
    }

    std::ofstream outfile;
    outfile.open(export_str);

    std::string table;
    char const* key_field;
    uint32 key;
    switch (wpOrigin)
    {
        case PATH_FROM_ENTRY: key = wpOwner->GetEntry();    key_field = "entry";    table = "creature_movement_template"; break;
        case PATH_FROM_GUID: key = wpOwner->GetGUIDLow();   key_field = "id";       table = "creature_movement"; break;
        case PATH_FROM_SPECIAL: key = wpOwner->GetEntry();  key_field = "id";    table = "creature_movement_special"; break;
        case PATH_NO_PATH:
            return false;
    }

    outfile << "DELETE FROM " << table << " WHERE " << key_field << "=" << key << ";\n";
    outfile << "INSERT INTO " << table << " (" << key_field << ", point, position_x, position_y, position_z, orientation, waittime, script_id) VALUES\n";

    WaypointPath::const_iterator itr = wpPath->begin();
    uint32 countDown = wpPath->size();
    for (; itr != wpPath->end(); ++itr, --countDown)
    {
        outfile << "(" << key << ",";
        outfile << itr->first << ",";
        outfile << itr->second.x << ",";
        outfile << itr->second.y << ",";
        outfile << itr->second.z << ",";
        outfile << itr->second.orientation << ",";
        outfile << itr->second.delay << ",";
        outfile << itr->second.script_id << ")";
        if (countDown > 1)
            outfile << ",\n";
        else
            outfile << ";\n";
    }

    PSendSysMessage(LANG_WAYPOINT_EXPORTED);
    outfile.close();

    return true;
}

bool ChatHandler::HandleEscortShowWpCommand(char *args)
{
    DEBUG_LOG("DEBUG: HandleEscortShowWpCommand");

    auto waypointInfo = ObjectMgr::GetCreatureTemplate(VISUAL_WAYPOINT);
    if (!waypointInfo || waypointInfo->GetHighGuid() != HIGHGUID_UNIT)
        return false; // must exist as normal creature in mangos.sql 'creature_template'

    CreatureInfo const* cInfo = nullptr;
    Creature const* pCreature = GetSelectedCreature();
    uint32 cr_id;

    // optional number or [name] Shift-click form |color|Hcreature_entry:creature_id|h[name]|h|r
    if (*args && ExtractUint32KeyFromLink(&args, "Hcreature_entry", cr_id))
        cInfo = ObjectMgr::GetCreatureTemplate(cr_id);
    else if (pCreature)
        cInfo = pCreature->GetCreatureInfo();

    if (!cInfo)
    {
        if (cr_id)
            PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, cr_id);
        else
            SendSysMessage(LANG_SELECT_CREATURE);

        SetSentErrorMessage(true);
        return false;
    }

    const auto pPlayer = m_session->GetPlayer();
    auto map = pPlayer->GetMap();

    auto& scriptPoints = sScriptMgr.GetPointMoveList(cInfo->entry);

    if (scriptPoints.empty())
    {
        PSendSysMessage(LANG_WAYPOINT_NOTFOUND, cInfo->entry);
        SetSentErrorMessage(true);
        return false;
    }

    for (const auto& wp : scriptPoints)
    {
        CreatureCreatePos pos{map, wp.fX, wp.fY, wp.fZ, pPlayer->GetOrientation()};
        Creature* wpCreature = new Creature;

        if (!wpCreature->Create(map->GenerateLocalLowGuid(HIGHGUID_UNIT), pos, waypointInfo, TEAM_NONE, waypointInfo->entry))
        {
            PSendSysMessage(LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
            delete wpCreature;
            return false;
        }

        wpCreature->SetVisibility(VISIBILITY_OFF);

        wpCreature->SaveToDB(map->GetId());
        wpCreature->LoadFromDB(wpCreature->GetGUIDLow(), map);
        map->Add(wpCreature);
    }

    return true;
}

bool ChatHandler::HandleEscortHideWpCommand(char* /*args*/)
{
    DEBUG_LOG("DEBUG: HandleEscortHideWpCommand");

    auto map = m_session->GetPlayer()->GetMap();

    std::unique_ptr<QueryResult> result(WorldDatabase.PQuery("SELECT guid FROM creature WHERE id=%u AND map=%u", VISUAL_WAYPOINT, map->GetId()));
    if (!result)
    {
        SendSysMessage(LANG_WAYPOINT_VP_NOTFOUND);
        SetSentErrorMessage(true);
        return false;
    }
    bool hasError = false;
    do
    {
        Field* fields = result->Fetch();
        uint32 wpGuid = fields[0].GetUInt32();
        Creature* pCreature = map->GetCreature(ObjectGuid(HIGHGUID_UNIT, VISUAL_WAYPOINT, wpGuid));
        if (!pCreature)
        {
            hasError = true;
            WorldDatabase.PExecuteLog("DELETE FROM creature WHERE guid=%u", wpGuid);
        }
        else
        {
            pCreature->DeleteFromDB();
            pCreature->AddObjectToRemoveList();
        }
    } while (result->NextRow());

    if (hasError)
    {
        PSendSysMessage(LANG_WAYPOINT_TOOFAR1);
        PSendSysMessage(LANG_WAYPOINT_TOOFAR2);
        PSendSysMessage(LANG_WAYPOINT_TOOFAR3);
    }

    SendSysMessage(LANG_WAYPOINT_VP_ALLREMOVED);

    return true;
}

bool ChatHandler::HandleEscortAddWpCommand(char *args)
{
    uint32 creatureEntry = 0;
    uint32 waypointId   = 0;
    uint32 waittime     = 0;
    Player*pPlayer      = m_session->GetPlayer();
    sscanf(args, "%u %u %u", &creatureEntry, &waittime, &waypointId);
    if (creatureEntry == 0)
    {
        Creature* target = GetSelectedCreature();
        if (target)
            creatureEntry = target->GetEntry();
    }
    if (creatureEntry == 0)
    {
        SendSysMessage("Utilisation :");
        SendSysMessage(".escorte addwp [$CreatureEntry=TARGET [$NewWaitTime=0 [$WayPointId=NEXT]]]");
        SetSentErrorMessage(true);
        return false;
    }
    QueryResult* pResult = nullptr;
    Field* pFields       = nullptr;
    if (waypointId == 0)
    {
        pResult = WorldDatabase.PQuery("SELECT MAX(pointid) FROM script_waypoint WHERE entry=%u", creatureEntry);
        if (pResult)
        {
            pFields = pResult->Fetch();
            waypointId = pFields[0].GetUInt32() + 1;
        }
    }
    WorldDatabase.PExecute("DELETE FROM script_waypoint WHERE entry=%u AND pointid=%u", creatureEntry, waypointId);
    WorldDatabase.PExecute("INSERT INTO script_waypoint SET "
                           "entry=%u, "
                           "pointid=%u, "
                           "location_x=%f, "
                           "location_y=%f, "
                           "location_z=%f, "
                           "waittime=%u; ",
                           creatureEntry, waypointId,
                           finiteAlways(pPlayer->GetPositionX()), finiteAlways(pPlayer->GetPositionY()), finiteAlways(pPlayer->GetPositionZ()),
                           waittime);
    PSendSysMessage("Point de passage %u ajoute pour la creature %u (attente %u ms)", waypointId, creatureEntry, waittime);
    return true;
}

bool ChatHandler::HandleEscortModifyWpCommand(char *args)
{
    uint32 creatureEntry = 0;
    uint32 waypointId    = 0;
    uint32 newWaitTime   = 0;
    sscanf(args, "%u %u %u", &creatureEntry, &waypointId, &newWaitTime);
    if (waypointId == 0)
    {
        SendSysMessage("Usage :");
        SendSysMessage(".escorte modwp $CreatureEntry $PointId [$NewWaitTime=0]");
        SetSentErrorMessage(true);
        return false;
    }
    Player* pPlayer = m_session->GetPlayer();

    WorldDatabase.PExecute("UPDATE script_waypoint "
                           "SET location_x=%f, location_y=%f, location_z=%f, waittime=%u"
                           "WHERE entry=%u AND pointid=%u",
                           finiteAlways(pPlayer->GetPositionX()), finiteAlways(pPlayer->GetPositionY()), finiteAlways(pPlayer->GetPositionZ()), newWaitTime,
                           creatureEntry, waypointId);
    SendSysMessage("Waypoint modified.");
    return true;
}

bool ChatHandler::HandleEscortCreateCommand(char *args)
{
    uint32 creatureEntry = 0;
    uint32 questEntry = 0;
    uint32 faction = 0;
    sscanf(args, "%u %u %u", &creatureEntry, &questEntry, &faction);


    if (faction == 0)
    {
        SendSysMessage("Usage :");
        SendSysMessage(".escorte create $CreatureEntry $QuestEntry $FactionInEscort");
        SetSentErrorMessage(true);
        return false;
    }

    WorldDatabase.PExecute("DELETE FROM script_escort_data WHERE creature_id=%u", creatureEntry);
    WorldDatabase.PExecute("INSERT INTO script_escort_data (creature_id, quest, escort_faction) VALUES "
                           "(%u, %u, %u)",
                           creatureEntry, questEntry, faction
                          );

    PSendSysMessage("Escort quest added to DB. Restart the server for the changes to be taken into account.");
    return true;
}

bool ChatHandler::HandleEscortClearWpCommand(char *args)
{
    uint32 creatureEntry = 0;
    sscanf(args, "%u", &creatureEntry);
    if (creatureEntry == 0)
    {
        SendSysMessage("Usage :");
        SendSysMessage(".escorte clearwp $CreatureEntry");
        SetSentErrorMessage(true);
        return false;
    }
    WorldDatabase.PExecute("DELETE FROM script_waypoint WHERE creature_id=%u", creatureEntry);
    PSendSysMessage("All script_waypoint entries for creature %u have been removed.", creatureEntry);
    return true;
}