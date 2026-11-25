// SPDX-License-Identifier: GPL-3.0-or-later
#include "builders.hpp"
#include "gamedata.hpp"
#include "test.hpp"

#include "nagus/data/cursor.hpp"
#include "nagus/data/font.hpp"
#include "nagus/data/image.hpp"
#include "nagus/data/layout.hpp"
#include "nagus/data/lexicon.hpp"
#include "nagus/data/resources.hpp"
#include "nagus/data/tables.hpp"
#include "nagus/data/zip_archive.hpp"

using namespace nagus;
using namespace nagus::test;

namespace {

Bytes tgaHeader(int w, int h, int bits, std::uint8_t descriptor) {
    Bytes b{0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    put16(b, static_cast<std::uint32_t>(w));
    put16(b, static_cast<std::uint32_t>(h));
    b.push_back(static_cast<std::uint8_t>(bits));
    b.push_back(descriptor);
    return b;
}

void putName(Bytes& b, const std::string& name) {
    std::string padded = name;
    padded.resize(13, ' ');
    putText(b, padded);
}

} // namespace

NGS_TEST("16-bit Targa pictures, bottom row first, with see-through magenta") {
    Bytes b = tgaHeader(2, 2, 16, 0);
    put16(b, 0x7c00); // bottom left: red
    put16(b, 0x7c1f); // bottom right: see-through
    put16(b, 0x001f); // top left: blue
    put16(b, 0x03e0); // top right: green
    const Image img = readTga(b);
    NGS_CHECK(img.width == 2 && img.height == 2);
    NGS_CHECK(img.at(0, 0) == rgb(0, 0, 255));
    NGS_CHECK(img.at(1, 0) == rgb(0, 255, 0));
    NGS_CHECK(img.at(0, 1) == rgb(255, 0, 0));
    NGS_CHECK(img.at(1, 1) == 0u);
}

NGS_TEST("Targa reader refuses short and odd pictures") {
    for (const Bytes& bad : {tgaHeader(4, 4, 16, 0), tgaHeader(0, 4, 16, 0), tgaHeader(1, 1, 8, 0), Bytes{1, 2, 3}}) {
        bool threw = false;
        try {
            readTga(bad);
        } catch (const FormatError&) {
            threw = true;
        }
        NGS_CHECK(threw);
    }
}

NGS_TEST("screen layouts: background, buttons and text fields") {
    Bytes b;
    put32(b, 2);
    put32(b, 1);
    put32(b, 0x8a);
    for (const int v : {0, 0, 800, 600}) {
        put32(b, static_cast<std::uint32_t>(v));
    }
    putName(b, "back.tga");
    b.resize(105, 0);
    // A button.
    put32(b, 2);
    put32(b, 0x8f);
    for (const int v : {621, 476, 164, 40}) {
        put32(b, static_cast<std::uint32_t>(v));
    }
    putName(b, "quit1.tga");
    put32(b, 0xffffffffu);
    b.insert(b.end(), 9, 0);
    putName(b, "quit1.tga");
    putName(b, "quit2.tga");
    // A text field.
    const std::size_t text = b.size();
    put32(b, 14);
    put32(b, 0x23f8);
    for (const int v : {361, 183, 91, 17}) {
        put32(b, static_cast<std::uint32_t>(v));
    }
    putName(b, "none");
    b.resize(text + 53, 0);
    b.insert(b.end(), {0x19, 0xfa, 0xbe, 0});
    b.resize(text + 84, 0);
    put32(b, 1);
    putName(b, "Human15.fnt");
    const Layout l = readLayout(b);
    NGS_CHECK(l.background == "back.tga");
    NGS_CHECK(l.rect.w == 800 && l.rect.h == 600);
    NGS_CHECK(l.elements.size() == 2);
    const auto* button = l.find(0x8f);
    NGS_CHECK(button != nullptr && button->kind == LayoutElement::Kind::Button);
    NGS_CHECK(button->rect.x == 621 && button->rect.h == 40);
    NGS_CHECK(button->upImage == "quit1.tga" && button->downImage == "quit2.tga");
    const auto* field = l.find(0x23f8);
    NGS_CHECK(field != nullptr && field->kind == LayoutElement::Kind::Text);
    NGS_CHECK(field->font == "Human15.fnt");
    NGS_CHECK(field->colour == rgb(0x19, 0xfa, 0xbe) && field->ownColour);
}

NGS_TEST("lexicon texts by number") {
    const Lexicon lex(makeLexicon({"Accept", "Cancel", "Galaxy"}));
    NGS_CHECK(lex.size() == 3);
    NGS_CHECK(lex[1] == "Cancel");
    NGS_CHECK(lex[7].empty());
}

NGS_TEST("all layouts, fonts, pointers and texts of the original read") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    for (const char* name : {"opening.wdf", "gmesetup.wdf", "emppick.wdf", "options.wdf", "movie.wdf"}) {
        const Layout l = readLayout(res.file(name));
        NGS_CHECK(!l.elements.empty());
        NGS_CHECK(!l.background.empty());
    }
    const auto layouts = res.names(".wdf");
    NGS_CHECK(layouts.size() == 377);
    for (const auto& name : layouts) {
        readLayout(res.file(name));
    }
    const Layout opening = readLayout(res.file("opening.wdf"));
    NGS_CHECK(opening.elements.size() == 9);
    NGS_CHECK(readTga(res.file(opening.background)).width == 800);
    for (const char* face : {"human", "cardsn", "ferngi", "klingn", "romuln"}) {
        for (const char* size : {"9", "15", "20", "28"}) {
            const Font f(res.file(std::string(face) + size + ".fnt"));
            NGS_CHECK(f.height() > 5);
            NGS_CHECK(f.glyph('A') != nullptr && f.glyph('z') != nullptr && f.glyph('?') != nullptr);
            NGS_CHECK(f.width("Nagus") > 10);
        }
    }
    const Cursor c = readCursor(res.file("generic.cur"));
    NGS_CHECK(c.frames.size() == 1 && c.frames[0].width == 17);
    NGS_CHECK(readCursor(res.file("target.cur")).frames.size() > 1);
    const Lexicon lex(res.file("lexicon.dic"));
    NGS_CHECK(lex.size() == 1460);
    NGS_CHECK(lex[text::kAccept] == "Accept");
    NGS_CHECK(lex[text::kCancel] == "Cancel");
}

