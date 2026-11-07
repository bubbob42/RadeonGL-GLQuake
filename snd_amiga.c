/*
**  snd_amiga.c
**
**  Written by Frank Wille <frank@phoenix.owl.de>
**  AHI support by Jarmo Laakkonen <jami.laakkonen@kolumbus.fi>
**
*/

#include "quakedef.h"

static int use_ahi;
static int snd_inuse;	// 0 none, 1 Paula, 2 AHI (SND_DeviceInUse)
/* 2026-11-06: the command line counts for the first start only; after the
 * Sound page's Apply its settings win (sound.h) */
int snd_use_cmdline = 1;

/* from AHI */
extern void SNDDMA_ShutdownAHI(void);
extern qboolean SNDDMA_InitAHI(void);
extern int SNDDMA_GetDMAPosAHI(void);

#ifndef NO_PAULA
/* from Paula */
extern void SNDDMA_ShutdownPaula(void);
extern qboolean SNDDMA_InitPaula(void);
extern int SNDDMA_GetDMAPosPaula(void);
extern void SNDDMA_PaulaFilterOn(void);
extern void SNDDMA_PaulaFilterOff(void);
#endif


void SNDDMA_Shutdown(void)
{
	#ifndef NO_PAULA

	if (!use_ahi)
		SNDDMA_ShutdownPaula();
	else
	#endif
		SNDDMA_ShutdownAHI();

	snd_inuse = 0;
}


/* 2026-11-06: the Sound page's settings (sound.h). The command line wins
 * for its run and is not written back: -ahi / -paula, -audspeed,
 * -swapchannels. */
cvar_t	snd_device = {"snd_device", "paula", true};
cvar_t	snd_ahimode = {"snd_ahimode", "0", true};
cvar_t	snd_rate = {"snd_rate", "11025", true};
cvar_t	snd_swap = {"snd_swap", "0", true};

void SND_RegisterCvars (void)
{
	Cvar_RegisterVariable (&snd_device);
	Cvar_RegisterVariable (&snd_ahimode);
	Cvar_RegisterVariable (&snd_rate);
	Cvar_RegisterVariable (&snd_swap);
}

int SND_DeviceInUse (void)
{
	return snd_inuse;
}

extern int desired_speed;

qboolean SNDDMA_Init(void)
{
	int	rate;
	qboolean ok;

	snd_inuse = 0;

	/* the mixing rate: -audspeed (read by the drivers themselves) or
	 * snd_rate; Paula's DMA stops near 28 kHz on a 15 kHz display */
	rate = (int)snd_rate.value;
	if (rate < 4000)
		rate = 11025;
	if (rate > 48000)
		rate = 48000;

	#ifndef NO_PAULA

	if (snd_use_cmdline && COM_CheckParm("-ahi"))
		use_ahi = 1;
	else if (snd_use_cmdline && COM_CheckParm("-paula"))
		use_ahi = 0;
	else if (!Q_strcasecmp(snd_device.string, "off"))
	{
		snd_use_cmdline = 0;
		return false;
	}
	else
		use_ahi = !Q_strcasecmp(snd_device.string, "ahi");

	if (!use_ahi)
	{
		desired_speed = (rate > 28000) ? 28000 : rate;

		ok = SNDDMA_InitPaula();
		snd_use_cmdline = 0;

		if (!ok)
		{
			SNDDMA_ShutdownPaula();	// what a failed start left
			return false;
		}

		snd_inuse = 1;
		return true;
	}

	#endif

	use_ahi = 1;
	desired_speed = rate;

	ok = SNDDMA_InitAHI();
	snd_use_cmdline = 0;

	if (!ok)
	{
		SNDDMA_ShutdownAHI();		// what a failed start left
		return false;
	}

	snd_inuse = 2;
	return true;
}


int SNDDMA_GetDMAPos(void)
{
	#ifndef NO_PAULA

	if (!use_ahi)
		return SNDDMA_GetDMAPosPaula();

	#endif

	return SNDDMA_GetDMAPosAHI();
}


void SNDDMA_Submit(void)
{
}


void SNDDMA_EnterWater(void)
{
	#ifndef NO_PAULA
	if (!use_ahi)
		SNDDMA_PaulaFilterOn();
	#endif
}


void SNDDMA_LeaveWater(void)
{
	#ifndef NO_PAULA
	if (!use_ahi)
		SNDDMA_PaulaFilterOff();
	#endif
}
