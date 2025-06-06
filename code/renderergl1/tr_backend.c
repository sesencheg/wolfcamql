/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "tr_local.h"

backEndData_t	*backEndData;
backEndState_t	backEnd;


static float	s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};

void	RB_SetGL2D (void);
static void RB_QLPostProcessing (void);

/*
** GL_Bind
*/
image_t *CurrentBoundImage;

void GL_Bind( image_t *image ) {
	int texnum;

	if ( !image ) {
		ri.Printf( PRINT_WARNING, "GL_Bind: NULL image\n" );
		texnum = tr.defaultImage->texnum;
		CurrentBoundImage = tr.defaultImage;
	} else {
		texnum = image->texnum;
		CurrentBoundImage = image;
	}

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		texnum = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[glState.currenttmu] != texnum ) {
		if ( image ) {
			image->frameUsed = tr.frameCount;
		}
		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture (GL_TEXTURE_2D, texnum);
		//ri.Printf(PRINT_ALL, "GL_Bind %d\n", texnum);
	}
}

/*
** GL_SelectTextureUnit
*/
void GL_SelectTextureUnit (int unit)
{
	if (!qglActiveTextureARB) {
		return;
	}

	if ( glState.currenttmu == unit )
	{
		return;
	}

	if ( unit == 0 )
	{
		qglActiveTextureARB( GL_TEXTURE0_ARB );
		GLimp_LogComment( "glActiveTextureARB( GL_TEXTURE0_ARB )\n" );
		qglClientActiveTextureARB( GL_TEXTURE0_ARB );
		GLimp_LogComment( "glClientActiveTextureARB( GL_TEXTURE0_ARB )\n" );
	}
	else if ( unit == 1 )
	{
		qglActiveTextureARB( GL_TEXTURE1_ARB );
		GLimp_LogComment( "glActiveTextureARB( GL_TEXTURE1_ARB )\n" );
		qglClientActiveTextureARB( GL_TEXTURE1_ARB );
		GLimp_LogComment( "glClientActiveTextureARB( GL_TEXTURE1_ARB )\n" );
	} else {
		ri.Error( ERR_DROP, "GL_SelectTextureUnit: unit = %i", unit );
	}

	glState.currenttmu = unit;
}


/*
** GL_BindMultitexture
*/
void GL_BindMultitexture( image_t *image0, GLuint env0, image_t *image1, GLuint env1 ) {
	int		texnum0, texnum1;

	texnum0 = image0->texnum;
	texnum1 = image1->texnum;

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		texnum0 = texnum1 = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[1] != texnum1 ) {
		GL_SelectTextureUnit( 1 );
		image1->frameUsed = tr.frameCount;
		glState.currenttextures[1] = texnum1;
		qglBindTexture( GL_TEXTURE_2D, texnum1 );
		//ri.Printf(PRINT_ALL, "GL_BindMulti1 %d\n", texnum1);
	}
	if ( glState.currenttextures[0] != texnum0 ) {
		GL_SelectTextureUnit( 0 );
		image0->frameUsed = tr.frameCount;
		glState.currenttextures[0] = texnum0;
		qglBindTexture( GL_TEXTURE_2D, texnum0 );
		//ri.Printf(PRINT_ALL, "GL_BindMulti0 %d\n", texnum0);
	}
}


/*
** GL_Cull
*/
void GL_Cull( int cullType ) {
	if ( glState.faceCulling == cullType ) {
		return;
	}

	glState.faceCulling = cullType;

	if ( cullType == CT_TWO_SIDED )
	{
		qglDisable( GL_CULL_FACE );
	}
	else
	{
		qboolean cullFront;
		qglEnable( GL_CULL_FACE );

		cullFront = (cullType == CT_FRONT_SIDED);
		if ( backEnd.viewParms.isMirror )
		{
			cullFront = !cullFront;
		}

		qglCullFace( cullFront ? GL_FRONT : GL_BACK );
	}
}

/*
** GL_TexEnv
*/
void GL_TexEnv( int env )
{
	if ( env == glState.texEnv[glState.currenttmu] )
	{
		return;
	}

	glState.texEnv[glState.currenttmu] = env;


	switch ( env )
	{
	case GL_MODULATE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		break;
	case GL_REPLACE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		break;
	case GL_DECAL:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
		break;
	case GL_ADD:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD );
		break;
	default:
		ri.Error( ERR_DROP, "GL_TexEnv: invalid env '%d' passed", env );
		break;
	}
}

