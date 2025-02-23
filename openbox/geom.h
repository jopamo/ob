/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   geom.h for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
   Copyright (c) 2003-2007   Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#ifndef __geom_h
#define __geom_h

#include <glib.h>

/*!
 * \brief A structure describing a gravity-based coordinate.
 *
 * \c pos      => The base position (in pixels).
 * \c denom    => A fractional denominator for advanced usage (often unused).
 * \c center   => Whether to center around \c pos.
 * \c opposite => If \c TRUE, interpret \c pos as the right/bottom side, etc.
 */
typedef struct _GravityCoord {
  gint pos;
  gint denom;
  gboolean center;
  gboolean opposite;
} GravityCoord;

/*!
 * \brief A pair of GravityCoords representing an (x,y) point with optional center/opposite logic.
 */
typedef struct _GravityPoint {
  GravityCoord x;
  GravityCoord y;
} GravityPoint;

/*!
 * \brief Helper macro to set a GravityCoord's fields in one line.
 * \param c   The GravityCoord variable.
 * \param p   Pixel position.
 * \param cen Whether to center around \c p.
 * \param opp Whether \c p is an opposite edge coordinate.
 */
#define GRAVITY_COORD_SET( c, p, cen, opp ) ( c ).pos = ( p ), ( c ).center = ( cen ), ( c ).opposite = ( opp )

/*!
 * \brief A simple 2D point with integer x/y.
 */
typedef struct _Point {
  int x; /*!< The X coordinate. */
  int y; /*!< The Y coordinate. */
} Point;

/*!
 * \brief Sets a Point to the given coordinates.
 */
#define POINT_SET( pt, nx, ny ) ( pt ).x = ( nx ), ( pt ).y = ( ny )

/*!
 * \brief Checks if two Points have the same x/y.
 */
#define POINT_EQUAL( p1, p2 ) ( ( p1 ).x == ( p2 ).x && ( p1 ).y == ( p2 ).y )

/*!
 * \brief A width/height dimension pair.
 */
typedef struct _Size {
  int width;  /*!< Width in pixels */
  int height; /*!< Height in pixels */
} Size;

/*!
 * \brief Sets a Size.
 */
#define SIZE_SET( sz, w, h ) ( sz ).width = ( w ), ( sz ).height = ( h )

/*!
 * \brief A basic rectangle with x,y for the top-left and width/height.
 */
typedef struct _Rect {
  int x;      /*!< Left coordinate */
  int y;      /*!< Top coordinate  */
  int width;  /*!< Width in pixels */
  int height; /*!< Height in pixels */
} Rect;

/*!
 * \brief Returns the left boundary (X coordinate) of a Rect.
 */
#define RECT_LEFT( r ) ( ( r ).x )

/*!
 * \brief Returns the top boundary (Y coordinate) of a Rect.
 */
#define RECT_TOP( r ) ( ( r ).y )

/*!
 * \brief Returns the right boundary (X coordinate) of a Rect, inclusive.
 */
#define RECT_RIGHT( r ) ( ( r ).x + ( r ).width - 1 )

/*!
 * \brief Returns the bottom boundary (Y coordinate) of a Rect, inclusive.
 */
#define RECT_BOTTOM( r ) ( ( r ).y + ( r ).height - 1 )

/*!
 * \brief Returns the area (width * height) of a Rect.
 */
#define RECT_AREA( r ) ( ( r ).width * ( r ).height )

/*!
 * \brief Sets the top-left point of a Rect.
 */
#define RECT_SET_POINT( r, nx, ny ) ( r ).x = ( nx ), ( r ).y = ( ny )

/*!
 * \brief Sets the width/height of a Rect.
 */
#define RECT_SET_SIZE( r, w, h ) ( r ).width = ( w ), ( r ).height = ( h )

/*!
 * \brief Sets all fields of a Rect.
 */
#define RECT_SET( r, nx, ny, w, h ) ( r ).x = ( nx ), ( r ).y = ( ny ), ( r ).width = ( w ), ( r ).height = ( h )

