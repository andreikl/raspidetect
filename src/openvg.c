#include "main.h"
#include "utils.h"
#include "openvg.h"

#include "VG/vgu.h"
#include "GLES/gl.h"

#define FLOAT_FROM_26_6(x) ( (VGfloat)x / 64.0f )

typedef struct {
    VGFont vg_font;
    FT_Face ft_face;
} openvg_font_t;

typedef struct openvg_font_cache_entry_t {
   struct openvg_font_cache_entry_t *next;
   openvg_font_t font;
   uint32_t ptsize; // size in points, 26.6
} openvg_font_cache_entry_t;

#define CHANGE_LAYER    (1<<0)
#define CHANGE_OPACITY  (1<<1)
#define CHANGE_DEST     (1<<2)
#define CHANGE_SRC      (1<<3)
#define CHANGE_MASK     (1<<4)
#define CHANGE_XFORM    (1<<5)

#define COORDS_COUNT_MAX 1024
#define SEGMENTS_COUNT_MAX 256
static VGuint openvg_segments_count;
static VGubyte openvg_segments[SEGMENTS_COUNT_MAX];
static VGuint openvg_coords_count = 0;
static VGfloat openvg_coords[COORDS_COUNT_MAX];

#define CHAR_COUNT_MAX 255
static VGuint openvg_glyph_indices[CHAR_COUNT_MAX];
static VGfloat openvg_adjustments_x[CHAR_COUNT_MAX];
static VGfloat openvg_adjustments_y[CHAR_COUNT_MAX];

static openvg_font_cache_entry_t *openvg_fonts = NULL;

static void convert_contour(const FT_Vector *points, const char *tags, short points_count) {
   int first_coords = openvg_coords_count;

   int first = 1;
   char last_tag = 0;
   int c = 0;

   for (; points_count != 0; ++points, ++tags, --points_count) {
      ++c;

      char tag = *tags;
      if (first) {
         assert(tag & 0x1);
         assert(c==1); c=0;
         openvg_segments[openvg_segments_count++] = VG_MOVE_TO;
         first = 0;
      } else if (tag & 0x1) {
         /* on curve */

         if (last_tag & 0x1) {
            /* last point was also on -- line */
            assert(c==1); c=0;
            openvg_segments[openvg_segments_count++] = VG_LINE_TO;
         } else {
            /* last point was off -- quad or cubic */
            if (last_tag & 0x2) {
               /* cubic */
               assert(c==3); c=0;
               openvg_segments[openvg_segments_count++] = VG_CUBIC_TO;
            } else {
               /* quad */
               assert(c==2); c=0;
               openvg_segments[openvg_segments_count++] = VG_QUAD_TO;
            }
         }
      } else {
         /* off curve */

         if (tag & 0x2) {
            /* cubic */

            assert((last_tag & 0x1) || (last_tag & 0x2)); /* last either on or off and cubic */
         } else {
            /* quad */

            if (!(last_tag & 0x1)) {
               /* last was also off curve */

               assert(!(last_tag & 0x2)); /* must be quad */

               /* add on point half-way between */
               assert(c==2); c=1;
               openvg_segments[openvg_segments_count++] = VG_QUAD_TO;
               VGfloat x = (openvg_coords[openvg_coords_count - 2] + FLOAT_FROM_26_6(points->x)) * 0.5f;
               VGfloat y = (openvg_coords[openvg_coords_count - 1] + FLOAT_FROM_26_6(points->y)) * 0.5f;
               openvg_coords[openvg_coords_count++] = x;
               openvg_coords[openvg_coords_count++] = y;
            }
         }
      }
      last_tag = tag;

      openvg_coords[openvg_coords_count++] = FLOAT_FROM_26_6(points->x);
      openvg_coords[openvg_coords_count++] = FLOAT_FROM_26_6(points->y);
   }

   if (last_tag & 0x1) {
      /* last point was also on -- line (implicit with close path) */
      assert(c==0);
   } else {
      ++c;

      /* last point was off -- quad or cubic */
      if (last_tag & 0x2) {
         /* cubic */
         assert(c==3); c=0;
         openvg_segments[openvg_segments_count++] = VG_CUBIC_TO;
      } else {
         /* quad */
         assert(c==2); c=0;
         openvg_segments[openvg_segments_count++] = VG_QUAD_TO;
      }

      openvg_coords[openvg_coords_count++] = openvg_coords[first_coords + 0];
      openvg_coords[openvg_coords_count++] = openvg_coords[first_coords + 1];
   }

   openvg_segments[openvg_segments_count++] = VG_CLOSE_PATH;
}

