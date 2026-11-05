#include "patches.h"

// Ext, a ninth control style for a modern pad, as the PC ports play one (Perfect Dark's port's Ext and
// goldeneye-pc-port's Modern scheme): the right stick turns and looks, the left stick walks and strafes, RT fires and
// LT aims. The game's side; src/game/ext_controls.cpp is the program's, and says what each button does.
//
// GoldenEye already has those sticks in its 2.4 style (Goodhead), played with two N64 pads: the first pad's stick turns
// and looks (and in aim mode moves the crosshair) and its Z aims, the second pad's stick walks and strafes and its Z
// fires. So Ext is 2.4 to the game: every reader of the style (cur_player_get_control_type) gets 2.4 for it, and the
// program plays one modern pad, the keyboard and the mouse as both of 2.4's pads while Ext plays (recomp_ext_playing,
// said each tick Bond is the player's to move, from patches/mouse.c). What Ext adds beyond the two pads (reload, crouch,
// next and previous weapon) comes as patches/mouse.c's actions.
//
// Where it is kept: in the player's style (cur_player_control_type_0) as 8, after 2.4 in the watch's Control Style list,
// where the watch shows Ext's buttons as text in place of the N64 pads. A save keeps the style in 3 bits of its options,
// which the eight styles fill, so Ext is a bit the game never sets or reads (SAVE_OPTION_EXT), with 1.1 in the 3 bits,
// and every other style is saved with another (SAVE_OPTION_STYLE_SAVED) beside it. A console reads an Ext file as 1.1.
// A save without either bit (a console's, a build's before Ext, or one the game has just made) keeps its style once a
// mission is completed in it, and is new, and plays Ext, until then if its style is 1.1, the game's default: the game
// makes a save in several places, each from a blank one. A folder without a save plays Ext, and the save made for it as
// its first mission is completed keeps Ext (fileUnlockStageInFolderAtDifficulty).

// The current player's fields, at the offsets the recompiled code reads them at.
#define PLAYER_FIELD(type, offset) (*(type*) ((u8*) g_CurrentPlayer + (offset)))
#define PLAYER_CONTROL_TYPE PLAYER_FIELD(s32, 0x2A58)
#define PLAYER_CONTROL_TYPE_1 PLAYER_FIELD(s32, 0x2A5C)
#define PLAYER_CONTROL_TYPE_2 PLAYER_FIELD(f32, 0x2A60)
#define PLAYER_CONTROL_TYPE_VSPACING PLAYER_FIELD(s32, 0x2A64)
#define PLAYER_HAS_SET_CONTROL_TYPE PLAYER_FIELD(u32, 0x2A68)

#define CONTROL_HONEY 0
#define CONTROL_GOODHEAD 7
#define CONTROL_EXT 8
#define CONTROL_STYLES 9

#define SAVE_OPTION_CONTROLTYPE 0x0700
#define SAVE_OPTION_EXT 0x1000
#define SAVE_OPTION_STYLE_SAVED 0x2000

#define CONTROLLER_OPTIONS_INDEX_STYLE 0
#define CONTROLLER_OPTIONS_INDEX_INPUTS 1

#define LOPTIONS 0x2B
#define OPTION_STRING(slot) ((LOPTIONS * 0x400) + (slot))
#define OPTION_STR_09_11HONEY_LF 0x09
#define OPTION_STR_32_CONTROLSTYLE_LF 0x32
#define OPTION_STR_33_CONTROLLER_LF 0x33

// The watch's left margin, and the text's line height in the list, by the font's size.
#define WATCH_XOFFSET 64
#define LINE_HEIGHT() (j_text_trigger ? 14 : 10)

extern s32 j_text_trigger;
extern s32 watch_item_is_actively_selected;
extern u32 controller_options_index;
extern s32 D_800409D8;
extern s32 selected_folder_num;
extern void* ptrFontBankGothicChars;
extern void* ptrFontBankGothic;

char* langGet(s32 id);
Gfx* microcode_constructor(Gfx* gdl);
void textMeasure(s32* textheight, s32* textwidth, char* text, void* chars, void* font, s32 lineheight);
Gfx* textRender(Gfx* gdl, s32* x, s32* y, char* text, void* chars, void* font, u32 colour, s32 width, s32 height,
                u32 yOffset, s32 lineheight);
Gfx* textRenderOutlined(Gfx* gdl, s32* x, s32* y, char* text, void* chars, void* font, u32 colour, u32 colour2,
                        s32 width, s32 height, s32 yOffset, s32 lineheight);
Gfx* draw_options_labels(Gfx* gdl, s32 x, s32 y, char* text, u32 colour, s32 outlined, u32 outlinecolour, s32 centre,
                         s32 drawbg, u32 bgcolour, s32 rightalign);