/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/
void GL_State( unsigned long stateBits )
{
	unsigned long diff = stateBits ^ glState.glStateBits;

	if ( !diff )
	{
		return;
	}

	//
	// check depthFunc bits
	//
	if ( diff & GLS_DEPTHFUNC_EQUAL )
	{
		if ( stateBits & GLS_DEPTHFUNC_EQUAL )
		{
			qglDepthFunc( GL_EQUAL );
			//qglDepthFunc(GL_GEQUAL);
		}
		else
		{
			qglDepthFunc( GL_LEQUAL );
			//qglDepthFunc(GL_LESS);
		}
	}

#if 0  // debugging
	if (Cvar_FindVar("debug_depthfunc")) {
		qglDepthFunc(GL_NEVER + Cvar_VariableIntegerValue("debug_depthfunc"));
	}
#endif

	//
	// check blend bits
	//
	if ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
	{
		GLenum srcFactor = GL_ONE, dstFactor = GL_ONE;

		if ( stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
		{
			switch ( stateBits & GLS_SRCBLEND_BITS )
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			default:
				ri.Error( ERR_DROP, "GL_State: invalid src blend state bits" );
				break;
			}

			switch ( stateBits & GLS_DSTBLEND_BITS )
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				ri.Error( ERR_DROP, "GL_State: invalid dst blend state bits" );
				break;
			}

			qglEnable( GL_BLEND );
			qglBlendFunc( srcFactor, dstFactor );
		}
		else
		{
			qglDisable( GL_BLEND );
		}
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK_TRUE )
	{
		if ( stateBits & GLS_DEPTHMASK_TRUE )
		{
			qglDepthMask( GL_TRUE );
		}
		else
		{
			qglDepthMask( GL_FALSE );
		}
	}

	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE )
	{
		if ( stateBits & GLS_POLYMODE_LINE )
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		else
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// depthtest
	//
	if ( diff & GLS_DEPTHTEST_DISABLE )
	{
		if ( stateBits & GLS_DEPTHTEST_DISABLE )
		{
			qglDisable( GL_DEPTH_TEST );
		}
		else
		{
			qglEnable( GL_DEPTH_TEST );
		}
	}

	//
	// alpha test
	//
	if ( diff & GLS_ATEST_BITS )
	{
		switch ( stateBits & GLS_ATEST_BITS )
		{
		case 0:
			qglDisable( GL_ALPHA_TEST );
			break;
		case GLS_ATEST_GT_0:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.0f );
			break;
		case GLS_ATEST_LT_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_LESS, 0.5f );
			break;
		case GLS_ATEST_GE_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.5f );
			break;
		default:
			assert( 0 );
			break;
		}
	}

	glState.glStateBits = stateBits;
}



/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace( void ) {
	float		c;

	if ( !backEnd.isHyperspace ) {
		// do initialization shit
	}

	c = ( backEnd.refdef.time & 255 ) / 255.0f;
	if (r_teleporterFlash->integer == 0) {
		c = 0;
	}
	qglClearColor( c, c, c, 1 );
	qglClear( GL_COLOR_BUFFER_BIT );

	backEnd.isHyperspace = qtrue;
}


static void SetViewportAndScissor( void ) {
	qglMatrixMode(GL_PROJECTION);
	qglLoadMatrixf( backEnd.viewParms.projectionMatrix );
	qglMatrixMode(GL_MODELVIEW);

	// set the window clipping
	qglViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	qglScissor( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
}

/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
void RB_BeginDrawingView (void) {
	int clearBits = 0;

	// sync with gl if needed
	if ( r_finish->integer == 1 && !glState.finishCalled ) {
		qglFinish ();
		glState.finishCalled = qtrue;
	}
	if ( r_finish->integer == 0 ) {
		glState.finishCalled = qtrue;
	}

	if (tr.usingMultiSample) {
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tr.frameBufferMultiSample);
	} else if (tr.usingFinalFrameBufferObject) {
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tr.frameBuffer);
	}

	if (tr.worldMapLoaded) {
		if (r_cloudHeight->modified) {
			if (*r_cloudHeight->string) {
				R_InitSkyTexCoords(r_cloudHeight->value);
			} else {
				if (r_cloudHeightOrig) {
					R_InitSkyTexCoords(r_cloudHeightOrig->value);
				} else {
					R_InitSkyTexCoords(256);
				}
			}
			r_cloudHeight->modified = qfalse;
		}

		if (r_singleShaderName->modified) {
			R_CreateSingleShader();
			r_singleShaderName->modified = qfalse;
		}
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

	// ensures that depth writes are enabled for the depth clear
	GL_State( GLS_DEFAULT );
	// clear relevant buffers
	clearBits = GL_DEPTH_BUFFER_BIT;

	if ( r_measureOverdraw->integer  ||  r_shadows->integer == 2  ||  tr.usingFboStencil)
	{
		clearBits |= GL_STENCIL_BUFFER_BIT;
	}
	if (r_fastsky->integer == 1  && !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL )) {
		clearBits |= GL_COLOR_BUFFER_BIT;	// FIXME: only if sky shaders have been used
		if (*r_fastSkyColor->string) {
			int v, tr, tg, tb;

			v = r_fastSkyColor->integer;
			tr = (v & 0xff0000) / 0x010000;
			tg = (v & 0x00ff00) / 0x000100;
			tb = (v & 0x0000ff) / 0x000001;

			qglClearColor((float)tr / 255.0, (float)tg / 255.0, (float)tb / 255.0, 1.0);
		} else {
			qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );	// FIXME: get color of sky
		}
	}

	qglClear( clearBits );

	if ( ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) )
	{
		RB_Hyperspace();
		return;
	}
	else
	{
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;		// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	// clip to the plane of the portal
	if ( backEnd.viewParms.isPortal ) {
		float	plane[4];
		GLdouble plane2[4];

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct (backEnd.viewParms.or.axis[0], plane);
		plane2[1] = DotProduct (backEnd.viewParms.or.axis[1], plane);
		plane2[2] = DotProduct (backEnd.viewParms.or.axis[2], plane);
		plane2[3] = DotProduct (plane, backEnd.viewParms.or.origin) - plane[3];

		qglLoadMatrixf( s_flipMatrix );
		qglClipPlane (GL_CLIP_PLANE0, plane2);
		qglEnable (GL_CLIP_PLANE0);
	} else {
		qglDisable (GL_CLIP_PLANE0);
	}
}

// reset to NULL when vid restart
static cameraPoint_t *PLcameraPoints = NULL;
static int *PLnumCameraPoints = NULL;
static vec3_t *PLsplinePoints = NULL;
static int *PLnumSplinePoints = NULL;
static vec4_t PLcolor;

