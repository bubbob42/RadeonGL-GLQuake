/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "quakedef.h"
#include "rglq_version.h"	// RGLQ_VERSION on the credits page

#ifdef _WIN32
#include "winquake.h"
#endif

//void (*vid_menudrawfn)(void);
//void (*vid_menukeyfn)(int key);

enum {m_none, m_main, m_singleplayer, m_load, m_save, m_multiplayer, m_setup, m_net, m_options, m_video, m_videomodes, m_keys, m_help, m_quit, m_serialconfig, m_modemconfig, m_lanconfig, m_gameoptions, m_search, m_slist, m_detail, m_sound} m_state;

void M_Menu_Detail_f (void); //surgeon
void M_Menu_Main_f (void);
void M_Menu_SinglePlayer_f (void);
void M_Menu_Load_f (void);
void M_Menu_Save_f (void);
void M_Menu_MultiPlayer_f (void);
void M_Menu_Setup_f (void);
void M_Menu_Net_f (void);
void M_Menu_Options_f (void);
void M_Menu_Keys_f (void);
void M_Menu_Sound_f (void);	// 2026-11-06
void M_Menu_Video_f (void);	// 2026-11-06
void M_Menu_VideoModes_f (void);	// 2026-11-13
void M_Menu_Help_f (void);
void M_Menu_Quit_f (void);
void M_Menu_SerialConfig_f (void);
void M_Menu_ModemConfig_f (void);
void M_Menu_LanConfig_f (void);
void M_Menu_GameOptions_f (void);
void M_Menu_Search_f (void);
void M_Menu_ServerList_f (void);

void M_Detail_Draw (void); //surgeo
void M_Main_Draw (void);
void M_SinglePlayer_Draw (void);
void M_Load_Draw (void);
void M_Save_Draw (void);
void M_MultiPlayer_Draw (void);
void M_Setup_Draw (void);
void M_Net_Draw (void);
void M_Options_Draw (void);
void M_Keys_Draw (void);
void M_Video_Draw (void);
void M_VideoModes_Draw (void);
void M_Help_Draw (void);
void M_Quit_Draw (void);
void M_SerialConfig_Draw (void);
void M_ModemConfig_Draw (void);
void M_LanConfig_Draw (void);
void M_GameOptions_Draw (void);
void M_Search_Draw (void);
void M_ServerList_Draw (void);

void M_Detail_Key (int key); //surgeon
void M_Main_Key (int key);
void M_SinglePlayer_Key (int key);
void M_Load_Key (int key);
void M_Save_Key (int key);
void M_MultiPlayer_Key (int key);
void M_Setup_Key (int key);
void M_Net_Key (int key);
void M_Options_Key (int key);
void M_Keys_Key (int key);
void M_Video_Key (int key);
void M_VideoModes_Key (int key);
void M_Help_Key (int key);
void M_Quit_Key (int key);
void M_SerialConfig_Key (int key);
void M_ModemConfig_Key (int key);
void M_LanConfig_Key (int key);
void M_GameOptions_Key (int key);
void M_Search_Key (int key);
void M_ServerList_Key (int key);

qboolean  m_entersound; // play after drawing a frame, so caching
			// won't disrupt the sound
qboolean  m_recursiveDraw;

int	m_return_state;
qboolean  m_return_onerror;
char	m_return_reason [32];

#define StartingGame  (m_multiplayer_cursor == 1)
#define JoiningGame   (m_multiplayer_cursor == 0)
#define SerialConfig  (m_net_cursor == 0)
#define DirectConfig  (m_net_cursor == 1)
#define IPXConfig   (m_net_cursor == 2)
#define TCPIPConfig   (m_net_cursor == 3)

void M_ConfigureNetSubsystem(void);

// windowplay -------------------------------------------------

//extern struct Window *QuakeWindow;
//extern /*unsigned short*/ UWORD *FakeMousePointer; // like minigl
//extern void Pointer (struct Window *QuakeWindow);
//extern void DeletePointer (struct Window *QuakeWindow);
extern qboolean MouseHandler;
extern void IN_ShutdownMouse ();
extern void IN_InitMouse ();
extern BOOL windowplay;

// ------------------------------------------------------------


//windowplay
static void checkfmp (void) // Cowcat
{
	if(windowplay)
	{
		if( !MouseHandler )
		{
			IN_InitMouse();
			//Pointer(QuakeWindow);
			mglSetPointer(); // new minigl - Cowcat 2022
		}
	}
}


/*
================
M_DrawCharacter

Draws one solid graphics character
================
*/
/* REAL BUG FIX (2026-08-03): only horizontal centering existed here
 * ((vid.width - 320)>>1) -- menu Y coordinates were used as raw
 * vanilla-Quake absolute pixels (320x200 baseline, matching the other
 * platform backends' vid.height=200 default, e.g. vid_sunx.c/vid_x.c)
 * with no equivalent vertical centering at all. At this hardware's
 * real resolution (800x600), that left the whole menu cramped near
 * the top instead of centered -- "not centered, top options off-
 * screen." Added the same (vid.height - 200)>>1 pattern, mirroring
 * the existing horizontal fix exactly. */
void M_DrawCharacter (int cx, int line, int num)
{
	Draw_Character ( cx + ((vid.width - 320)>>1), line + ((vid.height - 200)>>1), num);
}

void M_Print (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, (*str)+128);
		str++;
		cx += 8;
	}
}

void M_PrintWhite (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, *str);
		str++;
		cx += 8;
	}
}

void M_DrawTransPic (int x, int y, qpic_t *pic)
{
	Draw_TransPic (x + ((vid.width - 320)>>1), y + ((vid.height - 200)>>1), pic);
}

void M_DrawPic (int x, int y, qpic_t *pic)
{
	Draw_Pic (x + ((vid.width - 320)>>1), y + ((vid.height - 200)>>1), pic);
}

byte identityTable[256];
byte translationTable[256];

void M_BuildTranslationTable(int top, int bottom)
{
  int	j;
  byte	*dest, *source;

  for (j = 0; j < 256; j++)
    identityTable[j] = j;
  dest = translationTable;
  source = identityTable;
  memcpy (dest, source, 256);

  if (top < 128)  // the artists made some backwards ranges.  sigh.
    memcpy (dest + TOP_RANGE, source + top, 16);
  else
    for (j=0 ; j<16 ; j++)
      dest[TOP_RANGE+j] = source[top+15-j];

  if (bottom < 128)
    memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
  else
    for (j=0 ; j<16 ; j++)
      dest[BOTTOM_RANGE+j] = source[bottom+15-j];
}


void M_DrawTransPicTranslate (int x, int y, qpic_t *pic)
{
	Draw_TransPicTranslate (x + ((vid.width - 320)>>1), y + ((vid.height - 200)>>1), pic, translationTable);
}


void M_DrawTextBox (int x, int y, int width, int lines)
{
  qpic_t  *p;
  int	cx, cy;
  int	n;

  // draw left side
  cx = x;
  cy = y;
  p = Draw_CachePic ("gfx/box_tl.lmp");
  M_DrawTransPic (cx, cy, p);
  p = Draw_CachePic ("gfx/box_ml.lmp");
  for (n = 0; n < lines; n++)
  {
    cy += 8;
    M_DrawTransPic (cx, cy, p);
  }
  p = Draw_CachePic ("gfx/box_bl.lmp");
  M_DrawTransPic (cx, cy+8, p);

  // draw middle
  cx += 8;
  while (width > 0)
  {
    cy = y;
    p = Draw_CachePic ("gfx/box_tm.lmp");
    M_DrawTransPic (cx, cy, p);
    p = Draw_CachePic ("gfx/box_mm.lmp");
    for (n = 0; n < lines; n++)
    {
      cy += 8;
      if (n == 1)
	p = Draw_CachePic ("gfx/box_mm2.lmp");
      M_DrawTransPic (cx, cy, p);
    }
    p = Draw_CachePic ("gfx/box_bm.lmp");
    M_DrawTransPic (cx, cy+8, p);
    width -= 2;
    cx += 16;
  }

  // draw right side
  cy = y;
  p = Draw_CachePic ("gfx/box_tr.lmp");
  M_DrawTransPic (cx, cy, p);
  p = Draw_CachePic ("gfx/box_mr.lmp");
  for (n = 0; n < lines; n++)
  {
    cy += 8;
    M_DrawTransPic (cx, cy, p);
  }
  p = Draw_CachePic ("gfx/box_br.lmp");
  M_DrawTransPic (cx, cy+8, p);
}

//=============================================================================

int m_save_demonum;

/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
  m_entersound = true;	

  if (key_dest == key_menu)
  {
    if (m_state != m_main)
    {
      M_Menu_Main_f ();
      return;
    }
    
    key_dest = key_game;
    m_state = m_none;
    return;
  }
  if (key_dest == key_console)
  {
    Con_ToggleConsole_f ();
  }
  else
  {
    M_Menu_Main_f ();
  }
}


//=============================================================================
/* MAIN MENU */

int m_main_cursor;
#define MAIN_ITEMS  5

void M_Menu_Main_f (void)
{
	if (key_dest != key_menu)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}

	key_dest = key_menu;
	m_state = m_main;
	m_entersound = true;

	if(windowplay)
	{
		//if(FakeMousePointer) { IN_ShutdownMouse(); DeletePointer(QuakeWindow);}

		if(MouseHandler)
		{
			IN_ShutdownMouse();
			mglClearPointer(); // new minigl - Cowcat 2022
		}
    	} 
}


void M_Main_Draw (void)
{
	int	f;
	qpic_t  *p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_main.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/mainmenu.lmp") );

	f = (int)(host_time * 10)%6;

	M_DrawTransPic (54, 32 + m_main_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
}



void M_Main_Key (int key)
{
	switch (key)
	{

		case K_ESCAPE:
			key_dest = key_game;
			m_state = m_none;
			cls.demonum = m_save_demonum;

			if (cls.demonum != -1 && !cls.demoplayback && cls.state != ca_connected)
				CL_NextDemo ();

    			checkfmp(); // Cowcat

    			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");

			if (++m_main_cursor >= MAIN_ITEMS)
				m_main_cursor = 0;

			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");

			if (--m_main_cursor < 0)
				m_main_cursor = MAIN_ITEMS - 1;

			break;

  		case K_ENTER:
			m_entersound = true;

			switch (m_main_cursor)
			{
				case 0:
					M_Menu_SinglePlayer_f ();
					break;

				case 1:
					M_Menu_MultiPlayer_f ();
					break;

				case 2:
					M_Menu_Options_f ();
					break;

				case 3:
					M_Menu_Help_f ();
					break;

				case 4:
					M_Menu_Quit_f ();
					break;
			}
	}
}

//=============================================================================
/* SINGLE PLAYER MENU */

int m_singleplayer_cursor;
#define SINGLEPLAYER_ITEMS  3

void M_Menu_SinglePlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_singleplayer;
	m_entersound = true;

	checkfmp(); // Cowcat
}


void M_SinglePlayer_Draw (void)
{
	int	f;
	qpic_t  *p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/sp_menu.lmp") );

	f = (int)(host_time * 10)%6;

	M_DrawTransPic (54, 32 + m_singleplayer_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
}


void M_SinglePlayer_Key (int key)
{
  switch (key)
  {
  case K_ESCAPE:
    M_Menu_Main_f ();
    break;

  case K_DOWNARROW:
    S_LocalSound ("misc/menu1.wav");
    if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
      m_singleplayer_cursor = 0;
    break;

  case K_UPARROW:
    S_LocalSound ("misc/menu1.wav");
    if (--m_singleplayer_cursor < 0)
      m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
    break;

  case K_ENTER:
    m_entersound = true;

    switch (m_singleplayer_cursor)
    {
    case 0:
      if (sv.active)
	if (!SCR_ModalMessage("Are you sure you want to\nstart a new game?\n"))
	  break;
      key_dest = key_game;
      if (sv.active)
	Cbuf_AddText ("disconnect\n");
      Cbuf_AddText ("maxplayers 1\n");
      Cbuf_AddText ("map start\n");
      break;

    case 1:
      M_Menu_Load_f ();
      break;

    case 2:
      M_Menu_Save_f ();
      break;
    }
  }
}

//=============================================================================
/* LOAD/SAVE MENU */

int   load_cursor;    // 0 < load_cursor < MAX_SAVEGAMES

#define MAX_SAVEGAMES	12
char  m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH+1];
int   loadable[MAX_SAVEGAMES];

void M_ScanSaves (void)
{
  int	i, j;
  char	name[MAX_OSPATH];
  FILE	*f;
  int	version;

  for (i=0 ; i<MAX_SAVEGAMES ; i++)
  {
    strcpy (m_filenames[i], "--- UNUSED SLOT ---");
    loadable[i] = false;
    sprintf (name, "%s/s%i.sav", com_gamedir, i);
    f = fopen (name, "r");
    if (!f)
      continue;
    fscanf (f, "%i\n", &version);
    fscanf (f, "%79s\n", name);
    strncpy (m_filenames[i], name, sizeof(m_filenames[i])-1);

  // change _ back to space
    for (j=0 ; j<SAVEGAME_COMMENT_LENGTH ; j++)
      if (m_filenames[i][j] == '_')
	m_filenames[i][j] = ' ';
    loadable[i] = true;
    fclose (f);
  }
}

void M_Menu_Load_f (void)
{
	m_entersound = true;
	m_state = m_load;
	key_dest = key_menu;
	M_ScanSaves ();

	checkfmp(); // Cowcat
}


void M_Menu_Save_f (void)
{
	if (!sv.active)
		return;

	if (cl.intermission)
		return;

	if (svs.maxclients != 1)
		return;

	m_entersound = true;
	m_state = m_save;
	key_dest = key_menu;
	M_ScanSaves ();

	checkfmp(); // Cowcat
}


