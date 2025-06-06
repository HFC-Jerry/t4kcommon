/*
   t4k_loaders.c

   Functions responsible for loading media files.

   Copyright 2000, 2003, 2006, 2009, 2010.
Authors: Sam Hart, Jesse Andrews, David Bruce, Boleslaw Kulbabinski,
Wenyuan Guo, Brendan Luchen.
Project email: <tuxmath-devel@lists.sourceforge.net>
Project website: http://tux4kids.alioth.debian.org

t4k_loaders.c is part of the t4k_common library.

t4k_common is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

t4k_common is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#include "t4k_globals.h"
#include "t4k_compiler.h"
#include "t4k_common.h"
#include <errno.h>

#ifdef HAVE_LIBPNG
#include <dirent.h>
#include <sys/stat.h>
#include <png.h>
static int do_png_save(FILE * fi, const char *const fname, SDL_Surface * surf);
static void savePNG(SDL_Surface* surf,char* fn); //TODO this could be part of the API
static Uint32 get_pixel(SDL_Surface* surf, int x, int y);
#endif

#ifdef HAVE_RSVG
#include<librsvg/rsvg.h>
//## #include<librsvg/rsvg-cairo.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#endif

#define SOUNDS_DIR "sounds"
#define IMAGE_DIR "images"

/* local functions */

#ifdef HAVE_RSVG
int             get_number_of_frames_from_svg(const char *file_name);
SDL_Surface*    load_svg(const char* file_name, int width, int height, const char* layer_name);
sprite*         load_svg_sprite(const char* file_name, int width, int height);
SDL_Surface*    render_svg_from_handle(RsvgHandle* file_handle, int width, int height, const char* layer_name);
void            get_svg_dimensions(const char* file_name, int* width, int* height);
#endif

SDL_Surface*    load_image(const char* file_name, int mode, int w, int h, bool proportional);
void            fit_in_rectangle(int* width, int* height, int max_width, int max_height);
SDL_Surface*    set_format(SDL_Surface* img, int mode);
sprite*         load_sprite(const char* name, int mode, int w, int h, bool proportional);


static void savePNG(SDL_Surface* surf, char* fn);


#if HAVE_RSVG
/* structures related to svg info caching */
typedef struct
{
    char fn[T4K_PATH_MAX];
    int width;
    int height;
} svginfo;

#define SVGINFO_MAX 1000
int saveSVGInfo(const char* fn,int w,int h);
int SVGInfoIndex(const char* fn);

svginfo svg_info[SVGINFO_MAX];
int numSVG=0;
#endif //HAVE_RSVG

/* structures related to sdl surface caching */
typedef struct
{
    char fn[T4K_PATH_MAX];
    SDL_Surface* surf;
} cachedSurface;

#define CACHEDSURFACE_MAX 1000
int cacheSurface(const char* fn,SDL_Surface* surf);
int getCachedSurface(const char* fn);
SDL_Surface *IMG_Load_Cache(const char* fn);

cachedSurface cached_surface[CACHEDSURFACE_MAX];
int numSurfaces=0;



//directories to search in for loaded files, in addition to common data dir (just one for now)
static char app_prefix_path[1][T4K_PATH_MAX];

/* Remove trailing slash--STOLEN from tuxpaint */
char *T4K_RemoveSlash(char *path)
{
    if (!path)
    return NULL;
    
    int len = strlen(path);

    if (!len)
	return path;

    if (path[len-1] == '/' || path[len-1] == '\\')
	path[len-1] = '\0';

    return path;
}

int T4K_CheckFile(const char* file)
{
    FILE* fp = NULL;

    if (!file)
    {
	DEBUGMSG(debug_loaders, "T4K_CheckFile(): invalid char* argument!\n");
	return 0;
    }

    DEBUGMSG(debug_loaders, "T4K_CheckFile(): checking: %s\n", file);

    fp = fopen(file, "r");
    if (fp)
    {
	DEBUGMSG(debug_loaders, "T4K_CheckFile(): Opened successfully as FILE\n");
	fclose(fp);
	return 1;
    }
    
    DEBUGMSG(debug_loaders, "fopen(): %s\n", strerror(errno));
    DEBUGMSG(debug_loaders, "T4K_CheckFile(): Unable to open '%s' as either FILE or DIR\n", file);
    return 0;
}

void T4K_AddDataPrefix(const char* path)
{
    strncpy(app_prefix_path[0], path, T4K_PATH_MAX);
}

/* Look for a file as an absolute path, then in
   potential install directories */
/*##const char* find_file(const char* base_name)
{
    static char tmp_path[T4K_PATH_MAX];
    if (T4K_CheckFile(base_name))
	return base_name;
    snprintf(tmp_path, T4K_PATH_MAX, "%s/%s", app_prefix_path[0], base_name);
    if (T4K_CheckFile(tmp_path))
	return tmp_path;
    snprintf(tmp_path, T4K_PATH_MAX, "%s/%s", COMMON_DATA_PREFIX, base_name);
    if (T4K_CheckFile(tmp_path))
	return tmp_path;
    return "";	
}		*/

const char* find_file(const char* base_name)
{
    static char tmp_path[T4K_PATH_MAX];
    if (!base_name) {
        DEBUGMSG(debug_loaders, "find_file(): base_name is NULL\n");
        return "";
    }

    // Check if the base_name exists as-is
    if (T4K_CheckFile(base_name))
        return base_name;

    size_t base_len = strlen(base_name);

    // Try app_prefix_path[0]
    size_t prefix_len = strlen(app_prefix_path[0]);
    size_t required_len = prefix_len + 1 + base_len + 1; // +1 for '/', +1 for null terminator
    if (required_len > T4K_PATH_MAX) {
        DEBUGMSG(debug_loaders, "find_file(): Path too long for %s/%s\n", app_prefix_path[0], base_name);
    } else {
        if (snprintf(tmp_path, T4K_PATH_MAX, "%s/%s", app_prefix_path[0], base_name) >= T4K_PATH_MAX) {
            DEBUGMSG(debug_loaders, "find_file(): Path too long for %s/%s\n", app_prefix_path[0], base_name);
            return "";
        }
        if (T4K_CheckFile(tmp_path))
            return tmp_path;
    }

    // Try COMMON_DATA_PREFIX
    prefix_len = strlen(COMMON_DATA_PREFIX);
    required_len = prefix_len + 1 + base_len + 1;
    if (required_len > T4K_PATH_MAX) {
        DEBUGMSG(debug_loaders, "find_file(): Path too long for %s/%s\n", COMMON_DATA_PREFIX, base_name);
    } else {
        snprintf(tmp_path, T4K_PATH_MAX, "%s/%s", COMMON_DATA_PREFIX, base_name);
        if (T4K_CheckFile(tmp_path))
            return tmp_path;
    }

    return "";
}
#ifdef HAVE_RSVG

