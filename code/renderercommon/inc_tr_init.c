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