void M_Load_Draw (void)
{
  int	i;
  qpic_t  *p;

  p = Draw_CachePic ("gfx/p_load.lmp");
  M_DrawPic ( (320-p->width)/2, 4, p);

  for (i=0 ; i< MAX_SAVEGAMES; i++)
    M_Print (16, 32 + 8*i, m_filenames[i]);

// line cursor
  M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Save_Draw (void)
{
  int	i;
  qpic_t  *p;

  p = Draw_CachePic ("gfx/p_save.lmp");
  M_DrawPic ( (320-p->width)/2, 4, p);

  for (i=0 ; i<MAX_SAVEGAMES ; i++)
    M_Print (16, 32 + 8*i, m_filenames[i]);

// line cursor
  M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Load_Key (int k)
{
  switch (k)
  {
  case K_ESCAPE:
    M_Menu_SinglePlayer_f ();
    break;

  case K_ENTER:
    S_LocalSound ("misc/menu2.wav");
    if (!loadable[load_cursor])
      return;
    m_state = m_none;
    key_dest = key_game;

  // Host_Loadgame_f can't bring up the loading plaque because too much
  // stack space has been used, so do it now
    SCR_BeginLoadingPlaque ();

  // issue the load command
    Cbuf_AddText (va ("load s%i\n", load_cursor) );
    return;

  case K_UPARROW:
  case K_LEFTARROW:
    S_LocalSound ("misc/menu1.wav");
    load_cursor--;
    if (load_cursor < 0)
      load_cursor = MAX_SAVEGAMES-1;
    break;

  case K_DOWNARROW:
  case K_RIGHTARROW:
    S_LocalSound ("misc/menu1.wav");
    load_cursor++;
    if (load_cursor >= MAX_SAVEGAMES)
      load_cursor = 0;
    break;
  }
}


void M_Save_Key (int k)
{
  switch (k)
  {
  case K_ESCAPE:
    M_Menu_SinglePlayer_f ();
    break;

  case K_ENTER:
    m_state = m_none;
    key_dest = key_game;
    Cbuf_AddText (va("save s%i\n", load_cursor));
    return;

  case K_UPARROW:
  case K_LEFTARROW:
    S_LocalSound ("misc/menu1.wav");
    load_cursor--;
    if (load_cursor < 0)
      load_cursor = MAX_SAVEGAMES-1;
    break;

  case K_DOWNARROW:
  case K_RIGHTARROW:
    S_LocalSound ("misc/menu1.wav");
    load_cursor++;
    if (load_cursor >= MAX_SAVEGAMES)
      load_cursor = 0;
    break;
  }
}

//=============================================================================
/* MULTIPLAYER MENU */

int m_multiplayer_cursor;
#define MULTIPLAYER_ITEMS 3

void M_Menu_MultiPlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_multiplayer;
	m_entersound = true;

	checkfmp(); // Cowcat
}


void M_MultiPlayer_Draw (void)
{
  int	f;
  qpic_t  *p;

  M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
  p = Draw_CachePic ("gfx/p_multi.lmp");
  M_DrawPic ( (320-p->width)/2, 4, p);
  M_DrawTransPic (72, 32, Draw_CachePic ("gfx/mp_menu.lmp") );

  f = (int)(host_time * 10)%6;

  M_DrawTransPic (54, 32 + m_multiplayer_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );

  if (serialAvailable || ipxAvailable || tcpipAvailable)
    return;
  M_PrintWhite ((320/2) - ((27*8)/2), 148, "No Communications Available");
}


void M_MultiPlayer_Key (int key)
{
  switch (key)
  {
  case K_ESCAPE:
    M_Menu_Main_f ();
    break;

  case K_DOWNARROW:
    S_LocalSound ("misc/menu1.wav");
    if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
      m_multiplayer_cursor = 0;
    break;

  case K_UPARROW:
    S_LocalSound ("misc/menu1.wav");
    if (--m_multiplayer_cursor < 0)
      m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
    break;

  case K_ENTER:
    m_entersound = true;
    switch (m_multiplayer_cursor)
    {
    case 0:
      if (serialAvailable || ipxAvailable || tcpipAvailable)
	M_Menu_Net_f ();
      break;

    case 1:
      if (serialAvailable || ipxAvailable || tcpipAvailable)
	M_Menu_Net_f ();
      break;

    case 2:
      M_Menu_Setup_f ();
      break;
    }
  }
}

//=============================================================================
/* SETUP MENU */

int   setup_cursor = 4;
int   setup_cursor_table[] = {40, 56, 80, 104, 140};

char  setup_hostname[16];
char  setup_myname[16];
int   setup_oldtop;
int   setup_oldbottom;
int   setup_top;
int   setup_bottom;

#define NUM_SETUP_CMDS	5

void M_Menu_Setup_f (void)
{
	key_dest = key_menu;
	m_state = m_setup;
	m_entersound = true;
	Q_strcpy(setup_myname, cl_name.string);
	Q_strcpy(setup_hostname, hostname.string);
	setup_top = setup_oldtop = ((int)cl_color.value) >> 4;
	setup_bottom = setup_oldbottom = ((int)cl_color.value) & 15;

	checkfmp(); // Cowcat
}


void M_Setup_Draw (void)
{
  qpic_t  *p;

  M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
  p = Draw_CachePic ("gfx/p_multi.lmp");
  M_DrawPic ( (320-p->width)/2, 4, p);

  M_Print (64, 40, "Hostname");
  M_DrawTextBox (160, 32, 16, 1);
  M_Print (168, 40, setup_hostname);

  M_Print (64, 56, "Your name");
  M_DrawTextBox (160, 48, 16, 1);
  M_Print (168, 56, setup_myname);

  M_Print (64, 80, "Shirt color");
  M_Print (64, 104, "Pants color");

  M_DrawTextBox (64, 140-8, 14, 1);
  M_Print (72, 140, "Accept Changes");

  p = Draw_CachePic ("gfx/bigbox.lmp");
  M_DrawTransPic (160, 64, p);
  p = Draw_CachePic ("gfx/menuplyr.lmp");
  M_BuildTranslationTable(setup_top*16, setup_bottom*16);
  M_DrawTransPicTranslate (172, 72, p);

  M_DrawCharacter (56, setup_cursor_table [setup_cursor], 12+((int)(realtime*4)&1));

  if (setup_cursor == 0)
    M_DrawCharacter (168 + 8*strlen(setup_hostname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));

  if (setup_cursor == 1)
    M_DrawCharacter (168 + 8*strlen(setup_myname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));
}


void M_Setup_Key (int k)
{
	int	l;

	switch (k)
	{
		case K_ESCAPE:
			M_Menu_MultiPlayer_f ();
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			setup_cursor--;

			if (setup_cursor < 0)
      				setup_cursor = NUM_SETUP_CMDS-1;

    			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			setup_cursor++;

			if (setup_cursor >= NUM_SETUP_CMDS)
				setup_cursor = 0;

			break;

		case K_LEFTARROW:

			if (setup_cursor < 2)
				return;

			S_LocalSound ("misc/menu3.wav");

			if (setup_cursor == 2)
				setup_top = setup_top - 1;

			if (setup_cursor == 3)
				setup_bottom = setup_bottom - 1;

			break;

		case K_RIGHTARROW:

			if (setup_cursor < 2)
 				return;

		forward:
    			S_LocalSound ("misc/menu3.wav");

    			if (setup_cursor == 2)
      				setup_top = setup_top + 1;

    			if (setup_cursor == 3)
      				setup_bottom = setup_bottom + 1;

    			break;

		case K_ENTER:

			if (setup_cursor == 0 || setup_cursor == 1)
				return;

			if (setup_cursor == 2 || setup_cursor == 3)
				goto forward;

			// setup_cursor == 4 (OK)
			if (Q_strcmp(cl_name.string, setup_myname) != 0)
				Cbuf_AddText ( va ("name \"%s\"\n", setup_myname) );

			if (Q_strcmp(hostname.string, setup_hostname) != 0)
				Cvar_Set("hostname", setup_hostname);

			if (setup_top != setup_oldtop || setup_bottom != setup_oldbottom)
				Cbuf_AddText( va ("color %i %i\n", setup_top, setup_bottom) );

			m_entersound = true;

    			M_Menu_MultiPlayer_f ();
    			break;

		case K_BACKSPACE:

    			if (setup_cursor == 0)
    			{
      				if (strlen(setup_hostname))
					setup_hostname[strlen(setup_hostname)-1] = 0;
    			}

    			if (setup_cursor == 1)
    			{
      				if (strlen(setup_myname))
					setup_myname[strlen(setup_myname)-1] = 0;
    			}

    			break;

  		default:

    			if (k < 32 || k > 127)
      				break;

    			if (setup_cursor == 0)
    			{
      				l = strlen(setup_hostname);

      				if (l < 15)
      				{
					setup_hostname[l+1] = 0;
					setup_hostname[l] = k;
      				}
    			}

    			if (setup_cursor == 1)
    			{
      				l = strlen(setup_myname);

      				if (l < 15)
      				{
					setup_myname[l+1] = 0;
					setup_myname[l] = k;
      				}
    			}
	}

	if (setup_top > 13)
    		setup_top = 0;

  	if (setup_top < 0)
    		setup_top = 13;

  	if (setup_bottom > 13)
    		setup_bottom = 0;

  	if (setup_bottom < 0)
    		setup_bottom = 13;
}

//=============================================================================
/* NET MENU */

int m_net_cursor;
int m_net_items;
int m_net_saveHeight;

char *net_helpMessage [] =
{
/* .........1.........2.... */
  "                        ",
  " Two computers connected",
  "   through two modems.  ",
  "                        ",

  "                        ",
  " Two computers connected",
  " by a null-modem cable. ",
  "                        ",

  " Novell network LANs	   ",
  " or Windows 95 DOS-box. ",
  "                        ",
  "(LAN=Local Area Network)",

  " Commonly used to play  ",
  " over the Internet, but ",
  " also used on a Local   ",
  " Area Network.          "
};

void M_Menu_Net_f (void)
{
	key_dest = key_menu;
	m_state = m_net;
	m_entersound = true;
	m_net_items = 4;

	if (m_net_cursor >= m_net_items)
		m_net_cursor = 0;

	m_net_cursor--;
	M_Net_Key (K_DOWNARROW);
}


void M_Net_Draw (void)
{
  int	f;
  qpic_t  *p;

  M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
  p = Draw_CachePic ("gfx/p_multi.lmp");
  M_DrawPic ( (320-p->width)/2, 4, p);

  f = 32;

  if (serialAvailable)
  {
    p = Draw_CachePic ("gfx/netmen1.lmp");
  }
  else
  {
#ifdef _WIN32
    p = NULL;
#else
    p = Draw_CachePic ("gfx/dim_modm.lmp");
#endif
  }

  if (p)
    M_DrawTransPic (72, f, p);

  f += 19;

  if (serialAvailable)
  {
    p = Draw_CachePic ("gfx/netmen2.lmp");
  }
  else
  {
#ifdef _WIN32
    p = NULL;
#else
    p = Draw_CachePic ("gfx/dim_drct.lmp");
#endif
  }

  if (p)
    M_DrawTransPic (72, f, p);

  f += 19;
  if (ipxAvailable)
    p = Draw_CachePic ("gfx/netmen3.lmp");
  else
    p = Draw_CachePic ("gfx/dim_ipx.lmp");
  M_DrawTransPic (72, f, p);

  f += 19;
  if (tcpipAvailable)
    p = Draw_CachePic ("gfx/netmen4.lmp");
  else
    p = Draw_CachePic ("gfx/dim_tcp.lmp");
  M_DrawTransPic (72, f, p);

  if (m_net_items == 5) // JDC, could just be removed
  {
    f += 19;
    p = Draw_CachePic ("gfx/netmen5.lmp");
    M_DrawTransPic (72, f, p);
  }

  f = (320-26*8)/2;
  M_DrawTextBox (f, 134, 24, 4);
  f += 8;
  M_Print (f, 142, net_helpMessage[m_net_cursor*4+0]);
  M_Print (f, 150, net_helpMessage[m_net_cursor*4+1]);
  M_Print (f, 158, net_helpMessage[m_net_cursor*4+2]);
  M_Print (f, 166, net_helpMessage[m_net_cursor*4+3]);

  f = (int)(host_time * 10)%6;
  M_DrawTransPic (54, 32 + m_net_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
}


void M_Net_Key (int k)
{
again:
  switch (k)
  {
  case K_ESCAPE:
    M_Menu_MultiPlayer_f ();
    break;

  case K_DOWNARROW:
    S_LocalSound ("misc/menu1.wav");
    if (++m_net_cursor >= m_net_items)
      m_net_cursor = 0;
    break;

  case K_UPARROW:
    S_LocalSound ("misc/menu1.wav");
    if (--m_net_cursor < 0)
      m_net_cursor = m_net_items - 1;
    break;

  case K_ENTER:
    m_entersound = true;

    switch (m_net_cursor)
    {
    case 0:
      M_Menu_SerialConfig_f ();
      break;

    case 1:
      M_Menu_SerialConfig_f ();
      break;

    case 2:
      M_Menu_LanConfig_f ();
      break;

    case 3:
      M_Menu_LanConfig_f ();
      break;

    case 4:
// multiprotocol
      break;
    }
  }

  if (m_net_cursor == 0 && !serialAvailable)
    goto again;
  if (m_net_cursor == 1 && !serialAvailable)
    goto again;
  if (m_net_cursor == 2 && !ipxAvailable)
    goto again;
  if (m_net_cursor == 3 && !tcpipAvailable)
    goto again;
}

//=============================================================================
/* OPTIONS MENU */

#ifdef _WIN32
#define OPTIONS_ITEMS 17
#else
/* 2026-11-06: 16 entries, 0 (Customize details) to 15 (Sound Options). It
 * was 14 with only 13 rows, which left an empty row for the cursor; then
 * Mouse Look (row 11), Video Options (14) and Sound Options (15) came in. */
#define OPTIONS_ITEMS 16 //details by surgeon, +1 Brightness, +1 Mouse Look, +2 Video/Sound Options
#endif

#define SLIDER_RANGE  10

int   options_cursor;

void M_Menu_Options_f (void)
{
	key_dest = key_menu;
	m_state = m_options;
	m_entersound = true;

	checkfmp(); // Cowcat

	#ifdef _WIN32
	if ((options_cursor == 17) && (modestate != MS_WINDOWED))
	{
		options_cursor = 0;
	}
	#endif
}


extern cvar_t gl_gamma;	// the startup gamma, written by the slider below

void M_AdjustSliders (int dir)
{
  S_LocalSound ("misc/menu3.wav");

  switch (options_cursor)
  {
  case 4: // screen size
    scr_viewsize.value += dir * 10;
    if (scr_viewsize.value < 30)
      scr_viewsize.value = 30;
    if (scr_viewsize.value > 120)
      scr_viewsize.value = 120;
    Cvar_SetValue ("viewsize", scr_viewsize.value);
    break;

  /* Brightness. Takes effect on the NEXT LAUNCH: the gamma is baked into
     d_8to24table at startup and the 8-bit source texels are not kept, so
     nothing can be re-gammaed in place. The label says so. */
  case 5: // brightness, next launch
    gl_gamma.value -= dir * 0.05;
    if (gl_gamma.value < 0.5)
      gl_gamma.value = 0.5;
    if (gl_gamma.value > 1)
      gl_gamma.value = 1;
    Cvar_SetValue ("gl_gamma", gl_gamma.value);
    break;

  case 6: // mouse speed
    /* 2026-11-06: down to 0.5 (was 1): since 1.0 every mouse movement
     * counts, so the same value turns faster than before */
    sensitivity.value += dir * 0.5;
    if (sensitivity.value < 0.5)
      sensitivity.value = 0.5;
    if (sensitivity.value > 11)
      sensitivity.value = 11;
    Cvar_SetValue ("sensitivity", sensitivity.value);
    break;
  case 7: // music volume
#ifdef _WIN32
    bgmvolume.value += dir * 1.0;
#else
    bgmvolume.value += dir * 0.1;
#endif
    if (bgmvolume.value < 0)
      bgmvolume.value = 0;
    if (bgmvolume.value > 1)
      bgmvolume.value = 1;
    Cvar_SetValue ("bgmvolume", bgmvolume.value);
    break;
  case 8: // sfx volume
    volume.value += dir * 0.1;
    if (volume.value < 0)
      volume.value = 0;
    if (volume.value > 1)
      volume.value = 1;
    Cvar_SetValue ("volume", volume.value);
    break;

  case 9: // allways run
    if (cl_forwardspeed.value > 200)
    {
      Cvar_SetValue ("cl_forwardspeed", 200);
      Cvar_SetValue ("cl_backspeed", 200);
    }
    else
    {
      Cvar_SetValue ("cl_forwardspeed", 400);
      Cvar_SetValue ("cl_backspeed", 400);
    }
    break;

  case 10: // invert mouse
    Cvar_SetValue ("m_pitch", -m_pitch.value);
    break;

  case 11:  // mouse look (2026-11-06): as a lasting +mlook, saved
    /* 2026-11-07: the box shows +mlook as well (console, bound key); off
     * turns both off, as a console -mlook does */
    if (IN_MLOOK_ON())
    {
      Cvar_SetValue ("freelook", 0);
      in_mlook.down[0] = in_mlook.down[1] = 0;
      in_mlook.state = 4;	// impulse up
    }
    else
      Cvar_SetValue ("freelook", 1);
    if (!IN_MLOOK_ON() && lookspring.value)
      V_StartPitchDrift ();	// as releasing a +mlook key does
    break;

  case 12:  // lookspring
    Cvar_SetValue ("lookspring", !lookspring.value);
    break;

  case 13:  // lookstrafe
    Cvar_SetValue ("lookstrafe", !lookstrafe.value);
    break;

#ifdef _WIN32
  case 17:  // _windowed_mouse
    Cvar_SetValue ("_windowed_mouse", !_windowed_mouse.value);
    break;

#endif
  }
}


void M_DrawSlider (int x, int y, float range)
{
  int i;

  if (range < 0)
    range = 0;
  if (range > 1)
    range = 1;
  M_DrawCharacter (x-8, y, 128);
  for (i=0 ; i<SLIDER_RANGE ; i++)
    M_DrawCharacter (x + i*8, y, 129);
  M_DrawCharacter (x+i*8, y, 130);
  M_DrawCharacter (x + (SLIDER_RANGE-1)*8 * range, y, 131);
}

void M_DrawCheckbox (int x, int y, int on)
{
#if 0
  if (on)
    M_DrawCharacter (x, y, 131);
  else
    M_DrawCharacter (x, y, 129);
#endif
  if (on)
    M_Print (x, y, "on");
  else
    M_Print (x, y, "off");
}

void M_Options_Draw (void)
{
	float	r;
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

#if 0	
  M_Print (16, 32, "	 Customize details");
  M_Print (16, 40, "	Customize controls");
  M_Print (16, 48, "	     Go to console");
  M_Print (16, 56, "	 Reset to defaults");

  M_Print (16, 64, "	       Screen size");
  r = (scr_viewsize.value - 30) / (120 - 30);
  M_DrawSlider (220, 64, r);

  M_Print (16, 72, "		Brightness");
  r = (1.0 - v_gamma.value) / 0.5;
  M_DrawSlider (220, 72, r);

  M_Print (16, 80, "	       Mouse Speed");
  r = (sensitivity.value - 1)/10;
  M_DrawSlider (220, 80, r);

  M_Print (16, 88, "	   CD Music Volume");
  r = bgmvolume.value;
  M_DrawSlider (220, 88, r);

  M_Print (16, 96, "	      Sound Volume");
  r = volume.value;
  M_DrawSlider (220, 96, r);

  M_Print (16, 104,  "		  Always Run");
  M_DrawCheckbox (220, 104, cl_forwardspeed.value > 200);

  M_Print (16, 112, "	       Invert Mouse");
  M_DrawCheckbox (220, 112, m_pitch.value < 0);

  M_Print (16, 120, "		 Lookspring");
  M_DrawCheckbox (220, 120, lookspring.value);

  M_Print (16, 128, "		 Lookstrafe");
  M_DrawCheckbox (220, 128, lookstrafe.value);

  //if (vid_menudrawfn)
    //M_Print (16, 136, "	  Video Options");

#endif
  
  M_Print (16, 32, "     Customize details");
  M_Print (16, 40, "    Customize controls");
  M_Print (16, 48, "         Go to console");
  M_Print (16, 56, "     Reset to defaults");

  M_Print (16, 64, "           Screen size");
  r = (scr_viewsize.value - 30) / (120 - 30);
  M_DrawSlider (220, 64, r);
  
  M_Print (16, 72, "            Brightness");
  r = (1.0 - gl_gamma.value) / 0.5;
  M_DrawSlider (220, 72, r);

  M_Print (16, 80, "           Mouse Speed");
  r = (sensitivity.value - 0.5)/10.5;	// 0.5 to 11 (2026-11-06)
  M_DrawSlider (220, 80, r);

  M_Print (16, 88, "       CD Music Volume");
  r = bgmvolume.value;
  M_DrawSlider (220, 88, r);

  M_Print (16, 96, "          Sound Volume");
  r = volume.value;
  M_DrawSlider (220, 96, r);

  M_Print (16, 104, "            Always Run");
  M_DrawCheckbox (220, 104, cl_forwardspeed.value > 200);

  M_Print (16, 112, "          Invert Mouse");
  M_DrawCheckbox (220, 112, m_pitch.value < 0);

  M_Print (16, 120, "            Mouse Look");	// 2026-11-06
  M_DrawCheckbox (220, 120, IN_MLOOK_ON());

  M_Print (16, 128, "            Lookspring");
  M_DrawCheckbox (220, 128, lookspring.value);

  M_Print (16, 136, "            Lookstrafe");
  M_DrawCheckbox (220, 136, lookstrafe.value);

  M_Print (16, 144, "         Video Options");	// 2026-11-06: the Video Modes page
  M_Print (16, 152, "         Sound Options");	// 2026-11-06: the Sound page

  //if (vid_menudrawfn)
    //M_Print (16, 128, "	  Video Options");

#ifdef _WIN32
  if (modestate == MS_WINDOWED)
  {
    M_Print (16, 176, "		    Use Mouse");
    M_DrawCheckbox (220, 176, _windowed_mouse.value);
  }
#endif

  /* The Brightness hint, on its own line and only while that row is selected.
     It cannot go in the label: the labels are right-aligned in a 22-character
     field, so a longer one starts further LEFT and runs into the qplaque
     sidebar at x=16. The longest label the menu ships with is 18 characters,
     starting at x=48, which is why x=56 here is safe at any y. */
  if (options_cursor == 5)
    M_Print (56, 168, "applies on restart");	// below the last row (2026-11-06)

	// cursor
  	M_DrawCharacter (200, 32 + options_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Options_Key (int k)
{
  switch (k)
  {
  case K_ESCAPE:
    M_Menu_Main_f ();
    break;

  case K_ENTER:
    m_entersound = true;
    switch (options_cursor)
    {
    case 0:
      M_Menu_Detail_f ();
      break;
    case 1:
      M_Menu_Keys_f ();
      break;
    case 2:
      m_state = m_none;
      Con_ToggleConsole_f ();
      break;
    case 3:
      Cbuf_AddText ("exec default.cfg\n");
      break;
    case 14:	// 2026-11-06
      M_Menu_Video_f ();
      break;
    case 15:	// 2026-11-06
      M_Menu_Sound_f ();
      break;
    default:
      M_AdjustSliders (1);
      break;
    }
    return;

  case K_UPARROW:
    S_LocalSound ("misc/menu1.wav");
    options_cursor--;
    if (options_cursor < 0)
      options_cursor = OPTIONS_ITEMS-1;
    break;

  case K_DOWNARROW:
    S_LocalSound ("misc/menu1.wav");
    options_cursor++;
    if (options_cursor >= OPTIONS_ITEMS)
      options_cursor = 0;
    break;

  case K_LEFTARROW:
    M_AdjustSliders (-1);
    break;

  case K_RIGHTARROW:
    M_AdjustSliders (1);
    break;
  }

  /* 2026-11-06: id skipped row 12 here, its "Video Options" without a video
   * menu. Since "Customize details" moved every row down one, row 12 was
   * Lookstrafe (13 since Mouse Look), which the skip made unreachable going
   * down. Gone. */

#ifdef _WIN32
  if ((options_cursor == 17) && (modestate != MS_WINDOWED))
  {
    if (k == K_UPARROW)
      options_cursor = 15;
    else
      options_cursor = 0;
  }
#endif
}

//=============================================================================
/* KEYS MENU */

char *bindnames[][2] =
{
{"+attack",	"attack"},
{"impulse 10",	  "change weapon"},
{"+jump",	"jump / swim up"},
{"+forward",	"walk forward"},
{"+back",	"backpedal"},
{"+left",	"turn left"},
{"+right",	"turn right"},
{"+speed",	"run"},
{"+moveleft",	  "step left"},
{"+moveright",	  "step right"},
{"+strafe",	"sidestep"},
{"+lookup",	"look up"},
{"+lookdown",	  "look down"},
{"centerview",	  "center view"},
{"+mlook",	"mouse look"},
{"+klook",	"keyboard look"},
{"+moveup",	"swim up"},
{"+movedown",	"swim down"}
};

#define NUMCOMMANDS (sizeof(bindnames)/sizeof(bindnames[0]))

int   keys_cursor;
int   bind_grab;

void M_Menu_Keys_f (void)
{
  key_dest = key_menu;
  m_state = m_keys;
  m_entersound = true;
}


void M_FindKeysForCommand (char *command, int *twokeys)
{
  int	count;
  int	j;
  int	l;
  char	*b;

  twokeys[0] = twokeys[1] = -1;
  l = strlen(command);
  count = 0;

  for (j=0 ; j<256 ; j++)
  {
    b = keybindings[j];
    if (!b)
      continue;
    if (!strncmp (b, command, l) )
    {
      twokeys[count] = j;
      count++;
      if (count == 2)
	break;
    }
  }
}

void M_UnbindCommand (char *command)
{
  int	j;
  int	l;
  char	*b;

  l = strlen(command);

  for (j=0 ; j<256 ; j++)
  {
    b = keybindings[j];
    if (!b)
      continue;
    if (!strncmp (b, command, l) )
      Key_SetBinding (j, "");
  }
}


void M_Keys_Draw (void)
{
	int	i, x, y;
	int	keys[3]; // was 2 - Cowcat 2019
	char	*name;
	qpic_t  *p;

	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	if (bind_grab)
		M_Print (12, 32, "Press a key or button for this action");

	else
		M_Print (18, 32, "Enter to change, backspace to clear");

	// search for known bindings
	for (i=0 ; i<NUMCOMMANDS ; i++)
	{
		y = 48 + 8*i;

		M_Print (16, y, bindnames[i][1]);

		M_FindKeysForCommand (bindnames[i][0], keys);

		if (keys[0] == -1)
		{
			M_Print (140, y, "???");
		}

		else
		{
			name = Key_KeynumToString (keys[0]);
			M_Print (140, y, name);
			x = strlen(name) * 8;

			if (keys[1] != -1)
			{
				M_Print (140 + x + 8, y, "or");
				M_Print (140 + x + 32, y, Key_KeynumToString (keys[1]));
			}
		}
	}

	if (bind_grab)
		M_DrawCharacter (130, 48 + keys_cursor*8, '=');

	else
		M_DrawCharacter (130, 48 + keys_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Keys_Key (int k)
{
  char	cmd[80];
  int	keys[2];

  if (bind_grab)
  { // defining a key
    S_LocalSound ("misc/menu1.wav");
    if (k == K_ESCAPE)
    {
      bind_grab = false;
    }
    else if (k != '`')
    {
      sprintf (cmd, "bind \"%s\" \"%s\"\n", Key_KeynumToString (k), bindnames[keys_cursor][0]);
      Cbuf_InsertText (cmd);
    }

    bind_grab = false;
    return;
  }

  switch (k)
  {
  case K_ESCAPE:
    M_Menu_Options_f ();
    break;

  case K_LEFTARROW:
  case K_UPARROW:
    S_LocalSound ("misc/menu1.wav");
    keys_cursor--;
    if (keys_cursor < 0)
      keys_cursor = NUMCOMMANDS-1;
    break;

  case K_DOWNARROW:
  case K_RIGHTARROW:
    S_LocalSound ("misc/menu1.wav");
    keys_cursor++;
    if (keys_cursor >= NUMCOMMANDS)
      keys_cursor = 0;
    break;

  case K_ENTER:	  // go into bind mode
    M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
    S_LocalSound ("misc/menu2.wav");
    if (keys[1] != -1)
      M_UnbindCommand (bindnames[keys_cursor][0]);
    bind_grab = true;
    break;

  case K_BACKSPACE:   // delete bindings
  case K_DEL:	    // delete bindings
    S_LocalSound ("misc/menu2.wav");
    M_UnbindCommand (bindnames[keys_cursor][0]);
    break;
  }
}

//=============================================================================
/* SOUND MENU (2026-11-06)

Options > Sound Options: the device (Paula, AHI or off), AHI's mode, the
mixing rate and Paula's channel swap. The changes collect until Apply (or
leaving with Escape), which saves them (snd_device, snd_ahimode, snd_rate,
snd_swap) and restarts the sound in the running game (S_Restart). Rows
that do not apply to the chosen device say n/a and the cursor skips them.
*/

#define SOUND_ROWS	5		// device, AHI mode, rate, swap, Apply
#define SOUND_MAXMODES	32

static const int	sound_rowy[SOUND_ROWS] = {32, 40, 56, 64, 80};
static const int	sound_rates_paula[4] = {11025, 16000, 22050, 28000};
static const int	sound_rates_ahi[4] = {11025, 22050, 32000, 44100};
static const char	*sound_devnames[3] = {"Paula", "AHI", "off"};

static int		sound_cursor;
static int		sound_dev;		// 0 Paula, 1 AHI, 2 off (chosen, not yet applied)
static int		sound_mode;		// index into sound_modeids, 0 = AHI's default
static int		sound_rate;		// index into the device's rate table
static int		sound_swap;
static qboolean		sound_changed;
static int		sound_nummodes;
static unsigned long	sound_modeids[SOUND_MAXMODES + 1];
static char		sound_modenames[SOUND_MAXMODES + 1][SND_AHINAME];

static const int *Sound_Rates (int dev)
{
	return (dev == 1) ? sound_rates_ahi : sound_rates_paula;
}

static int Sound_NearestRate (int dev, int hz)
{
	const int	*r;
	int		i, best, d, bestd;

	r = Sound_Rates (dev);
	best = 0;
	bestd = 1 << 30;

	for (i = 0; i < 4; i++)
	{
		d = abs (r[i] - hz);

		if (d < bestd)
		{
			bestd = d;
			best = i;
		}
	}

	return best;
}

static qboolean Sound_RowUsed (int row)
{
	switch (row)
	{
		case 1:	return sound_dev == 1;		// AHI mode
		case 2:	return sound_dev != 2;		// rate
		case 3:	return sound_dev == 0;		// Paula's swap
	}

	return true;
}

void M_Menu_Sound_f (void)
{
	unsigned long	id;
	int		i;

	key_dest = key_menu;
	m_state = m_sound;
	m_entersound = true;

	// AHI's modes, the list read again on each visit
	sound_modeids[0] = 0;
	strcpy (sound_modenames[0], "AHI's default (AHI prefs)");
	sound_nummodes = 1 + SND_AHIListModes (&sound_modeids[1], &sound_modenames[1], SOUND_MAXMODES);

	// the page starts from the saved settings
	if (!Q_strcasecmp (snd_device.string, "ahi"))
		sound_dev = 1;
	else if (!Q_strcasecmp (snd_device.string, "off"))
		sound_dev = 2;
	else
		sound_dev = 0;

	id = strtoul (snd_ahimode.string, NULL, 0);
	sound_mode = 0;

	for (i = 1; i < sound_nummodes; i++)
	{
		if (sound_modeids[i] == id)
			sound_mode = i;
	}

	sound_rate = Sound_NearestRate (sound_dev, (int)snd_rate.value);
	sound_swap = (snd_swap.value != 0);
	sound_changed = false;
	sound_cursor = 0;
}

static void Sound_Apply (void)
{
	char	v[32];

	Cvar_Set ("snd_device", sound_dev == 1 ? "ahi" : (sound_dev == 2 ? "off" : "paula"));
	sprintf (v, "%lu", sound_modeids[sound_mode]);
	Cvar_Set ("snd_ahimode", v);
	sprintf (v, "%d", Sound_Rates (sound_dev)[sound_rate]);
	Cvar_Set ("snd_rate", v);
	Cvar_Set ("snd_swap", sound_swap ? "1" : "0");

	S_Restart ();
	sound_changed = false;
}

void M_Sound_Draw (void)
{
	qpic_t	*p;
	char	v[64];
	int	inuse;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);

	M_Print (16, 32, "          Sound device");
	M_Print (220, 32, (char *)sound_devnames[sound_dev]);

	M_Print (16, 40, "              AHI mode");

	if (Sound_RowUsed (1))
	{
		strncpy (v, sound_modenames[sound_mode], 32);
		v[32] = 0;
		M_PrintWhite (56, 48, v);
	}
	else
		M_Print (220, 40, "n/a");

	M_Print (16, 56, "           Mixing rate");

	if (Sound_RowUsed (2))
	{
		sprintf (v, "%d Hz", Sound_Rates (sound_dev)[sound_rate]);
		M_Print (220, 56, v);
	}
	else
		M_Print (220, 56, "n/a");

	M_Print (16, 64, "         Swap channels");

	if (Sound_RowUsed (3))
		M_Print (220, 64, sound_swap ? "on" : "off");
	else
		M_Print (220, 64, "n/a");

	M_Print (16, 80, "                 Apply");

	// what runs now
	inuse = SND_DeviceInUse ();

	if (inuse && shm)
		sprintf (v, "Now: %s, %d Hz", inuse == 2 ? "AHI" : "Paula", shm->speed);
	else
		strcpy (v, COM_CheckParm ("-nosound") ? "Now: off (-nosound)" : "Now: off");

	M_PrintWhite (56, 104, v);
	M_Print (56, 120, sound_changed ? "Apply restarts the sound" : "");
	M_Print (56, 128, "Esc applies and goes back");

	M_DrawCharacter (200, sound_rowy[sound_cursor], 12 + ((int)(realtime * 4) & 1));
}

static void Sound_Change (int dir)
{
	int	hz;

	switch (sound_cursor)
	{
		case 0:		// device: Paula, AHI, off
			hz = Sound_Rates (sound_dev)[sound_rate];
			sound_dev = (sound_dev + 3 + dir) % 3;
			sound_rate = Sound_NearestRate (sound_dev, hz);
			break;

		case 1:
			sound_mode = (sound_mode + sound_nummodes + dir) % sound_nummodes;
			break;

		case 2:
			sound_rate = (sound_rate + 4 + dir) % 4;
			break;

		case 3:
			sound_swap = !sound_swap;
			break;

		default:
			return;
	}

	sound_changed = true;
	S_LocalSound ("misc/menu3.wav");
}

void M_Sound_Key (int k)
{
	switch (k)
	{
		case K_ESCAPE:
			if (sound_changed)
				Sound_Apply ();

			M_Menu_Options_f ();
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");

			do
			{
				if (--sound_cursor < 0)
					sound_cursor = SOUND_ROWS - 1;
			} while (!Sound_RowUsed (sound_cursor));
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");

			do
			{
				if (++sound_cursor >= SOUND_ROWS)
					sound_cursor = 0;
			} while (!Sound_RowUsed (sound_cursor));
			break;

		case K_LEFTARROW:
			Sound_Change (-1);
			break;

		case K_RIGHTARROW:
			Sound_Change (1);
			break;

		case K_ENTER:
			if (sound_cursor == SOUND_ROWS - 1)
			{
				Sound_Apply ();
				S_LocalSound ("misc/menu2.wav");
			}
			else
				Sound_Change (1);
			break;
	}
}

//=============================================================================
/* VIDEO MENU (2026-11-06; 2026-11-13 three items, the user)

Options > Video Options: Choose screenmode (its own page, below), Display
sync (off, vsync, ScrollVPort, ChangeScreenBuffer or, 2026-11-13, the same
without the DispMessage wait, unsafe: VID_SetSyncMode, live,
saved as vid_vsync and vid_bufmode) and Screen depth (16 or 32 bit, saved as
vid_bpp, from the next start).
*/

#define VIDEO_MAXMODES	40
#define VIDEO_COLS	3

/* 2026-11-13 (the user): the Video Options page holds three items --
 * Choose screenmode (a page of its own, the modes as before), Display sync
 * (one chooser for the four ways a frame reaches the screen: off, vsync,
 * ScrollVPort, ChangeScreenBuffer -- the two separate items of 1.4 read as
 * independent, and they are not) and Screen depth (16 or 32 bit, from the
 * next start: the library keeps a screen's depth while it runs). */
#define VIDEO_ITEMS	3

static vidmode_t	video_modes[VIDEO_MAXMODES];
static int		video_nummodes, video_cursor;
static int		video_item;		// the Video Options page's cursor

/* Display sync's ways (VID_GetSyncMode), each with its cost in brackets
 * (the user, 2026-11-13): on a line of their own under the label, from x 48
 * -- the longest, 25 characters, would not fit beside it */
#define VIDEO_SYNCS	5

/* 2026-11-13: the fifth, ChangeScreenBuffer without waiting for the
 * DispMessage (Tim's way): Picasso96's maintainer says it is not safe, so
 * it says so. MEASURED on the A4000D (demo1): no faster than the fourth --
 * 21.6 fps at 1280 x 720 and 17.0 at 1920 x 1080 both ways (vsync 29.8 and
 * 22.5, ScrollVPort 26.7 and 21.9): the order above is the speed order.
 * 34 characters from x 48: the menu's last column. */
static char		*video_sync[VIDEO_SYNCS] = {"off", "vsync (slower)", "ScrollVPort (fast)",
					  "ChangeScreenBuffer (slow)", "ChangeScreenBuffer (slow / unsafe)"};

/* 2026-11-13 late: the chooser goes through them fastest first; the
 * numbers (vid_sync, saved as vid_vsync and vid_bufmode) stay as they were.
 * MEASURED with RadeonGL 0.14 on the A4000D (demo1, 1280 x 720 / 1920 x
 * 1080): off 32.2 / 28.7 fps, ScrollVPort 32.1 / 30.6 (its helper below the
 * game now; 0.13: 26.6 / 21.9), vsync 29.9 / 22.4, ChangeScreenBuffer
 * 21.6 / 17.0, the same unsafe 21.6 / 17.0. */
static int		video_sync_order[VIDEO_SYNCS] = {0, 2, 1, 3, 4};

/* the items' rows: Display sync's value takes the line under its label */
static int		video_item_y[VIDEO_ITEMS] = {48, 56, 72};

static char		*video_sync_help[VIDEO_SYNCS] = {
	"Copied at once: fast, tears",
	"Copied behind the beam",
	"3 buffers, panned: no tearing",
	"3 buffers, Intuition switches",
	"As before, not waiting: unsafe"};

/* white text in lines of up to 34 characters, broken at spaces (the
 * menu's 320 pixels less the margins) */
static void Video_PrintWrapped (int x, int y, const char *s)
{
	char	line[36];
	int	n, cut;

	while (*s)
	{
		while (*s == ' ')
			s++;

		n = strlen (s);

		if (n > 34)
		{
			for (cut = 34; cut > 0 && s[cut] != ' '; cut--)
				;

			if (cut == 0)
				cut = 34;
		}
		else
			cut = n;

		memcpy (line, s, cut);
		line[cut] = 0;
		M_PrintWhite (x, y, line);
		s += cut;
		y += 8;
	}
}

static int Video_Rows (void)
{
	return (video_nummodes + VIDEO_COLS - 1) / VIDEO_COLS;
}

/* the depth saved for the next start: 16 or 32 */
static int Video_SavedBpp (void)
{
	return (Cvar_VariableValue ("vid_bpp") > 16) ? 32 : 16;
}

void M_Menu_Video_f (void)
{
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;
	vid_switch_error[0] = 0;	// an old refusal is no news
}

void M_Video_Draw (void)
{
	qpic_t	*p;
	char	v[40];
	int	w, h, y, m;

	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);
	M_PrintWhite (100, 32, "Video Options");

	VID_GetSize (&w, &h);
	m = VID_GetSyncMode ();

	M_Print (24, video_item_y[0], "Choose screenmode");
	sprintf (v, "%dx%d", w, h);
	M_Print (24 + 18 * 8, video_item_y[0], v);
	M_Print (24, video_item_y[1], "Display sync");
	M_Print (48, video_item_y[1] + 8, video_sync[m]);
	M_Print (24, video_item_y[2], "Screen depth");
	sprintf (v, "%d bit", Video_SavedBpp ());
	M_Print (24 + 18 * 8, video_item_y[2], v);

	M_DrawCharacter (24 - 12, video_item_y[video_item], 12 + ((int)(realtime * 4) & 1));

	y = video_item_y[2] + 24;
	sprintf (v, "Running: %dx%d, %d bit", w, h, VID_GetBpp ());
	M_PrintWhite (48, y, v);

	// 2026-11-07: a refused switch says why, in place of the help
	if (vid_switch_error[0])
	{
		Video_PrintWrapped (24, y + 16, vid_switch_error);
		return;
	}

	switch (video_item)
	{
		case 0:
			M_Print (48, y + 16, "Enter lists the modes");
			break;

		case 1:
			M_Print (48, y + 16, video_sync_help[m]);
			M_Print (48, y + 24, "Enter or arrows: the next");
			break;

		default:
			M_Print (48, y + 16, "Enter: 16 or 32 bit");

			if (Video_SavedBpp () != VID_GetBpp ())
				M_Print (48, y + 24, "From the next start");
			break;
	}

	M_Print (48, y + 40, "Esc goes back");
}

void M_Video_Key (int k)
{
	int	step;

	step = 1;

	switch (k)
	{
		case K_ESCAPE:
			M_Menu_Options_f ();
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			video_item = (video_item + VIDEO_ITEMS - 1) % VIDEO_ITEMS;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			video_item = (video_item + 1) % VIDEO_ITEMS;
			break;

		case K_LEFTARROW:
			step = VIDEO_SYNCS - 1;	// one back of the five ways
			/* fall through */
		case K_RIGHTARROW:
		case K_ENTER:
			S_LocalSound (k == K_ENTER ? "misc/menu2.wav" : "misc/menu1.wav");

			if (video_item == 0)
			{
				if (k == K_ENTER || k == K_RIGHTARROW)
					M_Menu_VideoModes_f ();
			}
			else if (video_item == 1)
			{
				int	pos;

				vid_switch_error[0] = 0;

				for (pos = 0; pos < VIDEO_SYNCS - 1 && video_sync_order[pos] != VID_GetSyncMode (); pos++)
					;
				VID_SetSyncMode (video_sync_order[(pos + step) % VIDEO_SYNCS]);
			}
			else
				Cvar_SetValue ("vid_bpp", Video_SavedBpp () == 16 ? 32 : 16);
			break;
	}
}

/* Video Options > Choose screenmode (the 1.2-1.4 Video Modes page): the
 * screen modes the library offers (VID_ListModes, 5:4 to 16:9), in columns
 * as software Quake's; the running one in white. Enter switches live
 * (VID_SwitchMode: the library opens the new screen and keeps the textures)
 * and saves the choice (vid_width, vid_height, vid_modeid) for the next
 * start. A library that cannot switch keeps the size and says so. */
void M_Menu_VideoModes_f (void)
{
	int	w, h, i;

	key_dest = key_menu;
	m_state = m_videomodes;
	m_entersound = true;
	vid_switch_error[0] = 0;

	video_nummodes = VID_ListModes (video_modes, VIDEO_MAXMODES);
	VID_GetSize (&w, &h);
	video_cursor = 0;

	for (i = 0; i < video_nummodes; i++)
	{
		if (video_modes[i].width == w && video_modes[i].height == h)
			video_cursor = i;
	}
}

void M_VideoModes_Draw (void)
{
	qpic_t	*p;
	char	v[40];
	int	i, x, y, w, h, rows;

	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);

	VID_GetSize (&w, &h);
	rows = Video_Rows ();

	for (i = 0; i < video_nummodes; i++)
	{
		x = 24 + (i / rows) * 104;
		y = 36 + (i % rows) * 8;
		sprintf (v, "%dx%d", video_modes[i].width, video_modes[i].height);

		if (video_modes[i].width == w && video_modes[i].height == h)
			M_PrintWhite (x, y, v);
		else
			M_Print (x, y, v);

		if (i == video_cursor)
			M_DrawCharacter (x - 12, y, 12 + ((int)(realtime * 4) & 1));
	}

	y = 36 + rows * 8 + 8;
	sprintf (v, "Running: %dx%d, %d bit", w, h, VID_GetBpp ());
	M_PrintWhite (48, y, v);

	if (vid_switch_error[0])
	{
		Video_PrintWrapped (24, y + 16, vid_switch_error);
		return;
	}

	M_Print (48, y + 16, "Enter switches the mode");
	M_Print (48, y + 24, "Esc goes back");
}

void M_VideoModes_Key (int k)
{
	char	v[24];
	int	rows;

	rows = Video_Rows ();

	if (k == K_ESCAPE)
	{
		M_Menu_Video_f ();
		return;
	}

	if (video_nummodes == 0)
		return;

	switch (k)
	{
		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			video_cursor = (video_cursor + video_nummodes - 1) % video_nummodes;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			video_cursor = (video_cursor + 1) % video_nummodes;
			break;

		case K_LEFTARROW:
			S_LocalSound ("misc/menu1.wav");
			video_cursor -= rows;

			if (video_cursor < 0)
				video_cursor += rows * VIDEO_COLS;

			while (video_cursor >= video_nummodes)
				video_cursor -= rows;
			break;

		case K_RIGHTARROW:
			S_LocalSound ("misc/menu1.wav");
			video_cursor += rows;

			if (video_cursor >= video_nummodes)
				video_cursor %= rows;
			break;

		case K_ENTER:
			S_LocalSound ("misc/menu2.wav");
			VID_SwitchMode (video_modes[video_cursor].width, video_modes[video_cursor].height);
			sprintf (v, "%d", video_modes[video_cursor].width);
			Cvar_Set ("vid_width", v);
			sprintf (v, "%d", video_modes[video_cursor].height);
			Cvar_Set ("vid_height", v);
			sprintf (v, "%lu", video_modes[video_cursor].id);
			Cvar_Set ("vid_modeid", v);
			break;
	}
}

//=============================================================================
/* HELP MENU */

int   help_page;
#define NUM_HELP_PAGES	7

void M_Menu_Help_f (void)
{
	key_dest = key_menu;
	m_state = m_help;
	m_entersound = true;
	help_page = 0;

	checkfmp(); // Cowcat
}

void M_Help_Draw (void)
{
	int f;

	if (help_page)
		M_DrawPic (0, 0, Draw_CachePic ( va("gfx/help%i.lmp", help_page - 1)) );

	else
	{
		M_DrawTextBox (16, 16, 34, 20);

		M_PrintWhite(32, 48,  "        Amiga Quake V1.31        ");
		M_Print	    (32, 64,  "               by                ");
		M_Print	    (32, 72,  " Frank Wille and Steffen Haeuser ");
		M_PrintWhite(36, 88,  "     Includes ProQuake V1.03     ");
		M_Print	    (28, 96,  "   extensions by J.P. Grossman   ");

		M_PrintWhite(32, 112, "          2014 Updates           ");
		M_Print	    (32, 120, "     by Cowcat and Matthey       ");

		// 2026-11-13 (the user): the RadeonGL edition
		M_PrintWhite(32, 136, "    RadeonGL edition " RGLQ_VERSION "         ");
		M_Print	    (32, 144, "       by Marcus Gerards         ");

		M_Print	    (32, 160, "<-- Previous            Next -->");

		f = (int)(host_time * 8)%6;
		M_DrawTransPic (48, 40 ,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
		M_DrawTransPic (248, 40 ,Draw_CachePic( va("gfx/menudot%i.lmp", f?7-f:1) ) );
	}
}


void M_Help_Key (int key)
{
	switch (key)
	{
		case K_ESCAPE:
			M_Menu_Main_f ();
			break;

  		case K_UPARROW:
  		case K_RIGHTARROW:
    			m_entersound = true;

    			if (++help_page >= NUM_HELP_PAGES)
      				help_page = 0;

    			break;

 		 case K_DOWNARROW:
  		case K_LEFTARROW:
    			m_entersound = true;

    			if (--help_page < 0)
      				help_page = NUM_HELP_PAGES-1;

    			break;
  	}
}

//=============================================================================
/* QUIT MENU */

int   msgNumber;
int   m_quit_prevstate;
qboolean  wasInMenus;

#ifndef _WIN32

char *quitMessage [] = 
{
/* .........1.........2.... */
  "  Are you gonna quit    ",
  "  this game just like   ",
  "   everything else?     ",
  "                        ",
 
  " Milord, methinks that  ",
  "   thou art a lowly     ",
  " quitter. Is this true? ",
  "                        ",

  " Do I need to bust your ",
  "  face open for trying  ",
  "        to quit?        ",
  "                        ",

  " Man, I oughta smack you",
  "   for trying to quit!  ",
  "     Press Y to get     ",
  "      smacked out.      ",
 
  " Press Y to quit like a ",
  "   big loser in life.   ",
  "  Press N to stay proud ",
  "    and successful!     ",
 
  "   If you press Y to    ",
  "  quit, I will summon   ",
  "  Satan all over your   ",
  "      hard drive!       ",
 
  "  Um, Asmodeus dislikes ",
  " his children trying to ",
  " quit. Press Y to return",
  "   to your Tinkertoys.  ",
 
  "  If you quit now, I'll ",
  "  throw a blanket-party ",
  "   for you next time!   ",
  "                        "
};

#endif

void M_Menu_Quit_f (void)
{  
	if (m_state == m_quit)
		return;

	wasInMenus = (key_dest == key_menu);
	key_dest = key_menu;
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = true;
	msgNumber = rand()&7;

	checkfmp(); // Cowcat
}

void M_Quit_Key (int key)
{
	switch (key)
	{
		case K_ESCAPE:
		case 'n':
		case 'N':

			if (wasInMenus)
			{
				m_state = m_quit_prevstate;
				m_entersound = true;
			}

			else
			{
				key_dest = key_game;
				m_state = m_none;
			}

			break;

		case 'Y':
		case 'y':
			key_dest = key_console;

			#ifdef GLQUAKE
			mglLockMode(MGL_LOCK_AUTOMATIC);  
			#endif

			Host_Quit_f ();
			break;

		default:
			break;
	}
}


void M_Quit_Draw (void)
{
	if (wasInMenus)
	{
		m_state = m_quit_prevstate;
		m_recursiveDraw = true;
		M_Draw ();
		m_state = m_quit;
	}

#ifdef _WIN32
  M_DrawTextBox (0, 0, 38, 23);
  M_PrintWhite (16, 12,	 "  Quake version 1.09 by id Software\n\n");
  M_PrintWhite (16, 28,	 "Programming	     Art \n");
  M_Print (16, 36,  " John Carmack	 Adrian Carmack\n");
  M_Print (16, 44,  " Michael Abrash	 Kevin Cloud\n");
  M_Print (16, 52,  " John Cash		 Paul Steed\n");
  M_Print (16, 60,  " Dave 'Zoid' Kirsch\n");
  M_PrintWhite (16, 68,	 "Design	     Biz\n");
  M_Print (16, 76,  " John Romero	 Jay Wilbur\n");
  M_Print (16, 84,  " Sandy Petersen	 Mike Wilson\n");
  M_Print (16, 92,  " American McGee	 Donna Jackson\n");
  M_Print (16, 100,  " Tim Willits	  Todd Hollenshead\n");
  M_PrintWhite (16, 108, "Support	     Projects\n");
  M_Print (16, 116, " Barrett Alexander	 Shawn Green\n");
  M_PrintWhite (16, 124, "Sound Effects\n");
  M_Print (16, 132, " Trent Reznor and Nine Inch Nails\n\n");
  M_PrintWhite (16, 140, "Quake is a trademark of Id Software,\n");
  M_PrintWhite (16, 148, "inc., (c)1996 Id Software, inc. All\n");
  M_PrintWhite (16, 156, "rights reserved. NIN logo is a\n");
  M_PrintWhite (16, 164, "registered trademark licensed to\n");
  M_PrintWhite (16, 172, "Nothing Interactive, Inc. All rights\n");
  M_PrintWhite (16, 180, "reserved. Press y to exit\n");
#else
  M_DrawTextBox (56, 76, 24, 4);
  M_Print (64, 84,  quitMessage[msgNumber*4+0]);
  M_Print (64, 92,  quitMessage[msgNumber*4+1]);
  M_Print (64, 100, quitMessage[msgNumber*4+2]);
  M_Print (64, 108, quitMessage[msgNumber*4+3]);
#endif
}

//=============================================================================

/* SERIAL CONFIG MENU */

int   serialConfig_cursor;
int   serialConfig_cursor_table[] = {48, 64, 80, 96, 112, 132};
#define NUM_SERIALCONFIG_CMDS 6

static int ISA_uarts[]	= {0x3f8,0x2f8,0x3e8,0x2e8};
static int ISA_IRQs[] = {4,3,4,3};
int serialConfig_baudrate[] = {9600,14400,19200,28800,38400,57600};

int   serialConfig_comport;
int   serialConfig_irq ;
int   serialConfig_baud;
char  serialConfig_phone[16];

void M_Menu_SerialConfig_f (void)
{
  int	n;
  int	port;
  int	baudrate;
  qboolean  useModem;

  key_dest = key_menu;
  m_state = m_serialconfig;
  m_entersound = true;
  if (JoiningGame && SerialConfig)
    serialConfig_cursor = 4;
  else
    serialConfig_cursor = 5;

  (*GetComPortConfig) (0, &port, &serialConfig_irq, &baudrate, &useModem);

  // map uart's port to COMx
  for (n = 0; n < 4; n++)
    if (ISA_uarts[n] == port)
      break;
  if (n == 4)
  {
    n = 0;
    serialConfig_irq = 4;
  }
  serialConfig_comport = n + 1;

  // map baudrate to index
  for (n = 0; n < 6; n++)
    if (serialConfig_baudrate[n] == baudrate)
      break;
  if (n == 6)
    n = 5;
  serialConfig_baud = n;

  m_return_onerror = false;
  m_return_reason[0] = 0;
}


void M_SerialConfig_Draw (void)
{
  qpic_t  *p;
  int	basex;
  char	*startJoin;
  char	*directModem;

  M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
  p = Draw_CachePic ("gfx/p_multi.lmp");
  basex = (320-p->width)/2;
  M_DrawPic (basex, 4, p);

  if (StartingGame)
    startJoin = "New Game";
  else
    startJoin = "Join Game";
  if (SerialConfig)
    directModem = "Modem";
  else
    directModem = "Direct Connect";
  M_Print (basex, 32, va ("%s - %s", startJoin, directModem));
  basex += 8;

  M_Print (basex, serialConfig_cursor_table[0], "Port");
  M_DrawTextBox (160, 40, 4, 1);
  M_Print (168, serialConfig_cursor_table[0], va("COM%u", serialConfig_comport));

  M_Print (basex, serialConfig_cursor_table[1], "IRQ");
  M_DrawTextBox (160, serialConfig_cursor_table[1]-8, 1, 1);
  M_Print (168, serialConfig_cursor_table[1], va("%u", serialConfig_irq));

  M_Print (basex, serialConfig_cursor_table[2], "Baud");
  M_DrawTextBox (160, serialConfig_cursor_table[2]-8, 5, 1);
  M_Print (168, serialConfig_cursor_table[2], va("%u", serialConfig_baudrate[serialConfig_baud]));

  if (SerialConfig)
  {
    M_Print (basex, serialConfig_cursor_table[3], "Modem Setup...");
    if (JoiningGame)
    {
      M_Print (basex, serialConfig_cursor_table[4], "Phone number");
      M_DrawTextBox (160, serialConfig_cursor_table[4]-8, 16, 1);
      M_Print (168, serialConfig_cursor_table[4], serialConfig_phone);
    }
  }

  if (JoiningGame)
  {
    M_DrawTextBox (basex, serialConfig_cursor_table[5]-8, 7, 1);
    M_Print (basex+8, serialConfig_cursor_table[5], "Connect");
  }
  else
  {
    M_DrawTextBox (basex, serialConfig_cursor_table[5]-8, 2, 1);
    M_Print (basex+8, serialConfig_cursor_table[5], "OK");
  }

  M_DrawCharacter (basex-8, serialConfig_cursor_table [serialConfig_cursor], 12+((int)(realtime*4)&1));

  if (serialConfig_cursor == 4)
    M_DrawCharacter (168 + 8*strlen(serialConfig_phone), serialConfig_cursor_table [serialConfig_cursor], 10+((int)(realtime*4)&1));

  if (*m_return_reason)
    M_PrintWhite (basex, 148, m_return_reason);
}


void M_SerialConfig_Key (int key)
{
  int	l;

  switch (key)
  {
  case K_ESCAPE:
    M_Menu_Net_f ();
    break;

  case K_UPARROW:
    S_LocalSound ("misc/menu1.wav");
    serialConfig_cursor--;
    if (serialConfig_cursor < 0)
      serialConfig_cursor = NUM_SERIALCONFIG_CMDS-1;
    break;

  case K_DOWNARROW:
    S_LocalSound ("misc/menu1.wav");
    serialConfig_cursor++;
    if (serialConfig_cursor >= NUM_SERIALCONFIG_CMDS)
      serialConfig_cursor = 0;
    break;

  case K_LEFTARROW:
    if (serialConfig_cursor > 2)
      break;
    S_LocalSound ("misc/menu3.wav");

    if (serialConfig_cursor == 0)
    {
      serialConfig_comport--;
      if (serialConfig_comport == 0)
	serialConfig_comport = 4;
      serialConfig_irq = ISA_IRQs[serialConfig_comport-1];
    }

    if (serialConfig_cursor == 1)
    {
      serialConfig_irq--;
      if (serialConfig_irq == 6)
	serialConfig_irq = 5;
      if (serialConfig_irq == 1)
	serialConfig_irq = 7;
    }

    if (serialConfig_cursor == 2)
    {
      serialConfig_baud--;
      if (serialConfig_baud < 0)
	serialConfig_baud = 5;
    }

    break;

  case K_RIGHTARROW:
    if (serialConfig_cursor > 2)
      break;
forward:
    S_LocalSound ("misc/menu3.wav");

    if (serialConfig_cursor == 0)
    {
      serialConfig_comport++;
      if (serialConfig_comport > 4)
	serialConfig_comport = 1;
      serialConfig_irq = ISA_IRQs[serialConfig_comport-1];
    }

    if (serialConfig_cursor == 1)
    {
      serialConfig_irq++;
      if (serialConfig_irq == 6)
	serialConfig_irq = 7;
      if (serialConfig_irq == 8)
	serialConfig_irq = 2;
    }

    if (serialConfig_cursor == 2)
    {
      serialConfig_baud++;
      if (serialConfig_baud > 5)
	serialConfig_baud = 0;
    }

    break;

  case K_ENTER:
    if (serialConfig_cursor < 3)
      goto forward;

    m_entersound = true;

    if (serialConfig_cursor == 3)
    {
      (*SetComPortConfig) (0, ISA_uarts[serialConfig_comport-1], serialConfig_irq, serialConfig_baudrate[serialConfig_baud], SerialConfig);

      M_Menu_ModemConfig_f ();
      break;
    }

    if (serialConfig_cursor == 4)
    {
      serialConfig_cursor = 5;
      break;
    }

    // serialConfig_cursor == 5 (OK/CONNECT)
    (*SetComPortConfig) (0, ISA_uarts[serialConfig_comport-1], serialConfig_irq, serialConfig_baudrate[serialConfig_baud], SerialConfig);

    M_ConfigureNetSubsystem ();

    if (StartingGame)
    {
      M_Menu_GameOptions_f ();
      break;
    }

    m_return_state = m_state;
    m_return_onerror = true;
    key_dest = key_game;
    m_state = m_none;

    if (SerialConfig)
      Cbuf_AddText (va ("connect \"%s\"\n", serialConfig_phone));
    else
      Cbuf_AddText ("connect\n");
    break;

  case K_BACKSPACE:
    if (serialConfig_cursor == 4)
    {
      if (strlen(serialConfig_phone))
	serialConfig_phone[strlen(serialConfig_phone)-1] = 0;
    }
    break;

  default:
    if (key < 32 || key > 127)
      break;
    if (serialConfig_cursor == 4)
    {
      l = strlen(serialConfig_phone);
      if (l < 15)
      {
	serialConfig_phone[l+1] = 0;
	serialConfig_phone[l] = key;
      }
    }
  }

	if (DirectConfig && (serialConfig_cursor == 3 || serialConfig_cursor == 4))
	{
		if (key == K_UPARROW)
			serialConfig_cursor = 2;

		else
			serialConfig_cursor = 5;
	}

	if (SerialConfig && StartingGame && serialConfig_cursor == 4)
	{
		if (key == K_UPARROW)
			serialConfig_cursor = 3;

		else
			serialConfig_cursor = 5;
	}
}

//=============================================================================
/* MODEM CONFIG MENU */

int   modemConfig_cursor;
int   modemConfig_cursor_table [] = {40, 56, 88, 120, 156};
#define NUM_MODEMCONFIG_CMDS  5

char  modemConfig_dialing;
char  modemConfig_clear [16];
char  modemConfig_init [32];
char  modemConfig_hangup [16];

void M_Menu_ModemConfig_f (void)
{
  key_dest = key_menu;
  m_state = m_modemconfig;
  m_entersound = true;
  (*GetModemConfig) (0, &modemConfig_dialing, modemConfig_clear, modemConfig_init, modemConfig_hangup);
}


void M_ModemConfig_Draw (void)
{
  qpic_t  *p;
  int	basex;

  M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
  p = Draw_CachePic ("gfx/p_multi.lmp");
  basex = (320-p->width)/2;
  M_DrawPic (basex, 4, p);
  basex += 8;

  if (modemConfig_dialing == 'P')
    M_Print (basex, modemConfig_cursor_table[0], "Pulse Dialing");
  else
    M_Print (basex, modemConfig_cursor_table[0], "Touch Tone Dialing");

  M_Print (basex, modemConfig_cursor_table[1], "Clear");
  M_DrawTextBox (basex, modemConfig_cursor_table[1]+4, 16, 1);
  M_Print (basex+8, modemConfig_cursor_table[1]+12, modemConfig_clear);
  if (modemConfig_cursor == 1)
    M_DrawCharacter (basex+8 + 8*strlen(modemConfig_clear), modemConfig_cursor_table[1]+12, 10+((int)(realtime*4)&1));

  M_Print (basex, modemConfig_cursor_table[2], "Init");
  M_DrawTextBox (basex, modemConfig_cursor_table[2]+4, 30, 1);
  M_Print (basex+8, modemConfig_cursor_table[2]+12, modemConfig_init);
  if (modemConfig_cursor == 2)
    M_DrawCharacter (basex+8 + 8*strlen(modemConfig_init), modemConfig_cursor_table[2]+12, 10+((int)(realtime*4)&1));

  M_Print (basex, modemConfig_cursor_table[3], "Hangup");
  M_DrawTextBox (basex, modemConfig_cursor_table[3]+4, 16, 1);
  M_Print (basex+8, modemConfig_cursor_table[3]+12, modemConfig_hangup);
  if (modemConfig_cursor == 3)
    M_DrawCharacter (basex+8 + 8*strlen(modemConfig_hangup), modemConfig_cursor_table[3]+12, 10+((int)(realtime*4)&1));

  M_DrawTextBox (basex, modemConfig_cursor_table[4]-8, 2, 1);
  M_Print (basex+8, modemConfig_cursor_table[4], "OK");

  M_DrawCharacter (basex-8, modemConfig_cursor_table [modemConfig_cursor], 12+((int)(realtime*4)&1));
}


void M_ModemConfig_Key (int key)
{
  int	l;

  switch (key)
  {
  case K_ESCAPE:
    M_Menu_SerialConfig_f ();
    break;

  case K_UPARROW:
    S_LocalSound ("misc/menu1.wav");
    modemConfig_cursor--;
    if (modemConfig_cursor < 0)
      modemConfig_cursor = NUM_MODEMCONFIG_CMDS-1;
    break;

  case K_DOWNARROW:
    S_LocalSound ("misc/menu1.wav");
    modemConfig_cursor++;
    if (modemConfig_cursor >= NUM_MODEMCONFIG_CMDS)
      modemConfig_cursor = 0;
    break;

  case K_LEFTARROW:
  case K_RIGHTARROW:
    if (modemConfig_cursor == 0)
    {
      if (modemConfig_dialing == 'P')
	modemConfig_dialing = 'T';
      else
	modemConfig_dialing = 'P';
      S_LocalSound ("misc/menu1.wav");
    }
    break;

  case K_ENTER:
    if (modemConfig_cursor == 0)
    {
      if (modemConfig_dialing == 'P')
	modemConfig_dialing = 'T';
      else
	modemConfig_dialing = 'P';
      m_entersound = true;
    }

    if (modemConfig_cursor == 4)
    {
      (*SetModemConfig) (0, va ("%c", modemConfig_dialing), modemConfig_clear, modemConfig_init, modemConfig_hangup);
      m_entersound = true;
      M_Menu_SerialConfig_f ();
    }
    break;

  case K_BACKSPACE:
    if (modemConfig_cursor == 1)
    {
      if (strlen(modemConfig_clear))
	modemConfig_clear[strlen(modemConfig_clear)-1] = 0;
    }

    if (modemConfig_cursor == 2)
    {
      if (strlen(modemConfig_init))
	modemConfig_init[strlen(modemConfig_init)-1] = 0;
    }

    if (modemConfig_cursor == 3)
    {
      if (strlen(modemConfig_hangup))
	modemConfig_hangup[strlen(modemConfig_hangup)-1] = 0;
    }
    break;

  default:
    if (key < 32 || key > 127)
      break;

    if (modemConfig_cursor == 1)
    {
      l = strlen(modemConfig_clear);
      if (l < 15)
      {
	modemConfig_clear[l+1] = 0;
	modemConfig_clear[l] = key;
      }
    }

    if (modemConfig_cursor == 2)
    {
      l = strlen(modemConfig_init);
      if (l < 29)
      {
	modemConfig_init[l+1] = 0;
	modemConfig_init[l] = key;
      }
    }

    if (modemConfig_cursor == 3)
    {
      l = strlen(modemConfig_hangup);
      if (l < 15)
      {
	modemConfig_hangup[l+1] = 0;
	modemConfig_hangup[l] = key;
      }
    }
  }
}

//=============================================================================
/* LAN CONFIG MENU */

int   lanConfig_cursor = -1;
int   lanConfig_cursor_table [] = {72, 92, 124};
#define NUM_LANCONFIG_CMDS  3

int   lanConfig_port;
char  lanConfig_portname[6];
char  lanConfig_joinname[22];

void M_Menu_LanConfig_f (void)
{
  key_dest = key_menu;
  m_state = m_lanconfig;
  m_entersound = true;
  if (lanConfig_cursor == -1)
  {
    if (JoiningGame && TCPIPConfig)
      lanConfig_cursor = 2;
    else
      lanConfig_cursor = 1;
  }
  if (StartingGame && lanConfig_cursor == 2)
    lanConfig_cursor = 1;
  lanConfig_port = DEFAULTnet_hostport;
  sprintf(lanConfig_portname, "%u", (unsigned int)lanConfig_port);

  m_return_onerror = false;
  m_return_reason[0] = 0;
}


void M_LanConfig_Draw (void)
{
  qpic_t  *p;
  int	basex;
  char	*startJoin;
  char	*protocol;

  M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
  p = Draw_CachePic ("gfx/p_multi.lmp");
  basex = (320-p->width)/2;
  M_DrawPic (basex, 4, p);

  if (StartingGame)
    startJoin = "New Game";
  else
    startJoin = "Join Game";
  if (IPXConfig)
    protocol = "IPX";
  else
    protocol = "TCP/IP";
  M_Print (basex, 32, va ("%s - %s", startJoin, protocol));
  basex += 8;

  M_Print (basex, 52, "Address:");
  if (IPXConfig)
    M_Print (basex+9*8, 52, my_ipx_address);
  else
    M_Print (basex+9*8, 52, my_tcpip_address);

  M_Print (basex, lanConfig_cursor_table[0], "Port");
  M_DrawTextBox (basex+8*8, lanConfig_cursor_table[0]-8, 6, 1);
  M_Print (basex+9*8, lanConfig_cursor_table[0], lanConfig_portname);

  if (JoiningGame)
  {
    M_Print (basex, lanConfig_cursor_table[1], "Search for local games...");
    M_Print (basex, 108, "Join game at:");
    M_DrawTextBox (basex+8, lanConfig_cursor_table[2]-8, 22, 1);
    M_Print (basex+16, lanConfig_cursor_table[2], lanConfig_joinname);
  }
  else
  {
    M_DrawTextBox (basex, lanConfig_cursor_table[1]-8, 2, 1);
    M_Print (basex+8, lanConfig_cursor_table[1], "OK");
  }

  M_DrawCharacter (basex-8, lanConfig_cursor_table [lanConfig_cursor], 12+((int)(realtime*4)&1));

  if (lanConfig_cursor == 0)
    M_DrawCharacter (basex+9*8 + 8*strlen(lanConfig_portname), lanConfig_cursor_table [0], 10+((int)(realtime*4)&1));

  if (lanConfig_cursor == 2)
    M_DrawCharacter (basex+16 + 8*strlen(lanConfig_joinname), lanConfig_cursor_table [2], 10+((int)(realtime*4)&1));

  if (*m_return_reason)
    M_PrintWhite (basex, 148, m_return_reason);
}


void M_LanConfig_Key (int key)
{
  int	l;

  switch (key)
  {
  case K_ESCAPE:
    M_Menu_Net_f ();
    break;

  case K_UPARROW:
    S_LocalSound ("misc/menu1.wav");
    lanConfig_cursor--;
    if (lanConfig_cursor < 0)
      lanConfig_cursor = NUM_LANCONFIG_CMDS-1;
    break;

  case K_DOWNARROW:
    S_LocalSound ("misc/menu1.wav");
    lanConfig_cursor++;
    if (lanConfig_cursor >= NUM_LANCONFIG_CMDS)
      lanConfig_cursor = 0;
    break;

  case K_ENTER:
    if (lanConfig_cursor == 0)
      break;

    m_entersound = true;

    M_ConfigureNetSubsystem ();

    if (lanConfig_cursor == 1)
    {
      if (StartingGame)
      {
	M_Menu_GameOptions_f ();
	break;
      }
      M_Menu_Search_f();
      break;
    }

    if (lanConfig_cursor == 2)
    {
      m_return_state = m_state;
      m_return_onerror = true;
      key_dest = key_game;
      m_state = m_none;
      Cbuf_AddText ( va ("connect \"%s\"\n", lanConfig_joinname) );
      break;
    }

    break;

  case K_BACKSPACE:
    if (lanConfig_cursor == 0)
    {
      if (strlen(lanConfig_portname))
	lanConfig_portname[strlen(lanConfig_portname)-1] = 0;
    }

    if (lanConfig_cursor == 2)
    {
      if (strlen(lanConfig_joinname))
	lanConfig_joinname[strlen(lanConfig_joinname)-1] = 0;
    }
    break;

  default:
    if (key < 32 || key > 127)
      break;

    if (lanConfig_cursor == 2)
    {
      l = strlen(lanConfig_joinname);
      if (l < 21)
      {
	lanConfig_joinname[l+1] = 0;
	lanConfig_joinname[l] = key;
      }
    }

    if (key < '0' || key > '9')
      break;
    if (lanConfig_cursor == 0)
    {
      l = strlen(lanConfig_portname);
      if (l < 5)
      {
	lanConfig_portname[l+1] = 0;
	lanConfig_portname[l] = key;
      }
    }
  }

	if (StartingGame && lanConfig_cursor == 2)
	{
    		if (key == K_UPARROW)
      			lanConfig_cursor = 1;

    		else
      			lanConfig_cursor = 0;
	}

	l =  Q_atoi(lanConfig_portname);

	if (l > 65535)
		l = lanConfig_port;

	else
		lanConfig_port = l;

	sprintf(lanConfig_portname, "%u", (unsigned int)lanConfig_port);
}

//=============================================================================
/* GAME OPTIONS MENU */

typedef struct
{
  char	*name;
  char	*description;
} level_t;

level_t	  levels[] =
{
  {"start", "Entrance"},  // 0

  {"e1m1", "Slipgate Complex"},	      // 1
  {"e1m2", "Castle of the Damned"},
  {"e1m3", "The Necropolis"},
  {"e1m4", "The Grisly Grotto"},
  {"e1m5", "Gloom Keep"},
  {"e1m6", "The Door To Chthon"},
  {"e1m7", "The House of Chthon"},
  {"e1m8", "Ziggurat Vertigo"},

  {"e2m1", "The Installation"},	      // 9
  {"e2m2", "Ogre Citadel"},
  {"e2m3", "Crypt of Decay"},
  {"e2m4", "The Ebon Fortress"},
  {"e2m5", "The Wizard's Manse"},
  {"e2m6", "The Dismal Oubliette"},
  {"e2m7", "Underearth"},

  {"e3m1", "Termination Central"},	// 16
  {"e3m2", "The Vaults of Zin"},
  {"e3m3", "The Tomb of Terror"},
  {"e3m4", "Satan's Dark Delight"},
  {"e3m5", "Wind Tunnels"},
  {"e3m6", "Chambers of Torment"},
  {"e3m7", "The Haunted Halls"},

  {"e4m1", "The Sewage System"},	// 23
  {"e4m2", "The Tower of Despair"},
  {"e4m3", "The Elder God Shrine"},
  {"e4m4", "The Palace of Hate"},
  {"e4m5", "Hell's Atrium"},
  {"e4m6", "The Pain Maze"},
  {"e4m7", "Azure Agony"},
  {"e4m8", "The Nameless City"},

  {"end", "Shub-Niggurath's Pit"},	// 31

  {"dm1", "Place of Two Deaths"},	// 32
  {"dm2", "Claustrophobopolis"},
  {"dm3", "The Abandoned Base"},
  {"dm4", "The Bad Place"},
  {"dm5", "The Cistern"},
  {"dm6", "The Dark Zone"}
};

//MED 01/06/97 added hipnotic levels
level_t	    hipnoticlevels[] =
{
   {"start", "Command HQ"},  // 0

   {"hip1m1", "The Pumping Station"},	       // 1
   {"hip1m2", "Storage Facility"},
   {"hip1m3", "The Lost Mine"},
   {"hip1m4", "Research Facility"},
   {"hip1m5", "Military Complex"},

   {"hip2m1", "Ancient Realms"},	  // 6
   {"hip2m2", "The Black Cathedral"},
   {"hip2m3", "The Catacombs"},
   {"hip2m4", "The Crypt"},
   {"hip2m5", "Mortum's Keep"},
   {"hip2m6", "The Gremlin's Domain"},

   {"hip3m1", "Tur Torment"},	    // 12
   {"hip3m2", "Pandemonium"},
   {"hip3m3", "Limbo"},
   {"hip3m4", "The Gauntlet"},

   {"hipend", "Armagon's Lair"},       // 16

   {"hipdm1", "The Edge of Oblivion"}		// 17
};

//PGM 01/07/97 added rogue levels
//PGM 03/02/97 added dmatch level
level_t	  roguelevels[] =
{
  {"start", "Split Decision"},
  {"r1m1",  "Deviant's Domain"},
  {"r1m2",  "Dread Portal"},
  {"r1m3",  "Judgement Call"},
  {"r1m4",  "Cave of Death"},
  {"r1m5",  "Towers of Wrath"},
  {"r1m6",  "Temple of Pain"},
  {"r1m7",  "Tomb of the Overlord"},
  {"r2m1",  "Tempus Fugit"},
  {"r2m2",  "Elemental Fury I"},
  {"r2m3",  "Elemental Fury II"},
  {"r2m4",  "Curse of Osiris"},
  {"r2m5",  "Wizard's Keep"},
  {"r2m6",  "Blood Sacrifice"},
  {"r2m7",  "Last Bastion"},
  {"r2m8",  "Source of Evil"},
  {"ctf1",    "Division of Change"}
};

typedef struct
{
  char	*description;
  int	firstLevel;
  int	levels;
} episode_t;

episode_t episodes[] =
{
  {"Welcome to Quake", 0, 1},
  {"Doomed Dimension", 1, 8},
  {"Realm of Black Magic", 9, 7},
  {"Netherworld", 16, 7},
  {"The Elder World", 23, 8},
  {"Final Level", 31, 1},
  {"Deathmatch Arena", 32, 6}
};

//MED 01/06/97	added hipnotic episodes
episode_t   hipnoticepisodes[] =
{
   {"Scourge of Armagon", 0, 1},
   {"Fortress of the Dead", 1, 5},
   {"Dominion of Darkness", 6, 6},
   {"The Rift", 12, 4},
   {"Final Level", 16, 1},
   {"Deathmatch Arena", 17, 1}
};

//PGM 01/07/97 added rogue episodes
//PGM 03/02/97 added dmatch episode
episode_t rogueepisodes[] =
{
  {"Introduction", 0, 1},
  {"Hell's Fortress", 1, 7},
  {"Corridors of Time", 8, 8},
  {"Deathmatch Arena", 16, 1}
};

int startepisode;
int startlevel;
int maxplayers;
qboolean m_serverInfoMessage = false;
double m_serverInfoMessageTime;

void M_Menu_GameOptions_f (void)
{
  key_dest = key_menu;
  m_state = m_gameoptions;
  m_entersound = true;
  if (maxplayers == 0)
    maxplayers = svs.maxclients;
  if (maxplayers < 2)
    maxplayers = svs.maxclientslimit;
}

int gameoptions_cursor_table[] = {40, 56, 64, 72, 80, 88, 96, 112, 120};

#define NUM_GAMEOPTIONS 9
int   gameoptions_cursor;

void M_GameOptions_Draw (void)
{
  qpic_t  *p;
  int	x;

  M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
  p = Draw_CachePic ("gfx/p_multi.lmp");
  M_DrawPic ( (320-p->width)/2, 4, p);

  M_DrawTextBox (152, 32, 10, 1);
  M_Print (160, 40, "begin game");

  M_Print (0, 56, "	 Max players");
  M_Print (160, 56, va("%i", maxplayers) );

  M_Print (0, 64, "	   Game Type");
  if (coop.value)
    M_Print (160, 64, "Cooperative");
  else
    M_Print (160, 64, "Deathmatch");

  M_Print (0, 72, "	   Teamplay");
  if (rogue)
  {
    char *msg;

    switch((int)teamplay.value)
    {
      case 1: msg = "No Friendly Fire"; break;
      case 2: msg = "Friendly Fire"; break;
      case 3: msg = "Tag"; break;
      case 4: msg = "Capture the Flag"; break;
      case 5: msg = "One Flag CTF"; break;
      case 6: msg = "Three Team CTF"; break;
      default: msg = "Off"; break;
    }
    M_Print (160, 72, msg);
  }
  else
  {
    char *msg;

    switch((int)teamplay.value)
    {
      case 1: msg = "No Friendly Fire"; break;
      case 2: msg = "Friendly Fire"; break;
      default: msg = "Off"; break;
    }
    M_Print (160, 72, msg);
  }

  M_Print (0, 80, "	       Skill");
  if (skill.value == 0)
    M_Print (160, 80, "Easy difficulty");
  else if (skill.value == 1)
    M_Print (160, 80, "Normal difficulty");
  else if (skill.value == 2)
    M_Print (160, 80, "Hard difficulty");
  else
    M_Print (160, 80, "Nightmare difficulty");

  M_Print (0, 88, "	  Frag Limit");
  if (fraglimit.value == 0)
    M_Print (160, 88, "none");
  else
    M_Print (160, 88, va("%i frags", (int)fraglimit.value));

  M_Print (0, 96, "	  Time Limit");
  if (timelimit.value == 0)
    M_Print (160, 96, "none");
  else
    M_Print (160, 96, va("%i minutes", (int)timelimit.value));

  M_Print (0, 112, "	     Episode");
   //MED 01/06/97 added hipnotic episodes
   if (hipnotic)
      M_Print (160, 112, hipnoticepisodes[startepisode].description);
   //PGM 01/07/97 added rogue episodes
   else if (rogue)
      M_Print (160, 112, rogueepisodes[startepisode].description);
   else
      M_Print (160, 112, episodes[startepisode].description);

  M_Print (0, 120, "	       Level");
   //MED 01/06/97 added hipnotic episodes
   if (hipnotic)
   {
      M_Print (160, 120, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].description);
      M_Print (160, 128, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name);
   }
   //PGM 01/07/97 added rogue episodes
   else if (rogue)
   {
      M_Print (160, 120, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].description);
      M_Print (160, 128, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name);
   }
   else
   {
      M_Print (160, 120, levels[episodes[startepisode].firstLevel + startlevel].description);
      M_Print (160, 128, levels[episodes[startepisode].firstLevel + startlevel].name);
   }

// line cursor
  M_DrawCharacter (144, gameoptions_cursor_table[gameoptions_cursor], 12+((int)(realtime*4)&1));

  if (m_serverInfoMessage)
  {
    if ((realtime - m_serverInfoMessageTime) < 5.0)
    {
      x = (320-26*8)/2;
      M_DrawTextBox (x, 138, 24, 4);
      x += 8;
      M_Print (x, 146, "  More than 4 players	");
      M_Print (x, 154, " requires using command ");
      M_Print (x, 162, "line parameters; please ");
      M_Print (x, 170, "   see techinfo.txt.	");
    }
    else
    {
      m_serverInfoMessage = false;
    }
  }
}


void M_NetStart_Change (int dir)
{
  int count;

  switch (gameoptions_cursor)
  {
  case 1:
    maxplayers += dir;
    if (maxplayers > svs.maxclientslimit)
    {
      maxplayers = svs.maxclientslimit;
      m_serverInfoMessage = true;
      m_serverInfoMessageTime = realtime;
    }
    if (maxplayers < 2)
      maxplayers = 2;
    break;

  case 2:
    Cvar_SetValue ("coop", coop.value ? 0 : 1);
    break;

  case 3:
    if (rogue)
      count = 6;
    else
      count = 2;

    Cvar_SetValue ("teamplay", teamplay.value + dir);
    if (teamplay.value > count)
      Cvar_SetValue ("teamplay", 0);
    else if (teamplay.value < 0)
      Cvar_SetValue ("teamplay", count);
    break;

  case 4:
    Cvar_SetValue ("skill", skill.value + dir);
    if (skill.value > 3)
      Cvar_SetValue ("skill", 0);
    if (skill.value < 0)
      Cvar_SetValue ("skill", 3);
    break;

  case 5:
    Cvar_SetValue ("fraglimit", fraglimit.value + dir*10);
    if (fraglimit.value > 100)
      Cvar_SetValue ("fraglimit", 0);
    if (fraglimit.value < 0)
      Cvar_SetValue ("fraglimit", 100);
    break;

  case 6:
    Cvar_SetValue ("timelimit", timelimit.value + dir*5);
    if (timelimit.value > 60)
      Cvar_SetValue ("timelimit", 0);
    if (timelimit.value < 0)
      Cvar_SetValue ("timelimit", 60);
    break;

  case 7:
    startepisode += dir;
  //MED 01/06/97 added hipnotic count
    if (hipnotic)
      count = 6;
  //PGM 01/07/97 added rogue count
  //PGM 03/02/97 added 1 for dmatch episode
    else if (rogue)
      count = 4;
    else if (registered.value)
      count = 7;
    else
      count = 2;

    if (startepisode < 0)
      startepisode = count - 1;

    if (startepisode >= count)
      startepisode = 0;

    startlevel = 0;
    break;

  case 8:
    startlevel += dir;
    //MED 01/06/97 added hipnotic episodes
    if (hipnotic)
      count = hipnoticepisodes[startepisode].levels;
  //PGM 01/06/97 added hipnotic episodes
    else if (rogue)
      count = rogueepisodes[startepisode].levels;
    else
      count = episodes[startepisode].levels;

    if (startlevel < 0)
      startlevel = count - 1;

    if (startlevel >= count)
      startlevel = 0;
    break;
  }
}

void M_GameOptions_Key (int key)
{
  switch (key)
  {
  case K_ESCAPE:
    M_Menu_Net_f ();
    break;

  case K_UPARROW:
    S_LocalSound ("misc/menu1.wav");
    gameoptions_cursor--;
    if (gameoptions_cursor < 0)
      gameoptions_cursor = NUM_GAMEOPTIONS-1;
    break;

  case K_DOWNARROW:
    S_LocalSound ("misc/menu1.wav");
    gameoptions_cursor++;
    if (gameoptions_cursor >= NUM_GAMEOPTIONS)
      gameoptions_cursor = 0;
    break;

  case K_LEFTARROW:
    if (gameoptions_cursor == 0)
      break;
    S_LocalSound ("misc/menu3.wav");
    M_NetStart_Change (-1);
    break;

  case K_RIGHTARROW:
    if (gameoptions_cursor == 0)
      break;
    S_LocalSound ("misc/menu3.wav");
    M_NetStart_Change (1);
    break;

  case K_ENTER:
    S_LocalSound ("misc/menu2.wav");
    if (gameoptions_cursor == 0)
    {
      if (sv.active)
	Cbuf_AddText ("disconnect\n");
      Cbuf_AddText ("listen 0\n");  // so host_netport will be re-examined
      Cbuf_AddText ( va ("maxplayers %u\n", maxplayers) );
      SCR_BeginLoadingPlaque ();

      if (hipnotic)
	Cbuf_AddText ( va ("map %s\n", hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name) );
      else if (rogue)
	Cbuf_AddText ( va ("map %s\n", roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name) );
      else
	Cbuf_AddText ( va ("map %s\n", levels[episodes[startepisode].firstLevel + startlevel].name) );

      return;
    }

    M_NetStart_Change (1);
    break;
  }
}

//=============================================================================
/* SEARCH MENU */

qboolean  searchComplete = false;
double	  searchCompleteTime;

void M_Menu_Search_f (void)
{
	key_dest = key_menu;
	m_state = m_search;
	m_entersound = false;
	slistSilent = true;
	slistLocal = false;
	searchComplete = false;
	NET_Slist_f();
}


void M_Search_Draw (void)
{
  qpic_t  *p;
  int x;

  p = Draw_CachePic ("gfx/p_multi.lmp");
  M_DrawPic ( (320-p->width)/2, 4, p);
  x = (320/2) - ((12*8)/2) + 4;
  M_DrawTextBox (x-8, 32, 12, 1);
  M_Print (x, 40, "Searching...");

  if(slistInProgress)
  {
    NET_Poll();
    return;
  }

  if (! searchComplete)
  {
    searchComplete = true;
    searchCompleteTime = realtime;
  }

  if (hostCacheCount)
  {
    M_Menu_ServerList_f ();
    return;
  }

  M_PrintWhite ((320/2) - ((22*8)/2), 64, "No Quake servers found");
  if ((realtime - searchCompleteTime) < 3.0)
    return;

  M_Menu_LanConfig_f ();
}


void M_Search_Key (int key)
{
}

//=============================================================================
/* SLIST MENU */

int   slist_cursor;
qboolean slist_sorted;

void M_Menu_ServerList_f (void)
{
  key_dest = key_menu;
  m_state = m_slist;
  m_entersound = true;
  slist_cursor = 0;
  m_return_onerror = false;
  m_return_reason[0] = 0;
  slist_sorted = false;
}


void M_ServerList_Draw (void)
{
  int	n;
  char	string [64];
  qpic_t  *p;

  if (!slist_sorted)
  {
    if (hostCacheCount > 1)
    {
      int i,j;
      hostcache_t temp;
      for (i = 0; i < hostCacheCount; i++)
	for (j = i+1; j < hostCacheCount; j++)
	  if (strcmp(hostcache[j].name, hostcache[i].name) < 0)
	  {
	    Q_memcpy(&temp, &hostcache[j], sizeof(hostcache_t));
	    Q_memcpy(&hostcache[j], &hostcache[i], sizeof(hostcache_t));
	    Q_memcpy(&hostcache[i], &temp, sizeof(hostcache_t));
	  }
    }
    slist_sorted = true;
  }

  p = Draw_CachePic ("gfx/p_multi.lmp");
  M_DrawPic ( (320-p->width)/2, 4, p);
  for (n = 0; n < hostCacheCount; n++)
  {
    if (hostcache[n].maxusers)
      sprintf(string, "%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name,
	      hostcache[n].map, (unsigned int)hostcache[n].users,
	      (unsigned int)hostcache[n].maxusers);
    else
      sprintf(string, "%-15.15s %-15.15s\n", hostcache[n].name, hostcache[n].map);
    M_Print (16, 32 + 8*n, string);
  }
  M_DrawCharacter (0, 32 + slist_cursor*8, 12+((int)(realtime*4)&1));

  if (*m_return_reason)
    M_PrintWhite (16, 148, m_return_reason);
}


void M_ServerList_Key (int k)
{
  switch (k)
  {
  case K_ESCAPE:
    M_Menu_LanConfig_f ();
    break;

  case K_SPACE:
    M_Menu_Search_f ();
    break;

  case K_UPARROW:
  case K_LEFTARROW:
    S_LocalSound ("misc/menu1.wav");
    slist_cursor--;
    if (slist_cursor < 0)
      slist_cursor = hostCacheCount - 1;
    break;

  case K_DOWNARROW:
  case K_RIGHTARROW:
    S_LocalSound ("misc/menu1.wav");
    slist_cursor++;
    if (slist_cursor >= hostCacheCount)
      slist_cursor = 0;
    break;

  case K_ENTER:
    S_LocalSound ("misc/menu2.wav");
    m_return_state = m_state;
    m_return_onerror = true;
    slist_sorted = false;
    key_dest = key_game;
    m_state = m_none;
    Cbuf_AddText ( va ("connect \"%s\"\n", hostcache[slist_cursor].cname) );
    break;

  default:
    break;
  }

}

//=============================================================================
/* Menu Subsystem */


void M_Init (void)
{
	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddCommand ("menu_save", M_Menu_Save_f);
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_detail", M_Menu_Detail_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);	// 2026-11-07: the Video Modes page (bindable, and for tests)
	Cmd_AddCommand ("help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
}


void M_Draw (void)
{
	if (m_state == m_none || key_dest != key_menu)
		return;

	if (!m_recursiveDraw)
	{
		scr_copyeverything = 1;

		if (scr_con_current)
		{
			Draw_ConsoleBackground (vid.height);
			VID_UnlockBuffer ();
			S_ExtraUpdate ();
			VID_LockBuffer ();
		}

		else
			Draw_FadeScreen ();

		scr_fullupdate = 0;
  	}

	else
	{
		m_recursiveDraw = false;
	}

	switch (m_state)
	{
		case m_none:
			break;

		case m_main:
			M_Main_Draw ();
			break;

		case m_singleplayer:
			M_SinglePlayer_Draw ();
			break;

		case m_load:
			M_Load_Draw ();
			break;

		case m_save:
			M_Save_Draw ();
			break;

		case m_multiplayer:
			M_MultiPlayer_Draw ();
			break;

		case m_setup:
			M_Setup_Draw ();
			break;

		case m_net:
			M_Net_Draw ();
			break;

		case m_options:
			M_Options_Draw ();
			break;

		case m_detail:
			M_Detail_Draw ();
			break;

		case m_keys:
			M_Keys_Draw ();
			break;

		case m_sound:	// 2026-11-06
			M_Sound_Draw ();
			break;

		case m_video:	// 2026-11-06
			M_Video_Draw ();
			break;

		case m_videomodes:	// 2026-11-13
			M_VideoModes_Draw ();
			break;

		case m_help:
			M_Help_Draw ();
			break;

		case m_quit:
			M_Quit_Draw ();
			break;

		case m_serialconfig:
			M_SerialConfig_Draw ();
			break;

		case m_modemconfig:
			M_ModemConfig_Draw ();
			break;

		case m_lanconfig:
			M_LanConfig_Draw ();
			break;

		case m_gameoptions:
			M_GameOptions_Draw ();
			break;

		case m_search:
			M_Search_Draw ();
			break;

		case m_slist:
			M_ServerList_Draw ();
			break;

		default:
			break; // Cowcat 2019
	}

	if (m_entersound)
	{
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();
}


void M_Keydown (int key)
{
	switch (m_state)
	{
		case m_none:
			return;

		case m_main:
			M_Main_Key (key);
			return;

		case m_singleplayer:
			M_SinglePlayer_Key (key);
			return;

		case m_load:
			M_Load_Key (key);
			return;

		case m_save:
			M_Save_Key (key);
			return;

		case m_multiplayer:
			M_MultiPlayer_Key (key);
			return;

		case m_setup:
			M_Setup_Key (key);
			return;

		case m_net:
			M_Net_Key (key);
			return;

		case m_options:
			M_Options_Key (key);
			return;

		case m_detail:
			M_Detail_Key (key);
			return;

		case m_keys:
			M_Keys_Key (key);
			return;

		case m_video:	// 2026-11-06
			M_Video_Key (key);
			return;

		case m_videomodes:	// 2026-11-13
			M_VideoModes_Key (key);
			return;

		case m_help:
			M_Help_Key (key);
			return;

		case m_quit:
			M_Quit_Key (key);
			return;

		case m_serialconfig:
			M_SerialConfig_Key (key);
			return;

		case m_modemconfig:
			M_ModemConfig_Key (key);
			return;

		case m_lanconfig:
			M_LanConfig_Key (key);
			return;

		case m_gameoptions:
			M_GameOptions_Key (key);
			return;

		case m_search:
			M_Search_Key (key);
			break;

		case m_sound:	// 2026-11-06
			M_Sound_Key (key);
			return;

		case m_slist:
			M_ServerList_Key (key);
			return;

		default:
			return; // Cowcat 2019
	}
}


void M_ConfigureNetSubsystem(void)
{
	// enable/disable net systems to match desired config

	Cbuf_AddText ("stopdemo\n");

	if (SerialConfig || DirectConfig)
	{
		Cbuf_AddText ("com1 enable\n");
	}

	if (IPXConfig || TCPIPConfig)
		net_hostport = lanConfig_port;
}


/*-------------------------------
Details Menu by SuRgEoN
-------------------------------*/
#define NUM_DETAILS_CMDS	15

int	details_cursor = 0; 
int	details_cursor_table[] = {40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128, 136, 144, 152};
int	odddetails_cursor_table[] = {8, 14, 20, 26, 32, 38, 44, 50, 56, 62, 68, 74, 80, 86, 92};		

int	setup_bfil;
int	setup_gfil;
int	setup_flame;
int	setup_fskip;
int	setup_bskip;
int	setup_dyna;
int	setup_muzz;
int	setup_fastl;
int	setup_r2g;
int	setup_fcont;
int	setup_mcap;
int	setup_expl;
int	setup_tele;
int	setup_alert;
int	setup_power;

#ifndef GLQUAKE
extern cvar_t d_mipcap, d_mipscale; 
#else
extern cvar_t gl_polyblend, gl_flashblend, gl_picmip, gl_playermip, gl_subdivide_size;
#endif

extern cvar_t r_flames;

void M_Menu_Detail_f (void) 
{ 
	key_dest = key_menu; 
	m_state = m_detail; 
	m_entersound = true;
 
//filtering
	setup_bfil = 0;
	setup_gfil = 0;
	setup_flame = (int)r_flames.value;

//skipping
	setup_fskip = 0; 
	setup_bskip = 0; 

//light
#ifdef GLQUAKE
	setup_dyna = (int)r_dynamic.value; 
#else
	setup_dyna = 0;
#endif
	setup_r2g = 0; 

	setup_muzz = 0;
#ifndef GLQUAKE
	setup_fastl = 0;

//sky & textures
	setup_fcont = 0;
	setup_mcap = (int)d_mipcap.value;
#else
	setup_fcont = (int)gl_subdivide_size.value;
	setup_mcap = (int)gl_picmip.value;
#endif

//particles
	setup_expl = 0;
	setup_tele = 0;

//palette shift
#ifndef GLQUAKE
	setup_alert = 0;
	setup_power = 0;
#else
	setup_power = setup_alert = (int)gl_polyblend.value;
#endif
} 
 
 
void M_Detail_Draw (void) 
{ 
	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") ); 
	M_Print (64, 16, "Use arrowkeys.   Enter accepts");
	M_Print (64, 24, "   Space resets to defaults   ");

	M_PrintWhite  (64, 40, "   bodyfilter"); 
		if(setup_bfil > 0) 
		M_Print (200, 40, "on");
		else
		M_Print (200, 40, "off");
	M_PrintWhite  (64, 48, "    gibfilter"); 
		if(setup_gfil > 0) 
		M_Print (200, 48, "on");
		else
		M_Print (200, 48, "off");
	M_PrintWhite  (64, 56, "       flames"); 
		if(setup_flame > 0) 
		M_Print (200, 56, "on");
		else
		M_Print (200, 56, "off");
	M_PrintWhite  (64, 64, "    frameskip"); 
		if(setup_fskip < 1)
		M_Print (200, 64, "off");
		else if (setup_fskip <= 1)
		M_Print (200, 64, "1/5");
		else if (setup_fskip <= 2)
		M_Print (200, 64, "1/3");
		else if (setup_fskip > 2)
		M_Print (200, 64, "1/2");
	M_PrintWhite  (64, 72, "     beamskip"); 
		if(setup_bskip < 1)
		M_Print (200, 72, "off");
		else if (setup_bskip <= 1)
		M_Print (200, 72, "1/3");
		else if (setup_bskip > 1)
		M_Print (200, 72, "1/2");
	M_PrintWhite  (64, 80, "dynamic light"); 
		if(setup_dyna > 0) 
		M_Print (200, 80, "on");
		else
		M_Print (200, 80, "off");
	M_PrintWhite  (64, 88, "muzzleflashes"); 
		if(setup_muzz < 1)
		M_Print (200, 88, "off");
		else if (setup_muzz <= 1)
		M_Print (200, 88, "normal");
		else if (setup_muzz > 1)
		M_Print (200, 88, "own off");
	M_PrintWhite  (64, 96, "    fastlight"); 
		if(setup_fastl > 0) 
		M_Print (200, 96, "on");
		else
		M_Print (200, 96, "off");
	M_PrintWhite (64, 104, "rock->grenade"); 
		if(setup_r2g > 0) 
		M_Print (200, 104, "on");
		else
		M_Print (200, 104, "off");
	M_PrintWhite (64, 112, "fast contents"); 
#ifndef GLQUAKE
		if(setup_fcont > 0) 
#else
		if(setup_fcont > 128)
#endif 
		M_Print (200, 112, "on");
		else
		M_Print (200, 112, "off");
	M_PrintWhite (64, 120, "   explosions"); 
		if(setup_expl < 1)
		M_Print (200, 120, "off");
		else if (setup_expl <= 1)
		M_Print (200, 120, "normal");
		else if (setup_expl > 1)
		M_Print (200, 120, "restricted");
	M_PrintWhite (64, 128, "    teleports"); 
		if(setup_tele < 1)
		M_Print (200, 128, "off");
		else if (setup_tele <= 1)
		M_Print (200, 128, "normal");
		else if (setup_tele > 1)
		M_Print (200, 128, "restricted");
	M_PrintWhite (64, 136, " dmg/bf flash"); 
		if(setup_alert > 0) 
		M_Print (200, 136, "on");
		else
		M_Print (200, 136, "off");
	M_PrintWhite (64, 144, " pup palshift"); 
		if(setup_power > 0) 
		M_Print (200, 144, "on");
		else
		M_Print (200, 144, "off");
	M_PrintWhite  (56, 152, "texture detail"); 
		if(setup_mcap < 1)
		M_Print (200, 152, "max");
		else if (setup_mcap <= 1)
		M_Print (200, 152, "high");
		else if (setup_mcap <= 2)
		M_Print (200, 152, "med");
		else if (setup_mcap > 2)
		M_Print (200, 152, "low");
 
	M_DrawCharacter (176, details_cursor_table [details_cursor], 12+((int)(realtime*4)&1)); 
} 
 
 
void M_Detail_Key (int k)
{
	switch (k) 
	{ 
	case K_ESCAPE: 
		M_Menu_Options_f (); 
		break; 
 
	case K_UPARROW: 
		S_LocalSound ("misc/menu1.wav"); 
		details_cursor--; 
		if (details_cursor < 0) 
			details_cursor = NUM_DETAILS_CMDS-1; 
		break; 
 
	case K_DOWNARROW: 
		S_LocalSound ("misc/menu1.wav"); 
		details_cursor++; 
		if (details_cursor >= NUM_DETAILS_CMDS) 
			details_cursor = 0; 
		break; 
 
	case K_LEFTARROW: 
		S_LocalSound ("misc/menu3.wav"); 
		if (details_cursor == 0) 
			if(setup_bfil > 0)
			setup_bfil = 0; 
		if (details_cursor == 1)
			if(setup_gfil > 0) 
			setup_gfil = 0; 
		if (details_cursor == 2)
			if(setup_flame > 0) 
			setup_flame = 0; 
		if (details_cursor == 3)
		{
		if (setup_fskip <= 1)
		setup_fskip = 0;
		else if (setup_fskip <= 2)
		setup_fskip = 1;
		else if (setup_fskip > 2)
		setup_fskip = 2;
		}
		if (details_cursor == 4)
		{
		if (setup_bskip <= 1)
		setup_bskip = 0;
		else if (setup_bskip > 1)
		setup_bskip = 1;
		}
		if (details_cursor == 5)
			if(setup_dyna > 0) 
			setup_dyna = 0; 
		if (details_cursor == 6)
		{
		if (setup_muzz <= 1)
		setup_muzz = 0;
		else if (setup_muzz > 1)
		setup_muzz = 1;
		}
		if (details_cursor == 7)
			if(setup_fastl > 0) 
			setup_fastl = 0; 
		if (details_cursor == 8)
			if(setup_r2g > 0) 
			setup_r2g = 0; 
		if (details_cursor == 9)
#ifndef GLQUAKE
			if(setup_fcont > 0) 
			setup_fcont = 0; 
#else
			if(setup_fcont > 128)
			setup_fcont = 128;
#endif 

		if (details_cursor == 10)
		{
		if (setup_expl <= 1)
		setup_expl = 0;
		else if (setup_expl > 1)
		setup_expl = 1;
		}
		if (details_cursor == 11)
		{
		if (setup_tele <= 1)
		setup_tele = 0;
		else if (setup_tele > 1)
		setup_tele = 1;
		}
		if (details_cursor == 12)
			if(setup_alert > 0) 
			setup_alert = 0; 
		if (details_cursor == 13)
			if(setup_power > 0) 
			setup_power = 0; 
		if (details_cursor == 14)
		{
		if(setup_mcap < 1)
		setup_mcap = 1;
		else if (setup_mcap < 2)
		setup_mcap = 2;
		else if (setup_mcap >= 2)
		setup_mcap = 3;
		}
		break;
 
	case K_RIGHTARROW: 
		S_LocalSound ("misc/menu3.wav"); 
		if (details_cursor == 0) 
			if(setup_bfil < 1) 
			setup_bfil = 1; 
		if (details_cursor == 1)
			if(setup_gfil < 1) 
			setup_gfil = 1; 
		if (details_cursor == 2)
			if(setup_flame < 1) 
			setup_flame = 1; 
		if (details_cursor == 3)
		{
		if(setup_fskip < 1)
		setup_fskip = 1;
		else if (setup_fskip < 2)
		setup_fskip = 2;
		else if (setup_fskip >= 2)
		setup_fskip = 3;
		}
		if (details_cursor == 4)
		{
		if(setup_bskip < 1)
		setup_bskip = 1;
		else if (setup_bskip >= 1)
		setup_bskip = 2;
		}
		if (details_cursor == 5)
			if(setup_dyna < 1) 
			setup_dyna = 1; 
		if (details_cursor == 6)
		{
		if(setup_muzz < 1)
		setup_muzz = 1;
		else if (setup_muzz >= 1)
		setup_muzz = 2;
		}
		if (details_cursor == 7)
			if(setup_fastl < 1) 
			setup_fastl = 1; 
		if (details_cursor == 8)
			if(setup_r2g < 1) 
			setup_r2g = 1; 
		if (details_cursor == 9)
#ifndef GLQUAKE
			if(setup_fcont < 1) 
			setup_fcont = 1; 
#else
			if(setup_fcont < 1024) 
			setup_fcont = 1024; 
#endif
		if (details_cursor == 10)
		{
		if(setup_expl < 1)
		setup_expl = 1;
		else if (setup_expl >= 1)
		setup_expl = 2;
		}
		if (details_cursor == 11)
		{
		if(setup_tele < 1)
		setup_tele = 1;
		else if (setup_tele >= 1)
		setup_tele = 2;
		}
		if (details_cursor == 12)
			if(setup_alert < 1) 
			setup_alert = 1; 
		if (details_cursor == 13)
			if(setup_power < 1) 
			setup_power = 1; 
		if (details_cursor == 14)
		{
		if (setup_mcap <= 1)
		setup_mcap = 0;
		else if (setup_mcap <= 2)
		setup_mcap = 1;
		else if (setup_mcap > 2)
		setup_mcap = 2;
		}
		break; 

	case K_SPACE: //set vars to default
	S_LocalSound ("misc/menu3.wav"); 
	setup_bfil = 0; 
	setup_gfil = 0; 
	setup_flame = 1; 
#ifndef GLQUAKE
	setup_fcont = 0; 
#else
	setup_fcont = 128;
#endif 
	setup_fskip = 0; 
	setup_bskip = 0; 
	setup_dyna = 1;
	setup_fastl = 0;
	setup_muzz = 1;
	setup_r2g = 0;
	setup_expl = 1;
	setup_tele = 1;
	setup_alert = 1;
	setup_power = 1;
	setup_mcap = 0;
	break; 

	case K_ENTER: //set vars to new values

	Cvar_SetValue ("r_flames", setup_flame);

#ifdef GLQUAKE
	Cvar_SetValue ("r_dynamic", setup_dyna);
#endif
#ifndef GLQUAKE
	Cvar_SetValue ("d_mipcap", setup_mcap);
	if (setup_mcap == 2)
	Cvar_SetValue ("d_mipscale", 2);
	else if (setup_mcap == 3)
	Cvar_SetValue ("d_mipscale", 100);
	else
	Cvar_SetValue ("d_mipscale", 1);
#else
	Cvar_SetValue ("gl_subdivide_size", setup_fcont);
	Cvar_SetValue ("gl_polyblend", setup_power);
	if (setup_mcap == 2)
	Cvar_SetValue ("gl_picmip", 1);
	else if (setup_mcap == 3)
	Cvar_SetValue ("gl_picmip", 2);
	else
	Cvar_SetValue ("gl_picmip", 0);
#endif
	m_entersound = true; 
	M_Menu_Options_f (); 
	break; 
 

	default: 
		if (k < 32 || k > 127) 
			break; 
	} 
} 