static void openvg_convert_outline(const FT_Vector *points,
                            const char *tags,
                            const short *contours,
                            short contours_count,
                            short points_count) {

   openvg_segments_count = 0;
   openvg_coords_count = 0;

   short last_contour = 0;
   for (; contours_count != 0; ++contours, --contours_count) {
      short contour = *contours + 1;
      convert_contour(points + last_contour, tags + last_contour, contour - last_contour);
      last_contour = contour;
   }

   assert(last_contour == points_count);
   assert(openvg_segments_count <= SEGMENTS_COUNT_MAX);
   assert(openvg_coords_count <= COORDS_COUNT_MAX);
}

static int openvg_font_convert_glyphs(openvg_font_t *font, unsigned int char_height) {
    int res;
    res = FT_Set_Char_Size(font->ft_face, 0, char_height, 0, 0);
    if (res) {
        fprintf(stderr, "Failed to set char size, size: %d, res: %d\n", char_height, res);
        return -1;
    }

    FT_UInt glyph_index;
    FT_ULong ch = FT_Get_First_Char(font->ft_face, &glyph_index);
    while (ch != 0) {
        res = FT_Load_Glyph(font->ft_face, glyph_index, FT_LOAD_DEFAULT);
        if (res) {
            fprintf(stderr, "Failed to load glyph, index: %d, res: %d\n", glyph_index, res);
            return -1;
        }

        VGPath vg_path = VG_INVALID_HANDLE;
        FT_Outline *outline = &font->ft_face->glyph->outline;
        if (outline->n_contours != 0) {
            vg_path = vgCreatePath( VG_PATH_FORMAT_STANDARD,
                                    VG_PATH_DATATYPE_F,
                                    1.0f,
                                    0.0f,
                                    0,
                                    0,
                                    VG_PATH_CAPABILITY_ALL);
            if (vg_path == VG_INVALID_HANDLE) {
                fprintf(stderr, "Failed to create path, res: 0x%x\n", vgGetError());
                return -1;
            }

            openvg_convert_outline(outline->points, outline->tags, outline->contours, outline->n_contours, outline->n_points);
            vgAppendPathData(vg_path, openvg_segments_count, openvg_segments, openvg_coords);
        }

        VGfloat origin[] = { 0.0f, 0.0f };
        VGfloat escapement[] = {
            FLOAT_FROM_26_6(font->ft_face->glyph->advance.x),
            FLOAT_FROM_26_6(font->ft_face->glyph->advance.y)
        };
        vgSetGlyphToPath(font->vg_font, glyph_index, vg_path, VG_FALSE, origin, escapement);

        if (vg_path != VG_INVALID_HANDLE) {
            vgDestroyPath(vg_path);
        }
        ch = FT_Get_Next_Char(font->ft_face, ch, &glyph_index);
    }

    return 0;
}

// Find a font in cache, or create a new entry in the cache.
static openvg_font_t *openvg_find_font(app_state_t *state, const char *text, uint32_t text_size) {
    int ptsize = text_size << 6; // freetype takes size in points, in 26.6 format.

    openvg_font_cache_entry_t *font = openvg_fonts;
    while(font) {
        if (font->ptsize == ptsize)
            return &font->font;

        font = font->next;
    }

    font = malloc(sizeof(*font));
    if (!font) {
        fprintf(stderr, "Failed to allocate memory font\n");
        return NULL;
    }

    font->ptsize = ptsize;
    font->font.ft_face = NULL;
    font->font.vg_font = vgCreateFont(0);
    if (font->font.vg_font == VG_INVALID_HANDLE)
    {
        fprintf(stderr, "Failed to create font\n");
        goto memory;
    }

    // load the font
    int res;
    res = FT_New_Memory_Face(state->openvg.font_lib,
        state->openvg.font_data,
        state->openvg.font_len,
        0,
        &font->font.ft_face);
    if (res) {
        fprintf(stderr, "Failed to load font from memory: %d, data: %p, len: %d\n",
            res,
            state->openvg.font_data,
            state->openvg.font_len);
        goto font;
    }

    res = openvg_font_convert_glyphs(&font->font, ptsize);
    if (res) {
        fprintf(stderr, "Could not convert font '%s' at size %d\n", FONT_NAME, ptsize);
        goto face;
    }

    font->next = openvg_fonts;
    openvg_fonts = font;

    return &font->font;

face:
    FT_Done_Face(font->font.ft_face);
font:
    vgDestroyFont(font->font.vg_font);
memory:
    free(font);

    return NULL;
}

