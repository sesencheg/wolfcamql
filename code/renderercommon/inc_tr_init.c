// inc_*  files referenced as #include since they need to access renderer
// (rendergl1 or rendergl2) specific data

//FIXME hack, older jpeg-6b could accept rgba input buffer, now it has to be rgb
static void convert_rgba_to_rgb (byte *buffer, int width, int height)
{
	byte *src;
	byte *dst;
	int totalSize;


	totalSize = width * height * 4;
	src = buffer;
	dst = buffer;
	while (src < (buffer + totalSize)) {
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		// skip alpha src[3]

		src += 4;
		dst += 3;
	}
}

// swap rgb to bgr
static void swap_bgr (byte *buffer, int width, int height, qboolean hasAlpha)
{
	int temp;
	int i;
	int c;
	int psize;

	psize = 3 + (hasAlpha ? 1 : 0);

	c = width * height * psize;
	for (i = 0;  i < c;  i += psize) {
		temp = buffer[i];
		buffer[i] = buffer[i + 2];
		buffer[i + 2] = temp;
	}
}

void R_MME_GetDepth (byte *output)
{
	float focusStart, focusEnd, focusMul;
	float zBase, zAdd, zRange;
	int i, pixelCount;
	byte *temp;

	if (mme_depthRange->value <= 0)  {
		return;
	}

	pixelCount = glConfig.vidWidth * glConfig.vidHeight;

	focusStart = mme_depthFocus->value - mme_depthRange->value;
	focusEnd = mme_depthFocus->value + mme_depthRange->value;
	focusMul = 255.0f / (2 * mme_depthRange->value);

	zRange = backEnd.viewParms.zFar - r_znear->value;
	zBase = ( backEnd.viewParms.zFar + r_znear->value ) / zRange;
	zAdd =  ( 2 * backEnd.viewParms.zFar * r_znear->value ) / zRange;

	//temp = (byte *)ri.Hunk_AllocateTempMemory( pixelCount * sizeof( float ) );
	temp = (byte *)*ri.Video_DepthBuffer;
	temp += 18;

	qglDepthRange( 0.0f, 1.0f );
	qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_DEPTH_COMPONENT, GL_FLOAT, (GLfloat *)temp );
	/* Could probably speed this up a bit with SSE but frack it for now */
	for ( i=0 ; i < pixelCount; i++ ) {
		/* Read from the 0 - 1 depth */
		GLfloat zVal = ((GLfloat *)temp)[i];
		int outVal;
		/* Back to the original -1 to 1 range */
		zVal = zVal * 2.0f - 1.0f;
		/* Back to the original z values */
		zVal = zAdd / ( zBase - zVal );
		/* Clip and scale the range that's been selected */
		if (zVal <= focusStart)
			outVal = 0;
		else if (zVal >= focusEnd)
			outVal = 255;
		else
			outVal = (zVal - focusStart) * focusMul;
		output[i] = outVal;
	}
	//ri.Hunk_FreeTempMemory( temp );
}