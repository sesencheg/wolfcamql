#include "tr_mme.h"

// qglReadPixels
#define GLE(ret, name, ...) extern name##proc * qgl##name;
QGL_1_1_PROCS;
//QGL_1_1_FIXED_FUNCTION_PROCS;
//QGL_DESKTOP_1_1_PROCS;
//QGL_DESKTOP_1_1_FIXED_FUNCTION_PROCS;
//QGL_3_0_PROCS;
#undef GLE

#if 0
extern GLuint pboIds[4];
void R_MME_GetShot( void* output, mmeShotType_t type ) {
	GLenum format;
	switch (type) {
	case mmeShotTypeBGR:
		format = GL_BGR_EXT;
		break;
	default:
		format = GL_RGB;
		break;
	}
	if (!mme_pbo->integer || r_stereoSeparation->value != 0) {
		qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, format, GL_UNSIGNED_BYTE, output ); 
	} else {
		static int index = 0;
		qglBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboIds[index]);
		index = (index + 1) & 0x3;
		qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, format, GL_UNSIGNED_BYTE, 0 );

		// map the PBO to process its data by CPU
		qglBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboIds[index]);
		{GLubyte* ptr = (GLubyte*)qglMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
		if (ptr) {
			memcpy( output, ptr, glConfig.vidHeight * glConfig.vidWidth * 3 );
			qglUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
		}}
		// back to conventional pixel operation
		qglBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
}

void R_MME_GetStencil( void *output ) {
	qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, output ); 
}
#endif

// R_MME_GetDepth() in inc_tr_init.c since it needs to access backEndState_t

#if 0
void R_MME_SaveShot( mmeShot_t *shot, int width, int height, float fps, byte *inBuf, qboolean audio, int aSize, byte *aBuf ) {
	mmeShotFormat_t format;
	char *extension;
	char *outBuf;
	int outSize;
	char fileName[MAX_OSPATH];

	format = shot->format;
	switch (format) {
	case mmeShotFormatJPG:
		extension = "jpg";
		break;
	case mmeShotFormatTGA:
		/* Seems hardly any program can handle grayscale tga, switching to png */
		if (shot->type == mmeShotTypeGray) {
			format = mmeShotFormatPNG;
			extension = "png";
		} else {
			extension = "tga";
		}
		break;
	case mmeShotFormatPNG:
		extension = "png";
		break;
    case mmeShotFormatPIPE:
		if (!shot->avi.f) {
			shot->avi.pipe = qtrue;
		}
	case mmeShotFormatAVI:
		mmeAviShot( &shot->avi, shot->name, shot->type, width, height, fps, inBuf, audio );
		if (audio)
			mmeAviSound( &shot->avi, shot->name, shot->type, width, height, fps, aBuf, aSize );
		return;
	}

	if (shot->counter < 0) {
		int counter = 0;
		while ( counter < 1000000000) {
			Com_sprintf( fileName, sizeof(fileName), "%s.%010d.%s", shot->name, counter, extension);
			if (!FS_FileExists( fileName ))
				break;
			if ( mme_saveOverwrite->integer ) 
				FS_FileErase( fileName );
			counter++;
		}
		if ( mme_saveOverwrite->integer ) {
			shot->counter = 0;
		} else {
			shot->counter = counter;
		}
	} 

	Com_sprintf( fileName, sizeof(fileName), "%s.%010d.%s", shot->name, shot->counter, extension );
	shot->counter++;

	outSize = width * height * 4 + 2048;
	outBuf = (char *)ri.Hunk_AllocateTempMemory( outSize );
	switch ( format ) {
	case mmeShotFormatJPG:
		outSize = SaveJPG( mme_jpegQuality->integer, width, height, shot->type, inBuf, (byte *)outBuf, outSize );
		break;
	case mmeShotFormatTGA:
		outSize = SaveTGA( mme_tgaCompression->integer, width, height, shot->type, inBuf, (byte *)outBuf, outSize );
		break;
	case mmeShotFormatPNG:
		outSize = SavePNG( mme_pngCompression->integer, width, height, shot->type, inBuf, (byte *)outBuf, outSize );
		break;
	default:
		outSize = 0;
	}
	if (outSize)
		ri.FS_WriteFile( fileName, outBuf, outSize );
	ri.Hunk_FreeTempMemory( outBuf );
}
#endif

