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


// gl_vidamiga.c - 19/01/2000 Massimiliano Tretene
// modified by SuRgEoN
// modded by Cowcat 2014

#ifdef __GNUC__
//#include <sys/ioctl.h>
#endif

#ifdef __VBCC__
#pragma amiga-align
#elif defined(WARPUP)
#pragma pack(push,2)
#endif

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <intuition/intuition.h>

//#if defined (__GNUC__) && defined (__PPC__)
//#include <powerup/ppcproto/exec.h>
//#else
#include <proto/exec.h>
#include <dos/dos.h>
#include <proto/dos.h>
//#endif

#if defined (__PPC__)
#if defined(__GNUC__)
#include <powerpc/powerpc.h>
#include <powerpc/powerpc_protos.h>
#else
#include <powerpc/powerpc.h>
#include <proto/powerpc.h>
#endif
#endif

#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>

#ifdef __VBCC__
#pragma default-align
#elif defined(WARPUP)
#pragma pack(pop)
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <mgl/gl.h>

#include "quakedef.h"

#include "SDI_compiler.h"
#include "keys_amiga.h"	 //added from quakewos phx.


#define WARP_WIDTH	320
#define WARP_HEIGHT	240
 
unsigned char rawkeyconv[] = 
{
	'`','1','2','3','4','5','6','7','8','9','0','-','=','\\',0,'0',
	'q','w','e','r','t','y','u','i','o','p','[',']',0,K_END,K_DOWNARROW,K_PGDN,
	'a','s','d','f','g','h','j','k','l',';','\'',K_F12,0,K_LEFTARROW,0,K_RIGHTARROW,
	K_F11,'z','x','c','v','b','n','m',',','.','/',0,0,K_HOME,K_UPARROW,K_PGUP,
	K_SPACE,K_BACKSPACE,K_TAB,K_ENTER,K_ENTER,K_ESCAPE,K_DEL,0,
	0,0,'-',0,K_UPARROW,K_DOWNARROW,K_RIGHTARROW,K_LEFTARROW,
	K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,K_F9,K_F10,'(',')','/','*','+',K_PAUSE,
	/* 2026-11-06: both Amiga keys are K_AMIGA (they were 0: no key, so the
	 * Amiga key could not be bound and Amiga-V never pasted) */
	K_SHIFT,K_SHIFT,0,K_CTRL,K_ALT,K_ALT,K_AMIGA,K_AMIGA,
	K_MOUSE1,K_MOUSE2,K_MOUSE3,0,0,0,0,0,
	/* 2026-11-06: all 128 raw codes. The table ended at $6A, so NewMouse's
	 * wheel codes ($7A/$7B) read whatever followed it: no key with a name,
	 * the wheel could not be bound ("isn't a valid key"). */
	0,0,0,0,0,0,0,0,0,0,K_MWHEELUP,K_MWHEELDOWN,0,0,0,0
};
/* indexed with (code & 0x7f): a table one short fails to compile here */
typedef char rawkeyconv_has_128[(sizeof(rawkeyconv) == 128) ? 1 : -1];

struct Window *QuakeWindow = NULL;

// windowplay 
BOOL	windowplay;
int	mouseused;
//

//GLboolean zbuffer = GL_TRUE; // Cowcat

#ifdef NOMOUSEASM
int m_move[2] = {0,0};
int *mousemove = m_move;
#endif

GLfloat mouse_x = 0.0, mouse_y = 0.0;
// Cowcat
//GLfloat mouse_z = 0.0; GLint offset = 0; 
//GLfloat fov = 70.0;
//GLfloat inf_w = 0.1;
//GLfloat zback = 1000.0;
//GLfloat alpha = 1.0;

int numb = 3; //triplebuffer
int bpp = 16;

GLboolean guardband = GL_FALSE; //surgeon

qboolean gl_warp = false;

unsigned int	d_8to24table[256];
//unsigned int	d_8to24table_fence[256]; // future fence textures support Cowcat 2019

//unsigned char d_15to8table[65536]; // not used Cowcat

static qboolean gl_videosync = true;
static qboolean gl_dithering = true;

// stupid problematic disc loader icon - Cowcat
int gl_lockmode = MGL_LOCK_MANUAL;	// no static (for draw_disc) 
qboolean ddisc = false;			// draw_disc disabled by default
qboolean cachetris = true;		// Cowcat
qboolean fixboundbox = false;		// Cowcat

static int scr_width, scr_height;
static qboolean UPDATE = FALSE;	// a new size waits for the end of the frame (VID_SwitchMode)
static int pending_w, pending_h;
static GLfloat gl_polyoffset = 0.0;

void VID_MenuDraw (void);
void VID_MenuKey (int);

int odd_display = 0; //surgeon

extern void M_DrawPic(int x, int y, qpic_t *pic);
extern void M_Print(int cx, int cy, char *str);
extern void M_DrawCharacter(int cx, int line, int num);
extern void M_Menu_Options_f(void);

#if !defined(__PPC__)
#include <fenv.h> 
fenv_t fpcr; //fpu rounding
#endif

/*-----------------------------------------------------------------------*/


int texture_mode = GL_LINEAR;

extern	int   gl_filter_min;
extern	int   gl_filter_max;
extern	cvar_t gl_max_size;

int	texture_extension_number = 1;

float	gldepthmin, gldepthmax;

cvar_t	gl_ztrick = {"gl_ztrick","0"};
cvar_t	gl_width = {"gl_width","320"};

/* 2026-11-06: the Video Modes page's settings (vid.h), saved; 0 = not set */
cvar_t	vid_width = {"vid_width", "0", true};
cvar_t	vid_height = {"vid_height", "0", true};
cvar_t	vid_bpp = {"vid_bpp", "16", true};
cvar_t	vid_buffers = {"vid_buffers", "3", true};	// kept for old config.cfg files; the mode is vid_bufmode since 1.4
cvar_t	vid_modeid = {"vid_modeid", "0", true};
cvar_t	vid_conscale = {"vid_conscale", "0", true};	// 2D scale: 0 automatic, else 1, 2, 3 ...
cvar_t	vid_vsync = {"vid_vsync", "1", true};		// 2026-11-07: vertical sync (the page's switch), on by default
cvar_t	vid_bufmode = {"vid_bufmode", "0", true};	// 2026-11-12: 0 two buffers with vsync (the default), 1 three by panning, 2 three by ChangeScreenBuffer

void VID_RegisterCvars (void)
{
	Cvar_RegisterVariable (&vid_width);
	Cvar_RegisterVariable (&vid_height);
	Cvar_RegisterVariable (&vid_bpp);
	Cvar_RegisterVariable (&vid_buffers);
	Cvar_RegisterVariable (&vid_modeid);
	Cvar_RegisterVariable (&vid_conscale);
	Cvar_RegisterVariable (&vid_vsync);
	Cvar_RegisterVariable (&vid_bufmode);
}

//cvar_t _windowed_mouse = {"_windowed_mouse", "1", true}; 
//cvar_t  freemouse = {"freemouse","0",false};

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

/* 2026-11-07: why the last live switch failed ("" = it did not), for the
 * Video Modes page (menu.c): a refusal only went to the console, which the
 * menu hides. RadeonGL's extension GL_RGL_resize_error gives the reason. */
char vid_switch_error[128];
#ifndef GL_RGL_RESIZE_ERROR
#define GL_RGL_RESIZE_ERROR	0x52474c05
#endif

static float vid_gamma = 1.0;