Gfx* draw_background_health_and_armor(Gfx* gdl, Mtx* arg1, s32 zoom_squish);
Gfx* draw_watch_controller(Gfx* gdl);
s32 check_watch_page_transistion_running(void);
void sub_GAME_7F0A611C(f32* pos, s32* index, s32 count, s32* vspacing, s32* previndex, s32* hasset, s32 arg6, s32 arg7,
                       s32 lineheight);
void reset_watch_item_is_actively_selected(void);
void bondinvDetermineEquippedItem(void);
void cur_player_set_control_type(int type);
void fileLoadSettingsForFolder(u32 folder);
void fileClearSavefileForFolder(s32 folder);
save_data* fileGetSaveForFoldernum(u32 folder);
void fileSetSaveFoldernum(save_data* save, u32 folder);
void fileSaveSettingsForFolder(save_data* save);
void fileCheckSaveStageDifficultyTime(save_data* folder, LEVEL_SOLO_SEQUENCE levelid, DIFFICULTY difficulty,
                                      s32 newtime);
void fileOverwriteSaveSlotWithNewSave(save_data* save1, save_data* save2);
u32 controllerCheckDualControllerTypesAllowed(void);

extern u32 watch_screen_index;
extern s32 mission_brief_index;
extern f32 D_800409C8;
extern f32 D_800409CC;
#define WATCH_INDEX_MISSION_STATUS 0
#define BRIEF_INDEX_OBJECTIVES 4

// BLANKSAVEDATA: a save the game makes new, its reset flag set, Brosnan its Bond and DEFAULT_OPTIONS its options.
#define SAVEFLAG_DORESET 0x80
#define DEFAULT_SAVE_OPTIONS 0x003A

static void blank_save(save_data* save, u32 folder) {
    u8* bytes = (u8*) save;
    u32 i;
    for (i = 0; i < sizeof(save_data); i++) {
        bytes[i] = 0;
    }
    save->completion_bitflags = SAVEFLAG_DORESET;
    save->music_vol = 0xFF;
    save->sfx_vol = 0xFF;
    save->options = DEFAULT_SAVE_OPTIONS;
    fileSetSaveFoldernum(save, folder);
}

static s32 saves_differ(const save_data* a, const save_data* b) {
    const u8* x = (const u8*) a;
    const u8* y = (const u8*) b;
    u32 i;
    for (i = 0; i < sizeof(save_data); i++) {
        if (x[i] != y[i]) {
            return TRUE;
        }
    }
    return FALSE;
}

// Every reader of the style but the watch's list and the save's gets 2.4 for Ext.
RECOMP_PATCH int cur_player_get_control_type(void) {
    // @recomp
    return PLAYER_CONTROL_TYPE == CONTROL_EXT ? CONTROL_GOODHEAD : PLAYER_CONTROL_TYPE;
}

s32 ext_plays(void) {
    return PLAYER_CONTROL_TYPE == CONTROL_EXT;
}

// The style a save keeps, by the bits above.
static s32 ext_saved_style(u32 folder, const save_data* save) {
    LEVEL_SOLO_SEQUENCE level;
    DIFFICULTY difficulty;
    s32 style = (save->options & SAVE_OPTION_CONTROLTYPE) >> 8;

    if (save->options & SAVE_OPTION_EXT) {
        return CONTROL_EXT;
    }
    if ((save->options & SAVE_OPTION_STYLE_SAVED) || style != CONTROL_HONEY) {
        return style;
    }
    // SP_LEVEL_DAM - 1 when none is: signed, as the enum's type is unsigned.
    fileGetHighestStageDifficultyCompletedForFolder(folder, &level, &difficulty);
    return (s32) level >= SP_LEVEL_DAM ? style : CONTROL_EXT;
}

// At the start of each stage: the folder's settings, and Ext as the save keeps it.
RECOMP_PATCH void fileLoadSaveSettingsForSelectedFolder(int stage) {
    save_data* save;

    fileLoadSettingsForFolder(selected_folder_num);

    // @recomp
    if (getPlayerCount() == 1) {
        save = fileGetSaveForFoldernum(selected_folder_num);
        if (save == NULL || ext_saved_style(selected_folder_num, save) == CONTROL_EXT) {
            cur_player_set_control_type(CONTROL_EXT);
        }
    }
}

