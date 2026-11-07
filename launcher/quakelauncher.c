/*
 * QuakeLauncher -- the start window of GLQuake for R200 68k (the RadeonGL
 * edition of GLQuake for minigl.library).
 *
 * It shows the GLQuake build that is installed beside it (the Installer
 * puts in the one for this processor: GLQuake for a 68060, GLQuake040 for
 * a 68040; of an older installation with both, the one for this processor
 * is taken), offers the start options in a ReAction
 * window, keeps them in PROGDIR:QuakeLauncher.prefs and starts the game in
 * its own drawer, beside Quake's id1 drawer. While the game runs the window
 * is closed; it comes back when the game ends.
 *
 * C89 for SAS/C 6.58 and m68k-amigaos-gcc; 68000 code, so that it can tell
 * the owner of a slower processor why GLQuake will not run. AmigaOS 3.2
 * (the ReAction classes of 3.2; window.class V47 for the centred window).
 *
 * Written 2026-11-07 (GLQuake for R200 68k 1.2).
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/var.h>
#include <intuition/intuition.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <graphics/displayinfo.h>
#include <libraries/gadtools.h>
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <gadgets/chooser.h>
#include <gadgets/checkbox.h>
#include <gadgets/integer.h>
#include <gadgets/string.h>
#include <images/label.h>
#include <images/bevel.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/window.h>
#include <proto/layout.h>
#include <proto/button.h>
#include <proto/chooser.h>
#include <proto/checkbox.h>
#include <proto/integer.h>
#include <proto/string.h>
#include <proto/label.h>

#include "rglq_version.h"

#define LAUNCHER_NAME "QuakeLauncher"

static const char verstag[] = "$VER: " LAUNCHER_NAME " " RGLQ_VERSION " (" RGLQ_DATE ")";

/* Every base = NULL: an uninitialised one pulls GCC's libnix auto-open stubs,
 * which guess wrong names for the ReAction classes; and utility.library is
 * opened first, because libnix's 32-bit multiply and divide call it. */
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;
struct Library *UtilityBase = NULL;
struct Library *WindowBase = NULL;
struct Library *LayoutBase = NULL;
struct Library *ButtonBase = NULL;
struct Library *ChooserBase = NULL;
struct Library *CheckBoxBase = NULL;
struct Library *IntegerBase = NULL;
struct Library *StringBase = NULL;
struct Library *LabelBase = NULL;

extern struct ExecBase *SysBase;

#define GID_GAME     2
#define GID_SIZE     3
#define GID_DEPTH    4
#define GID_VSYNC    5
#define GID_SOUND    6
#define GID_MEMORY   7
#define GID_CHECK    8
#define GID_ARGS     9
#define GID_PLAY    10
#define GID_SAVE    11
#define GID_QUIT    12
#define MID_ABOUT   20

#define MAX_SIZES   48
#define MAX_GAMES   24
#define ARGS_LEN   200
#define CMD_LEN    480

#define PREFS_FILE "PROGDIR:QuakeLauncher.prefs"

struct Size
{
    UWORD w, h;
    char label[16];
};

struct Game
{
    char dir[32];       /* the drawer beside id1, "id1" for Quake itself */
    char label[48];
};

struct Settings
{
    UWORD width, height;    /* 0 x 0: the size the game saved (Video Options) */
    LONG bpp;               /* 16 or 32 */
    LONG vsync;
    LONG sound;
    LONG memory;            /* MB of game memory (-mem) */
    LONG check;             /* RadeonGL's command check */
    char game[32];
    char args[ARGS_LEN];
};

static struct Size sizes[MAX_SIZES];
static LONG nsizes;
static struct Game games[MAX_GAMES];
static LONG ngames;
static const char *program;         /* the build to start, NULL: none installed */
static char programline[64];
static char cpuline[64];
static BOOL cpu_ok;                 /* a 68040 or 68060 with FPU */
static BOOL is060;
static BOOL mismatch;               /* the 68060 build on another processor */
static struct Settings set;

static struct List gamelist, sizelist, depthlist;
static Object *winobj = NULL;
static Object *g_game, *g_size, *g_depth, *g_vsync, *g_sound, *g_memory, *g_check, *g_args;
static struct Window *win = NULL;

static struct NewMenu menus[] =
{
    { NM_TITLE, "Project",        NULL, 0, 0, NULL },
    { NM_ITEM,  "Play",           "P",  0, 0, (APTR) GID_PLAY },
    { NM_ITEM,  "Save settings",  "S",  0, 0, (APTR) GID_SAVE },
    { NM_ITEM,  NM_BARLABEL,      NULL, 0, 0, NULL },
    { NM_ITEM,  "About...",       "?",  0, 0, (APTR) MID_ABOUT },
    { NM_ITEM,  NM_BARLABEL,      NULL, 0, 0, NULL },
    { NM_ITEM,  "Quit",           "Q",  0, 0, (APTR) GID_QUIT },
    { NM_END,   NULL,             NULL, 0, 0, NULL }
};