int get_number_of_frames_from_svg(const char* file_name) {
    xmlDocPtr svgFile;
    xmlNodePtr svgNode = NULL, nodeIterator = NULL;
    int number_of_frames = 0, found = 0;

    svgFile = xmlReadFile(file_name, NULL, XML_PARSE_RECOVER | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);

    /* If it's null something's really wrong because we're trying to load a sprite that doesn't exist */
    if(svgFile == NULL) {
        DEBUGMSG(debug_loaders, "get_number_of_frames_from_svg: couldn't load svgFile: %s\n", file_name);
        return 0;
    }

    svgNode = xmlDocGetRootElement(svgFile);

    /* If it's null then something's really wrong because there should be a root in every svg file... */
    if(svgNode == NULL) {
        DEBUGMSG(debug_loaders, "get_number_of_frames_from_svg: couldn't load the root from the svgFile: %s", file_name);
        xmlFreeDoc(svgFile); /* be clean */
        return 0;
    }

    nodeIterator = svgNode->children;
    while(nodeIterator) {
        if(xmlStrcasecmp(nodeIterator->name, (const xmlChar*)"desc") == 0) {
            sscanf((const char*)xmlNodeGetContent(nodeIterator), "%d", &number_of_frames);
            xmlFreeDoc(svgFile);
            return number_of_frames;
        }
        nodeIterator = nodeIterator->next;
    }

    /* if we get here we had no description, which means something's really wrong */
    DEBUGMSG(debug_loaders, "get_number_of_frames_from_svg: couldn't find the description frame number count from svgFile: %s", file_name);
    xmlFreeDoc(svgFile);
    return 0;
}

/*##
 Load a layer of SVG file and resize it to given dimensions.
   If width or height is negative no resizing is applied.
   If layer = NULL then the whole image is loaded.
   layer_name must be preceded with a '#' symbol.
   Return NULL on failure.
   (partly based on TuxPaint's SVG loading function) 
SDL_Surface* load_svg(const char* file_name, int width, int height, const char* layer_name)
{
 //##   SDL_Surface* dest;
    RsvgHandle* file_handle;
	sprite* new_sprite;
    char lay_name[20];
    int i;

//##    DEBUGMSG(debug_loaders, "load_svg(): loading %s\n", file_name);
	DEBUGMSG(debug_loaders, "load_svg_sprite(): loading sprite from %s, width = %d, height = %d\n", file_name, width, height);

//##    rsvg_init();

    file_handle = rsvg_handle_new_from_file(file_name, NULL);
    if(NULL == file_handle)
    {
		DEBUGMSG(debug_loaders, "load_svg(): file %s not found\n", file_name);
//##	rsvg_term();
		return NULL;
    }

//##    dest = render_svg_from_handle(file_handle, width, height, layer_name);

//##    g_object_unref(file_handle);
//##    rsvg_term();

//##    return dest;
	new_sprite = malloc(sizeof(sprite));
    if (new_sprite == NULL)
    {
        DEBUGMSG(debug_loaders, "malloc(): can't allocate memory for a new sprite\n");
        g_object_unref(file_handle);
        return NULL;
    }
    new_sprite->default_img = render_svg_from_handle(file_handle, width, height, "#default");

    new_sprite->num_frames = get_number_of_frames_from_svg(file_name);
    DEBUGMSG(debug_loaders, "load_svg_sprite(): loading %d frames\n", new_sprite->num_frames);

    for(i = 0; i < new_sprite->num_frames; i++)
    {
        snprintf(lay_name, sizeof(lay_name), "#frame%d", i);
        new_sprite->frame[i] = render_svg_from_handle(file_handle, width, height, lay_name);
    }

    g_object_unref(file_handle);
    return new_sprite;
}		*/
SDL_Surface* load_svg(const char* file_name, int width, int height, const char* layer_name)
{
    SDL_Surface* dest;
    RsvgHandle* file_handle;

    DEBUGMSG(debug_loaders, "load_svg(): loading %s\n", file_name);

    file_handle = rsvg_handle_new_from_file(file_name, NULL);
    if(NULL == file_handle)
    {
        DEBUGMSG(debug_loaders, "load_svg(): file %s not found\n", file_name);
        return NULL;
    }

    dest = render_svg_from_handle(file_handle, width, height, layer_name);

    g_object_unref(file_handle);
    return dest;
}
sprite* load_svg_sprite(const char* file_name, int width, int height)
{
    RsvgHandle* file_handle;
    sprite* new_sprite;
    char lay_name[20];
    int i;

    DEBUGMSG(debug_loaders, "load_svg_sprite(): loading sprite from %s, width = %d, height = %d\n", file_name, width, height);

//##   rsvg_init();

    file_handle = rsvg_handle_new_from_file(file_name, NULL);
    if(NULL == file_handle)
    {
	DEBUGMSG(debug_loaders, "load_svg_sprite(): file %s not found\n", file_name);
//##	rsvg_term();
	return NULL;
    }

    new_sprite = malloc(sizeof(sprite));
    if (new_sprite == NULL)
    {
        DEBUGMSG(debug_loaders, "malloc(): can't allocate memory for a new sprite\n");
//##        rsvg_term();
        return NULL;
    }
    new_sprite->default_img = render_svg_from_handle(file_handle, width, height, "#default");

    /* get number of frames from description */
    new_sprite->num_frames = get_number_of_frames_from_svg(file_name);
    DEBUGMSG(debug_loaders, "load_svg_sprite(): loading %d frames\n", new_sprite->num_frames);
/*##
    for(i = 0; i < new_sprite->num_frames; i++)
    {
	sprintf(lay_name, "#frame%d", i);
	new_sprite->frame[i] = render_svg_from_handle(file_handle, width, height, lay_name);
    }
	*/
	for(i = 0; i < new_sprite->num_frames; i++)
	{
		snprintf(lay_name, sizeof(lay_name), "#frame%d", i);
		new_sprite->frame[i] = render_svg_from_handle(file_handle, width, height, lay_name);
	}

    g_object_unref(file_handle);
//##    rsvg_term();

    return new_sprite;
}

