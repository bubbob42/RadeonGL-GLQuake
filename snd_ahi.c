/*
**  snd_ahi.c
**
**  AHI sound driver
**
**  Written by Jarmo Laakkonen <jami.laakkonen@kolumbus.fi>
**  MorphOS and OS4-support by Frank Wille <frank@phoenix.owl.de>
**
*/

#pragma pack(push,2)

#include <exec/exec.h>
#include <devices/ahi.h>
#include <devices/timer.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <proto/ahi.h>

#ifndef __amigaos4__
#include <clib/alib_protos.h>
#endif

#pragma pack(pop)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

//#include "SDI_compiler.h"
#include "quakedef.h"

#pragma pack(push,2)
struct ChannelInfo
{
	struct AHIEffChannelInfo cinfo;
	ULONG x[1];
};
#pragma pack(pop)

struct Library *AHIBase = NULL;

#ifdef __amigaos4__
struct AHIIFace *IAHI;
#endif

static struct MsgPort *AHImp = NULL;
static struct AHIRequest *AHIio = NULL;
static BYTE AHIDevice = -1;
static struct AHIAudioCtrl *actrl = NULL;
static ULONG rc = 1;
static struct ChannelInfo info;

static int speed;
static UBYTE *dmabuf = NULL;
static int buflen;

extern int desired_speed;

#define MINBUFFERSIZE 16384

#if defined(__PPC__) && !defined(WOS) && !defined(__MORPHOS__) && !defined(__amigaos4__)
#define CNAME(x) _##x
#else
#define CNAME(x) x
#endif

int CNAME(ahi_pos) = 0;

#if 0 //#ifdef __MORPHOS__

extern ULONG EffFunc(void);

static struct EmulLibEntry GATE_EffFunc = {
  TRAP_LIB,0,(void(*)(void))EffFunc
};

struct Hook EffHook = {
  0, 0,
  (HOOKFUNC)&GATE_EffFunc,
  0, 0
};

#endif // /* AmigaOS/68k, OS4, PowerUp or WarpOS */

extern ULONG CNAME(EffFunc)();

struct Hook EffHook =
{
	0, 0,
	(HOOKFUNC)CNAME(EffFunc),
	0, 0
};

//#endif


void SNDDMA_ShutdownAHI(void)
{
	if (actrl)
	{
		info.cinfo.ahie_Effect = AHIET_CHANNELINFO | AHIET_CANCEL;
		AHI_SetEffect(&info, actrl);
		AHI_ControlAudio(actrl, AHIC_Play, FALSE, TAG_END);
	}

	if (rc == 0)
	{
		AHI_UnloadSound(0, actrl);
		rc = 1;
	}

	if (shm)
	{
		Sys_Free(shm);
		shm = NULL;
	}

	if (dmabuf)
	{
		Sys_Free(dmabuf);
		dmabuf = NULL;
	}

	if (actrl)
	{
		AHI_FreeAudio(actrl);
		actrl = NULL;
	}

	if (AHIDevice == 0)
	{
		#ifdef __amigaos4__
		DropInterface((struct Interface *)IAHI);
		#endif

		CloseDevice((struct IORequest *)AHIio);
		AHIDevice = -1;
		AHIBase = NULL;		// 2026-11-06: SND_AHIListModes tests it
	}

	if (AHIio)
	{
		DeleteIORequest((struct IORequest *)AHIio);
		AHIio = NULL;
	}

	if (AHImp)
	{
		DeleteMsgPort(AHImp);
		AHImp = NULL;
	}
}