/* The startup gamma, as a cvar so the Brightness slider can write it and
 * config.cfg can restore it. ARCHIVED (the trailing true), and read by
 * Check_Gamma below -- which runs AFTER host.c execs config.cfg, so a saved
 * value is in place by then. Deliberately NOT the existing `gamma` cvar:
 * that one is archived with default "1", so every existing config.cfg holds
 * gamma "1" and reading it here would darken the game against today's
 * effective 0.7. */
cvar_t gl_gamma = {"gl_gamma", "0.7", true};

//qboolean is8bit = false;
qboolean isPermedia = false;
qboolean gl_mtexable = false;

/* Soft-hang investigation (2026-08-03), "-noextensions" 4th pass:
 * earlier attempts forced gl_fake_multitexture to 0 (routing through
 * R_BlendLightmaps' classic blend path) via this same non-cvar global
 * (then called gl_force_classic_lightmap) -- hardware-tested: real
 * screen-update/dirty-rect problems ("one big mess with the screen not
 * updating everywhere"), a different, separate bug in that path on
 * this port. Reverted to forcing fake_multitexture ON instead (closer
 * to the ORIGINAL, real root cause found earlier: R_RenderBrushPoly's
 * fake-multitexture branch is fine mechanically -- it's a real per-
 * surface unit0+unit1 draw MiniGLV3D's own multitextured dispatch
 * combines correctly -- the only actual bug is TexEnv[1] ending up
 * GL_REPLACE, from R_DrawMultitextureBuffer's hardcoded GL_REPLACE
 * argument, which discards the diffuse texture. Real fix this pass:
 * R_RenderBrushPoly now explicitly forces GL_MODULATE for unit 1
 * itself right before drawing, instead of trusting whatever
 * R_DrawMultitextureBuffer left over from the previous frame -- see
 * gl_rsurf.c. Renamed from gl_force_classic_lightmap now that it
 * forces the opposite thing. Still a plain global, not a cvar --
 * immune to config/archive overrides, exactly like gl_mtexable. */
qboolean gl_force_fake_multitex = false;

qboolean quadtex = false; // QuadTexture particles retro - Cowcat

/*-----------------------------------------------------------------------*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}

#if 0 // now on minigl - Cowcat 2019
//unsigned short *FakeMousePointer = 0;
UWORD *FakeMousePointer = 0; // like minigl 

void Pointer(struct Window *QuakeWindow)
{
	if (!FakeMousePointer)
	{
		//FakeMousePointer = (unsigned short *)Sys_Alloc(16,MEMF_CLEAR);	
		FakeMousePointer = (unsigned short *)Sys_Alloc(8, MEMF_CLEAR|MEMF_CHIP|MEMF_PUBLIC); // just like minigl       
		SetPointer(QuakeWindow, FakeMousePointer, 0, 0, 0, 0); 
	}
}

void DeletePointer(struct Window *QuakeWindow)
{	
	if(IntuitionBase) // fix 
		ClearPointer(QuakeWindow);

	Sys_Free(FakeMousePointer);

	FakeMousePointer = 0;
}
#endif

/* 2026-11-12: how RadeonGL shows three buffers, its variable RadeonGLFlip
 * ("pan" or "csb"), set LOCAL so it reaches only this process -- and, started
 * from a Shell, put back as it was when the game ends (a local variable
 * belongs to the Shell then). Libraries without the variable ignore it. */
static int	flip_had = -1;		// -1: not touched; 0: there was none; 1: flip_old
static char	flip_old[16];

static void SetFlipVar (qboolean csb)
{
	if (flip_had < 0)
		flip_had = (GetVar ("RadeonGLFlip", flip_old, sizeof (flip_old), GVF_LOCAL_ONLY) > 0) ? 1 : 0;

	SetVar ("RadeonGLFlip", csb ? "csb" : "pan", -1, GVF_LOCAL_ONLY);
}

static void FlipVarBack (void)
{
	if (flip_had == 1)
		SetVar ("RadeonGLFlip", flip_old, -1, GVF_LOCAL_ONLY);
	else if (flip_had == 0)
		DeleteVar ("RadeonGLFlip", GVF_LOCAL_ONLY);

	flip_had = -1;
}

void VID_Shutdown(void)
{     
	//DeletePointer(QuakeWindow);
#ifdef MINIGL_SHARED_CLIENT
	/* RadeonGL: Host_Shutdown reaches this after a failed MiniGLOpen too,
	 * when the dispatch table is still NULL. A call through it then jumps
	 * through the CPU vector table (guru 810000FF, AN_BadQuickInt: measured
	 * in FS-UAE 2026-11-02). A dispatch client has no MGLTerm; the library
	 * cleans up in MiniGLClose. */
	if (MiniGLDispatch)
	{
		mglClearPointer(); // new minigl - Cowcat 2022
		mglDeleteContext();
		MiniGLClose();
	}
#else
	mglClearPointer(); // new minigl - Cowcat 2022
	mglDeleteContext();
	MGLTerm();
#endif
	FlipVarBack ();
}

static void Check_Gamma (unsigned char *pal)
{
	float		f, inf;
	static		unsigned char palette[768];
	register	int   i;

	//static qboolean firsttime = false;
	//if (firsttime) return;
  
	/* -gamma wins, then gl_gamma, which the Brightness slider writes and
	 * config.cfg restores. The branch that used to be here picked 1.0 when
	 * GL_RENDERER named Voodoo or GL_VENDOR named 3Dfx and 0.7 otherwise;
	 * neither string ever appears on this platform, so 0.7 was the only
	 * reachable answer and is now simply gl_gamma's default. The visible
	 * behaviour of a fresh install is unchanged. */
	if ((i = COM_CheckParm("-gamma")) != 0)
		vid_gamma = Q_atof(com_argv[i+1]);

	else
		vid_gamma = gl_gamma.value;

	//firsttime=true;

	for (i=0 ; i<768 ; i++)
	{
		f = pow ( (pal[i]+1)/256.0 , vid_gamma );
		inf = f*255 + 0.5;

		if (inf < 0)
			inf = 0;

		if (inf > 255)
			inf = 255;

		palette[i] = inf;
	}

	memcpy (pal, palette, sizeof(palette));
}


#if 0 // Cowcat
void VID_ShiftPalette(unsigned char *p)
{
	//VID_SetPalette(p);
}
#endif 

