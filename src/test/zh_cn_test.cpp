#include "../app_internal.h"
#include <cassert>
#include <cstdio>

#ifdef NDEBUG
#error Localization tests require active assertions.
#endif

static void VerifyDisplay()
{
    using zh_cn::Display;
    using zh_cn::Domain;

    zh_cn::SetFontReady(false);
    assert(Display(Domain::Ui, "APPLY", "APPLY") == "APPLY");
    zh_cn::SetFontReady(true);
    assert(Display(Domain::Ui, "APPLY", "APPLY") == u8"应用");
    assert(Display(Domain::Item, "weapons/AR-23 Liberator", "AR-23 Liberator") == u8"AR-23“解放者”");
    assert(Display(Domain::Item, "third-party/example", "Custom Weapon") == "Custom Weapon");
    assert(Display(Domain::Nav, "primary", "Primary") == u8"主武器");
    assert(Display(Domain::Field, "weapons|magazine.capacity|Magazine capacity", "Magazine capacity") == u8"弹匣容量");
    assert(Display(Domain::Field, "weapons|magazine.spare_magazines|Spare magazines", "Spare magazines") == u8"备用弹匣数");
    assert(Display(Domain::Field, "weapons|magazine.capacity|Capacity (new label)", "Capacity (new label)") == "Capacity (new label)");
    assert(Display(Domain::Option, "weapons|weapon.default_fire_mode|Automatic", "Automatic") == u8"全自动");
    assert(Display(Domain::Option, "weapons|weapon.default_fire_mode|Custom mode", "Custom mode") == "Custom mode");
    const char* smallAngle = "Armor penetration: slight angle";
    assert(Display(Domain::Field, "weapons|damage.ap_slight|Armor penetration: slight angle", smallAngle) == u8"小角度穿甲值");
    assert(Display(Domain::Field, "damage|ap_slight|AP slight", "AP slight") == u8"小角度穿甲值");
    assert(Display(Domain::Field, "statuses|damage.ap_slight|ap_slight", "ap_slight") == u8"小角度穿甲值");
    assert(Display(Domain::Field, "weapons|damage.standard_damage|Standard damage", "Standard damage") == u8"标准伤害");
    assert(Display(Domain::Field, "weapons|weapon.recoil_climb_vertical|Climb vertical recoil", "Climb vertical recoil") ==
        u8"垂直镜头后坐角速度");
    const char* ma5cOriginal = "MA5C Assault Rifle";
    const std::string ma5c = Display(Domain::Item, "weapons/MA5C Assault Rifle", ma5cOriginal);
    assert(ma5c == u8"MA5C 突击步枪");
    assert(Display(Domain::Item, "stratagems/Orbital Airburst Strike", "Orbital Airburst Strike") == u8"轨道空爆攻击");
    assert(Display(Domain::Item, "enemies/boomer_tier_2", "Bile Spewer - Boomer Tier 2") == u8"胆汁喷涌虫（2 阶）");
    const char* reticle = "Style 5 (5): MA5C Assault Rifle";
    assert(Display(Domain::Option, "weapons|weapon.third_person_reticle|Style 5 (5): MA5C Assault Rifle", reticle) ==
        u8"样式 5（5）：MA5C 突击步枪");
    assert(Display(Domain::Option, "reference|projectile|MA5C Assault Rifle", ma5cOriginal) == ma5c);
    assert(Display(Domain::Option, "reference|status|MA5C Assault Rifle", ma5cOriginal) == ma5cOriginal);
    const char* sharedProjectile = "projectile 276, also AR-23A Liberator Carbine, StA-52 Assault Rifle +1";
    assert(Display(Domain::Ui, sharedProjectile, sharedProjectile) ==
        u8"弹体 276，还用于 AR-23A “解放者”卡宾枪、StA-52 突击步枪，另有 1 种");
    assert(Display(Domain::Ui, "health/second", "health/second") == u8"生命值/秒");
    assert(Display(Domain::Ui, u8"°/s", u8"°/s") == u8"°/s");
    const char* noReticle = "No reticle (3): APW-1 Anti-Materiel Rifle";
    const char* pump = "Pump shotgun (6): SG-88 Break-Action Shotgun";
    assert(Display(Domain::Option, std::string("weapons|weapon.third_person_reticle|") + noReticle, noReticle) ==
        u8"无准星（3）：APW-1 反器材步枪");
    assert(Display(Domain::Option, std::string("weapons|weapon.third_person_reticle|") + pump, pump) ==
        u8"泵动式霰弹枪（6）：SG-88 中折式霰弹枪");
    assert(Display(Domain::Item, "enemies/soldier_mg", "Heavy Devastator - Soldier MG") == u8"重型蹂躏者");
    assert(Display(Domain::Item, "rosters/automatons/row_22", "Rocket Devastator - Soldier RPG") == u8"火箭蹂躏者");
    std::puts("PASS zh_cn: display and fallback");
}

