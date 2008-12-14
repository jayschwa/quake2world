/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "client.h"

typedef struct {
	int left;
	int right;
} portable_samplepair_t;

#define PAINTBUFFER_SIZE 2048
static portable_samplepair_t s_paintbuffer[PAINTBUFFER_SIZE];

#define MAX_RAW_SAMPLES	8192
static portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];

static int snd_scaletable[32][256];
static int *snd_p, snd_linear_count, snd_vol;

static short *snd_out;


/*
S_WriteLinearBlastStereo16
*/
static void S_WriteLinearBlastStereo16(void){
	int i;
	int val;

	for(i = 0; i < snd_linear_count; i += 2){
		val = snd_p[i] >> 8;
		if(val > 0x7ddd)
			snd_out[i] = 0x7ddd;
		else if(val < (short)0x8000)
			snd_out[i] = (short)0x8000;
		else
			snd_out[i] = val;

		val = snd_p[i + 1] >> 8;
		if(val > 0x7ddd)
			snd_out[i + 1] = 0x7ddd;
		else if(val < (short)0x8000)
			snd_out[i + 1] = (short)0x8000;
		else
			snd_out[i + 1] = val;
	}
}


/*
S_TransferStereo16
*/
static void S_TransferStereo16(unsigned long *pbuf, int endtime){
	int paintedtime;

	snd_p = (int *)s_paintbuffer;
	paintedtime = s_paintedtime;

	while(paintedtime < endtime){
		// handle recirculating buffer issues
		const int lpos = paintedtime &((dma.samples >> 1) - 1);

		snd_out = (short *)pbuf + (lpos << 1);

		snd_linear_count = (dma.samples >> 1) - lpos;
		if(paintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - paintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		S_WriteLinearBlastStereo16();

		snd_p += snd_linear_count;
		paintedtime +=(snd_linear_count >> 1);
	}
}


/*
S_TransferPaintBuffer
*/
static void S_TransferPaintBuffer(int endtime){
	int out_idx, out_mask;
	int i, count;
	int *p;
	int step, val;
	unsigned long *pbuf;

	pbuf = (unsigned long *)dma.buffer;

	if(s_testsound->value){  // write a fixed sine wave
		count = (endtime - s_paintedtime);
		for(i = 0; i < count; i++)
			s_paintbuffer[i].left = s_paintbuffer[i].right =
				sin((s_paintedtime + i) * 0.1) * 20000 * 256;
	}

	if(dma.bits == 16 && dma.channels == 2){  // optimized case
		S_TransferStereo16(pbuf, endtime);
		return;
	}

	p = (int *)s_paintbuffer;  // general case
	count = (endtime - s_paintedtime) * dma.channels;
	out_mask = dma.samples - 1;
	out_idx = s_paintedtime * dma.channels & out_mask;
	step = 3 - dma.channels;

	if(dma.bits == 16){
		short *out = (short *)pbuf;
		while(count--){
			val = *p >> 8;
			p += step;
			if(val > 0x7fff)
				val = 0x7fff;
			else if(val < (short)0x8000)
				val = (short)0x8000;
			out[out_idx] = val;
			out_idx = (out_idx + 1) & out_mask;
		}
	} else if(dma.bits == 8){
		unsigned char *out = (unsigned char *)pbuf;
		while(count--){
			val = *p >> 8;
			p += step;
			if(val > 0x7fff)
				val = 0x7fff;
			else if(val < (short)0x8000)
				val = (short)0x8000;
			out[out_idx] = (val >> 8) + 128;
			out_idx = (out_idx + 1) & out_mask;
		}
	}
}


void S_PaintChannelFrom8(channel_t *ch, sfxcache_t *sc, int endtime, int offset);
void S_PaintChannelFrom16(channel_t *ch, sfxcache_t *sc, int endtime, int offset);

/*
S_PaintChannels
*/
void S_PaintChannels(int endtime){
	int i;
	int end;
	channel_t *ch;
	sfxcache_t *sc;
	int ltime, count;
	playsound_t *ps;

	while(s_paintedtime < endtime){
		// if s_paintbuffer is smaller than DMA buffer
		end = endtime;
		if(endtime - s_paintedtime > PAINTBUFFER_SIZE)
			end = s_paintedtime + PAINTBUFFER_SIZE;

		i = 0;
		// start any playsounds
		while(true){
			ps = s_pendingplays.next;
			if(ps == &s_pendingplays)
				break;  // no more pending sounds

			if(ps->begin <= s_paintedtime){
				S_IssuePlaysound(ps);
				continue;
			}

			if(ps->begin < end)
				end = ps->begin;  // stop here
			break;
		}

		// clear the paint buffer
		if(s_rawend < s_paintedtime){
			memset(s_paintbuffer, 0, (end - s_paintedtime) * sizeof(portable_samplepair_t));
		} else {  // copy from the streaming sound source
			int s;
			int stop;

			stop = (end < s_rawend) ? end : s_rawend;

			for(i = s_paintedtime; i < stop; i++){
				s = i & (MAX_RAW_SAMPLES - 1);
				s_paintbuffer[i - s_paintedtime] = s_rawsamples[s];
			}

			for(; i < end; i++){
				s_paintbuffer[i - s_paintedtime].left =
					s_paintbuffer[i - s_paintedtime].right = 0;
			}
		}

		// paint in the channels.
		ch = channels;
		for(i = 0; i < MAX_CHANNELS; i++, ch++){
			ltime = s_paintedtime;

			while(ltime < end){
				if(!ch->sfx || (!ch->leftvol && !ch->rightvol))
					break;

				// max painting is to the end of the buffer
				count = end - ltime;

				// might be stopped by running out of data
				if(ch->end - ltime < count)
					count = ch->end - ltime;

				sc = S_LoadSfx(ch->sfx);
				if(!sc)
					break;

				if(count > 0 && ch->sfx){
					if(sc->width == 1)
						S_PaintChannelFrom8(ch, sc, count, ltime - s_paintedtime);
					else
						S_PaintChannelFrom16(ch, sc, count, ltime - s_paintedtime);

					ltime += count;
				}

				// if at end of loop, restart
				if(ltime >= ch->end){
					if(ch->autosound){  // autolooping sounds always go back to start
						ch->pos = 0;
						ch->end = ltime + sc->length;
					} else if(sc->loopstart >= 0){
						ch->pos = sc->loopstart;
						ch->end = ltime + sc->length - ch->pos;
					} else {  // channel just stopped
						ch->sfx = NULL;
					}
				}
			}
		}

		// transfer out according to DMA format
		S_TransferPaintBuffer(end);
		s_paintedtime = end;
	}
}


/*
S_InitScaletable
*/
void S_InitScaletable(void){
	int i, j;
	int scale;

	if(s_volume->value > 1)  //cap volume
		s_volume->value = 1;

	for(i = 0; i < 32; i++){
		scale = i * 8 * 256 * s_volume->value;
		for(j = 0; j < 256; j++)
			snd_scaletable[i][j] = (signed char)j * scale;
	}

	snd_vol = s_volume->value * 256;
	s_volume->modified = false;
}


/*
S_PaintChannelFrom8
*/
void S_PaintChannelFrom8(channel_t *ch, sfxcache_t *sc, int count, int offset){
	int data;
	int *lscale, *rscale;
	byte *sfx;
	int i;
	portable_samplepair_t *samp;

	if(ch->leftvol > 255)
		ch->leftvol = 255;
	if(ch->rightvol > 255)
		ch->rightvol = 255;

	lscale = snd_scaletable[ ch->leftvol >> 3];
	rscale = snd_scaletable[ ch->rightvol >> 3];
	sfx = (byte *)sc->data + ch->pos;

	samp = &s_paintbuffer[offset];

	for(i = 0; i < count; i++, samp++){
		data = sfx[i];
		samp->left += lscale[data];
		samp->right += rscale[data];
	}

	ch->pos += count;
}


/*
S_PaintChannelFrom16
*/
void S_PaintChannelFrom16(channel_t *ch, sfxcache_t *sc, int count, int offset){
	int data;
	int left, right;
	int leftvol, rightvol;
	signed short *sfx;
	int i;
	portable_samplepair_t *samp;

	leftvol = ch->leftvol * snd_vol;
	rightvol = ch->rightvol * snd_vol;
	sfx = (signed short *)sc->data + ch->pos;

	samp = &s_paintbuffer[offset];
	for(i = 0; i < count; i++, samp++){
		data = sfx[i];
		left = (data * leftvol) >> 8;
		right = (data * rightvol) >> 8;
		samp->left += left;
		samp->right += right;
	}

	ch->pos += count;
}