// fileClearSavefileForFolder, as the watch closes and a mission is aborted: the folder's settings into its save, with
// Ext's bits.
RECOMP_PATCH void deleteCurrentSelectedFolder(void) {
    s32 folder = selected_folder_num;

    if (folder >= 0 && folder < 4) {
        save_data* save = fileGetSaveForFoldernum(folder);
        save_data save_to_copy;
        save_data new_save;

        if (save) {
            save_to_copy = *save;
        } else {
            blank_save(&save_to_copy, folder);
        }

        new_save = save_to_copy;

        fileSaveSettingsForFolder(&new_save);

        // @recomp: the style as above, which fileSaveSettingsForFolder saves as 2.4 for Ext.
        new_save.options &= ~(SAVE_OPTION_CONTROLTYPE | SAVE_OPTION_EXT | SAVE_OPTION_STYLE_SAVED);
        if (getPlayerCount() == 1 && ext_plays()) {
            new_save.options |= SAVE_OPTION_EXT;
        } else {
            new_save.options |= ((u16) (cur_player_get_control_type() << 8) & SAVE_OPTION_CONTROLTYPE) |
                                SAVE_OPTION_STYLE_SAVED;
        }

        if (saves_differ(&new_save, &save_to_copy)) {
            fileOverwriteSaveSlotWithNewSave(save, &new_save);
        }
    }
}

// A completed mission's time into the folder's save, made blank if the folder has none: then with Ext, as the folder
// without a save played it (fileLoadSaveSettingsForSelectedFolder) until the watch closed, which would have saved it.
RECOMP_PATCH void fileUnlockStageInFolderAtDifficulty(s32 foldernum, LEVEL_SOLO_SEQUENCE stage, DIFFICULTY difficulty,
                                                      s32 newtime) {
    if ((foldernum >= 0) && (foldernum < 4) && (stage >= SP_LEVEL_DAM) && (stage < SP_LEVEL_MAX) &&
        (difficulty >= DIFFICULTY_AGENT) && (difficulty < DIFFICULTY_MAX)) {
        save_data new_save;
        save_data* save = fileGetSaveForFoldernum(foldernum);
        s32 i;

        if (save) {
            new_save = *save;
        } else {
            blank_save(&new_save, foldernum);
            // @recomp
            new_save.options |= SAVE_OPTION_EXT;
        }

        for (i = difficulty; i >= DIFFICULTY_AGENT; i--) {
            if (i == difficulty) {
                fileCheckSaveStageDifficultyTime(&new_save, stage, i, newtime);
            } else {
                fileCheckSaveStageDifficultyTime(&new_save, stage, i, 99999999);
            }
        }

        fileOverwriteSaveSlotWithNewSave(&save[0], &new_save);
    }
}

// As the watch comes up: how many styles its list has, 1.1 to 1.4 with one controller, and with two 2.1 to 2.4 and,
// @recomp, Ext when one player plays.
RECOMP_PATCH void sub_GAME_7F0A69A8(void) {
    if (joyGetControllerCount() < 2) {
        D_800409D8 = 4;
    } else if (getPlayerCount() == 1) {
        D_800409D8 = CONTROL_STYLES;
    } else {
        D_800409D8 = 8;
    }
    reset_watch_item_is_actively_selected();
    watch_screen_index = WATCH_INDEX_MISSION_STATUS;
    mission_brief_index = BRIEF_INDEX_OBJECTIVES;
    D_800409C8 = 0.999f;
    D_800409CC = 0.9999f;
    bondinvDetermineEquippedItem();
}

static const char ext_name[] = "ext\n";

// A style's name in the list, as the game keeps them: its game_control_styles entry's first, or Ext's.
static char* style_name(s32 style) {
    return style == CONTROL_EXT ? (char*) ext_name : langGet(OPTION_STRING(OPTION_STR_09_11HONEY_LF + style));
}

static void append(char* text, s32 size, const char* more) {
    s32 len = 0;
    while (text[len] != '\0') {
        len++;
    }
    while (*more != '\0' && len < size - 1) {
        text[len++] = *more++;
    }
    text[len] = '\0';
}

// The watch's list of styles, the selected one lit: the original, with Ext's name after 2.4's.
RECOMP_PATCH Gfx* draw_controller_style_text(Gfx* gdl) {
    char text[2000];
    s32 x;
    s32 y;
    s32 i;
    s32 textheight = 0;
    s32 textwidth = 0;
    void* font = ptrFontBankGothic;
    void* chars = ptrFontBankGothicChars;
    char* selectedtext;

    text[0] = '\0';

    for (i = 0; i < D_800409D8; i++) {
        append(text, sizeof(text), style_name(i));
    }

    if (watch_item_is_actively_selected && controller_options_index == CONTROLLER_OPTIONS_INDEX_STYLE) {
        sub_GAME_7F0A611C(&PLAYER_CONTROL_TYPE_2, &PLAYER_CONTROL_TYPE, D_800409D8, &PLAYER_CONTROL_TYPE_VSPACING,
                          &PLAYER_CONTROL_TYPE_1, (s32*) &PLAYER_HAS_SET_CONTROL_TYPE, 0, 1, LINE_HEIGHT());
    }

    x = 0xaa;
    y = 0x1a;

    textMeasure(&textheight, &textwidth, text, chars, font, LINE_HEIGHT());

    gdl = textRender(gdl, &x, &y, text, chars, font, 0x00aa00b0, textwidth, LINE_HEIGHT(), PLAYER_CONTROL_TYPE_VSPACING,
                     LINE_HEIGHT());

    if (PLAYER_HAS_SET_CONTROL_TYPE != 0) {
        selectedtext = style_name(PLAYER_CONTROL_TYPE);

        textMeasure(&textheight, &textwidth, selectedtext, chars, font, LINE_HEIGHT());

        x = 0xaa;
        y = 0x1a;

        gdl = textRender(gdl, &x, &y, selectedtext, chars, font, 0xa0ffa0f0, textwidth, 0x64, 0, LINE_HEIGHT());
    }

    return gdl;
}