void blurCreate(mmeBlurControl_t* control, const char* type, int frames) {
    float* blurFloat = control->Float;
    float blurMax, strength;
    float blurHalf = 0.5f * (frames - 1);
    float bestStrength;
    float floatTotal;
    int passes, bestTotal;
    int i;

    if (blurHalf <= 0)
        return;

    // Вычисление весов размытия (Gaussian, Triangle или Uniform)
    if (!Q_stricmp(type, "gaussian") || mme_blurStrength->value >= 1.0f) {
        float strengthVal = (mme_blurStrength->value >= 1.0f) ? (mme_blurStrength->value / 10.0f) : 1.0f;
        for (i = 0; i < frames; i++) {
            double xVal = ((i - blurHalf) / blurHalf) * 3;
            double expVal = exp(-(xVal * xVal) * strengthVal / 2);
            double sqrtVal = 1.0f / sqrt(2 * M_PI);
            blurFloat[i] = sqrtVal * expVal;
        }
    } else if (!Q_stricmp(type, "triangle")) {
        for (i = 0; i < frames; i++) {
            if (i <= blurHalf)
                blurFloat[i] = 1 + i;
            else
                blurFloat[i] = 1 + (frames - 1 - i);
        }
    } else { // Uniform (прямоугольное размытие)
        for (i = 0; i < frames; i++) {
            blurFloat[i] = 1;
        }
    }

    // Нормализация весов (сумма = 1)
    floatTotal = 0;
    blurMax = 0;
    for (i = 0; i < frames; i++) {
        if (blurFloat[i] > blurMax)
            blurMax = blurFloat[i];
        floatTotal += blurFloat[i];
    }

    floatTotal = 1 / floatTotal;
    for (i = 0; i < frames; i++)
        blurFloat[i] *= floatTotal;

    // Установка параметров размытия
    control->totalIndex = frames;
    control->overlapFrames = 0;
    control->overlapIndex = 0;
}

static void MME_AccumClearMMX(void* w, const void* r, short mul, int count) {
    const unsigned char* reader = (const unsigned char*)r;
    short* writer = (short*)w;
    int i, j;
    
    for (i = 0; i < count; i++) {
        for (j = 0; j < 8; j++) {
            writer[j] = ((short)reader[j]) * mul;
        }
        reader += 8;
        writer += 8;
    }
}

static void MME_AccumAddMMX(void *w, const void* r, short mul, int count) {
    const unsigned char* reader = (const unsigned char*)r;
    short* writer = (short*)w;
    int i, j;
    
    for (i = 0; i < count; i++) {
        for (j = 0; j < 8; j++) {
            // Распаковка байта в short и умножение на множитель
            short value = (short)reader[j] * mul;
            // Добавление к существующему значению
            writer[j] += value;
        }
        reader += 8;  // Перемещаемся к следующему блоку из 8 байт
        writer += 8;  // Перемещаемся к следующему блоку из 8 short
    }
}

static void MME_AccumShiftMMX(const void *r, void *w, int count) {
    const unsigned short* reader = (const unsigned short*)r;
    unsigned char* writer = (unsigned char*)w;
    int total_iterations = count * 4; // Process 4 elements per original MMX register
    
    for (int i = 0; i < total_iterations; i += 4) {
        // Right shift each 16-bit value by 8 (equivalent to taking the high byte)
        unsigned char b0 = (reader[i] >> 8) & 0xFF;
        unsigned char b1 = (reader[i+1] >> 8) & 0xFF;
        unsigned char b2 = (reader[i+2] >> 8) & 0xFF;
        unsigned char b3 = (reader[i+3] >> 8) & 0xFF;
        
        // Pack the bytes into the output (same as _mm_packs_pu16)
        writer[0] = b0;
        writer[1] = b1;
        writer[2] = b2;
        writer[3] = b3;
        
        reader += 4;
        writer += 4;
    }
}

void R_MME_BlurAccumAdd( mmeBlurBlock_t *block, const __m64 *add ) {
	mmeBlurControl_t* control = block->control;
	int index = control->totalIndex;
	if ( mme_cpuSSE2->integer  &&  ri.sse2_supported ) {
		if ( index == 0) {
			MME_AccumClearSSE( block->accum, add, control->SSE[ index ], block->count );
		} else {
			MME_AccumAddSSE( block->accum, add, control->SSE[ index ], block->count );
		}
	} else {
		if ( index == 0) {
			MME_AccumClearMMX( block->accum, add, control->MMX[ index ], block->count );
		} else {
			MME_AccumAddMMX( block->accum, add, control->MMX[ index ], block->count );
		}
	}
}

void R_MME_BlurOverlapAdd( mmeBlurBlock_t *block, int index ) {
	mmeBlurControl_t* control = block->control;
	index = ( index + control->overlapIndex ) % control->overlapFrames;
	R_MME_BlurAccumAdd( block, block->overlap + block->count * index );
}

void R_MME_BlurAccumShift( mmeBlurBlock_t *block ) {
	if ( mme_cpuSSE2->integer   &&  ri.sse2_supported ) {
		MME_AccumShiftSSE( block->accum, block->accum, block->count );
	} else {
		MME_AccumShiftMMX( block->accum, block->accum, block->count );
	}
}