/* render a layer of SVG file and resize it to given dimensions.
   If width or height is negative no resizing is applied. */
SDL_Surface* render_svg_from_handle(RsvgHandle* file_handle, int width, int height, const char* layer_name)
{
    RsvgDimensionData dimensions;
    cairo_surface_t* temp_surf;
    cairo_t* context;
    SDL_Surface* dest;
    float scale_x, scale_y;
    //##Uint32 Rmask, Gmask, Bmask, Amask;
    //##rsvg_handle_get_dimensions(file_handle, &dimensions);
	double intrinsic_width, intrinsic_height;
    if (!rsvg_handle_get_intrinsic_size_in_pixels(file_handle, &intrinsic_width, &intrinsic_height)) {
        DEBUGMSG(debug_loaders, "render_svg_from_handle(): Failed to get intrinsic size\n");
        return NULL;
    }
    /* set scale_x and scale_y */
  if(width < 0 || height < 0)
    {
        width = (int)intrinsic_width;
        height = (int)intrinsic_height;
        scale_x = 1.0;
        scale_y = 1.0;
    }
    else
    {
        scale_x = (float)width / intrinsic_width;
        scale_y = (float)height / intrinsic_height;
    }
    /*## set color masks 
    Rmask = T4K_GetScreen()->format->Rmask;
    Gmask = T4K_GetScreen()->format->Gmask;
    Bmask = T4K_GetScreen()->format->Bmask;
    if(T4K_GetScreen()->format->Amask == 0)
	 find a free byte to use for Amask 
	Amask = ~(Rmask | Gmask | Bmask);
    else
	Amask = T4K_GetScreen()->format->Amask;

    DEBUGMSG(debug_loaders, "render_svg_from_handle(): color masks: R=%u, G=%u, B=%u, A=%u\n",
	    Rmask, Gmask, Bmask, Amask);
*/
dest = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888);
    if (!dest)
    {
        DEBUGMSG(debug_loaders, "render_svg_from_handle(): SDL_CreateSurface failed: %s\n", SDL_GetError());
        return NULL;
    }

    SDL_LockSurface(dest);
    temp_surf = cairo_image_surface_create_for_data(dest->pixels,
            CAIRO_FORMAT_ARGB32, dest->w, dest->h, dest->pitch);

    context = cairo_create(temp_surf);
    if(cairo_status(context) != CAIRO_STATUS_SUCCESS)
    {
        DEBUGMSG(debug_loaders, "render_svg_from_handle(): error rendering SVG\n");
        SDL_DestroySurface(dest);
        cairo_surface_destroy(temp_surf);
        return NULL;
    }

    cairo_scale(context, scale_x, scale_y);
    if (!rsvg_handle_render_layer(file_handle, context, layer_name, NULL, NULL)) {
        DEBUGMSG(debug_loaders, "render_svg_from_handle(): rsvg_handle_render_layer failed\n");
        SDL_DestroySurface(dest);
        cairo_surface_destroy(temp_surf);
        cairo_destroy(context);
        return NULL;
    }
    SDL_UnlockSurface(dest);
    cairo_surface_destroy(temp_surf);
    cairo_destroy(context);

    return dest;
}

void get_svg_dimensions(const char* file_name, int* width, int* height)
{

    int index = SVGInfoIndex(file_name);
    RsvgHandle* file_handle;
//##RsvgDimensionData dimensions;
	double intrinsic_width, intrinsic_height;

    if (index != -1) //look for cached dimensions
    {
	*width = svg_info[index].width;
	*height = svg_info[index].height;
	return;
    }

    //FIXME do we really need to initialize and terminate RSVG every time?
//##    rsvg_init();

    file_handle = rsvg_handle_new_from_file(file_name, NULL);
    if(file_handle == NULL)
    {
	DEBUGMSG(debug_loaders, "get_svg_dimensions(): file %s not found\n", file_name);
//##	rsvg_term();
	return;
    }

/*##    rsvg_handle_get_dimensions(file_handle, &dimensions);

    *width = dimensions.width;
    *height = dimensions.height;	*/
if (!rsvg_handle_get_intrinsic_size_in_pixels(file_handle, &intrinsic_width, &intrinsic_height)) {
        DEBUGMSG(debug_loaders, "get_svg_dimensions(): Failed to get intrinsic size\n");
        g_object_unref(file_handle);
        return;
    }
	*width = (int)intrinsic_width;
    *height = (int)intrinsic_height;
    g_object_unref(file_handle);

    //FIXME see above
//##    rsvg_term();

    saveSVGInfo(file_name, *width, *height); //save dimensions for quick access
}

#endif /* HAVE_RSVG */


#ifdef BUILD_MINGW32
#include <windows.h>

/* STOLEN from tuxpaint */

/*
   Read access to Windows Registry
   */
static HRESULT ReadRegistry(const char *key, const char *option, char *value, int size)
{
    LONG        res;
    HKEY        hKey = NULL;

    res = RegOpenKeyEx(HKEY_CURRENT_USER, key, 0, KEY_READ, &hKey);
    if (res != ERROR_SUCCESS)
	goto err_exit;
    res = RegQueryValueEx(hKey, option, NULL, NULL, (LPBYTE)value, (LPDWORD)&size);
    if (res != ERROR_SUCCESS)
	goto err_exit;
    res = ERROR_SUCCESS;

err_exit:
    if (hKey) RegCloseKey(hKey);
    return HRESULT_FROM_WIN32(res);
}
#endif


