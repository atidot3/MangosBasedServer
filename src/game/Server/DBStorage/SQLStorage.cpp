#include <Database/DBStorage.h>

const char MapInfosrcfmt[] = "isiiisisiiii";
const char MapInfodstfmt[] = "iiiiiiiiiiii";
const char AreaEntryInfosrcfmt[] = "iiiiiis";
const char AreaEntryInfodstfmt[] = "iiiiiii";
const char ClassLevelStat[] = "iiiii";
const char XpForLevel[] = "ii";
const char PlayerCreateInfo[] = "iiiiffff";
const char PlayerCreateInfoItem[] = "iiii";
const char PlayerCreateInfoSpellsrcfrm[] = "iiis";
const char PlayerCreateInfoSpelldstfrm[] = "iiii";

SQLStorage sMapEntry(MapInfosrcfmt, MapInfodstfmt, "id", "map");
SQLStorage sAreaEntry(AreaEntryInfosrcfmt, AreaEntryInfodstfmt, "id", "areatable");
SQLStorage sClassLevelStatsEntry(ClassLevelStat, "id", "player_classlevelstats");
SQLStorage sXpForLevelEntry(XpForLevel, "lvl", "player_xp_for_level");
SQLStorage sCreateInfoEntry(PlayerCreateInfo, "id", "playercreateinfo");
SQLStorage sPlayerCreateInfoItem(PlayerCreateInfoItem, "id", "playercreateinfo_item");
SQLStorage sPlayerCreateInfoSpellEntry(PlayerCreateInfoSpellsrcfrm, PlayerCreateInfoSpelldstfrm, "id", "playercreateinfo_spell");