void VID_SetPalette (unsigned char *palette)
{
	register byte		*pal;
	register unsigned	short r,g,b;
	register unsigned	v;
	register unsigned	short i;
	register unsigned	*table;
	int			r1,g1,b1;
	//int			k;
	//FILE			*f;
	//char			s[255];
	//float			dist, bestdist;
	//static qboolean	palflag = false;

	//
	// 8 8 8 encoding
	//
	pal = palette;
	table = d_8to24table;

	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

		if (COM_CheckParm ("-hicontrast")) 
		{ 
			// add some contrast 
			r1 = r - 200; r1 = 1.05 * r1; r1 = 200 + r1; 
			g1 = g - 200; g1 = 1.05 * g1; g1 = 200 + g1; 
			b1 = b - 200; b1 = 1.05 * b1; b1 = 200 + b1; 
 
			if (r1 > 255) r1 = 255;
			if (r1 < 0) r1 = 0; 
			if (g1 > 255) g1 = 255;
			if (g1 < 0) g1 = 0; 
			if (b1 > 255) b1 = 255;
			if (b1 < 0) b1 = 0; 

			v = (170<<0) + (r1<<24) + (g1<<16) + (b1<<8); // Cowcat
		} 
		
		//v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		//v = (255<<0) + (r<<24) + (g<<16) + (b<<8);
		//v = LittleLong((170<<24) + (r<<0) + (g<<8) + (b<<16));

		//if (COM_CheckParm ("-hicontrast")) // Cowcat
			//v = (170<<0) + (r1<<24) + (g1<<16) + (b1<<8); //

		else
			v = (170<<0) + (r<<24) + (g<<16) + (b<<8);

		*table++ = v;
	}

	d_8to24table[255] &= 0xffffff00;	// 255 is transparent

	#if 0 // Cowcat

	// JACK: 3D distance calcs - k is last closest, l is the distance.
	// FIXME: Precalculate this and cache to disk.

	if (palflag)
		return;

	palflag = true;

	COM_FOpenFile("glquake/15to8.pal", &f);

	if (f)
	{
		fread(d_15to8table, 1<<15, 1, f);
		fclose(f);
	}

	else
	{
		for (i=0; i < (1<<15); i++)
		{
			/* Maps
			000000000000000
			000000000011111 = Red  = 0x1F
			000001111100000 = Blue = 0x03E0
			111110000000000 = Grn  = 0x7C00
			*/

			r = ((i & 0x1F) << 3)+4;
			g = ((i & 0x03E0) >> 2)+4;
			b = ((i & 0x7C00) >> 7)+4;

			pal = (unsigned char *)d_8to24table;

			for (v=0,k=0,bestdist=10000.0; v<256; v++, pal+=4)
			{
				r1 = (int)r - (int)pal[0];
				g1 = (int)g - (int)pal[1];
				b1 = (int)b - (int)pal[2];
				dist = sqrt(((r1*r1)+(g1*g1)+(b1*b1)));

				if (dist < bestdist)
				{
					k=v;
					bestdist = dist;
				}
			}

			d_15to8table[i]=k;
		}

		sprintf(s, "%s/glquake", com_gamedir);
		Sys_mkdir (s);
		sprintf(s, "%s/glquake/15to8.pal", com_gamedir);

		if ((f = fopen(s, "wb")) != NULL)
		{
			fwrite(d_15to8table, 1<<15, 1, f);
			fclose(f);
		}
	}

	#endif
}


/* REAL BUG FIX (white-texture investigation, 2026-08-03): this used to
 * require an explicit "-extensions" command-line flag to enable real
 * ARB multitexture at all -- forgetting it (the normal case, since
 * nothing about a missing flag is visible/obvious) silently falls back
 * to glquake's own "fake multitexture" 2-pass rendering path instead of
 * this driver's real, working, hardware-confirmed combined-multitexture
 * shader path. MiniGLV3D's own driver-side `multitextured` dispatch only
 * checks "is a texture bound at unit 1 right now" -- it can't tell real
 * combined-multitexture intent apart from fake-multitexture's own
 * incidental unit-1 usage, so running with real multitexture OFF (the
 * previous default) produced wrong/inconsistent lightmap combine
 * results. Real ARB_multitexture support genuinely works correctly on
 * this port -- confirmed hardware-tested fixed once "-extensions" was
 * passed. Flipped the default: multitexture is now ON whenever the real
 * extension string is present, unless "-noextensions" is explicitly
 * passed. NOTE (2026-08-03, 2nd pass): "-noextensions" does NOT route
 * through the old fake-multitexture path as originally intended here --
 * that path has its own real, separate GL_REPLACE-dispatch bug (see
 * this function's own comment below) and was hardware-confirmed still
 * broken. "-noextensions" now forces classic single-texture-unit
 * lightmapping instead (real multitexture off AND fake multitexture
 * off), the one combination confirmed to render correctly without real
 * ARB multitexture. */
void CheckMultiTextureExtensions (void)
{
	/* REAL BUG FIX, 2nd pass (2026-08-03): a first attempt here forced
	 * gl_fake_multitexture to 1, on the theory that "-noextensions"
	 * should reproduce the OLD fake-multitexture fallback path (this
	 * function's header comment above still describes that original,
	 * now-corrected intent). Hardware-tested: still white textures.
	 * Root cause is a REAL, separate bug in gl_fake_multitexture==1's
	 * own code path -- R_RenderBrushPoly (gl_rsurf.c) draws the
	 * lightmap on unit 1 using whatever TexEnv[1] mode
	 * R_DrawMultitextureBuffer last set, which is hardcoded to
	 * GL_REPLACE for the ORIGINAL engine's unrelated deferred-buffer-
	 * flush semantics. On THIS driver, TexEnv[1]==GL_REPLACE dispatches
	 * to a real shader variant (draw.c's multitex_env==2 case) that
	 * outputs the lightmap texture ALONE, discarding the diffuse
	 * texture -- lightmaps are bright, hence solid white/fullbright
	 * surfaces. Confirmed working alternative (user's own report):
	 * "it looked fine last time when you totally switched off
	 * multitexture" -- i.e. BOTH gl_mtexable AND gl_fake_multitexture
	 * off, which routes lightmapping through R_BlendLightmaps' own
	 * classic single-texture-unit blend path instead (real
	 * glEnable(GL_BLEND) + glBlendFunc, unaffected by the REPLACE bug
	 * above).
	 *
	 * REAL BUG FIX, 3rd pass (2026-08-03): forcing gl_fake_multitexture
	 * to 0 via Cvar_SetValue here still gave white textures on real
	 * hardware -- gl_fake_multitexture has archive=true, and quake.rc's
	 * "exec" (Host_Init) is only QUEUED via Cbuf_InsertText, actually
	 * executed later by the main loop's own Cbuf_Execute, well after
	 * this function already ran, so anything quake.rc sets for this
	 * cvar silently overwrites whatever this forces. A 3rd pass fixed
	 * that specific problem by forcing classic lightmapping via a
	 * non-cvar global instead -- but hardware-tested THAT introduced a
	 * different, real bug (screen not updating everywhere), a separate
	 * problem in R_BlendLightmaps' path on this port. 4th pass: force
	 * fake_multitexture ON instead (via the same non-cvar-safe
	 * mechanism, renamed to gl_force_fake_multitex), and fix the REAL
	 * root-cause bug at its actual source -- see gl_rsurf.c's
	 * R_RenderBrushPoly, now forces GL_MODULATE for unit 1 explicitly
	 * instead of trusting R_DrawMultitextureBuffer's leftover
	 * GL_REPLACE state. */
	/* DEFAULT FLIPPED BACK (2026-09-08, at the user's request): the
	 * -noextensions pathway is the DEFAULT again, and real ARB multitexture
	 * is now opt-in via "-extensions". This restores the arrangement that
	 * existed before the 2026-08-03 flip described in this function's header
	 * comment above -- read that for why it was flipped the other way, since
	 * none of those findings are retracted: real ARB multitexture does work
	 * on this port, it is simply no longer what you get by default.
	 *
	 * The GL_MODULATE root-cause fix in gl_rsurf.c's R_RenderBrushPoly stays
	 * in place and is what makes THIS default render correctly at all -- do
	 * not "clean it up" as dead code just because the header comment above
	 * describes the fake-multitexture path as the non-default one.
	 *
	 * "-noextensions" still lands here and still behaves identically, so
	 * existing scripts and muscle memory keep working; it is simply redundant
	 * now. Passing both flags means "-extensions" wins. */
	if (!COM_CheckParm("-extensions"))
	{
		if (COM_CheckParm("-noextensions"))
			Con_Printf("Multitexture extensions disabled (-noextensions), forcing fake multitexture with real GL_MODULATE.\n");
		else
			Con_Printf("Multitexture extensions disabled (default -- pass -extensions to enable), forcing fake multitexture with real GL_MODULATE.\n");
		gl_force_fake_multitex = true;
		return;
	}

	if (strstr(gl_extensions, "GL_MGL_ARB_multitexture "))
	{
		Con_Printf("Multitexture extensions found (-extensions).\n");
		gl_mtexable = true;
	}
	else
	{
		/* FALLBACK (2026-09-08): "-extensions" asked for the ARB path but the
		 * driver does not advertise it. Leaving BOTH gl_mtexable and
		 * gl_force_fake_multitex false is not a safe middle ground -- it means
		 * gl_rmisc.c's `if (gl_mtexable || gl_force_fake_multitex)` never
		 * fires, so gl_texsort stays 1 and rendering goes down the texture-
		 * chain path (DrawTextureChains/R_BlendLightmaps) instead of
		 * R_DrawSequentialPoly. That is the path the water smearing /
		 * duplication bug actually lives in; see that same comment in
		 * gl_rmisc.c. Fall back to fake multitexture, which is a real
		 * unit0+unit1 single-pass draw this driver handles correctly.
		 *
		 * Not reachable with a current minigl.library, which always advertises
		 * GL_MGL_ARB_multitexture -- only against an older one. */
		Con_Printf("-extensions requested but GL_MGL_ARB_multitexture is not advertised; falling back to fake multitexture with real GL_MODULATE.\n");
		gl_force_fake_multitex = true;
	}
}