/* STOLEN from Tuxmath */
void T4K_GetUserDataDir(char *opt_path, char* suffix)
{
    static char udd[T4K_PATH_MAX];
    static bool have_env = false;

    if (!have_env)
    {
#ifdef BUILD_MINGW32
	{
	    char          path[2*MAX_PATH];
	    const char   *key    = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders";
	    const char   *option = "AppData";
	    HRESULT hr = S_OK;

	    if (!SUCCEEDED(hr = ReadRegistry(key, option, udd, sizeof(udd))))
	    {
		perror("Error getting data dir");
		opt_path = NULL;
		return;
	    }
	}
#else
	snprintf(udd, T4K_PATH_MAX, "%s", getenv("HOME"));
#endif
	T4K_RemoveSlash(udd);
	have_env = true;
    }

    strncpy(opt_path, udd, T4K_PATH_MAX);

    if (suffix && *suffix)
    {
	strncat(opt_path, "/", T4K_PATH_MAX);
	strncat(opt_path, suffix, T4K_PATH_MAX);
    }
}



/* Load an image without resizing it */
SDL_Surface* T4K_LoadImage(const char* file_name, int mode)
{
    return T4K_LoadScaledImage(file_name, mode, -1, -1);
}

/* LoadScaledImage : Load an image and resize it to given dimensions.
   If width or height is negative no resizing is applied.
   The loader (load_svg() or IMG_Load()) is chosen depending on file extension,
   If an SVG file is not found try to load its PNG equivalent
   (unless IMG_NO_PNG_FALLBACK is set) */
SDL_Surface* T4K_LoadScaledImage(const char* file_name, int mode, int width, int height)
{
    return load_image(file_name, mode, width, height, false);
}

/* LoadImageOfBoundingBox : Same as LoadScaledImage but preserve image proportions
   and fit it into max_width x max_height rectangle.
   Returned surface is not necessarily max_width x max_height ! */
SDL_Surface* T4K_LoadImageOfBoundingBox(const char* file_name, int mode, int max_width, int max_height)
{
    return load_image(file_name, mode, max_width, max_height, true);
}


/* load_image : helper function used by LoadScaledImage and LoadImageOfBoundingBox */
SDL_Surface* load_image(const char* file_name, int mode, int w, int h, bool proportional)
{
    SDL_Surface* loaded_pic = NULL;
    SDL_Surface* final_pic = NULL;
    char fn[T4K_PATH_MAX];
    int fn_len;
    int width = -1, height = -1;
    bool is_svg = true;

    if(NULL == file_name)
    {
	DEBUGMSG(debug_loaders, "load_image(): file_name is NULL, exiting.\n");
	return NULL;
    }

    /* run loader depending on file extension */

    /* add path prefix */
    snprintf(fn, T4K_PATH_MAX, IMAGE_DIR "/%s", file_name);
    fn_len = strlen(fn);

    if(strcmp(fn + fn_len - 4, ".svg"))
    {
	DEBUGMSG(debug_loaders, "load_image(): %s is not an SVG, loading using IMG_Load()\n", fn);
	loaded_pic = IMG_Load_Cache(find_file(fn));
	is_svg = false;
	if (NULL == loaded_pic)
	{
	    is_svg = true;
	    DEBUGMSG(debug_loaders, "load_image(): Trying to load SVG equivalent of %s\n", fn);
	    sprintf(strrchr(fn, '.'), ".svg");
	}
    }
    if (is_svg)
    {
#ifdef HAVE_RSVG
	DEBUGMSG(debug_loaders, "load_image(): trying to load %s as SVG.\n", fn);
	if(proportional)
	{
	    get_svg_dimensions(find_file(fn), &width, &height);
	    if(width > 0 && height > 0)
		fit_in_rectangle(&width, &height, w, h);
	}
	else
	{
	    width = w;
	    height = h;
	}
	loaded_pic = load_svg(find_file(fn), width, height, NULL);
#endif

	if(loaded_pic == NULL)
	{
#ifdef HAVE_RSVG
	    DEBUGMSG(debug_loaders, "load_image(): failed to load %s as SVG.\n", fn);
#else
	    DEBUGMSG(debug_loaders, "load_image(): SVG support not available.\n");
#endif
	    if(mode & IMG_NO_PNG_FALLBACK)
	    {
		DEBUGMSG(debug_loaders, "load_image(): %s : IMG_NO_PNG_FALLBACK is set.\n", fn);
	    }
	    else
	    {
		DEBUGMSG(debug_loaders, "load_image(): Trying to load PNG equivalent of %s\n", fn);
		strcpy(fn + fn_len - 3, "png");

		loaded_pic = IMG_Load_Cache(find_file(fn));
		is_svg = false;
	    }
	}
    }

    if (NULL == loaded_pic) /* Could not load image: */
    {
	if (mode & IMG_NOT_REQUIRED)
	{
	    DEBUGMSG(debug_loaders, "load_image(): Warning: could not load optional graphics file %s\n", file_name);
	    return NULL;  /* Allow program to continue */
	}
	/* If image was required, exit from program: */
	fprintf(stderr, "load_image(): ERROR could not load required graphics file %s\n", file_name);
	fprintf(stderr, "SDL: %s\n", SDL_GetError() );
	//should do some cleanup first...
	exit(EXIT_FAILURE);
	return NULL;
    }

    //if we need to resize a loaded raster image
    if(!is_svg && w > 0 && h > 0)
    {
	if(proportional)
	{
	    width = loaded_pic->w;
	    height = loaded_pic->h;
	    fit_in_rectangle(&width, &height, w, h);
	}
	else
	{
	    width = w;
	    height = h;
	}
	final_pic = T4K_zoom(loaded_pic, width, height);
	SDL_DestroySurface(loaded_pic);
	loaded_pic = final_pic;
	final_pic = NULL;
    }

    final_pic = set_format(loaded_pic, mode);
    SDL_DestroySurface(loaded_pic);
    DEBUGMSG(debug_loaders, "Leaving load_image()\n\n");

    return final_pic;
}