static void VerifyReviewedTranslations()
{
    using zh_cn::Display;
    using zh_cn::Domain;

    assert(Display(Domain::Item, "enemies/ballistic_shield_backpack_mk2", "Backpack - Ballistic Shield Backpack Mk2") ==
        u8"防弹护盾背包 Mk2");
    assert(Display(Domain::Item, "enemies/ballistic_shield_backpack",
        "SH-20 Ballistic Shield Backpack - Ballistic Shield Backpack") == u8"SH-20 防弹护盾背包");
    const char* smallShield = "Ballistic Shield Backpack Small";
    assert(Display(Domain::Item, "enemies/ballistic_shield_backpack_small",
        "SH-20 Ballistic Shield Backpack - Ballistic Shield Backpack Small") == u8"SH-20 小型防弹护盾背包");
    assert(Display(Domain::Option, "pods|pod.slot_1|Ballistic Shield Backpack Small", smallShield) ==
        u8"SH-20 小型防弹护盾背包");
    assert(Display(Domain::Item, "enemies/generator", "Generator") == u8"发电机（仅尺寸）");
    assert(Display(Domain::Item, "enemies/power_generator", "Power Generator") == u8"发电机（含生命值与护甲）");

    assert(Display(Domain::Field, "vehicles|entity.armor|Armor", "Armor") == u8"护甲值");
    assert(Display(Domain::Field, "enemies|entity::entity.armor|Armor", "Armor") == u8"护甲值");
    assert(Display(Domain::Field, "backpacks|zone.armor|Zone armor", "Zone armor") == u8"护甲值");
    assert(Display(Domain::Field, "stratagems|zone.armor|Zone armor", "Zone armor") == u8"护甲值");
    assert(Display(Domain::Field, "weapons|entity.armor|Mount armor", "Mount armor") == u8"护甲值");
    assert(Display(Domain::Field, "damage|strength|Status applied per hit", "Status applied per hit") ==
        u8"每次命中施加的状态强度");

    const char* dragon = "Dragonroach - Dragon";
    const char* dragonGroup = "Group 1 · Dragonroach - Dragon";
    assert(Display(Domain::Item, "enemies/dragon", dragon) == u8"蟑龙");
    assert(Display(Domain::Item, "rosters/terminids/row_35", dragon) == u8"蟑龙");
    assert(Display(Domain::Ui, dragonGroup, dragonGroup) == u8"分组 1 · 蟑龙");

    const char* programmableAmmo = "Swap · Programmable ammo";
    const char* fullChargeExpiry = "Explosion · Full charge expiry";
    const char* fullChargeImpact = "Explosion · Full charge impact";
    const char* projectile = "projectile 36, also GR-8 Recoilless Rifle (programmable ammo)";
    assert(Display(Domain::Ui, programmableAmmo, programmableAmmo) == u8"更换 · 可编程弹药");
    assert(Display(Domain::Ui, fullChargeExpiry, fullChargeExpiry) == u8"爆炸 · 满蓄力到期");
    assert(Display(Domain::Ui, fullChargeImpact, fullChargeImpact) == u8"爆炸 · 满蓄力命中");
    assert(Display(Domain::Ui, projectile, projectile) == u8"弹体 36，还用于 GR-8 无后坐力炮（可编程弹药）");
    std::puts("PASS zh_cn: reviewed translations");
}