/*
===============
GL_Init
===============
*/
void GL_Init (void)
{  
	gl_vendor = glGetString (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = glGetString (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);

	if (strstr(gl_renderer, "(virge)"))
		gl_polyoffset = -0.0005;

	gl_version = glGetString (GL_VERSION);
	Con_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = glGetString (GL_EXTENSIONS);
	Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

	CheckMultiTextureExtensions();

	/*fpcr code fix for Permedia users on 68k blitz version */
  
	#if !defined(__PPC__)

	int i;

	if (strstr(gl_renderer, "(permedia)"))
		fpcr= FE_LDBLPREC|FE_TOWARDZERO;

	else
		fpcr= FE_LDBLPREC|FE_TONEAREST;

	if (i = COM_CheckParm("-fpcr"))
	{
		fpcr = Q_atoi(com_argv[i+1]);
	}
   
	fesetenv(&fpcr);  


	#endif

	gl_lightmap_format = MGL_UNSIGNED_SHORT_4_4_4_4;

	glClearColor (1,0,0,0);

	glCullFace(GL_FRONT);	

	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.5);	//surgeon - was 0.666

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	glShadeModel (GL_FLAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

/*
=================
GL_BeginRendering

=================
*/

void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = scr_width;
	*height = scr_height;

	mglLockDisplay();
}


void GL_EndRendering (void)
{
	mglSwitchDisplay(); //performs unlock

	/* Exit cleanly on a V3D render-pipeline stall instead of soft-locking
	 * (2026-09-10): MiniGLV3D sets this sticky flag when the deferred render
	 * wait fails (context.c MGLFlushPendingRender); without a consumer every
	 * following frame waits out the full timeout again and the game just
	 * loops. Same hook Quake2 (amiga/gla_imp.c) and ioq3 (amiga_glimp.c)
	 * already have; the disk log survives the exit. */
	{
		extern int g_mglv3d_last_render_wait_result;
		if (g_mglv3d_last_render_wait_result != 0)
			Sys_Error("V3D render pipeline stalled (wait result=%d, 1=error 2=timeout) -- exiting cleanly rather than soft-locking\n",
			          g_mglv3d_last_render_wait_result);
	}

	/* Was '#if 0' (Cowcat: "this really doesn't work well"). Enabled now
	 * that MiniGLV3D's MGLResizeContext keeps the textures and all render
	 * state across the resize, as the original MiniGL did. */
	/* 2026-11-06: a new size, live (VID_SwitchMode: the Video Modes page, and
	 * gl_width): the library opens a new screen and keeps the textures
	 * (mglResizeContext). Any size now, not only 4:3; a library that
	 * refuses keeps the old size, and so does GLQuake. */
	if (UPDATE) 
	{
		int	ow, oh;

		ow = scr_width;
		oh = scr_height;

		if (mglResizeContext(pending_w, pending_h))
		{
			scr_width = pending_w;
			scr_height = pending_h;
			vid_switch_error[0] = 0;
			Con_Printf("Video mode %dx%d\n", scr_width, scr_height);
		}
		else
		{
			const char	*why = NULL;

			if (gl_extensions && strstr (gl_extensions, "GL_RGL_resize_error"))
				why = (const char *) glGetString (GL_RGL_RESIZE_ERROR);

			if (why == NULL || why[0] == 0)
				why = "the library kept the old size";

			sprintf (vid_switch_error, "%dx%d did not open: %.90s", pending_w, pending_h, why);
			Con_Printf("Video mode %s (still %dx%d)\n", vid_switch_error, ow, oh);
		}

		VID_SetSizes ();

		/* Draw_Init (gl_draw.c) stamped the resolution it loaded at into the
		 * console background pic, and Draw_ConsoleBackground draws it at that
		 * size while placing it with the current vid.height -- refresh it, or
		 * it keeps the old size and sits too high. Con_CheckResize (called
		 * every frame from SCR_SetUpToDrawConsole) handles the console text. */
		{
			extern qpic_t *conback;

			conback->width = vid.width;
			conback->height = vid.height;
		}

		/* The reopened display has a NEW input window, and the driver opens
		 * it without IDCMP -- it has no UserPort until we ask for one, and
		 * Sys_SendKeyEvents' GetMsg(QuakeWindow->UserPort) would read NULL.
		 * Same sequence as VID_Init above. */
		if (QuakeWindow = mglGetInputWindowHandle())
	#ifdef NOMOUSEASM
			ModifyIDCMP(QuakeWindow, IDCMP_RAWKEY|IDCMP_MOUSEBUTTONS|IDCMP_MOUSEMOVE|IDCMP_DELTAMOVE);
	#else
			ModifyIDCMP(QuakeWindow, IDCMP_RAWKEY|IDCMP_MOUSEBUTTONS);
	#endif
		/* 2026-11-06: the key that asked for the switch (the menu's Enter)
		 * went up in the closed window: Quake would take the next press of
		 * it for an autorepeat and drop it (MEASURED on the A4000D: the
		 * first Enter after a switch was lost) */
		Key_ClearStates ();
		vid.recalc_refdef = 1;

		if (gl_lockmode == MGL_LOCK_MANUAL)
			mglLockDisplay();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		mglUnlockDisplay();
      
		UPDATE = FALSE;
	}
}

#if 0	//not used  - cowcat
qboolean VID_Is8bit(void)
{
	return is8bit;
}


void VID_Init8bitPalette() 
{
	// Check for 8bit Extensions and initialize them.
	int i;

	char thePalette[256*3];
	char *oldPalette, *newPalette;

	if(COM_CheckParm("-no8bit"))
		return;

	Con_SafePrintf("8-bit colortable enabled.\n");
	oldPalette = (char *) d_8to24table; //d_8to24table3dfx;
	newPalette = thePalette;

	for (i=0;i<256;i++)
	{
		*newPalette++ = *oldPalette++;
		*newPalette++ = *oldPalette++;
		*newPalette++ = *oldPalette++;
		oldPalette++;
	}

	glColorTable(GL_COLOR_TABLE, GL_RGB, 256, GL_RGB, GL_UNSIGNED_BYTE, newPalette);
	is8bit = true;
}
#endif //

void ChangeVideoSync_f (void)
{
	if (Cmd_Argc() == 1)
	{
		Con_Printf ("GL_VIDEOSYNC %d\n", gl_videosync);
		return;
	}

	VID_SetVSync (Q_atoi(Cmd_Argv(1)) != 0);
}

/* 2026-11-12: vid_buffering 0|1|2, the Buffering item from the console (and
 * +vid_buffering on the command line, for tests): 0 two buffers with
 * vertical sync, 1 three by panning, 2 three by ChangeScreenBuffer */
void ChangeBuffering_f (void)
{
	int	m;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("vid_buffering %d (0: 2, vsync; 1: 3, ScrollVPort; 2: 3, ChangeScreenBuffer)\n", VID_GetBuffering ());
		return;
	}

	m = Q_atoi(Cmd_Argv(1));

	if (m < 0 || m > 2)
	{
		Con_Printf ("vid_buffering: 0, 1 or 2\n");
		return;
	}

	VID_SetBuffering (m);
}

