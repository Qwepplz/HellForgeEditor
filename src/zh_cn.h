#pragma once

#include <string>
#include <string_view>
#include <utility>

struct ImFontGlyphRangesBuilder;

namespace zh_cn
{
enum class Domain { Ui, Nav, Item, Field, Option };

std::string Display(Domain domain, std::string_view key, std::string_view original);
const char* Ui(const char* original);

class Diagnostic
{
public:
    Diagnostic(const char* message = "", std::string first = {}, std::string second = {})
        : message(message), first(std::move(first)), second(std::move(second)) {}

    std::string Text(bool localized = true) const;

private:
    const char* message;
    std::string first, second;
};

void SetFontReady(bool ready);
void AddGlyphs(ImFontGlyphRangesBuilder& builder);
}