/* adjust width and height to fit in max_width x max_height rectangle
   but preserve their proportion */
void fit_in_rectangle(int* width, int* height, int max_width, int max_height)
{
    float scale_w, scale_h;

    if(width != 0 && height != 0)
    {
	scale_w = (float) max_width / (*width);
	scale_h = (float) max_height / (*height);
	*width *= min(scale_w, scale_h);
	*height *= min(scale_w, scale_h);
    }
}
/*##
SDL_Surface* set_format(SDL_Surface* img, int mode)
{
    switch (mode & IMG_MODES)
    {
	case IMG_REGULAR:
	    {
		DEBUGMSG(debug_loaders, "set_format(): handling IMG_REGULAR mode.\n");
		return SDL_DisplayFormat(img);
	    }

	case IMG_ALPHA:
	    {
		DEBUGMSG(debug_loaders, "set_format(): handling IMG_ALPHA mode.\n");
		return SDL_DisplayFormatAlpha(img);
	    }

	case IMG_COLORKEY:
	    {
		DEBUGMSG(debug_loaders, "set_format(): handling IMG_COLORKEY mode.\n");
		SDL_LockSurface(img);
		SDL_SetColorKey(img, (SDL_SRCCOLORKEY | SDL_RLEACCEL),
			SDL_MapRGB(img->format, 255, 255, 0));
		return SDL_DisplayFormat(img);
	    }

	default:
	    {
		DEBUGMSG(debug_loaders, "set_format(): Image mode not recognized\n");
	    }
    }

    return NULL;
}
*/
SDL_Surface* set_format(SDL_Surface* img, int mode)
{
    if (!img) {
        DEBUGMSG(debug_loaders, "set_format(): Input surface is NULL\n");
        return NULL;
    }

    SDL_Surface* converted = NULL;
    SDL_Surface* screen = T4K_GetScreen();
    
    if (!screen) {
        DEBUGMSG(debug_loaders, "set_format(): Could not get screen surface\n");
        return NULL;
    }

    switch (mode & IMG_MODES) {
        case IMG_REGULAR:
            DEBUGMSG(debug_loaders, "set_format(): handling IMG_REGULAR mode.\n");
            converted = SDL_ConvertSurface(img, screen->format);
            if (!converted) {
                DEBUGMSG(debug_loaders, "set_format(IMG_REGULAR): %s\n", SDL_GetError());
                return NULL;
            }
            break;

        case IMG_ALPHA:
            DEBUGMSG(debug_loaders, "set_format(): handling IMG_ALPHA mode.\n");
            converted = SDL_ConvertSurface(img, SDL_PIXELFORMAT_ARGB8888);
            if (!converted) {
                DEBUGMSG(debug_loaders, "set_format(IMG_ALPHA): %s\n", SDL_GetError());
                return NULL;
            }
            break;

        case IMG_COLORKEY:
            DEBUGMSG(debug_loaders, "set_format(): handling IMG_COLORKEY mode.\n");
            SDL_LockSurface(img);
            
            // Get format details and palette
            const SDL_PixelFormatDetails* format_details = SDL_GetPixelFormatDetails(img->format);
            SDL_Palette* palette = SDL_GetSurfacePalette(img);
            
            if (!format_details) {
                DEBUGMSG(debug_loaders, "set_format(): Failed to get pixel format details\n");
                SDL_UnlockSurface(img);
                return NULL;
            }
            
            // Map yellow as the color key
            Uint32 colorkey = SDL_MapRGB(format_details, palette, 255, 255, 0);
            if (SDL_SetSurfaceColorKey(img, true, colorkey) < 0) {
                DEBUGMSG(debug_loaders, "SDL_SetColorKey: %s\n", SDL_GetError());
                SDL_UnlockSurface(img);
                return NULL;
            }
            
            converted = SDL_ConvertSurface(img, screen->format);
            SDL_UnlockSurface(img);
            
            if (!converted) {
                DEBUGMSG(debug_loaders, "set_format(IMG_COLORKEY): %s\n", SDL_GetError());
                return NULL;
            }
            break;

        default:
            DEBUGMSG(debug_loaders, "set_format(): Image mode not recognized\n");
            return NULL;
    }

    return converted;
}/* T4K_LoadBkgd() : a wrapper for T4K_LoadImage() that optimizes
   the format of background image */
SDL_Surface* T4K_LoadBkgd(const char* file_name, int width, int height)
{
    SDL_Surface* orig = NULL;
    SDL_Surface* final_pic = NULL;

    orig = T4K_LoadScaledImage(file_name, IMG_REGULAR, width, height);

    if (!orig)
    {
	DEBUGMSG(debug_loaders, "In T4K_LoadBkgd(), T4K_LoadImage() returned NULL on %s\n",
		file_name);
	return NULL;
    }

    /* turn off transparency, since it's the background */
//##    SDL_SetAlpha(orig, SDL_RLEACCEL, SDL_ALPHA_OPAQUE);
//##    final_pic = SDL_DisplayFormat(orig); 
	final_pic = SDL_ConvertSurface(orig, T4K_GetScreen()->format);/* optimize the format */
    SDL_DestroySurface(orig);

    return final_pic;
}

/* T4K_LoadBothBkgds() : loads two scaled images: one for the fullscreen mode
   (fs_res_x,fs_rex_y) and one for the windowed mode (win_res_x,win_rex_y)
   Now we also optimize the format for best performance */
int T4K_LoadBothBkgds(const char* file_name, SDL_Surface** fs_bkgd, SDL_Surface** win_bkgd)
{
    int wx, wy, fx, fy;
    T4K_GetResolutions(&wx, &wy, &fx, &fy);

    if (!fs_bkgd || !win_bkgd)
    {
	fprintf(stderr, "T4K_LoadBothBkgds(): Invalid ptr arg");
	return 0;
    }

    *fs_bkgd = T4K_LoadBkgd(file_name, fx, fy);
    *win_bkgd = T4K_LoadBkgd(file_name, wx, wy);
    return 1;
}