/* 2026-11-07: vertical sync, the Video Modes page's switch. The library
 * (RadeonGL 0.7 and up) holds each present until the beam has left the
 * screen: no tearing when turning, at the price of waiting for the beam.
 * Saved as vid_vsync; vid_buffers 2 turns it on as well (MiniGL's custom). */
/* 2026-11-07: the depth of the screen the library opened (it falls back to
 * 16 bits where a size has no 32-bit mode), for the Video Modes page */
int VID_GetBpp (void)
{
	struct Window	*w;

	w = mglGetInputWindowHandle ();

	if (w && w->WScreen)
		return (GetBitMapAttr (w->WScreen->RastPort.BitMap, BMA_DEPTH) > 16) ? 32 : 16;

	return bpp;
}

qboolean VID_GetVSync (void)
{
	return gl_videosync;
}

void VID_SetVSync (qboolean on)
{
	gl_videosync = on;
	Cvar_SetValue ("vid_vsync", on ? 1 : 0);
	mglEnableSync (gl_videosync);
}

/* 2026-11-12: the Video Modes page's Buffering item -- 0: two buffers, the
 * copy with vertical sync; 1: three, panning a screen three times as tall
 * (ScrollVPort); 2: three, ChangeScreenBuffer. RadeonGL's triple-buffer
 * builds switch at once (the same size asked for again opens the screen
 * the new way); other libraries keep the old way until the next start. */
int VID_GetBuffering (void)
{
	if (numb == 2)
		return 0;

	return (vid_bufmode.value == 2) ? 2 : 1;
}

void VID_SetBuffering (int mode)
{
	numb = (mode == 0) ? 2 : 3;
	vid.numpages = numb;
	Cvar_SetValue ("vid_bufmode", mode);

	if (numb == 3)
		SetFlipVar (mode == 2);

	mglChooseNumberOfBuffers (numb);

	if (numb == 2)
		VID_SetVSync (true);	// two buffers: the copy, with vertical sync

	vid_switch_error[0] = 0;
	pending_w = scr_width;
	pending_h = scr_height;
	UPDATE = TRUE;
}

void ChangeDithering_f (void)
{
	if (Cmd_Argc() == 1)
	{
		Con_Printf ("GL_DITHERING IS %d\n", gl_dithering);
		return;
	}

	gl_dithering = Q_atoi(Cmd_Argv(1)); 

	if(gl_dithering)
		glEnable(GL_DITHER);

	else
		glDisable(GL_DITHER);
}

void ChangeLockMode_f (void)
{
	if (Cmd_Argc() == 1)
	{
		if (gl_lockmode == MGL_LOCK_SMART) Con_Printf ("SMART\n");
		else if (gl_lockmode == MGL_LOCK_AUTOMATIC) Con_Printf ("AUTOMATIC\n");
		else if (gl_lockmode == MGL_LOCK_MANUAL) Con_Printf ("MANUAL\n");
		else Con_Printf ("UNKNOWN\n");
		return;
	}
  
	if (Q_strcasecmp(Cmd_Argv(1),"SMART") == 0) gl_lockmode = MGL_LOCK_SMART;
	if (Q_strcasecmp(Cmd_Argv(1),"AUTOMATIC") == 0) gl_lockmode = MGL_LOCK_AUTOMATIC;
	if (Q_strcasecmp(Cmd_Argv(1),"MANUAL") == 0) gl_lockmode = MGL_LOCK_MANUAL;

	mglLockMode(gl_lockmode);
}

/* gl_width W [H]: a new size (H: W * 3 / 4 if not given, as before) */
void ChangeVideoMode_f (void)
{
	int	w;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("GL_WIDTH is %d (%dx%d)\n", scr_width, scr_width, scr_height);
		return;
	}

	w = Q_atoi(Cmd_Argv(1));
	VID_SwitchMode (w, (Cmd_Argc() > 2) ? Q_atoi(Cmd_Argv(2)) : w * 3 / 4);
}

/* 2026-11-06: vid's sizes from scr_width x scr_height (VID_Init and every
 * live switch) */
void VID_SetSizes (void)
{
	int	k;

	/* 2026-11-06: the 2D (menus, console, status bar) at a whole multiple
	 * of the screen's pixels, so that the 8-pixel font stays sharp and
	 * readable: the largest k that keeps the 2D at least 640 wide and 200
	 * high (1280x720: 640x360; 1600x1200: 800x600; 1024x768: 1:1), or
	 * vid_conscale's k. The 3D view keeps every pixel (R_SetupGL scales). */
	k = (int)vid_conscale.value;

	if (k < 1)
	{
		k = scr_width / 640;

		if (k < 1)
			k = 1;
	}

	while (k > 1 && (scr_width / k < 320 || scr_height / k < 200))
		k--;

	vid.conwidth = scr_width / k;
	vid.conwidth &= 0xfff8; // make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	vid.conheight = scr_height / k;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
	gl_width.value = vid.width;
}

/* 2026-11-06: w x h from the end of this frame on (GL_EndRendering) */
void VID_SwitchMode (int w, int h)
{
	if (w < 320 || h < 200 || w > 2048 || h > 2048)
	{
		Con_Printf ("Video mode %dx%d: not offered\n", w, h);
		return;
	}

	if (w == scr_width && h == scr_height)
		return;

	pending_w = w;
	pending_h = h;
	UPDATE = TRUE;
}

void VID_GetSize (int *w, int *h)
{
	*w = scr_width;
	*h = scr_height;
}

/* 2026-11-06, the Video Modes page: the library's screen modes
 * (mglGetSupportedScreenModes) between 5:4 and 16:9 (half-height modes such
 * as 640x200 would squash the picture), each size once, sorted; a library
 * without the call gets the common sizes. */
static vidmode_t *list_out;
static int list_n, list_max;

static GLboolean CollectMode (MGLScreenMode *m)
{
	float	r;
	int	i;

	if (list_n >= list_max)
		return GL_TRUE;		// enough: stop

	if (m->width < 320 || m->height < 200 || m->width > 2048 || m->height > 2048)
		return GL_FALSE;

	r = (float)m->width / (float)m->height;

	if (r < 1.2f || r > 1.9f)
		return GL_FALSE;

	for (i = 0; i < list_n; i++)
	{
		if (list_out[i].width == m->width && list_out[i].height == m->height)
			return GL_FALSE;
	}

	list_out[list_n].width = m->width;
	list_out[list_n].height = m->height;
	list_out[list_n].id = (unsigned long)m->id;
	list_n++;
	return GL_FALSE;
}