// Draws the characters from text
static void openvg_draw_chars(openvg_font_t *font, const char *text, int char_count) {
    // Put in first character
    openvg_glyph_indices[0] = FT_Get_Char_Index(font->ft_face, text[0]);
    int prev_glyph_index = openvg_glyph_indices[0];

    // Calculate glyph_indices and adjustments
    int i;
    FT_Vector kern;
    for (i = 1; i != char_count; ++i) {
        int glyph_index = FT_Get_Char_Index(font->ft_face, text[i]);
        if (!glyph_index) 
            return;

        openvg_glyph_indices[i] = glyph_index;

        if (FT_Get_Kerning(font->ft_face, prev_glyph_index, glyph_index, FT_KERNING_DEFAULT, &kern))
            assert(0);

        openvg_adjustments_x[i - 1] = FLOAT_FROM_26_6(kern.x);
        openvg_adjustments_y[i - 1] = FLOAT_FROM_26_6(kern.y);

        prev_glyph_index = glyph_index;
    }


    openvg_adjustments_x[char_count - 1] = 0.0f;
    openvg_adjustments_y[char_count - 1] = 0.0f;

    vgDrawGlyphs(font->vg_font, char_count, openvg_glyph_indices, openvg_adjustments_x, openvg_adjustments_y, VG_FILL_PATH, VG_FALSE);
}

int dispmanx_init(app_state_t *state) {
    int res;
    res = graphics_get_display_size(0, &state->openvg.display_width, &state->openvg.display_height);
    if (res < 0) {
        fprintf(stderr, "ERROR: Failed to get display size: 0x%x\n", res);
        return -1;
    }

    VC_RECT_T src_rect, dst_rect;
    src_rect.x = src_rect.y = 0;
    src_rect.width = state->openvg.display_width << 16;
    src_rect.height = state->openvg.display_height << 16;

    dst_rect.x = dst_rect.y = 0;
    dst_rect.width = state->openvg.display_width;
    dst_rect.height = state->openvg.display_height;

    DISPMANX_DISPLAY_HANDLE_T dispmanx_display = vc_dispmanx_display_open(0);
    if (dispmanx_display == DISPMANX_NO_HANDLE) {
        fprintf(stderr, "ERROR: Could not open dispmanx display 0\n");
        return -1;
    }

    DISPMANX_UPDATE_HANDLE_T dispmanx_update = vc_dispmanx_update_start(0);
    if (dispmanx_update == DISPMANX_NO_HANDLE) {
        fprintf(stderr, "ERROR: Could not start update on screen 0\n");
        return -1;
    }

	DISPMANX_UPDATE_HANDLE_T dispmanx_element = vc_dispmanx_element_add(
        dispmanx_update,
        dispmanx_display,
		1, //layer
        &dst_rect,
        0, // src
		&src_rect,
        DISPMANX_PROTECTION_NONE,
        0, //alpha
        0, //clamp
        0); //transform
    if (dispmanx_element == DISPMANX_NO_HANDLE) {
        fprintf(stderr, "ERROR: Could not add native screen %dx%d\n", state->openvg.display_width, state->openvg.display_height);
        return -1;
    }

	state->openvg.u.native_window.element = dispmanx_element;
	state->openvg.u.native_window.width = state->openvg.display_width;
	state->openvg.u.native_window.height = state->openvg.display_height;
    res = vc_dispmanx_update_submit_sync(dispmanx_update);
    if (res) {
        fprintf(stderr, "ERROR: Could not submit sync for screen 0, res: %d\n", res);
        return -1;
    }

    return 0;
}

void dispmanx_destroy(app_state_t *state) {
    DISPMANX_UPDATE_HANDLE_T current_update = vc_dispmanx_update_start(0);
    if (current_update != 0) {
        int res = vc_dispmanx_element_remove(current_update, state->openvg.u.native_window.element);
        if (res) {
            fprintf(stderr, "ERROR: Could not remove dispmanx element 0x%x\n", res);
        }
        res = vc_dispmanx_update_submit_sync(current_update);
        if (res) {
            fprintf(stderr, "ERROR: Could not update dispmanx 0x%x\n", res);
        }
    }
}