sprite* T4K_LoadSprite(const char* name, int mode)
{
    return T4K_LoadScaledSprite(name, mode, -1, -1);
}

sprite* T4K_LoadScaledSprite(const char* name, int mode, int width, int height)
{
    return load_sprite(name, mode, width, height, false);
}

sprite* T4K_LoadSpriteOfBoundingBox(const char* name, int mode, int max_width, int max_height)
{
    return load_sprite(name, mode, max_width, max_height, true);
}

sprite* load_sprite(const char* name, int mode, int w, int h, bool proportional)
{
    sprite *new_sprite = NULL;
    int i;
    char fn[T4K_PATH_MAX];
#ifdef HAVE_RSVG
    char* imgfn = NULL;
    char cachepath[T4K_PATH_MAX];
    char pngfn[T4K_PATH_MAX];
    int width, height;
    bool shouldcache = false;

    T4K_GetUserDataDir(cachepath, ".t4k_common/caches");
    sprintf(fn, IMAGE_DIR "/%s.svg", name);
    imgfn = (char*)find_file(fn);
    if (imgfn)
    {
        if (proportional)
        {
            get_svg_dimensions(imgfn, &width, &height);
            if (width > 0 && height > 0)
                fit_in_rectangle(&width, &height, w, h);
        }
        else
        {
            width = w;
            height = h;
        }

        // Check default image path length
        int len = snprintf(NULL, 0, "%s/images/%sd-%d-%d.png", cachepath, name, width, height);
        if (len < 0 || len >= T4K_PATH_MAX) {
            DEBUGMSG(debug_loaders, "load_sprite(): PNG path too long for %s default image\n", name);
        } 
		else 
		{
            if (snprintf(pngfn, T4K_PATH_MAX, "%s/images/%sd-%d-%d.png", cachepath, name, width, height) >= T4K_PATH_MAX) {
                DEBUGMSG(debug_loaders, "load_sprite(): Path too long for default image\n");
                return NULL;
            }
            if (T4K_CheckFile(pngfn) == 1)
            {
                new_sprite = (sprite*)malloc(sizeof(sprite));
                if (!new_sprite) {
                    DEBUGMSG(debug_loaders, "load_sprite(): Memory allocation failed\n");
                    return NULL;
                }
                new_sprite->default_img = IMG_Load(pngfn);
                i = 0;
                while (1)
                {
                    len = snprintf(NULL, 0, "%s/images/%s%d-%d-%d.png", cachepath, name, i, width, height);
                    if (len < 0 || len >= T4K_PATH_MAX) {
                        DEBUGMSG(debug_loaders, "load_sprite(): PNG path too long for %s frame %d\n", name, i);
                        break;
                    }
                    if (snprintf(pngfn, T4K_PATH_MAX, "%s/images/%s%d-%d-%d.png", cachepath, name, i, width, height) >= T4K_PATH_MAX) {
                        DEBUGMSG(debug_loaders, "load_sprite(): Path too long for frame %d\n", i);
                        break;
                    }
                    if (T4K_CheckFile(pngfn) == 1)
                    {
                        new_sprite->frame[i] = IMG_Load(pngfn);
                        if (!new_sprite->frame[i]) {
                            DEBUGMSG(debug_loaders, "load_sprite(): Failed to load frame %d for %s\n", i, name);
                            break;
                        }
                        i++;
                    }
                    else
                        break;
                }
                new_sprite->num_frames = i;
            }
            else
            {
                new_sprite = load_svg_sprite(find_file(fn), width, height);
                shouldcache = true;
            }
        }

        if (new_sprite)
        {
            set_format(new_sprite->default_img, mode);
            for (i = 0; i < new_sprite->num_frames; i++)
                set_format(new_sprite->frame[i], mode);
            new_sprite->cur = 0;

            width = new_sprite->default_img->w;
            height = new_sprite->default_img->h;

            if (shouldcache)
            {
                len = snprintf(NULL, 0, "%s/images/%sd-%d-%d.png", cachepath, name, width, height);
                if (len < 0 || len >= T4K_PATH_MAX) {
                    DEBUGMSG(debug_loaders, "load_sprite(): Cannot save PNG, path too long for %s default image\n", name);
                } else {
                    if (snprintf(pngfn, T4K_PATH_MAX, "%s/images/%sd-%d-%d.png", cachepath, name, width, height) >= T4K_PATH_MAX) {
                        DEBUGMSG(debug_loaders, "load_sprite(): Cannot save PNG, path too long for default image\n");
                    } else if (T4K_CheckFile(pngfn) != 1) {
                        savePNG(new_sprite->default_img, pngfn);
                    }
                }
                for (i = 0; i < new_sprite->num_frames; i++)
                {
                    len = snprintf(NULL, 0, "%s/images/%s%d-%d-%d.png", cachepath, name, i, width, height);
                    if (len < 0 || len >= T4K_PATH_MAX) {
                        DEBUGMSG(debug_loaders, "load_sprite(): Cannot save PNG, path too long for %s frame %d\n", name, i);
                    } else {
                        if (snprintf(pngfn, T4K_PATH_MAX, "%s/images/%s%d-%d-%d.png", cachepath, name, i, width, height) >= T4K_PATH_MAX) {
                            DEBUGMSG(debug_loaders, "load_sprite(): Cannot save PNG, path too long for frame %d\n", i);
                        } else if (T4K_CheckFile(pngfn) != 1) {
                            savePNG(new_sprite->frame[i], pngfn);
                        }
                    }
                }
            }
        }
    }
#endif

    if (!new_sprite)
    {
        /* Fallback to loading PNG files frame by frame */
        new_sprite = (sprite*)malloc(sizeof(sprite));
        if (!new_sprite) {
            DEBUGMSG(debug_loaders, "load_sprite(): Memory allocation failed in fallback\n");
            return NULL;
        }

        sprintf(fn, "%sd.png", name); // Default image
        if (proportional)
            new_sprite->default_img = T4K_LoadImageOfBoundingBox(fn, mode | IMG_NOT_REQUIRED, w, h);
        else
            new_sprite->default_img = T4K_LoadScaledImage(fn, mode | IMG_NOT_REQUIRED, w, h);

        if (!new_sprite->default_img)
            DEBUGMSG(debug_loaders, "load_sprite(): Failed to load default image for %s\n", name);

        new_sprite->cur = 0;
        new_sprite->num_frames = 0;
        for (i = 0; i < MAX_SPRITE_FRAMES; i++)
        {
            sprintf(fn, "%s%d.png", name, i);
            if (proportional)
                new_sprite->frame[i] = T4K_LoadImageOfBoundingBox(fn, mode | IMG_NOT_REQUIRED, w, h);
            else
                new_sprite->frame[i] = T4K_LoadScaledImage(fn, mode | IMG_NOT_REQUIRED, w, h);

            if (new_sprite->frame[i] == NULL)
                break;
            else
            {
                DEBUGMSG(debug_loaders, "load_sprite(): Loaded frame %d of %s\n", i, name);
                new_sprite->num_frames = i + 1;
            }
        }
    }

    if (new_sprite->num_frames == 0)
    {
        DEBUGMSG(debug_loaders, "load_sprite(): Failed to load %s\n", name);
        free(new_sprite);
        return NULL;
    }

    return new_sprite;
}
sprite* T4K_FlipSprite(sprite* in, int X, int Y)
{
    sprite *out;
    
    if (in == NULL)
        return NULL;

    out = malloc(sizeof(sprite));
    if (in->default_img != NULL)
	out->default_img = T4K_Flip( in->default_img, X, Y );
    else
	out->default_img = NULL;
    for( out->num_frames=0; out->num_frames<in->num_frames; out->num_frames++ )
	out->frame[out->num_frames] = T4K_Flip( in->frame[out->num_frames], X, Y );
    out->cur = 0;
    return out;
}