static const char *depthnames[] = { "16 bit", "32 bit (finer)", NULL };

/* --- small string helpers (no C library) --------------------------------- */

static ULONG Len(const char *s)
{
    ULONG n;

    n = 0;
    while (s[n] != 0)
        n++;
    return n;
}

static void Copy(char *dst, ULONG size, const char *src)
{
    ULONG i;

    if (size == 0)
        return;
    for (i = 0; i + 1 < size && src[i] != 0; i++)
        dst[i] = src[i];
    dst[i] = 0;
}

static void Append(char *dst, ULONG size, const char *src)
{
    ULONG n;

    n = Len(dst);
    if (n < size)
        Copy(dst + n, size - n, src);
}

static void AppendNum(char *dst, ULONG size, LONG v)
{
    char num[12];
    ULONG i, j;
    char t;

    i = 0;
    if (v < 0)
        v = 0;
    do
    {
        num[i++] = (char) ('0' + v % 10);
        v /= 10;
    } while (v != 0 && i < sizeof(num) - 1);
    num[i] = 0;
    for (j = 0; j < i / 2; j++)
    {
        t = num[j];
        num[j] = num[i - 1 - j];
        num[i - 1 - j] = t;
    }
    Append(dst, size, num);
}

static BOOL HasSpace(const char *s)
{
    while (*s != 0)
        if (*s++ == ' ')
            return TRUE;
    return FALSE;
}

static BOOL Exists(const char *path)
{
    BPTR l;

    l = Lock((STRPTR) path, SHARED_LOCK);
    if (l == 0)
        return FALSE;
    UnLock(l);
    return TRUE;
}

/* --- the processor and the builds ---------------------------------------- */

static void CheckCpu(void)
{
    UWORD f;
    const char *cpu;
    const char *fpu;

    f = SysBase->AttnFlags;
    is060 = (f & AFF_68060) ? TRUE : FALSE;
    if (f & AFF_68060)
        cpu = "68060";
    else if (f & AFF_68040)
        cpu = "68040";
    else if (f & AFF_68030)
        cpu = "68030";
    else if (f & AFF_68020)
        cpu = "68020";
    else if (f & AFF_68010)
        cpu = "68010";
    else
        cpu = "68000";
    if (f & AFF_FPU40)
        fpu = " with FPU";
    else if (f & (AFF_68881 | AFF_68882))
        fpu = " with a 6888x FPU";
    else
        fpu = " without FPU";
    cpu_ok = ((f & (AFF_68040 | AFF_68060)) && (f & AFF_FPU40)) ? TRUE : FALSE;
    Copy(cpuline, sizeof(cpuline), cpu);
    Append(cpuline, sizeof(cpuline), fpu);
    if (!cpu_ok)
        Append(cpuline, sizeof(cpuline), " (too slow)");
}

/* The build beside the launcher: the Installer puts in the one for this
 * processor. With both there (an older installation), the one for this
 * processor; the 68040 build runs on a 68060 as well. */
static void FindProgram(void)
{
    BOOL have060, have040;

    have060 = Exists("PROGDIR:GLQuake");
    have040 = Exists("PROGDIR:GLQuake040");
    program = NULL;
    mismatch = FALSE;
    if (have060 && (is060 || !have040))
        program = "GLQuake";
    else if (have040)
        program = "GLQuake040";
    if (program == NULL)
        Copy(programline, sizeof(programline), "none installed");
    else if (program[7] == 0)
    {
        Copy(programline, sizeof(programline), "GLQuake, for a 68060");
        if (!is060)
        {
            mismatch = TRUE;
            Append(programline, sizeof(programline), " (not this one)");
        }
    }
    else
        Copy(programline, sizeof(programline), "GLQuake040, for a 68040");
}

/* --- the games: id1 and the drawers beside it with Quake data ------------ */

static void AddGame(const char *dir, const char *label)
{
    if (ngames >= MAX_GAMES)
        return;
    Copy(games[ngames].dir, sizeof(games[ngames].dir), dir);
    Copy(games[ngames].label, sizeof(games[ngames].label), label);
    ngames++;
}

