#include "cbase.h"
#include "hl2mp_playerscoremanager.h"

static bool SteamIDLessFunc(const CSteamID& lhs, const CSteamID& rhs)
{
    return lhs.ConvertToUint64() < rhs.ConvertToUint64();
}

PlayerScoreManager::PlayerScoreManager() : m_PlayerScores(0, 0, SteamIDLessFunc)
{
}

void PlayerScoreManager::SavePlayerScore(const CSteamID& steamID, int frags, int deaths)
{
    int index = m_PlayerScores.Find(steamID);
    if (index == m_PlayerScores.InvalidIndex())
    {
        index = m_PlayerScores.Insert(steamID);
    }

    m_PlayerScores[index] = { frags, deaths };
}

bool PlayerScoreManager::LoadPlayerScore(const CSteamID& steamID, int &outFrags, int &outDeaths)
{
    int index = m_PlayerScores.Find(steamID);
    if (index != m_PlayerScores.InvalidIndex())
    {
        outFrags = m_PlayerScores[index].frags;
        outDeaths = m_PlayerScores[index].deaths;
        return true;
    }
    return false;
}

void PlayerScoreManager::ClearAllScores()
{
    m_PlayerScores.RemoveAll();
}

PlayerScoreManager g_PlayerScoreManager;