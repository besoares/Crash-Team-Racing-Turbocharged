// Dual monitor presentation: each two-player view fills its own window, and
// outside a dual view the second window mirrors the game. Included by
// native_depth_renderer_test.c after main.c, so renderer internals are visible.

static void DualTest_ReadWindowPixel(const SDL_Rect *viewport, float u, float v, u8 *pixel)
{
	// u, v: 0..1 across the viewport, v = 0 at its top.
	const int x = viewport->x + (int)(u * (float)(viewport->w - 1));
	const int y = viewport->y + (int)((1.0f - v) * (float)(viewport->h - 1));
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
}

static void DualTest_ExpectWindowColor(const char *what, const SDL_Rect *viewport, float u, float v, int r, int g, int b)
{
	u8 pixel[4];
	DualTest_ReadWindowPixel(viewport, u, v, pixel);
	if (abs(pixel[0] - r) > 12 || abs(pixel[1] - g) > 12 || abs(pixel[2] - b) > 12)
	{
		fprintf(stderr, "%s at %.2f,%.2f: %u,%u,%u; expected %d,%d,%d\n", what, u, v, pixel[0], pixel[1], pixel[2], r, g, b);
		abort();
	}
}

// One 512x216 frame: the top view red, the bottom view blue, the 4-row gap
// between them green. Outer rows of each view are white so a presentation that
// cropped instead of stretching, or showed the wrong rows, is caught.
static void DualTest_DrawSplitFrame(int viewHeight)
{
	NativePgxp_EndFrame();
	ClearSplits();
	SetDefDrawEnv(&activeDrawEnv, 0, 0, 512, 216);
	SetDefDispEnv(&activeDispEnv, 0, 0, 512, 216);
	activeDrawEnv.isbg = 1;
	activeDrawEnv.dfe = 1;
	NativeRenderer_BeginScene();
	NativeRenderer_Clear(0, 0, 512, 216, 0, 255, 0);
	NativeRenderer_Clear(0, 0, 512, viewHeight, 255, 0, 0);
	NativeRenderer_Clear(0, 216 - viewHeight, 512, viewHeight, 0, 0, 255);
	NativeRenderer_EndScene();
}

