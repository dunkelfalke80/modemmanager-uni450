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

 #include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <gmodule.h>

#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-plugin-uni450.h"
#include "mm-port.h"
#include "mm-bearer.h"

#define USB_VENDOR_UNI450   0x1076
#define USB_PRODUCT_UNI450  0x9082

G_DEFINE_TYPE (MMPluginUni450, mm_plugin_uni450, MM_TYPE_PLUGIN)

static MMPortGrabResult
static MMPortGrabResult
uni450_grab_port (MMPlugin    *self,
                  MMPort      *port,
                  MMPortProbe *probe,
                  GError     **error)
{
    guint16 vid = mm_port_get_vendor (port);
    guint16 pid = mm_port_get_product(port);

    if (vid == USB_VENDOR_UNI450 && pid == USB_PRODUCT_UNI450) {
        mm_debug ("[uni450] claiming port %s", mm_port_get_device (port));
        *probe = MM_PORT_PROBE_VENDOR_PRODUCT;
        return MM_PORT_GRAB_RESULT_CLAIMED;
    }
    return MM_PLUGIN_CLASS (mm_plugin_uni450_parent_class)
            ->grab_port (self, port, probe, error);
}

/* This is called when the generic modem base wants to start data.
 * We override it to run AT+CGDCONT and AT+CGATT, then do RNDIS up + DHCP.
 */
static void
uni450_connect_bearer (MMBroadbandModemGeneric  *modem,
                       MMBearer                 *bearer,
                       GCancellable             *cancellable,
                       GAsyncReadyCallback       callback,
                       gpointer                  user_data)
{
    GError     *err = NULL;
    gchar      *resp;
    const char *iface = "enx885d90efffff";    /* adjust if yours differs */
    const char *apn   = "internet.t-mobile";  /* <— replace or pull from config */

    /* 1) define PDP context */
    resp = mm_base_modem_run_command_sync (MM_BASE_MODEM (modem),
                                          "AT+CGDCONT=1,\"IP\",\"%s\"", 10,
                                          cancellable, &err,
                                          apn);
    if (err) {
        mm_critical ("%s: CGDCONT failed: %s", __func__, err->message);
        g_clear_error (&err);
    }
    g_free (resp);

    /* 2) attach */
    resp = mm_base_modem_run_command_sync (MM_BASE_MODEM (modem),
                                          "AT+CGATT=1", 10,
                                          cancellable, &err);
    if (err) {
        mm_critical ("%s: CGATT failed: %s", __func__, err->message);
        g_clear_error (&err);
    }
    g_free (resp);

    /* 3) bring up RNDIS + DHCP */
    system ("ip link set %s up", iface);
    system ("dhclient -1 %s", iface);

    /* 4) notify ModemManager that the bearer is now connected */
    mm_bearer_connected (bearer, NULL);

    /* finish */
    callback (modem, user_data);
}

static void
mm_plugin_uni450_class_init (MMPluginUni450Class *klass)
{
    MMPluginClass *pclass = MM_PLUGIN_CLASS (klass);

    pclass->grab_port          = uni450_grab_port;

    /* Hook up our connect_bearer override */
    MMBroadbandModemGenericClass *gbc =
        MMBROADBAND_MODEM_GENERIC_CLASS (klass);
    gbc->connect_bearer_async  = uni450_connect_bearer;
}

static void
mm_plugin_uni450_init (MMPluginUni450 *self)
{
    /* no instance data needed */
}

/* factory */
MMPlugin * mm_plugin_uni450_new (void)
{
    return g_object_new (MM_TYPE_PLUGIN_UNI450, NULL);
}