void RE_SetPathLines (int *numCameraPoints, cameraPoint_t *cameraPoints, int *numSplinePoints, vec3_t *splinePoints, const vec4_t color)
{
	PLnumCameraPoints = numCameraPoints;
	PLcameraPoints = cameraPoints;
	PLnumSplinePoints = numSplinePoints;
	PLsplinePoints = splinePoints;
	Vector4Copy(color, PLcolor);
}

static void RE_DrawPathLines (void)
{
	int	i;
	int j;
	cameraPoint_t *cp, *cpnext;
	int pcvar;

#if 0
	if (!PLnumSplinePoints  ||  *PLnumSplinePoints <= 0) {
		return;
	}
	if (!PLsplinePoints) {
		return;
	}
#endif

	if (!PLnumCameraPoints  ||  *PLnumCameraPoints < 2) {
		return;
	}

	pcvar = ri.Cvar_VariableIntegerValue("cg_drawCameraPath");
	if (!pcvar) {
		return;
	}

	GL_Bind(tr.whiteImage);
	qglColor3f(PLcolor[0], PLcolor[1], PLcolor[2]);
	//qglDepthRange(0, 0);	// never occluded
	qglDepthRange(0, 1);	// occluded
	GL_State(GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE);

	qglBegin(GL_LINES);

	for (i = 0;  i < *PLnumCameraPoints - 1;  i++) {
		cp = &PLcameraPoints[i];
		if (!(cp->flags & CAM_ORIGIN)) {
			continue;
		}
		cpnext = cp->next;
		while (cpnext  &&  !(cpnext->flags & CAM_ORIGIN)) {
			cpnext = cpnext->next;
		}
		if (!cpnext) {
			continue;
		}
		if (cp->type == CAMERA_SPLINE  ||  cp->type == CAMERA_SPLINE_BEZIER  ||  cp->type == CAMERA_SPLINE_CATMULLROM  ||  (cp->type == CAMERA_CURVE  &&  cp->hasQuadratic)) {
			qglColor3f(PLcolor[0], PLcolor[1], PLcolor[2]);
			if (cp->type == CAMERA_CURVE) {
				qglColor3f(1.0, 0.3, 0.3);
			} else if (cp->type == CAMERA_SPLINE_BEZIER) {
				qglColor3f(0.3, 1.0, 0.3);
			} else if (cp->type == CAMERA_SPLINE_CATMULLROM) {
				qglColor3f(0.3, 0.3, 1.0);
			} else if (cp->type == CAMERA_SPLINE) {
				qglColor3f(1.0, 0.7, 0.1);
			}
			for (j = cp->splineStart;  j < cpnext->splineStart;  j++) {
				qglVertex3fv(PLsplinePoints[j]);
				qglVertex3fv(PLsplinePoints[j + 1]);
			}
		} else if (cp->type == CAMERA_INTERP  ||  cp->type == CAMERA_CURVE) {  //FIXME curve
			qglColor3f(0.5, 0.5, 1);
			qglVertex3fv(cp->origin);
			if (cpnext->type == CAMERA_SPLINE) {
				qglVertex3fv(PLsplinePoints[cpnext->splineStart]);
			} else {
				qglVertex3fv(cpnext->origin);
			}
		}
	}

	if (pcvar == 2) {
		for (i = 0;  i < *PLnumCameraPoints;  i++) {
			vec3_t origin;

			cp = &PLcameraPoints[i];
			if (!(cp->flags & CAM_ORIGIN)) {
				continue;
			}
			qglColor3f(PLcolor[0], PLcolor[1], PLcolor[2]);
			VectorCopy(PLsplinePoints[cp->splineStart], origin);
			origin[2] += 10;
			qglVertex3fv(origin);
			origin[2] -= 20;
			qglVertex3fv(origin);

			qglColor3f(0.5, 0.5, 1);
			VectorCopy(cp->origin, origin);
			origin[2] += 10;
			qglVertex3fv(origin);
			origin[2] -= 20;
			qglVertex3fv(origin);
		}
	}

	qglEnd();

	qglDepthRange(0, 1);
}


//#define GTEST_SAMPLER 1

#ifdef GTEST_SAMPLER
static byte Testdata[2000 * 2000 * 4];


static void save_texture (const char *name)
{
	int i;
	byte *buffer;
	int width;
	int height;

	width = glConfig.vidWidth;
	height = glConfig.vidHeight;

	buffer = Testdata;
	Com_Memset(buffer, 0, 18);
	buffer[2] = 2;          // uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24 + 8 * 1;        // pixel size
	for (i = 18;  i < width * height * 4 + 18;  i += 4) {
		Testdata[i + 3] = 255;
	}

	ri.FS_WriteFile(name, Testdata, width * height * 4 + 18);
}
#endif