static void DualTest_Presentation(void)
{
	gNativeRendererMode = NATIVE_RENDERER_NATIVE;
	gNativeAntiAliasingMode = NATIVE_AA_OFF;
	gNativeColorDepth = NATIVE_COLOR_DEPTH_TRUE;
	gNativeDitheringEnabled = 0;
	gNativePgxpMode = NATIVE_PGXP_MODE_OFF;
	gNativeAspectRatio = NATIVE_ASPECT_16_9;
	Platform_InitScratchpad();
	NativeRenderer_EnableGamePresentation(1);
	SDL_ShowWindow(g_window);

	assert(NativeRenderer_OpenSecondWindow("CTR dual monitor test"));
	assert(g_secondWindow != NULL);
	SDL_ShowWindow(g_secondWindow);
	SDL_SetWindowSize(g_secondWindow, 640, 400);
	SDL_SyncWindow(g_secondWindow);
	SDL_GLContext context = SDL_GL_GetCurrentContext();
	assert(context != NULL);

	const int viewHeight = 106;
	for (int frame = 0; frame < 2; frame++)
	{
		const int dual = frame == 0;
		NativeAspect_SetDualView(dual ? viewHeight : 0, 216);
		DualTest_DrawSplitFrame(viewHeight);
		assert(NativeRenderer_IsDualViewFrame() == dual);

		const SDL_Rect first = s_presentViewport;
		const SDL_Rect second = s_secondPresentViewport;
		assert(first.w > 0 && first.h > 0 && second.w > 0 && second.h > 0);
		// Both windows keep the selected 16:9 ratio.
		assert(abs(first.w * 9 - first.h * 16) <= 16 && abs(second.w * 9 - second.h * 16) <= 16);

		const int largestH = (first.h > second.h) ? first.h : second.h;
		const int largestW = (first.w > second.w) ? first.w : second.w;
		if (dual)
		{
			// Every view gets the rows of the larger window.
			assert(s_mainRenderTarget.width == largestW);
			assert(abs(s_mainRenderTarget.height - largestH * 216 / viewHeight) <= 1);
		}
		else
		{
			assert(s_mainRenderTarget.width == first.w && s_mainRenderTarget.height == first.h);
		}

		NativeRenderer_PresentMainRenderTarget();
		if (dual)
		{
			// The main window shows only the top view, filling its viewport.
			DualTest_ExpectWindowColor("dual main top", &first, 0.5f, 0.02f, 255, 0, 0);
			DualTest_ExpectWindowColor("dual main centre", &first, 0.5f, 0.5f, 255, 0, 0);
			DualTest_ExpectWindowColor("dual main bottom", &first, 0.5f, 0.98f, 255, 0, 0);
		}
		else
		{
			DualTest_ExpectWindowColor("split main top", &first, 0.5f, 0.1f, 255, 0, 0);
			DualTest_ExpectWindowColor("split main gap", &first, 0.5f, 0.5f, 0, 255, 0);
			DualTest_ExpectWindowColor("split main bottom", &first, 0.5f, 0.9f, 0, 0, 255);
		}
		assert(glGetError() == GL_NO_ERROR);

		assert(NativeRenderer_DrawSecondWindow(context, 1));
		if (dual)
		{
			DualTest_ExpectWindowColor("dual second top", &second, 0.5f, 0.02f, 0, 0, 255);
			DualTest_ExpectWindowColor("dual second centre", &second, 0.5f, 0.5f, 0, 0, 255);
			DualTest_ExpectWindowColor("dual second bottom", &second, 0.5f, 0.98f, 0, 0, 255);
		}
		else
		{
			// Outside a dual view the second window mirrors the whole screen.
			DualTest_ExpectWindowColor("mirror top", &second, 0.5f, 0.1f, 255, 0, 0);
			DualTest_ExpectWindowColor("mirror gap", &second, 0.5f, 0.5f, 0, 255, 0);
			DualTest_ExpectWindowColor("mirror bottom", &second, 0.5f, 0.9f, 0, 0, 255);
		}
		// The 640x400 window is taller than 16:9, so it has black bars.
		const SDL_Rect whole = {0, 0, 640, 400};
		DualTest_ExpectWindowColor("second bar", &whole, 0.5f, 0.01f, 0, 0, 0);
		assert(glGetError() == GL_NO_ERROR);
		SDL_GL_SwapWindow(g_secondWindow);
		assert(SDL_GL_MakeCurrent(g_window, context));

		// A black second window, as during videos.
		assert(NativeRenderer_DrawSecondWindow(context, 0));
		DualTest_ExpectWindowColor("blank second", &second, 0.5f, 0.5f, 0, 0, 0);
		SDL_GL_SwapWindow(g_secondWindow);
		assert(SDL_GL_MakeCurrent(g_window, context));
		SDL_GL_SwapWindow(g_window);
	}

	// Pausing turns a dual frame into a split-screen backdrop: each view is
	// the centre of its single-player picture, so its outer rows are dropped.
	{
		NativeAspect_SetDualView(viewHeight, 216);
		DualTest_DrawSplitFrame(viewHeight);
		// Paint the outer quarter rows of each view white. The split-screen
		// crop keeps the middle viewHeight / 216 (~49%) of the rows.
		NativeRenderer_BeginScene();
		NativeRenderer_Clear(0, 0, 512, viewHeight / 4, 255, 255, 255);
		NativeRenderer_Clear(0, viewHeight - viewHeight / 4, 512, viewHeight / 4, 255, 255, 255);
		NativeRenderer_EndScene();
		u16 palette[16];
		for (int i = 0; i < 16; i++)
		{
			palette[i] = (u16)(i * 2 | (i * 2) << 5 | (i * 2) << 10);
		}
		assert(NativeRenderer_CapturePauseBackground(palette, 0));
		const struct NativeRenderTarget *backdrop = &s_pauseBackgroundTarget;
		u8 pixel[4];
		glBindFramebuffer(GL_READ_FRAMEBUFFER, backdrop->framebuffer);
		// Backdrop row 0 is the top of the screen. The top view's first and
		// last rows came from red (green 0), not from the white outer rows.
		const int rows[] = {1, backdrop->height * viewHeight / 216 - 2, backdrop->height * viewHeight / 432};
		for (int i = 0; i < 3; i++)
		{
			glReadPixels(backdrop->width / 2, rows[i], 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
			if (pixel[1] > 20)
			{
				fprintf(stderr, "pause backdrop row %d kept an outer row: %u,%u,%u\n", rows[i], pixel[0], pixel[1], pixel[2]);
				abort();
			}
		}
		NativeRenderer_BindMainRenderTarget();
		assert(glGetError() == GL_NO_ERROR);
	}

	NativeAspect_SetDualView(0, 0);
	NativeRenderer_CloseSecondWindow();
	assert(g_secondWindow == NULL);
	assert(s_secondPresentViewport.w == 0);
	NativeAspect_SetDualView(viewHeight, 216);
	DualTest_DrawSplitFrame(viewHeight);
	// Without a second window the renderer never presents a dual view.
	assert(!NativeRenderer_IsDualViewFrame());
	NativeAspect_SetDualView(0, 0);
	SDL_HideWindow(g_window);
}