static void VerifyDiagnostics()
{
    using zh_cn::Diagnostic;
    const Diagnostic range{"Allowed range: {0} to {1}", "0", "255"};
    const Diagnostic status{"status {0} is not one a damage type can apply", "pure_damage"};
    const Diagnostic slot{"'{0}' is not one HellForge offers here", "{1}"};
    const Diagnostic named{"'{0}' is not a value HellForge knows here", "test-id"};
    const Diagnostic minimum{"Must be at least {0}.", "0"};
    const Diagnostic maximum{"Must be at most {0}.", "2047"};
    assert(range.Text() == u8"允许范围为 0 至 255");
    assert(minimum.Text() == u8"数值不得小于 0。");
    assert(maximum.Text() == u8"数值不得大于 2047。");
    assert(minimum.Text(false) == "Must be at least 0.");
    assert(maximum.Text(false) == "Must be at most 2047.");
    assert(std::string(zh_cn::Ui("enemies")) == u8"敌人");
    assert(std::string(zh_cn::Ui("vehicles")) == u8"载具");
    assert(status.Text() == u8"伤害类型无法施加状态“pure_damage”");
    assert(slot.Text().find("{1}") != std::string::npos);
    assert(named.Text() == u8"HellForge 无法识别值“test-id”");
    assert(status.Text(false) == "status pure_damage is not one a damage type can apply");
    zh_cn::SetFontReady(false);
    assert(range.Text() == "Allowed range: 0 to 255");
    std::puts("PASS zh_cn: formatted diagnostics and original values");
}

static void VerifyFont(const wchar_t* fontPath, const wchar_t* folder)
{
    ImGui::CreateContext();
    const std::string fontFile = hfjson::Utf8(fontPath);
    const char* path = fontFile.c_str();
    if (folder) ui::SetFontFolder(hfjson::Utf8(folder));
    const std::string missing = std::string(path) + ".hellforge-zh-cn-missing";
    assert(ui::BuildFonts(1.0f, missing.c_str()));
    assert(zh_cn::Display(zh_cn::Domain::Ui, "APPLY", "APPLY") == "APPLY");
    const auto& fonts = ImGui::GetIO().Fonts->Fonts;
    assert(fonts.Size > 1);
    for (ImFont* font : fonts) assert(font->FindGlyphNoFallback('A'));

    if (FILE* file = _wfopen(fontPath, L"rb"))
    {
        std::fclose(file);
        for (float scale : {0.90f, 1.0f, 1.50f})
        {
            assert(ui::BuildFonts(scale, path));
            assert(!ui::BuildFonts(scale, path));
            for (ImFont* font : fonts)
                for (ImWchar ch : {u'应', u'用', u'弹', u'匣', u'“', u'”', u'，'})
                    assert(font->FindGlyphNoFallback(ch));
            assert(zh_cn::Display(zh_cn::Domain::Ui, "APPLY", "APPLY") == u8"应用");
        }
        ui::Canvas canvas;
        const char* nickname = u8"王小猫\U00020164";
        canvas.Measure(nickname, 16);
        assert(ui::BuildFonts(1.50f, path));
        for (ImFont* font : fonts)
        {
            for (ImWchar ch : {u'王', u'小', u'猫'})
                assert(font->FindGlyphNoFallback(ch));
            const ImFontGlyph* extended = font->FindGlyphNoFallback((ImWchar)0x20164);
            assert(extended && extended->Codepoint == 0x20164);
        }
        canvas.Measure(nickname, 16);
        assert(!ui::BuildFonts(1.50f, path));
        assert(ui::BuildFonts(1.50f, missing.c_str()));
        assert(zh_cn::Display(zh_cn::Domain::Ui, "APPLY", "APPLY") == "APPLY");
        for (ImFont* font : fonts) assert(font->FindGlyphNoFallback('A'));
        std::puts("PASS zh_cn: YaHei at 90%, 100%, 150%; nickname U+20164; missing font fallback");
    }
    else std::puts("PASS zh_cn: missing font fallback; SKIP YaHei glyphs (font file unavailable)");
    ImGui::DestroyContext();
}