/*!
 * \brief Checks if two Rects have identical x,y,width,height.
 */
#define RECT_EQUAL( r1, r2 ) \
  ( ( r1 ).x == ( r2 ).x && ( r1 ).y == ( r2 ).y && ( r1 ).width == ( r2 ).width && ( r1 ).height == ( r2 ).height )

/*!
 * \brief Checks if a Rect matches four given dimensions.
 */
#define RECT_EQUAL_DIMS( r, x, y, w, h ) \
  ( ( r ).x == ( x ) && ( r ).y == ( y ) && ( r ).width == ( w ) && ( r ).height == ( h ) )

/*!
 * \brief Extracts a Rect's x,y,width,height into separate variables.
 */
#define RECT_TO_DIMS( r, x, y, w, h ) ( x ) = ( r ).x, ( y ) = ( r ).y, ( w ) = ( r ).width, ( h ) = ( r ).height

/*!
 * \brief True if a point (px,py) lies within Rect r's boundaries.
 */
#define RECT_CONTAINS( r, px, py ) \
  ( ( px ) >= ( r ).x && ( px ) < ( r ).x + ( r ).width && ( py ) >= ( r ).y && ( py ) < ( r ).y + ( r ).height )

/*!
 * \brief True if Rect o is fully contained within Rect r.
 */
#define RECT_CONTAINS_RECT( r, o )                                                                \
  ( ( o ).x >= ( r ).x && ( o ).x + ( o ).width <= ( r ).x + ( r ).width && ( o ).y >= ( r ).y && \
    ( o ).y + ( o ).height <= ( r ).y + ( r ).height )

/*!
 * \brief True if two Rects overlap.
 */
#define RECT_INTERSECTS_RECT( r, o )                                                                          \
  ( ( o ).x < ( r ).x + ( r ).width && ( o ).x + ( o ).width > ( r ).x && ( o ).y < ( r ).y + ( r ).height && \
    ( o ).y + ( o ).height > ( r ).y )

/*!
 * \brief Sets Rect r to be the intersection of Rect a and b.
 * \warning Leaves r's width/height < 0 if a,b don't intersect.
 */
#define RECT_SET_INTERSECTION( r, a, b )                                                      \
  ( ( r ).x = MAX( ( a ).x, ( b ).x ), ( r ).y = MAX( ( a ).y, ( b ).y ),                     \
    ( r ).width  = MIN( ( a ).x + ( a ).width - 1, ( b ).x + ( b ).width - 1 ) - ( r ).x + 1, \
    ( r ).height = MIN( ( a ).y + ( a ).height - 1, ( b ).y + ( b ).height - 1 ) - ( r ).y + 1 )

/*!
 * \brief Returns the shortest Manhattan distance between two Rects r and o, or 0 if they intersect.
 *
 * \param r A Rect
 * \param o Another Rect
 * \return  0 if \p r intersects \p o, otherwise the shortest Manhattan distance.
 */
static inline gint rect_manhatten_distance( Rect r, Rect o ) {
  if ( RECT_INTERSECTS_RECT( r, o ) ) return 0;

  gint min_distance = G_MAXINT;
  if ( RECT_RIGHT( o ) < RECT_LEFT( r ) ) min_distance = MIN( min_distance, RECT_LEFT( r ) - RECT_RIGHT( o ) );
  if ( RECT_LEFT( o ) > RECT_RIGHT( r ) ) min_distance = MIN( min_distance, RECT_LEFT( o ) - RECT_RIGHT( r ) );
  if ( RECT_BOTTOM( o ) < RECT_TOP( r ) ) min_distance = MIN( min_distance, RECT_TOP( r ) - RECT_BOTTOM( o ) );
  if ( RECT_TOP( o ) > RECT_BOTTOM( r ) ) min_distance = MIN( min_distance, RECT_TOP( o ) - RECT_BOTTOM( r ) );
  return min_distance;
}

/*!
 * \brief A simple four-edge strut, typically used for margins or offsets on each side.
 *
 * For example, \c left=20 might mean "Keep a 20px margin on the left side."
 */
