/* Copyright (C) Roman Galperin 2025
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef MM_PLUGIN_UNI450_H
#define MM_PLUGIN_UNI450_H

#include <glib.h>
#include <glib-object.h>
#include "mm-plugin-common.h"

#define MM_TYPE_PLUGIN_UNI450            (mm_plugin_uni450_get_type ())
#define MM_PLUGIN_UNI450(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_PLUGIN_UNI450, MMPluginUni450))
#define MM_PLUGIN_UNI450_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MM_TYPE_PLUGIN_UNI450, MMPluginUni450Class))
#define MM_IS_PLUGIN_UNI450(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_PLUGIN_UNI450))
#define MM_IS_PLUGIN_UNI450_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MM_TYPE_PLUGIN_UNI450))
#define MM_PLUGIN_UNI450_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MM_TYPE_PLUGIN_UNI450, MMPluginUni450Class))

#define USB_VID_UNI450   0x1076
#define USB_PID_UNI450   0x9082

typedef struct _MMPluginUni450 MMPluginUni450;
typedef struct _MMPluginUni450Class MMPluginUni450Class;

struct _MMPluginUni450 {
    MMPlugin parent;
};

struct _MMPluginUni450Class {
    MMPluginClass parent;
};

GType mm_plugin_uni450_get_type (void);

#endif /* MM_PLUGIN_UNI450_H */