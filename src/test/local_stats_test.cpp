/* The in-game editor's Local Stats saving (local_stats.cpp) against a mods folder of its own: the mod made when missing,
 * a file the launcher wrote kept as it was apart from the changed fields, removals down to deleting the mod, and a broken
 * file or a held lock refusing the save while keeping the changes for the next one. Usage: local_stats_test.exe */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string>
#include "../local_stats.h"
#include "../json.h"

static int g_failures;
static void Check(bool ok, const char* what) { printf("%s %s\n", ok ? "  ok  " : "  FAIL", what); if (!ok) g_failures++; }

static std::string Read(const std::wstring& path)
{
    std::string text;
    FILE* file = _wfopen(path.c_str(), L"rb");
    if (!file) return "<missing>";
    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof buffer, file)) > 0) text.append(buffer, n);
    fclose(file);
    return text;
}

static void Write(const std::wstring& path, const std::string& text)
{
    FILE* file = _wfopen(path.c_str(), L"wb");
    if (!file) { Check(false, "the test file can be opened for writing"); return; }
    fwrite(text.data(), 1, text.size(), file);
    fclose(file);
}

static bool Exists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

int main()
{
    wchar_t temp[MAX_PATH];
    DWORD length = GetTempPathW(MAX_PATH, temp);
    if (!length || length >= MAX_PATH) { puts("FAIL could not find the Local Stats test directory"); return 1; }
    const std::wstring mods = std::wstring(temp, length) + L"hellforge-local-stats-test-" + std::to_wstring(GetCurrentProcessId());
    const std::string modsUtf8 = hfjson::Utf8(mods);
    if (modsUtf8.empty() || !CreateDirectoryW(mods.c_str(), NULL)) { puts("FAIL could not create the Local Stats test directory"); return 1; }
    const std::wstring folder = mods + L"\\zz.local.stats", stats = folder + L"\\hellforge-stats.json", manifest = folder + L"\\hellforge-mod.json";
    std::string error;

    /* No Local Stats yet: the mod is made. */
    localstats::Set("weapons", "AR-23 Liberator", "weapon.fire_rate", 1200, false);
    Check(localstats::Pending(), "a change is pending");
    Check(localstats::Save(modsUtf8, &error) && !localstats::Pending(), "saved into a new mod");
    Check(Read(stats) == "{\n  \"format\": 1,\n  \"weapons\": {\n    \"AR-23 Liberator\": {\n      \"weapon.fire_rate\": 1200\n    }\n  }\n}\n",
        "the new file holds the change");
    Check(Read(manifest).find("\"editor\": \"hellforge-stats-editor-v1\"") != std::string::npos, "the manifest is the launcher's");

    /* A file the launcher wrote: everything else stays as it was. */
    Write(stats, "{\n  \"format\": 1,\n  \"weapons\": {\n    \"AR-23C Liberator Concussive\": {\n      \"damage.ap_direct\": 10,\n"
        "      \"damage.standard_damage\": 2.50e4\n    },\n    \"AR-23 Liberator\": {\n      \"weapon.fire_rate\": 900,\n"
        "      \"weapon.mag_size\": 45\n    }\n  },\n  \"note\": \"caf\\u00e9 \\\"x\\\"\"\n}\n");
    localstats::Set("weapons", "AR-23 Liberator", "weapon.fire_rate", 640.5, false);
    localstats::Set("weapons", "AR-23 Liberator", "weapon.fire_rate", 750.25, false); /* the latest change of a field wins */
    Check(localstats::Save(modsUtf8, &error), "saved over the launcher's file");
    Check(Read(stats) == "{\n  \"format\": 1,\n  \"weapons\": {\n    \"AR-23C Liberator Concussive\": {\n      \"damage.ap_direct\": 10,\n"
        "      \"damage.standard_damage\": 2.50e4\n    },\n    \"AR-23 Liberator\": {\n      \"weapon.fire_rate\": 750.25,\n"
        "      \"weapon.mag_size\": 45\n    }\n  },\n  \"note\": \"caf\xc3\xa9 \\\"x\\\"\"\n}\n", "only the changed field differs");

    /* Removals: the field, then its target, then the mod. */
    localstats::Remove("weapons", "AR-23 Liberator", "weapon.fire_rate");
    localstats::Remove("weapons", "AR-23 Liberator", "weapon.mag_size");
    localstats::Remove("note", "x", "y"); /* not in the file */
    Check(localstats::Save(modsUtf8, &error) && Read(stats).find("\"AR-23 Liberator\"") == std::string::npos &&
        Read(stats).find("AR-23C Liberator Concussive") != std::string::npos, "a target with nothing left is removed");
    Write(stats, "{\n  \"format\": 1,\n  \"weapons\": {\n    \"AR-23 Liberator\": {\n      \"weapon.fire_rate\": 900\n    }\n  }\n}\n");
    localstats::Remove("weapons", "AR-23 Liberator", "weapon.fire_rate");
    Check(localstats::Save(modsUtf8, &error) && !Exists(folder), "the mod goes when nothing is left");

    /* A broken file is left alone, and the change waits. */
    CreateDirectoryW(folder.c_str(), NULL);
    Write(manifest, "{}");
    Write(stats, "{\n  \"format\": 1,\n  \"weapons\": {\n");
    localstats::Set("weapons", "AR-23 Liberator", "weapon.fire_rate", 1200, false);
    Check(!localstats::Save(modsUtf8, &error) && localstats::Pending() && Read(stats) == "{\n  \"format\": 1,\n  \"weapons\": {\n",
        "a broken file is refused and the change kept");
    printf("        (%s)\n", error.c_str());

    /* The launcher saving: the lock is held, the save gives up after a while, and goes through once it is free. */
    Write(stats, "{\n  \"format\": 1\n}\n");
    HANDLE lock = CreateFileW((mods + L"\\.hellforge-stats-editor.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, 0, NULL);
    Check(!localstats::Save(modsUtf8, &error) && localstats::Pending(), "a held lock is waited for, then the change kept");
    printf("        (%s)\n", error.c_str());
    CloseHandle(lock);
    Check(localstats::Save(modsUtf8, &error) && Read(stats).find("\"weapon.fire_rate\": 1200") != std::string::npos, "saved once the lock is free");

    /* A mod's content ("mod:" categories) goes to the editor's own file, never into Local Stats. */
    const std::wstring modFile = mods + L"\\mod-stats.json";
    localstats::SetModFile(modFile);
    std::string before = Read(stats);
    localstats::Set("mod:you/mod/sentry", "you/mod/sentry", "stratagem.cooldown", 150, false);
    double got = 0;
    Check(localstats::Get("mod:you/mod/sentry", "you/mod/sentry", "stratagem.cooldown", &got) && got == 150, "a mod entry reads back");
    Check(localstats::Save(modsUtf8, &error) && Read(stats) == before && Read(modFile).find("\"stratagem.cooldown\": 150") != std::string::npos,
        "a mod entry is saved in the editor's file, Local Stats unchanged");
    localstats::Set("weapons", "AR-23 Liberator", "weapon.fire_rate", 999, false);
    localstats::Set("mod:you/mod/sentry", "you/mod/sentry", "turret.yaw_speed", 30, false);
    Check(localstats::Save(modsUtf8, &error) && Read(stats).find("999") != std::string::npos && Read(stats).find("you/mod") == std::string::npos &&
        Read(modFile).find("turret.yaw_speed") != std::string::npos && Read(modFile).find("stratagem.cooldown") != std::string::npos,
        "both saved at once, each to its own file");
    Check(localstats::Load(modsUtf8, &error) && localstats::Get("mod:you/mod/sentry", "you/mod/sentry", "turret.yaw_speed", &got) && got == 30,
        "the editor's file is read back with Local Stats");
    localstats::Remove("mod:you/mod/sentry", "you/mod/sentry", "turret.yaw_speed");
    localstats::Remove("mod:you/mod/sentry", "you/mod/sentry", "stratagem.cooldown");
    Check(localstats::Save(modsUtf8, &error) && Read(modFile).find("you/mod") == std::string::npos, "mod entries are removed");

    /* Joined to a friend's lobby: the host's saved changes are what Get and Entries show; this machine's stay in its file
     * and come back with ShowOwn. */
    double own = 0;
    Check(localstats::Get("weapons", "AR-23 Liberator", "weapon.fire_rate", &own) && own == 999, "this machine's value before joining");
    unsigned version = localstats::Version();
    localstats::ShowHost("{\"format\":1,\"weapons\":{\"AR-23 Liberator\":{\"weapon.recoil\":5}}}",
        "{\"format\":1,\"mod:host/mod/silo\":{\"host/mod/silo\":{\"stratagem.cooldown\":60}}}");
    Check(localstats::ShowingHost() && localstats::Version() != version, "joined: the host's are shown");
    Check(!localstats::Get("weapons", "AR-23 Liberator", "weapon.fire_rate", &got) && localstats::Get("weapons", "AR-23 Liberator", "weapon.recoil", &got) && got == 5 &&
        localstats::Get("mod:host/mod/silo", "host/mod/silo", "stratagem.cooldown", &got) && got == 60, "Get answers with the host's, not this machine's");
    Check(localstats::Entries().size() == 2 && localstats::OwnEntries().size() == 1 && localstats::OwnEntries()[0].value == 999,
        "Entries are the host's; OwnEntries this machine's");
    localstats::ShowHost("", "not json");
    Check(localstats::ShowingHost() && localstats::Entries().empty(), "no host file yet (or a broken one): nothing shown");
    localstats::ShowOwn();
    Check(!localstats::ShowingHost() && localstats::Get("weapons", "AR-23 Liberator", "weapon.fire_rate", &got) && got == 999, "back on its own: this machine's");
    DeleteFileW(modFile.c_str());

    /* Sandboxed (low integrity): each save writes every change saved this session to changes.json; mods/ is only read. */
    const std::wstring sandboxDir = mods + L"\\sandbox-stats", journal = sandboxDir + L"\\changes.json";
    CreateDirectoryW(sandboxDir.c_str(), NULL);
    localstats::SetSandboxSaveFolder(sandboxDir);
    localstats::SetModFile(modFile);
    std::string realStats = Read(stats);
    localstats::Set("weapons", "AR-23 Liberator", "weapon.fire_rate", 500, false);
    Check(localstats::Save(modsUtf8, &error) && !localstats::Pending() && Read(journal).find("\"value\": 500") != std::string::npos,
        "sandbox: a save writes the change list");
    localstats::Set("weapons", "AR-23 Liberator", "weapon.mag_size", 60, true);
    localstats::Set("mod:you/mod/sentry", "you/mod/sentry", "stratagem.cooldown", 200, true);
    Check(localstats::Save(modsUtf8, &error) && Read(journal).find("\"value\": 500") != std::string::npos &&
        Read(journal).find("weapon.mag_size") != std::string::npos && Read(journal).find("stratagem.cooldown") != std::string::npos,
        "sandbox: a second save keeps the first's changes, mod entries among them");
    localstats::Remove("weapons", "AR-23 Liberator", "weapon.fire_rate");
    Check(localstats::Save(modsUtf8, &error) && Read(journal).find("\"value\": 500") == std::string::npos &&
        Read(journal).find("\"remove\": true") != std::string::npos, "sandbox: a removal replaces the field's change");
    Check(Read(stats) == realStats && !Exists(modFile), "sandbox: nothing in mods/ was written");
    Check(localstats::Get("weapons", "AR-23 Liberator", "weapon.mag_size", &got) && got == 60 &&
        !localstats::Get("weapons", "AR-23 Liberator", "weapon.fire_rate", &got), "sandbox: the editor shows the saved changes");
    Check(localstats::Load(modsUtf8, &error) && localstats::Get("weapons", "AR-23 Liberator", "weapon.mag_size", &got) && got == 60 &&
        localstats::Get("mod:you/mod/sentry", "you/mod/sentry", "stratagem.cooldown", &got) && got == 200 &&
        !localstats::Get("weapons", "AR-23 Liberator", "weapon.fire_rate", &got), "sandbox: read again (the editor opens), the saved changes still show");
    /* A change list that cannot be written: the changes wait for the next save. */
    const std::wstring blocker = journal + L".ingame.tmp";
    CreateDirectoryW(blocker.c_str(), NULL);
    localstats::Set("weapons", "AR-23 Liberator", "weapon.recoil", 3, false);
    Check(!localstats::Save(modsUtf8, &error) && localstats::Pending(), "sandbox: a failed save keeps the change");
    RemoveDirectoryW(blocker.c_str());
    Check(localstats::Save(modsUtf8, &error) && Read(journal).find("weapon.recoil") != std::string::npos, "sandbox: saved once it can be written");
    /* Back to normal: saves go to the mods folder again. */
    localstats::SetSandboxSaveFolder(L"");
    localstats::Set("weapons", "AR-23 Liberator", "weapon.fire_rate", 1200, false);
    Check(localstats::Save(modsUtf8, &error) && Read(stats).find("1200") != std::string::npos, "sandbox off: saves go to the real mods folder");
    DeleteFileW(journal.c_str()); DeleteFileW(modFile.c_str()); RemoveDirectoryW(sandboxDir.c_str());

    DeleteFileW(stats.c_str()); DeleteFileW(manifest.c_str()); RemoveDirectoryW(folder.c_str());
    DeleteFileW((mods + L"\\.hellforge-stats-editor.lock").c_str()); RemoveDirectoryW(mods.c_str());
    printf("%s local stats: %d failure(s)\n", g_failures ? "FAIL" : "PASS", g_failures);
    return g_failures ? 1 : 0;
}