static void RB_QLBloomDownSample (void)
{
	GLenum target;
	int width, height;
	GLint loc;

	GL_SelectTextureUnit(0);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	target = GL_TEXTURE_RECTANGLE_ARB;

	width = glConfig.vidWidth;
	height = glConfig.vidHeight;

	qglBindTexture(target, tr.backBufferTexture);

	qglCopyTexSubImage2D(target, 0, 0, 0, 0, 0, glConfig.vidWidth, glConfig.vidHeight);

	GL_SelectTextureUnit(1);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	qglBindTexture(target, tr.bloomTexture);

	qglUseProgramObjectARB(tr.downSample1Sp);
	loc = qglGetUniformLocationARB(tr.downSample1Sp, "backBufferTex");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get backBufferTex", __FUNCTION__);
	}
	qglUniform1iARB(loc, 0);

	//qglDisable(GL_BLEND);

    qglBegin(GL_QUADS);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, 0, 0);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, 0, 0);
	qglVertex2i(0, tr.bloomHeight);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, width, 0);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, width, 0);
	qglVertex2i(tr.bloomWidth, tr.bloomHeight);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, width, height);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, width, height);
	qglVertex2i(tr.bloomWidth, 0);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, 0, height);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, 0, height);
	qglVertex2i(0, 0);

	qglEnd();

	qglUseProgramObjectARB(0);

	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglDisable(GL_TEXTURE_2D);

	GL_SelectTextureUnit(0);
	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);
}

static void RB_QLBloomBrightness (void)
{
	GLenum target;
	int width, height;
	GLint loc;

	GL_SelectTextureUnit(0);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	target = GL_TEXTURE_RECTANGLE_ARB;

	width = tr.bloomWidth;
	height = tr.bloomHeight;

	qglBindTexture(target, tr.bloomTexture);
	qglCopyTexSubImage2D(target, 0, 0, 0, 0, glConfig.vidHeight - height, width, height);

	qglUseProgramObjectARB(tr.brightPassSp);
	loc = qglGetUniformLocationARB(tr.brightPassSp, "backBufferTex");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get backBufferTex", __FUNCTION__);
	}

	qglUniform1iARB(loc, 0);
	loc = qglGetUniformLocationARB(tr.brightPassSp, "p_brightthreshold");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get p_brightthreshold", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)r_BloomBrightThreshold->value);

	qglBegin(GL_QUADS);

	qglTexCoord2i(0, 0);
	qglVertex2i(0, height);

	qglTexCoord2i(width, 0);
	qglVertex2i(width, height);

	qglTexCoord2i(width, height);
	qglVertex2i(width, 0);

	qglTexCoord2i(0, height);
	qglVertex2i(0, 0);

	qglEnd();

	qglUseProgramObjectARB(0);

	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);
}

static void RB_QLBloomBlurHorizontal (void)
{
	GLenum target;
	int width, height;
	GLint loc;

	GL_SelectTextureUnit(0);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	target = GL_TEXTURE_RECTANGLE_ARB;

	width = tr.bloomWidth;
	height = tr.bloomHeight;

	qglBindTexture(target, tr.bloomTexture);
	qglCopyTexSubImage2D(target, 0, 0, 0, 0, glConfig.vidHeight - height, width, height);

	qglUseProgramObjectARB(tr.blurHorizSp);
	loc = qglGetUniformLocationARB(tr.blurHorizSp, "backBufferTex");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get backBufferTex", __FUNCTION__);
	}
	qglUniform1iARB(loc, 0);

	qglBegin(GL_QUADS);

	qglTexCoord2i(0, 0);
	qglVertex2i(0, height);

	qglTexCoord2i(width, 0);
	qglVertex2i(width, height);

	qglTexCoord2i(width, height);
	qglVertex2i(width, 0);

	qglTexCoord2i(0, height);
	qglVertex2i(0, 0);

	qglEnd();

	qglUseProgramObjectARB(0);

	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);
}

static void RB_QLBloomBlurVertical (void)
{
	GLenum target;
	int width, height;
	GLint loc;

	GL_SelectTextureUnit(0);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	target = GL_TEXTURE_RECTANGLE_ARB;

	width = tr.bloomWidth;
	height = tr.bloomHeight;

	qglBindTexture(target, tr.bloomTexture);
	qglCopyTexSubImage2D(target, 0, 0, 0, 0, glConfig.vidHeight - height, width, height);

	qglUseProgramObjectARB(tr.blurVerticalSp);
	loc = qglGetUniformLocationARB(tr.blurVerticalSp, "backBufferTex");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get backBufferTex", __FUNCTION__);
	}
	qglUniform1iARB(loc, 0);

	qglBegin(GL_QUADS);

	qglTexCoord2i(0, 0);
	qglVertex2i(0, height);

	qglTexCoord2i(width, 0);
	qglVertex2i(width, height);

	qglTexCoord2i(width, height);
	qglVertex2i(width, 0);

	qglTexCoord2i(0, height);
	qglVertex2i(0, 0);

	qglEnd();

	qglUseProgramObjectARB(0);

	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);
}

