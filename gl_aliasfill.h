/*
gl_aliasfill.h -- one chunk of GL_DrawAliasBatch's positions, texture
coordinates and colours into the model arrays (gl_rmain.c only).

Included twice, with AF_T the arrays' record and AF_NAME the function:
qglarray_t (28 bytes, with the texture coordinate w the other MiniGLs read)
and qglarray24_t (24 bytes, two texture coordinates: RadeonGL). 2026-11-07:
the loops moved here from GL_DrawAliasBatch unchanged.
*/

static void AF_NAME (AF_T *lva0, int numverts, short *bv, float *st,
	trivertx_t *verts1, trivertx_t *verts2, float blend, qboolean lerp, ULONG a)
{
	AF_T	*lva;
	int	i;
	float	l;

	(void) l;
	lva = lva0;

	if (lerp)
	{
		for (i=0 ; i<numverts ; i++, lva++, st += 2)
		{
			trivertx_t	*v1 = verts1 + bv[i];
			trivertx_t	*v2 = verts2 + bv[i];
			vec3_t		d;

			lva->s = st[0];
			lva->t = st[1];

			VectorSubtract(v2->v, v1->v, d);

			lva->x = v1->v[0] + (blend * d[0]);
			lva->y = v1->v[1] + (blend * d[1]);
			lva->z = v1->v[2] + (blend * d[2]);
		}
	}

	else
	{
		for (i=0 ; i<numverts ; i++, lva++, st += 2)
		{
			trivertx_t	*v1 = verts1 + bv[i];

			lva->s = st[0];
			lva->t = st[1];

			lva->x = (float)v1->v[0];
			lva->y = (float)v1->v[1];
			lva->z = (float)v1->v[2];
		}
	}

	// lit from pose1's normals, as both frame functions do
	if (gl_smoothmodels.value)
	{
		lva = lva0;

		#ifdef LITFILES
		if(eyecandy)
		{
			for (i=0 ; i<numverts ; i++, lva++)
			{
				ULONG	r, g, b;

				l = shadedots[verts1[bv[i]].lightnormalindex];

				r = (ULONG)(lightcolor[0]*l);
				g = (ULONG)(lightcolor[1]*l);
				b = (ULONG)(lightcolor[2]*l);

				if(r > 255) r = 255;
				if(g > 255) g = 255;
				if(b > 255) b = 255;

				lva->c = a | (r<<16) | (g<<8) | b;
			}
		}

		else
		#endif
		{
			/* 2026-11-06: each normal's colour (162 of them) worked
			 * out the first time this draw meets it, then reused:
			 * the same value as per vertex, one float multiply and
			 * conversion a normal instead of a vertex */
			for (i=0 ; i<numverts ; i++, lva++)
			{
				int	n = verts1[bv[i]].lightnormalindex;

				if (normstamp[n] != normframe)
				{
					ULONG	lit = (ULONG)(shadedots[n] * shadelight);

					if(lit > 255)
						lit = 255; //clamp

					normcolor[n] = a | (lit<<16) | (lit<<8) | lit;
					normstamp[n] = normframe;
				}

				lva->c = normcolor[n];
			}
		}
	}
}
