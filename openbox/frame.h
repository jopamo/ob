#ifndef __frame_h
#define __frame_h

#include "geom.h"
#include "obrender/render.h"

typedef struct _ObFrame ObFrame;

struct _ObClient;

typedef void ( *ObFrameIconifyAnimateFunc )( gpointer data );

/*!
 * \brief Different contexts for parts of the frame (titlebar, corners, etc.).
 */
typedef enum {
  OB_FRAME_CONTEXT_NONE,
  OB_FRAME_CONTEXT_DESKTOP,
  OB_FRAME_CONTEXT_ROOT,
  OB_FRAME_CONTEXT_CLIENT,
  OB_FRAME_CONTEXT_TITLEBAR,
  OB_FRAME_CONTEXT_FRAME,
  OB_FRAME_CONTEXT_BLCORNER,
  OB_FRAME_CONTEXT_BRCORNER,
  OB_FRAME_CONTEXT_TLCORNER,
  OB_FRAME_CONTEXT_TRCORNER,
  OB_FRAME_CONTEXT_TOP,
  OB_FRAME_CONTEXT_BOTTOM,
  OB_FRAME_CONTEXT_LEFT,
  OB_FRAME_CONTEXT_RIGHT,
  OB_FRAME_CONTEXT_MAXIMIZE,
  OB_FRAME_CONTEXT_ALLDESKTOPS,
  OB_FRAME_CONTEXT_SHADE,
  OB_FRAME_CONTEXT_ICONIFY,
  OB_FRAME_CONTEXT_ICON,
  OB_FRAME_CONTEXT_CLOSE,
  /*! Special context for while dragging/resizing a window */
  OB_FRAME_CONTEXT_MOVE_RESIZE,
  OB_FRAME_CONTEXT_DOCK,
  OB_FRAME_NUM_CONTEXTS
} ObFrameContext;

/*! Helper macros for detecting frame/client contexts */
#define FRAME_CONTEXT( co, cl ) ( ( cl && cl->type != OB_CLIENT_TYPE_DESKTOP ) ? co == OB_FRAME_CONTEXT_FRAME : FALSE )
#define CLIENT_CONTEXT( co, cl ) \
  ( ( cl && cl->type == OB_CLIENT_TYPE_DESKTOP ) ? co == OB_FRAME_CONTEXT_DESKTOP : co == OB_FRAME_CONTEXT_CLIENT )

/*!
 * \brief Decorations bitmask that indicates which UI elements a window wants.
 */
typedef enum {
  OB_FRAME_DECOR_TITLEBAR = 1 << 0, /*!< Display a titlebar */
  OB_FRAME_DECOR_HANDLE   = 1 << 1, /*!< Display a handle (bottom) */
  OB_FRAME_DECOR_GRIPS    = 1 << 2, /*!< Display grips in the handle */
  OB_FRAME_DECOR_BORDER   = 1 << 3, /*!< Display a border */
  OB_FRAME_DECOR_ICON     = 1 << 4, /*!< Display the window's icon */
  OB_FRAME_DECOR_ICONIFY  = 1 << 5, /*!< Display an iconify button */
  OB_FRAME_DECOR_MAXIMIZE = 1 << 6, /*!< Display a maximize button */
  /*! Button to toggle "on all desktops" mode */
  OB_FRAME_DECOR_ALLDESKTOPS = 1 << 7,
  OB_FRAME_DECOR_SHADE       = 1 << 8, /*!< Display a shade button */
  OB_FRAME_DECOR_CLOSE       = 1 << 9  /*!< Display a close button */
} ObFrameDecorations;

/*!
 * \brief The main frame structure that wraps a client window.
 */
struct _ObFrame {
  struct _ObClient *client; /*!< The client window we are framing */

  Window window; /*!< The top-level X window for the frame */

  Strut size;       /*!< The current sizes of each frame edge */
  Strut oldsize;    /*!< The last published sizes to the client */
  Rect area;        /*!< The absolute geometry of the frame window */
  gboolean visible; /*!< TRUE if this frame is currently mapped */

  guint functions;   /*!< Window management functions bitmask */
  guint decorations; /*!< Which decorations are shown (bitmask) */

  /* Windows used for decorations and border areas */
  Window title;
  Window label;
  Window max;
  Window close;
  Window desk;
  Window shade;
  Window icon;
  Window iconify;
  Window handle;
  Window lgrip;
  Window rgrip;

  /* Borders and sub-windows for corners, edges, etc. */
  Window titleleft;
  Window titletop;
  Window titletopleft;
  Window titletopright;
  Window titleright;
  Window titlebottom;
  Window left;
  Window right;
  Window handleleft;
  Window handletop;
  Window handleright;
  Window handlebottom;
  Window lgriptop;
  Window lgripleft;
  Window lgripbottom;
  Window rgriptop;
  Window rgripright;
  Window rgripbottom;
  Window innerleft; /*!< For drawing an inner client border */
  Window innertop;
  Window innerright;
  Window innerbottom;
  Window innerblb;
  Window innerbll;
  Window innerbrb;
  Window innerbrr;
  Window backback;  /*!< Colored window shown while resizing */
  Window backfront; /*!< Prevents flashing on unmap */

  /* Resize handles inside the titlebar */
  Window topresize;
  Window tltresize;
  Window tllresize;
  Window trtresize;
  Window trrresize;

  Colormap colormap; /*!< Possibly from a 32-bit visual if needed */

  /* Booleans indicating which titlebar buttons are visible */
  gint icon_on;
  gint label_on;
  gint iconify_on;
  gint desk_on;
  gint shade_on;
  gint max_on;
  gint close_on;