static void RB_QLBloomCombine (void)
{
	GLenum target;
	int width, height;
	GLint loc;

	GL_SelectTextureUnit(0);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);
	target = GL_TEXTURE_RECTANGLE_ARB;

	width = tr.bloomWidth;
	height = tr.bloomHeight;

	qglBindTexture(target, tr.bloomTexture);
	qglCopyTexSubImage2D(target, 0, 0, 0, 0, glConfig.vidHeight - height, width, height);

	qglUseProgramObjectARB(tr.combineSp);

	qglBindTexture(target, tr.backBufferTexture);
	loc = qglGetUniformLocationARB(tr.combineSp, "backBufferTex");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get backBufferTex", __FUNCTION__);
	}
	qglUniform1iARB(loc, 0);

	GL_SelectTextureUnit(1);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);

	qglBindTexture(target, tr.bloomTexture);
	loc = qglGetUniformLocationARB(tr.combineSp, "bloomTex");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get bloomTex", __FUNCTION__);
	}
	qglUniform1iARB(loc, 1);

	loc = qglGetUniformLocationARB(tr.combineSp, "p_bloomsaturation");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get p_bloomsaturation", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)r_BloomSaturation->value);

	loc = qglGetUniformLocationARB(tr.combineSp, "p_scenesaturation");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get p_scenesaturation", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)r_BloomSceneSaturation->value);

	loc = qglGetUniformLocationARB(tr.combineSp, "p_bloomintensity");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get p_bloomintensity", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)r_BloomIntensity->value);

	loc = qglGetUniformLocationARB(tr.combineSp, "p_sceneintensity");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get p_sceneintensity", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)r_BloomSceneIntensity->value);


	width = glConfig.vidWidth;
	height = glConfig.vidHeight;

    qglBegin(GL_QUADS);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, 0, 0);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, 0, 0);
	qglVertex2i(0, height);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, width, 0);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, tr.bloomWidth, 0);
	qglVertex2i(width, height);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, width, height);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, tr.bloomWidth, tr.bloomHeight);
	qglVertex2i(width, 0);

	qglMultiTexCoord2iARB(GL_TEXTURE0_ARB, 0, height);
	qglMultiTexCoord2iARB(GL_TEXTURE1_ARB, 0, tr.bloomHeight);
	qglVertex2i(0, 0);

	qglEnd();

	qglUseProgramObjectARB(0);

	GL_SelectTextureUnit(1);
	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglDisable(GL_TEXTURE_2D);

	GL_SelectTextureUnit(0);
	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);
}

static void RB_QLBloom (void)
{
	int i;

	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	RB_SetGL2D();
	GL_State(GLS_DEPTHTEST_DISABLE);

	for (i = 0;  i < r_BloomPasses->integer;  i++) {
		RB_QLBloomDownSample();
		RB_QLBloomBrightness();
		if (r_BloomDebug->integer < 2) {
			RB_QLBloomBlurHorizontal();
			RB_QLBloomBlurVertical();
		}
		if (r_BloomDebug->integer == 0  ||  r_BloomDebug->integer == 3) {
			RB_QLBloomCombine();
		}
	}
}

static void RB_QLPostProcessing (void)
{
	//GLenum target;
	//int width, height;

	//ri.Printf(PRINT_ALL, "count %d\n", tr.drawSurfsCount);
	if (tr.drawSurfsCount != 0) {
		//return;
	}


	if (!r_enablePostProcess->integer  ||  !glConfig.qlGlsl) {
		return;
	}

	if (!r_enableBloom->integer) {
		return;
	}

	// 2017-03-23 qglScissor() not working with bloom and 3d icons which set
	//  a new scissor value  --  GL_RENDERER: AMD Radeon R9 M370X
	//FIXME not enough to set scissor to size of viewport like in RB_SetGL2D()?
	qglDisable(GL_SCISSOR_TEST);


	//qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	// since bloom is using readpixels()
	if (tr.usingMultiSample) {
		qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.frameBufferMultiSample);
		qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.frameBuffer);
		if (tr.usingFboStencil) {
			// packed depth stencil, but apparently some cards have a problem
			// if GL_STENCIL_BUFFER_BIT used
			qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		} else {
			qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		//qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.frameBuffer);
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tr.frameBuffer);
	}

	RB_QLBloom();

	// anti alias 2d
	if (tr.usingMultiSample) {
		qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.frameBufferMultiSample);
		qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.frameBuffer);
		if (0) {  //(tr.usingFboStencil) {
			// packed depth stencil, but apparently some cards have a problem
			// if GL_STENCIL_BUFFER_BIT used
			qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		} else {
			qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		//qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.frameBuffer);
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tr.frameBufferMultiSample);
	}

	qglEnable(GL_SCISSOR_TEST);
}

static void RB_ColorCorrect (void)
{
	GLint loc;
	GLenum target;
	int width, height;
	int shift;
	float mul;

	if (!r_enablePostProcess->integer  ||  !r_enableColorCorrect->integer  ||  !glConfig.qlGlsl) {
		return;
	}

	GL_SelectTextureUnit(0);
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_TEXTURE_RECTANGLE_ARB);

	target = GL_TEXTURE_RECTANGLE_ARB;

	width = glConfig.vidWidth;
	height = glConfig.vidHeight;

	qglBindTexture(target, tr.backBufferTexture);
	qglCopyTexSubImage2D(target, 0, 0, 0, 0, 0, glConfig.vidWidth, glConfig.vidHeight);

	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	RB_SetGL2D();
	GL_State( GLS_DEPTHTEST_DISABLE );

    qglUseProgramObjectARB(tr.colorCorrectSp);
	loc = qglGetUniformLocationARB(tr.colorCorrectSp, "p_gammaRecip");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get p_gammaRecip", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)(1.0 / r_gamma->value));

	mul = r_overBrightBitsValue->value;
	if (mul < 0.0) {
		mul = 0.0;
	}
	shift = tr.overbrightBits;

	loc = qglGetUniformLocationARB(tr.colorCorrectSp, "p_overbright");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get p_overbright", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)((float)(1 << shift) * mul));

	loc = qglGetUniformLocationARB(tr.colorCorrectSp, "p_contrast");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get p_contrast", __FUNCTION__);
	}
	qglUniform1fARB(loc, (GLfloat)r_contrast->value);

	loc = qglGetUniformLocationARB(tr.colorCorrectSp, "backBufferTex");
	if (loc < 0) {
		ri.Error(ERR_FATAL, "%s() couldn't get backBufferTex", __FUNCTION__);
	}
	qglUniform1iARB(loc, 0);

	qglBegin(GL_QUADS);

	qglTexCoord2i(0, 0);
	qglVertex2i(0, height);

	qglTexCoord2i(width, 0);
	qglVertex2i(width, height);

	qglTexCoord2i(width, height);
	qglVertex2i(width, 0);

	qglTexCoord2i(0, height);
	qglVertex2i(0, 0);

	qglEnd();

	qglUseProgramObjectARB(0);

	qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	qglEnable(GL_TEXTURE_2D);
}