qboolean SNDDMA_InitAHI(void)
{
	struct AHISampleInfo sample;
	int	i;
	ULONG	mixfreq, playsamples;
	UBYTE	name[256];
	ULONG	mode;
	ULONG	type;
	int	ahichannels;
	int	ahibits;

	info.cinfo.ahieci_Channels = 1;
	info.cinfo.ahieci_Func = &EffHook;
	info.cinfo.ahie_Effect = AHIET_CHANNELINFO;

	if ( snd_use_cmdline && (i = COM_CheckParm("-audspeed")) )
		speed = Q_atoi(com_argv[i+1]);

	else
		speed = desired_speed;

	#if 0
	if ( (i = COM_CheckParm("-ahimono")) )
		ahichannels = 1;
	else
	#endif

		ahichannels = 2;

	#if 0
	if ( (i = COM_CheckParm("-ahibits")) )
		ahibits = Q_atoi(com_argv[i+1]);
	else
	#endif

		ahibits = 16;

	#if 0
	if (ahibits != 8 && ahibits != 16)
		ahibits = 16;

	if ( ahichannels == 1 )
	{
		if (ahibits == 16) type = AHIST_M16S;
		else {type = AHIST_M8S;} 
	}

	else
	{
		if (ahibits == 16) type = AHIST_S16S;
		else {type = AHIST_S8S;}
	}

	#endif 

	type = AHIST_S16S;

	if ( (AHImp = CreateMsgPort()) == NULL )
	{
		Con_Printf("Can't create AHI message port\n");
		return false;
	}

	if ( (AHIio = (struct AHIRequest *)CreateIORequest(AHImp, sizeof(struct AHIRequest)) ) == NULL )
	{
		Con_Printf("Can't create AHI io request\n");
		return false;
	}

	AHIio->ahir_Version = 4;

	if ( (AHIDevice = OpenDevice("ahi.device", AHI_NO_UNIT, (struct IORequest *)AHIio, 0)) != 0 )
	{
		Con_Printf("Can't open ahi.device version 4\n");
		return false;
	}

	AHIBase = (struct Library *)AHIio->ahir_Std.io_Device;

	#ifdef __amigaos4__
	IAHI = (struct AHIIFace *)GetInterface((struct Library *)AHIBase,"main",1,0);
	#endif

	/* 2026-11-06: the Sound page's AHI mode (snd_ahimode), else AHI's
	 * default; a saved mode this machine does not have falls back */
	mode = (ULONG)strtoul(snd_ahimode.string, NULL, 0);

	if (mode == 0 || mode == AHI_INVALID_ID)
		mode = AHI_DEFAULT_ID;

	actrl = AHI_AllocAudio(AHIA_AudioID, mode,
		AHIA_MixFreq, speed,
		AHIA_Channels, 1,
		AHIA_Sounds, 1,
		TAG_END);

	if (actrl == NULL && mode != AHI_DEFAULT_ID)
	{
		Con_Printf("AHI mode $%08lx not available, AHI's default\n", mode);
		actrl = AHI_AllocAudio(AHIA_AudioID, AHI_DEFAULT_ID,
			AHIA_MixFreq, speed,
			AHIA_Channels, 1,
			AHIA_Sounds, 1,
			TAG_END);
	}

	if (actrl == NULL)
	{
		Con_Printf("Can't allocate audio\n");
		return false;
	}

	AHI_GetAudioAttrs(AHI_INVALID_ID, actrl,
			AHIDB_MaxPlaySamples,	(ULONG)&playsamples,
			AHIDB_BufferLen,	256,
			AHIDB_Name,		(ULONG)&name,
			AHIDB_AudioID,		(ULONG)&mode,
			TAG_END);

	AHI_ControlAudio(actrl, AHIC_MixFreq_Query, (ULONG)&mixfreq, TAG_END);

	#if 0

	buflen = playsamples * speed / mixfreq;

	if (buflen < MINBUFFERSIZE)
		buflen = MINBUFFERSIZE;

	#else

	buflen = 0x8000;

	#endif

	if ( (dmabuf = Sys_Alloc(buflen, MEMF_ANY | MEMF_PUBLIC | MEMF_CLEAR)) == NULL )
	{
		Con_Printf("Can't allocate AHI dma buffer\n");
		return false;
	}

	if ( (shm = Sys_Alloc(sizeof(dma_t), MEMF_ANY | MEMF_CLEAR)) == NULL )
	{
		Con_Printf("Can't allocate shm\n");
		return false;
	}

	shm->buffer = (unsigned char *)dmabuf;
	shm->channels = ahichannels;
	shm->speed = speed;
	shm->samplebits = ahibits;
	shm->samples = buflen / (shm->samplebits / 8);
	shm->submission_chunk = 1;
	shm->samplepos = 0; // added Cowcat 2019

	sample.ahisi_Type = type;
	sample.ahisi_Address = (APTR)dmabuf;
	sample.ahisi_Length = buflen / AHI_SampleFrameSize(type);

	if ( (rc = AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &sample, actrl)) != 0 )
	{
		Con_Printf("Can't load sound\n");
		return false;
	}
 
	if ( AHI_ControlAudio(actrl, AHIC_Play, TRUE, TAG_END) != 0 )
	{
		Con_Printf("Can't start playback\n");
		return false;
	}

	Con_Printf("AHI audio initialized\n");
	Con_Printf("AHI mode: %s (%08x)\n", name, mode);
	Con_Printf("Output: %ibit %s, %i Hz (AHI mixes at %lu Hz)\n", ahibits, ahichannels == 2 ? "stereo" : "mono", speed, (unsigned long)mixfreq);

	AHI_Play(actrl,
		AHIP_BeginChannel,	0,
		AHIP_Freq,		speed,
		AHIP_Vol,		0x10000,
		AHIP_Pan,		0x8000,
		AHIP_Sound,		0,
		AHIP_EndChannel,	NULL,
		TAG_END);

	AHI_SetEffect(&info, actrl);

	return true;
}