  gint width;        /*!< Overall width of the titlebar+handle area */
  gint label_width;  /*!< Computed width of the window label */
  gint icon_x;       /*!< X-position of the icon button */
  gint label_x;      /*!< X-position of the label */
  gint iconify_x;    /*!< X-position of the iconify button */
  gint desk_x;       /*!< X-position of the all-desktops button */
  gint shade_x;      /*!< X-position of the shade button */
  gint max_x;        /*!< X-position of the maximize button */
  gint close_x;      /*!< X-position of the close button */
  gint bwidth;       /*!< Border width */
  gint cbwidth_l;    /*!< Client border width, left */
  gint cbwidth_t;    /*!< Client border width, top */
  gint cbwidth_r;    /*!< Client border width, right */
  gint cbwidth_b;    /*!< Client border width, bottom */
  gboolean max_horz; /*!< Window maximized horizontally? */
  gboolean max_vert; /*!< Window maximized vertically? */
  gboolean shaded;   /*!< Window is shaded? */

  /*! The leftmost and rightmost clickable elements in the titlebar */
  ObFrameContext leftmost;
  ObFrameContext rightmost;

  /* Press/hover states for buttons */
  gboolean max_press;
  gboolean close_press;
  gboolean desk_press;
  gboolean shade_press;
  gboolean iconify_press;
  gboolean max_hover;
  gboolean close_hover;
  gboolean desk_hover;
  gboolean shade_hover;
  gboolean iconify_hover;

  gboolean focused;     /*!< Is the client/focus in this frame? */
  gboolean need_render; /*!< Need re-rendering after changes? */

  /*! Whether we're in a "flash" state (for focus alerting, etc.) */
  gboolean flashing;
  gboolean flash_on;
  /*! When the flash effect ends, in microseconds since epoch (g_get_real_time) */
  gint64 flash_end;
  /*! Timer ID for the flash g_timeout */
  guint flash_timer;

  /*!
   * \brief Is the frame currently in an animation for iconify or restore?
   *  0 means no animation; >0 means iconify; <0 means restore.
   */
  gint iconify_animation_going;
  /*! Timer ID for the iconify animation */
  guint iconify_animation_timer;
  /*!
   * \brief When the iconify/restore animation ends (microseconds since epoch).
   */
  gint64 iconify_animation_end;
};

/*!
 * \brief Creates a new frame for a given client.
 */
ObFrame *frame_new( struct _ObClient *c );

/*!
 * \brief Frees (destroys) the frame and all its windows.
 */
void frame_free( ObFrame *self );

void frame_show( ObFrame *self );
void frame_hide( ObFrame *self );
void frame_adjust_theme( ObFrame *self );
#ifdef SHAPE
void frame_adjust_shape_kind( ObFrame *self, int kind );
#endif
void frame_adjust_shape( ObFrame *self );
void frame_adjust_area( ObFrame *self, gboolean moved, gboolean resized, gboolean fake );
void frame_adjust_client_area( ObFrame *self );
void frame_adjust_state( ObFrame *self );
void frame_adjust_focus( ObFrame *self, gboolean hilite );
void frame_adjust_title( ObFrame *self );
void frame_adjust_icon( ObFrame *self );
void frame_grab_client( ObFrame *self );
void frame_release_client( ObFrame *self );

/*! Convert a string to a frame context, e.g. "Titlebar" => OB_FRAME_CONTEXT_TITLEBAR. */
ObFrameContext frame_context_from_string( const gchar *name );

/*!
 * \brief Parses one ObFrameContext from a space-delimited string.
 * \param names The string. The first recognized context is removed from it.
 * \param cx The parsed context, or OB_FRAME_CONTEXT_NONE if invalid.
 * \return TRUE if a context was read, FALSE if the input is empty.
 */
gboolean frame_next_context_from_string( gchar *names, ObFrameContext *cx );

/*!
 * \brief Determines which part of the frame a click belongs to (titlebar, handle, etc.).
 */
ObFrameContext frame_context( struct _ObClient *self, Window win, gint x, gint y );

/*!
 * \brief Applies gravity to a client's position so we know where to place the frame.
 */
void frame_client_gravity( ObFrame *self, gint *x, gint *y );

/*!
 * \brief Reverses gravity from the frame to find where to place the client window.
 */
void frame_frame_gravity( ObFrame *self, gint *x, gint *y );

/*!
 * \brief Convert a rectangle in client coords to frame coords.
 */
void frame_rect_to_frame( ObFrame *self, Rect *r );

/*!
 * \brief Convert a rectangle in frame coords to client coords.
 */
void frame_rect_to_client( ObFrame *self, Rect *r );

/*!
 * \brief Start flashing the frame (e.g., user alert). A timer stops it automatically.
 */
void frame_flash_start( ObFrame *self );

/*!
 * \brief Stop the flashing effect immediately.
 */
void frame_flash_stop( ObFrame *self );

/*!
 * \brief Start iconify animation. If iconifying=TRUE, it animates "zooming out" to an icon location;
 *        if iconifying=FALSE, it animates restoring from icon to full size.
 */
void frame_begin_iconify_animation( ObFrame *self, gboolean iconifying );

/*!
 * \brief Called when the animation finishes or is canceled.
 */
void frame_end_iconify_animation( gpointer data );

/*! Check if a frame is currently in iconify animation. */
#define frame_iconify_animating( f ) ( ( f )->iconify_animation_going != 0 )

#endif