/*
==================
RB_RenderDrawSurfList
==================
*/
void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader, *oldShader;
	int				fogNum, oldFogNum;
	int				entityNum, oldEntityNum;
	int				dlighted, oldDlighted;
	qboolean		depthRange, oldDepthRange, isCrosshair, wasCrosshair;
	int				i;
	drawSurf_t		*drawSurf;
	int				oldSort;
	double			originalTime;

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView ();

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldFogNum = -1;
	oldDepthRange = qfalse;
	wasCrosshair = qfalse;
	oldDlighted = qfalse;
	oldSort = -1;
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++) {
		if ( drawSurf->sort == oldSort ) {
			// fast path, same as previous sort
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			continue;
		}
		oldSort = drawSurf->sort;
		R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from separate
		// entities merged into a single batch, like smoke and blood puff sprites
		if ( shader != NULL && ( shader != oldShader || fogNum != oldFogNum || dlighted != oldDlighted 
			|| ( entityNum != oldEntityNum && !shader->entityMergable ) ) ) {
			if (oldShader != NULL) {
				RB_EndSurface();
			}
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
			oldFogNum = fogNum;
			oldDlighted = dlighted;
		}

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = isCrosshair = qfalse;

			if ( entityNum != REFENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];

				// FIXME: e.shaderTime must be passed as int to avoid fp-precision loss issues
				backEnd.refdef.floatTime = originalTime - (double)backEnd.currentEntity->ent.shaderTime;

				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.or );

				// set up the dynamic lighting if needed
				if ( backEnd.currentEntity->needDlights ) {
					R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
				}

				if(backEnd.currentEntity->ent.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
					
					if(backEnd.currentEntity->ent.renderfx & RF_CROSSHAIR)
						isCrosshair = qtrue;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;
				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
				R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
			}

			qglLoadMatrixf( backEnd.or.modelMatrix );

			//
			// change depthrange. Also change projection matrix so first person weapon does not look like coming
			// out of the screen.
			//
			if (oldDepthRange != depthRange || wasCrosshair != isCrosshair)
			{
				if (depthRange)
				{
					if(backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						if(isCrosshair)
						{
							if(oldDepthRange)
							{
								// was not a crosshair but now is, change back proj matrix
								qglMatrixMode(GL_PROJECTION);
								qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
								qglMatrixMode(GL_MODELVIEW);
							}
						}
						else
						{
							viewParms_t temp = backEnd.viewParms;

							R_SetupProjection(&temp, r_znear->value, qfalse);

							qglMatrixMode(GL_PROJECTION);
							qglLoadMatrixf(temp.projectionMatrix);
							qglMatrixMode(GL_MODELVIEW);
						}
					}

					if(!oldDepthRange)
						qglDepthRange (0, 0.3);
				}
				else
				{
					if(!wasCrosshair && backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						qglMatrixMode(GL_PROJECTION);
						qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
						qglMatrixMode(GL_MODELVIEW);
					}

					qglDepthRange (0, 1);
				}

				oldDepthRange = depthRange;
				wasCrosshair = isCrosshair;
			}

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
	}

	backEnd.refdef.floatTime = originalTime;

	// draw the contents of the last shader batch
	if (oldShader != NULL) {
		RB_EndSurface();
	}

	// go back to the world modelview matrix
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		qglDepthRange (0, 1);
	}

	if (r_drawSun->integer) {
		RB_DrawSun(0.1, tr.sunShader);
	}

	// darken down any stencil shadows
	RB_ShadowFinish();		

	// add light flares on lights that aren't obscured
	RB_RenderFlares();
}

/*
============================================================================

RENDER BACK END FUNCTIONS

============================================================================
*/

/*
================
RB_SetGL2D

================
*/
void	RB_SetGL2D (void) {
	backEnd.projection2D = qtrue;

	// set 2D virtual screen size
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1);
	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	GL_State( GLS_DEPTHTEST_DISABLE |
			  GLS_SRCBLEND_SRC_ALPHA |
			  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	GL_Cull( CT_TWO_SIDED );
	qglDisable( GL_CLIP_PLANE0 );

	// set time for 2D shaders
	backEnd.refdef.time = ri.ScaledMilliseconds();
	backEnd.refdef.floatTime = backEnd.refdef.time * 0.001f;

	backEnd.refdef.realTime = ri.RealMilliseconds();
	backEnd.refdef.realFloatTime = backEnd.refdef.realTime * 0.001f;
}