int openvg_init(app_state_t *state) {
    EGLBoolean egl_res;

    state->openvg.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (state->openvg.display == EGL_NO_DISPLAY) {
        fprintf(stderr, "ERROR: Failed to get egl display: 0x%x\n", eglGetError());
        return -1;
    }

    egl_res = eglInitialize(state->openvg.display, &state->openvg.egl_maj, &state->openvg.egl_min);
    if (!egl_res)
    {
        fprintf(stderr, "ERROR: Failed to initilise egl display: 0x%x\n", eglGetError());
        return -1;
    }

    EGLint nconfigs;
    EGLConfig config;
    EGLint attribs[] = {
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 6,
        EGL_BLUE_SIZE, 5,
        EGL_ALPHA_SIZE, 0,
        EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE
    };

    egl_res = eglChooseConfig(state->openvg.display, attribs, &config, 1, &nconfigs);
    if (egl_res == EGL_FALSE || nconfigs == 0) {
        fprintf(stderr, "ERROR: GRAPHICS_RESOURCE_RGB888 isn't supported\n");
        return -1;
    }

	egl_res = eglBindAPI(EGL_OPENVG_API);
    if (egl_res == EGL_FALSE) {
        fprintf(stderr, "ERROR: Failed to configure egl frame buffer: 0x%x\n", eglGetError());
        return -1;
    }

    state->openvg.context = eglCreateContext(state->openvg.display, config, EGL_NO_CONTEXT, 0);
    if (!state->openvg.context) {
        fprintf(stderr, "ERROR: Failed to create OpenVg context: 0x%x\n", eglGetError());
        return -1;
    }

    int res;
    res = FT_Init_FreeType(&state->openvg.font_lib);
    if (res != 0) {
        fprintf(stderr, "ERROR: Failed to initialise FreeType library\n");
        return -1;
    }

    state->openvg.font_data = utils_read_file(FONT_PATH, &state->openvg.font_len);
    if (state->openvg.font_data == NULL || state->openvg.font_len == 0) {
        fprintf(stderr, "ERROR: Failed to load font. path: %s\n", FONT_PATH);
        return -1;
    }

	state->openvg.surface = eglCreateWindowSurface(state->openvg.display, config, &state->openvg.u.native_window, NULL);
	if (state->openvg.surface == EGL_NO_SURFACE) {
        fprintf(stderr, "ERROR: Failed to create egl surface: %d (res: EGL_NO_SURFACE)\n", glGetError());
        return -1;

    }

	// connect the context to the surface
	egl_res = eglMakeCurrent(state->openvg.display, state->openvg.surface, state->openvg.surface, state->openvg.context);
    if (egl_res == EGL_FALSE) {
        fprintf(stderr, "ERROR: Failed to connect to surface (res: EGL_FALSE)\n");
        return -1;
    }

    state->openvg.video_buffer.c = malloc((state->video_width * state->video_height) << 1);
    if (!state->openvg.video_buffer.c) {
        fprintf(stderr, "ERROR: Failed to allocate memory for video buffer\n");
        return -1;
    }

    eglSwapInterval(state->openvg.display, 1);
    egl_res = eglSurfaceAttrib(state->openvg.display, state->openvg.surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED);
    if (!egl_res) {
        fprintf(stderr, "ERROR: Failed to set surface attribute: 0x%x\n", eglGetError());
        return -1;
    }

    if (state->verbose) {
        fprintf(stderr, "INFO: Supported EGL APIS: %s\n", eglQueryString(state->openvg.display, EGL_CLIENT_APIS));
        fprintf(stderr, "INFO: EGL API %d.%d\n", state->openvg.egl_maj, state->openvg.egl_min);
    }
   
    return 0;
}

void openvg_destroy(app_state_t *state) {
    if (state->openvg.video_buffer.c) {
        free(state->openvg.video_buffer.c);
    }
   
    if (state->openvg.context != NULL) {
        eglDestroyContext(state->openvg.display, state->openvg.context);
    }

    if (state->openvg.display != NULL) {
        eglTerminate(state->openvg.display);
    }

    if (state->openvg.font_data != NULL) {
        free(state->openvg.font_data);
    }

    while (openvg_fonts != NULL) {
        struct openvg_font_cache_entry_t *next = openvg_fonts->next;

        if (openvg_fonts->font.ft_face)
            FT_Done_Face(openvg_fonts->font.ft_face);

        if (openvg_fonts->font.vg_font)
            vgDestroyFont(openvg_fonts->font.vg_font);

        free(openvg_fonts);
        openvg_fonts = next;
    }

    FT_Done_FreeType(state->openvg.font_lib);
}