int VID_ListModes (vidmode_t *modes, int max)
{
	static const short common[][2] = {{320, 240}, {400, 300}, {512, 384}, {640, 400}, {640, 480},
					  {800, 600}, {1024, 768}, {1280, 720}, {1280, 1024}};
	vidmode_t	t;
	int		i, j;

	list_out = modes;
	list_n = 0;
	list_max = max;
	mglGetSupportedScreenModes (CollectMode);

	if (list_n == 0)
	{
		for (i = 0; i < (int)(sizeof(common) / sizeof(common[0])) && i < max; i++)
		{
			modes[i].width = common[i][0];
			modes[i].height = common[i][1];
			modes[i].id = 0;
		}

		list_n = i;
	}

	for (i = 1; i < list_n; i++)	// by width, then height
	{
		t = modes[i];

		for (j = i; j > 0 && (modes[j - 1].width > t.width
			|| (modes[j - 1].width == t.width && modes[j - 1].height > t.height)); j--)
			modes[j] = modes[j - 1];

		modes[j] = t;
	}

	return list_n;
}

void ChangePolyOffset_f (void)
{
	if (Cmd_Argc() == 1)
	{
		Con_Printf ("GL_POLYOFFSET is %f\n", gl_polyoffset);
		return;
	}

	gl_polyoffset = Q_atof(Cmd_Argv(1)); 

	mglSetZOffset(gl_polyoffset);
}

void ChangeWarp_f (void)
{
	if (Cmd_Argc() == 1)
	{
		Con_Printf ("gl_warp is %d\n", gl_warp);
		return;
	}

	gl_warp = Q_atof(Cmd_Argv(1)); 
}


void ShowLightmapFormat_f (void)
{
	extern int gl_lightmap_format;	// gl_draw.c; chosen in GL_BuildLightmaps (2026-11-06)

	if (gl_lightmap_format == GL_LUMINANCE)
		Con_Printf ("lightmapformat: GL_LUMINANCE\n");

	else if (gl_lightmap_format == MGL_UNSIGNED_SHORT_4_4_4_4)
		Con_Printf ("lightmapformat: MGL_UNSIGNED_SHORT_4_4_4_4\n");

	else if (COM_CheckParm ("-lm_LUMINANCE"))
		Con_Printf ("lightmapformat: GL_LUMINANCE\n");

	else if (COM_CheckParm ("-lm_ALPHA"))
		Con_Printf ("lightmapformat: GL_ALPHA\n");

	else if (COM_CheckParm ("-lm_RGB"))
		Con_Printf ("lightmapformat: MGL_UNSIGNED_SHORT_5_6_5\n");

	else if (COM_CheckParm ("-lm_RGBA"))
		Con_Printf ("lightmapformat: GL_RGBA\n");

	else
		Con_Printf ("lightmapformat: MGL_UNSIGNED_SHORT_4_4_4_4\n");
}

static qboolean firstinit = true;

/* mglv3d_log() CONSOLIDATED (2026-09-02): glquake used to carry its own
 * inline implementation here (dos.library Open/Seek/Write/Close against
 * host_parms.basedir, gated by MGLV3D_NO_LOGGING), separate from
 * Quake2's/ioq3's own per-game copies -- three independently-maintained
 * implementations that had already drifted out of sync at least once
 * before (see project_minigl_v3d_glquake_newgame_guru_gcc_vbcc.md).
 * Replaced by linking ONE shared implementation from
 * MiniGLV3D/demos/mglv3d_log_noop.c (showcase builds, does nothing) or
 * mglv3d_log_progdir.c (debug builds, writes PROGDIR:mglv3d_debug.log)
 * -- see reference_mglv3d_log_progdir.md. Swap which one this build
 * script links, don't maintain per-game copies of the real thing. */

/* BUILD VERSION MARKER: bumped by hand on every rebuild so a captured
 * mglv3d_debug.log can always be checked against the exact source state
 * that produced it, instead of trusting file-transfer timing/clock-skew
 * between machines -- kept as a permanent debugging aid (not specific
 * to any one investigation), logged as the very first thing VID_Init
 * does. */
#define MGLV3D_GLQUAKE_BUILD_VERSION 253

/* Exposes the build version to other compilation units (MiniGLV3D's own
 * context.c, e.g. its STALE LOCK DIAG) so a log line is self-identifying
 * without needing to cross-reference this file's own VID_Init log line --
 * useful since mglv3d_debug.log accumulates across multiple test runs. */
int g_mglv3d_glquake_build_version = MGLV3D_GLQUAKE_BUILD_VERSION;