/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
void RE_StretchRaw (int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty) {
	int			i, j;
	int			start, end;

	if ( !tr.registered ) {
		return;
	}
	R_IssuePendingRenderCommands();

	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	// we definitely want to sync every frame for the cinematics
	qglFinish();

	start = 0;
	if ( r_speeds->integer ) {
		start = ri.RealMilliseconds();
	}

	// make sure rows and cols are powers of 2
	for ( i = 0 ; ( 1 << i ) < cols ; i++ ) {
	}
	for ( j = 0 ; ( 1 << j ) < rows ; j++ ) {
	}
	if ( ( 1 << i ) != cols || ( 1 << j ) != rows) {
		ri.Error (ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

	RE_UploadCinematic (w, h, cols, rows, data, client, dirty);
	GL_Bind( tr.scratchImage[client] );

	if ( r_speeds->integer ) {
		end = ri.RealMilliseconds();
		ri.Printf( PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start );
	}

	RB_SetGL2D();

	qglColor3f( tr.identityLight, tr.identityLight, tr.identityLight );

	qglBegin (GL_QUADS);
	qglTexCoord2f ( 0.5f / cols,  0.5f / rows );
	qglVertex2f (x, y);
	qglTexCoord2f ( ( cols - 0.5f ) / cols ,  0.5f / rows );
	qglVertex2f (x+w, y);
	qglTexCoord2f ( ( cols - 0.5f ) / cols, ( rows - 0.5f ) / rows );
	qglVertex2f (x+w, y+h);
	qglTexCoord2f ( 0.5f / cols, ( rows - 0.5f ) / rows );
	qglVertex2f (x, y+h);
	qglEnd ();
}

// glConfig.vidWidth x glConfig.vidHeight rgb data
static void RE_StretchRawRectScreen (const byte *data)
{
	int start, end;
	GLenum target;
	GLenum err;
	int width;
	int height;

	if ( !tr.registered ) {
		return;
	}

	if (!glConfig.textureRectangleAvailable) {
		static qboolean warningIssued = qfalse;

		if (!warningIssued) {
			ri.Printf(PRINT_ALL, "^3%s: rectangle texture not supported\n", __FUNCTION__);
			warningIssued = qtrue;
		}

		return;
	}

	width = glConfig.vidWidth;
	height = glConfig.vidHeight;

	//R_IssuePendingRenderCommands();

	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	// we definitely want to sync every frame for the cinematics
	qglFinish();

	start = 0;
	if ( r_speeds->integer ) {
		start = ri.RealMilliseconds();
	}

	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	RB_SetGL2D();
	GL_State(GLS_DEPTHTEST_DISABLE);

	GL_SelectTextureUnit(0);

	target = GL_TEXTURE_RECTANGLE_ARB;

	qglDisable(GL_TEXTURE_2D);
	qglEnable(target);

	qglBindTexture(target, tr.rectScreenTexture);

	qglTexSubImage2D( target, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data );

	err = qglGetError();
	if (err != GL_NO_ERROR) {
		ri.Printf(PRINT_ALL, "^1opengl error updating rect texture:  0x%x\n", err);
	}

	if ( r_speeds->integer ) {
		end = ri.RealMilliseconds();
		ri.Printf( PRINT_ALL, "rect: qglTexSubImage2D %i, %i: %i msec\n", width, height, end - start );
	}

	//RB_SetGL2D();

	//qglColor3f( tr.identityLight, tr.identityLight, tr.identityLight );

	qglBegin (GL_QUADS);

	//FIXME testing
	//width /= 2;
	//height /= 2;

	qglTexCoord2i(0, 0);
	qglVertex2i(0, height);

	qglTexCoord2i(width, 0);
	qglVertex2i(width, height);

	qglTexCoord2i(width, height);
	qglVertex2i(width, 0);

	qglTexCoord2i(0, height);
	qglVertex2i(0, 0);

	qglEnd();

	qglDisable(target);
	qglEnable(GL_TEXTURE_2D);

	//qglFinish();
}

void RE_UploadCinematic (int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty) {

	GL_Bind( tr.scratchImage[client] );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height ) {
		tr.scratchImage[client]->width = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, haveClampToEdge ? GL_CLAMP_TO_EDGE : GL_CLAMP );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, haveClampToEdge ? GL_CLAMP_TO_EDGE : GL_CLAMP );
	} else {
		if (dirty) {
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
		}
	}
}


/*
=============
RB_SetColor

=============
*/
const void	*RB_SetColor( const void *data ) {
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D[0] = cmd->color[0] * 255;
	backEnd.color2D[1] = cmd->color[1] * 255;
	backEnd.color2D[2] = cmd->color[2] * 255;
	backEnd.color2D[3] = cmd->color[3] * 255;

	return (const void *)(cmd + 1);
}

/*
=============
RB_StretchPic
=============
*/
const void *RB_StretchPic ( const void *data ) {
	const stretchPicCommand_t	*cmd;
	shader_t *shader;
	int		numVerts, numIndexes;

	cmd = (const stretchPicCommand_t *)data;

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}

	RB_CHECKOVERFLOW( 4, 6 );
	numVerts = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[ numIndexes ] = numVerts + 3;
	tess.indexes[ numIndexes + 1 ] = numVerts + 0;
	tess.indexes[ numIndexes + 2 ] = numVerts + 2;
	tess.indexes[ numIndexes + 3 ] = numVerts + 2;
	tess.indexes[ numIndexes + 4 ] = numVerts + 0;
	tess.indexes[ numIndexes + 5 ] = numVerts + 1;

	*(int *)tess.vertexColors[ numVerts ] =
		*(int *)tess.vertexColors[ numVerts + 1 ] =
		*(int *)tess.vertexColors[ numVerts + 2 ] =
		*(int *)tess.vertexColors[ numVerts + 3 ] = *(int *)backEnd.color2D;

	tess.xyz[ numVerts ][0] = cmd->x;
	tess.xyz[ numVerts ][1] = cmd->y;
	tess.xyz[ numVerts ][2] = 0;

	tess.texCoords[ numVerts ][0][0] = cmd->s1;
	tess.texCoords[ numVerts ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 1 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 1 ][1] = cmd->y;
	tess.xyz[ numVerts + 1 ][2] = 0;

	tess.texCoords[ numVerts + 1 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 1 ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 2 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 2 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 2 ][2] = 0;

	tess.texCoords[ numVerts + 2 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 2 ][0][1] = cmd->t2;

	tess.xyz[ numVerts + 3 ][0] = cmd->x;
	tess.xyz[ numVerts + 3 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 3 ][2] = 0;

	tess.texCoords[ numVerts + 3 ][0][0] = cmd->s1;
	tess.texCoords[ numVerts + 3 ][0][1] = cmd->t2;

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawSurfs

