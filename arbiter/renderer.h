#pragma once

#include <shared.h>
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <cstdio>
#include <cstring>
#include <string>
#include <pthread.h>
#include <unistd.h>
using namespace sf;
using namespace std;

struct RenderContext
{
    SharedState* state;
    int windowWidth;
    int windowHeight;
    Font fontRegular;
    Font fontBold;
    int running;
};

struct ButtonRect
{
    float x;
    float y;
    float w;
    float h;
    int id;
};

struct EnemyRect
{
    float x;
    float y;
    float w;
    float h;
    int enemyId;
};

static RenderContext g_render_ctx;
static ButtonRect g_action_buttons[10];
static int g_action_button_count = 0;
static EnemyRect g_enemy_rects[9];
static int g_enemy_rect_count = 0;
static ButtonRect g_weapon_buttons[20];
static int g_weapon_button_count = 0;
static ButtonRect g_lts_buttons[20];
static int g_lts_button_count = 0;
static ButtonRect g_artifact_buttons[3];
static int g_artifact_button_count = 0;
static ButtonRect g_drop_buttons[2];
static int g_local_gui_phase = 0;
static int g_local_action_chosen = -1;
static int g_local_target_chosen = -1;
static int g_local_weapon_chosen = -1;
static int g_local_lts_chosen = -1;
static int g_local_artifact_chosen = -1;
static int g_local_selected_enemy = -1;
static int g_show_inventory_overlay = 0;
static Texture g_charizard_tex;
static Texture g_peppino_tex;
static int g_textures_loaded = 0;