void T4K_FreeSprite(sprite* gfx)
{
    int x;
    if (!gfx)
	return;

    DEBUGMSG(debug_loaders, "Freeing image at %p", gfx);
    for (x = 0; x < gfx->num_frames; x++)
    {
	DEBUGMSG(debug_loaders, ".");
	if (gfx->frame[x])
	{
	    SDL_DestroySurface(gfx->frame[x]);
	    gfx->frame[x] = NULL;
	}
    }

    if (gfx->default_img)
    {
	SDL_DestroySurface(gfx->default_img);
	gfx->default_img = NULL;
    }

    DEBUGMSG(debug_loaders, "T4K_FreeSprite() - done\n");
    free(gfx);
}

void T4K_NextFrame(sprite* s)
{
    if (s && s->num_frames)
	s->cur = (s->cur + 1) % s->num_frames;
}

#if HAVE_RSVG
/* save SVG info */
int saveSVGInfo(const char* fn,int w,int h)
{
    strcpy(svg_info[numSVG].fn,fn);
    svg_info[numSVG].width=w;
    svg_info[numSVG].height=h;

    numSVG++;
    numSVG %= SVGINFO_MAX;

    return numSVG-1;
}
/* get SVG info index */
int SVGInfoIndex(const char* fn)
{
    int i;
    for(i=0;i<numSVG;i++)
    {
	if(!strcmp(fn,svg_info[i].fn))
	{
	    return i;
	}
    }

    return -1;
}

#endif //HAVE_RSVG

/* save sdl surface */
int cacheSurface(const char* fn,SDL_Surface* surf)
{
    strcpy(cached_surface[numSurfaces].fn,fn);
    cached_surface[numSurfaces].surf=surf;
    surf->refcount++;

    numSurfaces++;
    return numSurfaces-1;
}
/* get sdl surface index */
int getCachedSurface(const char* fn)
{
    int i;
    for(i=0;i<numSurfaces;i++)
    {
	if(!strcmp(fn,cached_surface[i].fn))
	{
	    return i;
	}
    }

    return -1;
}
/* attempt to load cached sdl surface if possible, otherwise use IMG_Load and cache the returned surface */
SDL_Surface *IMG_Load_Cache(const char* fn)
{
    int index=getCachedSurface(fn);
    if(index!=-1)
    {
	cached_surface[index].surf->refcount++;
	return cached_surface[index].surf;
    }
    else
    {
	SDL_Surface* temp=IMG_Load(fn);
	if(temp==NULL) return NULL;
	cacheSurface(fn,temp);
	return temp;
    }
}


#if HAVE_LIBPNG
/* Get a pixel value from a surface using SDL3's pixel format */
static Uint32 get_pixel(SDL_Surface* surf, int x, int y)
{
    if (!surf) return 0;

    int bpp = SDL_BYTESPERPIXEL(surf->format);
    Uint8* pixel = (Uint8*)surf->pixels + y * surf->pitch + x * bpp;

    switch (bpp) {
        case 1:
            return *pixel;
        case 2:
            return *(Uint16*)pixel;
        case 3:
            return (pixel[0]) | (pixel[1] << 8) | (pixel[2] << 16);
        case 4:
            return *(Uint32*)pixel;
        default:
            return 0;
    }
}