typedef struct _Strut {
  int left;   /*!< Pixels reserved on the left edge */
  int top;    /*!< Pixels reserved on the top edge  */
  int right;  /*!< Pixels reserved on the right edge */
  int bottom; /*!< Pixels reserved on the bottom edge */
} Strut;

/*!
 * \brief A more detailed strut partial with start/end fields for each edge.
 */
typedef struct _StrutPartial {
  int left;
  int top;
  int right;
  int bottom;

  int left_start, left_end;
  int top_start, top_end;
  int right_start, right_end;
  int bottom_start, bottom_end;
} StrutPartial;

/*!
 * \brief Sets the four edges of a Strut in one go.
 */
#define STRUT_SET( s, l, t, r, b ) ( s ).left = ( l ), ( s ).top = ( t ), ( s ).right = ( r ), ( s ).bottom = ( b )

/*!
 * \brief Sets all fields of a StrutPartial in one go.
 */
#define STRUT_PARTIAL_SET( s, l, t, r, b, ls, le, ts, te, rs, re, bs, be )                                     \
  ( s ).left = ( l ), ( s ).top = ( t ), ( s ).right = ( r ), ( s ).bottom = ( b ), ( s ).left_start = ( ls ), \
      ( s ).left_end = ( le ), ( s ).top_start = ( ts ), ( s ).top_end = ( te ), ( s ).right_start = ( rs ),   \
      ( s ).right_end = ( re ), ( s ).bottom_start = ( bs ), ( s ).bottom_end = ( be )

/*!
 * \brief Expands Strut s1 so it includes any edges that s2 has larger.
 */
#define STRUT_ADD( s1, s2 )                                                                        \
  ( s1 ).left = MAX( ( s1 ).left, ( s2 ).left ), ( s1 ).right = MAX( ( s1 ).right, ( s2 ).right ), \
       ( s1 ).top = MAX( ( s1 ).top, ( s2 ).top ), ( s1 ).bottom = MAX( ( s1 ).bottom, ( s2 ).bottom )

/*!
 * \brief True if at least one of left, top, right, or bottom is nonzero.
 */
#define STRUT_EXISTS( s1 ) ( ( s1 ).left || ( s1 ).top || ( s1 ).right || ( s1 ).bottom )

/*!
 * \brief Checks if two Struts have identical left,top,right,bottom.
 */
#define STRUT_EQUAL( s1, s2 )                                                                 \
  ( ( s1 ).left == ( s2 ).left && ( s1 ).top == ( s2 ).top && ( s1 ).right == ( s2 ).right && \
    ( s1 ).bottom == ( s2 ).bottom )

/*!
 * \brief Checks if two StrutPartial have identical fields.
 */
#define PARTIAL_STRUT_EQUAL( s1, s2 )                                                                                 \
  ( ( s1 ).left == ( s2 ).left && ( s1 ).top == ( s2 ).top && ( s1 ).right == ( s2 ).right &&                         \
    ( s1 ).bottom == ( s2 ).bottom && ( s1 ).left_start == ( s2 ).left_start && ( s1 ).left_end == ( s2 ).left_end && \
    ( s1 ).top_start == ( s2 ).top_start && ( s1 ).top_end == ( s2 ).top_end &&                                       \
    ( s1 ).right_start == ( s2 ).right_start && ( s1 ).right_end == ( s2 ).right_end &&                               \
    ( s1 ).bottom_start == ( s2 ).bottom_start && ( s1 ).bottom_end == ( s2 ).bottom_end )

/*!
 * \brief Checks if two ranges [r1x, r1x+r1w) and [r2x, r2x+r2w) intersect horizontally.
 *
 * \warning Both ranges must have nonzero width to matter.
 */
#define RANGES_INTERSECT( r1x, r1w, r2x, r2w ) \
  ( r1w && r2w && ( r1x ) < ( r2x ) + ( r2w ) && ( r1x ) + ( r1w ) > ( r2x ) )

#endif /* __geom_h */