void VID_Init(unsigned char *palette)
{
	int	i;
	char	gldir[MAX_OSPATH];
	char	buildbuf[64];

	/* Permanent marker (2026-08-05): always called, regardless of which
	 * mglv3d_log() this build links -- the noop variant just does
	 * nothing with it, matching what MGLV3D_NO_LOGGING used to gate here
	 * before mglv3d_log() itself was consolidated into a swappable
	 * external source (2026-09-02). */
	{
		extern void mglv3d_log(const char *msg);
		sprintf(buildbuf, "===== BUILD VERSION %d =====\n", MGLV3D_GLQUAKE_BUILD_VERSION);
		mglv3d_log(buildbuf);
	}

	if (firstinit)
	{
		S_Init();
		CDAudio_Init ();
		firstinit = false;
	}

	//Cvar_RegisterVariable (&freemouse);
	Cvar_RegisterVariable (&gl_ztrick);

	Cmd_AddCommand ("gl_lightmapformat", &ShowLightmapFormat_f);
	Cmd_AddCommand ("gl_videosync", &ChangeVideoSync_f);
	Cmd_AddCommand ("vid_buffering", &ChangeBuffering_f);
	Cmd_AddCommand ("gl_lockmode", &ChangeLockMode_f);
	Cmd_AddCommand ("gl_dithering", &ChangeDithering_f);
	Cmd_AddCommand ("gl_width", &ChangeVideoMode_f);
	Cmd_AddCommand ("gl_polyoffset", &ChangePolyOffset_f);
	Cmd_AddCommand ("gl_warp", &ChangeWarp_f);
  
	// texturemode
	if ((i = COM_CheckParm("-texturemode")) != 0)
	{
		if (Q_strcasecmp(com_argv[i+1],"GL_NEAREST") == 0) texture_mode = GL_NEAREST;
		if (Q_strcasecmp(com_argv[i+1],"GL_LINEAR") == 0) texture_mode = GL_LINEAR;
		if (Q_strcasecmp(com_argv[i+1],"GL_NEAREST_MIPMAP_NEAREST") == 0) texture_mode = GL_NEAREST_MIPMAP_NEAREST;
		if (Q_strcasecmp(com_argv[i+1],"GL_NEAREST_MIPMAP_LINEAR") == 0) texture_mode = GL_NEAREST_MIPMAP_LINEAR;
		if (Q_strcasecmp(com_argv[i+1],"GL_LINEAR_MIPMAP_NEAREST") == 0) texture_mode = GL_LINEAR_MIPMAP_NEAREST;
		if (Q_strcasecmp(com_argv[i+1],"GL_LINEAR_MIPMAP_LINEAR") == 0) texture_mode = GL_LINEAR_MIPMAP_LINEAR;

		gl_filter_min = texture_mode;
		gl_filter_max = texture_mode;
	}  
  
	// quadtexture - Cowcat (see gl_rmisc.c)
	if (i = COM_CheckParm("-quadtex"))
		quadtex = true;

	// dither
	if ((i = COM_CheckParm("-dither")) != 0)
	{
		if (Q_strcasecmp(com_argv[i+1],"ON") == 0) gl_dithering = GL_TRUE;
		if (Q_strcasecmp(com_argv[i+1],"OFF") == 0) gl_dithering = GL_FALSE;
	}

	// lockmode
	if ((i = COM_CheckParm("-lockmode")) != 0)
	{
		if (Q_strcasecmp(com_argv[i+1],"SMART") == 0) gl_lockmode = MGL_LOCK_SMART;
		if (Q_strcasecmp(com_argv[i+1],"AUTOMATIC") == 0) gl_lockmode = MGL_LOCK_AUTOMATIC;
		if (Q_strcasecmp(com_argv[i+1],"MANUAL") == 0) gl_lockmode = MGL_LOCK_MANUAL;
	}  

	// Cowcat
	if (i = COM_CheckParm("-ddisc")) ddisc = true;	// draw_disc
	if (i = COM_CheckParm("-nocachetris")) cachetris = false; // Cowcat
	if (i = COM_CheckParm("-fixboundbox")) fixboundbox = true; // Cowcat

	// number of buffers: the saved Buffering item (vid_bufmode, 1.4: two
	// with vertical sync unless the player chose three -- the old
	// vid_buffers "3" every config.cfg carries is not read any more), the
	// command line wins
	numb = (vid_bufmode.value == 1 || vid_bufmode.value == 2) ? 3 : 2;

	if ((i = COM_CheckParm("-buf")) != 0)
		numb = Q_atoi(com_argv[i+1]);
  
	if (numb < 2)
		numb = 2;

	if (numb > 3)
		numb = 3;   
  
	if (numb == 2 || vid_vsync.value)
		gl_videosync = true;

	else
		gl_videosync = false;

	// width
	scr_width = 320; //gl_width.value;
	scr_height = scr_width * 3 / 4;

	/* 2026-11-06: the saved Video Modes choice (vid_width, vid_height,
	 * vid_bpp; config.cfg ran before VID_Init) -- -width, -height and -bpp
	 * on the command line win for their run */
	if (vid_width.value >= 320)
	{
		scr_width = (int)vid_width.value;
		scr_height = (vid_height.value >= 200) ? (int)vid_height.value : scr_width * 3 / 4;
	}

	/* 2026-11-07 (PLAN F1): 32 bits as well -- RadeonGL draws on a B8G8R8A8
	 * screen for anything above 16 (24 means 32) */
	if (vid_bpp.value == 15 || vid_bpp.value == 16)
		bpp = (int)vid_bpp.value;
	else if (vid_bpp.value == 24 || vid_bpp.value == 32)
		bpp = 32;
  
	if ((i = COM_CheckParm("-width")) != 0)
	{
		scr_width = Q_atoi(com_argv[i+1]);
		scr_height = scr_width * 3 / 4;
	}  

	// height
	if ((i = COM_CheckParm("-height")) != 0)
	{
		scr_height = Q_atoi(com_argv[i+1]);
	}  

	// bpp
	if ((i = COM_CheckParm("-bpp")) != 0)
	{
		bpp = Q_atoi(com_argv[i+1]);

		if (bpp < 15) bpp = 15;
		if (bpp > 16) bpp = 32;		// 2026-11-07: 24 and 32 give a 32-bit screen
	}  

	//guardband clipping
	if (COM_CheckParm("-guardband"))
	{
		guardband = GL_TRUE;
	}

	// set vid parameters
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	VID_SetSizes ();	// vid.conwidth ... vid.aspect from scr_width, scr_height
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.numpages = 2 + numb - 2;

#ifdef MINIGL_SHARED_CLIENT
	/* RadeonGL: open minigl.library and take its ABI-5 dispatch table;
	 * MGLInit runs inside the library. Same messages as the shipped binary. */
	Con_Printf("Opening minigl.library...\n");
	if (!MiniGLOpen())
		Sys_Error("Unable to open minigl.library V%d or obtain a compatible dispatch table\n", MINIGL_VERSION);
#else
	Con_Printf("Calling MGLInit...\n");
	MGLInit();
#endif

	mglChooseGuardBand(guardband);
	
	//Con_Printf("Setting Vertex Buffer Size to 3000...\n");  //why tell?
	mglChooseVertexBufferSize(4096); //

	mglChooseMtexBufferSize(6000);

	//mglChooseTMU1Sort(1024, 1024+64); //1024 is the base texnum for lightmaps

	// windowmode, windowplay & mouse parm check
	
	mouseused =0;

	if ((i = COM_CheckParm("-nomouse")) == 0)
		mouseused = 1;

	if ((i = COM_CheckParm("-windowmode")) != 0)
	{
		if(mouseused != 0 )
			windowplay = TRUE;  //windowplay check
	 
		Con_Printf("Window mode ON\n");
		mglChooseWindowMode(GL_TRUE);		
	}  
	 
	else
	{  
		//Con_Printf("Setting pixel depth to %d...\n",bpp);
		//mglChoosePixelDepth(bpp);

		Con_Printf("Setting number of buffers to %d...\n", numb);

		if (numb == 3)
			SetFlipVar (vid_bufmode.value == 2);	// 2026-11-12: panning or ChangeScreenBuffer

		mglChooseNumberOfBuffers(numb);

		Con_Printf("gl_videosync is %d\n", gl_videosync);

		if ((i = COM_CheckParm("-closewb")) != 0)
		{
			Con_Printf("Closing the Workbench...\n");
			mglProposeCloseDesktop(GL_TRUE);
		}   
	}   
  
	Con_Printf("Setting pixel depth to %d...\n",bpp);

	mglChoosePixelDepth(bpp);

	//mglChooseStencilBuffer(1); // future 2019 - Cowcat

	Con_Printf("Creating context...\n");

	if (mglCreateContext(0,0, scr_width, scr_height))	// the screen's pixels, not the 2D's units (vid_conscale)
	{
#if defined(MGLV3D_EXITPOINT) && MGLV3D_EXITPOINT == 1
		VID_Shutdown();
		exit(0);
#endif
		Con_Printf("Switching sync...\n");
		mglEnableSync(gl_videosync);

		mglLockMode(gl_lockmode);

		glEnable(GL_DITHER);

		/* REAL BUG FIX (2026-08-03): mglGetWindowHandle() (== v3dWindow)
		 * is always NULL in fullscreen mode -- same trap MGLMainLoop
		 * already hit and got a dedicated fix for (inputWindow,
		 * MiniGLV3D's context.h/others.c). QuakeWindow staying NULL
		 * meant Sys_SendKeyEvents' own `if (QuakeWindow)` guard never
		 * ran at all in fullscreen -- "no keys register at all".
		 * mglGetInputWindowHandle() (new MiniGLV3D API, mirrors
		 * MGLMainLoop's own fix) returns the real backdrop window
		 * fullscreen mode opens specifically for IDCMP input. */
		if (QuakeWindow = mglGetInputWindowHandle())
	#ifdef NOMOUSEASM
			ModifyIDCMP(QuakeWindow, IDCMP_RAWKEY|IDCMP_MOUSEBUTTONS|IDCMP_MOUSEMOVE|IDCMP_DELTAMOVE);
	#else
			ModifyIDCMP(QuakeWindow, IDCMP_RAWKEY|IDCMP_MOUSEBUTTONS);
	#endif

		//Pointer(QuakeWindow); // now done by minigl - Cowcat 2019

		if(windowplay);
			mglSetPointer(); // new Cowcat 2022

		GL_Init();
#if defined(MGLV3D_EXITPOINT) && MGLV3D_EXITPOINT == 2
		VID_Shutdown();
		exit(0);
#endif

		glHint(MGL_W_ONE_HINT, GL_FASTEST);

		//global setting for amiga since there is no quality loss
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

		//glHint(GL_FOG_HINT, GL_FASTEST); // test

		mglSetZOffset(gl_polyoffset);

		sprintf (gldir, "%s/glquake", com_gamedir);
		Sys_mkdir (gldir);

		Check_Gamma(palette);
		VID_SetPalette(palette);

		Con_SafePrintf ("Video mode %dx%d initialized.\n", scr_width, scr_height);
#if defined(MGLV3D_EXITPOINT) && MGLV3D_EXITPOINT == 3
		VID_Shutdown();
		exit(0);
#endif

		vid.recalc_refdef = 1;	      // force a surface cache flush
	}
   
	else
		Sys_Error("no context, exiting...\n");

}

