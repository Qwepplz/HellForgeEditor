#include "../canvas.h"
#include "../json.h"
#include "../zh_cn.h"
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
    const char* noReticle = "No reticle (3): APW-1 Anti-Materiel Rifle";
    const char* pump = "Pump shotgun (6): SG-88 Break-Action Shotgun";
    assert(Display(Domain::Option, std::string("weapons|weapon.third_person_reticle|") + noReticle, noReticle).find(u8"无准星（3）：APW-1") == 0);
    assert(Display(Domain::Option, std::string("weapons|weapon.third_person_reticle|") + pump, pump).find(u8"泵动式霰弹枪（6）：SG-88") == 0);
    assert(Display(Domain::Item, "enemies/soldier_mg", "Heavy Devastator - Soldier MG") == u8"重型蹂躏者");
    assert(Display(Domain::Item, "rosters/automatons/row_22", "Rocket Devastator - Soldier RPG") == u8"火箭炮蹂躏者");
    std::puts("PASS zh_cn: display and fallback");
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

int wmain(int argc, wchar_t** argv)
{
    VerifyDisplay();
    VerifyDiagnostics();
    assert(argc >= 2);
    VerifyFont(argv[1], argc >= 3 ? argv[2] : nullptr);
}