static void VerifyArmorSummaryLabels()
{
    const auto summaryLabel = editorapp::SummaryFieldLabel;
    catalog::Object exo, walker, tank;
    catalog::Row body, left, right, pod3, pod4;

    const auto addArmor = [](catalog::Object& object, catalog::Row& row, const char* group,
                             const char* category, const char* label) {
        row.object = &object;
        row.category = category;
        row.key = "entity.armor";
        row.label = label;
        auto& section = object.page.emplace_back();
        section.title = group;
        section.parts.emplace_back().rows.push_back(&row);
    };
    addArmor(exo, body, "Health & Armor", "vehicles", "Armor");
    body.ref = 0x79E4B3D2DA5E45E3ull;
    body.offset = 280;
    catalog::Row frontLeft = body, frontRight = body;
    frontLeft.key = "zone_0.armor";
    frontLeft.offset = 736;
    frontRight.key = "zone_1.armor";
    frontRight.offset = 1288;
    auto& zones = exo.page.emplace_back();
    zones.title = "Damage Zones";
    zones.parts = {
        {"Cockpit front left", {}, {}, false, {&frontLeft}},
        {"Cockpit front right", {}, {}, false, {&frontRight}},
    };

    catalog::Row walkerBody = body, walkerLeft = frontLeft, walkerRight = frontRight;
    addArmor(walker, walkerBody, "Health & Armor", "enemies", "Armor");
    walkerBody.key = "entity::entity.armor";
    walkerLeft.object = walkerRight.object = &walker;
    walkerLeft.category = walkerRight.category = "enemies";
    walkerLeft.key = "damage_zone:zone_0:zone.armor";
    walkerRight.key = "damage_zone:zone_1:zone.armor";
    auto& walkerZones = walker.page.emplace_back();
    walkerZones.title = "Damage Zones";
    walkerZones.parts = {
        {"Zone 0 (zone_0)", {}, {}, false, {&walkerLeft}},
        {"Zone 1 (zone_1)", {}, {}, false, {&walkerRight}},
    };

    addArmor(exo, left, u8"Weapon · Left gun", "weapons", "Mount armor");
    addArmor(exo, right, u8"Weapon · Right gun", "weapons", "Mount armor");
    addArmor(tank, pod3, u8"Weapon · Missile pods (slot 3)", "weapons", "Mount armor");
    addArmor(tank, pod4, u8"Weapon · Missile pods (slot 4)", "weapons", "Mount armor");
    pod3.mountSlot = pod4.mountSlot = "mount.slot_3";
    pod3.link = pod4.link = "component:HealthComponentData:108:280";

    assert(editorapp::FieldLabel(&body) == u8"护甲值");
    assert(editorapp::FieldLabel(&left) == u8"护甲值");
    assert(summaryLabel(&body) == u8"护甲值");
    assert(editorapp::FieldLabel(&frontLeft) == u8"护甲值");
    assert(summaryLabel(&frontLeft) == u8"驾驶舱左前侧 · 护甲值");
    assert(summaryLabel(&frontRight) == u8"驾驶舱右前侧 · 护甲值");
    assert(summaryLabel(&walkerBody) == u8"护甲值");
    assert(editorapp::FieldLabel(&walkerLeft) == editorapp::FieldLabel(&frontLeft));
    assert(summaryLabel(&walkerLeft) == u8"区域 0（zone_0） · 护甲值");
    assert(summaryLabel(&walkerRight) == u8"区域 1（zone_1） · 护甲值");
    assert(summaryLabel(&left) == u8"武器 · 左侧武器 · 护甲值");
    assert(summaryLabel(&right) == u8"武器 · 右侧武器 · 护甲值");
    assert(summaryLabel(&pod3) == u8"武器 · 导弹发射舱 · 槽位 3 · 护甲值");
    assert(summaryLabel(&pod4) == u8"武器 · 导弹发射舱 · 槽位 4 · 护甲值");

    zh_cn::SetFontReady(false);
    assert(summaryLabel(&body) == "Armor");
    assert(summaryLabel(&frontLeft) == u8"Cockpit front left · Armor");
    assert(summaryLabel(&frontRight) == u8"Cockpit front right · Armor");
    assert(summaryLabel(&walkerLeft) == u8"Zone 0 (zone_0) · Armor");
    assert(summaryLabel(&walkerRight) == u8"Zone 1 (zone_1) · Armor");
    assert(summaryLabel(&left) == u8"Weapon · Left gun · Mount armor");
    zh_cn::SetFontReady(true);
    std::puts("PASS zh_cn: body, vehicle and entity damage zones, and mount armor summaries");
}

