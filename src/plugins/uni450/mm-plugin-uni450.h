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

#include "mm-plugin-common.h"
#include "mm-broadband-modem.h"
#include "mm-serial-parsers.h"
#include "mm-log-object.h"

G_BEGIN_DECLS

#define MM_TYPE_PLUGIN_UNI450            (mm_plugin_uni450_get_type ())
#define MM_PLUGIN_UNI450(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_PLUGIN_UNI450, MMPluginUni450))
#define MM_IS_PLUGIN_UNI450(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_PLUGIN_UNI450))

G_DECLARE_FINAL_TYPE (MMPluginUni450, mm_plugin_uni450, MM, PLUGIN_UNI450, MMPlugin)

/* Standard plugin factory */
MM_PLUGIN_NAMED_CREATOR_SCOPE MMPlugin * mm_plugin_create_uni450 (void);

G_END_DECLS
#endif /* MM_PLUGIN_UNI450_H */