//save a surface to file as a PNG.
void savePNG(SDL_Surface* surf, char* fn)
{
    if (!surf || !fn) {
        DEBUGMSG(debug_loaders, "savePNG(): Invalid arguments\n");
        return;
    }

    FILE* fi;
    DIR* dir_ptr;
    size_t i = 0;
    size_t len = strlen(fn);

    // Check for buffer overflow
    if (len == 0 || len >= T4K_PATH_MAX) {
        DEBUGMSG(debug_loaders, "savePNG(): Filename too long or empty\n");
        return;
    }

    char path_buf[T4K_PATH_MAX];
    strncpy(path_buf, fn, T4K_PATH_MAX - 1);
    path_buf[T4K_PATH_MAX - 1] = '\0';

    while (i < len) {
        if (path_buf[i] == '/') {
            // Temporarily null-terminate at this position
            char temp = path_buf[i + 1];
            path_buf[i + 1] = '\0';

            /* test if the directory already exists */
            dir_ptr = opendir(path_buf);
            if (dir_ptr) {
                closedir(dir_ptr);
            } else {
                /* create new directory */
                int status;

#ifndef BUILD_MINGW32
                status = mkdir(path_buf, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#else
                status = mkdir(path_buf);
#endif

                if (0 == status) {
                    DEBUGMSG(debug_loaders, "mkdir %s succeeded\n", path_buf);
                } else {
                    DEBUGMSG(debug_loaders, "mkdir %s failed: %s\n", path_buf, strerror(errno));
                    return;
                }
            }
            path_buf[i + 1] = temp;
        }
        i++;
    }

    fi = fopen(fn, "wb");
    if (!fi) {
        DEBUGMSG(debug_loaders, "Error: Couldn't write to file %s: %s\n", fn, strerror(errno));
        return;
    }

    if (!do_png_save(fi, fn, surf)) {
        DEBUGMSG(debug_loaders, "PNG Not saved!\n");
        fclose(fi);
        if (T4K_CheckFile(fn)) {
            if (remove(fn) != 0) {
                DEBUGMSG(debug_loaders, "Failed to remove incomplete PNG file: %s\n", strerror(errno));
            }
        }
    }
}

/* The following functions are taken from Tuxpaint with minor changes */

/* Actually save the PNG data to the file stream: */
static int do_png_save(FILE * fi, const char *const fname, SDL_Surface * surf)
{
    if (!surf || !fi || !fname) {
        fprintf(stderr, "do_png_save: Invalid arguments\n");
        return 0;
    }

    png_structp png_ptr;
    png_infop info_ptr;
    png_text text_ptr[4];
    unsigned char **png_rows;
    Uint8 r, g, b, a;
    int x, y, count;

    /* Get pixel format details and palette */
    const SDL_PixelFormatDetails* format_details = SDL_GetPixelFormatDetails(surf->format);
    SDL_Palette* palette = SDL_GetSurfacePalette(surf);
    
    if (!format_details) {
        fprintf(stderr, "do_png_save: Invalid surface format\n");
        return 0;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        fclose(fi);
        fprintf(stderr, "\nError: Couldn't create PNG write struct!\n%s\n\n", fname);
        return 0;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fclose(fi);
        png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
        fprintf(stderr, "\nError: Couldn't create PNG info struct!\n%s\n\n", fname);
        return 0;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        fclose(fi);
        png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
        fprintf(stderr, "\nError: PNG write error!\n%s\n\n", fname);
        return 0;
    }

    png_init_io(png_ptr, fi);
    png_set_IHDR(png_ptr, info_ptr, surf->w, surf->h, 8,
                 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_set_sRGB_gAMA_and_cHRM(png_ptr, info_ptr,
                               PNG_sRGB_INTENT_PERCEPTUAL);

    /* Set headers */
    count = 0;
    text_ptr[count].key = (png_charp) "Software";
    text_ptr[count].text = (png_charp) PACKAGE_STRING;
    text_ptr[count].compression = PNG_TEXT_COMPRESSION_NONE;
    count++;

    png_set_text(png_ptr, info_ptr, text_ptr, count);
    png_write_info(png_ptr, info_ptr);

    /* Save the picture: */
    png_rows = malloc(sizeof(char *) * surf->h);
    if (!png_rows) {
        fclose(fi);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fprintf(stderr, "\nError: Memory allocation failed!\n%s\n\n", fname);
        return 0;
    }

    SDL_LockSurface(surf);
    
    for (y = 0; y < surf->h; y++) {
        png_rows[y] = malloc(sizeof(char) * 4 * surf->w);
        if (!png_rows[y]) {
            fclose(fi);
            png_destroy_write_struct(&png_ptr, &info_ptr);
            for (int i = 0; i < y; i++) {
                free(png_rows[i]);
            }
            free(png_rows);
            SDL_UnlockSurface(surf);
            fprintf(stderr, "\nError: Memory allocation failed!\n%s\n\n", fname);
            return 0;
        }

        for (x = 0; x < surf->w; x++) {
            Uint32 pixel = get_pixel(surf, x, y);
            SDL_GetRGBA(pixel, format_details, palette, &r, &g, &b, &a);
            png_rows[y][x * 4 + 0] = r;
            png_rows[y][x * 4 + 1] = g;
            png_rows[y][x * 4 + 2] = b;
            png_rows[y][x * 4 + 3] = a;
        }
    }

    SDL_UnlockSurface(surf);
    
    png_write_image(png_ptr, png_rows);

    for (y = 0; y < surf->h; y++) {
        free(png_rows[y]);
    }
    free(png_rows);

    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fi);

    return 1;
}
#else
void savePNG(SDL_Surface* surf, char* fn)
{
    DEBUGMSG(debug_loaders, "PNG caching unavailable in this version\n");
}
#endif //HAVE_LIBPNG


/* LoadSound : Load a sound/music patch from a file. */
Mix_Chunk* T4K_LoadSound( char *datafile )
{
    Mix_Chunk* tempChunk = NULL;
    char fn[T4K_PATH_MAX];

    sprintf(fn, SOUNDS_DIR "/%s", datafile);
    tempChunk = Mix_LoadWAV(fn);
    if (!tempChunk)
    {
	fprintf(stderr, "T4K_LoadSound(): %s not found\n\n", fn);
    }
    return tempChunk;
}

/* LoadMusic : Load music from a datafile */
Mix_Music* T4K_LoadMusic(char *datafile )
{
    char tempfn[T4K_PATH_MAX];
    const char* fn = NULL;
    Mix_Music* tempMusic = NULL;

    sprintf(tempfn, SOUNDS_DIR "/%s", datafile);

    fn = find_file(tempfn);

    if (1 != T4K_CheckFile(fn))
    {
	fprintf(stderr, "T4K_LoadMusic(): Music '%s' not found\n\n", fn);
	return NULL;
    }

    tempMusic = Mix_LoadMUS(fn);

    if (!tempMusic)
    {
	fprintf(stderr, "T4K_LoadMusic(): %s not loaded successfully\n", fn);
	printf("Error was: %s\n\n", SDL_GetError());
    }
    return tempMusic;
}