static void VerifyComponentArmorLabels()
{
    using editorapp::FieldLabel;
    using editorapp::SummaryFieldLabel;

    catalog::Object silo, harvester;
    catalog::Row rack;
    rack.object = &silo;
    rack.category = "enemies";
    rack.label = "Armor";
    rack.key = "entity::entity.armor";
    rack.component = "HealthComponentData";
    rack.ref = 0xDE18775FA447A9BFull;
    rack.offset = 280;
    catalog::Row missile = rack;
    missile.ref = 0xDDDB2910FF2B24E9ull;
    silo.page = {{"Health & Armor", {}, {}, false, false, {
        {u8"Rack · Health & Armor", {}, {}, false, {&rack}},
        {u8"Missile · Health & Armor", {}, {}, false, {&missile}},
    }}};

    catalog::Row body = rack;
    body.object = &harvester;
    body.ref = 0x965EAE5A51ACDD4Aull;
    catalog::Row shield = body;
    shield.key = "entity::shield.armor";
    shield.component = "ShieldComponentData";
    shield.offset = 80;
    harvester.page = {
        {"Health & Armor", {}, {}, false, false, {{"Health & Armor", {}, {}, false, {&body}}}},
        {"Shield", {}, {}, false, false, {{"Shield", {}, {}, false, {&shield}}}},
    };

    assert(FieldLabel(&rack) == u8"护甲值");
    assert(FieldLabel(&shield) == u8"护甲值");
    assert(SummaryFieldLabel(&rack) == u8"挂架 · 护甲值");
    assert(SummaryFieldLabel(&missile) == u8"导弹 · 护甲值");
    assert(SummaryFieldLabel(&body) == u8"护甲值");
    assert(SummaryFieldLabel(&shield) == u8"护盾 · 护甲值");

    zh_cn::SetFontReady(false);
    assert(SummaryFieldLabel(&rack) == u8"Rack · Armor");
    assert(SummaryFieldLabel(&missile) == u8"Missile · Armor");
    assert(SummaryFieldLabel(&body) == "Armor");
    assert(SummaryFieldLabel(&shield) == u8"Shield · Armor");
    zh_cn::SetFontReady(true);
    std::puts("PASS zh_cn: rack, missile, body and shield armor summaries");
}

static void VerifyCrossObjectArmorLabels()
{
    using editorapp::ObjectName;
    using editorapp::SummaryFieldLabel;

    const auto makeObject = [](const char* id, const char* name) {
        catalog::Object result;
        result.id = id;
        result.name = name;
        return result;
    };
    auto equipment = makeObject("stratagems/SH-20 Ballistic Shield Backpack", "SH-20 Ballistic Shield Backpack");
    auto entity = makeObject("enemies/ballistic_shield_backpack", "SH-20 Ballistic Shield Backpack - Ballistic Shield Backpack");
    auto tripod = makeObject("enemies/tripod", "Harvester - Tripod");
    auto structure = makeObject("enemies/harvester", "Harvester");
    auto landingBeacon = makeObject("enemies/content/fac_helldivers/hellpod/landingzone_beacon/sos_beacon", "Sos Beacon");
    auto podBeacon = makeObject("enemies/content/fac_helldivers/hellpod/sos_beacon/sos_beacon", "Sos Beacon");

    catalog::Row zone;
    zone.object = &equipment;
    zone.category = "backpacks";
    zone.key = "zone.armor";
    zone.label = "Zone armor";
    equipment.page = {{"Health & Armor", {}, {}, false, false, {{"Damage zones", {}, {}, false, {&zone}}}}};
    catalog::Row body;
    body.object = &entity;
    body.category = "enemies";
    body.key = "entity::entity.armor";
    body.label = "Armor";

    assert(ObjectName(&equipment) == ObjectName(&entity));
    assert(SummaryFieldLabel(&zone) == u8"伤害部位 · 护甲值");
    assert(SummaryFieldLabel(&body) == u8"护甲值");
    assert(ObjectName(&tripod) == u8"猎杀器");
    assert(ObjectName(&structure) == u8"猎杀器（构筑物）");
    assert(ObjectName(&landingBeacon) == u8"S.O.S.求救信标（着陆区）");
    assert(ObjectName(&podBeacon) == u8"S.O.S.求救信标（绝地喷射舱）");

    zh_cn::SetFontReady(false);
    assert(SummaryFieldLabel(&zone) == u8"Damage zones · Zone armor");
    assert(SummaryFieldLabel(&body) == "Armor");
    assert(ObjectName(&tripod) == "Harvester - Tripod");
    assert(ObjectName(&structure) == "Harvester");
    assert(ObjectName(&landingBeacon) == "Sos Beacon (Landing Zone)");
    assert(ObjectName(&podBeacon) == "Sos Beacon (Hellpod)");
    zh_cn::SetFontReady(true);
    std::puts("PASS zh_cn: cross-object armor identities and SOS fallback names");
}

int wmain(int argc, wchar_t** argv)
{
    VerifyDisplay();
    VerifyReviewedTranslations();
    VerifyArmorSummaryLabels();
    VerifyComponentArmorLabels();
    VerifyCrossObjectArmorLabels();
    VerifyDiagnostics();
    assert(argc >= 2);
    VerifyFont(argv[1], argc >= 3 ? argv[2] : nullptr);
}
