#ifndef cg_draw_h_included
#define cg_draw_h_included

#include "../game/bg_public.h"
#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"  // stereoFrame_t
#include "cg_public.h"

#define TEXT_PIC_PAINT_ICON 256
#define TEXT_PIC_PAINT_COLOR 257
#define TEXT_PIC_PAINT_NEWLINE 258
#define TEXT_PIC_PAINT_XOFFSET 259
#define TEXT_PIC_PAINT_YOFFSET 260
#define TEXT_PIC_PAINT_FONT 261
#define TEXT_PIC_PAINT_SCALE 262
#define TEXT_PIC_PAINT_ICONSCALE 263
#define TEXT_PIC_PAINT_STYLE 264

extern	int sortedTeamPlayers[TEAM_MAXOVERLAY];
extern	int numSortedTeamPlayers;
extern	int drawTeamOverlayModificationCount;
extern  char systemChat[256];
extern  char teamChat1[256];
extern  char teamChat2[256];

float CG_Text_Pic_Width (const floatint_t *text, float scale, float iconScale, int limit, float textHeight, const fontInfo_t *fontOrig);
float CG_Text_Width(const char *text, float scale, int limit, const fontInfo_t *font);
float CG_Text_Width_old(const char *text, float scale, int limit, int fontIndex);
float CG_Text_Height(const char *text, float scale, int limit, const fontInfo_t *font);
float CG_Text_Height_old(const char *text, float scale, int limit, int fontIndex);

// unused
//void CG_Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader);

void CG_Text_PaintCharScale (float x, float y, float width, float height, float xscale, float yscale, float s, float t, float s2, float t2, qhandle_t hShader);

void CG_Text_Paint(float x, float y, float scale, const vec4_t color, const char *text, float adjust, int limit, int style, const fontInfo_t *font);
void CG_Text_Pic_Paint (float x, float y, float scale, const vec4_t color, const floatint_t *text, float adjust, int limit, int style, const fontInfo_t *fontOrig, float textHeight, float iconScale);

void CG_Text_Paint_Bottom (float x, float y, float scale, const vec4_t color, const char *text, float adjust, int limit, int style, const fontInfo_t *font);
void CG_Text_Paint_old(float x, float y, float scale, const vec4_t color, const char *text, float adjust, int limit, int style, int fontIndex);

void CG_Draw3DModel( float x, float y, float w, float h, qhandle_t model, qhandle_t skin, const vec3_t origin, const vec3_t angles );
void CG_DrawHead( float x, float y, float w, float h, int clientNum, const vec3_t headAngles, qboolean useDefaultTeamSkin, qboolean useChangedModel, qboolean isScoreboard );
void CG_DrawFlagModel( float x, float y, float w, float h, int team, qboolean force2D );
void CG_DrawTeamBackground( int x, int y, int w, int h, float alpha, int team );

#define LAG_SAMPLES		128

typedef struct {
	int             frameSamples[LAG_SAMPLES];
	int             frameCount;
	int             snapshotFlags[LAG_SAMPLES];
	int             snapshotSamples[LAG_SAMPLES];
	int             snapshotCount;
} lagometer_t;

extern lagometer_t  lagometer;

void CG_ResetLagometer (void);
void CG_AddLagometerFrameInfo( void );
void CG_AddLagometerSnapshotInfo( snapshot_t *snap );
void CG_LagometerMarkNoMove (void);

void CG_CenterPrint (const char *str, float y, int charWidth);
void CG_CenterPrintFragMessage (const char *str, float y, int charWidth);
floatint_t *CG_CreateFragString (qboolean lastFrag, int indexNum, const char *tokenStringOverride);
void CG_CreateNewCrosshairs (void);

void CG_Fade( int a, int time, int duration );
void CG_DrawFlashFade( void );

void CG_DrawActive( stereoFrame_t stereoView );

const fontInfo_t *CG_ScaleFont (const fontInfo_t *font, float *scale, float	*useScale);
void CG_PrintPicString (const floatint_t *s);

// same size as CG_CreateFragString
extern floatint_t tmpExtString[MAX_STRING_CHARS];

#endif  // cg_draw_h_included