static void FindGames(void)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    char path[80];
    char label[48];

    ngames = 0;
    AddGame("id1", "Quake");
    lock = Lock("PROGDIR:", SHARED_LOCK);
    if (lock == 0)
        return;
    fib = (struct FileInfoBlock *) AllocDosObject(DOS_FIB, NULL);
    if (fib != NULL && Examine(lock, fib))
    {
        while (ExNext(lock, fib) && ngames < MAX_GAMES)
        {
            if (fib->fib_DirEntryType <= 0 || Stricmp(fib->fib_FileName, "id1") == 0
                || Len(fib->fib_FileName) >= sizeof(games[0].dir))
                continue;
            Copy(path, sizeof(path), "PROGDIR:");
            Append(path, sizeof(path), fib->fib_FileName);
            Append(path, sizeof(path), "/pak0.pak");
            if (!Exists(path))
            {
                Copy(path, sizeof(path), "PROGDIR:");
                Append(path, sizeof(path), fib->fib_FileName);
                Append(path, sizeof(path), "/progs.dat");
                if (!Exists(path))
                    continue;
            }
            if (Stricmp(fib->fib_FileName, "hipnotic") == 0)
                Copy(label, sizeof(label), "Mission Pack 1 (hipnotic)");
            else if (Stricmp(fib->fib_FileName, "rogue") == 0)
                Copy(label, sizeof(label), "Mission Pack 2 (rogue)");
            else
                Copy(label, sizeof(label), fib->fib_FileName);
            AddGame(fib->fib_FileName, label);
        }
    }
    if (fib != NULL)
        FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
}

/* --- the screen sizes: the display database's RTG modes ------------------ */

/* Hi- and true-colour modes (depth 15 and up: the graphics card's) from
 * 5:4 to 16:9, each size once, sorted -- the same choice as GLQuake's
 * Video Modes page. Unavailable entries are skipped. */
static void FindSizes(void)
{
    ULONG id;
    struct DimensionInfo dims;
    LONG w, h, i, j;

    nsizes = 0;
    for (id = NextDisplayInfo(INVALID_ID); id != (ULONG) INVALID_ID; id = NextDisplayInfo(id))
    {
        if (ModeNotAvailable(id) != 0)
            continue;
        if (GetDisplayInfoData(NULL, (UBYTE *) &dims, sizeof(dims), DTAG_DIMS, id) == 0)
            continue;
        if (dims.MaxDepth < 15)
            continue;
        w = dims.Nominal.MaxX - dims.Nominal.MinX + 1;
        h = dims.Nominal.MaxY - dims.Nominal.MinY + 1;
        if (w < 320 || h < 200 || w > 2048 || h > 2048)
            continue;
        if (w * 10 < h * 12 || w * 10 > h * 19)
            continue;
        for (i = 0; i < nsizes; i++)
            if (sizes[i].w == w && sizes[i].h == h)
                break;
        if (i < nsizes || nsizes >= MAX_SIZES)
            continue;
        /* sorted by width, then height */
        for (i = nsizes; i > 0 && (sizes[i - 1].w > w || (sizes[i - 1].w == w && sizes[i - 1].h > h)); i--)
            sizes[i] = sizes[i - 1];
        sizes[i].w = (UWORD) w;
        sizes[i].h = (UWORD) h;
        nsizes++;
    }
    for (j = 0; j < nsizes; j++)
    {
        sizes[j].label[0] = 0;
        AppendNum(sizes[j].label, sizeof(sizes[j].label), sizes[j].w);
        Append(sizes[j].label, sizeof(sizes[j].label), " x ");
        AppendNum(sizes[j].label, sizeof(sizes[j].label), sizes[j].h);
    }
}

/* --- settings ------------------------------------------------------------- */

static void DefaultSettings(void)
{
    set.width = 0;
    set.height = 0;
    set.bpp = 16;
    set.vsync = 1;
    set.sound = 1;
    set.memory = 16;
    set.check = 0;
    Copy(set.game, sizeof(set.game), "id1");
    set.args[0] = 0;
}

static LONG Number(const char *s)
{
    LONG v;

    v = 0;
    if (StrToLong((STRPTR) s, &v) <= 0)
        return 0;
    return v;
}

