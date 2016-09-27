/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-  */
/*
 * libgfbgraph - GObject library for Facebook Graph API
 * Copyright (C) 2016 Krzesimir Nowak <krzesimir@kinvolk.io>
 *
 * GFBGraph is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GFBGraph is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GFBGraph.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GFBGRAPH_ENUMS_H__
#define __GFBGRAPH_ENUMS_H__

#include <glib.h>

G_BEGIN_DECLS

/**
 * GFBGraphPictureType:
 * @GFBGRAPH_PICTURE_SMALL: Small picture (shortest size likely sized to 50 pixels wide or high).
 * @GFBGRAPH_PICTURE_NORMAL: Normal picture (shortest size likely sized to 100 pixels wide or high).
 * @GFBGRAPH_PICTURE_ALBUM: Album picture (shortest size likely sized to 50 pixels wide or high? Looks the same as the small version).
 * @GFBGRAPH_PICTURE_LARGE: Large picture (shortest size likely sized to 200 pixels wide or high).
 * @GFBGRAPH_PICTURE_SQUARE: Square picture (likely sized to 50 pixels wide and high).
 *
 * Describes the size type of the fetched picture.
 */
typedef enum {
  GFBGRAPH_PICTURE_SMALL,
  GFBGRAPH_PICTURE_NORMAL,
  GFBGRAPH_PICTURE_ALBUM,
  GFBGRAPH_PICTURE_LARGE,
  GFBGRAPH_PICTURE_SQUARE
} GFBGraphPictureType;

G_END_DECLS

#endif /* __GFBGRAPH_ENUMS_H__ */