//Replace rad with _rad gogo includes
/* Slightly stolen from blender */
static void RE_jitterate1(float *jit1, float *jit2, int num, float _rad1) {
	int i , j , k;
	float vecx, vecy, dvecx, dvecy, x, y, len;

	for (i = 2*num-2; i>=0 ; i-=2) {
		dvecx = dvecy = 0.0;
		x = jit1[i];
		y = jit1[i+1];
		for (j = 2*num-2; j>=0 ; j-=2) {
			if (i != j){
				vecx = jit1[j] - x - 1.0;
				vecy = jit1[j+1] - y - 1.0;
				for (k = 3; k>0 ; k--){
					if( fabs(vecx)<_rad1 && fabs(vecy)<_rad1) {
						len=  sqrt(vecx*vecx + vecy*vecy);
						if(len>0 && len<_rad1) {
							len= len/_rad1;
							dvecx += vecx/len;
							dvecy += vecy/len;
						}
					}
					vecx += 1.0;

					if( fabs(vecx)<_rad1 && fabs(vecy)<_rad1) {
						len=  sqrt(vecx*vecx + vecy*vecy);
						if(len>0 && len<_rad1) {
							len= len/_rad1;
							dvecx += vecx/len;
							dvecy += vecy/len;
						}
					}
					vecx += 1.0;

					if( fabs(vecx)<_rad1 && fabs(vecy)<_rad1) {
						len=  sqrt(vecx*vecx + vecy*vecy);
						if(len>0 && len<_rad1) {
							len= len/_rad1;
							dvecx += vecx/len;
							dvecy += vecy/len;
						}
					}
					vecx -= 2.0;
					vecy += 1.0;
				}
			}
		}

		x -= dvecx/18.0 ;
		y -= dvecy/18.0;
		x -= floor(x) ;
		y -= floor(y);
		jit2[i] = x;
		jit2[i+1] = y;
	}
	memcpy(jit1,jit2,2 * num * sizeof(float));
}

static void RE_jitterate2(float *jit1, float *jit2, int num, float _rad2) {
	int i, j;
	float vecx, vecy, dvecx, dvecy, x, y;

	for (i=2*num -2; i>= 0 ; i-=2){
		dvecx = dvecy = 0.0;
		x = jit1[i];
		y = jit1[i+1];
		for (j =2*num -2; j>= 0 ; j-=2){
			if (i != j){
				vecx = jit1[j] - x - 1.0;
				vecy = jit1[j+1] - y - 1.0;

				if( fabs(vecx)<_rad2) dvecx+= vecx*_rad2;
				vecx += 1.0;
				if( fabs(vecx)<_rad2) dvecx+= vecx*_rad2;
				vecx += 1.0;
				if( fabs(vecx)<_rad2) dvecx+= vecx*_rad2;

				if( fabs(vecy)<_rad2) dvecy+= vecy*_rad2;
				vecy += 1.0;
				if( fabs(vecy)<_rad2) dvecy+= vecy*_rad2;
				vecy += 1.0;
				if( fabs(vecy)<_rad2) dvecy+= vecy*_rad2;

			}
		}

		x -= dvecx/2 ;
		y -= dvecy/2;
		x -= floor(x) ;
		y -= floor(y);
		jit2[i] = x;
		jit2[i+1] = y;
	}
	memcpy(jit1,jit2,2 * num * sizeof(float));
}

void R_MME_JitterTable(float *jitarr, int num) {
	float jit2[12 + 256*2];
	float x, _rad1, _rad2, _rad3;
	int i;

	if(num==0)
		return;
	if(num>256)
		return;

	_rad1=  1.0/sqrt((float)num);
	_rad2= 1.0/((float)num);
	_rad3= sqrt((float)num)/((float)num);

	x= 0;
	for(i=0; i<2*num; i+=2) {
		jitarr[i]= x+ _rad1*(0.5-random());
		jitarr[i+1]= ((float)i/2)/num +_rad1*(0.5-random());
		x+= _rad3;
		x -= floor(x);
	}

	for (i=0 ; i<24 ; i++) {
		RE_jitterate1(jitarr, jit2, num, _rad1);
		RE_jitterate1(jitarr, jit2, num, _rad1);
		RE_jitterate2(jitarr, jit2, num, _rad2);
	}
	
	/* finally, move jittertab to be centered around (0,0) */
	for(i=0; i<2*num; i+=2) {
		jitarr[i] -= 0.5;
		jitarr[i+1] -= 0.5;
	}
	
}

#define FOCUS_CENTRE 128.0f //if focus is 128 or less than it starts blurring far obejcts very slowly

float R_MME_FocusScale(float focus) {
	return (focus < FOCUS_CENTRE) ? ((focus / FOCUS_CENTRE) * (1.0f + ((1.0f - (focus / FOCUS_CENTRE)) * (1.1f - 1.0f)))) : 1.0f; 
}

void R_MME_ClampDof(float *focus, float *radius) {
	if (*radius <= 0.0f && *focus <= 0.0f) *radius = mme_dofRadius->value;
	if (*radius < 0.0f) *radius = 0.0f;	
	if (*focus <= 0.0f) *focus = mme_depthFocus->value;
	if (*focus < 0.001f) *focus = 0.001f;
}
