#ifndef MANGOS_BattleBotAI_H
#define MANGOS_BattleBotAI_H

#include "CombatBotBaseAI.h"
#include "BattleBotWaypoints.h"

enum BattleBotWsgWaitSpot
{
    BB_WSG_WAIT_SPOT_SPAWN,
    BB_WSG_WAIT_SPOT_LEFT,
    BB_WSG_WAIT_SPOT_RIGHT
};

enum FlagSpellsWS
{
    AURA_WARSONG_FLAG    = 23333,
    AURA_SILVERWING_FLAG = 23335
};

class BattleBotAI : public CombatBotBaseAI
{
public:

    BattleBotAI(uint8 race, uint8 class_, uint32 mapId, uint32 instanceId, float x, float y, float z, float o, uint8 bgId)
        : CombatBotBaseAI(),  m_race(race), m_class(class_), m_mapId(mapId), m_instanceId(instanceId), m_x(x), m_y(y), m_z(z), m_o(o), m_battlegroundId(bgId)
    {
        m_updateTimer.Reset(2000);
    }
    bool OnSessionLoaded(PlayerBotEntry* entry, WorldSession* sess) override
    {
        return SpawnNewPlayer(sess, m_class, m_race, m_mapId, m_instanceId, m_x, m_y, m_z, m_o);
    }

    void OnPlayerLogin() final;
    void UpdateAI(uint32 const diff) final;
    void OnPacketReceived(WorldPacket const* packet) final;
    void SendFakePacket(uint16 opcode) final;
    void MovementInform(uint32 MovementType, uint32 Data = 0) final;

    bool DrinkAndEat();
    bool UseMount();
    uint32 GetMountSpellId() const;
    void AddPremadeGearAndSpells();
    bool CheckForUnreachableTarget();
    float GetMaxAggroDistanceForMap() const;
    bool AttackStart(Unit* pVictim);
    Unit* SelectAttackTarget(Unit* pExcept = nullptr) const;
    Unit* SelectFollowTarget() const;

    void OnJustRevived();
    void OnJustDied();
    void OnEnterBattleGround();
    void OnLeaveBattleGround();

    void UpdateBattleGroundAI();
    void UpdateInCombatAI() final;
    void UpdateOutOfCombatAI() final;
    void UpdateInCombatAI_Paladin() final;
    void UpdateOutOfCombatAI_Paladin() final;
    void UpdateInCombatAI_Shaman() final;
    void UpdateOutOfCombatAI_Shaman() final;
    void UpdateInCombatAI_Hunter() final;
    void UpdateOutOfCombatAI_Hunter() final;
    void UpdateInCombatAI_Mage() final;
    void UpdateOutOfCombatAI_Mage() final;
    void UpdateInCombatAI_Priest() final;
    void UpdateOutOfCombatAI_Priest() final;
    void UpdateInCombatAI_Warlock() final;
    void UpdateOutOfCombatAI_Warlock() final;
    void UpdateInCombatAI_Warrior() final;
    void UpdateOutOfCombatAI_Warrior() final;
    void UpdateInCombatAI_Rogue() final;
    void UpdateOutOfCombatAI_Rogue() final;
    void UpdateInCombatAI_Druid() final;
    void UpdateOutOfCombatAI_Druid() final;

    uint8 m_battlegroundId = 0;
    ShortTimeTracker m_updateTimer;
    uint8 m_race = 0;
    uint8 m_class = 0;
    uint32 m_mapId = 0;
    uint32 m_instanceId = 0;
    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_z = 0.0f;
    float m_o = 0.0f;
    bool m_wasDead = false;
    bool m_wasInBG = false;

    // Movement System
    void UpdateWaypointMovement();
    void DoGraveyardJump();
    void MoveToNextPoint();
    bool StartNewPathFromBeginning();
    void StartNewPathFromAnywhere();
    bool StartNewPathToObjective();
    bool StartNewPathToPosition(Position const& position, std::vector<BattleBotPath*> const& vPaths);
    void ClearPath();
    void StopMoving();
    bool m_doingGraveyardJump = false;
    bool m_movingInReverse = false;
    uint32 m_currentPoint = 0;
    BattleBotPath* m_currentPath = nullptr;
    uint8 m_waitingSpot = BB_WSG_WAIT_SPOT_SPAWN;
};

#endif