// Ext's buttons, as the watch lists them where it draws the N64 pads for the other styles.
static const char* const ext_labels[] = {
    "right stick: look\n",
    "left stick: move\n",
    "rt: fire\n",
    "lt: aim\n",
    "a or rb: action\n",
    "lb: weapon\n",
    "y / b: next / last weapon\n",
    "x: reload\n",
    "left stick click: crouch\n",
    "start: pause\n",
};

#define EXT_LABELS_Y 0x40
#define EXT_LABELS_YINC 12

// The watch's Control page: the original, but for Ext, which has its buttons as text in place of the pads, and one
// controller.
RECOMP_PATCH Gfx* draw_watch_control_options_page(Gfx* gdl, Mtx* param_2) {
    s32 colour;
    char* textptr;
    s32 x;
    s32 y;
    s32 textwidth;
    s32 textheight;
    void* font;
    void* chars;
    // @recomp
    s32 ext = ext_plays();
    s32 i;

    gdl = draw_background_health_and_armor(gdl, param_2, 0);

    if (check_watch_page_transistion_running() != 1) {
        // @recomp
        if (!ext) {
            gdl = draw_watch_controller(gdl);
        }
        font = ptrFontBankGothic;
        chars = ptrFontBankGothicChars;

        gdl = microcode_constructor(gdl);

        // @recomp
        if (ext) {
            for (i = 0; i < (s32) (sizeof(ext_labels) / sizeof(ext_labels[0])); i++) {
                gdl = draw_options_labels(gdl, WATCH_XOFFSET, EXT_LABELS_Y + i * EXT_LABELS_YINC, (char*) ext_labels[i],
                                          0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0);
            }
        }

        textptr = langGet(OPTION_STRING(OPTION_STR_32_CONTROLSTYLE_LF));

        x = WATCH_XOFFSET;
        y = 0x1A;
        colour = 0xFF00B0;
        if (controller_options_index == CONTROLLER_OPTIONS_INDEX_STYLE) {
            colour = 0xA0FFA0F0;
            if (watch_item_is_actively_selected != 0) {
                colour = -1;
            }
        }

        textMeasure(&textheight, &textwidth, textptr, chars, font, 0);

        if ((watch_item_is_actively_selected != 0) && (controller_options_index == CONTROLLER_OPTIONS_INDEX_STYLE)) {
            gdl = textRenderOutlined(gdl, &x, &y, textptr, chars, font, colour, 0x7000A0, textwidth + 1, textheight, 0,
                                     0);
        } else {
            gdl = textRender(gdl, &x, &y, textptr, chars, font, colour, textwidth, textheight, 0, 0);
        }

        gdl = draw_controller_style_text(gdl);
        textptr = langGet(OPTION_STRING(OPTION_STR_33_CONTROLLER_LF));

        colour = 0xFF00B0;

        // @recomp: "controllers" for 2.1 to 2.4, but not Ext
        if (controllerCheckDualControllerTypesAllowed() && !ext) {
            textptr = langGet(OPTION_STRING(OPTION_STR_33_CONTROLLER_LF + 1));
        }
        x = WATCH_XOFFSET;
        y = 0x2B;

        if (controller_options_index == CONTROLLER_OPTIONS_INDEX_INPUTS) {
            colour = 0xA0FFA0F0;
            if (watch_item_is_actively_selected != 0) {
                colour = -1;
            }
        }

        textMeasure(&textheight, &textwidth, textptr, chars, font, 0);

        if ((watch_item_is_actively_selected != 0) && (controller_options_index == CONTROLLER_OPTIONS_INDEX_INPUTS)) {
            gdl = textRenderOutlined(gdl, &x, &y, textptr, chars, font, colour, 0x7000A0, textwidth + 1, textheight, 0,
                                     0);
        } else {
            gdl = textRender(gdl, &x, &y, textptr, chars, font, colour, textwidth, textheight, 0, 0);
        }
    }
    return gdl;
}