static void LoadSettings(void)
{
    BPTR f;
    char line[ARGS_LEN + 16];
    char *val;
    ULONG n;
    LONG v;

    f = Open(PREFS_FILE, MODE_OLDFILE);
    if (f == 0)
        return;
    while (FGets(f, line, sizeof(line)) != NULL)
    {
        n = Len(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = 0;
        if (line[0] == ';' || line[0] == 0)
            continue;
        for (val = line; *val != 0 && *val != '='; val++)
            ;
        if (*val != '=')
            continue;
        *val++ = 0;
        if (Stricmp(line, "SIZE") == 0)
        {
            set.width = (UWORD) Number(val);
            while (*val >= '0' && *val <= '9')
                val++;
            set.height = (*val == 'x') ? (UWORD) Number(val + 1) : 0;
            if (set.width == 0 || set.height == 0)
                set.width = set.height = 0;
        }
        else if (Stricmp(line, "BPP") == 0)
            set.bpp = (Number(val) > 16) ? 32 : 16;
        else if (Stricmp(line, "VSYNC") == 0)
            set.vsync = Number(val) ? 1 : 0;
        else if (Stricmp(line, "SOUND") == 0)
            set.sound = Number(val) ? 1 : 0;
        else if (Stricmp(line, "MEMORY") == 0)
        {
            v = Number(val);
            set.memory = (v >= 8 && v <= 256) ? v : 16;
        }
        else if (Stricmp(line, "CHECK") == 0)
            set.check = Number(val) ? 1 : 0;
        else if (Stricmp(line, "GAME") == 0)
            Copy(set.game, sizeof(set.game), val);
        else if (Stricmp(line, "ARGS") == 0)
            Copy(set.args, sizeof(set.args), val);
    }
    Close(f);
}

static BOOL SaveSettings(void)
{
    BPTR f;
    char line[ARGS_LEN + 16];

    f = Open(PREFS_FILE, MODE_NEWFILE);
    if (f == 0)
        return FALSE;
    FPuts(f, "; QuakeLauncher's settings (GLQuake for R200 68k)\n");
    Copy(line, sizeof(line), "SIZE=");
    AppendNum(line, sizeof(line), set.width);
    Append(line, sizeof(line), "x");
    AppendNum(line, sizeof(line), set.height);
    Append(line, sizeof(line), "\nBPP=");
    AppendNum(line, sizeof(line), set.bpp);
    Append(line, sizeof(line), "\nVSYNC=");
    AppendNum(line, sizeof(line), set.vsync);
    Append(line, sizeof(line), "\nSOUND=");
    AppendNum(line, sizeof(line), set.sound);
    Append(line, sizeof(line), "\nMEMORY=");
    AppendNum(line, sizeof(line), set.memory);
    Append(line, sizeof(line), "\nCHECK=");
    AppendNum(line, sizeof(line), set.check);
    Append(line, sizeof(line), "\n");
    FPuts(f, line);
    Copy(line, sizeof(line), "GAME=");
    Append(line, sizeof(line), set.game);
    Append(line, sizeof(line), "\n");
    FPuts(f, line);
    Copy(line, sizeof(line), "ARGS=");
    Append(line, sizeof(line), set.args);
    Append(line, sizeof(line), "\n");
    FPuts(f, line);
    Close(f);
    return TRUE;
}

/* --- requesters ------------------------------------------------------------ */

static void Tell(const char *text)
{
    struct EasyStruct es;

    es.es_StructSize = sizeof(es);
    es.es_Flags = 0;
    es.es_Title = (UBYTE *) LAUNCHER_NAME;
    es.es_TextFormat = (UBYTE *) text;
    es.es_GadgetFormat = (UBYTE *) "OK";
    EasyRequestArgs(win, &es, NULL, NULL);
}

/* --- the window ------------------------------------------------------------- */

static BOOL MakeLabels(struct List *l, const char **names, LONG n)
{
    struct Node *node;
    LONG i;

    NewList(l);
    for (i = 0; i < n; i++)
    {
        node = AllocChooserNode(CNA_Text, (ULONG) names[i], TAG_DONE);
        if (node == NULL)
            return FALSE;
        AddTail(l, node);
    }
    return TRUE;
}

static void FreeLabels(struct List *l)
{
    struct Node *node;

    if (l->lh_Head == NULL)
        return;
    while ((node = RemHead(l)) != NULL)
        FreeChooserNode(node);
}

static Object *Label(const char *text)
{
    return (Object *) NewObject(LABEL_GetClass(), NULL, LABEL_Text, (ULONG) text, TAG_DONE);
}

/* A titled section holding one inner layout (the leaf gadgets never sit
 * directly in a labelled BVS_GROUP: layout.gadget trap, see the GUI notes) */
static Object *Section(const char *title, Object *inner)
{
    if (inner == NULL)
        return NULL;
    return (Object *) NewObject(LAYOUT_GetClass(), NULL,
                                LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
                                LAYOUT_BevelStyle, BVS_GROUP,
                                LAYOUT_Label, (ULONG) title,
                                LAYOUT_SpaceInner, TRUE,
                                LAYOUT_SpaceOuter, TRUE,
                                LAYOUT_AddChild, (ULONG) inner,
                                TAG_DONE);
}

static LONG SizeIndex(void)
{
    LONG i;

    if (set.width == 0)
        return 0;
    for (i = 0; i < nsizes; i++)
        if (sizes[i].w == set.width && sizes[i].h == set.height)
            return i + 1;
    return 0;
}

static LONG GameIndex(void)
{
    LONG i;

    for (i = 0; i < ngames; i++)
        if (Stricmp(games[i].dir, set.game) == 0)
            return i;
    return 0;
}

static Object *BuildWindow(void)
{
    const char *names[MAX_SIZES + 2];
    Object *game, *screen, *start, *rgl, *argsec, *buttons, *root;
    LONG i;

    for (i = 0; i < ngames; i++)
        names[i] = games[i].label;
    if (!MakeLabels(&gamelist, names, ngames))
        return NULL;
    names[0] = "As in the game";
    for (i = 0; i < nsizes; i++)
        names[i + 1] = sizes[i].label;
    if (!MakeLabels(&sizelist, names, nsizes + 1))
        return NULL;
    if (!MakeLabels(&depthlist, depthnames, 2))
        return NULL;

    g_game = (Object *) NewObject(CHOOSER_GetClass(), NULL,
                                  GA_ID, GID_GAME, GA_RelVerify, TRUE,
                                  CHOOSER_Labels, (ULONG) &gamelist,
                                  CHOOSER_Selected, GameIndex(),
                                  TAG_DONE);
    game = (Object *) NewObject(LAYOUT_GetClass(), NULL,
                                LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
                                LAYOUT_AddImage, (ULONG) Label(programline),
                                CHILD_Label, (ULONG) Label("Program"),
                                CHILD_WeightedHeight, 0,
                                LAYOUT_AddImage, (ULONG) Label(cpuline),
                                CHILD_Label, (ULONG) Label("Amiga"),
                                CHILD_WeightedHeight, 0,
                                LAYOUT_AddChild, (ULONG) g_game,
                                CHILD_Label, (ULONG) Label("Game"),
                                TAG_DONE);

    g_size = (Object *) NewObject(CHOOSER_GetClass(), NULL,
                                  GA_ID, GID_SIZE, GA_RelVerify, TRUE,
                                  CHOOSER_Labels, (ULONG) &sizelist,
                                  CHOOSER_Selected, SizeIndex(),
                                  CHOOSER_MaxLabels, 24,
                                  TAG_DONE);
    g_depth = (Object *) NewObject(CHOOSER_GetClass(), NULL,
                                   GA_ID, GID_DEPTH, GA_RelVerify, TRUE,
                                   CHOOSER_Labels, (ULONG) &depthlist,
                                   CHOOSER_Selected, (set.bpp == 32) ? 1 : 0,
                                   TAG_DONE);
    g_vsync = (Object *) NewObject(CHECKBOX_GetClass(), NULL,
                                   GA_ID, GID_VSYNC, GA_RelVerify, TRUE,
                                   GA_Text, (ULONG) "Vertical sync",
                                   GA_Selected, set.vsync ? TRUE : FALSE,
                                   TAG_DONE);
    screen = (Object *) NewObject(LAYOUT_GetClass(), NULL,
                                  LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
                                  LAYOUT_AddChild, (ULONG) g_size,
                                  CHILD_Label, (ULONG) Label("Size"),
                                  LAYOUT_AddChild, (ULONG) g_depth,
                                  CHILD_Label, (ULONG) Label("Colours"),
                                  LAYOUT_AddChild, (ULONG) g_vsync,
                                  CHILD_Label, (ULONG) Label(""),
                                  TAG_DONE);

    g_sound = (Object *) NewObject(CHECKBOX_GetClass(), NULL,
                                   GA_ID, GID_SOUND, GA_RelVerify, TRUE,
                                   GA_Text, (ULONG) "Sound",
                                   GA_Selected, set.sound ? TRUE : FALSE,
                                   TAG_DONE);
    g_memory = (Object *) NewObject(INTEGER_GetClass(), NULL,
                                    GA_ID, GID_MEMORY, GA_RelVerify, TRUE,
                                    INTEGER_Number, set.memory,
                                    INTEGER_Minimum, 8,
                                    INTEGER_Maximum, 256,
                                    INTEGER_MaxChars, 3,
                                    INTEGER_Arrows, TRUE,
                                    TAG_DONE);
    start = (Object *) NewObject(LAYOUT_GetClass(), NULL,
                                 LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
                                 LAYOUT_AddChild, (ULONG) g_sound,
                                 CHILD_Label, (ULONG) Label(""),
                                 LAYOUT_AddChild, (ULONG) g_memory,
                                 CHILD_Label, (ULONG) Label("Memory (MB)"),
                                 TAG_DONE);

    g_check = (Object *) NewObject(CHECKBOX_GetClass(), NULL,
                                   GA_ID, GID_CHECK, GA_RelVerify, TRUE,
                                   GA_Text, (ULONG) "Command check (slower)",
                                   GA_Selected, set.check ? TRUE : FALSE,
                                   TAG_DONE);
    rgl = (Object *) NewObject(LAYOUT_GetClass(), NULL,
                               LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
                               LAYOUT_AddChild, (ULONG) g_check,
                               TAG_DONE);

    g_args = (Object *) NewObject(STRING_GetClass(), NULL,
                                  GA_ID, GID_ARGS, GA_RelVerify, TRUE,
                                  STRINGA_MaxChars, ARGS_LEN - 1,
                                  STRINGA_TextVal, (ULONG) set.args,
                                  TAG_DONE);
    argsec = (Object *) NewObject(LAYOUT_GetClass(), NULL,
                                  LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
                                  LAYOUT_AddChild, (ULONG) g_args,
                                  TAG_DONE);

    buttons = (Object *) NewObject(LAYOUT_GetClass(), NULL,
                                   LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
                                   LAYOUT_EvenSize, TRUE,
                                   LAYOUT_SpaceInner, TRUE,
                                   LAYOUT_AddChild, (ULONG) NewObject(BUTTON_GetClass(), NULL,
                                       GA_ID, GID_PLAY, GA_RelVerify, TRUE,
                                       GA_Text, (ULONG) "Play",
                                       GA_Disabled, (program == NULL) ? TRUE : FALSE,
                                       TAG_DONE),
                                   LAYOUT_AddChild, (ULONG) NewObject(BUTTON_GetClass(), NULL,
                                       GA_ID, GID_SAVE, GA_RelVerify, TRUE,
                                       GA_Text, (ULONG) "Save",
                                       TAG_DONE),
                                   LAYOUT_AddChild, (ULONG) NewObject(BUTTON_GetClass(), NULL,
                                       GA_ID, GID_QUIT, GA_RelVerify, TRUE,
                                       GA_Text, (ULONG) "Quit",
                                       TAG_DONE),
                                   TAG_DONE);

    root = (Object *) NewObject(LAYOUT_GetClass(), NULL,
                                LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
                                LAYOUT_SpaceInner, TRUE,
                                LAYOUT_SpaceOuter, TRUE,
                                LAYOUT_AddChild, (ULONG) NewObject(LAYOUT_GetClass(), NULL,
                                    LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
                                    LAYOUT_SpaceInner, TRUE,
                                    LAYOUT_AddChild, (ULONG) Section("Game", game),
                                    LAYOUT_AddChild, (ULONG) Section("Screen", screen),
                                    TAG_DONE),
                                LAYOUT_AddChild, (ULONG) NewObject(LAYOUT_GetClass(), NULL,
                                    LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
                                    LAYOUT_SpaceInner, TRUE,
                                    LAYOUT_AddChild, (ULONG) Section("Start", start),
                                    LAYOUT_AddChild, (ULONG) Section("RadeonGL", rgl),
                                    TAG_DONE),
                                LAYOUT_AddChild, (ULONG) Section("More arguments", argsec),
                                CHILD_WeightedHeight, 0,
                                LAYOUT_AddChild, (ULONG) buttons,
                                CHILD_WeightedHeight, 0,
                                TAG_DONE);
    if (root == NULL)
        return NULL;

    return (Object *) NewObject(WINDOW_GetClass(), NULL,
                                WA_Title, (ULONG) "GLQuake for R200 68k",
                                WA_ScreenTitle, (ULONG) (verstag + 6),
                                WA_DragBar, TRUE,
                                WA_DepthGadget, TRUE,
                                WA_CloseGadget, TRUE,
                                WA_SizeGadget, TRUE,
                                WA_Activate, TRUE,
                                WA_NewLookMenus, TRUE,
                                WINDOW_Position, WPOS_CENTERSCREEN,
                                WINDOW_NewMenu, (ULONG) menus,
                                WINDOW_ParentGroup, (ULONG) root,
                                TAG_DONE);
}

static void ReadGadgets(void)
{
    ULONG v;
    STRPTR s;

    v = 0;
    GetAttr(CHOOSER_Selected, g_size, &v);
    if (v >= 1 && (LONG) v <= nsizes)
    {
        set.width = sizes[v - 1].w;
        set.height = sizes[v - 1].h;
    }
    else
        set.width = set.height = 0;
    v = 0;
    GetAttr(CHOOSER_Selected, g_depth, &v);
    set.bpp = (v == 1) ? 32 : 16;
    v = 0;
    GetAttr(GA_Selected, g_vsync, &v);
    set.vsync = v ? 1 : 0;
    v = 0;
    GetAttr(GA_Selected, g_sound, &v);
    set.sound = v ? 1 : 0;
    v = 16;
    GetAttr(INTEGER_Number, g_memory, &v);
    set.memory = ((LONG) v >= 8 && (LONG) v <= 256) ? (LONG) v : 16;
    v = 0;
    GetAttr(GA_Selected, g_check, &v);
    set.check = v ? 1 : 0;
    v = 0;
    GetAttr(CHOOSER_Selected, g_game, &v);
    Copy(set.game, sizeof(set.game), ((LONG) v < ngames) ? games[v].dir : "id1");
    s = NULL;
    GetAttr(STRINGA_TextVal, g_args, (ULONG *) &s);
    Copy(set.args, sizeof(set.args), s != NULL ? (const char *) s : "");
}

/* --- starting the game ------------------------------------------------------ */

static void BuildCommand(char *cmd, ULONG size, const char *exe)
{
    Copy(cmd, size, exe);
    Append(cmd, size, " -nocdaudio");
    if (Stricmp(set.game, "hipnotic") == 0)
        Append(cmd, size, " -hipnotic");
    else if (Stricmp(set.game, "rogue") == 0)
        Append(cmd, size, " -rogue");
    else if (Stricmp(set.game, "id1") != 0)
    {
        Append(cmd, size, HasSpace(set.game) ? " -game \"" : " -game ");
        Append(cmd, size, set.game);
        if (HasSpace(set.game))
            Append(cmd, size, "\"");
    }
    if (set.width != 0)
    {
        Append(cmd, size, " -width ");
        AppendNum(cmd, size, set.width);
        Append(cmd, size, " -height ");
        AppendNum(cmd, size, set.height);
    }
    Append(cmd, size, (set.bpp == 32) ? " -bpp 32" : " -bpp 16");
    if (!set.sound)
        Append(cmd, size, " -nosound");
    Append(cmd, size, " -mem ");
    AppendNum(cmd, size, set.memory);
    if (set.args[0] != 0)
    {
        Append(cmd, size, " ");
        Append(cmd, size, set.args);
    }
    /* last: GLQuake runs +commands after its start, in this order */
    Append(cmd, size, set.vsync ? " +gl_videosync 1" : " +gl_videosync 0");
}

/* GLQuake in its drawer (the current directory: id1 beside it), with
 * RadeonGL's command check as a LOCAL variable -- the new shell copies it
 * and nothing is left in ENV:. Errors go to a console window that opens
 * only if the game writes to it (GLQuake's I_Error prints to stdout). */
static void Play(void)
{
    static char cmd[CMD_LEN];
    BPTR lock, old, in, out;
    LONG rc;

    if (!cpu_ok)
    {
        Tell("GLQuake needs a 68040 or 68060 with FPU.\n\n"
             "This Amiga has a slower processor or no FPU.");
        return;
    }
    if (program == NULL)
        return;
    if (mismatch)
    {
        Tell("The GLQuake installed here is the one for a 68060,\n"
             "and this Amiga has a 68040.\n\n"
             "Install GLQuake for R200 68k again on this Amiga:\n"
             "the Installer puts in the build for its processor.");
        return;
    }
    BuildCommand(cmd, sizeof(cmd), program);

    lock = Lock("PROGDIR:", SHARED_LOCK);
    if (lock == 0)
    {
        Tell("The launcher's own drawer cannot be found.");
        return;
    }
    in = Open("NIL:", MODE_OLDFILE);
    out = Open("CON:20/20/600/150/GLQuake/AUTO/CLOSE/WAIT", MODE_NEWFILE);
    if (in == 0 || out == 0)
    {
        if (in != 0)
            Close(in);
        if (out != 0)
            Close(out);
        UnLock(lock);
        Tell("No console for the game could be opened.");
        return;
    }
    SetVar("RadeonGLCheck", set.check ? "1" : "0", -1, GVF_LOCAL_ONLY);
    old = CurrentDir(lock);
    rc = SystemTags(cmd,
                    SYS_Input, in,
                    SYS_Output, out,
                    NP_StackSize, 32768,
                    NP_Name, (ULONG) "GLQuake",
                    TAG_DONE);
    CurrentDir(old);
    DeleteVar("RadeonGLCheck", GVF_LOCAL_ONLY);
    Close(out);
    Close(in);
    UnLock(lock);
    if (rc == -1)
        Tell("GLQuake could not be started.");
}

/* --- main ----------------------------------------------------------------- */

static BOOL OpenClasses(void)
{
    WindowBase = OpenLibrary("window.class", 44);
    LayoutBase = OpenLibrary("gadgets/layout.gadget", 44);
    ButtonBase = OpenLibrary("gadgets/button.gadget", 44);
    ChooserBase = OpenLibrary("gadgets/chooser.gadget", 45);
    CheckBoxBase = OpenLibrary("gadgets/checkbox.gadget", 44);
    IntegerBase = OpenLibrary("gadgets/integer.gadget", 44);
    StringBase = OpenLibrary("gadgets/string.gadget", 44);
    LabelBase = OpenLibrary("images/label.image", 44);
    return (BOOL) (WindowBase && LayoutBase && ButtonBase && ChooserBase && CheckBoxBase
                   && IntegerBase && StringBase && LabelBase);
}

static void CloseClasses(void)
{
    if (LabelBase) CloseLibrary(LabelBase);
    if (StringBase) CloseLibrary(StringBase);
    if (IntegerBase) CloseLibrary(IntegerBase);
    if (CheckBoxBase) CloseLibrary(CheckBoxBase);
    if (ChooserBase) CloseLibrary(ChooserBase);
    if (ButtonBase) CloseLibrary(ButtonBase);
    if (LayoutBase) CloseLibrary(LayoutBase);
    if (WindowBase) CloseLibrary(WindowBase);
}

#define ACT_NONE  0
#define ACT_PLAY  1
#define ACT_SAVE  2
#define ACT_QUIT  3
#define ACT_ABOUT 4

static LONG MenuAction(ULONG code)
{
    struct MenuItem *item;
    LONG act;
    ULONG id;

    act = ACT_NONE;
    while (code != MENUNULL && win != NULL && win->MenuStrip != NULL)
    {
        item = ItemAddress(win->MenuStrip, code);
        if (item == NULL)
            break;
        code = item->NextSelect;        /* read before acting */
        id = (ULONG) GTMENUITEM_USERDATA(item);
        if (id == GID_PLAY)
            act = ACT_PLAY;
        else if (id == GID_SAVE)
            act = ACT_SAVE;
        else if (id == MID_ABOUT)
            act = ACT_ABOUT;
        else if (id == GID_QUIT)
            return ACT_QUIT;
    }
    return act;
}

int main(void)
{
    ULONG sigs, result;
    UWORD code;
    LONG act;
    BOOL running;
    int rc;

    rc = RETURN_FAIL;
    UtilityBase = OpenLibrary("utility.library", 39);
    IntuitionBase = (struct IntuitionBase *) OpenLibrary("intuition.library", 39);
    GfxBase = (struct GfxBase *) OpenLibrary("graphics.library", 39);
    if (UtilityBase == NULL || IntuitionBase == NULL || GfxBase == NULL)
        goto out;
    if (!OpenClasses())
    {
        Tell("The ReAction classes of AmigaOS 3.2 are missing\n"
             "(window.class and the layout, button, chooser, checkbox,\n"
             "integer and string gadgets, label.image).");
        goto out;
    }

    NewList(&gamelist);
    NewList(&sizelist);
    NewList(&depthlist);
    CheckCpu();
    FindProgram();
    FindGames();
    FindSizes();
    DefaultSettings();
    LoadSettings();

    winobj = BuildWindow();
    if (winobj == NULL || (win = (struct Window *) DoMethod(winobj, WM_OPEN)) == NULL)
    {
        Tell("The window could not be opened.");
        goto out;
    }
    if (program == NULL)
        Tell("There is no GLQuake in the launcher's drawer.\n"
             "Install GLQuake for R200 68k, or put the launcher\n"
             "beside GLQuake and Quake's id1 drawer.");

    running = TRUE;
    rc = RETURN_OK;
    while (running)
    {
        sigs = 0;
        GetAttr(WINDOW_SigMask, winobj, &sigs);
        if (Wait(sigs | SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
            break;
        act = ACT_NONE;
        while ((result = DoMethod(winobj, WM_HANDLEINPUT, (ULONG) &code)) != WMHI_LASTMSG)
        {
            switch (result & WMHI_CLASSMASK)
            {
                case WMHI_CLOSEWINDOW:
                    running = FALSE;
                    break;
                case WMHI_GADGETUP:
                    switch (result & WMHI_GADGETMASK)
                    {
                        case GID_PLAY: act = ACT_PLAY; break;
                        case GID_SAVE: act = ACT_SAVE; break;
                        case GID_QUIT: running = FALSE; break;
                    }
                    break;
                case WMHI_MENUPICK:
                    act = MenuAction(result & WMHI_MENUMASK);
                    if (act == ACT_QUIT)
                        running = FALSE;
                    break;
                case WMHI_RAWKEY:
                    if ((result & WMHI_KEYMASK) == 0x45)            /* Esc */
                        running = FALSE;
                    else if ((result & WMHI_KEYMASK) == 0x44 || (result & WMHI_KEYMASK) == 0x43)
                        act = ACT_PLAY;                             /* Return, Enter */
                    break;
            }
        }
        if (!running)
            break;
        if (act == ACT_SAVE)
        {
            ReadGadgets();
            if (!SaveSettings())
                Tell("The settings could not be saved\n(" PREFS_FILE ").");
        }
        else if (act == ACT_ABOUT)
            Tell(LAUNCHER_NAME " " RGLQ_VERSION " (" RGLQ_DATE ")\n"
                 "the start window of GLQuake for R200 68k\n\n"
                 "It starts the GLQuake installed beside it with the\n"
                 "options of this window, and keeps them in\n"
                 "QuakeLauncher.prefs.");
        else if (act == ACT_PLAY && program != NULL)
        {
            ReadGadgets();
            (void) SaveSettings();      /* the next start begins here */
            DoMethod(winobj, WM_CLOSE);
            win = NULL;
            Play();
            win = (struct Window *) DoMethod(winobj, WM_OPEN);
            if (win == NULL)
                break;
        }
    }

out:
    if (winobj != NULL)
        DisposeObject(winobj);      /* the gadgets with it; the chooser lists are ours */
    FreeLabels(&depthlist);
    FreeLabels(&sizelist);
    FreeLabels(&gamelist);
    CloseClasses();
    if (GfxBase) CloseLibrary((struct Library *) GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *) IntuitionBase);
    if (UtilityBase) CloseLibrary(UtilityBase);
    return rc;
}