void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

//#ifdef __GNUC__
#if 0

void Sys_SendKeyEvents (void)
{
	struct IntuiMessage *imsg;
	int kn;
  
	if (QuakeWindow)
	{
		while (imsg = (struct IntuiMessage *)GetMsg(QuakeWindow->UserPort))
		{
		#ifdef NOMOUSEASM
			if(imsg->Class==IDCMP_MOUSEMOVE)
			{
				m_move[0] = (int)imsg->MouseX;
				m_move[1] = (int)imsg->MouseY;
			}
			else
		#endif

			if (imsg->Class & (IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS))
			{
				kn = (int)rawkeyconv[imsg->Code & 0x7f];

				if (imsg->Code & IECODE_UP_PREFIX)
				{
					Key_Event(kn,false);
				}

				else
				{
					Key_Event(kn,true);
				}

				if (imsg->Code==NM_WHEEL_UP || imsg->Code==NM_WHEEL_DOWN)
					Key_Event(kn, false);

				/*
				if (shutdown_keyboard)
				{
					shutdown_keyboard=0;
					return;
				}
				*/
			}
      
			ReplyMsg((struct Message *)imsg);
		}      
	}
}

#else 
 
// getkeysevents from quakewos phx version

int Sys_GetKeyEvents(void *port, void *msgarray, int arraysize)
{ 
	#if defined (__PPC__)

	extern int GetMessages68k();
	struct PPCArgs args;

	args.PP_Code = (APTR)GetMessages68k;
	args.PP_Offset = 0;
	args.PP_Flags = 0;
	args.PP_Stack = NULL;
	args.PP_StackSize = 0;
	args.PP_Regs[PPREG_A0] = (ULONG)msgarray;
	args.PP_Regs[PPREG_A1] = (ULONG)port;
	args.PP_Regs[PPREG_D0] = arraysize;

	Run68K(&args);
	return args.PP_Regs[PPREG_D0];

	#else

	return GetMessages68k((struct MsgPort *)port, (struct MsgStruct *)msgarray, arraysize);

	#endif
}

#define ENCODEKEY(code, qual) (int)(((int)code & 0x7F) | ((int)qual << 8))

void Sys_SendKeyEvents (void)
{
	int	i, kn;
	ULONG	code;
	UWORD	rawkey;

	if (QuakeWindow)
	{
		struct MsgStruct events[50];
		int messages = Sys_GetKeyEvents(QuakeWindow->UserPort, events, 50);

		for (i=0; i<messages; i++)
		{
			if (events[i].Class==IDCMP_RAWKEY || events[i].Class==IDCMP_MOUSEBUTTONS)
			{
				kn = (int)rawkeyconv[events[i].Code & 0x7f];
				code = events[i].Code;
				rawkey = events[i].rawkey;

				if (code & IECODE_UP_PREFIX)
					Key_Event(kn, false, ENCODEKEY(code, rawkey));
					//Key_Event(kn, false);

				else
					Key_Event(kn, true, ENCODEKEY(code, rawkey));
					//Key_Event(kn, true);

				if (code==NM_WHEEL_UP || code==NM_WHEEL_DOWN)
					Key_Event(kn, false, -1);
					//Key_Event(kn, false);
			}
		}
	}
}


#endif //end vbbc new getkeysevents

#if 0
/*
================
VID_MenuDraw
================
*/

#define MAX_COLUMN_SIZE 11
#define MAX_MODEDESCS	33

typedef struct
{
	int	modenum;
	char	*desc;
	int	iscur;
}	modedesc_t;

static modedesc_t   modedescs[33];
int	numvidmodes=11;
static int vid_wmodes,vid_line;
int	vid_column_size;

void VID_MenuDraw (void)
{
	qpic_t	    *p;
	char	    *ptr;
	int	    nummodes, i, j, column, row, dup;
	char	    temp[100];

	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	
	M_Print (9*8, 36 + MAX_COLUMN_SIZE * 8 + 8*3, "Press Enter to set mode");
	//ptr = VID_GetModeDescription (vid_modenum);
	sprintf (temp, "D to make %s the default", ptr);
	M_Print (6*8, 36 + MAX_COLUMN_SIZE * 8 + 8*5, temp);
	//ptr = VID_GetModeDescription ((int)_vid_default_mode.value);

	/*
	if (ptr)
	{
		sprintf (temp, "Current default is %s", ptr);
		M_Print (7*8, 36 + MAX_COLUMN_SIZE * 8 + 8*6, temp);
	}
	*/

	M_Print (15*8, 36 + MAX_COLUMN_SIZE * 8 + 8*8, "Esc to exit");

	row = 36 + (vid_line % vid_column_size) * 8;
	column = 8 + (vid_line / vid_column_size) * 13*8;

	M_DrawCharacter (column, row, 12+((int)(realtime*4)&1));
  
}

/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
	switch (key)
	{

		case K_ESCAPE:
			S_LocalSound ("misc/menu1.wav");
			M_Menu_Options_f ();
			break;
  
		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			vid_line--;
  
			if (vid_line < 0) vid_line = vid_wmodes - 1;
			break;
  
		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			vid_line++;

			if (vid_line >= vid_wmodes)
			vid_line = 0;
			break;
  
		case K_LEFTARROW:
			S_LocalSound ("misc/menu1.wav");
			vid_line -= vid_column_size;
  
			if (vid_line < 0)
			{
				vid_line += ((vid_wmodes + (vid_column_size - 1)) / vid_column_size) * vid_column_size;
  
				while (vid_line >= vid_wmodes)
					vid_line -= vid_column_size;
			}
			break;
  
		case K_RIGHTARROW:
			S_LocalSound ("misc/menu1.wav");
			vid_line += vid_column_size;
  
			if (vid_line >= vid_wmodes)
			{
				vid_line -= ((vid_wmodes + (vid_column_size - 1)) / vid_column_size) * vid_column_size;
  
				while (vid_line < 0)
					vid_line += vid_column_size;
			}
			break;
  
		case K_ENTER:
			S_LocalSound ("misc/menu1.wav");
			Cvar_SetValue("gl_width",modedescs[vid_line].modenum);
			//vid_modenum=modedescs[vid_line].modenum;
			//config_notify=1;
			break;
		/*
		case 'D':
		case 'd':
			S_LocalSound ("misc/menu1.wav");
			Cvar_SetValue ("_vid_default_mode", modedescs[vid_line].modenum);
			vid_modenum=modedescs[vid_line].modenum;
			break;
		*/

		default:
		break;
  }
}
#endif