int load_fonts()
{
    if (!g_render_ctx.fontRegular.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"))
    {
        return -1;
    }
    if (!g_render_ctx.fontBold.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"))
    {
        return -1;
    }
    return 0;
}

int load_textures()
{
    if (!g_charizard_tex.loadFromFile("assets/charizard/charizard_00.png"))
    {
        return -1;
    }
    if (!g_peppino_tex.loadFromFile("assets/peppino/idle_00.png"))
    {
        return -1;
    }
    g_textures_loaded = 1;
    return 0;
}

Color color_from_rgb(int r, int g, int b)
{
    return Color(r, g, b);
}

Color color_from_rgba(int r, int g, int b, int a)
{
    return Color(r, g, b, a);
}

void draw_rect(RenderWindow* window, float x, float y, float w, float h, Color fillColor)
{
    RectangleShape rect(Vector2f(w, h));
    rect.setPosition(x, y);
    rect.setFillColor(fillColor);
    window->draw(rect);
}

void draw_rect_outline(RenderWindow* window, float x, float y, float w, float h,
                       Color fillColor, Color outlineColor, float thickness)
{
    RectangleShape rect(Vector2f(w, h));
    rect.setPosition(x, y);
    rect.setFillColor(fillColor);
    rect.setOutlineColor(outlineColor);
    rect.setOutlineThickness(thickness);
    window->draw(rect);
}

void draw_text(RenderWindow* window, const string& str, float x, float y,
               int fontSize, Color color, Font& font)
{
    Text text;
    text.setFont(font);
    text.setString(str);
    text.setCharacterSize(fontSize);
    text.setFillColor(color);
    text.setPosition(x, y);
    window->draw(text);
}

void draw_bar(RenderWindow* window, float x, float y, float w, float h,
              int current, int maximum, Color barColor, Color bgColor)
{
    draw_rect(window, x, y, w, h, bgColor);
    if (maximum > 0 && current > 0)
    {
        float fillRatio = (float)current / (float)maximum;
        if (fillRatio > 1.0f)
        {
            fillRatio = 1.0f;
        }
        float fillWidth = w * fillRatio;
        draw_rect(window, x, y, fillWidth, h, barColor);
    }
    draw_rect_outline(window, x, y, w, h, Color::Transparent,
                      color_from_rgba(200, 200, 200, 100), 1.0f);
}

string get_weapon_name_str(int weaponId)
{
    if (weaponId == 0)
    {
        return "Solar Core";
    }
    if (weaponId == 1)
    {
        return "Lunar Blade";
    }
    if (weaponId == 2)
    {
        return "Iron Halberd";
    }
    if (weaponId == 3)
    {
        return "Venom Dagger";
    }
    if (weaponId == 4)
    {
        return "Thunderstaff";
    }
    if (weaponId == 5)
    {
        return "Obsidian Axe";
    }
    if (weaponId == 6)
    {
        return "Frostbow";
    }
    if (weaponId == 7)
    {
        return "Splinter Stick";
    }
    return "None";
}

string get_artifact_name_str(int artifactId)
{
    if (artifactId == 0)
    {
        return "Solar Core";
    }
    if (artifactId == 1)
    {
        return "Lunar Blade";
    }
    if (artifactId == 2)
    {
        return "Eclipse Relic";
    }
    return "None";
}

Color get_weapon_color(int weaponId)
{
    if (weaponId == 0)
    {
        return color_from_rgb(255, 200, 50);
    }
    if (weaponId == 1)
    {
        return color_from_rgb(150, 180, 255);
    }
    if (weaponId == 2)
    {
        return color_from_rgb(180, 180, 180);
    }
    if (weaponId == 3)
    {
        return color_from_rgb(100, 200, 80);
    }
    if (weaponId == 4)
    {
        return color_from_rgb(255, 255, 100);
    }
    if (weaponId == 5)
    {
        return color_from_rgb(80, 80, 80);
    }
    if (weaponId == 6)
    {
        return color_from_rgb(100, 200, 255);
    }
    if (weaponId == 7)
    {
        return color_from_rgb(160, 120, 80);
    }
    return color_from_rgb(60, 60, 60);
}

void draw_top_bar(RenderWindow* window, SharedState* state)
{
    draw_rect(window, 0, 0, 1280, 50, color_from_rgb(20, 20, 30));

    draw_text(window, "CHRONO RIFT", 20, 8, 24, color_from_rgb(220, 180, 50), g_render_ctx.fontBold);

    string killStr = "Kills: " + to_string(state->Status.EnemiesKilled) + " / 10";
    draw_text(window, killStr, 550, 14, 18, color_from_rgb(200, 200, 200), g_render_ctx.fontRegular);

    string turnStr = "";
    if (state->Status.GameOver)
    {
        if (state->Status.PlayerWon)
        {
            turnStr = "VICTORY!";
            draw_text(window, turnStr, 900, 8, 24, color_from_rgb(50, 220, 50), g_render_ctx.fontBold);
        }
        else
        {
            turnStr = "DEFEAT";
            draw_text(window, turnStr, 900, 8, 24, color_from_rgb(220, 50, 50), g_render_ctx.fontBold);
        }
    }
    else
    {
        if (state->Turn.ActiveEntityId >= 0)
        {
            if (state->Turn.IsPlayerTurn)
            {
                turnStr = "Player " + to_string(state->Turn.ActiveEntityId) + "'s Turn";
                draw_text(window, turnStr, 900, 14, 18, color_from_rgb(100, 200, 255), g_render_ctx.fontBold);
            }
            else
            {
                turnStr = "Enemy " + to_string(state->Turn.ActiveEntityId) + "'s Turn";
                draw_text(window, turnStr, 900, 14, 18, color_from_rgb(255, 120, 100), g_render_ctx.fontBold);
            }
        }
        else
        {
            turnStr = "Waiting...";
            draw_text(window, turnStr, 900, 14, 18, color_from_rgb(150, 150, 150), g_render_ctx.fontRegular);
        }
    }

    if (state->Status.EclipseRelicIntroduced)
    {
        draw_text(window, "Eclipse Relic Active", 700, 14, 14,
                  color_from_rgb(200, 100, 255), g_render_ctx.fontRegular);
    }
}

void draw_player_card(RenderWindow* window, SharedState* state, int playerId, float x, float y)
{
    float cardW = 280;
    float cardH = 140;

    Color cardBg = color_from_rgb(30, 35, 50);
    Color borderColor = color_from_rgb(60, 70, 100);

    int isActive = 0;
    if (state->Turn.IsPlayerTurn && state->Turn.ActiveEntityId == playerId)
    {
        isActive = 1;
        borderColor = color_from_rgb(100, 200, 255);
    }

    if (!state->Players[playerId].Alive)
    {
        cardBg = color_from_rgb(40, 20, 20);
        borderColor = color_from_rgb(100, 30, 30);
    }

    draw_rect_outline(window, x, y, cardW, cardH, cardBg, borderColor, 2.0f);

    string nameStr = "Player " + to_string(playerId);
    if (!state->Players[playerId].Alive)
    {
        nameStr = nameStr + " [DEAD]";
    }
    else if (state->Players[playerId].Stunned)
    {
        nameStr = nameStr + " [STUNNED]";
    }

    Color nameColor = color_from_rgb(200, 200, 220);
    if (isActive)
    {
        nameColor = color_from_rgb(100, 220, 255);
    }
    draw_text(window, nameStr, x + 8, y + 4, 14, nameColor, g_render_ctx.fontBold);

    string hpStr = "HP: " + to_string(state->Players[playerId].Hp)
                        + "/" + to_string(state->Players[playerId].MaxHp);
    draw_text(window, hpStr, x + 8, y + 24, 11, color_from_rgb(180, 180, 180), g_render_ctx.fontRegular);
    draw_bar(window, x + 8, y + 40, 180, 12,
             state->Players[playerId].Hp, state->Players[playerId].MaxHp,
             color_from_rgb(50, 200, 80), color_from_rgb(60, 30, 30));

    string stamStr = "Stamina: " + to_string(state->Players[playerId].Stamina)
                          + "/" + to_string(state->Players[playerId].MaxStamina);
    draw_text(window, stamStr, x + 8, y + 56, 11, color_from_rgb(180, 180, 180), g_render_ctx.fontRegular);
    draw_bar(window, x + 8, y + 72, 180, 12,
             state->Players[playerId].Stamina, state->Players[playerId].MaxStamina,
             color_from_rgb(50, 150, 255), color_from_rgb(30, 30, 60));

    string dmgStr = "DMG:" + to_string(state->Players[playerId].Damage)
                         + " SPD:" + to_string(state->Players[playerId].Speed);
    draw_text(window, dmgStr, x + 200, y + 24, 11, color_from_rgb(160, 160, 160), g_render_ctx.fontRegular);

    if (state->Players[playerId].HoldsArtifact >= 0)
    {
        string artStr = "Artifact: " + get_artifact_name_str(state->Players[playerId].HoldsArtifact);
        draw_text(window, artStr, x + 8, y + 90, 11, color_from_rgb(200, 150, 255), g_render_ctx.fontRegular);
    }

    float invX = x + 8;
    float invY = y + 108;
    float slotW = 13;
    float slotH = 14;
    for (int s = 0; s < 20; s++)
    {
        Color slotColor = color_from_rgb(40, 40, 50);
        if (state->Inventory[playerId][s].WeaponId != -1)
        {
            slotColor = get_weapon_color(state->Inventory[playerId][s].WeaponId);
        }
        draw_rect_outline(window, invX + s * (slotW + 1), invY, slotW, slotH,
                          slotColor, color_from_rgb(80, 80, 90), 1.0f);
    }

    int ltsCount = state->Lts[playerId].Count;
    if (ltsCount > 0)
    {
        string ltsStr = "LTS:" + to_string(ltsCount);
        draw_text(window, ltsStr, x + 200, y + 108, 10, color_from_rgb(140, 140, 160), g_render_ctx.fontRegular);
    }
}

void draw_enemy_card(RenderWindow* window, SharedState* state, int enemyId, float x, float y,
                     int isSelectable, int isHighlighted)
{
    float cardW = 135;
    float cardH = 100;

    Color cardBg = color_from_rgb(45, 30, 30);
    Color borderColor = color_from_rgb(100, 60, 60);

    int isActive = 0;
    if (!state->Turn.IsPlayerTurn && state->Turn.ActiveEntityId == enemyId)
    {
        isActive = 1;
        borderColor = color_from_rgb(255, 120, 80);
    }

    if (!state->Enemies[enemyId].Alive)
    {
        cardBg = color_from_rgb(25, 25, 25);
        borderColor = color_from_rgb(50, 50, 50);
    }
    else if (isHighlighted)
    {
        borderColor = color_from_rgb(255, 255, 100);
        cardBg = color_from_rgb(60, 50, 30);
    }
    else if (isSelectable)
    {
        borderColor = color_from_rgb(200, 200, 80);
    }

    draw_rect_outline(window, x, y, cardW, cardH, cardBg, borderColor, 2.0f);

    string nameStr = "E" + to_string(enemyId);
    if (!state->Enemies[enemyId].Alive)
    {
        nameStr = nameStr + " [X]";
    }
    else if (state->Enemies[enemyId].Stunned)
    {
        nameStr = nameStr + " [STUN]";
    }

    Color nameColor = color_from_rgb(220, 150, 150);
    if (isActive)
    {
        nameColor = color_from_rgb(255, 180, 100);
    }
    if (isHighlighted)
    {
        nameColor = color_from_rgb(255, 255, 150);
    }
    draw_text(window, nameStr, x + 5, y + 3, 12, nameColor, g_render_ctx.fontBold);

    if (isSelectable && state->Enemies[enemyId].Alive)
    {
        string keyStr = "[" + to_string(enemyId) + "]";
        draw_text(window, keyStr, x + 100, y + 3, 11, color_from_rgb(255, 255, 100), g_render_ctx.fontBold);
    }

    draw_bar(window, x + 5, y + 22, 100, 10,
             state->Enemies[enemyId].Hp, state->Enemies[enemyId].MaxHp,
             color_from_rgb(220, 50, 50), color_from_rgb(60, 30, 30));
    string hpStr = to_string(state->Enemies[enemyId].Hp);
    draw_text(window, hpStr, x + 108, y + 19, 10, color_from_rgb(180, 180, 180), g_render_ctx.fontRegular);

    draw_bar(window, x + 5, y + 38, 100, 10,
             state->Enemies[enemyId].Stamina, state->Enemies[enemyId].MaxStamina,
             color_from_rgb(200, 150, 50), color_from_rgb(30, 30, 40));
    string stamStr = to_string(state->Enemies[enemyId].Stamina);
    draw_text(window, stamStr, x + 108, y + 35, 10, color_from_rgb(180, 180, 180), g_render_ctx.fontRegular);

    string statStr = "D:" + to_string(state->Enemies[enemyId].Damage)
                          + " S:" + to_string(state->Enemies[enemyId].Speed);
    draw_text(window, statStr, x + 5, y + 54, 10, color_from_rgb(140, 140, 140), g_render_ctx.fontRegular);

    if (state->Enemies[enemyId].HasWeapon)
    {
        string wpnStr = get_weapon_name_str(state->Enemies[enemyId].WeaponId);
        draw_text(window, wpnStr, x + 5, y + 68, 10, color_from_rgb(200, 170, 100), g_render_ctx.fontRegular);
    }

    if (state->Enemies[enemyId].HoldsArtifact >= 0)
    {
        draw_text(window, get_artifact_name_str(state->Enemies[enemyId].HoldsArtifact),
                  x + 5, y + 82, 9, color_from_rgb(200, 100, 255), g_render_ctx.fontRegular);
    }
}

void draw_player_battleground(RenderWindow* window, SharedState* state)
{
    draw_rect(window, 0, 50, 500, 370, color_from_rgb(18, 20, 28));

    draw_text(window, "PLAYERS", 10, 55, 14, color_from_rgb(100, 200, 255), g_render_ctx.fontBold);

    int numPlayers = state->Status.NumPlayers;
    float spriteScale = 2.8f;
    float sprW = g_charizard_tex.getSize().x * spriteScale;
    float sprH = g_charizard_tex.getSize().y * spriteScale;

    float zigzagOffset = 70.0f;
    float startY = 78.0f;
    float spacing = 80.0f;
    if (numPlayers <= 2)
    {
        spacing = 120.0f;
    }
    else if (numPlayers == 3)
    {
        spacing = 90.0f;
    }

    float barW = sprW + 20;
    float barH = 5;

    for (int i = 0; i < numPlayers; i++)
    {
        float baseX = 30.0f;
        if (i % 2 == 1)
        {
            baseX = baseX + zigzagOffset;
        }
        float posY = startY + i * spacing;

        int isActive = 0;
        if (state->Turn.IsPlayerTurn && state->Turn.ActiveEntityId == i)
        {
            isActive = 1;
        }

        if (g_textures_loaded)
        {
            if (isActive)
            {
                draw_rect_outline(window, baseX - 3, posY - 3,
                    sprW + 6, sprH + 6,
                    color_from_rgba(40, 60, 90, 100), color_from_rgb(100, 200, 255), 2.0f);
            }

            Sprite spr(g_charizard_tex);
            spr.setScale(spriteScale, spriteScale);
            spr.setPosition(baseX, posY);

            if (!state->Players[i].Alive)
            {
                spr.setColor(color_from_rgba(100, 50, 50, 120));
            }
            else if (state->Players[i].Stunned)
            {
                spr.setColor(color_from_rgba(200, 200, 50, 200));
            }

            window->draw(spr);
        }

        float infoX = baseX + sprW + 8;

        string nameStr = "P" + to_string(i);
        if (!state->Players[i].Alive)
        {
            nameStr = nameStr + " [DEAD]";
        }
        else if (state->Players[i].Stunned)
        {
            nameStr = nameStr + " [STUN]";
        }
        Color nameColor = color_from_rgb(200, 200, 220);
        if (isActive)
        {
            nameColor = color_from_rgb(100, 220, 255);
        }
        draw_text(window, nameStr, infoX, posY, 11, nameColor, g_render_ctx.fontBold);

        draw_bar(window, infoX, posY + 16, barW, barH,
                 state->Players[i].Hp, state->Players[i].MaxHp,
                 color_from_rgb(50, 200, 80), color_from_rgb(60, 30, 30));

        string hpStr = to_string(state->Players[i].Hp) + "/" + to_string(state->Players[i].MaxHp);
        draw_text(window, hpStr, infoX + barW + 5, posY + 14, 9,
                  color_from_rgb(150, 200, 150), g_render_ctx.fontRegular);

        draw_bar(window, infoX, posY + 26, barW, barH,
                 state->Players[i].Stamina, state->Players[i].MaxStamina,
                 color_from_rgb(50, 150, 255), color_from_rgb(30, 30, 60));

        string stamStr = to_string(state->Players[i].Stamina) + "/" + to_string(state->Players[i].MaxStamina);
        draw_text(window, stamStr, infoX + barW + 5, posY + 24, 9,
                  color_from_rgb(100, 150, 255), g_render_ctx.fontRegular);

        string dmgStr = "D:" + to_string(state->Players[i].Damage) + " S:" + to_string(state->Players[i].Speed);
        draw_text(window, dmgStr, infoX, posY + 36, 9,
                  color_from_rgb(140, 140, 140), g_render_ctx.fontRegular);

        if (state->Players[i].HoldsArtifact >= 0)
        {
            string artStr = get_artifact_name_str(state->Players[i].HoldsArtifact);
            draw_text(window, artStr, infoX, posY + 48, 8,
                      color_from_rgb(200, 150, 255), g_render_ctx.fontRegular);
        }

        float invY = posY + 58;
        float slotW = 8;
        float slotH = 7;
        for (int s = 0; s < 20; s++)
        {
            Color slotColor = color_from_rgb(40, 40, 50);
            if (state->Inventory[i][s].WeaponId != -1)
            {
                slotColor = get_weapon_color(state->Inventory[i][s].WeaponId);
            }
            draw_rect_outline(window, infoX + s * (slotW + 1), invY, slotW, slotH,
                              slotColor, color_from_rgb(80, 80, 90), 1.0f);
        }

        int ltsCount = state->Lts[i].Count;
        if (ltsCount > 0)
        {
            string ltsStr = "LTS:" + to_string(ltsCount);
            draw_text(window, ltsStr, infoX + 20 * (8 + 1) + 5, invY, 8,
                      color_from_rgb(140, 140, 160), g_render_ctx.fontRegular);
        }
    }
}

void draw_enemy_battleground(RenderWindow* window, SharedState* state)
{
    draw_rect(window, 500, 50, 780, 370, color_from_rgb(22, 18, 22));

    draw_text(window, "ENEMIES", 510, 55, 14, color_from_rgb(255, 120, 100), g_render_ctx.fontBold);

    if (g_local_gui_phase == 2)
    {
        draw_text(window, ">> Click enemy or press number <<", 620, 55, 11,
                  color_from_rgb(255, 255, 100), g_render_ctx.fontBold);
    }

    int numEnemies = state->Status.NumEnemies;
    g_enemy_rect_count = 0;

    float spriteScale = 0.55f;
    if (numEnemies > 6)
    {
        spriteScale = 0.42f;
    }
    else if (numEnemies > 4)
    {
        spriteScale = 0.48f;
    }

    float sprW = g_peppino_tex.getSize().x * spriteScale;
    float sprH = g_peppino_tex.getSize().y * spriteScale;

    float zigzagOffset = 60.0f;
    float startY = 75.0f;
    float areaHeight = 340.0f;
    float spacing = areaHeight / numEnemies;
    if (spacing > 80.0f)
    {
        spacing = 80.0f;
    }
    if (spacing < 36.0f)
    {
        spacing = 36.0f;
    }

    float barW = sprW + 15;
    float barH = 4;

    for (int i = 0; i < numEnemies; i++)
    {
        float baseX = 1100.0f;
        if (i % 2 == 1)
        {
            baseX = baseX - zigzagOffset;
        }
        float posY = startY + i * spacing;

        int isActive = 0;
        if (!state->Turn.IsPlayerTurn && state->Turn.ActiveEntityId == i)
        {
            isActive = 1;
        }

        int isSelectable = 0;
        int isHighlighted = 0;
        if (g_local_gui_phase == 2 && state->Enemies[i].Alive)
        {
            isSelectable = 1;
            if (g_local_selected_enemy == i)
            {
                isHighlighted = 1;
            }
        }

        float hitboxW = sprW + 80;
        float hitboxH = spacing;
        float hitboxX = baseX - 80;

        if (g_textures_loaded)
        {
            if (isActive)
            {
                draw_rect_outline(window, baseX - 3, posY - 3,
                    sprW + 6, sprH + 6,
                    color_from_rgba(60, 30, 30, 100), color_from_rgb(255, 120, 80), 2.0f);
            }
            else if (isHighlighted)
            {
                draw_rect_outline(window, baseX - 3, posY - 3,
                    sprW + 6, sprH + 6,
                    color_from_rgba(60, 50, 20, 100), color_from_rgb(255, 255, 100), 2.0f);
            }
            else if (isSelectable)
            {
                draw_rect_outline(window, baseX - 2, posY - 2,
                    sprW + 4, sprH + 4,
                    Color::Transparent, color_from_rgb(200, 200, 80), 1.0f);
            }

            Sprite spr(g_peppino_tex);
            spr.setScale(-spriteScale, spriteScale);
            spr.setOrigin((float)g_peppino_tex.getSize().x, 0);
            spr.setPosition(baseX, posY);

            if (!state->Enemies[i].Alive)
            {
                spr.setColor(color_from_rgba(80, 80, 80, 100));
            }
            else if (state->Enemies[i].Stunned)
            {
                spr.setColor(color_from_rgba(200, 200, 50, 200));
            }

            window->draw(spr);
        }

        string nameStr = "E" + to_string(i);
        if (!state->Enemies[i].Alive)
        {
            nameStr = nameStr + " [X]";
        }
        else if (state->Enemies[i].Stunned)
        {
            nameStr = nameStr + " [S]";
        }
        Color nameColor = color_from_rgb(220, 150, 150);
        if (isActive)
        {
            nameColor = color_from_rgb(255, 180, 100);
        }
        if (isHighlighted)
        {
            nameColor = color_from_rgb(255, 255, 150);
        }
        draw_text(window, nameStr, baseX - sprW - 70, posY, 10, nameColor, g_render_ctx.fontBold);

        if (isSelectable && state->Enemies[i].Alive)
        {
            string keyStr = "[" + to_string(i) + "]";
            draw_text(window, keyStr, baseX - sprW - 70, posY + 12, 9,
                      color_from_rgb(255, 255, 100), g_render_ctx.fontBold);
        }

        string hpStr = to_string(state->Enemies[i].Hp);
        draw_text(window, hpStr, baseX - sprW - 70, posY + 22, 8,
                  color_from_rgb(180, 100, 100), g_render_ctx.fontRegular);

        string statStr = "D:" + to_string(state->Enemies[i].Damage)
                              + " S:" + to_string(state->Enemies[i].Speed);
        draw_text(window, statStr, baseX - sprW - 70, posY + 32, 8,
                  color_from_rgb(140, 140, 140), g_render_ctx.fontRegular);

        if (state->Enemies[i].HasWeapon)
        {
            string wpnStr = get_weapon_name_str(state->Enemies[i].WeaponId);
            draw_text(window, wpnStr, baseX - sprW - 70, posY + 42, 8,
                      color_from_rgb(200, 170, 100), g_render_ctx.fontRegular);
        }

        float barX = baseX;
        float barY = posY + sprH - 10;

        draw_bar(window, barX, barY, barW, barH,
                 state->Enemies[i].Hp, state->Enemies[i].MaxHp,
                 color_from_rgb(220, 50, 50), color_from_rgb(60, 30, 30));

        draw_bar(window, barX, barY + barH + 1, barW, barH,
                 state->Enemies[i].Stamina, state->Enemies[i].MaxStamina,
                 color_from_rgb(200, 150, 50), color_from_rgb(30, 30, 40));

        g_enemy_rects[g_enemy_rect_count].x = hitboxX;
        g_enemy_rects[g_enemy_rect_count].y = posY;
        g_enemy_rects[g_enemy_rect_count].w = hitboxW;
        g_enemy_rects[g_enemy_rect_count].h = hitboxH;
        g_enemy_rects[g_enemy_rect_count].enemyId = i;
        g_enemy_rect_count = g_enemy_rect_count + 1;
    }
}

void draw_artifact_panel(RenderWindow* window, SharedState* state)
{
    float panelX = 10;
    float panelY = 422;

    draw_text(window, "ARTIFACTS", panelX, panelY, 14, color_from_rgb(200, 150, 255), g_render_ctx.fontBold);

    g_artifact_button_count = 0;

    for (int i = 0; i < 3; i++)
    {
        if (!state->Artifacts[i].InWorld)
        {
            continue;
        }
        float artX = panelX + 100 + i * 200;
        float artY = panelY + 16;

        Color artBg = color_from_rgb(40, 30, 50);
        Color artBorder = color_from_rgb(100, 70, 130);

        if (!state->Artifacts[i].Free)
        {
            artBg = color_from_rgb(50, 30, 60);
            artBorder = color_from_rgb(160, 80, 200);
        }

        if (g_local_gui_phase == 5 && state->Artifacts[i].Free)
        {
            artBorder = color_from_rgb(255, 255, 100);
        }

        draw_rect_outline(window, artX, artY, 180, 28, artBg, artBorder, 1.5f);

        string artName = "[" + to_string(i) + "] " + get_artifact_name_str(i);
        Color artTextColor = color_from_rgb(220, 180, 255);
        if (state->Artifacts[i].Free)
        {
            artName = artName + " (FREE)";
        }
        else
        {
            artName = artName + " (E" + to_string(state->Artifacts[i].EntityId) + ")";
            artTextColor = color_from_rgb(220, 100, 100);
        }
        draw_text(window, artName, artX + 6, artY + 6, 11, artTextColor, g_render_ctx.fontBold);

        if (g_local_gui_phase == 5 && state->Artifacts[i].Free)
        {
            g_artifact_buttons[g_artifact_button_count].x = artX;
            g_artifact_buttons[g_artifact_button_count].y = artY;
            g_artifact_buttons[g_artifact_button_count].w = 180;
            g_artifact_buttons[g_artifact_button_count].h = 28;
            g_artifact_buttons[g_artifact_button_count].id = i;
            g_artifact_button_count = g_artifact_button_count + 1;
        }
    }

    if (g_local_gui_phase == 5)
    {
        draw_text(window, ">> Click artifact or press number key <<", panelX + 680, panelY + 2, 11,
                  color_from_rgb(255, 255, 100), g_render_ctx.fontBold);
    }
}

void draw_log_panel(RenderWindow* window, SharedState* state)
{
    float panelX = 0;
    float panelY = 470;
    float panelW = 700;
    float panelH = 250;

    draw_rect(window, panelX, panelY, panelW, panelH, color_from_rgb(15, 15, 20));

    draw_text(window, "ACTION LOG", panelX + 10, panelY + 5, 14,
              color_from_rgb(150, 150, 170), g_render_ctx.fontBold);

    int logCount = state->LogCount;
    int logHead = state->LogHead;

    int displayCount = logCount;
    if (displayCount > 13)
    {
        displayCount = 13;
    }

    for (int i = 0; i < displayCount; i++)
    {
        int idx = (logHead - displayCount + i + 50) % 50;
        string logText(state->Log[idx].Text);
        if (logText.length() == 0)
        {
            continue;
        }

        float textY = panelY + 25 + i * 17;

        Color logColor = color_from_rgb(160, 160, 170);

        if (logText.find("DEADLOCK") != string::npos)
        {
            logColor = color_from_rgb(255, 80, 80);
        }
        else if (logText.find("defeated") != string::npos)
        {
            logColor = color_from_rgb(255, 200, 50);
        }
        else if (logText.find("STUNNED") != string::npos)
        {
            logColor = color_from_rgb(255, 150, 50);
        }
        else if (logText.find("ULTIMATE") != string::npos)
        {
            logColor = color_from_rgb(255, 220, 100);
        }
        else if (logText.find("Victory") != string::npos)
        {
            logColor = color_from_rgb(50, 255, 50);
        }
        else if (logText.find("Defeat") != string::npos)
        {
            logColor = color_from_rgb(255, 50, 50);
        }
        else if (logText.find("Eclipse") != string::npos)
        {
            logColor = color_from_rgb(200, 100, 255);
        }
        else if (logText.find("dropped") != string::npos)
        {
            logColor = color_from_rgb(100, 220, 200);
        }
        else if (logText.find("heals") != string::npos)
        {
            logColor = color_from_rgb(100, 220, 100);
        }

        draw_text(window, logText, panelX + 10, textY, 11, logColor, g_render_ctx.fontRegular);
    }
}

void draw_action_menu(RenderWindow* window, SharedState* state)
{
    float panelX = 700;
    float panelY = 470;
    float panelW = 580;
    float panelH = 250;

    draw_rect(window, panelX, panelY, panelW, panelH, color_from_rgb(20, 25, 35));
    draw_rect_outline(window, panelX, panelY, panelW, panelH,
                      Color::Transparent, color_from_rgb(80, 120, 180), 2.0f);

    if (g_local_gui_phase == 0)
    {
        draw_text(window, "Waiting for turn...", panelX + 20, panelY + 130, 16,
                  color_from_rgb(100, 100, 120), g_render_ctx.fontRegular);
        g_action_button_count = 0;
        return;
    }

    if (g_local_gui_phase == 2)
    {
        draw_text(window, "SELECT TARGET", panelX + 20, panelY + 8, 16,
                  color_from_rgb(255, 200, 50), g_render_ctx.fontBold);
        draw_text(window, "Click an enemy above or", panelX + 20, panelY + 35, 13,
                  color_from_rgb(180, 180, 180), g_render_ctx.fontRegular);
        draw_text(window, "press enemy number (0-8)", panelX + 20, panelY + 55, 13,
                  color_from_rgb(180, 180, 180), g_render_ctx.fontRegular);
        draw_text(window, "Press ESC to go back", panelX + 20, panelY + 80, 12,
                  color_from_rgb(150, 100, 100), g_render_ctx.fontRegular);
        return;
    }

    if (g_local_gui_phase == 3)
    {
        draw_text(window, "SELECT WEAPON", panelX + 20, panelY + 8, 16,
                  color_from_rgb(255, 200, 50), g_render_ctx.fontBold);

        int playerId = state->Gui.ActivePlayerId;
        g_weapon_button_count = 0;
        int checked[20];
        for (int i = 0; i < 20; i++)
        {
            checked[i] = 0;
        }

        int row = 0;
        for (int s = 0; s < 20; s++)
        {
            if (state->Inventory[playerId][s].WeaponId != -1 && !checked[s])
            {
                int startSlot = state->Inventory[playerId][s].OccupiedBy;
                if (startSlot == s)
                {
                    int wid = state->Inventory[playerId][s].WeaponId;
                    float btnX = panelX + 20;
                    float btnY = panelY + 35 + row * 30;
                    float btnW = 340;
                    float btnH = 25;

                    draw_rect_outline(window, btnX, btnY, btnW, btnH,
                                      color_from_rgb(35, 40, 55), color_from_rgb(100, 150, 200), 1.0f);

                    string wpnStr = "[" + to_string(row) + "] " + get_weapon_name_str(wid);
                    draw_text(window, wpnStr, btnX + 8, btnY + 4, 12,
                              get_weapon_color(wid), g_render_ctx.fontRegular);

                    g_weapon_buttons[g_weapon_button_count].x = btnX;
                    g_weapon_buttons[g_weapon_button_count].y = btnY;
                    g_weapon_buttons[g_weapon_button_count].w = btnW;
                    g_weapon_buttons[g_weapon_button_count].h = btnH;
                    g_weapon_buttons[g_weapon_button_count].id = wid;
                    g_weapon_button_count = g_weapon_button_count + 1;

                    int endSlot = s;
                    while (endSlot + 1 < 20 && state->Inventory[playerId][endSlot + 1].OccupiedBy == s)
                    {
                        endSlot = endSlot + 1;
                    }
                    for (int k = s; k <= endSlot; k++)
                    {
                        checked[k] = 1;
                    }
                    row = row + 1;
                }
                else
                {
                    checked[s] = 1;
                }
            }
        }

        if (row == 0)
        {
            draw_text(window, "No weapons! Defaulting to Strike.", panelX + 20, panelY + 40, 13,
                      color_from_rgb(200, 100, 100), g_render_ctx.fontRegular);
        }

        draw_text(window, "Press ESC to go back", panelX + 20, panelY + 270, 12,
                  color_from_rgb(150, 100, 100), g_render_ctx.fontRegular);
        return;
    }

    if (g_local_gui_phase == 4)
    {
        draw_text(window, "SELECT LTS WEAPON", panelX + 20, panelY + 8, 16,
                  color_from_rgb(255, 200, 50), g_render_ctx.fontBold);

        int playerId = state->Gui.ActivePlayerId;
        g_lts_button_count = 0;
        int row = 0;

        for (int w = 0; w < 20; w++)
        {
            if (state->Lts[playerId].Weapons[w].InUse)
            {
                float btnX = panelX + 20;
                float btnY = panelY + 35 + row * 30;
                float btnW = 340;
                float btnH = 25;

                draw_rect_outline(window, btnX, btnY, btnW, btnH,
                                  color_from_rgb(35, 40, 55), color_from_rgb(100, 150, 200), 1.0f);

                string ltsStr = "[" + to_string(w) + "] "
                    + get_weapon_name_str(state->Lts[playerId].Weapons[w].WeaponId)
                    + " d:" + to_string(state->Lts[playerId].Weapons[w].Damage)
                    + " sz:" + to_string(state->Lts[playerId].Weapons[w].SlotSize);
                draw_text(window, ltsStr, btnX + 8, btnY + 4, 11,
                          color_from_rgb(180, 200, 220), g_render_ctx.fontRegular);

                g_lts_buttons[g_lts_button_count].x = btnX;
                g_lts_buttons[g_lts_button_count].y = btnY;
                g_lts_buttons[g_lts_button_count].w = btnW;
                g_lts_buttons[g_lts_button_count].h = btnH;
                g_lts_buttons[g_lts_button_count].id = w;
                g_lts_button_count = g_lts_button_count + 1;

                row = row + 1;
            }
        }

        if (row == 0)
        {
            draw_text(window, "LTS is empty!", panelX + 20, panelY + 40, 13,
                      color_from_rgb(200, 100, 100), g_render_ctx.fontRegular);
        }

        draw_text(window, "Press ESC to go back", panelX + 20, panelY + 270, 12,
                  color_from_rgb(150, 100, 100), g_render_ctx.fontRegular);
        return;
    }

    if (g_local_gui_phase == 5)
    {
        draw_text(window, "SELECT ARTIFACT", panelX + 20, panelY + 8, 16,
                  color_from_rgb(255, 200, 50), g_render_ctx.fontBold);
        draw_text(window, "Click an artifact above or", panelX + 20, panelY + 35, 13,
                  color_from_rgb(180, 180, 180), g_render_ctx.fontRegular);
        draw_text(window, "press its number key (0-2)", panelX + 20, panelY + 55, 13,
                  color_from_rgb(180, 180, 180), g_render_ctx.fontRegular);
        draw_text(window, "Press ESC to go back", panelX + 20, panelY + 80, 12,
                  color_from_rgb(150, 100, 100), g_render_ctx.fontRegular);
        return;
    }

    if (g_local_gui_phase == 6)
    {
        draw_text(window, "WEAPON DROP!", panelX + 20, panelY + 8, 18,
                  color_from_rgb(100, 255, 200), g_render_ctx.fontBold);

        string dropName = get_weapon_name_str(state->DropEvent.WeaponId);
        string dropInfo = dropName + " (size:" + to_string(state->DropEvent.SlotSize)
                          + " dmg:" + to_string(state->DropEvent.WeaponDamage) + ")";
        draw_text(window, dropInfo, panelX + 20, panelY + 40, 14,
                  color_from_rgb(200, 220, 200), g_render_ctx.fontRegular);

        draw_text(window, "Pick up this weapon?", panelX + 20, panelY + 70, 14,
                  color_from_rgb(180, 180, 180), g_render_ctx.fontRegular);

        float yesX = panelX + 40;
        float yesY = panelY + 110;
        float noX = panelX + 210;
        float noY = panelY + 110;
        float btnW = 120;
        float btnH = 40;

        draw_rect_outline(window, yesX, yesY, btnW, btnH,
                          color_from_rgb(30, 80, 50), color_from_rgb(80, 220, 100), 2.0f);
        draw_text(window, "[1] ACCEPT", yesX + 10, yesY + 10, 16,
                  color_from_rgb(100, 255, 100), g_render_ctx.fontBold);

        draw_rect_outline(window, noX, noY, btnW, btnH,
                          color_from_rgb(80, 30, 30), color_from_rgb(220, 80, 80), 2.0f);
        draw_text(window, "[0] REJECT", noX + 10, noY + 10, 16,
                  color_from_rgb(255, 100, 100), g_render_ctx.fontBold);

        g_drop_buttons[0].x = yesX;
        g_drop_buttons[0].y = yesY;
        g_drop_buttons[0].w = btnW;
        g_drop_buttons[0].h = btnH;
        g_drop_buttons[0].id = 1;

        g_drop_buttons[1].x = noX;
        g_drop_buttons[1].y = noY;
        g_drop_buttons[1].w = btnW;
        g_drop_buttons[1].h = btnH;
        g_drop_buttons[1].id = 0;
        return;
    }

    draw_text(window, "ACTIONS", panelX + 20, panelY + 8, 16,
              color_from_rgb(100, 200, 255), g_render_ctx.fontBold);

    string actionNames[9];
    actionNames[0] = "[0] Strike";
    actionNames[1] = "[1] Exhaust";
    actionNames[2] = "[2] Use Weapon";
    actionNames[3] = "[3] Swap In (LTS)";
    actionNames[4] = "[4] Heal";
    actionNames[5] = "[5] Skip";
    actionNames[6] = "[6] Ultimate";
    actionNames[7] = "[7] Acquire Artifact";
    actionNames[8] = "[8] Quit Game";

    Color actionColors[9];
    actionColors[0] = color_from_rgb(220, 100, 100);
    actionColors[1] = color_from_rgb(200, 150, 50);
    actionColors[2] = color_from_rgb(150, 180, 255);
    actionColors[3] = color_from_rgb(100, 200, 150);
    actionColors[4] = color_from_rgb(100, 220, 100);
    actionColors[5] = color_from_rgb(150, 150, 150);
    actionColors[6] = color_from_rgb(255, 220, 50);
    actionColors[7] = color_from_rgb(200, 150, 255);
    actionColors[8] = color_from_rgb(180, 60, 60);

    g_action_button_count = 0;

    for (int i = 0; i < 9; i++)
    {
        float btnX = panelX + 15;
        float btnY = panelY + 30 + i * 23;
        float btnW = 350;
        float btnH = 20;

        Color btnBg = color_from_rgb(30, 35, 50);
        Color btnBorder = color_from_rgb(60, 70, 100);

        draw_rect_outline(window, btnX, btnY, btnW, btnH, btnBg, btnBorder, 1.0f);
        draw_text(window, actionNames[i], btnX + 10, btnY + 4, 13,
                  actionColors[i], g_render_ctx.fontRegular);

        g_action_buttons[g_action_button_count].x = btnX;
        g_action_buttons[g_action_button_count].y = btnY;
        g_action_buttons[g_action_button_count].w = btnW;
        g_action_buttons[g_action_button_count].h = btnH;
        g_action_buttons[g_action_button_count].id = i;
        g_action_button_count = g_action_button_count + 1;
    }
}

void draw_inventory_overlay(RenderWindow* window, SharedState* state)
{
    if (!g_show_inventory_overlay)
    {
        return;
    }

    int playerId = state->Gui.ActivePlayerId;
    if (playerId < 0 || playerId >= 4)
    {
        return;
    }

    float boxX = 300;
    float boxY = 80;
    float boxW = 680;
    float boxH = 320;

    draw_rect_outline(window, boxX, boxY, boxW, boxH,
                      color_from_rgba(20, 25, 35, 240), color_from_rgb(100, 150, 200), 2.0f);
    draw_text(window, "INVENTORY - Player " + to_string(playerId), boxX + 15, boxY + 8, 16,
              color_from_rgb(100, 200, 255), g_render_ctx.fontBold);
    draw_text(window, "Press I or ESC to close", boxX + 350, boxY + 10, 12,
              color_from_rgb(150, 150, 150), g_render_ctx.fontRegular);

    float slotStartX = boxX + 15;
    float slotStartY = boxY + 35;
    float slotW = 26;
    float slotH = 22;

    for (int s = 0; s < 20; s++)
    {
        Color slotColor = color_from_rgb(40, 40, 50);
        if (state->Inventory[playerId][s].WeaponId != -1)
        {
            slotColor = get_weapon_color(state->Inventory[playerId][s].WeaponId);
        }
        draw_rect_outline(window, slotStartX + s * (slotW + 2), slotStartY, slotW, slotH,
                          slotColor, color_from_rgb(80, 80, 90), 1.0f);
        draw_text(window, to_string(s), slotStartX + s * (slotW + 2) + 6, slotStartY + 3, 10,
                  color_from_rgb(200, 200, 200), g_render_ctx.fontRegular);
    }

    int checked[20];
    for (int i = 0; i < 20; i++)
    {
        checked[i] = 0;
    }

    int row = 0;
    for (int s = 0; s < 20; s++)
    {
        if (state->Inventory[playerId][s].WeaponId != -1 && !checked[s])
        {
            int startSlot = state->Inventory[playerId][s].OccupiedBy;
            if (startSlot == s)
            {
                int wid = state->Inventory[playerId][s].WeaponId;
                int endSlot = s;
                while (endSlot + 1 < 20 && state->Inventory[playerId][endSlot + 1].OccupiedBy == s)
                {
                    endSlot = endSlot + 1;
                }
                string wStr = "Slots [" + to_string(s) + "-" + to_string(endSlot) + "]: "
                    + get_weapon_name_str(wid) + " (id=" + to_string(wid) + ")";
                draw_text(window, wStr, boxX + 15, slotStartY + 30 + row * 20, 12,
                          get_weapon_color(wid), g_render_ctx.fontRegular);

                for (int k = s; k <= endSlot; k++)
                {
                    checked[k] = 1;
                }
                row = row + 1;
            }
            else
            {
                checked[s] = 1;
            }
        }
    }

    draw_text(window, "--- Long Term Storage ---", boxX + 15, slotStartY + 35 + row * 20, 13,
              color_from_rgb(140, 140, 180), g_render_ctx.fontBold);

    int ltsRow = 0;
    float ltsStartY = slotStartY + 55 + row * 20;
    for (int w = 0; w < 20; w++)
    {
        if (state->Lts[playerId].Weapons[w].InUse)
        {
            string ltsStr = "[" + to_string(w) + "] "
                + get_weapon_name_str(state->Lts[playerId].Weapons[w].WeaponId)
                + " (dmg=" + to_string(state->Lts[playerId].Weapons[w].Damage)
                + " size=" + to_string(state->Lts[playerId].Weapons[w].SlotSize) + ")";
            draw_text(window, ltsStr, boxX + 15, ltsStartY + ltsRow * 18, 11,
                      color_from_rgb(160, 180, 200), g_render_ctx.fontRegular);
            ltsRow = ltsRow + 1;
        }
    }

    if (ltsRow == 0)
    {
        draw_text(window, "(empty)", boxX + 15, ltsStartY, 11,
                  color_from_rgb(100, 100, 100), g_render_ctx.fontRegular);
    }
}

void draw_weapon_drop_notification(RenderWindow* window, SharedState* state)
{
    if (!state->DropEvent.Pending)
    {
        return;
    }
    if (state->DropEvent.PlayerDecided)
    {
        return;
    }
    if (g_local_gui_phase == 6)
    {
        return;
    }

    float boxX = 400;
    float boxY = 200;
    float boxW = 480;
    float boxH = 50;

    draw_rect_outline(window, boxX, boxY, boxW, boxH,
                      color_from_rgb(30, 40, 50), color_from_rgb(100, 220, 200), 2.0f);

    string dropStr = "WEAPON DROP: " + get_weapon_name_str(state->DropEvent.WeaponId)
                     + " (waiting for player input)";
    draw_text(window, dropStr, boxX + 15, boxY + 15, 14,
              color_from_rgb(100, 255, 200), g_render_ctx.fontBold);
}

void draw_game_over_overlay(RenderWindow* window, SharedState* state)
{
    if (!state->Status.GameOver)
    {
        return;
    }

    draw_rect(window, 0, 0, 1280, 720, color_from_rgba(0, 0, 0, 150));

    if (state->Status.PlayerWon)
    {
        draw_text(window, "VICTORY", 440, 250, 72,
                  color_from_rgb(50, 255, 80), g_render_ctx.fontBold);
        draw_text(window, "All enemies have been vanquished!", 400, 340, 20,
                  color_from_rgb(180, 220, 180), g_render_ctx.fontRegular);
    }
    else
    {
        draw_text(window, "DEFEAT", 460, 250, 72,
                  color_from_rgb(255, 50, 50), g_render_ctx.fontBold);
        draw_text(window, "Your party has fallen...", 440, 340, 20,
                  color_from_rgb(220, 180, 180), g_render_ctx.fontRegular);
    }

    string killsStr = "Total Kills: " + to_string(state->Status.EnemiesKilled);
    draw_text(window, killsStr, 520, 390, 18,
              color_from_rgb(200, 200, 200), g_render_ctx.fontRegular);
}

void draw_separator_lines(RenderWindow* window)
{
    draw_rect(window, 0, 50, 1280, 2, color_from_rgb(50, 50, 60));
    draw_rect(window, 500, 52, 2, 368, color_from_rgb(40, 40, 50));
    draw_rect(window, 0, 420, 1280, 2, color_from_rgb(50, 50, 60));
    draw_rect(window, 0, 468, 1280, 2, color_from_rgb(50, 50, 60));
    draw_rect(window, 700, 470, 2, 250, color_from_rgb(50, 50, 60));
}

void finalize_gui_action(SharedState* state, int action, int target, int weapon, int lts, int artifact)
{
    sem_wait(&state->GlobalLock);
    state->Gui.ActionChosen = action;
    state->Gui.TargetChosen = target;
    state->Gui.WeaponChosen = weapon;
    state->Gui.LtsChosen = lts;
    state->Gui.ArtifactChosen = artifact;
    state->Gui.Confirmed = 1;
    sem_post(&state->GlobalLock);

    g_local_gui_phase = 0;
    g_local_action_chosen = -1;
    g_local_target_chosen = -1;
    g_local_weapon_chosen = -1;
    g_local_lts_chosen = -1;
    g_local_artifact_chosen = -1;
    g_local_selected_enemy = -1;
}

void finalize_drop_choice(SharedState* state, int choice)
{
    sem_wait(&state->GlobalLock);
    state->Gui.DropChoice = choice;
    state->Gui.DropConfirmed = 1;
    sem_post(&state->GlobalLock);

    g_local_gui_phase = 0;
}

void handle_action_chosen(SharedState* state, int actionId)
{
    if (actionId == 0 || actionId == 1)
    {
        g_local_action_chosen = actionId;
        g_local_gui_phase = 2;
        g_local_selected_enemy = -1;
    }
    else if (actionId == 2)
    {
        g_local_action_chosen = 2;
        int playerId = state->Gui.ActivePlayerId;
        int hasWeapon = 0;
        for (int s = 0; s < 20; s++)
        {
            if (state->Inventory[playerId][s].WeaponId != -1)
            {
                hasWeapon = 1;
                break;
            }
        }
        if (hasWeapon)
        {
            g_local_gui_phase = 3;
        }
        else
        {
            g_local_action_chosen = 0;
            g_local_gui_phase = 2;
            g_local_selected_enemy = -1;
        }
    }
    else if (actionId == 3)
    {
        g_local_action_chosen = 3;
        int playerId = state->Gui.ActivePlayerId;
        if (state->Lts[playerId].Count > 0)
        {
            g_local_gui_phase = 4;
        }
        else
        {
            finalize_gui_action(state, 5, -1, -1, -1, -1);
        }
    }
    else if (actionId == 4)
    {
        finalize_gui_action(state, 4, -1, -1, -1, -1);
    }
    else if (actionId == 5)
    {
        finalize_gui_action(state, 5, -1, -1, -1, -1);
    }
    else if (actionId == 6)
    {
        finalize_gui_action(state, 6, -1, -1, -1, -1);
    }
    else if (actionId == 7)
    {
        g_local_action_chosen = 7;
        g_local_gui_phase = 5;
    }
    else if (actionId == 8)
    {
        finalize_gui_action(state, 9, -1, -1, -1, -1);
    }
}

void handle_target_chosen(SharedState* state, int targetId)
{
    if (targetId < 0 || targetId >= state->Status.NumEnemies)
    {
        return;
    }
    if (!state->Enemies[targetId].Alive)
    {
        return;
    }

    g_local_target_chosen = targetId;

    if (g_local_action_chosen == 0)
    {
        finalize_gui_action(state, 0, targetId, -1, -1, -1);
    }
    else if (g_local_action_chosen == 1)
    {
        finalize_gui_action(state, 1, targetId, -1, -1, -1);
    }
    else if (g_local_action_chosen == 2)
    {
        finalize_gui_action(state, 2, targetId, g_local_weapon_chosen, -1, -1);
    }
}

void handle_weapon_chosen(SharedState* state, int weaponId)
{
    g_local_weapon_chosen = weaponId;
    g_local_gui_phase = 2;
    g_local_selected_enemy = -1;
}

void handle_lts_chosen(SharedState* state, int ltsIndex)
{
    g_local_lts_chosen = ltsIndex;
    finalize_gui_action(state, 3, -1, -1, ltsIndex, -1);
}

void handle_artifact_chosen(SharedState* state, int artifactId)
{
    if (artifactId < 0 || artifactId > 2)
    {
        return;
    }
    if (!state->Artifacts[artifactId].InWorld || !state->Artifacts[artifactId].Free)
    {
        return;
    }
    g_local_artifact_chosen = artifactId;
    finalize_gui_action(state, 7, artifactId, -1, -1, artifactId);
}

int point_in_rect(float px, float py, float rx, float ry, float rw, float rh)
{
    if (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh)
    {
        return 1;
    }
    return 0;
}

void handle_mouse_click(SharedState* state, float mx, float my)
{
    if (g_local_gui_phase == 1)
    {
        for (int i = 0; i < g_action_button_count; i++)
        {
            if (point_in_rect(mx, my, g_action_buttons[i].x, g_action_buttons[i].y,
                              g_action_buttons[i].w, g_action_buttons[i].h))
            {
                handle_action_chosen(state, g_action_buttons[i].id);
                return;
            }
        }
    }
    else if (g_local_gui_phase == 2)
    {
        for (int i = 0; i < g_enemy_rect_count; i++)
        {
            if (point_in_rect(mx, my, g_enemy_rects[i].x, g_enemy_rects[i].y,
                              g_enemy_rects[i].w, g_enemy_rects[i].h))
            {
                int eid = g_enemy_rects[i].enemyId;
                if (state->Enemies[eid].Alive)
                {
                    handle_target_chosen(state, eid);
                    return;
                }
            }
        }
    }
    else if (g_local_gui_phase == 3)
    {
        for (int i = 0; i < g_weapon_button_count; i++)
        {
            if (point_in_rect(mx, my, g_weapon_buttons[i].x, g_weapon_buttons[i].y,
                              g_weapon_buttons[i].w, g_weapon_buttons[i].h))
            {
                handle_weapon_chosen(state, g_weapon_buttons[i].id);
                return;
            }
        }
    }
    else if (g_local_gui_phase == 4)
    {
        for (int i = 0; i < g_lts_button_count; i++)
        {
            if (point_in_rect(mx, my, g_lts_buttons[i].x, g_lts_buttons[i].y,
                              g_lts_buttons[i].w, g_lts_buttons[i].h))
            {
                handle_lts_chosen(state, g_lts_buttons[i].id);
                return;
            }
        }
    }
    else if (g_local_gui_phase == 5)
    {
        for (int i = 0; i < g_artifact_button_count; i++)
        {
            if (point_in_rect(mx, my, g_artifact_buttons[i].x, g_artifact_buttons[i].y,
                              g_artifact_buttons[i].w, g_artifact_buttons[i].h))
            {
                handle_artifact_chosen(state, g_artifact_buttons[i].id);
                return;
            }
        }
    }
    else if (g_local_gui_phase == 6)
    {
        if (point_in_rect(mx, my, g_drop_buttons[0].x, g_drop_buttons[0].y,
                          g_drop_buttons[0].w, g_drop_buttons[0].h))
        {
            finalize_drop_choice(state, 1);
            return;
        }
        if (point_in_rect(mx, my, g_drop_buttons[1].x, g_drop_buttons[1].y,
                          g_drop_buttons[1].w, g_drop_buttons[1].h))
        {
            finalize_drop_choice(state, 0);
            return;
        }
    }
}

void handle_mouse_move(SharedState* state, float mx, float my)
{
    if (g_local_gui_phase == 2)
    {
        g_local_selected_enemy = -1;
        for (int i = 0; i < g_enemy_rect_count; i++)
        {
            if (point_in_rect(mx, my, g_enemy_rects[i].x, g_enemy_rects[i].y,
                              g_enemy_rects[i].w, g_enemy_rects[i].h))
            {
                int eid = g_enemy_rects[i].enemyId;
                if (state->Enemies[eid].Alive)
                {
                    g_local_selected_enemy = eid;
                }
                break;
            }
        }
    }
}

void handle_key_press(SharedState* state, int keyCode)
{
    if (keyCode == Keyboard::I)
    {
        g_show_inventory_overlay = !g_show_inventory_overlay;
        return;
    }

    if (g_show_inventory_overlay && keyCode == Keyboard::Escape)
    {
        g_show_inventory_overlay = 0;
        return;
    }

    if (g_local_gui_phase == 1)
    {
        int actionId = -1;
        if (keyCode == Keyboard::Num0 || keyCode == Keyboard::Numpad0)
        {
            actionId = 0;
        }
        else if (keyCode == Keyboard::Num1 || keyCode == Keyboard::Numpad1)
        {
            actionId = 1;
        }
        else if (keyCode == Keyboard::Num2 || keyCode == Keyboard::Numpad2)
        {
            actionId = 2;
        }
        else if (keyCode == Keyboard::Num3 || keyCode == Keyboard::Numpad3)
        {
            actionId = 3;
        }
        else if (keyCode == Keyboard::Num4 || keyCode == Keyboard::Numpad4)
        {
            actionId = 4;
        }
        else if (keyCode == Keyboard::Num5 || keyCode == Keyboard::Numpad5)
        {
            actionId = 5;
        }
        else if (keyCode == Keyboard::Num6 || keyCode == Keyboard::Numpad6)
        {
            actionId = 6;
        }
        else if (keyCode == Keyboard::Num7 || keyCode == Keyboard::Numpad7)
        {
            actionId = 7;
        }
        else if (keyCode == Keyboard::Num8 || keyCode == Keyboard::Numpad8)
        {
            actionId = 8;
        }

        if (actionId >= 0)
        {
            handle_action_chosen(state, actionId);
        }
    }
    else if (g_local_gui_phase == 2)
    {
        if (keyCode == Keyboard::Escape)
        {
            g_local_gui_phase = 1;
            g_local_action_chosen = -1;
            g_local_selected_enemy = -1;
            return;
        }

        int targetId = -1;
        if (keyCode == Keyboard::Num0 || keyCode == Keyboard::Numpad0)
        {
            targetId = 0;
        }
        else if (keyCode == Keyboard::Num1 || keyCode == Keyboard::Numpad1)
        {
            targetId = 1;
        }
        else if (keyCode == Keyboard::Num2 || keyCode == Keyboard::Numpad2)
        {
            targetId = 2;
        }
        else if (keyCode == Keyboard::Num3 || keyCode == Keyboard::Numpad3)
        {
            targetId = 3;
        }
        else if (keyCode == Keyboard::Num4 || keyCode == Keyboard::Numpad4)
        {
            targetId = 4;
        }
        else if (keyCode == Keyboard::Num5 || keyCode == Keyboard::Numpad5)
        {
            targetId = 5;
        }
        else if (keyCode == Keyboard::Num6 || keyCode == Keyboard::Numpad6)
        {
            targetId = 6;
        }
        else if (keyCode == Keyboard::Num7 || keyCode == Keyboard::Numpad7)
        {
            targetId = 7;
        }
        else if (keyCode == Keyboard::Num8 || keyCode == Keyboard::Numpad8)
        {
            targetId = 8;
        }

        if (targetId >= 0)
        {
            handle_target_chosen(state, targetId);
        }
    }
    else if (g_local_gui_phase == 3)
    {
        if (keyCode == Keyboard::Escape)
        {
            g_local_gui_phase = 1;
            g_local_action_chosen = -1;
            return;
        }

        int idx = -1;
        if (keyCode == Keyboard::Num0 || keyCode == Keyboard::Numpad0)
        {
            idx = 0;
        }
        else if (keyCode == Keyboard::Num1 || keyCode == Keyboard::Numpad1)
        {
            idx = 1;
        }
        else if (keyCode == Keyboard::Num2 || keyCode == Keyboard::Numpad2)
        {
            idx = 2;
        }
        else if (keyCode == Keyboard::Num3 || keyCode == Keyboard::Numpad3)
        {
            idx = 3;
        }
        else if (keyCode == Keyboard::Num4 || keyCode == Keyboard::Numpad4)
        {
            idx = 4;
        }
        else if (keyCode == Keyboard::Num5 || keyCode == Keyboard::Numpad5)
        {
            idx = 5;
        }
        else if (keyCode == Keyboard::Num6 || keyCode == Keyboard::Numpad6)
        {
            idx = 6;
        }
        else if (keyCode == Keyboard::Num7 || keyCode == Keyboard::Numpad7)
        {
            idx = 7;
        }
        else if (keyCode == Keyboard::Num8 || keyCode == Keyboard::Numpad8)
        {
            idx = 8;
        }
        else if (keyCode == Keyboard::Num9 || keyCode == Keyboard::Numpad9)
        {
            idx = 9;
        }

        if (idx >= 0 && idx < g_weapon_button_count)
        {
            handle_weapon_chosen(state, g_weapon_buttons[idx].id);
        }
    }
    else if (g_local_gui_phase == 4)
    {
        if (keyCode == Keyboard::Escape)
        {
            g_local_gui_phase = 1;
            g_local_action_chosen = -1;
            return;
        }

        int idx = -1;
        if (keyCode == Keyboard::Num0 || keyCode == Keyboard::Numpad0)
        {
            idx = 0;
        }
        else if (keyCode == Keyboard::Num1 || keyCode == Keyboard::Numpad1)
        {
            idx = 1;
        }
        else if (keyCode == Keyboard::Num2 || keyCode == Keyboard::Numpad2)
        {
            idx = 2;
        }
        else if (keyCode == Keyboard::Num3 || keyCode == Keyboard::Numpad3)
        {
            idx = 3;
        }
        else if (keyCode == Keyboard::Num4 || keyCode == Keyboard::Numpad4)
        {
            idx = 4;
        }
        else if (keyCode == Keyboard::Num5 || keyCode == Keyboard::Numpad5)
        {
            idx = 5;
        }
        else if (keyCode == Keyboard::Num6 || keyCode == Keyboard::Numpad6)
        {
            idx = 6;
        }
        else if (keyCode == Keyboard::Num7 || keyCode == Keyboard::Numpad7)
        {
            idx = 7;
        }
        else if (keyCode == Keyboard::Num8 || keyCode == Keyboard::Numpad8)
        {
            idx = 8;
        }
        else if (keyCode == Keyboard::Num9 || keyCode == Keyboard::Numpad9)
        {
            idx = 9;
        }

        if (idx >= 0 && idx < g_lts_button_count)
        {
            handle_lts_chosen(state, g_lts_buttons[idx].id);
        }
    }
    else if (g_local_gui_phase == 5)
    {
        if (keyCode == Keyboard::Escape)
        {
            g_local_gui_phase = 1;
            g_local_action_chosen = -1;
            return;
        }

        int artifactId = -1;
        if (keyCode == Keyboard::Num0 || keyCode == Keyboard::Numpad0)
        {
            artifactId = 0;
        }
        else if (keyCode == Keyboard::Num1 || keyCode == Keyboard::Numpad1)
        {
            artifactId = 1;
        }
        else if (keyCode == Keyboard::Num2 || keyCode == Keyboard::Numpad2)
        {
            artifactId = 2;
        }

        if (artifactId >= 0)
        {
            handle_artifact_chosen(state, artifactId);
        }
    }
    else if (g_local_gui_phase == 6)
    {
        if (keyCode == Keyboard::Num1)
        {
            finalize_drop_choice(state, 1);
        }
        else if (keyCode == Keyboard::Num0)
        {
            finalize_drop_choice(state, 0);
        }
    }
}

void update_gui_phase(SharedState* state)
{
    if (state->Status.GameOver)
    {
        g_local_gui_phase = 0;
        return;
    }

    int guiPhase = state->Gui.Phase;

    if (guiPhase == 1 && g_local_gui_phase == 0)
    {
        g_local_gui_phase = 1;
        g_local_action_chosen = -1;
        g_local_target_chosen = -1;
        g_local_weapon_chosen = -1;
        g_local_lts_chosen = -1;
        g_local_artifact_chosen = -1;
        g_local_selected_enemy = -1;
        g_show_inventory_overlay = 0;
    }
    else if (guiPhase == 6 && g_local_gui_phase != 6)
    {
        g_local_gui_phase = 6;
    }
    else if (guiPhase == 0 && g_local_gui_phase != 0)
    {
        if (state->Gui.Confirmed || state->Gui.DropConfirmed)
        {
            g_local_gui_phase = 0;
        }
        else
        {
            g_local_gui_phase = 0;
        }
    }
}

void render_frame(RenderWindow* window, SharedState* state)
{
    window->clear(color_from_rgb(12, 12, 18));

    draw_top_bar(window, state);
    draw_player_battleground(window, state);
    draw_enemy_battleground(window, state);
    draw_artifact_panel(window, state);
    draw_log_panel(window, state);
    draw_action_menu(window, state);
    draw_separator_lines(window);
    draw_weapon_drop_notification(window, state);
    draw_inventory_overlay(window, state);
    draw_game_over_overlay(window, state);

    window->display();
}

void* render_thread_func(void* arg)
{
    SharedState* state = (SharedState*)arg;

    RenderWindow window(VideoMode(1280, 720), "Chrono Rift",
                            Style::Titlebar | Style::Close | Style::Fullscreen);
    window.setFramerateLimit(30);

    g_render_ctx.state = state;
    g_render_ctx.windowWidth = 1280;
    g_render_ctx.windowHeight = 720;
    g_render_ctx.running = 1;

    if (load_fonts() == -1)
    {
        window.close();
        return NULL;
    }

    load_textures();

    g_local_gui_phase = 0;

    SharedState snapshot;

    while (window.isOpen() && g_render_ctx.running)
    {
        Event event;
        while (window.pollEvent(event))
        {
            if (event.type == Event::Closed)
            {
                g_render_ctx.running = 0;
                sem_wait(&state->GlobalLock);
                state->Status.GameOver = 1;
                state->Status.PlayerWon = 0;
                sem_post(&state->GlobalLock);
                window.close();
                return NULL;
            }
            else if (event.type == Event::MouseButtonPressed)
            {
                if (event.mouseButton.button == Mouse::Left)
                {
                    float mx = (float)event.mouseButton.x;
                    float my = (float)event.mouseButton.y;
                    handle_mouse_click(state, mx, my);
                }
            }
            else if (event.type == Event::MouseMoved)
            {
                float mx = (float)event.mouseMove.x;
                float my = (float)event.mouseMove.y;
                handle_mouse_move(state, mx, my);
            }
            else if (event.type == Event::KeyPressed)
            {
                handle_key_press(state, event.key.code);
            }
        }

        update_gui_phase(state);

        sem_wait(&state->GlobalLock);
        int totalBytes = sizeof(SharedState);
        char* dst = (char*)&snapshot;
        char* src = (char*)state;
        for (int b = 0; b < totalBytes; b++)
        {
            dst[b] = src[b];
        }
        sem_post(&state->GlobalLock);

        render_frame(&window, &snapshot);

        if (state->Status.GameOver)
        {
            sem_wait(&state->GlobalLock);
            int totalBytes2 = sizeof(SharedState);
            char* dst2 = (char*)&snapshot;
            char* src2 = (char*)state;
            for (int b = 0; b < totalBytes2; b++)
            {
                dst2[b] = src2[b];
            }
            sem_post(&state->GlobalLock);

            render_frame(&window, &snapshot);
            sleep(3);
            g_render_ctx.running = 0;
            window.close();
            return NULL;
        }
    }

    return NULL;
}

void stop_renderer()
{
    g_render_ctx.running = 0;
}

int is_renderer_running()
{
    return g_render_ctx.running;
}