int SNDDMA_GetDMAPosAHI(void)
{
	return (shm->samplepos = CNAME(ahi_pos) * shm->channels);
}




/*
==================
SND_AHIListModes

2026-11-06, the Sound page: AHI's real-time audio modes (the file-saving
ones are left out), id and name, at most max. ahi.device is opened here
for the list when the sound does not run on it; 0 when it is not there.
==================
*/
int SND_AHIListModes (unsigned long *ids, char (*names)[SND_AHINAME], int max)
{
	struct MsgPort		*mp;
	struct AHIRequest	*io;
	ULONG			id, realtime;
	int			n;
	BOOL			ours;

	n = 0;
	mp = NULL;
	io = NULL;
	ours = (AHIBase == NULL);

	if (ours)
	{
		if ( (mp = CreateMsgPort()) == NULL )
			return 0;

		if ( (io = (struct AHIRequest *)CreateIORequest(mp, sizeof(struct AHIRequest))) == NULL )
		{
			DeleteMsgPort(mp);
			return 0;
		}

		io->ahir_Version = 4;

		if (OpenDevice("ahi.device", AHI_NO_UNIT, (struct IORequest *)io, 0) != 0)
		{
			DeleteIORequest((struct IORequest *)io);
			DeleteMsgPort(mp);
			return 0;
		}

		AHIBase = (struct Library *)io->ahir_Std.io_Device;
	}

	for (id = AHI_NextAudioID(AHI_INVALID_ID); id != AHI_INVALID_ID && n < max; id = AHI_NextAudioID(id))
	{
		realtime = TRUE;
		names[n][0] = 0;

		AHI_GetAudioAttrs(id, NULL,
			AHIDB_Realtime,		(ULONG)&realtime,
			AHIDB_BufferLen,	SND_AHINAME,
			AHIDB_Name,		(ULONG)names[n],
			TAG_END);

		if (!realtime)
			continue;

		names[n][SND_AHINAME - 1] = 0;
		ids[n] = id;
		n++;
	}

	if (ours)
	{
		CloseDevice((struct IORequest *)io);
		DeleteIORequest((struct IORequest *)io);
		DeleteMsgPort(mp);
		AHIBase = NULL;
	}

	return n;
}
