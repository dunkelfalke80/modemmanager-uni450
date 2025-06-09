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

#include <config.h>
#include <gmodule.h>
#include <glib.h>
#include <glib-object.h>

#include "mm-plugin-uni450.h"
#include "mm-broadband-modem.h"
#include "mm-port-serial-at.h"
#include "mm-log-object.h"

G_MODULE_EXPORT MMPlugin *mm_plugin_create (void);

static MMBaseModem *
create_modem (MMPlugin *self,
              const gchar *device,
              const gchar *physdev,
              const gchar **drivers,
              guint16 vendor,
              guint16 product,
              guint16 subsystem_vendor,
              guint16 subsystem_product,
              GList *probes,
              GError **error)
{
    return MM_BASE_MODEM (mm_broadband_modem_new (device,
                                                   physdev,
                                                   drivers,
                                                   mm_plugin_get_name (self),
                                                   vendor,
                                                   product));
}

static gboolean
grab_port (MMPlugin     *self,
           MMBaseModem  *modem,
           MMPortProbe  *probe,
           GError      **error)
{
    MMPortSerialAtFlag pflags = MM_PORT_SERIAL_AT_FLAG_NONE_NO_GENERIC;
    MMPortType         ptype;

    ptype = mm_port_probe_get_port_type (probe);

    /* Only handle AT interfaces, ignore everything else */
    if (ptype != MM_PORT_TYPE_AT)
        return FALSE;

    /* Skip if a primary AT port is already present */
    if (mm_base_modem_peek_port_primary (modem))
        return FALSE;

    /* First AT seen ==> mark as PRIMARY */
    pflags |= MM_PORT_SERIAL_AT_FLAG_PRIMARY;

    if (mm_base_modem_grab_port (modem,
                                 mm_port_probe_peek_port (probe),
                                 mm_port_probe_get_port_group (probe),
                                 ptype,
                                 pflags,
                                 error))
    {
        mm_obj_info (self,
                     "(%s/%s) primary AT port grabbed",
                     mm_port_probe_get_port_subsys (probe),
                     mm_port_probe_get_port_name  (probe));
        g_message ("ModemManager UNI450: AT port acquired: %s",
                   mm_port_probe_get_port_name (probe));
        return TRUE;
    }

    return FALSE;
}

/*****************************************************************************/

G_MODULE_EXPORT MMPlugin *
mm_plugin_create (void)
{
    static const gchar *subsystems[] = { "tty", NULL };
    static const guint16 vendor_ids[] = { USB_VID_UNI450, 0 };

    return MM_PLUGIN (
        g_object_new (MM_TYPE_PLUGIN_UNI450,
                      MM_PLUGIN_NAME,               "UNI450",
                      MM_PLUGIN_ALLOWED_SUBSYSTEMS, subsystems,
                      MM_PLUGIN_ALLOWED_VENDOR_IDS, vendor_ids,
                      MM_PLUGIN_ALLOWED_AT,         TRUE,
                      NULL));
}

static void
mm_plugin_uni450_init (MMPluginUni450 *self)
{
}

static void
mm_plugin_uni450_class_init (MMPluginUni450Class *klass)
{
    MMPluginClass *plugin_class = MM_PLUGIN_CLASS (klass);

    plugin_class->create_modem = create_modem;
    plugin_class->grab_port = grab_port;
}

GType
mm_plugin_uni450_get_type (void)
{
    static GType g_define_type_id = 0;

    if (g_once_init_enter (&g_define_type_id)) {
        GType new_type = g_type_register_static (MM_TYPE_PLUGIN,
                                                 "MMPluginUni450",
                                                 &(GTypeInfo) {
                                                     sizeof (MMPluginUni450Class),
                                                     NULL,
                                                     NULL,
                                                     (GClassInitFunc) mm_plugin_uni450_class_init,
                                                     NULL,
                                                     NULL,
                                                     sizeof (MMPluginUni450),
                                                     0,
                                                     (GInstanceInitFunc) mm_plugin_uni450_init,
                                                     NULL
                                                 },
                                                 0);
        g_once_init_leave (&g_define_type_id, new_type);
    }

    return g_define_type_id;
}