// Render text.
int openvg_draw_text(   app_state_t *state,
                        float x,
                        float y,
                        const char *text,
                        uint32_t text_length,
                        uint32_t text_size,
                        VGfloat colour[4]) {

    openvg_font_t *font = openvg_find_font(state, text, text_size);
    if (!font)
        return -1;

    VGPaint fg = vgCreatePaint();
    if (!fg)
        return -1;

    vgSetParameteri(fg, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
    vgSetParameterfv(fg, VG_PAINT_COLOR, 4, colour);
    vgSetPaint(fg, VG_FILL_PATH);

    //transform y from normal coordinates
    y = state->openvg.display_height - y - FLOAT_FROM_26_6(font->ft_face->size->metrics.height);

    y -= FLOAT_FROM_26_6(font->ft_face->size->metrics.descender);

    int i = 0;
    int last_draw = 0;
    while (1) {
        int last = (i == text_length) || !text[i];
        if ((text[i] == '\n') || last) {
            // Set origin to requested x,y
            VGfloat glor[] = { x, y };
            vgSetfv(VG_GLYPH_ORIGIN, 2, glor);

            openvg_draw_chars(font, text + last_draw, i - last_draw);

            last_draw = i + 1;
            y -= FLOAT_FROM_26_6(font->ft_face->size->metrics.height);
        }
        if (last) 
            break;
        ++i;
    };

    vgDestroyPaint(fg);

    int res = vgGetError();
    if (res != 0) {
        fprintf(stderr, "ERROR: Failed to draw text: 0x%x\n", res);
        return -1;
    }

    return 0;
}

int openvg_draw_boxes(app_state_t *state, VGfloat colour[4]) {
    VGPath path = vgCreatePath(VG_PATH_FORMAT_STANDARD,
        VG_PATH_DATATYPE_F,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        VG_PATH_CAPABILITY_ALL);

    VGPaint paint = vgCreatePaint();
    vgSetParameteri(paint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
    vgSetParameterfv(paint, VG_PAINT_COLOR, 4, colour);
    vgSetPaint(paint, VG_STROKE_PATH);
    vgSetf(VG_STROKE_LINE_WIDTH, 2);
    //vgSeti(VG_STROKE_CAP_STYLE, VG_CAP_BUTT);
    //vgSeti(VG_STROKE_JOIN_STYLE, VG_JOIN_MITER);

    int n = 0;
    for (int i = 0; i < state->worker_total_objects; i++) {
        if(state->worker_scores[i] > THRESHOLD) {
            n++;
            float *box = &state->worker_boxes[i * 4];
            int x1 = box[0] * state->video_width;
            int y1 = state->video_height - box[0] * state->video_height;
            int x2 = box[2] * state->video_width;
            int y2 = state->video_height - box[3] * state->video_height;
            vguRect(path, x1, y1, x2 - x1, y1 - y2);
        }
    }
    state->worker_objects = n;

    vgDrawPath(path, VG_STROKE_PATH);

    vgDestroyPaint(paint);
    vgDestroyPath(path);

    int res = vgGetError();
    if (res != 0) {
        fprintf(stderr, "ERROR: Failed to draw boxes: 0x%x\n", res);
        return -1;
    }

    return 0;
}

#define RB_555_MASK      (0b00000000000111110000000000011111)
#define G_555_MASK       (0b01111111111000000111111111100000)

int openvg_read_buffer(app_state_t *state) {
    vgReadPixels(state->openvg.video_buffer.c,
                state->video_width << 1,
                VG_sRGB_565,
                0, 0,
                state->video_width, state->video_height);

#if defined(ENV32BIT)
    int i = state->video_height >> 1;
    int w = state->video_width >> 1, wb = state->video_width << 1;
    int *start_ptr = state->openvg.video_buffer.i, *end_ptr = &state->openvg.video_buffer.i[w * (state->video_height - 1)];
    int* t = malloc(wb);
    while (i) {
        memcpy(t, start_ptr, wb);
        memcpy(start_ptr, end_ptr, wb);
        memcpy(end_ptr, t, wb);
        start_ptr += w;
        end_ptr -= w;
        i--;
    }
    free(t);

    start_ptr = state->openvg.video_buffer.i; end_ptr = &state->openvg.video_buffer.i[(state->video_height * w) - 1];
    int v;
    while (end_ptr != start_ptr) {
        v = *start_ptr;
        *start_ptr = ((v >> 1) & G_555_MASK) | (v & RB_555_MASK);
        start_ptr++;
    }

    return 0;
#else
    fprintf(stderr, "ERROR: 64 bits aren't supported\n");
    return -1;
#endif
}