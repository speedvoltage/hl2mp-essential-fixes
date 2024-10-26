#include "utlmap.h"
#include "eiface.h"

struct PlayerScore
{
    int frags;
    int deaths;
};

class PlayerScoreManager
{
public:
    PlayerScoreManager();
    void SavePlayerScore(const CSteamID& steamID, int frags, int deaths);
    bool LoadPlayerScore(const CSteamID& steamID, int &outFrags, int &outDeaths);
    void ClearAllScores();

private:
    CUtlMap<CSteamID, PlayerScore> m_PlayerScores;
};

extern PlayerScoreManager g_PlayerScoreManager;