NGS_TEST("tables of races, stars, home systems and technologies") {
    const auto lib = gameLibrary();
    if (!lib) {
        NGS_SKIP("no game files in gamedata/");
    }
    const Resources res(*lib);
    const auto races = readRaces(res.file("race.rst"));
    NGS_CHECK(races.size() == 35);
    NGS_CHECK(races[0].name == "Cardassians" && races[4].name == "Romulans");
    NGS_CHECK(races[0].picture == "cardn_02.tga");
    const auto kinds = readStarObjectKinds(res.file("objstruc.smt"));
    NGS_CHECK(kinds.size() == 12);
    float total = 0;
    for (const auto& k : kinds) {
        total += k.share;
    }
    NGS_CHECK(total > 0.99f && total < 1.01f);
    NGS_CHECK(kinds[6].code == "s-y-" && kinds[3].code == "ns");
    NGS_CHECK(res.has(kinds[6].code + "map.tga"));
    const auto names = readStarNames(res.file("starname.bin"));
    NGS_CHECK(names.size() == 297 && names[0] == "Exo");
    const auto home = readHomePlanets(res.file("meplanet.bin"));
    NGS_CHECK(home.size() == 30 && home[8].name == "Earth" && home[8].race == "Federation");
    const auto techs = readTechTree(res.file("techtree.tec"));
    NGS_CHECK(techs.size() == 77);
    NGS_CHECK(techs[2].name == "Bionics" && techs[2].level == 2 && techs[2].cost == 200);
    NGS_CHECK(techs[65].field == TechField::Weapons && techs[65].level == 10);
}

NGS_TEST("table readers refuse damaged tables") {
    for (const Bytes& bad : {Bytes{}, Bytes{1}, Bytes{0xff, 0xff, 0, 0}}) {
        bool threw = false;
        try {
            readRaces(bad);
            readStarNames(bad);
        } catch (const FormatError&) {
            threw = true;
        }
        NGS_CHECK(threw);
    }
    Bytes tech(72, 0);
    tech[0x37] = 9; // no such field
    bool threw = false;
    try {
        readTechTree(tech);
    } catch (const FormatError&) {
        threw = true;
    }
    NGS_CHECK(threw);
}