=============
*/
const void	*RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t	*cmd;

	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawBuffer

=============
*/
const void	*RB_DrawBuffer( const void *data ) {
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;

	if (tr.usingFinalFrameBufferObject) {
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	}

	qglDrawBuffer(cmd->buffer);
	qglReadBuffer(cmd->buffer);

	if (tr.usingFinalFrameBufferObject) {
		if (tr.usingMultiSample) {
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tr.frameBufferMultiSample);
		} else {
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tr.frameBuffer);
		}
		qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		qglReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	}

	// clear screen
	if ( r_clear->integer ) {
		if (*r_clearColor->string) {
			int v, tr, tg, tb;

			v = r_clearColor->integer;
			tr = (v & 0xff0000) / 0x010000;
			tg = (v & 0x00ff00) / 0x000100;
			tb = (v & 0x0000ff) / 0x000001;

			qglClearColor((float)tr / 255.0, (float)tg / 255.0, (float)tb / 255.0, 1.0);
		} else {
			qglClearColor( 1, 0, 0.5, 1 );
		}

		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	return (const void *)(cmd + 1);
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
void RB_ShowImages( void ) {
	int		i;
	image_t	*image;
	float	x, y, w, h;
	int		start, end;

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	start = ri.RealMilliseconds();

	for ( i=0 ; i<tr.numImages ; i++ ) {
		image = tr.images[i];

		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		// show in proportional size in mode 2
		if ( r_showImages->integer == 2 ) {
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		GL_Bind( image );
		qglBegin (GL_QUADS);
		qglTexCoord2f( 0, 0 );
		qglVertex2f( x, y );
		qglTexCoord2f( 1, 0 );
		qglVertex2f( x + w, y );
		qglTexCoord2f( 1, 1 );
		qglVertex2f( x + w, y + h );
		qglTexCoord2f( 0, 1 );
		qglVertex2f( x, y + h );
		qglEnd();
	}

	qglFinish();

	end = ri.RealMilliseconds();
	ri.Printf( PRINT_ALL, "%i msec to draw all images\n", end - start );

}

/*
=============
RB_ColorMask

=============
*/
const void *RB_ColorMask(const void *data)
{
	const colorMaskCommand_t *cmd = data;

	qglColorMask(cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3]);
	//ri.Printf(PRINT_ALL, "RB_ColorMask %d %d %d %d\n", cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3]);
	return (const void *)(cmd + 1);
}

/*
=============
RB_ClearDepth

=============
*/
const void *RB_ClearDepth(const void *data)
{
	const clearDepthCommand_t *cmd = data;

	if(tess.numIndexes)
		RB_EndSurface();

	// texture swapping test
	if (r_showImages->integer)
		RB_ShowImages();

	qglClear(GL_DEPTH_BUFFER_BIT);

	return (const void *)(cmd + 1);
}

/*
=============
RB_SwapBuffers

=============
*/
const void	*RB_SwapBuffers( const void *data ) {
	const swapBuffersCommand_t	*cmd;

	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	// texture swapping test
	if ( r_showImages->integer ) {
		RB_ShowImages();
	}

	cmd = (const swapBuffersCommand_t *)data;

	// we measure overdraw by reading back the stencil buffer and
	// counting up the number of increments that have happened
	if ( r_measureOverdraw->integer ) {
		int i;
		long sum = 0;
		unsigned char *stencilReadback;

		stencilReadback = ri.Hunk_AllocateTempMemory( glConfig.vidWidth * glConfig.vidHeight );
		qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback );

		for ( i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++ ) {
			sum += stencilReadback[i];
		}

		backEnd.pc.c_overDraw += sum;
		ri.Hunk_FreeTempMemory( stencilReadback );
	}


	if ( !glState.finishCalled ) {
		qglFinish();
	}

	GLimp_LogComment( "***************** RB_SwapBuffers *****************\n\n\n" );

	GLimp_EndFrame();

	backEnd.projection2D = qfalse;

	return (const void *)(cmd + 1);
}

/*
====================
RB_ExecuteRenderCommands
====================
*/

#if 0
#define dprintf(format, varargs...) ri.Printf(PRINT_ALL, format, ## varargs)
#else
  #define dprintf(format, varargs...)
#endif

void RB_ExecuteRenderCommands( const void *data ) {
	int		t1, t2;

	t1 = ri.RealMilliseconds ();

	while ( 1 ) {
		data = PADP(data, sizeof(void *));

		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer( data );
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers( data );
			break;
		case RC_SCREENSHOT:
			data = RB_TakeScreenshotCmd( data );
			break;
		case RC_VIDEOFRAME:
			data = RB_TakeVideoFrameCmd( data );
			break;
		case RC_COLORMASK:
			data = RB_ColorMask(data);
			break;
		case RC_CLEARDEPTH:
			data = RB_ClearDepth(data);
			break;
		case RC_END_OF_LIST:
		default:
			// stop rendering
			t2 = ri.RealMilliseconds ();
			backEnd.pc.msec = t2 - t1;
			return;
		}
	}

}