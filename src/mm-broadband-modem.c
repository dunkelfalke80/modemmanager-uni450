/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details:
 *
 * Copyright (C) 2008 - 2009 Novell, Inc.
 * Copyright (C) 2009 - 2012 Red Hat, Inc.
 * Copyright (C) 2015 Marco Bascetta <marco.bascetta@sadel.it>
 * Copyright (C) 2019 Purism SPC
 * Copyright (C) 2011 - 2022 Google, Inc.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <ModemManager.h>
#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-base-modem-at.h"
#include "mm-broadband-modem.h"
#include "mm-iface-modem.h"
#include "mm-iface-modem-3gpp.h"
#include "mm-iface-modem-3gpp-profile-manager.h"
#include "mm-iface-modem-3gpp-ussd.h"
#include "mm-iface-modem-cdma.h"
#include "mm-iface-modem-cell-broadcast.h"
#include "mm-iface-modem-simple.h"
#include "mm-iface-modem-location.h"
#include "mm-iface-modem-messaging.h"
#include "mm-iface-modem-voice.h"
#include "mm-iface-modem-time.h"
#include "mm-iface-modem-firmware.h"
#include "mm-iface-modem-sar.h"
#include "mm-iface-modem-signal.h"
#include "mm-iface-modem-oma.h"
#include "mm-broadband-bearer.h"
#include "mm-bearer-list.h"
#include "mm-cbm-list.h"
#include "mm-cbm-part.h"
#include "mm-sms-list.h"
#include "mm-sms-part-3gpp.h"
#include "mm-sms-at.h"
#include "mm-call-list.h"
#include "mm-base-sim.h"
#include "mm-log-object.h"
#include "mm-modem-helpers.h"
#include "mm-error-helpers.h"
#include "mm-port-serial-qcdm.h"
#include "libqcdm/src/errors.h"
#include "libqcdm/src/commands.h"
#include "libqcdm/src/logs.h"
#include "libqcdm/src/log-items.h"
#include "mm-helper-enums-types.h"
#include "mm-bind.h"

static void iface_modem_init                      (MMIfaceModemInterface                   *iface);
static void iface_modem_3gpp_init                 (MMIfaceModem3gppInterface               *iface);
static void iface_modem_3gpp_profile_manager_init (MMIfaceModem3gppProfileManagerInterface *iface);
static void iface_modem_3gpp_ussd_init            (MMIfaceModem3gppUssdInterface           *iface);
static void iface_modem_cdma_init                 (MMIfaceModemCdmaInterface               *iface);
static void iface_modem_cell_broadcast_init       (MMIfaceModemCellBroadcastInterface      *iface);
static void iface_modem_simple_init               (MMIfaceModemSimpleInterface             *iface);
static void iface_modem_location_init             (MMIfaceModemLocationInterface           *iface);
static void iface_modem_messaging_init            (MMIfaceModemMessagingInterface          *iface);
static void iface_modem_voice_init                (MMIfaceModemVoiceInterface              *iface);
static void iface_modem_time_init                 (MMIfaceModemTimeInterface               *iface);
static void iface_modem_signal_init               (MMIfaceModemSignalInterface             *iface);
static void iface_modem_oma_init                  (MMIfaceModemOmaInterface                *iface);
static void iface_modem_firmware_init             (MMIfaceModemFirmwareInterface           *iface);
static void iface_modem_sar_init                  (MMIfaceModemSarInterface                *iface);

G_DEFINE_TYPE_EXTENDED (MMBroadbandModem, mm_broadband_modem, MM_TYPE_BASE_MODEM, 0,
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM, iface_modem_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_3GPP, iface_modem_3gpp_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_3GPP_PROFILE_MANAGER, iface_modem_3gpp_profile_manager_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_3GPP_USSD, iface_modem_3gpp_ussd_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_CDMA, iface_modem_cdma_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_CELL_BROADCAST, iface_modem_cell_broadcast_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_SIMPLE, iface_modem_simple_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_LOCATION, iface_modem_location_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_MESSAGING, iface_modem_messaging_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_VOICE, iface_modem_voice_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_TIME, iface_modem_time_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_SIGNAL, iface_modem_signal_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_OMA, iface_modem_oma_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_SAR, iface_modem_sar_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_FIRMWARE, iface_modem_firmware_init))

enum {
    PROP_0,
    PROP_MODEM_DBUS_SKELETON,
    PROP_MODEM_3GPP_DBUS_SKELETON,
    PROP_MODEM_3GPP_PROFILE_MANAGER_DBUS_SKELETON,
    PROP_MODEM_3GPP_USSD_DBUS_SKELETON,
    PROP_MODEM_CDMA_DBUS_SKELETON,
    PROP_MODEM_CELL_BROADCAST_DBUS_SKELETON,
    PROP_MODEM_SIMPLE_DBUS_SKELETON,
    PROP_MODEM_LOCATION_DBUS_SKELETON,
    PROP_MODEM_MESSAGING_DBUS_SKELETON,
    PROP_MODEM_VOICE_DBUS_SKELETON,
    PROP_MODEM_TIME_DBUS_SKELETON,
    PROP_MODEM_SIGNAL_DBUS_SKELETON,
    PROP_MODEM_OMA_DBUS_SKELETON,
    PROP_MODEM_FIRMWARE_DBUS_SKELETON,
    PROP_MODEM_SAR_DBUS_SKELETON,
    PROP_MODEM_SIM,
    PROP_MODEM_SIM_SLOTS,
    PROP_MODEM_BEARER_LIST,
    PROP_MODEM_STATE,
    PROP_MODEM_3GPP_REGISTRATION_STATE,
    PROP_MODEM_3GPP_CS_NETWORK_SUPPORTED,
    PROP_MODEM_3GPP_PS_NETWORK_SUPPORTED,
    PROP_MODEM_3GPP_EPS_NETWORK_SUPPORTED,
    PROP_MODEM_3GPP_5GS_NETWORK_SUPPORTED,
    PROP_MODEM_3GPP_IGNORED_FACILITY_LOCKS,
    PROP_MODEM_3GPP_INITIAL_EPS_BEARER,
    PROP_MODEM_3GPP_PACKET_SERVICE_STATE,
    PROP_MODEM_CDMA_CDMA1X_REGISTRATION_STATE,
    PROP_MODEM_CDMA_EVDO_REGISTRATION_STATE,
    PROP_MODEM_CDMA_CDMA1X_NETWORK_SUPPORTED,
    PROP_MODEM_CDMA_EVDO_NETWORK_SUPPORTED,
    PROP_MODEM_CELL_BROADCAST_CBM_LIST,
    PROP_MODEM_MESSAGING_SMS_LIST,
    PROP_MODEM_MESSAGING_SMS_PDU_MODE,
    PROP_MODEM_MESSAGING_SMS_DEFAULT_STORAGE,
    PROP_MODEM_LOCATION_ALLOW_GPS_UNMANAGED_ALWAYS,
    PROP_MODEM_VOICE_CALL_LIST,
    PROP_MODEM_SIMPLE_STATUS,
    PROP_MODEM_SIM_HOT_SWAP_SUPPORTED,
    PROP_MODEM_PERIODIC_SIGNAL_CHECK_DISABLED,
    PROP_MODEM_PERIODIC_ACCESS_TECH_CHECK_DISABLED,
    PROP_MODEM_PERIODIC_CALL_LIST_CHECK_DISABLED,
    PROP_MODEM_INDICATION_CALL_LIST_RELOAD_ENABLED,
    PROP_MODEM_CARRIER_CONFIG_MAPPING,
    PROP_MODEM_FIRMWARE_IGNORE_CARRIER,
    PROP_FLOW_CONTROL,
    PROP_INDICATORS_DISABLED,
    PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

#if defined WITH_SUSPEND_RESUME

enum {
    SIGNAL_SYNC_NEEDED,
    SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

#endif

/* When CIND is supported, invalid indicators are marked with this value */
#define CIND_INDICATOR_INVALID 255
#define CIND_INDICATOR_IS_VALID(u) (u != CIND_INDICATOR_INVALID)

typedef struct _PortsContext PortsContext;

struct _MMBroadbandModemPrivate {
    /* Broadband modem specific implementation */
    PortsContext *enabled_ports_ctx;
    PortsContext *sim_hot_swap_ports_ctx;
    PortsContext *in_call_ports_ctx;
    gboolean modem_init_run;
    gboolean sim_hot_swap_supported;
    gboolean periodic_signal_check_disabled;
    gboolean periodic_access_tech_check_disabled;

    /*<--- Modem interface --->*/
    /* Properties */
    GObject *modem_dbus_skeleton;
    MMBaseSim *modem_sim;
    GPtrArray *modem_sim_slots;
    MMBearerList *modem_bearer_list;
    MMModemState modem_state;
    gchar *carrier_config_mapping;
    /* Implementation helpers */
    MMModemCharset modem_current_charset;
    gboolean modem_cind_disabled;
    gboolean modem_cind_support_checked;
    gboolean modem_cind_supported;
    guint modem_cind_indicator_signal_quality;
    guint modem_cind_min_signal_quality;
    guint modem_cind_max_signal_quality;
    guint modem_cind_indicator_roaming;
    guint modem_cind_indicator_service;
    MM3gppCmerMode modem_cmer_enable_mode;
    MM3gppCmerMode modem_cmer_disable_mode;
    MM3gppCmerInd modem_cmer_ind;
    gboolean modem_cgerep_support_checked;
    MM3gppCgerepMode modem_cgerep_enable_mode;
    MM3gppCgerepMode modem_cgerep_disable_mode;
    gboolean modem_cgerep_supported;
    MMFlowControl flow_control;

    /*<--- Modem 3GPP interface --->*/
    /* Properties */
    GObject *modem_3gpp_dbus_skeleton;
    MMModem3gppRegistrationState modem_3gpp_registration_state;
    gboolean modem_3gpp_cs_network_supported;
    gboolean modem_3gpp_ps_network_supported;
    gboolean modem_3gpp_eps_network_supported;
    gboolean modem_3gpp_5gs_network_supported;
    /* Implementation helpers */
    GPtrArray *modem_3gpp_registration_regex;
    MMModem3gppFacility modem_3gpp_ignored_facility_locks;
    MMBaseBearer *modem_3gpp_initial_eps_bearer;
    MMModem3gppPacketServiceState modem_3gpp_packet_service_state;
    gboolean initial_eps_bearer_cid_support_checked;
    gint initial_eps_bearer_cid;

    /*<--- Modem 3GPP Profile Manager interface --->*/
    /* Properties */
    GObject *modem_3gpp_profile_manager_dbus_skeleton;

    /*<--- Modem 3GPP USSD interface --->*/
    /* Properties */
    GObject *modem_3gpp_ussd_dbus_skeleton;
    /* Implementation helpers */
    gboolean use_unencoded_ussd;
    GTask *pending_ussd_action;

    /*<--- Modem CDMA interface --->*/
    /* Properties */
    GObject *modem_cdma_dbus_skeleton;
    MMModemCdmaRegistrationState modem_cdma_cdma1x_registration_state;
    MMModemCdmaRegistrationState modem_cdma_evdo_registration_state;
    gboolean modem_cdma_cdma1x_network_supported;
    gboolean modem_cdma_evdo_network_supported;
    GCancellable *modem_cdma_pending_registration_cancellable;
    /* Implementation helpers */
    gboolean checked_sprint_support;
    gboolean has_spservice;
    gboolean has_speri;
    gint evdo_pilot_rssi;

    /*<--- Modem Simple interface --->*/
    /* Properties */
    GObject *modem_simple_dbus_skeleton;
    MMSimpleStatus *modem_simple_status;

    /*<--- Modem Location interface --->*/
    /* Properties */
    GObject *modem_location_dbus_skeleton;
    gboolean modem_location_allow_gps_unmanaged_always;

    /*<--- Modem Messaging interface --->*/
    /* Properties */
    GObject *modem_messaging_dbus_skeleton;
    MMSmsList *modem_messaging_sms_list;
    gboolean modem_messaging_sms_pdu_mode;
    MMSmsStorage modem_messaging_sms_default_storage;
    /* Implementation helpers */
    gboolean sms_supported_modes_checked;
    gboolean mem1_storage_locked;
    MMSmsStorage current_sms_mem1_storage;
    gboolean mem2_storage_locked;
    MMSmsStorage current_sms_mem2_storage;

    /*<--- Modem Voice interface --->*/
    /* Properties */
    GObject    *modem_voice_dbus_skeleton;
    MMCallList *modem_voice_call_list;
    gboolean    periodic_call_list_check_disabled;
    gboolean    indication_call_list_reload_enabled;
    gboolean    clcc_supported;

    /*<--- Modem Time interface --->*/
    /* Properties */
    GObject *modem_time_dbus_skeleton;

    /*<--- Modem Signal interface --->*/
    /* Properties */
    GObject *modem_signal_dbus_skeleton;

    /*<--- Modem OMA interface --->*/
    /* Properties */
    GObject *modem_oma_dbus_skeleton;

    /*<--- Modem Firmware interface --->*/
    /* Properties */
    GObject  *modem_firmware_dbus_skeleton;

    /*<--- Modem Sar interface --->*/
    /* Properties */
    GObject  *modem_sar_dbus_skeleton;

    /*<--- Modem CellBroadcast interface --->*/
    /* Properties */
    GObject *modem_cell_broadcast_dbus_skeleton;
    MMCbmList *modem_cell_broadcast_cbm_list;

    gboolean  modem_firmware_ignore_carrier;
};

/*****************************************************************************/
/* Generic ports open/close context */

struct _PortsContext {
    volatile gint     ref_count;
    MMPortSerialAt   *primary;
    gboolean          primary_open;
    MMPortSerialAt   *secondary;
    gboolean          secondary_open;
    MMPortSerialQcdm *qcdm;
    gboolean          qcdm_open;
};

static PortsContext *
ports_context_ref (PortsContext *ctx)
{
    g_atomic_int_inc (&ctx->ref_count);
    return ctx;
}

static void
ports_context_dispose (PortsContext *ctx)
{
    if (ctx->primary && ctx->primary_open) {
        mm_port_serial_close (MM_PORT_SERIAL (ctx->primary));
        ctx->primary_open = FALSE;
    }
    if (ctx->secondary && ctx->secondary_open) {
        mm_port_serial_close (MM_PORT_SERIAL (ctx->secondary));
        ctx->secondary_open = FALSE;
    }
    if (ctx->qcdm && ctx->qcdm_open) {
        mm_port_serial_close (MM_PORT_SERIAL (ctx->qcdm));
        ctx->qcdm_open = FALSE;
    }
}

static void
ports_context_unref (PortsContext *ctx)
{
    if (g_atomic_int_dec_and_test (&ctx->ref_count)) {
        ports_context_dispose (ctx);
        g_clear_object (&ctx->primary);
        g_clear_object (&ctx->secondary);
        g_clear_object (&ctx->qcdm);

        g_free (ctx);
    }
}

static gboolean
ports_context_open (MMBroadbandModem  *self,
                    PortsContext      *ctx,
                    gboolean           disable_at_init_sequence,
                    gboolean           with_at_secondary,
                    gboolean           with_qcdm,
                    GError           **error)
{
    /* Open primary */
    ctx->primary = mm_base_modem_get_port_primary (MM_BASE_MODEM (self));
    if (!ctx->primary) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "Couldn't get primary port");
        return FALSE;
    }
    /* If we'll need to run modem initialization, disable port init sequence */
    if (disable_at_init_sequence)
        g_object_set (ctx->primary,
                      MM_PORT_SERIAL_AT_INIT_SEQUENCE_ENABLED, FALSE,
                      NULL);
    if (!mm_port_serial_open (MM_PORT_SERIAL (ctx->primary), error)) {
        g_prefix_error (error, "Couldn't open primary port: ");
        return FALSE;
    }
    ctx->primary_open = TRUE;

    /* Open secondary (optional) */
    if (with_at_secondary) {
        ctx->secondary = mm_base_modem_get_port_secondary (MM_BASE_MODEM (self));
        if (ctx->secondary) {
            /* If we'll need to run modem initialization, disable port init sequence */
            if (disable_at_init_sequence)
                g_object_set (ctx->secondary,
                              MM_PORT_SERIAL_AT_INIT_SEQUENCE_ENABLED, FALSE,
                              NULL);
            if (!mm_port_serial_open (MM_PORT_SERIAL (ctx->secondary), error)) {
                g_prefix_error (error, "Couldn't open secondary port: ");
                return FALSE;
            }
            ctx->secondary_open = TRUE;
        }
    }

    /* Open qcdm (optional) */
    if (with_qcdm) {
        ctx->qcdm = mm_base_modem_get_port_qcdm (MM_BASE_MODEM (self));
        if (ctx->qcdm) {
            if (!mm_port_serial_open (MM_PORT_SERIAL (ctx->qcdm), error)) {
                g_prefix_error (error, "Couldn't open QCDM port: ");
                return FALSE;
            }
            ctx->qcdm_open = TRUE;
        }
    }

    return TRUE;
}

static PortsContext *
ports_context_new (void)
{
    PortsContext *ctx;

    ctx = g_new0 (PortsContext, 1);
    ctx->ref_count = 1;
    return ctx;
}

/*****************************************************************************/
/* Create Bearer (Modem interface) */

static MMBaseBearer *
modem_create_bearer_finish (MMIfaceModem *self,
                            GAsyncResult *res,
                            GError **error)
{
    return g_task_propagate_pointer (G_TASK (res), error);
}

static void
broadband_bearer_new_ready (GObject *source,
                            GAsyncResult *res,
                            GTask *task)
{
    MMBaseBearer *bearer = NULL;
    GError *error = NULL;

    bearer = mm_broadband_bearer_new_finish (res, &error);
    if (!bearer)
        g_task_return_error (task, error);
    else
        g_task_return_pointer (task, bearer, g_object_unref);
    g_object_unref (task);
}

static void
modem_create_bearer (MMIfaceModem *self,
                     MMBearerProperties *props,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* We just create a MMBroadbandBearer */
    mm_obj_dbg (self, "creating broadband bearer in broadband modem...");
    mm_broadband_bearer_new (MM_BROADBAND_MODEM (self),
                             props,
                             NULL, /* cancellable */
                             (GAsyncReadyCallback)broadband_bearer_new_ready,
                             task);
}

/*****************************************************************************/
/* Create Bearer List (Modem interface) */

static MMBearerList *
modem_create_bearer_list (MMIfaceModem *self)
{
    guint n;

    /* The maximum number of available/connected modems is guessed from
     * the size of the data ports list. */
    n = g_list_length (mm_base_modem_peek_data_ports (MM_BASE_MODEM (self)));
    mm_obj_dbg (self, "allowed up to %u active bearers", n);

    /* by default, no multiplexing support */
    return mm_bearer_list_new (n, 0);
}

/*****************************************************************************/
/* Create SIM (Modem interface) */

static MMBaseSim *
modem_create_sim_finish (MMIfaceModem *self,
                         GAsyncResult *res,
                         GError **error)
{
    return mm_base_sim_new_finish (res, error);
}

static void
modem_create_sim (MMIfaceModem *self,
                  GAsyncReadyCallback callback,
                  gpointer user_data)
{
    /* New generic SIM */
    mm_base_sim_new (MM_BASE_MODEM (self),
                     G_OBJECT (self),
                     NULL, /* cancellable */
                     callback,
                     user_data);
}

/*****************************************************************************/
/* Helper to manage AT-based SIM hot swap ports context */

gboolean
mm_broadband_modem_sim_hot_swap_ports_context_init (MMBroadbandModem  *self,
                                                    GError           **error)
{
    PortsContext *ports;

    mm_obj_dbg (self, "creating serial ports context for SIM hot swap...");
    ports = ports_context_new ();
    if (!ports_context_open (self, ports, FALSE, FALSE, FALSE, error)) {
        ports_context_unref (ports);
        return FALSE;
    }

    g_assert (!self->priv->sim_hot_swap_ports_ctx);
    self->priv->sim_hot_swap_ports_ctx = ports;
    return TRUE;
}

void
mm_broadband_modem_sim_hot_swap_ports_context_reset (MMBroadbandModem *self)
{
    if (self->priv->sim_hot_swap_ports_ctx) {
        mm_obj_dbg (self, "releasing serial ports context for SIM hot swap");
        ports_context_unref (self->priv->sim_hot_swap_ports_ctx);
        self->priv->sim_hot_swap_ports_ctx = NULL;
    }
}

/*****************************************************************************/
/* Capabilities loading (Modem interface) */

typedef struct {
    MMModemCapability caps;
    MMPortSerialQcdm *qcdm_port;
} LoadCapabilitiesContext;

static void
load_capabilities_context_free (LoadCapabilitiesContext *ctx)
{
    if (ctx->qcdm_port) {
        mm_port_serial_close (MM_PORT_SERIAL (ctx->qcdm_port));
        g_object_unref (ctx->qcdm_port);
    }
    g_slice_free (LoadCapabilitiesContext, ctx);
}

static MMModemCapability
modem_load_current_capabilities_finish (MMIfaceModem *self,
                                        GAsyncResult *res,
                                        GError **error)
{
    GError *inner_error = NULL;
    gssize value;

    value = g_task_propagate_int (G_TASK (res), &inner_error);
    if (inner_error) {
        g_propagate_error (error, inner_error);
        return MM_MODEM_CAPABILITY_NONE;
    }
    return (MMModemCapability)value;
}

static void
current_capabilities_ws46_test_ready (MMBaseModem *self,
                                      GAsyncResult *res,
                                      GTask *task)
{
    LoadCapabilitiesContext *ctx;
    const gchar *response;
    GArray      *modes;
    guint        i;
    gboolean     is_2g = FALSE;
    gboolean     is_3g = FALSE;
    gboolean     is_4g = FALSE;
    gboolean     is_5g = FALSE;

    ctx = g_task_get_task_data (task);

    /* Completely ignore errors in AT+WS46=? */
    response = mm_base_modem_at_command_finish (self, res, NULL);
    if (!response)
        goto out;

    modes = mm_3gpp_parse_ws46_test_response (response, self, NULL);
    if (!modes)
        goto out;

    /* Process list of modes to gather supported ones */
    for (i = 0; i < modes->len; i++) {
        if (g_array_index (modes, MMModemMode, i) & MM_MODEM_MODE_2G)
            is_2g = TRUE;
        if (g_array_index (modes, MMModemMode, i) & MM_MODEM_MODE_3G)
            is_3g = TRUE;
        if (g_array_index (modes, MMModemMode, i) & MM_MODEM_MODE_4G)
            is_4g = TRUE;
        if (g_array_index (modes, MMModemMode, i) & MM_MODEM_MODE_5G)
            is_5g = TRUE;
    }

    /* Add capabilities from modes */
    if (is_2g || is_3g)
        ctx->caps |= MM_MODEM_CAPABILITY_GSM_UMTS;
    if (is_4g)
        ctx->caps |= MM_MODEM_CAPABILITY_LTE;
    if (is_5g)
        ctx->caps |= MM_MODEM_CAPABILITY_5GNR;

    /* The +CGSM capability is saying that the modem is a 3GPP modem, but that
     * doesn't necessarily mean it's a GSM/UMTS modem, it could be a LTE-only
     * or 5GNR-only device. We did add the GSM_UMTS capability when +CGSM was
     * found, so now we'll check if the device only reports 4G or 5G modes, and
     * remove the capability if so.
     *
     * Note that we don't change the default +CGSM -> GSM/UMTS logic, we just
     * fix it up.
     */
    if ((is_4g || is_5g) && !is_2g && !is_3g)
        ctx->caps &= ~MM_MODEM_CAPABILITY_GSM_UMTS;

    g_array_unref (modes);

out:
    g_task_return_int (task, ctx->caps);
    g_object_unref (task);
}

typedef struct {
    const gchar *name;
    MMModemCapability bits;
} ModemCaps;

static const ModemCaps modem_caps[] = {
    { "+CGSM",     MM_MODEM_CAPABILITY_GSM_UMTS  },
    { "+CLTE2",    MM_MODEM_CAPABILITY_LTE       }, /* Novatel */
    { "+CLTE1",    MM_MODEM_CAPABILITY_LTE       }, /* Novatel */
    { "+CLTE",     MM_MODEM_CAPABILITY_LTE       },
    { "+CIS707-A", MM_MODEM_CAPABILITY_CDMA_EVDO },
    { "+CIS707A",  MM_MODEM_CAPABILITY_CDMA_EVDO }, /* Cmotech */
    { "+CIS707",   MM_MODEM_CAPABILITY_CDMA_EVDO },
    { "CIS707",    MM_MODEM_CAPABILITY_CDMA_EVDO }, /* Qualcomm Gobi */
    { "+CIS707P",  MM_MODEM_CAPABILITY_CDMA_EVDO },
    { "CIS-856",   MM_MODEM_CAPABILITY_CDMA_EVDO },
    { "+IS-856",   MM_MODEM_CAPABILITY_CDMA_EVDO }, /* Cmotech */
    { "CIS-856-A", MM_MODEM_CAPABILITY_CDMA_EVDO },
    { "CIS-856A",  MM_MODEM_CAPABILITY_CDMA_EVDO }, /* Kyocera KPC680 */
    { "+WIRIDIUM", MM_MODEM_CAPABILITY_IRIDIUM   }, /* Iridium satellite modems */
    { "CDMA 1x",   MM_MODEM_CAPABILITY_CDMA_EVDO }, /* Huawei Data07, ATI reply */
    /* TODO: FCLASS, MS, ES, DS? */
    { NULL }
};

static MMBaseModemAtResponseProcessorResult
parse_caps_gcap (MMBaseModem   *self,
                 gpointer       none,
                 const gchar   *command,
                 const gchar   *response,
                 gboolean       last_command,
                 const GError  *error,
                 GVariant     **result,
                 GError       **result_error)
{
    const ModemCaps *cap = modem_caps;
    guint32 ret = 0;

    *result = NULL;
    *result_error = NULL;

    if (!response)
        return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_CONTINUE;

    /* Some modems (Huawei E160g) won't respond to +GCAP with no SIM, but
     * will respond to ATI.  Ignore the error and continue.
     */
    if (strstr (response, "+CME ERROR:") || (error && error->domain == MM_MOBILE_EQUIPMENT_ERROR))
        return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_CONTINUE;

    while (cap->name) {
        if (strstr (response, cap->name))
            ret |= cap->bits;
        cap++;
    }

    /* No result built? */
    if (ret == 0)
        return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_CONTINUE;

    *result = g_variant_new_uint32 (ret);
    return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_SUCCESS;
}

static MMBaseModemAtResponseProcessorResult
parse_caps_cpin (MMBaseModem   *self,
                 gpointer       none,
                 const gchar   *command,
                 const gchar   *response,
                 gboolean       last_command,
                 const GError  *error,
                 GVariant     **result,
                 GError       **result_error)
{
    *result = NULL;
    *result_error = NULL;

    if (!response) {
        if (error &&
            (g_error_matches (error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_NOT_INSERTED) ||
             g_error_matches (error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_FAILURE) ||
             g_error_matches (error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_BUSY) ||
             g_error_matches (error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_WRONG))) {
            /* At least, it's a GSM modem */
            *result = g_variant_new_uint32 (MM_MODEM_CAPABILITY_GSM_UMTS);
            return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_SUCCESS;
        }
        return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_CONTINUE;
    }

    if (strcasestr (response, "SIM PIN") ||
        strcasestr (response, "SIM PUK") ||
        strcasestr (response, "PH-SIM PIN") ||
        strcasestr (response, "PH-FSIM PIN") ||
        strcasestr (response, "PH-FSIM PUK") ||
        strcasestr (response, "SIM PIN2") ||
        strcasestr (response, "SIM PUK2") ||
        strcasestr (response, "PH-NET PIN") ||
        strcasestr (response, "PH-NET PUK") ||
        strcasestr (response, "PH-NETSUB PIN") ||
        strcasestr (response, "PH-NETSUB PUK") ||
        strcasestr (response, "PH-SP PIN") ||
        strcasestr (response, "PH-SP PUK") ||
        strcasestr (response, "PH-CORP PIN") ||
        strcasestr (response, "PH-CORP PUK") ||
        strcasestr (response, "READY")) {
        /* At least, it's a GSM modem */
        *result = g_variant_new_uint32 (MM_MODEM_CAPABILITY_GSM_UMTS);
        return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_SUCCESS;
    }

    return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_CONTINUE;
}

static MMBaseModemAtResponseProcessorResult
parse_caps_cgmm (MMBaseModem   *self,
                 gpointer       none,
                 const gchar   *command,
                 const gchar   *response,
                 gboolean       last_command,
                 const GError  *error,
                 GVariant     **result,
                 GError       **result_error)
{
    *result = NULL;
    *result_error = NULL;

    if (!response)
        return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_CONTINUE;

    /* This check detects some really old Motorola and Mediatek GPRS dongles
     * and phones.
     */
    if (strstr (response, "GSM900") ||
        strstr (response, "GSM1800") ||
        strstr (response, "GSM1900") ||
        strstr (response, "GSM850") ||
        strstr (response, "MTK2")) {
        /* At least, it's a GSM modem */
        *result = g_variant_new_uint32 (MM_MODEM_CAPABILITY_GSM_UMTS);
        return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_SUCCESS;
    }

    return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_CONTINUE;
}

static const MMBaseModemAtCommand capabilities[] = {
    { "+GCAP",  2, TRUE,  parse_caps_gcap },
    { "I",      1, TRUE,  parse_caps_gcap }, /* yes, really parse as +GCAP */
    { "+CPIN?", 1, FALSE, parse_caps_cpin },
    { "+CGMM",  1, TRUE,  parse_caps_cgmm },
    { NULL }
};

static void
capabilities_sequence_ready (MMBaseModem *self,
                             GAsyncResult *res,
                             GTask *task)
{
    LoadCapabilitiesContext *ctx;
    GError *error = NULL;
    GVariant *result;

    ctx = g_task_get_task_data (task);

    result = mm_base_modem_at_sequence_finish (self, res, NULL, &error);
    if (!result) {
        if (error)
            g_task_return_error (task, error);
        else
            g_task_return_new_error (task,
                                     MM_CORE_ERROR,
                                     MM_CORE_ERROR_FAILED,
                                     "%s",
                                     "Failed to determine modem capabilities.");
        g_object_unref (task);
        return;
    }

    ctx->caps = (MMModemCapability)g_variant_get_uint32 (result);

    /* Some modems (e.g. Sierra Wireless MC7710 or ZTE MF820D) won't report LTE
     * capabilities even if they have them. So just run AT+WS46=? as well to see
     * if the current supported modes includes any LTE-specific mode.
     * This is not a big deal, as the AT+WS46=? command is a test command with a
     * cache-able result.
     *
     * E.g.:
     *  AT+WS46=?
     *   +WS46: (12,22,25,28,29)
     *   OK
     *
     */
    if (ctx->caps & MM_MODEM_CAPABILITY_GSM_UMTS &&
        !(ctx->caps & MM_MODEM_CAPABILITY_LTE)) {
        mm_base_modem_at_command (
            self,
            "+WS46=?",
            3,
            TRUE, /* allow caching, it's a test command */
            (GAsyncReadyCallback)current_capabilities_ws46_test_ready,
            task);
        return;
    }

    /* Otherwise, just set the already retrieved capabilities */
    g_task_return_int (task, ctx->caps);
    g_object_unref (task);
}

static void
load_current_capabilities_at (GTask *task)
{
    MMBroadbandModem *self;

    self = g_task_get_source_object (task);

    /* Launch sequence, we will expect a "u" GVariant */
    mm_base_modem_at_sequence (
        MM_BASE_MODEM (self),
        capabilities,
        NULL, /* response_processor_context */
        NULL, /* response_processor_context_free */
        (GAsyncReadyCallback)capabilities_sequence_ready,
        task);
}

static void
mode_pref_qcdm_ready (MMPortSerialQcdm *port,
                      GAsyncResult *res,
                      GTask *task)
{
    MMBroadbandModem *self;
    LoadCapabilitiesContext *ctx;
    QcdmResult *result;
    gint err = QCDM_SUCCESS;
    uint8_t pref = 0;
    GError *error = NULL;
    GByteArray *response;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    response = mm_port_serial_qcdm_command_finish (port, res, &error);
    if (error) {
        /* Fall back to AT checking */
        mm_obj_dbg (self, "failed to load NV ModePref: %s", error->message);
        g_error_free (error);
        goto at_caps;
    }

    /* Parse the response */
    result = qcdm_cmd_nv_get_mode_pref_result ((const gchar *)response->data,
                                               response->len,
                                               &err);
    g_byte_array_unref (response);
    if (!result) {
        mm_obj_dbg (self, "failed to parse NV ModePref result: %d", err);
        g_byte_array_unref (response);
        goto at_caps;
    }

    err = qcdm_result_get_u8 (result, QCDM_CMD_NV_GET_MODE_PREF_ITEM_MODE_PREF, &pref);
    qcdm_result_unref (result);
    if (err) {
        mm_obj_dbg (self, "failed to read NV ModePref: %d", err);
        goto at_caps;
    }

    /* Only parse explicit modes; for 'auto' just fall back to whatever
     * the AT current capabilities probing figures out.
     */
    switch (pref) {
    case QCDM_CMD_NV_MODE_PREF_ITEM_MODE_PREF_1X_HDR_LTE_ONLY:
        ctx->caps |= MM_MODEM_CAPABILITY_LTE;
        /* Fall through */
    case QCDM_CMD_NV_MODE_PREF_ITEM_MODE_PREF_1X_ONLY:
    case QCDM_CMD_NV_MODE_PREF_ITEM_MODE_PREF_HDR_ONLY:
    case QCDM_CMD_NV_MODE_PREF_ITEM_MODE_PREF_1X_HDR_ONLY:
        ctx->caps |= MM_MODEM_CAPABILITY_CDMA_EVDO;
        break;
    case QCDM_CMD_NV_MODE_PREF_ITEM_MODE_PREF_GSM_UMTS_LTE_ONLY:
        ctx->caps |= MM_MODEM_CAPABILITY_LTE;
        /* Fall through */
    case QCDM_CMD_NV_MODE_PREF_ITEM_MODE_PREF_GPRS_ONLY:
    case QCDM_CMD_NV_MODE_PREF_ITEM_MODE_PREF_UMTS_ONLY:
    case QCDM_CMD_NV_MODE_PREF_ITEM_MODE_PREF_GSM_UMTS_ONLY:
        ctx->caps |= MM_MODEM_CAPABILITY_GSM_UMTS;
        break;
    case QCDM_CMD_NV_MODE_PREF_ITEM_MODE_PREF_LTE_ONLY:
        ctx->caps |= MM_MODEM_CAPABILITY_LTE;
        break;
    default:
        break;
    }

    if (ctx->caps != MM_MODEM_CAPABILITY_NONE) {
        g_task_return_int (task, ctx->caps);
        g_object_unref (task);
        return;
    }

at_caps:
    load_current_capabilities_at (task);
}

static void
load_current_capabilities_qcdm (GTask *task)
{
    MMBroadbandModem *self;
    LoadCapabilitiesContext *ctx;
    GByteArray *cmd;
    GError *error = NULL;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    ctx->qcdm_port = mm_base_modem_peek_port_qcdm (MM_BASE_MODEM (self));
    g_assert (ctx->qcdm_port);

    if (!mm_port_serial_open (MM_PORT_SERIAL (ctx->qcdm_port), &error)) {
        mm_obj_dbg (self, "failed to open QCDM port for NV ModePref request: %s", error->message);
        g_error_free (error);
        ctx->qcdm_port = NULL;
        load_current_capabilities_at (task);
        return;
    }

    g_object_ref (ctx->qcdm_port);

    cmd = g_byte_array_sized_new (300);
    cmd->len = qcdm_cmd_nv_get_mode_pref_new ((char *) cmd->data, 300, 0);
    g_assert (cmd->len);

    mm_port_serial_qcdm_command (ctx->qcdm_port,
                                 cmd,
                                 3,
                                 NULL,
                                 (GAsyncReadyCallback)mode_pref_qcdm_ready,
                                 task);
    g_byte_array_unref (cmd);
}

static void
modem_load_current_capabilities (MMIfaceModem *self,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
    LoadCapabilitiesContext *ctx;
    GTask *task;

    mm_obj_dbg (self, "loading current capabilities...");

    task = g_task_new (self, NULL, callback, user_data);
    ctx = g_slice_new0 (LoadCapabilitiesContext);
    g_task_set_task_data (task, ctx, (GDestroyNotify)load_capabilities_context_free);

    if (mm_base_modem_peek_port_qcdm (MM_BASE_MODEM (self)))
        load_current_capabilities_qcdm (task);
    else
        load_current_capabilities_at (task);
}

/*****************************************************************************/
/* Manufacturer loading (Modem interface) */

static gchar *
sanitize_info_reply (GVariant *v, const char *prefix)
{
    const gchar *reply, *p;
    gchar *sanitized;

    /* Strip any leading command reply */
    reply = g_variant_get_string (v, NULL);
    p = strstr (reply, prefix);
    if (p)
        reply = p + strlen (prefix);
    sanitized = g_strdup (reply);
    return mm_strip_quotes (g_strstrip (sanitized));
}

static gchar *
modem_load_manufacturer_finish (MMIfaceModem *self,
                                GAsyncResult *res,
                                GError **error)
{
    GVariant *result;
    gchar *manufacturer = NULL;

    result = mm_base_modem_at_sequence_finish (MM_BASE_MODEM (self), res, NULL, error);
    if (result) {
        manufacturer = sanitize_info_reply (result, "GMI:");
        mm_obj_dbg (self, "loaded manufacturer: %s", manufacturer);
    }
    return manufacturer;
}

static const MMBaseModemAtCommand manufacturers[] = {
    { "+CGMI",  3, TRUE, mm_base_modem_response_processor_string_ignore_at_errors },
    { "+GMI",   3, TRUE, mm_base_modem_response_processor_string_ignore_at_errors },
    { NULL }
};

static void
modem_load_manufacturer (MMIfaceModem *self,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
    mm_obj_dbg (self, "loading manufacturer...");
    mm_base_modem_at_sequence (
        MM_BASE_MODEM (self),
        manufacturers,
        NULL, /* response_processor_context */
        NULL, /* response_processor_context_free */
        callback,
        user_data);
}

/*****************************************************************************/
/* Model loading (Modem interface) */

static gchar *
modem_load_model_finish (MMIfaceModem *self,
                         GAsyncResult *res,
                         GError **error)
{
    GVariant *result;
    gchar *model = NULL;

    result = mm_base_modem_at_sequence_finish (MM_BASE_MODEM (self), res, NULL, error);
    if (result) {
        model = sanitize_info_reply (result, "GMM:");
        mm_obj_dbg (self, "loaded model: %s", model);
    }
    return model;
}

static const MMBaseModemAtCommand models[] = {
    { "+CGMM",  3, TRUE, mm_base_modem_response_processor_string_ignore_at_errors },
    { "+GMM",   3, TRUE, mm_base_modem_response_processor_string_ignore_at_errors },
    { NULL }
};

static void
modem_load_model (MMIfaceModem *self,
                  GAsyncReadyCallback callback,
                  gpointer user_data)
{
    mm_obj_dbg (self, "loading model...");
    mm_base_modem_at_sequence (
        MM_BASE_MODEM (self),
        models,
        NULL, /* response_processor_context */
        NULL, /* response_processor_context_free */
        callback,
        user_data);
}

/*****************************************************************************/
/* Revision loading */

static gchar *
modem_load_revision_finish (MMIfaceModem *self,
                            GAsyncResult *res,
                            GError **error)
{
    GVariant *result;
    gchar *revision = NULL;

    result = mm_base_modem_at_sequence_finish (MM_BASE_MODEM (self), res, NULL, error);
    if (result) {
        revision = sanitize_info_reply (result, "GMR:");
        mm_obj_dbg (self, "loaded revision: %s", revision);
    }
    return revision;
}

static const MMBaseModemAtCommand revisions[] = {
    { "+CGMR",  3, TRUE, mm_base_modem_response_processor_string_ignore_at_errors },
    { "+GMR",   3, TRUE, mm_base_modem_response_processor_string_ignore_at_errors },
    { NULL }
};

static void
modem_load_revision (MMIfaceModem *self,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    mm_obj_dbg (self, "loading revision...");
    mm_base_modem_at_sequence (
        MM_BASE_MODEM (self),
        revisions,
        NULL, /* response_processor_context */
        NULL, /* response_processor_context_free */
        callback,
        user_data);
}

/*****************************************************************************/
/* Equipment ID loading (Modem interface) */

static gchar *
modem_load_equipment_identifier_finish (MMIfaceModem *self,
                                        GAsyncResult *res,
                                        GError **error)
{
    GVariant *result;
    gchar *equip_id = NULL, *esn = NULL, *meid = NULL, *imei = NULL;

    result = mm_base_modem_at_sequence_finish (MM_BASE_MODEM (self), res, NULL, error);
    if (result) {
        equip_id = sanitize_info_reply (result, "GSN:");

        /* Modems put all sorts of things into the GSN response; sanitize it */
        if (mm_parse_gsn (equip_id, &imei, &meid, &esn)) {
            g_clear_pointer (&equip_id, g_free);

            if (imei)
                equip_id = g_strdup (imei);
            else if (meid)
                equip_id = g_strdup (meid);
            else if (esn)
                equip_id = g_strdup (esn);
            else
                g_assert_not_reached ();

            g_free (esn);
            g_free (meid);
            g_free (imei);
        } else {
            /* Leave whatever the modem returned alone */
        }
        mm_obj_dbg (self, "loaded equipment identifier: %s", equip_id);
    }
    return equip_id;
}

static const MMBaseModemAtCommand equipment_identifiers[] = {
    { "+CGSN",  3, TRUE, mm_base_modem_response_processor_string_ignore_at_errors },
    { "+GSN",   3, TRUE, mm_base_modem_response_processor_string_ignore_at_errors },
    { NULL }
};

static void
modem_load_equipment_identifier (MMIfaceModem *self,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
    const MMBaseModemAtCommand *commands = equipment_identifiers;

    mm_obj_dbg (self, "loading equipment identifier...");

    /* On CDMA-only (non-3GPP) modems, just try +GSN */
    if (mm_iface_modem_is_cdma_only (self))
        commands++;

    mm_base_modem_at_sequence (
        MM_BASE_MODEM (self),
        commands,
        NULL, /* response_processor_context */
        NULL, /* response_processor_context_free */
        callback,
        user_data);
}

/*****************************************************************************/
/* Device identifier loading (Modem interface) */

typedef struct {
    gchar *ati;
    gchar *ati1;
} DeviceIdentifierContext;

static void
device_identifier_context_free (DeviceIdentifierContext *ctx)
{
    g_free (ctx->ati);
    g_free (ctx->ati1);
    g_free (ctx);
}

static gchar *
modem_load_device_identifier_finish (MMIfaceModem *self,
                                     GAsyncResult *res,
                                     GError **error)
{
    GError   *inner_error = NULL;
    gpointer  ctx = NULL;
    gchar    *device_identifier;

    mm_base_modem_at_sequence_finish (MM_BASE_MODEM (self), res, &ctx, &inner_error);
    if (inner_error) {
        g_propagate_error (error, inner_error);
        return NULL;
    }

    g_assert (ctx != NULL);
    device_identifier = mm_broadband_modem_create_device_identifier (
                            MM_BROADBAND_MODEM (self),
                            ((DeviceIdentifierContext *)ctx)->ati,
                            ((DeviceIdentifierContext *)ctx)->ati1,
                            &inner_error);
    if (!device_identifier) {
        g_propagate_error (error, inner_error);
        return NULL;
    }

    mm_obj_dbg (self, "loaded device identifier: %s", device_identifier);
    return device_identifier;
}

static gboolean
parse_ati_reply (MMBaseModem *self,
                 DeviceIdentifierContext *ctx,
                 const gchar *command,
                 const gchar *response,
                 gboolean last_command,
                 const GError *error,
                 GVariant **result,
                 GError **result_error)
{
    /* Store the proper string in the proper place */
    if (!error) {
        if (g_str_equal (command, "ATI1"))
            ctx->ati1 = g_strdup (response);
        else
            ctx->ati = g_strdup (response);
    }

    /* Always keep on, this is a sequence where all the steps should be taken */
    return TRUE;
}

static const MMBaseModemAtCommand device_identifier_steps[] = {
    { "ATI",  3, TRUE, (MMBaseModemAtResponseProcessor)parse_ati_reply },
    { "ATI1", 3, TRUE, (MMBaseModemAtResponseProcessor)parse_ati_reply },
    { NULL }
};

static void
modem_load_device_identifier (MMIfaceModem *self,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
    mm_obj_dbg (self, "loading device identifier...");
    mm_base_modem_at_sequence (
        MM_BASE_MODEM (self),
        device_identifier_steps,
        g_new0 (DeviceIdentifierContext, 1),
        (GDestroyNotify)device_identifier_context_free,
        callback,
        user_data);
}

/*****************************************************************************/
/* Load own numbers (Modem interface) */

typedef struct {
    MMPortSerialQcdm *qcdm;
} OwnNumbersContext;

static void
own_numbers_context_free (OwnNumbersContext *ctx)
{
    if (ctx->qcdm) {
        mm_port_serial_close (MM_PORT_SERIAL (ctx->qcdm));
        g_object_unref (ctx->qcdm);
    }
    g_free (ctx);
}

static GStrv
modem_load_own_numbers_finish (MMIfaceModem *self,
                               GAsyncResult *res,
                               GError **error)
{
    return g_task_propagate_pointer (G_TASK (res), error);
}

static void
mdn_qcdm_ready (MMPortSerialQcdm *port,
                GAsyncResult *res,
                GTask *task)
{
    QcdmResult *result;
    gint err = QCDM_SUCCESS;
    const char *numbers[2] = { NULL, NULL };
    GByteArray *response;
    GError *error = NULL;

    response = mm_port_serial_qcdm_command_finish (port, res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Parse the response */
    result = qcdm_cmd_nv_get_mdn_result ((const gchar *) response->data,
                                         response->len,
                                         &err);
    g_byte_array_unref (response);
    if (!result) {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Failed to parse NV MDN command result: %d",
                                 err);
        g_object_unref (task);
        return;
    }

    if (qcdm_result_get_string (result, QCDM_CMD_NV_GET_MDN_ITEM_MDN, &numbers[0]) >= 0) {
        gboolean valid = TRUE;
        const char *p = numbers[0];

        /* Returned NV item data is read directly out of NV memory on the card,
         * so minimally verify it.
         */
        if (strlen (numbers[0]) < 6 || strlen (numbers[0]) > 15)
            valid = FALSE;

        /* MDN is always decimal digits; allow + for good measure */
        while (p && *p && valid)
            valid = g_ascii_isdigit (*p++) || (*p == '+');

        if (valid) {
            g_task_return_pointer (task,
                                   g_strdupv ((gchar **) numbers),
                                   (GDestroyNotify)g_strfreev);
        } else {
            g_task_return_new_error (task,
                                     MM_CORE_ERROR,
                                     MM_CORE_ERROR_FAILED,
                                     "%s",
                                     "MDN from NV memory appears invalid");
        }
    } else {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "%s",
                                 "Failed retrieve MDN");
    }
    g_object_unref (task);

    qcdm_result_unref (result);
}

static MMBaseModemAtResponseProcessorResult
modem_load_own_numbers_continue_on_sim_busy (MMBaseModem   *self,
                                             gpointer       none,
                                             const gchar   *command,
                                             const gchar   *response,
                                             gboolean       last_command,
                                             const GError  *error,
                                             GVariant     **result,
                                             GError       **result_error)
{
    if (error) {
        *result = NULL;

        if (!g_error_matches (error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_BUSY) || last_command) {
            *result_error = g_error_copy (error);
            return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_FAILURE;
        }

        /* Retry on SIM BUSY errors */
        *result_error = NULL;
        return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_CONTINUE;
    }

    *result = g_variant_new_string (response);
    return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_SUCCESS;
}

static const MMBaseModemAtCommand own_numbers_sequence[] = {
    { "+CNUM", 3, FALSE, modem_load_own_numbers_continue_on_sim_busy, 0 },
    { "+CNUM", 3, FALSE, modem_load_own_numbers_continue_on_sim_busy, 3 },
    { "+CNUM", 3, FALSE, modem_load_own_numbers_continue_on_sim_busy, 3 },
    { NULL }
};

static void
modem_load_own_numbers_ready (MMIfaceModem *self,
                             GAsyncResult *res,
                             GTask *task)
{
    OwnNumbersContext *ctx;
    GVariant *result;
    GError *error = NULL;
    GStrv numbers;

    ctx = g_task_get_task_data (task);

    result = mm_base_modem_at_sequence_finish (MM_BASE_MODEM (self), res, NULL, &error);
    if (!result) {
        /* try QCDM */
        if (ctx->qcdm) {
            GByteArray *mdn;

            g_clear_error (&error);

            mdn = g_byte_array_sized_new (200);
            mdn->len = qcdm_cmd_nv_get_mdn_new ((char *) mdn->data, 200, 0);
            g_assert (mdn->len);

            mm_port_serial_qcdm_command (ctx->qcdm,
                                         mdn,
                                         3,
                                         NULL,
                                         (GAsyncReadyCallback)mdn_qcdm_ready,
                                         task);
            g_byte_array_unref (mdn);
            return;
        }
        g_task_return_error (task, error);
    } else {
        numbers = mm_3gpp_parse_cnum_exec_response (g_variant_get_string (result, NULL));
        g_task_return_pointer (task, numbers, (GDestroyNotify)g_strfreev);
    }
    g_object_unref (task);
}

static void
modem_load_own_numbers (MMIfaceModem *self,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
    OwnNumbersContext *ctx;
    GError *error = NULL;
    GTask *task;

    ctx = g_new0 (OwnNumbersContext, 1);
    ctx->qcdm = mm_base_modem_peek_port_qcdm (MM_BASE_MODEM (self));
    if (ctx->qcdm) {
        if (mm_port_serial_open (MM_PORT_SERIAL (ctx->qcdm), &error)) {
            ctx->qcdm = g_object_ref (ctx->qcdm);
        } else {
            mm_obj_dbg (self, "couldn't open QCDM port: (%d) %s",
                        error ? error->code : -1,
                        error ? error->message : "(unknown)");
            ctx->qcdm = NULL;
        }
    }

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, (GDestroyNotify)own_numbers_context_free);

    mm_obj_dbg (self, "loading own numbers...");
    mm_base_modem_at_sequence (
        MM_BASE_MODEM (self),
        own_numbers_sequence,
        NULL, /* response_processor_context */
        NULL, /* response_processor_context_free */
        (GAsyncReadyCallback)modem_load_own_numbers_ready,
        task);
}

/*****************************************************************************/
/* Check if unlock required (Modem interface) */

typedef struct {
    const gchar *result;
    MMModemLock code;
} CPinResult;

static CPinResult unlock_results[] = {
    /* Longer entries first so we catch the correct one with strcmp() */
    { "READY",         MM_MODEM_LOCK_NONE           },
    { "SIM PIN2",      MM_MODEM_LOCK_SIM_PIN2       },
    { "SIM PUK2",      MM_MODEM_LOCK_SIM_PUK2       },
    { "SIM PIN",       MM_MODEM_LOCK_SIM_PIN        },
    { "SIM PUK",       MM_MODEM_LOCK_SIM_PUK        },
    { "PH-NETSUB PIN", MM_MODEM_LOCK_PH_NETSUB_PIN  },
    { "PH-NETSUB PUK", MM_MODEM_LOCK_PH_NETSUB_PUK  },
    { "PH-FSIM PIN",   MM_MODEM_LOCK_PH_FSIM_PIN    },
    { "PH-FSIM PUK",   MM_MODEM_LOCK_PH_FSIM_PUK    },
    { "PH-CORP PIN",   MM_MODEM_LOCK_PH_CORP_PIN    },
    { "PH-CORP PUK",   MM_MODEM_LOCK_PH_CORP_PUK    },
    { "PH-SIM PIN",    MM_MODEM_LOCK_PH_SIM_PIN     },
    { "PH-NET PIN",    MM_MODEM_LOCK_PH_NET_PIN     },
    { "PH-NET PUK",    MM_MODEM_LOCK_PH_NET_PUK     },
    { "PH-SP PIN",     MM_MODEM_LOCK_PH_SP_PIN      },
    { "PH-SP PUK",     MM_MODEM_LOCK_PH_SP_PUK      },
    { NULL }
};

static MMModemLock
modem_load_unlock_required_finish (MMIfaceModem *self,
                                   GAsyncResult *res,
                                   GError **error)
{
    GError *inner_error = NULL;
    gssize value;

    value = g_task_propagate_int (G_TASK (res), &inner_error);
    if (inner_error) {
        g_propagate_error (error, inner_error);
        return MM_MODEM_LOCK_UNKNOWN;
    }
    return (MMModemLock)value;
}

static void
cpin_query_ready (MMIfaceModem *self,
                  GAsyncResult *res,
                  GTask *task)
{

    MMModemLock lock = MM_MODEM_LOCK_UNKNOWN;
    const gchar *result;
    GError *error = NULL;

    result = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    if (result &&
        strstr (result, "+CPIN:")) {
        CPinResult *iter = &unlock_results[0];
        const gchar *str;

        str = strstr (result, "+CPIN:") + 6;
        /* Skip possible whitespaces after '+CPIN:' and before the response */
        while (*str == ' ')
            str++;

        /* Some phones (Motorola EZX models) seem to quote the response */
        if (str[0] == '"')
            str++;

        /* Translate the reply */
        while (iter->result) {
            if (g_str_has_prefix (str, iter->result)) {
                lock = iter->code;
                break;
            }
            iter++;
        }
    }

    g_task_return_int (task, lock);
    g_object_unref (task);
}

static void
modem_load_unlock_required (MMIfaceModem        *self,
                            gboolean             last_attempt,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
    GTask *task;

    task = g_task_new (self, cancellable, callback, user_data);

    /* CDMA-only modems don't need this */
    if (mm_iface_modem_is_cdma_only (self)) {
        mm_obj_dbg (self, "skipping unlock check in CDMA-only modem...");
        g_task_return_int (task, MM_MODEM_LOCK_NONE);
        g_object_unref (task);
        return;
    }

    mm_obj_dbg (self, "checking if unlock required...");
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CPIN?",
                              10,
                              FALSE,
                              (GAsyncReadyCallback)cpin_query_ready,
                              task);
}

/*****************************************************************************/
/* Supported modes loading (Modem interface) */

typedef struct {
    MMUnlockRetries *retries;
    guint            i;
} LoadUnlockRetriesContext;

typedef struct {
    MMModemLock  lock;
    const gchar *command;
} UnlockRetriesMap;

static const UnlockRetriesMap unlock_retries_map [] = {
    { MM_MODEM_LOCK_SIM_PIN,     "+CSIM=10,\"0020000100\"" },
    { MM_MODEM_LOCK_SIM_PUK,     "+CSIM=10,\"002C000100\"" },
    { MM_MODEM_LOCK_SIM_PIN2,    "+CSIM=10,\"0020008100\"" },
    { MM_MODEM_LOCK_SIM_PUK2,    "+CSIM=10,\"002C008100\"" },
};

static void
load_unlock_retries_context_free (LoadUnlockRetriesContext *ctx)
{
    g_object_unref (ctx->retries);
    g_slice_free (LoadUnlockRetriesContext, ctx);
}

static MMUnlockRetries *
load_unlock_retries_finish (MMIfaceModem  *self,
                            GAsyncResult  *res,
                            GError       **error)
{
    return g_task_propagate_pointer (G_TASK (res), error);
}

static void load_unlock_retries_context_step (GTask *task);

static void
csim_ready (MMBaseModem  *self,
            GAsyncResult *res,
            GTask        *task)
{
    LoadUnlockRetriesContext *ctx;
    const gchar              *response;
    GError                   *error = NULL;
    gint                      val;

    ctx = g_task_get_task_data (task);

    response = mm_base_modem_at_command_finish (self, res, &error);
    if (!response) {
        mm_obj_dbg (self, "couldn't load retry count for lock '%s': %s",
                    mm_modem_lock_get_string (unlock_retries_map[ctx->i].lock),
                    error->message);
        goto next;
    }

    val = mm_parse_csim_response (response, &error);
    if (val < 0) {
        mm_obj_dbg (self, "couldn't parse retry count value for lock '%s': %s",
                    mm_modem_lock_get_string (unlock_retries_map[ctx->i].lock),
                    error->message);
        goto next;
    }

    mm_unlock_retries_set (ctx->retries, unlock_retries_map[ctx->i].lock, val);

next:
    g_clear_error (&error);

    /* Go to next lock value */
    ctx->i++;
    load_unlock_retries_context_step (task);
}

static void
load_unlock_retries_context_step (GTask *task)
{
    MMBroadbandModem *self;
    LoadUnlockRetriesContext  *ctx;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    if (ctx->i == G_N_ELEMENTS (unlock_retries_map)) {
        g_task_return_pointer (task, g_object_ref (ctx->retries), g_object_unref);
        g_object_unref (task);
        return;
    }

    mm_base_modem_at_command (
        MM_BASE_MODEM (self),
        unlock_retries_map[ctx->i].command,
        3,
        FALSE,
        (GAsyncReadyCallback)csim_ready,
        task);
}

static void
load_unlock_retries (MMIfaceModem        *self,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
    GTask                    *task;
    LoadUnlockRetriesContext *ctx;

    task = g_task_new (self, NULL, callback, user_data);

    ctx = g_slice_new0 (LoadUnlockRetriesContext);
    ctx->retries = mm_unlock_retries_new ();
    ctx->i = 0;
    g_task_set_task_data (task, ctx, (GDestroyNotify)load_unlock_retries_context_free);

    load_unlock_retries_context_step (task);
}

/*****************************************************************************/
/* Supported modes loading (Modem interface) */

typedef struct {
    MMModemMode mode;
    gboolean run_cnti;
    gboolean run_ws46;
    gboolean run_gcap;
    gboolean run_cereg;
    gboolean run_c5greg;
} LoadSupportedModesContext;

static GArray *
modem_load_supported_modes_finish (MMIfaceModem *self,
                                   GAsyncResult *res,
                                   GError **error)
{
    GError *inner_error = NULL;
    gssize value;
    GArray *modes;
    MMModemModeCombination mode;

    value = g_task_propagate_int (G_TASK (res), &inner_error);
    if (inner_error) {
        g_propagate_error (error, inner_error);
        return NULL;
    }

    /* Build a mask with all supported modes */
    modes = g_array_sized_new (FALSE, FALSE, sizeof (MMModemModeCombination), 1);
    mode.allowed = (MMModemMode) value;
    mode.preferred = MM_MODEM_MODE_NONE;
    g_array_append_val (modes, mode);

    return modes;
}

static void load_supported_modes_step (GTask *task);

static void
supported_modes_c5greg_ready (MMBaseModem *_self,
                              GAsyncResult *res,
                              GTask *task)
{
    LoadSupportedModesContext *ctx;
    g_autoptr(GError)          error = NULL;

    ctx = g_task_get_task_data (task);

    mm_base_modem_at_command_finish (_self, res, &error);
    if (!error)
        ctx->mode |= MM_MODEM_MODE_5G;

    ctx->run_c5greg = FALSE;
    load_supported_modes_step (task);
}

static void
supported_modes_cereg_ready (MMBaseModem *_self,
                             GAsyncResult *res,
                             GTask *task)
{
    LoadSupportedModesContext *ctx;
    g_autoptr(GError)          error = NULL;

    ctx = g_task_get_task_data (task);

    mm_base_modem_at_command_finish (_self, res, &error);
    if (!error)
        ctx->mode |= MM_MODEM_MODE_4G;

    ctx->run_cereg = FALSE;
    load_supported_modes_step (task);
}

static void
supported_modes_gcap_ready (MMBaseModem *_self,
                            GAsyncResult *res,
                            GTask *task)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    LoadSupportedModesContext *ctx;
    const gchar *response;
    GError *error = NULL;

    ctx = g_task_get_task_data (task);

    response = mm_base_modem_at_command_finish (_self, res, &error);
    if (!error) {
        MMModemMode mode = MM_MODEM_MODE_NONE;

        if (strstr (response, "IS")) {
            /* IS-856 is the EV-DO family */
            if (strstr (response, "856")) {
                if (!self->priv->modem_cdma_evdo_network_supported) {
                    self->priv->modem_cdma_evdo_network_supported = TRUE;
                    g_object_notify (G_OBJECT (self), MM_IFACE_MODEM_CDMA_EVDO_NETWORK_SUPPORTED);
                }
                mm_obj_dbg (self, "device allows (CDMA) 3G network mode");
                mode |= MM_MODEM_MODE_3G;
            }
            /* IS-707 is the 1xRTT family, which we consider as 2G */
            if (strstr (response, "707")) {
                if (!self->priv->modem_cdma_cdma1x_network_supported) {
                    self->priv->modem_cdma_cdma1x_network_supported = TRUE;
                    g_object_notify (G_OBJECT (self), MM_IFACE_MODEM_CDMA_CDMA1X_NETWORK_SUPPORTED);
                }
                mm_obj_dbg (self, "device allows (CDMA) 2G network mode");
                mode |= MM_MODEM_MODE_2G;
            }
        }

        /* If no expected mode found, error */
        if (mode == MM_MODEM_MODE_NONE) {
            /* This should really never happen in the default implementation. */
            error = g_error_new (MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Couldn't find specific CDMA mode in capabilities string: '%s'",
                                 response);
        } else {
            /* Keep our results */
            ctx->mode |= mode;
        }
    }

    if (error) {
        mm_obj_dbg (self, "generic query of supported CDMA networks failed: '%s'", error->message);
        g_error_free (error);

        /* Use defaults */
        if (self->priv->modem_cdma_cdma1x_network_supported) {
            mm_obj_dbg (self, "assuming device allows (CDMA) 2G network mode");
            ctx->mode |= MM_MODEM_MODE_2G;
        }
        if (self->priv->modem_cdma_evdo_network_supported) {
            mm_obj_dbg (self, "assuming device allows (CDMA) 3G network mode");
            ctx->mode |= MM_MODEM_MODE_3G;
        }
    }

    /* Now keep on with the loading, we're probably finishing now */
    ctx->run_gcap = FALSE;
    load_supported_modes_step (task);
}

static void
supported_modes_ws46_test_ready (MMBroadbandModem *self,
                                 GAsyncResult *res,
                                 GTask *task)
{
    LoadSupportedModesContext *ctx;
    const gchar *response;
    GArray      *modes;
    GError      *error = NULL;
    guint        i;

    ctx = g_task_get_task_data (task);

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        mm_obj_dbg (self, "generic query of supported 3GPP networks with WS46=? failed: '%s'", error->message);
        g_error_free (error);
        goto out;
    }

    modes = mm_3gpp_parse_ws46_test_response (response, self, &error);
    if (!modes) {
        mm_obj_dbg (self, "parsing WS46=? response failed: '%s'", error->message);
        g_error_free (error);
        goto out;
    }

    for (i = 0; i < modes->len; i++) {
        MMModemMode  mode;
        gchar       *str;

        mode = g_array_index (modes, MMModemMode, i);

        ctx->mode |= mode;

        str = mm_modem_mode_build_string_from_mask (mode);
        mm_obj_dbg (self, "device allows (3GPP) mode combination: %s", str);
        g_free (str);
    }

    g_array_unref (modes);

out:
    /* Now keep on with the loading, we may need CDMA-specific checks */
    ctx->run_ws46 = FALSE;
    load_supported_modes_step (task);
}

static void
supported_modes_cnti_ready (MMBroadbandModem *self,
                            GAsyncResult *res,
                            GTask *task)
{
    LoadSupportedModesContext *ctx;
    const gchar *response;
    GError *error = NULL;

    ctx = g_task_get_task_data (task);

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (!error) {
        MMModemMode mode = MM_MODEM_MODE_NONE;
        gchar *lower;

        lower = g_ascii_strdown (response, -1);

        if (g_strstr_len (lower, -1, "gsm") ||
            g_strstr_len (lower, -1, "gprs") ||
            g_strstr_len (lower, -1, "edge")) {
            mm_obj_dbg (self, "device allows (3GPP) 2G networks");
            mode |= MM_MODEM_MODE_2G;
        }

        if (g_strstr_len (lower, -1, "umts") ||
            g_strstr_len (lower, -1, "hsdpa") ||
            g_strstr_len (lower, -1, "hsupa") ||
            g_strstr_len (lower, -1, "hspa+")) {
            mm_obj_dbg (self, "device allows (3GPP) 3G networks");
            mode |= MM_MODEM_MODE_3G;
        }

        if (g_strstr_len (lower, -1, "lte")) {
            mm_obj_dbg (self, "device allows (3GPP) 4G networks");
            mode |= MM_MODEM_MODE_4G;
        }

        g_free (lower);

        /* If no expected ID found, log error */
        if (mode == MM_MODEM_MODE_NONE)
            mm_obj_dbg (self, "invalid list of supported networks reported by *CNTI: '%s'", response);
        else
            ctx->mode |= mode;
    } else {
        mm_obj_dbg (self, "generic query of supported 3GPP networks with *CNTI failed: '%s'", error->message);
        g_error_free (error);
    }

    /* Now keep on with the loading */
    ctx->run_cnti = FALSE;
    load_supported_modes_step (task);
}

static void
load_supported_modes_step (GTask *task)
{
    MMBroadbandModem *self;
    LoadSupportedModesContext *ctx;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    if (ctx->run_cnti) {
        mm_base_modem_at_command (
            MM_BASE_MODEM (self),
            "*CNTI=2",
            3,
            FALSE,
            (GAsyncReadyCallback)supported_modes_cnti_ready,
            task);
        return;
    }

    if (ctx->run_ws46) {
        mm_base_modem_at_command (
            MM_BASE_MODEM (self),
            "+WS46=?",
            3,
            TRUE, /* allow caching, it's a test command */
            (GAsyncReadyCallback)supported_modes_ws46_test_ready,
            task);
        return;
    }

    if (ctx->run_gcap) {
        mm_base_modem_at_command (
            MM_BASE_MODEM (self),
            "+GCAP",
            3,
            TRUE, /* allow caching */
            (GAsyncReadyCallback)supported_modes_gcap_ready,
            task);
        return;
    }

    if (ctx->run_cereg) {
        mm_base_modem_at_command (
            MM_BASE_MODEM (self),
            "+CEREG=?",
            3,
            TRUE, /* allow caching */
            (GAsyncReadyCallback)supported_modes_cereg_ready,
            task);
        return;
    }

    if (ctx->run_c5greg) {
        mm_base_modem_at_command (
            MM_BASE_MODEM (self),
            "+C5GREG=?",
            3,
            TRUE, /* allow caching */
            (GAsyncReadyCallback)supported_modes_c5greg_ready,
            task);
        return;
    }

    /* All done.
     * If no mode found, error */
    if (ctx->mode == MM_MODEM_MODE_NONE)
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Couldn't retrieve supported modes");
    else
        g_task_return_int (task, ctx->mode);

    g_object_unref (task);
}

static void
modem_load_supported_modes (MMIfaceModem *self,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
    LoadSupportedModesContext *ctx;
    GTask *task;

    mm_obj_dbg (self, "loading supported modes...");
    ctx = g_new0 (LoadSupportedModesContext, 1);
    ctx->mode = MM_MODEM_MODE_NONE;

    if (mm_iface_modem_is_3gpp (self)) {
        /* Run +WS46=? and *CNTI=2 */
        ctx->run_ws46 = TRUE;
        ctx->run_cnti = TRUE;
        ctx->run_cereg = TRUE;
        ctx->run_c5greg = TRUE;
    }

    if (mm_iface_modem_is_cdma (self)) {
        /* Run +GCAP in order to know if the modem is CDMA1x only or CDMA1x/EV-DO */
        ctx->run_gcap = TRUE;
    }

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, g_free);

    load_supported_modes_step (task);
}

/*****************************************************************************/
/* Supported IP families loading (Modem interface) */

static MMBearerIpFamily
modem_load_supported_ip_families_finish (MMIfaceModem *self,
                                         GAsyncResult *res,
                                         GError **error)
{
    GError *inner_error = NULL;
    gssize value;

    value = g_task_propagate_int (G_TASK (res), &inner_error);
    if (inner_error) {
        g_propagate_error (error, inner_error);
        return MM_BEARER_IP_FAMILY_NONE;
    }
    return (MMBearerIpFamily)value;
}

static void
supported_ip_families_cgdcont_test_ready (MMBaseModem *self,
                                          GAsyncResult *res,
                                          GTask *task)
{
    const gchar *response;
    GError *error = NULL;
    MMBearerIpFamily mask = MM_BEARER_IP_FAMILY_NONE;

    response = mm_base_modem_at_command_finish (self, res, &error);
    if (response) {
        GList *formats, *l;

        formats = mm_3gpp_parse_cgdcont_test_response (response, self, &error);
        for (l = formats; l; l = g_list_next (l))
            mask |= ((MM3gppPdpContextFormat *)(l->data))->pdp_type;

        mm_3gpp_pdp_context_format_list_free (formats);
    }

    if (error)
        g_task_return_error (task, error);
    else
        g_task_return_int (task, mask);

    g_object_unref (task);
}

static void
modem_load_supported_ip_families (MMIfaceModem *self,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
    GTask *task;

    mm_obj_dbg (self, "loading supported IP families...");
    task = g_task_new (self, NULL, callback, user_data);

    if (mm_iface_modem_is_cdma_only (self)) {
        g_task_return_int (task, MM_BEARER_IP_FAMILY_IPV4);
        g_object_unref (task);
        return;
    }

    /* Query with CGDCONT=? */
    mm_base_modem_at_command (
        MM_BASE_MODEM (self),
        "+CGDCONT=?",
        3,
        TRUE, /* allow caching, it's a test command */
        (GAsyncReadyCallback)supported_ip_families_cgdcont_test_ready,
        task);
}

/*****************************************************************************/
/* Signal quality loading (Modem interface) */

static void
qcdm_evdo_pilot_sets_log_handle (MMPortSerialQcdm *port,
                                 GByteArray *log_buffer,
                                 gpointer user_data)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (user_data);
    QcdmResult *result;
    uint32_t num_active = 0;
    uint32_t pilot_pn = 0;
    uint32_t pilot_energy = 0;
    int32_t rssi_dbm = 0;

    result = qcdm_log_item_evdo_pilot_sets_v2_new ((const char *) log_buffer->data,
                                                   log_buffer->len,
                                                   NULL);
    if (!result)
        return;

    if (!qcdm_log_item_evdo_pilot_sets_v2_get_num (result,
                                                   QCDM_LOG_ITEM_EVDO_PILOT_SETS_V2_TYPE_ACTIVE,
                                                   &num_active)) {
        qcdm_result_unref (result);
        return;
    }

    if (num_active > 0 &&
        qcdm_log_item_evdo_pilot_sets_v2_get_pilot (result,
                                                    QCDM_LOG_ITEM_EVDO_PILOT_SETS_V2_TYPE_ACTIVE,
                                                    0,
                                                    &pilot_pn,
                                                    &pilot_energy,
                                                    &rssi_dbm)) {
        mm_obj_dbg (self, "EVDO active pilot RSSI: %ddBm", rssi_dbm);
        self->priv->evdo_pilot_rssi = rssi_dbm;
    }

    qcdm_result_unref (result);
}

typedef struct {
    MMIfacePortAt *at_port;
    MMPortSerial  *qcdm_port;
} SignalQualityContext;

static void
signal_quality_context_free (SignalQualityContext *ctx)
{
    g_clear_object (&ctx->at_port);
    if (ctx->qcdm_port) {
        mm_port_serial_close (ctx->qcdm_port);
        g_object_unref (ctx->qcdm_port);
    }
    g_free (ctx);
}

static guint
modem_load_signal_quality_finish (MMIfaceModem *self,
                                  GAsyncResult *res,
                                  GError **error)
{
    gssize value;

    value = g_task_propagate_int (G_TASK (res), error);
    return value < 0 ? 0 : value;
}

static guint
signal_quality_evdo_pilot_sets (MMBroadbandModem *self)
{
    if (self->priv->modem_cdma_evdo_registration_state == MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN)
        return 0;

    if (self->priv->evdo_pilot_rssi >= 0)
        return 0;

    return MM_RSSI_TO_QUALITY (self->priv->evdo_pilot_rssi);
}

static void
signal_quality_csq_ready (MMBroadbandModem *self,
                          GAsyncResult *res,
                          GTask *task)
{
    GError *error = NULL;
    GVariant *result;
    const gchar *result_str;

    result = mm_base_modem_at_sequence_full_finish (MM_BASE_MODEM (self), res, NULL, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    result_str = g_variant_get_string (result, NULL);
    if (result_str) {
        /* Got valid reply */
        int quality;
        int ber;

        result_str = mm_strip_tag (result_str, "+CSQ:");
        if (sscanf (result_str, "%d, %d", &quality, &ber)) {
            if (quality == 99) {
                /* 99 can mean unknown, no service, etc.  But the modem may
                 * also only report CDMA 1x quality in CSQ, so try EVDO via
                 * QCDM log messages too.
                 */
                quality = signal_quality_evdo_pilot_sets (self);
            } else {
                /* Normalize the quality */
                quality = CLAMP (quality, 0, 31) * 100 / 31;
            }
            g_task_return_int (task, quality);
            g_object_unref (task);
            return;
        }
    }

    g_task_return_new_error (task,
                             MM_CORE_ERROR,
                             MM_CORE_ERROR_FAILED,
                             "Could not parse signal quality results");
    g_object_unref (task);
}

/* Some modems want +CSQ, others want +CSQ?, and some of both types
 * will return ERROR if they don't get the command they want.  So
 * try the other command if the first one fails.
 */
static const MMBaseModemAtCommand signal_quality_csq_sequence[] = {
    { "+CSQ",  3, FALSE, mm_base_modem_response_processor_string_ignore_at_errors },
    { "+CSQ?", 3, FALSE, mm_base_modem_response_processor_string_ignore_at_errors },
    { NULL }
};

static void
signal_quality_csq (GTask *task)
{
    MMBroadbandModem *self;
    SignalQualityContext *ctx;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    mm_base_modem_at_sequence_full (
        MM_BASE_MODEM (self),
        ctx->at_port,
        signal_quality_csq_sequence,
        NULL, /* response_processor_context */
        NULL, /* response_processor_context_free */
        NULL, /* cancellable */
        (GAsyncReadyCallback)signal_quality_csq_ready,
        task);
}

static guint
normalize_ciev_cind_signal_quality (guint quality,
                                    guint min,
                                    guint max)
{
    if (!max) {
        /* If we didn't get a max, assume it was 5. Note that we do allow
         * 0, meaning no signal at all. */
        return (quality <= 5) ? (quality * 20) : 100;
    }

    if (quality >= min &&
        quality <= max)
        return ((100 * (quality - min)) / (max - min));

    /* Value out of range, assume no signal here. Some modems (Cinterion
     * for example) will send out-of-range values when they cannot get
     * the signal strength. */
    return 0;
}

static void
signal_quality_cind_ready (MMBroadbandModem *self,
                           GAsyncResult *res,
                           GTask *task)
{
    GError *error = NULL;
    const gchar *result;
    GByteArray *indicators;
    guint quality = 0;

    result = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        g_clear_error (&error);
        goto try_csq;
    }

    indicators = mm_3gpp_parse_cind_read_response (result, &error);
    if (!indicators) {
        mm_obj_dbg (self, "could not parse CIND signal quality results: %s", error->message);
        g_clear_error (&error);
        goto try_csq;
    }

    if (indicators->len < self->priv->modem_cind_indicator_signal_quality) {
        mm_obj_dbg (self,
                    "could not parse CIND signal quality results; signal "
                    "index (%u) outside received range (0-%u)",
                    self->priv->modem_cind_indicator_signal_quality,
                    indicators->len);
    } else {
        quality = g_array_index (indicators,
                                 guint8,
                                 self->priv->modem_cind_indicator_signal_quality);
        quality = normalize_ciev_cind_signal_quality (quality,
                                                      self->priv->modem_cind_min_signal_quality,
                                                      self->priv->modem_cind_max_signal_quality);
    }
    g_byte_array_free (indicators, TRUE);

    if (quality > 0) {
        /* +CIND success */
        g_task_return_int (task, quality);
        g_object_unref (task);
        return;
    }

try_csq:
    /* Always fall back to +CSQ if for whatever reason +CIND failed.  Also,
     * some QMI-based devices say they support signal via CIND, but always
     * report zero even though they have signal.  So if we get zero signal
     * from +CIND, try CSQ too.  (bgo #636040)
     */
    signal_quality_csq (task);
}

static void
signal_quality_cind (GTask *task)
{
    MMBroadbandModem *self;
    SignalQualityContext *ctx;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    mm_base_modem_at_command_full (MM_BASE_MODEM (self),
                                   ctx->at_port,
                                   "+CIND?",
                                   5,
                                   FALSE,
                                   FALSE, /* raw */
                                   NULL, /* cancellable */
                                   (GAsyncReadyCallback)signal_quality_cind_ready,
                                   task);
}

static void
signal_quality_qcdm_ready (MMPortSerialQcdm *port,
                           GAsyncResult *res,
                           GTask *task)
{
    QcdmResult *result;
    guint32 num = 0, quality = 0, i;
    gfloat best_db = -28;
    gint err = QCDM_SUCCESS;
    GByteArray *response;
    GError *error = NULL;

    response = mm_port_serial_qcdm_command_finish (port, res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Parse the response */
    result = qcdm_cmd_pilot_sets_result ((const gchar *) response->data,
                                         response->len,
                                         &err);
    g_byte_array_unref (response);
    if (!result) {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Failed to parse pilot sets command result: %d",
                                 err);
        g_object_unref (task);
        return;
    }

    qcdm_cmd_pilot_sets_result_get_num (result, QCDM_CMD_PILOT_SETS_TYPE_ACTIVE, &num);
    for (i = 0; i < num; i++) {
        guint32 pn_offset = 0, ecio = 0;
        gfloat db = 0;

        qcdm_cmd_pilot_sets_result_get_pilot (result,
                                              QCDM_CMD_PILOT_SETS_TYPE_ACTIVE,
                                              i,
                                              &pn_offset,
                                              &ecio,
                                              &db);
        best_db = MAX (db, best_db);
    }
    qcdm_result_unref (result);

    if (num > 0) {
        #define BEST_ECIO 3
        #define WORST_ECIO 25

        /* EC/IO dB ranges from roughly 0 to -31 dB.  Lower == worse.  We
         * really only care about -3 to -25 dB though, since that's about what
         * you'll see in real-world usage.
         */
        best_db = CLAMP (ABS (best_db), BEST_ECIO, WORST_ECIO) - BEST_ECIO;
        quality = (guint32) (100 - (best_db * 100 / (WORST_ECIO - BEST_ECIO)));
    }

    g_task_return_int (task, quality);
    g_object_unref (task);
}

static void
signal_quality_qcdm (GTask *task)
{
    MMBroadbandModem *self;
    SignalQualityContext *ctx;
    GByteArray *pilot_sets;
    guint quality;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    /* If EVDO is active try that signal strength first */
    quality = signal_quality_evdo_pilot_sets (self);
    if (quality > 0) {
        g_task_return_int (task, quality);
        g_object_unref (task);
        return;
    }

    /* Use CDMA1x pilot EC/IO if we can */
    pilot_sets = g_byte_array_sized_new (25);
    pilot_sets->len = qcdm_cmd_pilot_sets_new ((char *) pilot_sets->data, 25);
    g_assert (pilot_sets->len);

    mm_port_serial_qcdm_command (MM_PORT_SERIAL_QCDM (ctx->qcdm_port),
                                 pilot_sets,
                                 3,
                                 NULL,
                                 (GAsyncReadyCallback)signal_quality_qcdm_ready,
                                 task);
    g_byte_array_unref (pilot_sets);
}

static void
modem_load_signal_quality (MMIfaceModem *_self,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    SignalQualityContext *ctx;
    GError *error = NULL;
    GTask *task;

    mm_obj_dbg (self, "loading signal quality...");
    ctx = g_new0 (SignalQualityContext, 1);

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, (GDestroyNotify)signal_quality_context_free);

    /* Check whether we can get a non-connected AT port */
    ctx->at_port = mm_base_modem_get_best_at_port (MM_BASE_MODEM (self), &error);
    if (ctx->at_port) {
        if (!self->priv->modem_cind_disabled &&
            self->priv->modem_cind_supported &&
            CIND_INDICATOR_IS_VALID (self->priv->modem_cind_indicator_signal_quality))
            signal_quality_cind (task);
        else
            signal_quality_csq (task);
        return;
    }

    /* If no best AT port available (all connected), try with QCDM ports */
    ctx->qcdm_port = (MMPortSerial *)mm_base_modem_peek_port_qcdm (MM_BASE_MODEM (self));
    if (ctx->qcdm_port) {
        g_clear_error (&error);

        /* Need to open QCDM port as it may be closed/blocked */
        if (mm_port_serial_open (MM_PORT_SERIAL (ctx->qcdm_port), &error)) {
            g_object_ref (ctx->qcdm_port);
            signal_quality_qcdm (task);
            return;
        }

        ctx->qcdm_port = NULL;
        mm_obj_dbg (self, "couldn't open QCDM port: %s", error->message);
    }

    /* Return the error we got when getting best AT port */
    g_task_return_error (task, error);
    g_object_unref (task);
}

/*****************************************************************************/
/* Load access technology (Modem interface) */

typedef struct {
    MMModemAccessTechnology access_technologies;
    guint mask;
} AccessTechAndMask;

static gboolean
modem_load_access_technologies_finish (MMIfaceModem *self,
                                       GAsyncResult *res,
                                       MMModemAccessTechnology *access_technologies,
                                       guint *mask,
                                       GError **error)
{
    AccessTechAndMask *tech;

    tech = g_task_propagate_pointer (G_TASK (res), error);
    if (!tech)
        return FALSE;

    *access_technologies = tech->access_technologies;
    *mask = tech->mask;
    g_free (tech);
    return TRUE;
}

typedef struct {
    MMPortSerialQcdm *port;

    guint32 opmode;
    guint32 sysmode;
    gboolean hybrid;

    gboolean wcdma_open;
    gboolean evdo_open;

    MMModemAccessTechnology fallback_act;
    guint fallback_mask;
} AccessTechContext;

static void
access_tech_context_free (AccessTechContext *ctx)
{
    if (ctx->port) {
        mm_port_serial_close (MM_PORT_SERIAL (ctx->port));
        g_object_unref (ctx->port);
    }
    g_free (ctx);
}

static AccessTechAndMask *
access_tech_and_mask_new (MMBroadbandModem  *self,
                          AccessTechContext *ctx)
{
    AccessTechAndMask *tech;
    MMModemAccessTechnology act = MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN;
    guint mask = MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN;

    if (ctx->fallback_mask) {
        mm_obj_dbg (self, "fallback access technology: 0x%08x", ctx->fallback_act);
        act = ctx->fallback_act;
        mask = ctx->fallback_mask;
        goto done;
    }

    mm_obj_dbg (self, "QCDM operating mode: %d", ctx->opmode);
    mm_obj_dbg (self, "QCDM system mode: %d", ctx->sysmode);
    mm_obj_dbg (self, "QCDM hybrid pref: %d", ctx->hybrid);
    mm_obj_dbg (self, "QCDM WCDMA open: %d", ctx->wcdma_open);
    mm_obj_dbg (self, "QCDM EVDO open: %d", ctx->evdo_open);

    if (ctx->opmode == QCDM_CMD_CM_SUBSYS_STATE_INFO_OPERATING_MODE_ONLINE) {
        switch (ctx->sysmode) {
        case QCDM_CMD_CM_SUBSYS_STATE_INFO_SYSTEM_MODE_CDMA:
            if (!ctx->hybrid || !ctx->evdo_open) {
                act = MM_MODEM_ACCESS_TECHNOLOGY_1XRTT;
                mask = MM_IFACE_MODEM_CDMA_ALL_ACCESS_TECHNOLOGIES_MASK;
                break;
            }
            /* Fall through */
        case QCDM_CMD_CM_SUBSYS_STATE_INFO_SYSTEM_MODE_HDR:
            /* Assume EVDOr0; can't yet determine r0 vs. rA with QCDM */
            if (ctx->evdo_open)
                act = MM_MODEM_ACCESS_TECHNOLOGY_EVDO0;
            mask = MM_IFACE_MODEM_CDMA_ALL_ACCESS_TECHNOLOGIES_MASK;
            break;
        case QCDM_CMD_CM_SUBSYS_STATE_INFO_SYSTEM_MODE_GSM:
        case QCDM_CMD_CM_SUBSYS_STATE_INFO_SYSTEM_MODE_GW:
            if (ctx->wcdma_open) {
                /* Assume UMTS; can't yet determine UMTS/HSxPA/HSPA+ with QCDM */
                act = MM_MODEM_ACCESS_TECHNOLOGY_UMTS;
            } else {
                /* Assume GPRS; can't yet determine GSM/GPRS/EDGE with QCDM */
                act = MM_MODEM_ACCESS_TECHNOLOGY_GPRS;
            }
            mask = MM_IFACE_MODEM_3GPP_ALL_ACCESS_TECHNOLOGIES_MASK;
            break;
        case QCDM_CMD_CM_SUBSYS_STATE_INFO_SYSTEM_MODE_WCDMA:
            if (ctx->wcdma_open) {
                /* Assume UMTS; can't yet determine UMTS/HSxPA/HSPA+ with QCDM */
                act = MM_MODEM_ACCESS_TECHNOLOGY_UMTS;
            }
            mask = MM_IFACE_MODEM_3GPP_ALL_ACCESS_TECHNOLOGIES_MASK;
            break;
        case QCDM_CMD_CM_SUBSYS_STATE_INFO_SYSTEM_MODE_LTE:
            act = MM_MODEM_ACCESS_TECHNOLOGY_LTE;
            mask = MM_IFACE_MODEM_3GPP_ALL_ACCESS_TECHNOLOGIES_MASK;
            break;
        default:
            break;
        }
    }

done:
    tech = g_new0 (AccessTechAndMask, 1);
    tech->access_technologies = act;
    tech->mask = mask;
    return tech;
}

static void
access_tech_qcdm_wcdma_ready (MMPortSerialQcdm *port,
                              GAsyncResult *res,
                              GTask *task)
{
    MMBroadbandModem *self;
    AccessTechContext *ctx;
    QcdmResult *result;
    gint err = QCDM_SUCCESS;
    guint8 l1;
    GError *error = NULL;
    GByteArray *response;

    response = mm_port_serial_qcdm_command_finish (port, res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    /* Parse the response */
    result = qcdm_cmd_wcdma_subsys_state_info_result ((const gchar *) response->data,
                                                      response->len,
                                                      &err);
    g_byte_array_unref (response);
    if (result) {
        qcdm_result_get_u8 (result, QCDM_CMD_WCDMA_SUBSYS_STATE_INFO_ITEM_L1_STATE, &l1);
        qcdm_result_unref (result);

        if (l1 == QCDM_WCDMA_L1_STATE_PCH ||
            l1 == QCDM_WCDMA_L1_STATE_FACH ||
            l1 == QCDM_WCDMA_L1_STATE_DCH ||
            l1 == QCDM_WCDMA_L1_STATE_PCH_SLEEP)
            ctx->wcdma_open = TRUE;
    }

    g_task_return_pointer (task,
                           access_tech_and_mask_new (MM_BROADBAND_MODEM (self), ctx),
                           g_free);
    g_object_unref (task);
}

static void
access_tech_qcdm_gsm_ready (MMPortSerialQcdm *port,
                            GAsyncResult *res,
                            GTask *task)
{
    AccessTechContext *ctx;
    GByteArray *cmd;
    QcdmResult *result;
    gint err = QCDM_SUCCESS;
    guint8 opmode = 0;
    guint8 sysmode = 0;
    GError *error = NULL;
    GByteArray *response;

    response = mm_port_serial_qcdm_command_finish (port, res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Parse the response */
    result = qcdm_cmd_gsm_subsys_state_info_result ((const gchar *) response->data,
                                                    response->len,
                                                    &err);
    g_byte_array_unref (response);
    if (!result) {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Failed to parse GSM subsys command result: %d",
                                 err);
        g_object_unref (task);
        return;
    }

    ctx = g_task_get_task_data (task);

    qcdm_result_get_u8 (result, QCDM_CMD_GSM_SUBSYS_STATE_INFO_ITEM_CM_OP_MODE, &opmode);
    qcdm_result_get_u8 (result, QCDM_CMD_GSM_SUBSYS_STATE_INFO_ITEM_CM_SYS_MODE, &sysmode);
    qcdm_result_unref (result);

    ctx->opmode = opmode;
    ctx->sysmode = sysmode;

    /* WCDMA subsystem state */
    cmd = g_byte_array_sized_new (50);
    cmd->len = qcdm_cmd_wcdma_subsys_state_info_new ((char *) cmd->data, 50);
    g_assert (cmd->len);

    mm_port_serial_qcdm_command (port,
                                 cmd,
                                 3,
                                 NULL,
                                 (GAsyncReadyCallback)access_tech_qcdm_wcdma_ready,
                                 task);
    g_byte_array_unref (cmd);
}

static void
access_tech_qcdm_hdr_ready (MMPortSerialQcdm *port,
                            GAsyncResult *res,
                            GTask *task)
{
    MMBroadbandModem *self;
    AccessTechContext *ctx;
    QcdmResult *result;
    gint err = QCDM_SUCCESS;
    guint8 session = 0;
    guint8 almp = 0;
    GError *error = NULL;
    GByteArray *response;

    response = mm_port_serial_qcdm_command_finish (port, res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    self = g_task_get_source_object (task);
    ctx  = g_task_get_task_data (task);

    /* Parse the response */
    result = qcdm_cmd_hdr_subsys_state_info_result ((const gchar *) response->data,
                                                    response->len,
                                                    &err);
    g_byte_array_unref (response);
    if (result) {
        qcdm_result_get_u8 (result, QCDM_CMD_HDR_SUBSYS_STATE_INFO_ITEM_SESSION_STATE, &session);
        qcdm_result_get_u8 (result, QCDM_CMD_HDR_SUBSYS_STATE_INFO_ITEM_ALMP_STATE, &almp);
        qcdm_result_unref (result);

        if (session == QCDM_CMD_HDR_SUBSYS_STATE_INFO_SESSION_STATE_OPEN &&
            (almp == QCDM_CMD_HDR_SUBSYS_STATE_INFO_ALMP_STATE_IDLE ||
             almp == QCDM_CMD_HDR_SUBSYS_STATE_INFO_ALMP_STATE_CONNECTED))
            ctx->evdo_open = TRUE;
    }

    g_task_return_pointer (task, access_tech_and_mask_new (self, ctx), g_free);
    g_object_unref (task);
}

static void
access_tech_qcdm_cdma_ready (MMPortSerialQcdm *port,
                             GAsyncResult *res,
                             GTask *task)
{
    AccessTechContext *ctx;
    GByteArray *cmd;
    QcdmResult *result;
    gint err = QCDM_SUCCESS;
    guint32 hybrid;
    GError *error = NULL;
    GByteArray *response;

    response = mm_port_serial_qcdm_command_finish (port, res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Parse the response */
    result = qcdm_cmd_cm_subsys_state_info_result ((const gchar *) response->data,
                                                   response->len,
                                                   &err);
    g_byte_array_unref (response);
    if (!result) {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Failed to parse CM subsys command result: %d",
                                 err);
        g_object_unref (task);
        return;
    }

    ctx = g_task_get_task_data (task);

    qcdm_result_get_u32 (result, QCDM_CMD_CM_SUBSYS_STATE_INFO_ITEM_OPERATING_MODE, &ctx->opmode);
    qcdm_result_get_u32 (result, QCDM_CMD_CM_SUBSYS_STATE_INFO_ITEM_SYSTEM_MODE, &ctx->sysmode);
    qcdm_result_get_u32 (result, QCDM_CMD_CM_SUBSYS_STATE_INFO_ITEM_HYBRID_PREF, &hybrid);
    qcdm_result_unref (result);

    ctx->hybrid = !!hybrid;

    /* HDR subsystem state */
    cmd = g_byte_array_sized_new (50);
    cmd->len = qcdm_cmd_hdr_subsys_state_info_new ((char *) cmd->data, 50);
    g_assert (cmd->len);

    mm_port_serial_qcdm_command (port,
                                 cmd,
                                 3,
                                 NULL,
                                 (GAsyncReadyCallback)access_tech_qcdm_hdr_ready,
                                 task);
    g_byte_array_unref (cmd);
}

static void
access_tech_from_cdma_registration_state (MMBroadbandModem *self,
                                          AccessTechContext *ctx)
{
    gboolean cdma1x_registered = FALSE;
    gboolean evdo_registered = FALSE;

    if (self->priv->modem_cdma_evdo_registration_state > MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN)
        evdo_registered = TRUE;

    if (self->priv->modem_cdma_cdma1x_registration_state > MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN)
        cdma1x_registered = TRUE;

    if (self->priv->modem_cdma_evdo_network_supported && evdo_registered) {
        ctx->fallback_act = MM_MODEM_ACCESS_TECHNOLOGY_EVDO0;
        ctx->fallback_mask = MM_IFACE_MODEM_CDMA_ALL_ACCESS_TECHNOLOGIES_MASK;
    } else if (self->priv->modem_cdma_cdma1x_network_supported && cdma1x_registered) {
        ctx->fallback_act = MM_MODEM_ACCESS_TECHNOLOGY_1XRTT;
        ctx->fallback_mask = MM_IFACE_MODEM_CDMA_ALL_ACCESS_TECHNOLOGIES_MASK;
    }

    mm_obj_dbg (self, "EVDO registration: %d", self->priv->modem_cdma_evdo_registration_state);
    mm_obj_dbg (self, "CDMA1x registration: %d", self->priv->modem_cdma_cdma1x_registration_state);
    mm_obj_dbg (self, "fallback access tech: 0x%08x", ctx->fallback_act);
}

static void
modem_load_access_technologies (MMIfaceModem *self,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
    AccessTechContext *ctx;
    GTask *task;
    GByteArray *cmd;
    GError *error = NULL;

    /* For modems where only QCDM provides detailed information, try to
     * get access technologies via the various QCDM subsystems or from
     * registration state
     */
    ctx = g_new0 (AccessTechContext, 1);
    ctx->port = mm_base_modem_peek_port_qcdm (MM_BASE_MODEM (self));

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, (GDestroyNotify)access_tech_context_free);

    if (ctx->port) {
        /* Need to open QCDM port as it may be closed/blocked */
        if (mm_port_serial_open (MM_PORT_SERIAL (ctx->port), &error)) {
            g_object_ref (ctx->port);

            mm_obj_dbg (self, "loading access technologies via QCDM...");

            /* FIXME: we may want to run both the CDMA and 3GPP in sequence to ensure
             * that a multi-mode device that's in CDMA-mode but still has 3GPP capabilities
             * will get the correct access tech, since the 3GPP check is run first.
             */

            if (mm_iface_modem_is_3gpp (self)) {
                cmd = g_byte_array_sized_new (50);
                cmd->len = qcdm_cmd_gsm_subsys_state_info_new ((char *) cmd->data, 50);
                g_assert (cmd->len);

                mm_port_serial_qcdm_command (ctx->port,
                                             cmd,
                                             3,
                                             NULL,
                                             (GAsyncReadyCallback)access_tech_qcdm_gsm_ready,
                                             task);
                g_byte_array_unref (cmd);
                return;
            }

            if (mm_iface_modem_is_cdma (self)) {
                cmd = g_byte_array_sized_new (50);
                cmd->len = qcdm_cmd_cm_subsys_state_info_new ((char *) cmd->data, 50);
                g_assert (cmd->len);

                mm_port_serial_qcdm_command (ctx->port,
                                             cmd,
                                             3,
                                             NULL,
                                             (GAsyncReadyCallback)access_tech_qcdm_cdma_ready,
                                             task);
                g_byte_array_unref (cmd);
                return;
            }

            g_assert_not_reached ();
        }

        ctx->port = NULL;
        mm_obj_dbg (self, "couldn't open QCDM port: %s", error->message);
        g_clear_error (&error);
    }

    /* Fall back if we don't have a QCDM port or it couldn't be opened */
    if (mm_iface_modem_is_cdma (self)) {
        /* If we don't have a QCDM port but the modem is CDMA-only, then
         * guess access technologies from the registration information.
         */
        access_tech_from_cdma_registration_state (MM_BROADBAND_MODEM (self), ctx);
        g_task_return_pointer (task,
                               access_tech_and_mask_new (MM_BROADBAND_MODEM (self), ctx),
                               g_free);
    } else {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_UNSUPPORTED,
                                 "Cannot get 3GPP access technology without a QCDM port");
    }
    g_object_unref (task);
}

/*****************************************************************************/

gint
mm_broadband_modem_get_initial_eps_bearer_cid (MMBroadbandModem *self)
{
    return self->priv->initial_eps_bearer_cid_support_checked ? self->priv->initial_eps_bearer_cid : -1;
}

/*****************************************************************************/
/* Load initial EPS bearer cid */

static gint
load_initial_eps_bearer_cid_finish (MMBroadbandModem  *self,
                                    GAsyncResult      *res,
                                    GError           **error)
{
    return g_task_propagate_int (G_TASK (res), error);
}

static void
initial_eps_bearer_cid_cgdcont_test_ready (MMBaseModem  *_self,
                                           GAsyncResult *res,
                                           GTask        *task)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    const gchar      *response;
    GError           *error = NULL;
    guint             min_cid;

    response = mm_base_modem_at_command_full_finish (_self, res, &error);
    if (!response)
        mm_obj_dbg (self, "failed +CGDCONT format check : %s", error->message);
    else {
        GList *format_list;

        format_list = mm_3gpp_parse_cgdcont_test_response (response, self, &error);
        if (error)
            mm_obj_dbg (self, "error parsing +CGDCONT test response: %s", error->message);
        else if (!mm_3gpp_pdp_context_format_list_find_range (format_list, MM_BEARER_IP_FAMILY_IPV4, &min_cid, NULL)) {
            /* We check for IPv4 following the assumption that modems will generally support v4, while
             * v6 is considered optional. If we ever find a modem supporting v6 exclusively and not v4
             * we would need to update this logic to also check for v6. */
            mm_obj_dbg (self, "context format check for IP not found");
            error = g_error_new (MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                                 "Unexpected +CGDCONT test response");
        } else {
            mm_obj_dbg (self, "initial EPS bearer context cid found: %u", min_cid);
        }
        mm_3gpp_pdp_context_format_list_free (format_list);
    }

    if (error)
        g_task_return_error (task, error);
    else
        g_task_return_int (task, (gint) min_cid);
    g_object_unref (task);
}

static void
load_initial_eps_bearer_cid (MMBroadbandModem    *self,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /*
     * The cid for the attach settings:
     *   - As per 3GPP specs, it should be cid=0
     *   - Qualcomm based modems default this to cid=1
     */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CGDCONT=?",
                              3,
                              TRUE, /* cached */
                              (GAsyncReadyCallback)initial_eps_bearer_cid_cgdcont_test_ready,
                              task);
}

/*****************************************************************************/
/* Load initial EPS bearer settings currently configured in modem (3GPP interface) */

static MMBearerProperties *
modem_3gpp_load_initial_eps_bearer_settings_finish (MMIfaceModem3gpp  *self,
                                                    GAsyncResult      *res,
                                                    GError           **error)
{
    return MM_BEARER_PROPERTIES (g_task_propagate_pointer (G_TASK (res), error));
}

static void
load_initial_eps_bearer_get_profile_ready (MMIfaceModem3gppProfileManager *self,
                                           GAsyncResult                   *res,
                                           GTask                          *task)
{
    GError                   *error = NULL;
    g_autoptr(MM3gppProfile)  profile = NULL;
    MMBearerProperties       *props;

    profile = mm_iface_modem_3gpp_profile_manager_get_profile_finish (self, res, &error);
    if (!profile) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    props = mm_bearer_properties_new_from_profile (profile, &error);
    if (!props)
        g_task_return_error (task, error);
    else
        g_task_return_pointer (task, props, g_object_unref);
    g_object_unref (task);
}

static void
load_initial_eps_bearer_profile (GTask *task)
{
    MMBroadbandModem *self;

    self = g_task_get_source_object (task);
    g_assert (self->priv->initial_eps_bearer_cid_support_checked);

    if (self->priv->initial_eps_bearer_cid < 0) {
        g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_UNSUPPORTED,
                                 "initial EPS bearer context management unsupported");
        g_object_unref (task);
        return;
    }

    /* Note that we may be calling this before initializing the 3GPP profile manager interface,
     * but it should not be an issue, because the interface initialization exclusively checks
     * for the feature support and takes care of creating the DBus skeleton. There is currently
     * no explicit state initialized that is required for operations later on. Ideally, though,
     * an interface method should not be used unless it is initialized, but in this case it's
     * problematic because the profile manager interface is initialized always *after* the 3GPP
     * interface. */
    mm_iface_modem_3gpp_profile_manager_get_profile (
        MM_IFACE_MODEM_3GPP_PROFILE_MANAGER (self),
        self->priv->initial_eps_bearer_cid,
        (GAsyncReadyCallback) load_initial_eps_bearer_get_profile_ready,
        task);
}

static void
load_initial_eps_bearer_cid_ready (MMBaseModem  *_self,
                                   GAsyncResult *res,
                                   GTask        *task)
{
    MMBroadbandModem  *self = MM_BROADBAND_MODEM (_self);
    g_autoptr(GError)  error = NULL;

    g_assert (self->priv->initial_eps_bearer_cid < 0);

    self->priv->initial_eps_bearer_cid = MM_BROADBAND_MODEM_GET_CLASS (self)->load_initial_eps_bearer_cid_finish (self, res, &error);
    if (error)
        mm_obj_dbg (self, "couldn't load initial EPS bearer cid: %s", error->message);

    load_initial_eps_bearer_profile (task);
}

static void
modem_3gpp_load_initial_eps_bearer_settings (MMIfaceModem3gpp    *_self,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    GTask            *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* Lookup which is supposed to be the initial EPS bearer context cid */
    if (G_UNLIKELY (!self->priv->initial_eps_bearer_cid_support_checked)) {
        self->priv->initial_eps_bearer_cid_support_checked = TRUE;
        g_assert (self->priv->initial_eps_bearer_cid < 0);
        mm_obj_dbg (self, "looking for the initial EPS bearer context cid,,,");
        MM_BROADBAND_MODEM_GET_CLASS (self)->load_initial_eps_bearer_cid (self,
                                                                          (GAsyncReadyCallback)load_initial_eps_bearer_cid_ready,
                                                                          task);
        return;
    }

    load_initial_eps_bearer_profile (task);
}

/*****************************************************************************/
/* Load initial EPS bearer properties as agreed with network (3GPP interface) */

static MMBearerProperties *
modem_3gpp_load_initial_eps_bearer_finish (MMIfaceModem3gpp  *self,
                                           GAsyncResult      *res,
                                           GError           **error)
{
    return MM_BEARER_PROPERTIES (g_task_propagate_pointer (G_TASK (res), error));
}

static void
load_initial_eps_cgcontrdp_ready (MMBaseModem  *self,
                                  GAsyncResult *res,
                                  GTask        *task)
{
    GError           *error = NULL;
    const gchar      *response;
    g_autofree gchar *apn = NULL;

    response = mm_base_modem_at_command_finish (self, res, &error);
    if (!response || !mm_3gpp_parse_cgcontrdp_response (response, NULL, NULL, &apn, NULL, NULL, NULL, NULL, NULL, &error))
        g_task_return_error (task, error);
    else {
        MMBearerProperties *props;

        props = mm_bearer_properties_new ();
        mm_bearer_properties_set_apn (props, apn);
        g_task_return_pointer (task, props, g_object_unref);
    }
    g_object_unref (task);
}

static void
modem_3gpp_load_initial_eps_bearer (MMIfaceModem3gpp    *_self,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    GTask            *task;
    g_autofree gchar *cmd = NULL;

    task = g_task_new (self, NULL, callback, user_data);

    g_assert (self->priv->initial_eps_bearer_cid_support_checked);

    if (self->priv->initial_eps_bearer_cid < 0) {
        g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_UNSUPPORTED,
                                 "initial EPS bearer context ID unknown");
        g_object_unref (task);
        return;
    }

    cmd = g_strdup_printf ("+CGCONTRDP=%d", self->priv->initial_eps_bearer_cid);
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              cmd,
                              3,
                              FALSE,
                              (GAsyncReadyCallback) load_initial_eps_cgcontrdp_ready,
                              task);
}

/*****************************************************************************/
/* Set initial EPS bearer settings (3GPP interface) */

static gboolean
modem_3gpp_set_initial_eps_bearer_settings_finish (MMIfaceModem3gpp  *self,
                                                   GAsyncResult      *res,
                                                   GError           **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
set_initial_eps_bearer_modify_profile_ready (MMIfaceModem3gppProfileManager *self,
                                             GAsyncResult                   *res,
                                             GTask                          *task)
{
    GError                   *error = NULL;
    g_autoptr(MM3gppProfile)  stored = NULL;

    stored = mm_iface_modem_3gpp_profile_manager_set_profile_finish (self, res, &error);
    if (!stored)
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_3gpp_set_initial_eps_bearer_settings (MMIfaceModem3gpp    *_self,
                                            MMBearerProperties  *props,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    GTask            *task;
    MMBearerIpFamily  ip_family;
    MM3gppProfile    *profile = NULL;

    task = g_task_new (self, NULL, callback, user_data);

    g_assert (self->priv->initial_eps_bearer_cid_support_checked);
    if (self->priv->initial_eps_bearer_cid < 0) {
        g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_UNSUPPORTED,
                                 "initial EPS bearer context ID unknown");
        g_object_unref (task);
        return;
    }

    profile = mm_bearer_properties_peek_3gpp_profile (props);
    mm_3gpp_profile_set_profile_id (profile, self->priv->initial_eps_bearer_cid);
    ip_family = mm_3gpp_profile_get_ip_type (profile);
    if (ip_family == MM_BEARER_IP_FAMILY_NONE || ip_family == MM_BEARER_IP_FAMILY_ANY)
        mm_3gpp_profile_set_ip_type (profile, MM_BEARER_IP_FAMILY_IPV4);

    mm_iface_modem_3gpp_profile_manager_set_profile (MM_IFACE_MODEM_3GPP_PROFILE_MANAGER (self),
                                                     profile,
                                                     "profile-id",
                                                     TRUE,
                                                     (GAsyncReadyCallback) set_initial_eps_bearer_modify_profile_ready,
                                                     task);
}

/*****************************************************************************/
/* Setup/Cleanup unsolicited events (3GPP interface) */

static gboolean
modem_3gpp_setup_cleanup_unsolicited_events_finish (MMIfaceModem3gpp *self,
                                                    GAsyncResult *res,
                                                    GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
bearer_report_disconnected (MMBaseBearer *bearer,
                            gpointer      user_data)
{
    gint profile_id;

    profile_id = GPOINTER_TO_INT (user_data);

    /* If we're told to disconnect a single context and this is not the
     * bearer associated to that context, ignore operation */
    if ((profile_id != MM_3GPP_PROFILE_ID_UNKNOWN) && (mm_base_bearer_get_profile_id (bearer) != profile_id))
        return;

    /* If already disconnected, ignore operation */
    if (mm_base_bearer_get_status (bearer) == MM_BEARER_STATUS_DISCONNECTED)
        return;

    mm_obj_msg (bearer, "explicitly disconnected");
    mm_base_bearer_report_connection_status (bearer, MM_BEARER_CONNECTION_STATUS_DISCONNECTED);
}

static void
bearer_list_report_disconnections (MMBroadbandModem *self,
                                   gint              profile_id)
{
    g_autoptr(MMBearerList) list = NULL;

    g_object_get (self,
                  MM_IFACE_MODEM_BEARER_LIST, &list,
                  NULL);

    /* If empty bearer list, nothing else to do */
    if (list)
        mm_bearer_list_foreach (list, (MMBearerListForeachFunc)bearer_report_disconnected, GINT_TO_POINTER (profile_id));
}

static void
cgev_process_detach (MMBroadbandModem *self,
                     MM3gppCgev        type)
{
    if (type == MM_3GPP_CGEV_NW_DETACH) {
        mm_obj_msg (self, "network forced PS detach: all contexts have been deactivated");
        bearer_list_report_disconnections (self, MM_3GPP_PROFILE_ID_UNKNOWN);
        return;
    }

    if (type == MM_3GPP_CGEV_ME_DETACH) {
        mm_obj_msg (self, "mobile equipment forced PS detach: all contexts have been deactivated");
        bearer_list_report_disconnections (self, MM_3GPP_PROFILE_ID_UNKNOWN);
        return;
    }

    g_assert_not_reached ();
}

static void
cgev_process_primary (MMBroadbandModem *self,
                      MM3gppCgev        type,
                      const gchar      *str)
{
    GError *error = NULL;
    guint   cid = 0;

    if (!mm_3gpp_parse_cgev_indication_primary (str, type, &cid, &error)) {
        mm_obj_warn (self, "couldn't parse cid info from +CGEV indication '%s': %s", str, error->message);
        g_error_free (error);
        return;
    }

    switch (type) {
    case MM_3GPP_CGEV_NW_ACT_PRIMARY:
        mm_obj_msg (self, "network request to activate context (cid %u)", cid);
        break;
    case MM_3GPP_CGEV_ME_ACT_PRIMARY:
        mm_obj_msg (self, "mobile equipment request to activate context (cid %u)", cid);
        break;
    case MM_3GPP_CGEV_NW_DEACT_PRIMARY:
        mm_obj_msg (self, "network request to deactivate context (cid %u)", cid);
        bearer_list_report_disconnections (self, (gint)cid);
        break;
    case MM_3GPP_CGEV_ME_DEACT_PRIMARY:
        mm_obj_msg (self, "mobile equipment request to deactivate context (cid %u)", cid);
        bearer_list_report_disconnections (self, (gint)cid);
        break;
    case MM_3GPP_CGEV_UNKNOWN:
    case MM_3GPP_CGEV_NW_DETACH:
    case MM_3GPP_CGEV_ME_DETACH:
    case MM_3GPP_CGEV_NW_CLASS:
    case MM_3GPP_CGEV_ME_CLASS:
    case MM_3GPP_CGEV_NW_ACT_SECONDARY:
    case MM_3GPP_CGEV_ME_ACT_SECONDARY:
    case MM_3GPP_CGEV_NW_DEACT_SECONDARY:
    case MM_3GPP_CGEV_ME_DEACT_SECONDARY:
    case MM_3GPP_CGEV_NW_DEACT_PDP:
    case MM_3GPP_CGEV_ME_DEACT_PDP:
    case MM_3GPP_CGEV_NW_MODIFY:
    case MM_3GPP_CGEV_ME_MODIFY:
    case MM_3GPP_CGEV_REJECT:
    case MM_3GPP_CGEV_NW_REACT:
    default:
        g_assert_not_reached ();
        break;
    }
}

static void
cgev_process_secondary (MMBroadbandModem *self,
                        MM3gppCgev        type,
                        const gchar      *str)
{
    GError *error = NULL;
    guint   p_cid = 0;
    guint   cid = 0;

    if (!mm_3gpp_parse_cgev_indication_secondary (str, type, &p_cid, &cid, NULL, &error)) {
        mm_obj_warn (self, "couldn't parse p_cid/cid info from +CGEV indication '%s': %s", str, error->message);
        g_error_free (error);
        return;
    }

    switch (type) {
    case MM_3GPP_CGEV_NW_ACT_SECONDARY:
        mm_obj_msg (self, "network request to activate secondary context (cid %u, primary cid %u)", cid, p_cid);
        break;
    case MM_3GPP_CGEV_ME_ACT_SECONDARY:
        mm_obj_msg (self, "mobile equipment request to activate secondary context (cid %u, primary cid %u)", cid, p_cid);
        break;
    case MM_3GPP_CGEV_NW_DEACT_SECONDARY:
        mm_obj_msg (self, "network request to deactivate secondary context (cid %u, primary cid %u)", cid, p_cid);
        bearer_list_report_disconnections (self, (gint)cid);
        break;
    case MM_3GPP_CGEV_ME_DEACT_SECONDARY:
        mm_obj_msg (self, "mobile equipment request to deactivate secondary context (cid %u, primary cid %u)", cid, p_cid);
        bearer_list_report_disconnections (self, (gint)cid);
        break;
    case MM_3GPP_CGEV_UNKNOWN:
    case MM_3GPP_CGEV_NW_DETACH:
    case MM_3GPP_CGEV_ME_DETACH:
    case MM_3GPP_CGEV_NW_CLASS:
    case MM_3GPP_CGEV_ME_CLASS:
    case MM_3GPP_CGEV_NW_ACT_PRIMARY:
    case MM_3GPP_CGEV_ME_ACT_PRIMARY:
    case MM_3GPP_CGEV_NW_DEACT_PRIMARY:
    case MM_3GPP_CGEV_ME_DEACT_PRIMARY:
    case MM_3GPP_CGEV_NW_DEACT_PDP:
    case MM_3GPP_CGEV_ME_DEACT_PDP:
    case MM_3GPP_CGEV_NW_MODIFY:
    case MM_3GPP_CGEV_ME_MODIFY:
    case MM_3GPP_CGEV_REJECT:
    case MM_3GPP_CGEV_NW_REACT:
    default:
        g_assert_not_reached ();
        break;
    }
}

static void
cgev_process_pdp (MMBroadbandModem *self,
                  MM3gppCgev        type,
                  const gchar      *str)
{
    GError *error = NULL;
    gchar  *pdp_type = NULL;
    gchar  *pdp_addr = NULL;
    guint   cid = 0;

    if (!mm_3gpp_parse_cgev_indication_pdp (str, type, &pdp_type, &pdp_addr, &cid, &error)) {
        mm_obj_warn (self, "couldn't parse PDP info from +CGEV indication '%s': %s", str, error->message);
        g_error_free (error);
        return;
    }

    switch (type) {
    case MM_3GPP_CGEV_REJECT:
        mm_obj_msg (self, "network request to activate context (type %s, address %s) has been automatically rejected", pdp_type, pdp_addr);
        break;
    case MM_3GPP_CGEV_NW_REACT:
        /* NOTE: we don't currently notify about automatic reconnections like this one */
        if (cid)
            mm_obj_msg (self, "network request to reactivate context (type %s, address %s, cid %u)", pdp_type, pdp_addr, cid);
        else
            mm_obj_msg (self, "network request to reactivate context (type %s, address %s, cid unknown)", pdp_type, pdp_addr);
        break;
    case MM_3GPP_CGEV_NW_DEACT_PDP:
        if (cid) {
            mm_obj_msg (self, "network request to deactivate context (type %s, address %s, cid %u)", pdp_type, pdp_addr, cid);
            bearer_list_report_disconnections (self, (gint)cid);
        } else
            mm_obj_msg (self, "network request to deactivate context (type %s, address %s, cid unknown)", pdp_type, pdp_addr);
        break;
    case MM_3GPP_CGEV_ME_DEACT_PDP:
        if (cid) {
            mm_obj_msg (self, "mobile equipment request to deactivate context (type %s, address %s, cid %u)", pdp_type, pdp_addr, cid);
            bearer_list_report_disconnections (self, (gint)cid);
        } else
            mm_obj_msg (self, "mobile equipment request to deactivate context (type %s, address %s, cid unknown)", pdp_type, pdp_addr);
        break;
    case MM_3GPP_CGEV_UNKNOWN:
    case MM_3GPP_CGEV_NW_DETACH:
    case MM_3GPP_CGEV_ME_DETACH:
    case MM_3GPP_CGEV_NW_CLASS:
    case MM_3GPP_CGEV_ME_CLASS:
    case MM_3GPP_CGEV_NW_ACT_PRIMARY:
    case MM_3GPP_CGEV_ME_ACT_PRIMARY:
    case MM_3GPP_CGEV_NW_ACT_SECONDARY:
    case MM_3GPP_CGEV_ME_ACT_SECONDARY:
    case MM_3GPP_CGEV_NW_DEACT_PRIMARY:
    case MM_3GPP_CGEV_ME_DEACT_PRIMARY:
    case MM_3GPP_CGEV_NW_DEACT_SECONDARY:
    case MM_3GPP_CGEV_ME_DEACT_SECONDARY:
    case MM_3GPP_CGEV_NW_MODIFY:
    case MM_3GPP_CGEV_ME_MODIFY:
    default:
        g_assert_not_reached ();
        break;
    }

    g_free (pdp_addr);
    g_free (pdp_type);
}

static void
cgev_received (MMPortSerialAt   *port,
               GMatchInfo       *info,
               MMBroadbandModem *self)
{
    gchar      *str;
    MM3gppCgev  type;

    str = mm_get_string_unquoted_from_match_info (info, 1);
    if (!str)
        return;

    type = mm_3gpp_parse_cgev_indication_action (str);

    switch (type) {
    case MM_3GPP_CGEV_NW_DETACH:
    case MM_3GPP_CGEV_ME_DETACH:
        cgev_process_detach (self, type);
        break;
    case MM_3GPP_CGEV_NW_ACT_PRIMARY:
    case MM_3GPP_CGEV_ME_ACT_PRIMARY:
    case MM_3GPP_CGEV_NW_DEACT_PRIMARY:
    case MM_3GPP_CGEV_ME_DEACT_PRIMARY:
        cgev_process_primary (self, type, str);
        break;
    case MM_3GPP_CGEV_NW_ACT_SECONDARY:
    case MM_3GPP_CGEV_ME_ACT_SECONDARY:
    case MM_3GPP_CGEV_NW_DEACT_SECONDARY:
    case MM_3GPP_CGEV_ME_DEACT_SECONDARY:
        cgev_process_secondary (self, type, str);
        break;
    case MM_3GPP_CGEV_NW_DEACT_PDP:
    case MM_3GPP_CGEV_ME_DEACT_PDP:
    case MM_3GPP_CGEV_REJECT:
    case MM_3GPP_CGEV_NW_REACT:
        cgev_process_pdp (self, type, str);
        break;
    case MM_3GPP_CGEV_NW_CLASS:
    case MM_3GPP_CGEV_ME_CLASS:
    case MM_3GPP_CGEV_NW_MODIFY:
    case MM_3GPP_CGEV_ME_MODIFY:
        /* ignore */
        break;
    case MM_3GPP_CGEV_UNKNOWN:
    default:
        mm_obj_dbg (self, "unhandled +CGEV indication: %s", str);
        break;
    }

    g_free (str);
}

static void
set_cgev_unsolicited_events_handlers (MMBroadbandModem *self,
                                      gboolean          enable)
{
    MMPortSerialAt    *ports[2];
    g_autoptr(GRegex)  cgev_regex = NULL;
    guint              i;

    cgev_regex = mm_3gpp_cgev_regex_get ();
    ports[0] = mm_base_modem_peek_port_primary (MM_BASE_MODEM (self));
    ports[1] = mm_base_modem_peek_port_secondary (MM_BASE_MODEM (self));

    /* Enable unsolicited events in given port */
    for (i = 0; i < 2; i++) {
        if (!ports[i])
            continue;

        /* Set/unset unsolicited CGEV event handler */
        mm_obj_dbg (self, "%s 3GPP +CGEV unsolicited events handlers in %s",
                    enable ? "setting" : "removing",
                    mm_port_get_device (MM_PORT (ports[i])));
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            cgev_regex,
            enable ? (MMPortSerialAtUnsolicitedMsgFn) cgev_received : NULL,
            enable ? self : NULL,
            NULL);
    }
}

static void
ciev_signal_received (MMBroadbandModem *self,
                      GMatchInfo       *match_info)
{
    guint quality;

    if (!mm_get_uint_from_match_info (match_info, 2, &quality)) {
        mm_obj_dbg (self, "couldn't parse signal quality value from +CIEV");
        return;
    }

    mm_iface_modem_update_signal_quality (
        MM_IFACE_MODEM (self),
        normalize_ciev_cind_signal_quality (quality,
                                            self->priv->modem_cind_min_signal_quality,
                                            self->priv->modem_cind_max_signal_quality));
}

static void
ciev_received (MMPortSerialAt   *port,
               GMatchInfo       *match_info,
               MMBroadbandModem *self)
{
    guint  ind;
    gchar *item;

    item = mm_get_string_unquoted_from_match_info (match_info, 1);
    if (!item)
        return;

    /* numeric index? */
    if (mm_get_uint_from_str (item, &ind)) {
        if (ind == self->priv->modem_cind_indicator_signal_quality)
            ciev_signal_received (self, match_info);
    }
    /* string index? */
    else {
        if (g_str_equal (item, "signal"))
            ciev_signal_received (self, match_info);
    }

    g_free (item);
}

static void
set_ciev_unsolicited_events_handlers (MMBroadbandModem *self,
                                      gboolean          enable)
{
    MMPortSerialAt    *ports[2];
    g_autoptr(GRegex)  ciev_regex = NULL;
    guint              i;

    ciev_regex = mm_3gpp_ciev_regex_get ();
    ports[0] = mm_base_modem_peek_port_primary (MM_BASE_MODEM (self));
    ports[1] = mm_base_modem_peek_port_secondary (MM_BASE_MODEM (self));

    /* Enable unsolicited events in given port */
    for (i = 0; i < 2; i++) {
        if (!ports[i])
            continue;

        /* Set/unset unsolicited CIEV event handler */
        mm_obj_dbg (self, "%s 3GPP +CIEV unsolicited events handlers in %s",
                    enable ? "setting" : "removing",
                    mm_port_get_device (MM_PORT (ports[i])));
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            ciev_regex,
            enable ? (MMPortSerialAtUnsolicitedMsgFn) ciev_received : NULL,
            enable ? self : NULL,
            NULL);
    }
}

static void
support_checked_setup_unsolicited_events (GTask *task)
{
    MMBroadbandModem *self;

    self = g_task_get_source_object (task);

    if (self->priv->modem_cind_supported)
        set_ciev_unsolicited_events_handlers (self, TRUE);

    if (self->priv->modem_cgerep_supported)
        set_cgev_unsolicited_events_handlers (self, TRUE);

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void check_and_setup_3gpp_urc_support (GTask *task);

static void
cgerep_format_check_ready (MMBroadbandModem *self,
                           GAsyncResult     *res,
                           GTask            *task)
{
    MM3gppCgerepMode  supported_modes = MM_3GPP_CGEREP_MODE_NONE;
    GError           *error = NULL;
    const gchar      *result;
    gchar            *aux;

    result = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error || !mm_3gpp_parse_cgerep_test_response (result, self, &supported_modes, &error)) {
        mm_obj_dbg (self, "+CGEREP check failed: %s", error->message);
        mm_obj_dbg (self, "packet domain event reporting is unsupported");
        g_error_free (error);
        goto out;
    }

    aux = mm_3gpp_cgerep_mode_build_string_from_mask (supported_modes);
    mm_obj_dbg (self, "supported +CGEREP modes: %s", aux);
    g_free (aux);

    self->priv->modem_cgerep_supported = TRUE;

    if (supported_modes & MM_3GPP_CGEREP_MODE_BUFFER_URCS_IF_LINK_RESERVED)
        self->priv->modem_cgerep_enable_mode = MM_3GPP_CGEREP_MODE_BUFFER_URCS_IF_LINK_RESERVED;
    else if (supported_modes & MM_3GPP_CGEREP_MODE_DISCARD_URCS_IF_LINK_RESERVED)
        self->priv->modem_cgerep_enable_mode = MM_3GPP_CGEREP_MODE_DISCARD_URCS_IF_LINK_RESERVED;

    if (supported_modes & MM_3GPP_CGEREP_MODE_DISCARD_URCS)
        self->priv->modem_cgerep_disable_mode = MM_3GPP_CGEREP_MODE_DISCARD_URCS;

out:
    /* go on with remaining checks */
    check_and_setup_3gpp_urc_support (task);
}

static void
cmer_format_check_ready (MMBroadbandModem *self,
                         GAsyncResult     *res,
                         GTask            *task)
{
    MM3gppCmerMode  supported_modes = MM_3GPP_CMER_MODE_NONE;
    MM3gppCmerInd   supported_inds = MM_3GPP_CMER_IND_NONE;
    GError         *error = NULL;
    const gchar    *result;
    gchar          *aux;

    result = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error || !mm_3gpp_parse_cmer_test_response (result, self, &supported_modes, &supported_inds, &error)) {
        mm_obj_dbg (self, "+CMER check failed: %s", error->message);
        mm_obj_dbg (self, "generic indications are unsupported");
        g_error_free (error);
        goto out;
    }

    aux = mm_3gpp_cmer_mode_build_string_from_mask (supported_modes);
    mm_obj_dbg (self, "supported +CMER modes: %s", aux);
    g_free (aux);

    aux = mm_3gpp_cmer_ind_build_string_from_mask (supported_inds);
    mm_obj_dbg (self, "supported +CMER indication settings: %s", aux);
    g_free (aux);

    /* Flag +CMER supported values */

    if (supported_modes & MM_3GPP_CMER_MODE_FORWARD_URCS)
        self->priv->modem_cmer_enable_mode = MM_3GPP_CMER_MODE_FORWARD_URCS;
    else if (supported_modes & MM_3GPP_CMER_MODE_BUFFER_URCS_IF_LINK_RESERVED)
        self->priv->modem_cmer_enable_mode = MM_3GPP_CMER_MODE_BUFFER_URCS_IF_LINK_RESERVED;
    else if (supported_modes & MM_3GPP_CMER_MODE_DISCARD_URCS_IF_LINK_RESERVED)
        self->priv->modem_cmer_enable_mode = MM_3GPP_CMER_MODE_DISCARD_URCS_IF_LINK_RESERVED;

    aux = mm_3gpp_cmer_mode_build_string_from_mask (self->priv->modem_cmer_enable_mode);
    mm_obj_dbg (self, "+CMER enable mode: %s", aux);
    g_free (aux);

    if (supported_modes & MM_3GPP_CMER_MODE_DISCARD_URCS)
        self->priv->modem_cmer_disable_mode = MM_3GPP_CMER_MODE_DISCARD_URCS;

    aux = mm_3gpp_cmer_mode_build_string_from_mask (self->priv->modem_cmer_disable_mode);
    mm_obj_dbg (self, "+CMER disable mode: %s", aux);
    g_free (aux);

    if (supported_inds & MM_3GPP_CMER_IND_ENABLE_NOT_CAUSED_BY_CIND)
        self->priv->modem_cmer_ind = MM_3GPP_CMER_IND_ENABLE_NOT_CAUSED_BY_CIND;
    else if (supported_inds & MM_3GPP_CMER_IND_ENABLE_ALL)
        self->priv->modem_cmer_ind = MM_3GPP_CMER_IND_ENABLE_ALL;

    aux = mm_3gpp_cmer_ind_build_string_from_mask (self->priv->modem_cmer_ind);
    mm_obj_dbg (self, "+CMER indication setting: %s", aux);
    g_free (aux);

out:
    /* go on with remaining checks */
    check_and_setup_3gpp_urc_support (task);
}

static void
cind_format_check_ready (MMBroadbandModem *self,
                         GAsyncResult     *res,
                         GTask            *task)
{
    GHashTable *indicators = NULL;
    GError *error = NULL;
    const gchar *result;
    MM3gppCindResponse *r;

    result = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error ||
        !(indicators = mm_3gpp_parse_cind_test_response (result, &error))) {
        /* unsupported indications */
        mm_obj_dbg (self, "+CIND check failed: %s", error->message);
        mm_obj_dbg (self, "generic indications are unsupported");
        g_error_free (error);
        /* go on with remaining checks */
        check_and_setup_3gpp_urc_support (task);
        return;
    }

    /* Mark CIND as being supported and find the proper indexes for the
     * indicators. */
    self->priv->modem_cind_supported = TRUE;

    /* Check if we support signal quality indications */
    r = g_hash_table_lookup (indicators, "signal");
    if (r) {
        self->priv->modem_cind_indicator_signal_quality = mm_3gpp_cind_response_get_index (r);
        self->priv->modem_cind_min_signal_quality = mm_3gpp_cind_response_get_min (r);
        self->priv->modem_cind_max_signal_quality = mm_3gpp_cind_response_get_max (r);

        mm_obj_dbg (self, "signal quality indications via CIND are supported at index '%u' (min: %u, max: %u)",
                    self->priv->modem_cind_indicator_signal_quality,
                    self->priv->modem_cind_min_signal_quality,
                    self->priv->modem_cind_max_signal_quality);
    } else
        self->priv->modem_cind_indicator_signal_quality = CIND_INDICATOR_INVALID;

    /* Check if we support roaming indications */
    r = g_hash_table_lookup (indicators, "roam");
    if (r) {
        self->priv->modem_cind_indicator_roaming = mm_3gpp_cind_response_get_index (r);
        mm_obj_dbg (self, "roaming indications via CIND are supported at index '%u'",
                    self->priv->modem_cind_indicator_roaming);
    } else
        self->priv->modem_cind_indicator_roaming = CIND_INDICATOR_INVALID;

    /* Check if we support service indications */
    r = g_hash_table_lookup (indicators, "service");
    if (r) {
        self->priv->modem_cind_indicator_service = mm_3gpp_cind_response_get_index (r);
        mm_obj_dbg (self, "service indications via CIND are supported at index '%u'",
                    self->priv->modem_cind_indicator_service);
    } else
        self->priv->modem_cind_indicator_service = CIND_INDICATOR_INVALID;

    g_hash_table_destroy (indicators);

    /* Check +CMER required format */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CMER=?",
                              3,
                              TRUE,
                              (GAsyncReadyCallback)cmer_format_check_ready,
                              task);
}

static void
check_and_setup_3gpp_urc_support (GTask *task)
{
    MMBroadbandModem *self;

    self = g_task_get_source_object (task);

    /* Check support for +CIEV indications, managed with +CIND/+CMER */
    if (!self->priv->modem_cind_disabled && !self->priv->modem_cind_support_checked) {
        mm_obj_dbg (self, "checking indicator support...");
        self->priv->modem_cind_support_checked = TRUE;
        mm_base_modem_at_command (MM_BASE_MODEM (self),
                                  "+CIND=?",
                                  3,
                                  TRUE,
                                  (GAsyncReadyCallback)cind_format_check_ready,
                                  task);
        return;
    }

    /* Check support for +CGEV indications, managed with +CGEREP */
    if (!self->priv->modem_cgerep_support_checked) {
        mm_obj_dbg (self, "checking packet domain event reporting...");
        self->priv->modem_cgerep_support_checked = TRUE;
        mm_base_modem_at_command (MM_BASE_MODEM (self),
                                  "+CGEREP=?",
                                  3,
                                  TRUE,
                                  (GAsyncReadyCallback)cgerep_format_check_ready,
                                  task);
        return;
    }

    support_checked_setup_unsolicited_events (task);
}

static void
modem_3gpp_setup_unsolicited_events (MMIfaceModem3gpp    *self,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);
    check_and_setup_3gpp_urc_support (task);
}

static void
modem_3gpp_cleanup_unsolicited_events (MMIfaceModem3gpp    *_self,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    if (!self->priv->modem_cind_disabled && self->priv->modem_cind_support_checked && self->priv->modem_cind_supported)
        set_ciev_unsolicited_events_handlers (self, FALSE);

    if (self->priv->modem_cgerep_supported)
        set_cgev_unsolicited_events_handlers (self, FALSE);

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

/*****************************************************************************/
/* Enabling/disabling unsolicited events (3GPP interface) */

typedef struct {
    gboolean        enable;
    MMPortSerialAt *primary;
    MMPortSerialAt *secondary;
    gchar          *cmer_command;
    gboolean        cmer_primary_done;
    gboolean        cmer_secondary_done;
    gchar          *cgerep_command;
    gboolean        cgerep_primary_done;
    gboolean        cgerep_secondary_done;
} UnsolicitedEventsContext;

static void
unsolicited_events_context_free (UnsolicitedEventsContext *ctx)
{
    if (ctx->secondary)
        g_object_unref (ctx->secondary);
    if (ctx->primary)
        g_object_unref (ctx->primary);
    g_free (ctx->cgerep_command);
    g_free (ctx->cmer_command);
    g_free (ctx);
}

static gboolean
modem_3gpp_enable_disable_unsolicited_events_finish (MMIfaceModem3gpp *self,
                                                     GAsyncResult *res,
                                                     GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void run_unsolicited_events_setup (GTask *task);

static void
unsolicited_events_setup_ready (MMBroadbandModem *self,
                                GAsyncResult     *res,
                                GTask            *task)
{
    UnsolicitedEventsContext *ctx;
    GError                   *error = NULL;

    ctx = g_task_get_task_data (task);

    if (!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error)) {
        mm_obj_dbg (self, "couldn't %s event reporting: '%s'",
                    ctx->enable ? "enable" : "disable",
                    error->message);
        g_error_free (error);
    }

    /* Continue on next port/command */
    run_unsolicited_events_setup (task);
}

static void
run_unsolicited_events_setup (GTask *task)
{
    MMBroadbandModem         *self;
    UnsolicitedEventsContext *ctx;
    MMPortSerialAt           *port = NULL;
    const gchar              *command = NULL;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    /* CMER on primary port */
    if (!ctx->cmer_primary_done && ctx->cmer_command && ctx->primary && !self->priv->modem_cind_disabled) {
        mm_obj_dbg (self, "%s +CIND event reporting in primary port...", ctx->enable ? "enabling" : "disabling");
        ctx->cmer_primary_done = TRUE;
        command = ctx->cmer_command;
        port = ctx->primary;
    }
    /* CMER on secondary port */
    else if (!ctx->cmer_secondary_done && ctx->cmer_command && ctx->secondary && !self->priv->modem_cind_disabled) {
        mm_obj_dbg (self, "%s +CIND event reporting in secondary port...", ctx->enable ? "enabling" : "disabling");
        ctx->cmer_secondary_done = TRUE;
        command = ctx->cmer_command;
        port = ctx->secondary;
    }
    /* CGEREP on primary port */
    else if (!ctx->cgerep_primary_done && ctx->cgerep_command && ctx->primary) {
        mm_obj_dbg (self, "%s +CGEV event reporting in primary port...", ctx->enable ? "enabling" : "disabling");
        ctx->cgerep_primary_done = TRUE;
        command = ctx->cgerep_command;
        port = ctx->primary;
    }
    /* CGEREP on secondary port */
    else if (!ctx->cgerep_secondary_done && ctx->cgerep_command && ctx->secondary) {
        mm_obj_dbg (self, "%s +CGEV event reporting in secondary port...", ctx->enable ? "enabling" : "disabling");
        ctx->cgerep_secondary_done = TRUE;
        port = ctx->secondary;
        command = ctx->cgerep_command;
    }

    /* Enable unsolicited events in given port */
    if (port && command) {
        mm_base_modem_at_command_full (MM_BASE_MODEM (self),
                                       MM_IFACE_PORT_AT (port),
                                       command,
                                       3,
                                       FALSE,
                                       FALSE, /* raw */
                                       NULL, /* cancellable */
                                       (GAsyncReadyCallback)unsolicited_events_setup_ready,
                                       task);
        return;
    }

    /* Fully done now */
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_3gpp_enable_unsolicited_events (MMIfaceModem3gpp *_self,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
    MMBroadbandModem         *self = MM_BROADBAND_MODEM (_self);
    GTask                    *task;
    UnsolicitedEventsContext *ctx;

    task = g_task_new (self, NULL, callback, user_data);

    ctx = g_new0 (UnsolicitedEventsContext, 1);
    ctx->enable = TRUE;
    ctx->primary = mm_base_modem_get_port_primary (MM_BASE_MODEM (self));
    ctx->secondary = mm_base_modem_get_port_secondary (MM_BASE_MODEM (self));
    g_task_set_task_data (task, ctx, (GDestroyNotify)unsolicited_events_context_free);

    if (self->priv->modem_cind_support_checked && self->priv->modem_cind_supported)
        ctx->cmer_command = mm_3gpp_build_cmer_set_request (self->priv->modem_cmer_enable_mode, self->priv->modem_cmer_ind);

    if (self->priv->modem_cgerep_support_checked && self->priv->modem_cgerep_supported)
        ctx->cgerep_command = mm_3gpp_build_cgerep_set_request (self->priv->modem_cgerep_enable_mode);

    run_unsolicited_events_setup (task);
}

static void
modem_3gpp_disable_unsolicited_events (MMIfaceModem3gpp *_self,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
    MMBroadbandModem         *self = MM_BROADBAND_MODEM (_self);
    GTask                    *task;
    UnsolicitedEventsContext *ctx;

    task = g_task_new (self, NULL, callback, user_data);

    ctx = g_new0 (UnsolicitedEventsContext, 1);
    ctx->primary = mm_base_modem_get_port_primary (MM_BASE_MODEM (self));
    ctx->secondary = mm_base_modem_get_port_secondary (MM_BASE_MODEM (self));
    g_task_set_task_data (task, ctx, (GDestroyNotify)unsolicited_events_context_free);

    if (self->priv->modem_cind_support_checked && self->priv->modem_cind_supported)
        ctx->cmer_command = mm_3gpp_build_cmer_set_request (self->priv->modem_cmer_disable_mode, MM_3GPP_CMER_IND_NONE);

    if (self->priv->modem_cgerep_support_checked && self->priv->modem_cgerep_supported)
        ctx->cgerep_command = mm_3gpp_build_cgerep_set_request (self->priv->modem_cgerep_disable_mode);

    run_unsolicited_events_setup (task);
}

/*****************************************************************************/
/* Setting modem charset (Modem interface) */

typedef struct {
    MMModemCharset charset;
    /* Commands to try in the sequence:
     *  First one with quotes
     *  Second without.
     *  + last NUL */
    MMBaseModemAtCommandAlloc charset_commands[3];
} SetupCharsetContext;

static void
setup_charset_context_free (SetupCharsetContext *ctx)
{
    mm_base_modem_at_command_alloc_clear (&ctx->charset_commands[0]);
    mm_base_modem_at_command_alloc_clear (&ctx->charset_commands[1]);
    g_free (ctx);
}

static gboolean
modem_setup_charset_finish (MMIfaceModem *self,
                            GAsyncResult *res,
                            GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
current_charset_query_ready (MMBroadbandModem *self,
                             GAsyncResult *res,
                             GTask *task)
{
    SetupCharsetContext *ctx;
    GError *error = NULL;
    const gchar *response;

    ctx = g_task_get_task_data (task);

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (!response)
        g_task_return_error (task, error);
    else {
        MMModemCharset current;
        const gchar *p;

        p = response;
        if (g_str_has_prefix (p, "+CSCS:"))
            p += 6;
        while (*p == ' ')
            p++;

        current = mm_modem_charset_from_string (p);
        if (ctx->charset != current)
            g_task_return_new_error (task,
                                     MM_CORE_ERROR,
                                     MM_CORE_ERROR_FAILED,
                                     "Modem failed to change character set to %s",
                                     mm_modem_charset_to_string (ctx->charset));
        else {
            /* We'll keep track ourselves of the current charset.
             * TODO: Make this a property so that plugins can also store it. */
            self->priv->modem_current_charset = current;
            g_task_return_boolean (task, TRUE);
        }
    }

    g_object_unref (task);
}

static void
charset_change_ready (MMBroadbandModem *self,
                      GAsyncResult *res,
                      GTask *task)
{
    GError *error = NULL;

    mm_base_modem_at_sequence_finish (MM_BASE_MODEM (self), res, NULL, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Check whether we did properly set the charset */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CSCS?",
                              3,
                              FALSE,
                              (GAsyncReadyCallback)current_charset_query_ready,
                              task);
}

static void
modem_setup_charset (MMIfaceModem *self,
                     MMModemCharset charset,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    SetupCharsetContext *ctx;
    const gchar *charset_str;
    GTask *task;

    /* NOTE: we already notified that CDMA-only modems couldn't load supported
     * charsets, so we'll never get here in such a case */
    g_assert (mm_iface_modem_is_cdma_only (self) == FALSE);

    /* Build charset string to use */
    charset_str = mm_modem_charset_to_string (charset);
    if (!charset_str) {
        g_task_report_new_error (self,
                                 callback,
                                 user_data,
                                 modem_setup_charset,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Unhandled character set 0x%X",
                                 charset);
        return;
    }

    /* Setup context, including commands to try */
    ctx = g_new0 (SetupCharsetContext, 1);
    ctx->charset = charset;
    /* First try, with quotes */
    ctx->charset_commands[0].command = g_strdup_printf ("+CSCS=\"%s\"", charset_str);
    ctx->charset_commands[0].timeout = 3;
    ctx->charset_commands[0].allow_cached = FALSE;
    ctx->charset_commands[0].response_processor = mm_base_modem_response_processor_no_result;
    /* Second try.
     * Some modems puke if you include the quotes around the character
     * set name, so lets try it again without them.
     */
    ctx->charset_commands[1].command = g_strdup_printf ("+CSCS=%s", charset_str);
    ctx->charset_commands[1].timeout = 3;
    ctx->charset_commands[1].allow_cached = FALSE;
    ctx->charset_commands[1].response_processor = mm_base_modem_response_processor_no_result;

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, (GDestroyNotify)setup_charset_context_free);

    /* Launch sequence */
    mm_base_modem_at_sequence (
        MM_BASE_MODEM (self),
        (const MMBaseModemAtCommand *)ctx->charset_commands,
        NULL, /* response_processor_context */
        NULL, /* response_processor_context_free */
        (GAsyncReadyCallback)charset_change_ready,
        task);
}

/*****************************************************************************/
/* Loading supported charsets (Modem interface) */

static MMModemCharset
modem_load_supported_charsets_finish (MMIfaceModem *self,
                                      GAsyncResult *res,
                                      GError **error)
{
    GError *inner_error = NULL;
    gssize value;

    value = g_task_propagate_int (G_TASK (res), &inner_error);
    if (inner_error) {
        g_propagate_error (error, inner_error);
        return MM_MODEM_CHARSET_UNKNOWN;
    }
    return (MMModemCharset)value;
}

static void
cscs_format_check_ready (MMBaseModem *self,
                         GAsyncResult *res,
                         GTask *task)
{
    MMModemCharset charsets = MM_MODEM_CHARSET_UNKNOWN;
    const gchar *response;
    GError *error = NULL;

    response = mm_base_modem_at_command_finish (self, res, &error);
    if (error)
        g_task_return_error (task, error);
    else if (!mm_3gpp_parse_cscs_test_response (response, &charsets))
        g_task_return_new_error (
            task,
            MM_CORE_ERROR,
            MM_CORE_ERROR_FAILED,
            "Failed to parse the supported character sets response");
    else
        g_task_return_int (task, charsets);

    g_object_unref (task);
}

static void
modem_load_supported_charsets (MMIfaceModem *self,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* CDMA-only modems don't need this */
    if (mm_iface_modem_is_cdma_only (self)) {
        mm_obj_dbg (self, "skipping supported charset loading in CDMA-only modem...");
        g_task_return_int (task, MM_MODEM_CHARSET_UNKNOWN);
        g_object_unref (task);
        return;
    }

    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CSCS=?",
                              3,
                              TRUE,
                              (GAsyncReadyCallback)cscs_format_check_ready,
                              task);
}

/*****************************************************************************/
/* configuring flow control (Modem interface) */

static gboolean
modem_setup_flow_control_finish (MMIfaceModem  *self,
                                 GAsyncResult  *res,
                                 GError       **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
ifc_test_ready (MMBaseModem  *_self,
                GAsyncResult *res,
                GTask        *task)
{
    MMBroadbandModem *self;
    GError           *error = NULL;
    const gchar      *response;
    const gchar      *cmd;
    MMFlowControl     flow_control_supported;
    MMFlowControl     flow_control_selected = MM_FLOW_CONTROL_UNKNOWN;
    MMFlowControl     flow_control_requested;
    gchar            *flow_control_supported_str = NULL;
    gchar            *flow_control_selected_str = NULL;
    MMPortSerialAt   *port;

    self = MM_BROADBAND_MODEM (_self);

    /* Completely ignore errors in AT+IFC=? */
    response = mm_base_modem_at_command_finish (_self, res, &error);
    if (!response)
        goto out;

    /* Parse response */
    flow_control_supported = mm_parse_ifc_test_response (response, self, &error);
    if (flow_control_supported == MM_FLOW_CONTROL_UNKNOWN)
        goto out;
    flow_control_supported_str = mm_flow_control_build_string_from_mask (flow_control_supported);

    port = mm_base_modem_peek_port_primary (_self);
    if (!port) {
        g_set_error (&error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "No primary AT port");
        goto out;
    }

    flow_control_requested = mm_port_serial_get_flow_control (MM_PORT_SERIAL (port));
    if (flow_control_requested != MM_FLOW_CONTROL_UNKNOWN) {
        gchar *flow_control_requested_str;

        flow_control_requested_str = mm_flow_control_build_string_from_mask (flow_control_requested);

        /* If flow control settings requested via udev tag are not supported by
         * the modem, we trigger a fatal error */
        if (!(flow_control_supported & flow_control_requested)) {
            g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_UNSUPPORTED,
                                     "Explicitly requested flow control settings (%s) are not supported by the device (%s)",
                                     flow_control_requested_str, flow_control_supported_str);
            g_object_unref (task);
            g_free (flow_control_requested_str);
            g_free (flow_control_supported_str);
            return;
        }

        mm_obj_dbg (self, "flow control settings explicitly requested: %s", flow_control_requested_str);
        flow_control_selected = flow_control_requested;
        flow_control_selected_str = flow_control_requested_str;
    } else {
        /* If flow control is not set explicitly by udev tags,
         * we prefer the methods in this order:
         *  RTS/CTS
         *  XON/XOFF
         *  None.
         */
        if (flow_control_supported & MM_FLOW_CONTROL_RTS_CTS)
            flow_control_selected = MM_FLOW_CONTROL_RTS_CTS;
        else if (flow_control_supported & MM_FLOW_CONTROL_XON_XOFF)
            flow_control_selected = MM_FLOW_CONTROL_XON_XOFF;
        else if (flow_control_supported & MM_FLOW_CONTROL_NONE)
            flow_control_selected = MM_FLOW_CONTROL_NONE;
        else
            g_assert_not_reached ();
        flow_control_selected_str = mm_flow_control_build_string_from_mask (flow_control_selected);
        mm_obj_dbg (self, "flow control settings automatically selected: %s", flow_control_selected_str);
    }

    /* Select flow control for all connections */
    self->priv->flow_control = flow_control_selected;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FLOW_CONTROL]);

    /* Set flow control settings and ignore result */
    switch (flow_control_selected) {
    case MM_FLOW_CONTROL_RTS_CTS:
        cmd = "+IFC=2,2";
        break;
    case MM_FLOW_CONTROL_XON_XOFF:
        cmd = "+IFC=1,1";
        break;
    case MM_FLOW_CONTROL_NONE:
        cmd = "+IFC=0,0";
        break;
    case MM_FLOW_CONTROL_UNKNOWN:
    default:
        g_assert_not_reached ();
    }
    mm_base_modem_at_command (_self, cmd, 3, FALSE, NULL, NULL);

out:
    g_free (flow_control_supported_str);
    g_free (flow_control_selected_str);

    /* Ignore errors */
    if (error) {
        mm_obj_dbg (self, "couldn't load supported flow control methods: %s", error->message);
        g_error_free (error);
    }

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_setup_flow_control (MMIfaceModem *self,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* Query supported flow control methods */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+IFC=?",
                              3,
                              TRUE,
                              (GAsyncReadyCallback)ifc_test_ready,
                              task);
}

/*****************************************************************************/
/* Power state loading (Modem interface) */

static MMModemPowerState
modem_load_power_state_finish (MMIfaceModem *self,
                               GAsyncResult *res,
                               GError **error)
{
    GError *inner_error = NULL;
    gssize value;

    value = g_task_propagate_int (G_TASK (res), &inner_error);
    if (inner_error) {
        g_propagate_error (error, inner_error);
        return MM_MODEM_POWER_STATE_UNKNOWN;
    }
    return (MMModemPowerState)value;
}

static void
cfun_query_ready (MMBaseModem *self,
                  GAsyncResult *res,
                  GTask *task)
{
    const gchar *result;
    MMModemPowerState state;
    GError *error = NULL;

    result = mm_base_modem_at_command_finish (self, res, &error);
    if (!result || !mm_3gpp_parse_cfun_query_generic_response (result, &state, &error))
        g_task_return_error (task, error);
    else
        g_task_return_int (task, state);;

    g_object_unref (task);
}

static void
modem_load_power_state (MMIfaceModem *self,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* CDMA-only modems don't need this */
    if (mm_iface_modem_is_cdma_only (self)) {
        mm_obj_dbg (self, "assuming full power state in CDMA-only modem...");
        g_task_return_int (task, MM_MODEM_POWER_STATE_ON);
        g_object_unref (task);
        return;
    }

    mm_obj_dbg (self, "loading power state...");
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CFUN?",
                              3,
                              FALSE,
                              (GAsyncReadyCallback)cfun_query_ready,
                              task);
}

/*****************************************************************************/
/* Powering up the modem (Modem interface) */

static gboolean
modem_power_up_finish (MMIfaceModem *self,
                       GAsyncResult *res,
                       GError **error)
{
    /* By default, errors in the power up command are ignored.
     * Plugins wanting to treat power up errors should subclass the power up
     * handling. */
    return TRUE;
}

static void
modem_power_up (MMIfaceModem *self,
                GAsyncReadyCallback callback,
                gpointer user_data)
{
    GTask *task;

    /* CDMA-only modems don't need this */
    if (mm_iface_modem_is_cdma_only (self))
        mm_obj_dbg (self, "skipping power-up in CDMA-only modem...");
    else
        mm_base_modem_at_command (MM_BASE_MODEM (self),
                                  "+CFUN=1",
                                  5,
                                  FALSE,
                                  NULL,
                                  NULL);

    task = g_task_new (self, NULL, callback, user_data);
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

/*****************************************************************************/
/* Reprobing the modem if the SIM changed across a power-off or power-down */

#define SIM_SWAP_CHECK_LOAD_RETRIES_MAX 3

typedef enum {
    SIM_SWAP_CHECK_STEP_FIRST,
    SIM_SWAP_CHECK_STEP_ICCID_CHANGED,
    SIM_SWAP_CHECK_STEP_IMSI_CHANGED,
    SIM_SWAP_CHECK_STEP_LAST,
} SimSwapCheckStep;

typedef struct {
    MMBaseSim        *sim;
    guint             retries;
    gchar            *iccid;
    gboolean          iccid_check_done;
    gchar            *imsi;
    gboolean          imsi_check_done;
    SimSwapCheckStep  step;
} SimSwapContext;

static void
sim_swap_context_free (SimSwapContext *ctx)
{
    g_free (ctx->iccid);
    g_free (ctx->imsi);
    g_clear_object (&ctx->sim);
    g_slice_free (SimSwapContext, ctx);
}

static gboolean
modem_check_for_sim_swap_finish (MMIfaceModem  *self,
                                 GAsyncResult  *res,
                                 GError       **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static gboolean load_sim_identifier (GTask *task);
static gboolean load_sim_imsi       (GTask *task);
static void     sim_swap_check_step (GTask *task);

static void
complete_sim_swap_check (GTask       *task,
                         const gchar *current)
{
    MMBroadbandModem *self;
    SimSwapContext   *ctx;
    const gchar      *cached;
    const gchar      *str;
    gboolean          ignore_sim_event = FALSE;
    gboolean          same;

    self = MM_BROADBAND_MODEM (g_task_get_source_object (task));
    ctx = g_task_get_task_data (task);

    if (ctx->step == SIM_SWAP_CHECK_STEP_ICCID_CHANGED) {
        ctx->iccid_check_done = TRUE;
        cached = mm_gdbus_sim_get_sim_identifier (MM_GDBUS_SIM (ctx->sim));
        str = "identifier";
    } else if (ctx->step == SIM_SWAP_CHECK_STEP_IMSI_CHANGED) {
        ctx->imsi_check_done = TRUE;
        cached = mm_gdbus_sim_get_imsi (MM_GDBUS_SIM (ctx->sim));
        str = "imsi";

        /* If the modem is locked and we couldn't previously read the IMSI
         * (because it was locked) then the IMSI change is probably not a swap.
         */
        ignore_sim_event = (self->priv->modem_state == MM_MODEM_STATE_LOCKED && !cached);
    } else
        g_assert_not_reached();

    same = (g_strcmp0 (current, cached) == 0);
    if (same) {
        mm_obj_info (self, "SIM %s has not changed: %s",
                     str, mm_log_str_personal_info (current));
    } else {
        mm_obj_msg (self, "SIM %s has changed: '%s' -> '%s'",
                    str,
                    mm_log_str_personal_info (cached ? cached : ""),
                    mm_log_str_personal_info (current ? current : ""));
    }

    if (same || ignore_sim_event) {
        ctx->step++;
    } else {
        mm_iface_modem_process_sim_event (MM_IFACE_MODEM (self));
        ctx->step = SIM_SWAP_CHECK_STEP_LAST;
    }

    sim_swap_check_step (task);
 }

static void
load_sim_step_ready (MMBaseSim    *sim,
                     GAsyncResult *res,
                     GTask        *task)
{
    MMBroadbandModem  *self;
    SimSwapContext    *ctx;
    g_autofree gchar  *current = NULL;
    g_autoptr(GError)  error = NULL;
    const gchar       *str;

    self = MM_BROADBAND_MODEM (g_task_get_source_object (task));
    ctx = g_task_get_task_data (task);

    if (ctx->step == SIM_SWAP_CHECK_STEP_ICCID_CHANGED) {
        current = mm_base_sim_load_sim_identifier_finish (sim, res, &error);
        str = "identifier";
    } else if (ctx->step == SIM_SWAP_CHECK_STEP_IMSI_CHANGED) {
        current = MM_BASE_SIM_GET_CLASS (sim)->load_imsi_finish (sim, res, &error);
        str = "imsi";
    } else
        g_assert_not_reached();

    if (error) {
        if (g_error_matches (error, MM_CORE_ERROR, MM_CORE_ERROR_UNSUPPORTED)) {
            /* Skip checking this field right away */
            ctx->step++;
            sim_swap_check_step (task);
            return;
        }

        if (ctx->retries > 0) {
            mm_obj_warn (self, "could not load SIM %s: %s (%d retries left)",
                         str, error->message, ctx->retries);
            --ctx->retries;
            if (ctx->step == SIM_SWAP_CHECK_STEP_ICCID_CHANGED)
                g_timeout_add_seconds (1, (GSourceFunc) load_sim_identifier, task);
            else if (ctx->step == SIM_SWAP_CHECK_STEP_IMSI_CHANGED)
                g_timeout_add_seconds (1, (GSourceFunc) load_sim_imsi, task);
            else
                g_assert_not_reached();
            return;
        }

        mm_obj_warn (self, "could not load SIM %s: %s", str, error->message);
        complete_sim_swap_check (task, NULL);
        return;
    }

    complete_sim_swap_check (task, current);
 }

static gboolean
load_sim_identifier (GTask *task)
{
    SimSwapContext *ctx;

    ctx = g_task_get_task_data (task);

    mm_base_sim_load_sim_identifier (ctx->sim,
                                     (GAsyncReadyCallback)load_sim_step_ready,
                                     task);

    return G_SOURCE_REMOVE;
}

static gboolean
load_sim_imsi (GTask *task)
{
    SimSwapContext *ctx;

    ctx = g_task_get_task_data (task);

    if (MM_BASE_SIM_GET_CLASS (ctx->sim)->load_imsi &&
        MM_BASE_SIM_GET_CLASS (ctx->sim)->load_imsi_finish) {
        MM_BASE_SIM_GET_CLASS (ctx->sim)->load_imsi (ctx->sim,
                                                     (GAsyncReadyCallback)load_sim_step_ready,
                                                     task);
    } else {
        ctx->step++;
        sim_swap_check_step (task);
    }

    return G_SOURCE_REMOVE;
}

static void
sim_swap_check_step (GTask *task)
{
    SimSwapContext *ctx;

    ctx  = g_task_get_task_data (task);

    switch (ctx->step) {
    case SIM_SWAP_CHECK_STEP_FIRST:
        ctx->step++;
        /* fall-through */

    case SIM_SWAP_CHECK_STEP_ICCID_CHANGED:
        ctx->retries = SIM_SWAP_CHECK_LOAD_RETRIES_MAX;
        load_sim_identifier (task);
        return;

    case SIM_SWAP_CHECK_STEP_IMSI_CHANGED:
        ctx->retries = SIM_SWAP_CHECK_LOAD_RETRIES_MAX;
        load_sim_imsi (task);
        return;

    case SIM_SWAP_CHECK_STEP_LAST:
        if (!ctx->iccid_check_done && !ctx->imsi_check_done)
            g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                                     "Couldn't do either ICCID or IMSI check");
        else
            g_task_return_boolean (task, TRUE);
        g_object_unref (task);
        return;

    default:
        g_assert_not_reached ();
    }
}

static void
modem_check_for_sim_swap (MMIfaceModem        *self,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
    GTask          *task;
    SimSwapContext *ctx;

    mm_obj_info (self, "checking if SIM was swapped...");

    task = g_task_new (self, NULL, callback, user_data);
    ctx = g_slice_new0 (SimSwapContext);
    ctx->step = SIM_SWAP_CHECK_STEP_FIRST;
    g_task_set_task_data (task, ctx, (GDestroyNotify)sim_swap_context_free);

    g_object_get (self,
                  MM_IFACE_MODEM_SIM, &ctx->sim,
                  NULL);
    if (!ctx->sim) {
        MMModemState modem_state;

        modem_state = MM_MODEM_STATE_UNKNOWN;
        g_object_get (self,
                      MM_IFACE_MODEM_STATE, &modem_state,
                      NULL);

        if (modem_state == MM_MODEM_STATE_FAILED) {
            mm_obj_msg (self, "new SIM detected, handle as SIM hot-swap");
            mm_iface_modem_process_sim_event (MM_IFACE_MODEM (self));
            g_task_return_boolean (task, TRUE);
        } else {
            g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                                     "could not acquire SIM object");
        }
        g_object_unref (task);
        return;
    }

    sim_swap_check_step (task);
}

/*****************************************************************************/
/* Sending a command to the modem (Modem interface) */

static const gchar *
modem_command_finish (MMIfaceModem *self,
                      GAsyncResult *res,
                      GError **error)
{
    return mm_base_modem_at_command_finish (MM_BASE_MODEM (self),
                                            res,
                                            error);
}

static void
modem_command (MMIfaceModem *self,
               const gchar *cmd,
               guint timeout,
               GAsyncReadyCallback callback,
               gpointer user_data)
{

    mm_base_modem_at_command (MM_BASE_MODEM (self), cmd, timeout,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* IMEI loading (3GPP interface) */

static gchar *
modem_3gpp_load_imei_finish (MMIfaceModem3gpp *self,
                             GAsyncResult *res,
                             GError **error)
{
    const gchar *result;
    gchar *imei = NULL;

    result = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
    if (!result)
        return NULL;

    result = mm_strip_tag (result, "+CGSN:");
    mm_parse_gsn (result, &imei, NULL, NULL);
    mm_obj_dbg (self, "loaded IMEI: %s", imei);
    return imei;
}

static void
modem_3gpp_load_imei (MMIfaceModem3gpp *self,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
    mm_obj_dbg (self, "loading IMEI...");
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CGSN",
                              3,
                              TRUE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Facility locks status loading (3GPP interface) */

typedef struct {
    guint current;
    MMModem3gppFacility facilities;
    MMModem3gppFacility locks;
} LoadEnabledFacilityLocksContext;

static void get_next_facility_lock_status (GTask *task);

static MMModem3gppFacility
modem_3gpp_load_enabled_facility_locks_finish (MMIfaceModem3gpp *self,
                                               GAsyncResult *res,
                                               GError **error)
{
    GError *inner_error = NULL;
    gssize value;

    value = g_task_propagate_int (G_TASK (res), &inner_error);
    if (inner_error) {
        g_propagate_error (error, inner_error);
        return MM_MODEM_3GPP_FACILITY_NONE;
    }
    return (MMModem3gppFacility)value;
}

static void
clck_single_query_ready (MMBaseModem *self,
                         GAsyncResult *res,
                         GTask *task)
{
    LoadEnabledFacilityLocksContext *ctx;
    const gchar *response;
    gboolean enabled = FALSE;

    ctx = g_task_get_task_data (task);

    response = mm_base_modem_at_command_finish (self, res, NULL);
    if (response &&
        mm_3gpp_parse_clck_write_response (response, &enabled) &&
        enabled) {
        ctx->locks |= (1 << ctx->current);
    } else {
        /* On errors, we'll just assume disabled */
        ctx->locks &= ~(1 << ctx->current);
    }

    /* And go on with the next one */
    ctx->current++;
    get_next_facility_lock_status (task);
}

static void
get_next_facility_lock_status (GTask *task)
{
    MMBroadbandModem *self;
    LoadEnabledFacilityLocksContext *ctx;
    guint i;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    for (i = ctx->current; i < sizeof (MMModem3gppFacility) * 8; i++) {
        guint32 facility = 1u << i;

        /* Found the next one to query! */
        if (ctx->facilities & facility) {
            gchar *cmd;

            /* Keep the current one */
            ctx->current = i;

            /* Query current */
            cmd = g_strdup_printf ("+CLCK=\"%s\",2",
                                   mm_3gpp_facility_to_acronym (facility));
            mm_base_modem_at_command (MM_BASE_MODEM (self),
                                      cmd,
                                      3,
                                      FALSE,
                                      (GAsyncReadyCallback)clck_single_query_ready,
                                      task);
            g_free (cmd);
            return;
        }
    }

    /* No more facilities to query, all done */
    g_task_return_int (task, ctx->locks);
    g_object_unref (task);
}

static void
clck_test_ready (MMBaseModem *self,
                 GAsyncResult *res,
                 GTask *task)
{
    LoadEnabledFacilityLocksContext *ctx;
    const gchar *response;
    GError *error = NULL;

    response = mm_base_modem_at_command_finish (self, res, &error);
    if (!response) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    ctx = g_task_get_task_data (task);

    if (!mm_3gpp_parse_clck_test_response (response, &ctx->facilities)) {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Couldn't parse list of available lock facilities: '%s'",
                                 response);
        g_object_unref (task);
        return;
    }

    /* Ignore facility locks specified by the plugins */
    if (MM_BROADBAND_MODEM (self)->priv->modem_3gpp_ignored_facility_locks) {
        gchar *str;

        str = mm_modem_3gpp_facility_build_string_from_mask (MM_BROADBAND_MODEM (self)->priv->modem_3gpp_ignored_facility_locks);
        mm_obj_dbg (self, "ignoring facility locks: '%s'", str);
        g_free (str);

        ctx->facilities &= ~MM_BROADBAND_MODEM (self)->priv->modem_3gpp_ignored_facility_locks;
    }

    /* Go on... */
    get_next_facility_lock_status (task);
}

static void
modem_3gpp_load_enabled_facility_locks (MMIfaceModem3gpp *self,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data)
{
    LoadEnabledFacilityLocksContext *ctx;
    GTask *task;

    ctx = g_new (LoadEnabledFacilityLocksContext, 1);
    ctx->facilities = MM_MODEM_3GPP_FACILITY_NONE;
    ctx->locks = MM_MODEM_3GPP_FACILITY_NONE;
    ctx->current = 0;

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, g_free);

    mm_obj_dbg (self, "loading enabled facility locks...");
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CLCK=?",
                              3,
                              TRUE,
                              (GAsyncReadyCallback)clck_test_ready,
                              task);
}

/*****************************************************************************/
/* Operator Code loading (3GPP interface) */

static gchar *
modem_3gpp_load_operator_code_finish (MMIfaceModem3gpp *self,
                                      GAsyncResult *res,
                                      GError **error)
{
    const gchar *result;
    gchar *operator_code = NULL;

    result = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
    if (!result)
        return NULL;

    if (!mm_3gpp_parse_cops_read_response (result,
                                           NULL, /* mode */
                                           NULL, /* format */
                                           &operator_code,
                                           NULL, /* act */
                                           self,
                                           error))
        return NULL;

    mm_3gpp_normalize_operator (&operator_code, MM_BROADBAND_MODEM (self)->priv->modem_current_charset, self);
    if (operator_code)
        mm_obj_dbg (self, "loaded Operator Code: %s", operator_code);
    return operator_code;
}

static void
modem_3gpp_load_operator_code (MMIfaceModem3gpp *self,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
    mm_obj_dbg (self, "loading Operator Code...");
    mm_base_modem_at_command (MM_BASE_MODEM (self), "+COPS=3,2", 3, FALSE, NULL, NULL);
    mm_base_modem_at_command (MM_BASE_MODEM (self), "+COPS?", 3, FALSE, callback, user_data);
}

/*****************************************************************************/
/* Operator Name loading (3GPP interface) */

static gchar *
modem_3gpp_load_operator_name_finish (MMIfaceModem3gpp *self,
                                      GAsyncResult *res,
                                      GError **error)
{
    const gchar *result;
    gchar *operator_name = NULL;

    result = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
    if (!result)
        return NULL;

    if (!mm_3gpp_parse_cops_read_response (result,
                                           NULL, /* mode */
                                           NULL, /* format */
                                           &operator_name,
                                           NULL, /* act */
                                           self,
                                           error))
        return NULL;

    mm_3gpp_normalize_operator (&operator_name, MM_BROADBAND_MODEM (self)->priv->modem_current_charset, self);
    if (operator_name)
        mm_obj_dbg (self, "loaded Operator Name: %s", operator_name);
    return operator_name;
}

static void
modem_3gpp_load_operator_name (MMIfaceModem3gpp *self,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
    mm_obj_dbg (self, "loading Operator Name...");
    mm_base_modem_at_command (MM_BASE_MODEM (self), "+COPS=3,0", 3, FALSE, NULL, NULL);
    mm_base_modem_at_command (MM_BASE_MODEM (self), "+COPS?", 3, FALSE, callback, user_data);
}

/*****************************************************************************/
/* UE mode of operation for EPS loading (3GPP interface) */

static MMModem3gppEpsUeModeOperation
modem_3gpp_load_eps_ue_mode_operation_finish (MMIfaceModem3gpp  *self,
                                              GAsyncResult      *res,
                                              GError          **error)
{
    MMModem3gppEpsUeModeOperation  mode;
    const gchar                   *result;

    result = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
    if (!result || !mm_3gpp_parse_cemode_query_response (result, &mode, error))
        return MM_MODEM_3GPP_EPS_UE_MODE_OPERATION_UNKNOWN;

    return mode;
}

static void
modem_3gpp_load_eps_ue_mode_operation (MMIfaceModem3gpp    *self,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
    mm_obj_dbg (self, "loading UE mode of operation for EPS...");
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CEMODE?",
                              3,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* UE mode of operation for EPS setting (3GPP interface) */

static gboolean
modem_3gpp_set_eps_ue_mode_operation_finish (MMIfaceModem3gpp  *self,
                                             GAsyncResult      *res,
                                             GError          **error)
{
    return !!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
}

static void
modem_3gpp_set_eps_ue_mode_operation (MMIfaceModem3gpp              *self,
                                      MMModem3gppEpsUeModeOperation  mode,
                                      GAsyncReadyCallback            callback,
                                      gpointer                       user_data)
{
    gchar *cmd;

    mm_obj_dbg (self, "updating UE mode of operation for EPS...");
    cmd = mm_3gpp_build_cemode_set_request (mode);
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              cmd,
                              3,
                              FALSE,
                              callback,
                              user_data);
    g_free (cmd);
}

/*****************************************************************************/
/* Set packet service state (3GPP interface) */

static gboolean
modem_3gpp_set_packet_service_state_finish (MMIfaceModem3gpp  *self,
                                            GAsyncResult      *res,
                                            GError          **error)
{
    return !!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
}

static void
modem_3gpp_set_packet_service_state (MMIfaceModem3gpp              *self,
                                     MMModem3gppPacketServiceState  state,
                                     GAsyncReadyCallback            callback,
                                     gpointer                       user_data)
{
    g_autofree gchar *cmd = NULL;

    cmd = mm_3gpp_build_cgatt_set_request (state);
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              cmd,
                              10,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Unsolicited registration messages handling (3GPP interface) */

static gboolean
modem_3gpp_setup_unsolicited_registration_events_finish (MMIfaceModem3gpp *self,
                                                         GAsyncResult *res,
                                                         GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
registration_state_changed (MMPortSerialAt *port,
                            GMatchInfo *match_info,
                            MMBroadbandModem *self)
{
    MMModem3gppRegistrationState state = MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;
    gulong lac = 0, tac = 0, cell_id = 0;
    MMModemAccessTechnology act = MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN;
    gboolean cgreg = FALSE;
    gboolean cereg = FALSE;
    gboolean c5greg = FALSE;
    GError *error = NULL;

    if (!mm_3gpp_parse_creg_response (match_info,
                                      self,
                                      &state,
                                      &lac,
                                      &cell_id,
                                      &act,
                                      &cgreg,
                                      &cereg,
                                      &c5greg,
                                      &error)) {
        mm_obj_warn (self, "error parsing unsolicited registration: %s",
                     error && error->message ? error->message : "(unknown)");
        g_clear_error (&error);
        return;
    }

    /* Report new registration state and fix LAC/TAC.
     * According to 3GPP TS 27.007:
     *  - If CREG reports <AcT> 7 (LTE) then the <lac> field contains TAC
     *  - CEREG/C5GREG always reports TAC
     */
    if (cgreg)
        mm_iface_modem_3gpp_update_ps_registration_state (MM_IFACE_MODEM_3GPP (self), state, FALSE);
    else if (cereg) {
        tac = lac;
        lac = 0;
        mm_iface_modem_3gpp_update_eps_registration_state (MM_IFACE_MODEM_3GPP (self), state, FALSE);
    } else if (c5greg) {
        tac = lac;
        lac = 0;
        mm_iface_modem_3gpp_update_5gs_registration_state (MM_IFACE_MODEM_3GPP (self), state, FALSE);
    } else {
        if (act == MM_MODEM_ACCESS_TECHNOLOGY_LTE) {
            tac = lac;
            lac = 0;
        }
        mm_iface_modem_3gpp_update_cs_registration_state (MM_IFACE_MODEM_3GPP (self), state, FALSE);
    }

    /* Only update access technologies from CREG/CGREG response if the modem
     * doesn't have custom commands for access technology loading, otherwise
     * we fight with the custom commands.  Plus CREG/CGREG access technologies
     * don't have fine-grained distinction between HSxPA or GPRS/EDGE, etc.
     */
    if (MM_IFACE_MODEM_GET_IFACE (self)->load_access_technologies == modem_load_access_technologies ||
        MM_IFACE_MODEM_GET_IFACE (self)->load_access_technologies == NULL)
        mm_iface_modem_3gpp_update_access_technologies (MM_IFACE_MODEM_3GPP (self), act);

    mm_iface_modem_3gpp_update_location (MM_IFACE_MODEM_3GPP (self), lac, tac, cell_id);
}

static void
modem_3gpp_setup_unsolicited_registration_events (MMIfaceModem3gpp *self,
                                                  GAsyncReadyCallback callback,
                                                  gpointer user_data)
{
    MMPortSerialAt *ports[2];
    GPtrArray *array;
    guint i;
    guint j;
    GTask *task;

    ports[0] = mm_base_modem_peek_port_primary (MM_BASE_MODEM (self));
    ports[1] = mm_base_modem_peek_port_secondary (MM_BASE_MODEM (self));

    /* Set up CREG unsolicited message handlers in both ports */
    array = mm_3gpp_creg_regex_get (FALSE);
    for (i = 0; i < 2; i++) {
        if (!ports[i])
            continue;

        mm_obj_dbg (self, "setting up 3GPP unsolicited registration messages handlers in %s",
                    mm_port_get_device (MM_PORT (ports[i])));
        for (j = 0; j < array->len; j++) {
            mm_port_serial_at_add_unsolicited_msg_handler (
                MM_PORT_SERIAL_AT (ports[i]),
                (GRegex *) g_ptr_array_index (array, j),
                (MMPortSerialAtUnsolicitedMsgFn)registration_state_changed,
                self,
                NULL);
        }
    }
    mm_3gpp_creg_regex_destroy (array);

    task = g_task_new (self, NULL, callback, user_data);
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

/*****************************************************************************/
/* Unsolicited registration messages cleaning up (3GPP interface) */

static gboolean
modem_3gpp_cleanup_unsolicited_registration_events_finish (MMIfaceModem3gpp *self,
                                                           GAsyncResult *res,
                                                           GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
modem_3gpp_cleanup_unsolicited_registration_events (MMIfaceModem3gpp *self,
                                                    GAsyncReadyCallback callback,
                                                    gpointer user_data)
{
    MMPortSerialAt *ports[2];
    GPtrArray *array;
    guint i;
    guint j;
    GTask *task;

    ports[0] = mm_base_modem_peek_port_primary (MM_BASE_MODEM (self));
    ports[1] = mm_base_modem_peek_port_secondary (MM_BASE_MODEM (self));

    /* Set up CREG unsolicited message handlers in both ports */
    array = mm_3gpp_creg_regex_get (FALSE);
    for (i = 0; i < 2; i++) {
        if (!ports[i])
            continue;

        mm_obj_dbg (self, "cleaning up unsolicited registration messages handlers in %s",
                    mm_port_get_device (MM_PORT (ports[i])));
        for (j = 0; j < array->len; j++) {
            mm_port_serial_at_add_unsolicited_msg_handler (
                MM_PORT_SERIAL_AT (ports[i]),
                (GRegex *) g_ptr_array_index (array, j),
                NULL,
                NULL,
                NULL);
        }
    }
    mm_3gpp_creg_regex_destroy (array);

    task = g_task_new (self, NULL, callback, user_data);
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

/*****************************************************************************/
/* Scan networks (3GPP interface) */

static GList *
modem_3gpp_scan_networks_finish (MMIfaceModem3gpp *self,
                                 GAsyncResult *res,
                                 GError **error)
{
    const gchar *result;

    result = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
    if (!result)
        return NULL;

    return mm_3gpp_parse_cops_test_response (result, MM_BROADBAND_MODEM (self)->priv->modem_current_charset, self, error);
}

static void
modem_3gpp_scan_networks (MMIfaceModem3gpp *self,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+COPS=?",
                              315,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Register in network (3GPP interface) */

typedef struct {
    gchar         *operator_id;
    MMIfacePortAt *port;
} RegisterInNetworkContext;

static void
register_in_network_context_free (RegisterInNetworkContext *ctx)
{
    g_free (ctx->operator_id);
    g_object_unref (ctx->port);
    g_slice_free (RegisterInNetworkContext, ctx);
}

static gboolean
modem_3gpp_register_in_network_finish (MMIfaceModem3gpp  *self,
                                       GAsyncResult      *res,
                                       GError           **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cops_set_ready (MMBaseModem  *self,
                GAsyncResult *res,
                GTask        *task)
{
    GError *error = NULL;

    if (!mm_base_modem_at_command_full_finish (MM_BASE_MODEM (self), res, &error))
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
cops_ascii_set_ready (MMBaseModem  *_self,
                      GAsyncResult *res,
                      GTask        *task)
{
    MMBroadbandModem  *self = MM_BROADBAND_MODEM (_self);
    g_autoptr(GError)  error = NULL;

    if (!mm_base_modem_at_command_full_finish (_self, res, &error)) {
        RegisterInNetworkContext *ctx;
        g_autoptr(GError)         enc_error = NULL;
        g_autofree gchar         *operator_id_enc = NULL;
        g_autofree gchar         *command = NULL;

        if (!g_error_matches (error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_SUPPORTED)) {
            g_task_return_error (task, g_steal_pointer (&error));
            g_object_unref (task);
            return;
        }

        /* If it failed with an unsupported error, retry with current modem charset */
        ctx = g_task_get_task_data (task);
        operator_id_enc = mm_modem_charset_str_from_utf8 (ctx->operator_id, self->priv->modem_current_charset, FALSE, &enc_error);
        if (!operator_id_enc) {
            mm_obj_dbg (self, "couldn't convert operator id to current charset: %s", enc_error->message);
            g_task_return_error (task, g_steal_pointer (&error));
            g_object_unref (task);
            return;
        }

        /* retry only if encoded string is different to the non-encoded one */
        if (g_strcmp0 (ctx->operator_id, operator_id_enc) == 0) {
            g_task_return_error (task, g_steal_pointer (&error));
            g_object_unref (task);
            return;
        }

        command = g_strdup_printf ("+COPS=1,2,\"%s\"", operator_id_enc);
        mm_base_modem_at_command_full (_self,
                                       ctx->port,
                                       command,
                                       120,
                                       FALSE,
                                       FALSE, /* raw */
                                       g_task_get_cancellable (task),
                                       (GAsyncReadyCallback)cops_set_ready,
                                       task);
        return;
    }

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_3gpp_register_in_network (MMIfaceModem3gpp    *self,
                                const gchar         *operator_id,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
    RegisterInNetworkContext *ctx;
    GTask                    *task;
    MMIfacePortAt            *port;
    GError                   *error = NULL;
    g_autofree gchar         *command = NULL;

    task = g_task_new (self, cancellable, callback, user_data);

    port = mm_base_modem_peek_best_at_port (MM_BASE_MODEM (self), &error);
    if (!port) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    ctx = g_slice_new0 (RegisterInNetworkContext);
    ctx->port = g_object_ref (port);
    ctx->operator_id = g_strdup (operator_id);
    g_task_set_task_data (task, ctx, (GDestroyNotify)register_in_network_context_free);

    /* Trigger automatic network registration if no explicit operator id given */
    if (!ctx->operator_id) {
        /* Note that '+COPS=0,,' (same but with commas) won't work in some Nokia phones */
        mm_base_modem_at_command_full (MM_BASE_MODEM (self),
                                       port,
                                       "+COPS=0",
                                       120,
                                       FALSE,
                                       FALSE, /* raw */
                                       cancellable,
                                       (GAsyncReadyCallback)cops_set_ready,
                                       task);
        return;
    }

    /* Use the operator id given in ASCII initially */
    command = g_strdup_printf ("+COPS=1,2,\"%s\"", ctx->operator_id);
    mm_base_modem_at_command_full (MM_BASE_MODEM (self),
                                   port,
                                   command,
                                   120,
                                   FALSE,
                                   FALSE, /* raw */
                                   cancellable,
                                   (GAsyncReadyCallback)cops_ascii_set_ready,
                                   task);
}

/*****************************************************************************/
/* Registration checks (3GPP interface) */

typedef struct {
    gboolean is_cs_supported;
    gboolean is_ps_supported;
    gboolean is_eps_supported;
    gboolean is_5gs_supported;
    gboolean run_cs;
    gboolean run_ps;
    gboolean run_eps;
    gboolean run_5gs;
    gboolean running_cs;
    gboolean running_ps;
    gboolean running_eps;
    gboolean running_5gs;
    GError *error_cs;
    GError *error_ps;
    GError *error_eps;
    GError *error_5gs;
} RunRegistrationChecksContext;

static void
run_registration_checks_context_free (RunRegistrationChecksContext *ctx)
{
    g_clear_error (&ctx->error_cs);
    g_clear_error (&ctx->error_ps);
    g_clear_error (&ctx->error_eps);
    g_clear_error (&ctx->error_5gs);
    g_free (ctx);
}

static gboolean
modem_3gpp_run_registration_checks_finish (MMIfaceModem3gpp  *self,
                                           GAsyncResult      *res,
                                           GError           **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
run_registration_checks_reload_current_registration_info_ready (MMIfaceModem3gpp *modem,
                                                                GAsyncResult     *res,
                                                                GTask            *task)
{
    GError *error = NULL;

    mm_iface_modem_3gpp_reload_current_registration_info_finish (modem, res, &error);
    if (error)
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void run_registration_checks_context_step (GTask *task);

static void
run_registration_checks_context_set_error (RunRegistrationChecksContext *ctx,
                                           GError                       *error)
{
    g_assert (error != NULL);
    if (ctx->running_cs)
        ctx->error_cs = error;
    else if (ctx->running_ps)
        ctx->error_ps = error;
    else if (ctx->running_eps)
        ctx->error_eps = error;
    else if (ctx->running_5gs)
        ctx->error_5gs = error;
    else
        g_assert_not_reached ();
}

static void
registration_status_check_ready (MMBroadbandModem *self,
                                 GAsyncResult     *res,
                                 GTask            *task)
{
    g_autoptr(GMatchInfo)         match_info = NULL;
    RunRegistrationChecksContext *ctx;
    const gchar                  *response;
    GError                       *error = NULL;
    guint                         i;
    gboolean                      parsed;
    gboolean                      cgreg = FALSE;
    gboolean                      cereg = FALSE;
    gboolean                      c5greg = FALSE;
    MMModem3gppRegistrationState  state = MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;
    MMModemAccessTechnology       act = MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN;
    gulong                        lac = 0;
    gulong                        tac = 0;
    gulong                        cid = 0;

    ctx = g_task_get_task_data (task);

    /* Only one must be running */
    g_assert ((ctx->running_cs + ctx->running_ps + ctx->running_eps + ctx->running_5gs) == 1);

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (!response) {
        run_registration_checks_context_set_error (ctx, error);
        run_registration_checks_context_step (task);
        return;
    }

    /* Unsolicited registration status handlers will usually process the
     * response for us, but just in case they don't, do that here.
     */
    if (!response[0]) {
        /* Done */
        run_registration_checks_context_step (task);
        return;
    }

    /* Try to match the response */
    for (i = 0;
         i < self->priv->modem_3gpp_registration_regex->len;
         i++) {
        if (g_regex_match ((GRegex *)g_ptr_array_index (self->priv->modem_3gpp_registration_regex, i),
                           response,
                           0,
                           &match_info))
            break;
        g_clear_pointer (&match_info, g_match_info_free);
    }

    if (!match_info) {
        error = g_error_new (MM_CORE_ERROR,
                             MM_CORE_ERROR_FAILED,
                             "Unknown registration status response: '%s'",
                             response);
        run_registration_checks_context_set_error (ctx, error);
        run_registration_checks_context_step (task);
        return;
    }

    parsed = mm_3gpp_parse_creg_response (match_info,
                                          self,
                                          &state,
                                          &lac,
                                          &cid,
                                          &act,
                                          &cgreg,
                                          &cereg,
                                          &c5greg,
                                          &error);

    if (!parsed) {
        if (!error)
            error = g_error_new (MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Error parsing registration response: '%s'",
                                 response);
        run_registration_checks_context_set_error (ctx, error);
        run_registration_checks_context_step (task);
        return;
    }

    /* Report new registration state and fix LAC/TAC.
     * According to 3GPP TS 27.007:
     *  - If CREG reports <AcT> 7 (LTE) then the <lac> field contains TAC
     *  - CEREG always reports TAC
     */
    if (cgreg) {
        if (ctx->running_cs)
            mm_obj_dbg (self, "got PS registration state when checking CS registration state");
        else if (ctx->running_eps)
            mm_obj_dbg (self, "got PS registration state when checking EPS registration state");
        else if (ctx->running_5gs)
            mm_obj_dbg (self, "got PS registration state when checking 5GS registration state");
        mm_iface_modem_3gpp_update_ps_registration_state (MM_IFACE_MODEM_3GPP (self), state, FALSE);
    } else if (cereg) {
        tac = lac;
        lac = 0;
        if (ctx->running_cs)
            mm_obj_dbg (self, "got EPS registration state when checking CS registration state");
        else if (ctx->running_ps)
            mm_obj_dbg (self, "got EPS registration state when checking PS registration state");
        else if (ctx->running_5gs)
            mm_obj_dbg (self, "got EPS registration state when checking 5GS registration state");
        mm_iface_modem_3gpp_update_eps_registration_state (MM_IFACE_MODEM_3GPP (self), state, FALSE);
    } else if (c5greg) {
        tac = lac;
        lac = 0;
        if (ctx->running_cs)
            mm_obj_dbg (self, "got 5GS registration state when checking CS registration state");
        else if (ctx->running_ps)
            mm_obj_dbg (self, "got 5GS registration state when checking PS registration state");
        else if (ctx->running_eps)
            mm_obj_dbg (self, "got 5GS registration state when checking EPS registration state");
        mm_iface_modem_3gpp_update_5gs_registration_state (MM_IFACE_MODEM_3GPP (self), state, FALSE);
    } else {
        if (act == MM_MODEM_ACCESS_TECHNOLOGY_LTE) {
            tac = lac;
            lac = 0;
        }
        if (ctx->running_ps)
            mm_obj_dbg (self, "got CS registration state when checking PS registration state");
        else if (ctx->running_eps)
            mm_obj_dbg (self, "got CS registration state when checking EPS registration state");
        else if (ctx->running_5gs)
            mm_obj_dbg (self, "got CS registration state when checking 5GS registration state");
        mm_iface_modem_3gpp_update_cs_registration_state (MM_IFACE_MODEM_3GPP (self), state, FALSE);
    }

    mm_iface_modem_3gpp_update_access_technologies (MM_IFACE_MODEM_3GPP (self), act);
    mm_iface_modem_3gpp_update_location (MM_IFACE_MODEM_3GPP (self), lac, tac, cid);

    run_registration_checks_context_step (task);
}

static void
run_registration_checks_context_step (GTask *task)
{
    MMBroadbandModem *self;
    RunRegistrationChecksContext *ctx;
    GError *error = NULL;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    ctx->running_cs = FALSE;
    ctx->running_ps = FALSE;
    ctx->running_eps = FALSE;
    ctx->running_5gs = FALSE;

    if (ctx->run_cs) {
        ctx->running_cs = TRUE;
        ctx->run_cs = FALSE;
        /* Check current CS-registration state. */
        mm_base_modem_at_command (MM_BASE_MODEM (self),
                                  "+CREG?",
                                  10,
                                  FALSE,
                                  (GAsyncReadyCallback)registration_status_check_ready,
                                  task);
        return;
    }

    if (ctx->run_ps) {
        ctx->running_ps = TRUE;
        ctx->run_ps = FALSE;
        /* Check current PS-registration state. */
        mm_base_modem_at_command (MM_BASE_MODEM (self),
                                  "+CGREG?",
                                  10,
                                  FALSE,
                                  (GAsyncReadyCallback)registration_status_check_ready,
                                  task);
        return;
    }

    if (ctx->run_eps) {
        ctx->running_eps = TRUE;
        ctx->run_eps = FALSE;
        /* Check current EPS-registration state. */
        mm_base_modem_at_command (MM_BASE_MODEM (self),
                                  "+CEREG?",
                                  10,
                                  FALSE,
                                  (GAsyncReadyCallback)registration_status_check_ready,
                                  task);
        return;
    }

    if (ctx->run_5gs) {
        ctx->running_5gs = TRUE;
        ctx->run_5gs = FALSE;
        /* Check current 5GS-registration state. */
        mm_base_modem_at_command (MM_BASE_MODEM (self),
                                  "+C5GREG?",
                                  10,
                                  FALSE,
                                  (GAsyncReadyCallback)registration_status_check_ready,
                                  task);
        return;
    }

    /* If all run checks returned errors we fail */
    if ((ctx->is_cs_supported || ctx->is_ps_supported || ctx->is_eps_supported || ctx->is_5gs_supported) &&
        (!ctx->is_cs_supported || ctx->error_cs) &&
        (!ctx->is_ps_supported || ctx->error_ps) &&
        (!ctx->is_eps_supported || ctx->error_eps) &&
        (!ctx->is_5gs_supported || ctx->error_5gs)) {
        /* When reporting errors, prefer the 5GS, then EPS, then PS, then CS */
        if (ctx->error_5gs)
            error = g_steal_pointer (&ctx->error_5gs);
        else if (ctx->error_eps)
            error = g_steal_pointer (&ctx->error_eps);
        else if (ctx->error_ps)
            error = g_steal_pointer (&ctx->error_ps);
        else if (ctx->error_cs)
            error = g_steal_pointer (&ctx->error_cs);
        else
            g_assert_not_reached ();

        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* When everything is done, recheck operator info as it may have changed */
    mm_iface_modem_3gpp_reload_current_registration_info (
        MM_IFACE_MODEM_3GPP (self),
        (GAsyncReadyCallback)run_registration_checks_reload_current_registration_info_ready,
        task);
}

static void
modem_3gpp_run_registration_checks (MMIfaceModem3gpp    *self,
                                    gboolean             is_cs_supported,
                                    gboolean             is_ps_supported,
                                    gboolean             is_eps_supported,
                                    gboolean             is_5gs_supported,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
    RunRegistrationChecksContext *ctx;
    GTask *task;

    ctx = g_new0 (RunRegistrationChecksContext, 1);
    ctx->is_cs_supported = is_cs_supported;
    ctx->is_ps_supported = is_ps_supported;
    ctx->is_eps_supported = is_eps_supported;
    ctx->is_5gs_supported = is_5gs_supported;
    ctx->run_cs = is_cs_supported;
    ctx->run_ps = is_ps_supported;
    ctx->run_eps = is_eps_supported;
    ctx->run_5gs = is_5gs_supported;

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, (GDestroyNotify)run_registration_checks_context_free);

    run_registration_checks_context_step (task);
}

/*****************************************************************************/
/* Create initial EPS bearer object */

static MMBaseBearer *
modem_3gpp_create_initial_eps_bearer (MMIfaceModem3gpp   *self,
                                      MMBearerProperties *config)
{
    MMBaseBearer *bearer;

    /* NOTE: by default we create a bearer object that is CONNECTED but which doesn't
     * have an associated data interface already set. This is so that upper layers don't
     * attempt connection through this bearer object. */
    bearer = g_object_new (MM_TYPE_BASE_BEARER,
                           MM_BASE_BEARER_MODEM,  MM_BASE_MODEM (self),
                           MM_BIND_TO,            G_OBJECT (self),
                           MM_BASE_BEARER_CONFIG, config,
                           "bearer-type",         MM_BEARER_TYPE_DEFAULT_ATTACH,
                           "connected",           TRUE,
                           NULL);
    mm_base_bearer_export (bearer);
    return bearer;
}

/*****************************************************************************/
/* Enable/Disable unsolicited registration events (3GPP interface) */

typedef struct {
    MMPortSerialAt *primary;
    MMPortSerialAt *secondary; /* optional */
    gboolean        enable; /* TRUE for enabling, FALSE for disabling */
    gboolean        run_cs;
    gboolean        run_ps;
    gboolean        run_eps;
    gboolean        running_cs;
    gboolean        running_ps;
    gboolean        running_eps;
    GError         *cs_error;
    GError         *ps_error;
    GError         *eps_error;
    gboolean        secondary_sequence;
    gboolean        secondary_done;
} UnsolicitedRegistrationEventsContext;

static void
unsolicited_registration_events_context_free (UnsolicitedRegistrationEventsContext *ctx)
{
    g_clear_object (&ctx->primary);
    g_clear_object (&ctx->secondary);
    g_clear_error (&ctx->cs_error);
    g_clear_error (&ctx->ps_error);
    g_clear_error (&ctx->eps_error);
    g_slice_free (UnsolicitedRegistrationEventsContext, ctx);
}

static GTask *
unsolicited_registration_events_task_new (MMBroadbandModem   *self,
                                          gboolean            enable,
                                          gboolean            cs_supported,
                                          gboolean            ps_supported,
                                          gboolean            eps_supported,
                                          GAsyncReadyCallback callback,
                                          gpointer            user_data)
{
    UnsolicitedRegistrationEventsContext *ctx;
    GTask *task;

    ctx = g_slice_new0 (UnsolicitedRegistrationEventsContext);
    ctx->enable = enable;
    ctx->run_cs = cs_supported;
    ctx->run_ps = ps_supported;
    ctx->run_eps = eps_supported;
    ctx->primary = mm_base_modem_get_port_primary (MM_BASE_MODEM (self));
    ctx->secondary = mm_base_modem_get_port_secondary (MM_BASE_MODEM (self));

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, (GDestroyNotify)unsolicited_registration_events_context_free);
    return task;
}

static gboolean
modem_3gpp_enable_disable_unsolicited_registration_events_finish (MMIfaceModem3gpp  *self,
                                                                  GAsyncResult      *res,
                                                                  GError           **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static MMBaseModemAtResponseProcessorResult
parse_registration_setup_reply (MMBaseModem  *self,
                                gpointer      none,
                                const gchar  *command,
                                const gchar  *response,
                                gboolean      last_command,
                                const GError *error,
                                GVariant    **result,
                                GError      **result_error)
{
    *result_error = NULL;

    /* If error, try next command */
    if (error) {
        *result = NULL;
        return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_CONTINUE;
    }

    /* Set COMMAND as result! */
    *result = g_variant_new_string (command);
    return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_SUCCESS;
}

static const MMBaseModemAtCommand cs_registration_sequence[] = {
    /* Enable unsolicited registration notifications in CS network, with location */
    { "+CREG=2", 3, FALSE, parse_registration_setup_reply },
    /* Enable unsolicited registration notifications in CS network, without location */
    { "+CREG=1", 3, FALSE, parse_registration_setup_reply },
    { NULL }
};

static const MMBaseModemAtCommand cs_unregistration_sequence[] = {
    /* Disable unsolicited registration notifications in CS network */
    { "+CREG=0", 3, FALSE, parse_registration_setup_reply },
    { NULL }
};

static const MMBaseModemAtCommand ps_registration_sequence[] = {
    /* Enable unsolicited registration notifications in PS network, with location */
    { "+CGREG=2", 3, FALSE, parse_registration_setup_reply },
    /* Enable unsolicited registration notifications in PS network, without location */
    { "+CGREG=1", 3, FALSE, parse_registration_setup_reply },
    { NULL }
};

static const MMBaseModemAtCommand ps_unregistration_sequence[] = {
    /* Disable unsolicited registration notifications in PS network */
    { "+CGREG=0", 3, FALSE, parse_registration_setup_reply },
    { NULL }
};

static const MMBaseModemAtCommand eps_registration_sequence[] = {
    /* Enable unsolicited registration notifications in EPS network, with location */
    { "+CEREG=2", 3, FALSE, parse_registration_setup_reply },
    /* Enable unsolicited registration notifications in EPS network, without location */
    { "+CEREG=1", 3, FALSE, parse_registration_setup_reply },
    { NULL }
};

static const MMBaseModemAtCommand eps_unregistration_sequence[] = {
    /* Disable unsolicited registration notifications in PS network */
    { "+CEREG=0", 3, FALSE, parse_registration_setup_reply },
    { NULL }
};

static void unsolicited_registration_events_context_step (GTask *task);

static void
unsolicited_registration_events_sequence_ready (MMBroadbandModem *self,
                                                GAsyncResult     *res,
                                                GTask            *task)
{
    UnsolicitedRegistrationEventsContext *ctx;
    GError                               *error = NULL;
    GVariant                             *command;

    ctx = g_task_get_task_data (task);

    /* Only one must be running */
    g_assert ((ctx->running_cs ? 1 : 0) +
              (ctx->running_ps ? 1 : 0) +
              (ctx->running_eps ? 1 : 0) == 1);

    if (ctx->secondary_done) {
        if (ctx->secondary_sequence)
            mm_base_modem_at_sequence_finish (MM_BASE_MODEM (self), res, NULL, &error);
        else
            mm_base_modem_at_command_full_finish (MM_BASE_MODEM (self), res, &error);

        if (error) {
            mm_obj_dbg (self, "%s unsolicited registration events in secondary port failed: %s",
                        ctx->enable ? "enabling" : "disabling",
                        error->message);
            /* Keep errors reported */
            if (ctx->running_cs && !ctx->cs_error)
                ctx->cs_error = error;
            else if (ctx->running_ps && !ctx->ps_error)
                ctx->ps_error = error;
            else if (ctx->running_eps && !ctx->eps_error)
                ctx->eps_error = error;
            else
                g_error_free (error);
        } else {
            /* If successful in secondary port, cleanup primary error if any */
            if (ctx->running_cs && ctx->cs_error)
                g_clear_error (&ctx->cs_error);
            else if (ctx->running_ps && ctx->ps_error)
                g_clear_error (&ctx->ps_error);
            else if (ctx->running_eps && ctx->eps_error)
                g_clear_error (&ctx->eps_error);
        }

        /* Done with primary and secondary, keep on */
        unsolicited_registration_events_context_step (task);
        return;
    }

    /*  We just run the sequence in the primary port */
    command = mm_base_modem_at_sequence_finish (MM_BASE_MODEM (self), res, NULL, &error);
    if (!command) {
        if (!error)
            error = g_error_new (MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "AT sequence failed");
        mm_obj_dbg (self, "%s unsolicited registration events in primary port failed: %s",
                    ctx->enable ? "enabling" : "disabling",
                    error->message);
        /* Keep errors reported */
        if (ctx->running_cs)
            ctx->cs_error = error;
        else if (ctx->running_ps)
            ctx->ps_error = error;
        else
            ctx->eps_error = error;
        /* Even if primary failed, go on and try to enable in secondary port */
    }

    if (ctx->secondary) {
        const MMBaseModemAtCommand *registration_sequence = NULL;

        ctx->secondary_done = TRUE;

        /* Now use the same registration setup in secondary port, if any */
        if (command) {
            mm_base_modem_at_command_full (
                MM_BASE_MODEM (self),
                MM_IFACE_PORT_AT (ctx->secondary),
                g_variant_get_string (command, NULL),
                3,
                FALSE,
                FALSE, /* raw */
                NULL, /* cancellable */
                (GAsyncReadyCallback)unsolicited_registration_events_sequence_ready,
                task);
            return;
        }

        /* If primary failed, run the whole sequence in secondary */
        ctx->secondary_sequence = TRUE;
        if (ctx->running_cs)
            registration_sequence = ctx->enable ? cs_registration_sequence : cs_unregistration_sequence;
        else if (ctx->running_ps)
            registration_sequence = ctx->enable ? ps_registration_sequence : ps_unregistration_sequence;
        else
            registration_sequence = ctx->enable ? eps_registration_sequence : eps_unregistration_sequence;
        mm_base_modem_at_sequence_full (
            MM_BASE_MODEM (self),
            MM_IFACE_PORT_AT (ctx->secondary),
            registration_sequence,
            NULL,  /* response processor context */
            NULL,  /* response processor context free */
            NULL, /* cancellable */
            (GAsyncReadyCallback)unsolicited_registration_events_sequence_ready,
            task);
        return;
    }

    /* We're done */
    unsolicited_registration_events_context_step (task);
}

static void
unsolicited_registration_events_context_step (GTask *task)
{
    MMBroadbandModem                     *self;
    UnsolicitedRegistrationEventsContext *ctx;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    ctx->running_cs = FALSE;
    ctx->running_ps = FALSE;
    ctx->running_eps = FALSE;
    ctx->secondary_done = FALSE;

    if (ctx->run_cs) {
        ctx->running_cs = TRUE;
        ctx->run_cs = FALSE;
        mm_base_modem_at_sequence_full (
            MM_BASE_MODEM (self),
            MM_IFACE_PORT_AT (ctx->primary),
            ctx->enable ? cs_registration_sequence : cs_unregistration_sequence,
            NULL,  /* response processor context */
            NULL,  /* response processor context free */
            NULL,  /* cancellable */
            (GAsyncReadyCallback)unsolicited_registration_events_sequence_ready,
            task);
        return;
    }

    if (ctx->run_ps) {
        ctx->running_ps = TRUE;
        ctx->run_ps = FALSE;
        mm_base_modem_at_sequence_full (
            MM_BASE_MODEM (self),
            MM_IFACE_PORT_AT (ctx->primary),
            ctx->enable ? ps_registration_sequence : ps_unregistration_sequence,
            NULL,  /* response processor context */
            NULL,  /* response processor context free */
            NULL,  /* cancellable */
            (GAsyncReadyCallback)unsolicited_registration_events_sequence_ready,
            task);
        return;
    }

    if (ctx->run_eps) {
        ctx->running_eps = TRUE;
        ctx->run_eps = FALSE;
        mm_base_modem_at_sequence_full (
            MM_BASE_MODEM (self),
            MM_IFACE_PORT_AT (ctx->primary),
            ctx->enable ? eps_registration_sequence : eps_unregistration_sequence,
            NULL,  /* response processor context */
            NULL,  /* response processor context free */
            NULL,  /* cancellable */
            (GAsyncReadyCallback)unsolicited_registration_events_sequence_ready,
            task);
        return;
    }

    /* All done!
     * If we have any error reported, we'll propagate it. EPS errors take
     * precedence over PS errors and PS errors take precedence over CS errors. */
    if (ctx->eps_error)
        g_task_return_error (task, g_steal_pointer (&ctx->eps_error));
    else if (ctx->ps_error)
        g_task_return_error (task, g_steal_pointer (&ctx->ps_error));
    else if (ctx->cs_error)
        g_task_return_error (task, g_steal_pointer (&ctx->cs_error));
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_3gpp_disable_unsolicited_registration_events (MMIfaceModem3gpp    *self,
                                                    gboolean             cs_supported,
                                                    gboolean             ps_supported,
                                                    gboolean             eps_supported,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data)
{
    unsolicited_registration_events_context_step (
        unsolicited_registration_events_task_new (MM_BROADBAND_MODEM (self),
                                                  FALSE,
                                                  cs_supported,
                                                  ps_supported,
                                                  eps_supported,
                                                  callback,
                                                  user_data));
}

static void
modem_3gpp_enable_unsolicited_registration_events (MMIfaceModem3gpp    *self,
                                                   gboolean             cs_supported,
                                                   gboolean             ps_supported,
                                                   gboolean             eps_supported,
                                                   GAsyncReadyCallback  callback,
                                                   gpointer             user_data)
{
    unsolicited_registration_events_context_step (
        unsolicited_registration_events_task_new (MM_BROADBAND_MODEM (self),
                                                  TRUE,
                                                  cs_supported,
                                                  ps_supported,
                                                  eps_supported,
                                                  callback,
                                                  user_data));
}

/*****************************************************************************/
/* Cancel USSD (3GPP/USSD interface) */

static gboolean
modem_3gpp_ussd_cancel_finish (MMIfaceModem3gppUssd  *self,
                               GAsyncResult          *res,
                               GError               **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cancel_command_ready (MMBroadbandModem *self,
                      GAsyncResult *res,
                      GTask *task)
{
    GError *error = NULL;

    mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);

    /* Complete the pending action, regardless of the CUSD result */
    if (self->priv->pending_ussd_action) {
        GTask *pending_task;

        pending_task = self->priv->pending_ussd_action;
        self->priv->pending_ussd_action = NULL;

        g_task_return_new_error (pending_task, MM_CORE_ERROR, MM_CORE_ERROR_ABORTED,
                                 "USSD session was cancelled");
        g_object_unref (pending_task);
    }

    mm_iface_modem_3gpp_ussd_update_state (MM_IFACE_MODEM_3GPP_USSD (self),
                                           MM_MODEM_3GPP_USSD_SESSION_STATE_IDLE);

    if (error)
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_3gpp_ussd_cancel (MMIfaceModem3gppUssd *self,
                        GAsyncReadyCallback   callback,
                        gpointer              user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CUSD=2",
                              10,
                              TRUE,
                              (GAsyncReadyCallback)cancel_command_ready,
                              task);
}

/*****************************************************************************/
/* Send command (3GPP/USSD interface) */

typedef struct {
    gchar    *command;
    gboolean  current_is_unencoded;
    gboolean  encoded_used;
    gboolean  unencoded_used;
} Modem3gppUssdSendContext;

static void
modem_3gpp_ussd_send_context_free (Modem3gppUssdSendContext *ctx)
{
    g_free (ctx->command);
    g_slice_free (Modem3gppUssdSendContext, ctx);
}

static gchar *
modem_3gpp_ussd_send_finish (MMIfaceModem3gppUssd  *self,
                             GAsyncResult          *res,
                             GError               **error)
{
    return g_task_propagate_pointer (G_TASK (res), error);
}

static void modem_3gpp_ussd_context_step (MMBroadbandModem *self);

static void cusd_process_string (MMBroadbandModem *self,
                                 const gchar *str);

static void
ussd_send_command_ready (MMBaseModem  *_self,
                         GAsyncResult *res)
{
    MMBroadbandModem         *self;
    Modem3gppUssdSendContext *ctx;
    GError                   *error = NULL;
    const gchar              *response;

    self = MM_BROADBAND_MODEM (_self);

    response = mm_base_modem_at_command_finish (_self, res, &error);
    if (error) {
        /* Some immediate error happened when sending the USSD request */
        mm_obj_dbg (self, "error sending USSD request: '%s'", error->message);
        g_error_free (error);

        if (self->priv->pending_ussd_action) {
            modem_3gpp_ussd_context_step (self);
            return;
        }
    }

    if (!self->priv->pending_ussd_action) {
        mm_obj_dbg (self, "USSD operation finished already via URCs");
        return;
    }

    /* Cache the hint for the next time we send something */
    ctx = g_task_get_task_data (self->priv->pending_ussd_action);
    if (!self->priv->use_unencoded_ussd && ctx->current_is_unencoded) {
        mm_obj_dbg (self, "will assume we want unencoded USSD commands");
        self->priv->use_unencoded_ussd = TRUE;
    } else if (self->priv->use_unencoded_ussd && !ctx->current_is_unencoded) {
        mm_obj_dbg (self, "will assume we want encoded USSD commands");
        self->priv->use_unencoded_ussd = FALSE;
    }

    if (response && response[0]) {
        response = mm_strip_tag (response, "+CUSD:");
        cusd_process_string (self, response);
    }
}

static void
modem_3gpp_ussd_context_send_encoded (MMBroadbandModem *self)
{
    Modem3gppUssdSendContext *ctx;
    gchar                    *at_command = NULL;
    GError                   *error = NULL;
    guint                     scheme = 0;
    gchar                    *encoded;

    g_assert (self->priv->pending_ussd_action);
    ctx = g_task_get_task_data (self->priv->pending_ussd_action);

    /* Encode USSD command */
    encoded = mm_iface_modem_3gpp_ussd_encode (MM_IFACE_MODEM_3GPP_USSD (self), ctx->command, &scheme, &error);
    if (!encoded) {
        GTask *task;

        task = self->priv->pending_ussd_action;
        self->priv->pending_ussd_action = NULL;

        mm_iface_modem_3gpp_ussd_update_state (MM_IFACE_MODEM_3GPP_USSD (self), MM_MODEM_3GPP_USSD_SESSION_STATE_IDLE);

        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Build AT command */
    ctx->encoded_used = TRUE;
    ctx->current_is_unencoded = FALSE;
    at_command = g_strdup_printf ("+CUSD=1,\"%s\",%d", encoded, scheme);
    g_free (encoded);

    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              at_command,
                              10,
                              FALSE,
                              (GAsyncReadyCallback)ussd_send_command_ready,
                              NULL);
    g_free (at_command);
}

static void
modem_3gpp_ussd_context_send_unencoded (MMBroadbandModem *self)
{
    Modem3gppUssdSendContext *ctx;
    gchar                    *at_command = NULL;

    g_assert (self->priv->pending_ussd_action);
    ctx = g_task_get_task_data (self->priv->pending_ussd_action);

    /* Build AT command with action unencoded */
    ctx->unencoded_used = TRUE;
    ctx->current_is_unencoded = TRUE;
    at_command = g_strdup_printf ("+CUSD=1,\"%s\",%d",
                                  ctx->command,
                                  MM_MODEM_GSM_USSD_SCHEME_7BIT);

    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              at_command,
                              10,
                              FALSE,
                              (GAsyncReadyCallback)ussd_send_command_ready,
                              NULL);
    g_free (at_command);
}

static void
modem_3gpp_ussd_context_step (MMBroadbandModem *self)
{
    Modem3gppUssdSendContext *ctx;

    g_assert (self->priv->pending_ussd_action);
    ctx = g_task_get_task_data (self->priv->pending_ussd_action);

    if (ctx->encoded_used && ctx->unencoded_used) {
        GTask *task;

        task = self->priv->pending_ussd_action;
        self->priv->pending_ussd_action = NULL;

        mm_iface_modem_3gpp_ussd_update_state (MM_IFACE_MODEM_3GPP_USSD (self),
                                               MM_MODEM_3GPP_USSD_SESSION_STATE_IDLE);

        g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                                 "Sending USSD command failed");
        g_object_unref (task);
        return;
    }

    if (self->priv->use_unencoded_ussd) {
        if (!ctx->unencoded_used)
            modem_3gpp_ussd_context_send_unencoded (self);
        else if (!ctx->encoded_used)
            modem_3gpp_ussd_context_send_encoded (self);
        else
            g_assert_not_reached ();
    } else {
        if (!ctx->encoded_used)
            modem_3gpp_ussd_context_send_encoded (self);
        else if (!ctx->unencoded_used)
            modem_3gpp_ussd_context_send_unencoded (self);
        else
            g_assert_not_reached ();
    }
}

static void
modem_3gpp_ussd_send (MMIfaceModem3gppUssd *_self,
                      const gchar          *command,
                      GAsyncReadyCallback   callback,
                      gpointer              user_data)
{
    MMBroadbandModem         *self;
    GTask                    *task;
    Modem3gppUssdSendContext *ctx;

    self = MM_BROADBAND_MODEM (_self);

    task = g_task_new (self, NULL, callback, user_data);
    ctx = g_slice_new0 (Modem3gppUssdSendContext);
    ctx->command = g_strdup (command);
    g_task_set_task_data (task, ctx, (GDestroyNotify) modem_3gpp_ussd_send_context_free);

    /* Fail if there is an ongoing operation already */
    if (self->priv->pending_ussd_action) {
        g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_IN_PROGRESS,
                                 "there is already an ongoing USSD operation");
        g_object_unref (task);
        return;
    }

    /* Cache the action, as it may be completed via URCs */
    self->priv->pending_ussd_action = task;

    mm_iface_modem_3gpp_ussd_update_state (_self, MM_MODEM_3GPP_USSD_SESSION_STATE_ACTIVE);

    modem_3gpp_ussd_context_step (self);
}

/*****************************************************************************/
/* USSD Encode/Decode (3GPP/USSD interface) */

static gchar *
modem_3gpp_ussd_encode (MMIfaceModem3gppUssd  *_self,
                        const gchar           *command,
                        guint                 *scheme,
                        GError               **error)
{
    MMBroadbandModem      *self = MM_BROADBAND_MODEM (_self);
    g_autoptr(GByteArray)  ussd_command = NULL;

    /* Encode to the current charset (as per AT+CSCS, which is what most modems
     * (except for Huawei it seems) will ask for. */
    ussd_command = mm_modem_charset_bytearray_from_utf8 (command, self->priv->modem_current_charset, FALSE, error);
    if (!ussd_command) {
        g_prefix_error (error, "Failed to encode USSD command: ");
        return NULL;
    }

    /* The scheme value does NOT represent the encoding used to encode the string
     * we're giving. This scheme reflects the encoding that the modem should use when
     * sending the data out to the network. We're hardcoding this to GSM-7 because
     * USSD commands fit well in GSM-7, unlike USSD responses that may contain code
     * points that may only be encoded in UCS-2. */
    *scheme = MM_MODEM_GSM_USSD_SCHEME_7BIT;

    /* convert to hex representation */
    return (gchar *) mm_utils_bin2hexstr (ussd_command->data, ussd_command->len);
}

static gchar *
modem_3gpp_ussd_decode (MMIfaceModem3gppUssd  *self,
                        const gchar           *reply,
                        GError               **error)
{
    MMBroadbandModem      *broadband = MM_BROADBAND_MODEM (self);
    guint8                *bin = NULL;
    gsize                  bin_len = 0;
    g_autoptr(GByteArray)  barray = NULL;

    bin = (guint8 *) mm_utils_hexstr2bin (reply, -1, &bin_len, error);
    if (!bin) {
        g_prefix_error (error, "Couldn't convert HEX string to binary: ");
        return NULL;
    }
    barray = g_byte_array_new_take (bin, bin_len);

    /* Decode from current charset (as per AT+CSCS, which is what most modems
     * (except for Huawei it seems) will ask for. */
    return mm_modem_charset_bytearray_to_utf8 (barray, broadband->priv->modem_current_charset, FALSE, error);
}

/*****************************************************************************/
/* Setup/Cleanup unsolicited result codes (3GPP/USSD interface) */

static gboolean
modem_3gpp_ussd_setup_cleanup_unsolicited_events_finish (MMIfaceModem3gppUssd  *self,
                                                         GAsyncResult          *res,
                                                         GError               **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static gchar *
decode_ussd_response (MMBroadbandModem *self,
                      const gchar *reply,
                      GError **error)
{
    gchar *p;
    gchar *str;
    gchar *decoded;

    /* Look for the first ',' */
    p = strchr (reply, ',');
    if (!p) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "Cannot decode USSD response (%s): missing field separator",
                     reply);
        return NULL;
    }

    /* Assume the string is the next field, and strip quotes. While doing this,
     * we also skip any other additional field we may have afterwards */
    if (p[1] == '"') {
        str = g_strdup (&p[2]);
        p = strchr (str, '"');
        if (p)
            *p = '\0';
    } else {
        str = g_strdup (&p[1]);
        p = strchr (str, ',');
        if (p)
            *p = '\0';
    }

    /* If reply doesn't seem to be hex; just return itself... */
    if (!mm_utils_ishexstr (str))
        decoded = g_strdup (str);
    else
        decoded = mm_iface_modem_3gpp_ussd_decode (MM_IFACE_MODEM_3GPP_USSD (self), str, error);

    g_free (str);

    return decoded;
}

static void
cusd_process_string (MMBroadbandModem *self,
                     const gchar      *str)
{
    MMModem3gppUssdSessionState  ussd_state = MM_MODEM_3GPP_USSD_SESSION_STATE_IDLE;
    GError                      *error = NULL;
    gint                         status;
    GTask                       *task;
    gchar                       *converted = NULL;

    /* If there is a pending action, it is ALWAYS completed here */
    task = self->priv->pending_ussd_action;
    self->priv->pending_ussd_action = NULL;

    if (!str || !isdigit (*str)) {
        error = g_error_new (MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                             "Invalid USSD message received: '%s'", str ? str : "(none)");
        goto out;
    }

    status = g_ascii_digit_value (*str);
    switch (status) {
    case 0:
        /* no further action required */
        converted = decode_ussd_response (self, str, &error);
        if (!converted)
            break;

        /* Response to the user's request? */
        if (task)
            break;

        /* Network-initiated USSD-Notify */
        mm_iface_modem_3gpp_ussd_update_network_notification (MM_IFACE_MODEM_3GPP_USSD (self), converted);
        g_clear_pointer (&converted, g_free);
        break;

    case 1:
        /* further action required */
        ussd_state = MM_MODEM_3GPP_USSD_SESSION_STATE_USER_RESPONSE;

        converted = decode_ussd_response (self, str, &error);
        if (!converted)
            break;

        /* Response to the user's request? */
        if (task)
            break;

        /* Network-initiated USSD-Request */
        mm_iface_modem_3gpp_ussd_update_network_request (MM_IFACE_MODEM_3GPP_USSD (self), converted);
        g_clear_pointer (&converted, g_free);
        break;

    case 2:
        /* Response to the user's request? */
        if (task)
            error = g_error_new (MM_CORE_ERROR, MM_CORE_ERROR_ABORTED, "USSD terminated by network");
        break;

    case 4:
        /* Response to the user's request? */
        if (task)
            error = g_error_new (MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_NOT_SUPPORTED, "Operation not supported");
        break;

    default:
        /* Response to the user's request? */
        if (task)
            error = g_error_new (MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                                 "Unhandled USSD reply: %s (%d)", str, status);
        break;
    }

out:

    mm_iface_modem_3gpp_ussd_update_state (MM_IFACE_MODEM_3GPP_USSD (self), ussd_state);

    /* Complete the pending action */
    if (task) {
        if (error)
            g_task_return_error (task, error);
        else if (converted)
            g_task_return_pointer (task, g_steal_pointer (&converted), g_free);
        else
            g_assert_not_reached ();
        g_clear_pointer (&converted, g_free);
        return;
    }

    /* If no pending task, just report the error */
    if (error) {
        mm_obj_warn (self, "invalid USSD message: %s", error->message);
        g_error_free (error);
    }

    g_assert (!converted);
}

static void
cusd_received (MMPortSerialAt *port,
               GMatchInfo *info,
               MMBroadbandModem *self)
{
    gchar *str;

    mm_obj_dbg (self, "unsolicited USSD URC received");
    str = g_match_info_fetch (info, 1);
    cusd_process_string (self, str);
    g_free (str);
}

static void
set_unsolicited_result_code_handlers (MMIfaceModem3gppUssd *self,
                                      gboolean enable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
    MMPortSerialAt    *ports[2];
    g_autoptr(GRegex)  cusd_regex = NULL;
    guint              i;
    GTask             *task;

    cusd_regex = mm_3gpp_cusd_regex_get ();
    ports[0] = mm_base_modem_peek_port_primary (MM_BASE_MODEM (self));
    ports[1] = mm_base_modem_peek_port_secondary (MM_BASE_MODEM (self));

    /* Enable unsolicited result codes in given port */
    for (i = 0; i < 2; i++) {
        if (!ports[i])
            continue;
        /* Set/unset unsolicited CUSD event handler */
        mm_obj_dbg (self, "%s unsolicited result code handlers in %s",
                    enable ? "setting" : "removing",
                    mm_port_get_device (MM_PORT (ports[i])));
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            cusd_regex,
            enable ? (MMPortSerialAtUnsolicitedMsgFn) cusd_received : NULL,
            enable ? self : NULL,
            NULL);
    }

    task = g_task_new (self, NULL, callback, user_data);
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_3gpp_ussd_setup_unsolicited_events (MMIfaceModem3gppUssd *self,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data)
{
    set_unsolicited_result_code_handlers (self, TRUE, callback, user_data);
}

static void
modem_3gpp_ussd_cleanup_unsolicited_events (MMIfaceModem3gppUssd *self,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data)
{
    set_unsolicited_result_code_handlers (self, FALSE, callback, user_data);
}

/*****************************************************************************/
/* Enable/Disable URCs (3GPP/USSD interface) */

static gboolean
modem_3gpp_ussd_enable_disable_unsolicited_events_finish (MMIfaceModem3gppUssd  *self,
                                                          GAsyncResult          *res,
                                                          GError               **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
urc_enable_disable_ready (MMBroadbandModem *self,
                          GAsyncResult *res,
                          GTask *task)
{
    GError *error = NULL;

    mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_3gpp_ussd_disable_unsolicited_events (MMIfaceModem3gppUssd *self,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CUSD=0",
                              3,
                              TRUE,
                              (GAsyncReadyCallback)urc_enable_disable_ready,
                              task);
}

static void
modem_3gpp_ussd_enable_unsolicited_events (MMIfaceModem3gppUssd *self,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CUSD=1",
                              3,
                              TRUE,
                              (GAsyncReadyCallback)urc_enable_disable_ready,
                              task);
}

/*****************************************************************************/
/* Check if USSD supported (3GPP/USSD interface) */

static gboolean
modem_3gpp_ussd_check_support_finish (MMIfaceModem3gppUssd *self,
                                      GAsyncResult *res,
                                      GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cusd_format_check_ready (MMBroadbandModem *self,
                         GAsyncResult *res,
                         GTask *task)
{
    GError *error = NULL;

    mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_3gpp_ussd_check_support (MMIfaceModem3gppUssd *self,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* Check USSD support */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CUSD=?",
                              3,
                              TRUE,
                              (GAsyncReadyCallback)cusd_format_check_ready,
                              task);
}

/*****************************************************************************/
/* Check if Messaging supported (Messaging interface) */

static gboolean
modem_messaging_check_support_finish (MMIfaceModemMessaging *self,
                                      GAsyncResult *res,
                                      GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cnmi_format_check_ready (MMBroadbandModem *self,
                         GAsyncResult *res,
                         GTask *task)
{
    GError *error = NULL;

    mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* CNMI command is supported; assume we have full messaging capabilities */
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_messaging_check_support (MMIfaceModemMessaging *self,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* We assume that CDMA-only modems don't have messaging capabilities */
    if (mm_iface_modem_is_cdma_only (MM_IFACE_MODEM (self))) {
        g_task_return_new_error (
            task,
            MM_CORE_ERROR,
            MM_CORE_ERROR_UNSUPPORTED,
            "CDMA-only modems don't have messaging capabilities");
        g_object_unref (task);
        return;
    }

    /* Check CNMI support */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CNMI=?",
                              3,
                              TRUE,
                              (GAsyncReadyCallback)cnmi_format_check_ready,
                              task);
}

/*****************************************************************************/
/* Load supported SMS storages (Messaging interface) */

typedef struct {
    GArray *mem1;
    GArray *mem2;
    GArray *mem3;
} SupportedStoragesResult;

static void
supported_storages_result_free (SupportedStoragesResult *result)
{
    if (result->mem1)
        g_array_unref (result->mem1);
    if (result->mem2)
        g_array_unref (result->mem2);
    if (result->mem3)
        g_array_unref (result->mem3);
    g_free (result);
}

static gboolean
modem_messaging_load_supported_storages_finish (MMIfaceModemMessaging *self,
                                                GAsyncResult *res,
                                                GArray **mem1,
                                                GArray **mem2,
                                                GArray **mem3,
                                                GError **error)
{
    SupportedStoragesResult *result;

    result = g_task_propagate_pointer (G_TASK (res), error);
    if (!result)
        return FALSE;

    *mem1 = g_array_ref (result->mem1);
    *mem2 = g_array_ref (result->mem2);
    *mem3 = g_array_ref (result->mem3);
    supported_storages_result_free (result);
    return TRUE;
}

static void
cpms_format_check_ready (MMBroadbandModem *self,
                         GAsyncResult *res,
                         GTask *task)
{
    const gchar *response;
    GError *error = NULL;
    SupportedStoragesResult *result;

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    result = g_new0 (SupportedStoragesResult, 1);

    /* Parse reply */
    if (!mm_3gpp_parse_cpms_test_response (response,
                                           &result->mem1,
                                           &result->mem2,
                                           &result->mem3,
                                           &error)) {
        supported_storages_result_free (result);
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    g_task_return_pointer (task,
                           result,
                           (GDestroyNotify)supported_storages_result_free);
    g_object_unref (task);
}

static void
modem_messaging_load_supported_storages (MMIfaceModemMessaging *self,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* Check support storages */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CPMS=?",
                              3,
                              TRUE,
                              (GAsyncReadyCallback)cpms_format_check_ready,
                              task);
}

/*****************************************************************************/
/* Init current SMS storages (Messaging interface) */

static gboolean
modem_messaging_init_current_storages_finish (MMIfaceModemMessaging *_self,
                                              GAsyncResult *res,
                                              MMSmsStorage *current_storage,
                                              GError **error)
{
    gssize result;

    result = g_task_propagate_int (G_TASK (res), error);
    if (result < 0)
        return FALSE;

    if (current_storage)
        *current_storage = (MMSmsStorage)result;
    return TRUE;
}

static void
cpms_query_ready (MMBroadbandModem *self,
                  GAsyncResult *res,
                  GTask *task)
{
    const gchar *response;
    GError *error = NULL;
    MMSmsStorage mem1;
    MMSmsStorage mem2;

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Parse reply */
    if (!mm_3gpp_parse_cpms_query_response (response,
                                            &mem1,
                                            &mem2,
                                            &error)) {
        g_task_return_error (task, error);
    } else {
        gchar *aux;

        self->priv->current_sms_mem1_storage = mem1;
        self->priv->current_sms_mem2_storage = mem2;

        mm_obj_dbg (self, "current storages initialized:");

        aux = mm_common_build_sms_storages_string (&mem1, 1);
        mm_obj_dbg (self, "  mem1 (list/read/delete) storages: '%s'", aux);
        g_free (aux);

        aux = mm_common_build_sms_storages_string (&mem2, 1);
        mm_obj_dbg (self, "  mem2 (write/send) storages:       '%s'", aux);
        g_free (aux);

        g_task_return_int (task, mem2);
    }
    g_object_unref (task);
}

static void
modem_messaging_init_current_storages (MMIfaceModemMessaging *self,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* Check support storages */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CPMS?",
                              3,
                              TRUE,
                              (GAsyncReadyCallback)cpms_query_ready,
                              task);
}

/*****************************************************************************/
/* Lock/unlock SMS storage (Messaging interface implementation helper)
 *
 * The basic commands to work with SMS storages play with AT+CPMS and three
 * different storages: mem1, mem2 and mem3.
 *   'mem1' is the storage for reading, listing and deleting.
 *   'mem2' is the storage for writing and sending from storage.
 *   'mem3' is the storage for receiving.
 *
 * When a command is to be issued for a specific storage, we need a way to
 * lock the access so that other actions are forbidden until the current one
 * finishes. Just think of two sequential actions to store two different
 * SMS into 2 different storages. If the second action is run while the first
 * one is still running, we should issue a RETRY error.
 *
 * Note that mem3 cannot be locked; we just set the default mem3 and that's it.
 *
 * When we unlock the storage, we don't go back to the default storage
 * automatically, we just keep track of which is the current one and only go to
 * the default one if needed.
 */

static void
modem_messaging_unlock_storages (MMIfaceModemMessaging *_self,
                                 gboolean               mem1,
                                 gboolean               mem2)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);

    if (mem1) {
        g_assert (self->priv->mem1_storage_locked);
        self->priv->mem1_storage_locked = FALSE;
    }

    if (mem2) {
        g_assert (self->priv->mem2_storage_locked);
        self->priv->mem2_storage_locked = FALSE;
    }
}

static gboolean
modem_messaging_lock_storages_finish (MMIfaceModemMessaging  *self,
                                      GAsyncResult           *res,
                                      GError                **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

typedef struct {
    MMSmsStorage previous_mem1;
    gboolean mem1_locked;
    MMSmsStorage previous_mem2;
    gboolean mem2_locked;
} LockSmsStoragesContext;

static void
lock_storages_cpms_set_ready (MMBaseModem *_self,
                              GAsyncResult *res,
                              GTask *task)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    LockSmsStoragesContext *ctx;
    GError *error = NULL;

    ctx = g_task_get_task_data (task);

    mm_base_modem_at_command_finish (_self, res, &error);
    if (error) {
        /* Reset previous storages and set unlocked */
        if (ctx->mem1_locked) {
            self->priv->current_sms_mem1_storage = ctx->previous_mem1;
            self->priv->mem1_storage_locked = FALSE;
        }
        if (ctx->mem2_locked) {
            self->priv->current_sms_mem2_storage = ctx->previous_mem2;
            self->priv->mem2_storage_locked = FALSE;
        }
        g_task_return_error (task, error);
    }
    else
        g_task_return_boolean (task, TRUE);

    g_object_unref (task);
}

static void
modem_messaging_lock_storages (MMIfaceModemMessaging *_self,
                               MMSmsStorage           mem1, /* reading/listing/deleting */
                               MMSmsStorage           mem2, /* storing/sending */
                               GAsyncReadyCallback    callback,
                               gpointer               user_data)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    LockSmsStoragesContext *ctx;
    GTask *task;
    gchar *cmd = NULL;
    gchar *mem1_str = NULL;
    gchar *mem2_str = NULL;

    /* If storages are currently locked by someone else, just return an
     * error */
    if ((mem1 != MM_SMS_STORAGE_UNKNOWN && self->priv->mem1_storage_locked) ||
        (mem2 != MM_SMS_STORAGE_UNKNOWN && self->priv->mem2_storage_locked)) {
        g_task_report_new_error (
            self,
            callback,
            user_data,
            modem_messaging_lock_storages,
            MM_CORE_ERROR,
            MM_CORE_ERROR_RETRY,
            "SMS storage currently locked, try again later");
        return;
    }

    /* We allow locking either just one or both */
    g_assert (mem1 != MM_SMS_STORAGE_UNKNOWN ||
              mem2 != MM_SMS_STORAGE_UNKNOWN);

    ctx = g_new0 (LockSmsStoragesContext, 1);

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, g_free);

    /* Some modems may not support empty string parameters, then if mem1 is
     * UNKNOWN, and current sms mem1 storage has a valid value, we send again
     * the already locked mem1 value in place of an empty string.
     * This way we also avoid to confuse the environment of other sync operation
     * that have potentially locked mem1 previously.
     */
    if (mem1 != MM_SMS_STORAGE_UNKNOWN) {
        ctx->mem1_locked = TRUE;
        ctx->previous_mem1 = self->priv->current_sms_mem1_storage;

        self->priv->current_sms_mem1_storage = mem1;
        self->priv->mem1_storage_locked = TRUE;
    } else if (self->priv->current_sms_mem1_storage != MM_SMS_STORAGE_UNKNOWN) {
        mm_obj_dbg (self, "given sms mem1 storage is unknown. Using current sms mem1 storage value '%s' instead",
                    mm_sms_storage_get_string (self->priv->current_sms_mem1_storage));
    } else {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_RETRY,
                                 "Cannot lock mem2 storage alone when current mem1 storage is unknown");
        g_object_unref (task);
        return;
    }
    mem1_str = g_ascii_strup (mm_sms_storage_get_string (self->priv->current_sms_mem1_storage), -1);

    if (mem2 != MM_SMS_STORAGE_UNKNOWN) {
        ctx->mem2_locked = TRUE;
        ctx->previous_mem2 = self->priv->current_sms_mem2_storage;

        self->priv->mem2_storage_locked = TRUE;
        self->priv->current_sms_mem2_storage = mem2;

        mem2_str = g_ascii_strup (mm_sms_storage_get_string (self->priv->current_sms_mem2_storage), -1);
    }

    g_assert (mem1_str != NULL);

    /* We don't touch 'mem3' here */
    mm_obj_dbg (self, "locking SMS storages to: mem1 (%s), mem2 (%s)...",
                mem1_str, mem2_str ? mem2_str : "none");

    if (mem2_str)
        cmd = g_strdup_printf ("+CPMS=\"%s\",\"%s\"", mem1_str, mem2_str);
    else
        cmd = g_strdup_printf ("+CPMS=\"%s\"", mem1_str);

    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              cmd,
                              3,
                              FALSE,
                              (GAsyncReadyCallback)lock_storages_cpms_set_ready,
                              task);
    g_free (mem1_str);
    g_free (mem2_str);
    g_free (cmd);
}

/*****************************************************************************/
/* Set default SMS storage (Messaging interface) */

static gboolean
modem_messaging_set_default_storage_finish (MMIfaceModemMessaging *self,
                                            GAsyncResult *res,
                                            GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cpms_set_ready (MMBroadbandModem *self,
                GAsyncResult *res,
                GTask *task)
{
    GError *error = NULL;

    mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error)
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_messaging_set_default_storage (MMIfaceModemMessaging *_self,
                                     MMSmsStorage storage,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    gchar *cmd;
    gchar *mem1_str;
    gchar *mem_str;
    GTask *task;

    /* We provide the current sms storage for mem1 if not UNKNOWN */
    if (self->priv->current_sms_mem1_storage == MM_SMS_STORAGE_UNKNOWN) {
        g_task_report_new_error (self,
                                 callback,
                                 user_data,
                                 modem_messaging_set_default_storage,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_INVALID_ARGS,
                                 "Cannot set default storage when current mem1 storage is unknown");
        return;
    }

    /* Set defaults as current */
    self->priv->current_sms_mem2_storage = storage;

    mem1_str = g_ascii_strup (mm_sms_storage_get_string (self->priv->current_sms_mem1_storage), -1);
    mem_str = g_ascii_strup (mm_sms_storage_get_string (storage), -1);

    cmd = g_strdup_printf ("+CPMS=\"%s\",\"%s\",\"%s\"", mem1_str, mem_str, mem_str);

    task = g_task_new (self, NULL, callback, user_data);

    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              cmd,
                              3,
                              FALSE,
                              (GAsyncReadyCallback)cpms_set_ready,
                              task);
    g_free (mem1_str);
    g_free (mem_str);
    g_free (cmd);
}

/*****************************************************************************/
/* Setup SMS format (Messaging interface) */

static gboolean
modem_messaging_setup_sms_format_finish (MMIfaceModemMessaging *self,
                                         GAsyncResult *res,
                                         GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cmgf_set_ready (MMBroadbandModem *self,
                GAsyncResult *res,
                GTask *task)
{
    GError *error = NULL;

    mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        mm_obj_dbg (self, "failed to set preferred SMS mode: %s; assuming text mode'", error->message);
        g_error_free (error);
        self->priv->modem_messaging_sms_pdu_mode = FALSE;
    } else
        mm_obj_dbg (self, "successfully set preferred SMS mode: '%s'",
                    self->priv->modem_messaging_sms_pdu_mode ? "PDU" : "text");

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
set_preferred_sms_format (MMBroadbandModem *self,
                          GTask *task)
{
    gchar *cmd;

    cmd = g_strdup_printf ("+CMGF=%s",
                           self->priv->modem_messaging_sms_pdu_mode ? "0" : "1");
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              cmd,
                              3,
                              TRUE,
                              (GAsyncReadyCallback)cmgf_set_ready,
                              task);
    g_free (cmd);
}

static void
cmgf_format_check_ready (MMBroadbandModem *self,
                         GAsyncResult *res,
                         GTask *task)
{
    GError *error = NULL;
    const gchar *response;
    gboolean sms_pdu_supported = FALSE;
    gboolean sms_text_supported = FALSE;

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error ||
        !mm_3gpp_parse_cmgf_test_response (response,
                                           &sms_pdu_supported,
                                           &sms_text_supported,
                                           &error)) {
        mm_obj_dbg (self, "failed to query supported SMS modes: %s", error->message);
        g_error_free (error);
    }

    /* Only use text mode if PDU mode not supported */
    self->priv->modem_messaging_sms_pdu_mode = TRUE;
    if (!sms_pdu_supported) {
        if (sms_text_supported) {
            mm_obj_dbg (self, "PDU mode not supported, will try to use Text mode");
            self->priv->modem_messaging_sms_pdu_mode = FALSE;
        } else
            mm_obj_dbg (self, "neither PDU nor Text modes are reported as supported; "
                        "will anyway default to PDU mode");
    }

    self->priv->sms_supported_modes_checked = TRUE;

    set_preferred_sms_format (self, task);
}

static void
modem_messaging_setup_sms_format (MMIfaceModemMessaging *self,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* If we already checked for supported SMS types, go on to select the
     * preferred format. */
    if (MM_BROADBAND_MODEM (self)->priv->sms_supported_modes_checked) {
        set_preferred_sms_format (MM_BROADBAND_MODEM (self), task);
        return;
    }

    /* Check supported SMS formats */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CMGF=?",
                              3,
                              TRUE,
                              (GAsyncReadyCallback)cmgf_format_check_ready,
                              task);
}

/*****************************************************************************/
/* Setup/cleanup messaging related unsolicited events (Messaging interface) */

static gboolean
modem_messaging_setup_cleanup_unsolicited_events_finish (MMIfaceModemMessaging *self,
                                                         GAsyncResult *res,
                                                         GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

typedef struct {
    guint idx;
} SmsPartContext;

static void
sms_part_ready (MMBroadbandModem *self,
                GAsyncResult *res,
                GTask *task)
{
    SmsPartContext *ctx;
    MMSmsPart      *part;
    MM3gppPduInfo  *info;
    const gchar    *response;
    GError         *error = NULL;

    /* Always always always unlock mem1 storage. Warned you've been. */
    mm_iface_modem_messaging_unlock_storages (MM_IFACE_MODEM_MESSAGING (self), TRUE, FALSE);

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        /* We're really ignoring this error afterwards, as we don't have a callback
         * passed to the async operation, so just log the error here. */
        mm_obj_warn (self, "couldn't retrieve SMS part: '%s'", error->message);
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    ctx = g_task_get_task_data (task);

    info = mm_3gpp_parse_cmgr_read_response (response, ctx->idx, &error);
    if (!info) {
        mm_obj_warn (self, "couldn't parse SMS part: '%s'", error->message);
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    part = mm_sms_part_3gpp_new_from_pdu (info->index, info->pdu, self, &error);
    if (!part) {
        /* Don't treat the error as critical */
        mm_obj_dbg (self, "error parsing PDU (%d): %s", ctx->idx, error->message);
        g_error_free (error);
    } else {
        mm_obj_dbg (self, "correctly parsed PDU (%d)", ctx->idx);
        if (!mm_iface_modem_messaging_take_part (MM_IFACE_MODEM_MESSAGING (self),
                                                 mm_broadband_modem_create_sms (MM_BROADBAND_MODEM (self)),
                                                 part,
                                                 MM_SMS_STATE_RECEIVED,
                                                 self->priv->modem_messaging_sms_default_storage,
                                                 &error)) {
            /* Don't treat the error as critical */
            mm_obj_dbg (self, "error adding SMS (%d): %s", ctx->idx, error->message);
            g_error_free (error);
        }
    }

    /* All done */
    mm_3gpp_pdu_info_free (info);
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
indication_lock_storages_ready (MMIfaceModemMessaging *messaging,
                                GAsyncResult          *res,
                                GTask                 *task)
{
    SmsPartContext *ctx;
    gchar *command;
    GError *error = NULL;

    if (!mm_iface_modem_messaging_lock_storages_finish (messaging, res, &error)) {
        /* TODO: we should either make this lock() never fail, by automatically
         * retrying after some time, or otherwise retry here. */
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Storage now set and locked */

    /* Retrieve the message */
    ctx = g_task_get_task_data (task);
    command = g_strdup_printf ("+CMGR=%d", ctx->idx);
    mm_base_modem_at_command (MM_BASE_MODEM (messaging),
                              command,
                              10,
                              FALSE,
                              (GAsyncReadyCallback)sms_part_ready,
                              task);
    g_free (command);
}

static void
cmti_received (MMPortSerialAt *port,
               GMatchInfo *info,
               MMBroadbandModem *self)
{
    SmsPartContext *ctx;
    GTask *task;
    guint idx = 0;
    MMSmsStorage storage;
    gchar *str;

    if (!mm_get_uint_from_match_info (info, 2, &idx))
        return;

    /* The match info gives us in which storage the index applies */
    str = mm_get_string_unquoted_from_match_info (info, 1);
    storage = mm_common_get_sms_storage_from_string (str, NULL);
    if (storage == MM_SMS_STORAGE_UNKNOWN) {
        mm_obj_dbg (self, "skipping CMTI indication, unknown storage '%s' reported", str);
        g_free (str);
        return;
    }
    g_free (str);

    /* Don't signal multiple times if there are multiple CMTI notifications for a message */
    if (mm_sms_list_has_part (self->priv->modem_messaging_sms_list,
                              storage,
                              idx)) {
        mm_obj_dbg (self, "skipping CMTI indication, part already processed");
        return;
    }

    ctx = g_new (SmsPartContext, 1);
    ctx->idx = idx;

    task = g_task_new (self, NULL, NULL, NULL);
    g_task_set_task_data (task, ctx, g_free);

    /* First, request to set the proper storage to read from */
    mm_iface_modem_messaging_lock_storages (MM_IFACE_MODEM_MESSAGING (self),
                                            storage,
                                            MM_SMS_STORAGE_UNKNOWN,
                                            (GAsyncReadyCallback)indication_lock_storages_ready,
                                            task);
}

static void
cds_received (MMPortSerialAt *port,
              GMatchInfo *info,
              MMBroadbandModem *self)
{
    g_autoptr(GError)  error = NULL;
    MMSmsPart         *part;
    guint              length;
    gchar             *pdu;

    mm_obj_dbg (self, "got new non-stored message indication");

    if (!mm_get_uint_from_match_info (info, 1, &length))
        return;

    pdu = g_match_info_fetch (info, 2);
    if (!pdu)
        return;

    part = mm_sms_part_3gpp_new_from_pdu (SMS_PART_INVALID_INDEX, pdu, self, &error);
    if (!part) {
        /* Don't treat the error as critical */
        mm_obj_dbg (self, "error parsing non-stored PDU: %s", error->message);
    } else {
        mm_obj_dbg (self, "correctly parsed non-stored PDU");
        if (!mm_iface_modem_messaging_take_part (MM_IFACE_MODEM_MESSAGING (self),
                                                 mm_broadband_modem_create_sms (MM_BROADBAND_MODEM (self)),
                                                 part,
                                                 MM_SMS_STATE_RECEIVED,
                                                 MM_SMS_STORAGE_UNKNOWN,
                                                 &error)) {
            /* Don't treat the error as critical */
            mm_obj_dbg (self, "error adding SMS: %s", error->message);
        }
    }
}

static void
set_messaging_unsolicited_events_handlers (MMIfaceModemMessaging *self,
                                           gboolean enable,
                                           GAsyncReadyCallback callback,
                                           gpointer user_data)
{
    MMPortSerialAt    *ports[2];
    g_autoptr(GRegex)  cmti_regex = NULL;
    g_autoptr(GRegex)  cds_regex = NULL;
    guint              i;
    GTask             *task;

    cmti_regex = mm_3gpp_cmti_regex_get ();
    cds_regex = mm_3gpp_cds_regex_get ();
    ports[0] = mm_base_modem_peek_port_primary (MM_BASE_MODEM (self));
    ports[1] = mm_base_modem_peek_port_secondary (MM_BASE_MODEM (self));

    /* Add messaging unsolicited events handler for port primary and secondary */
    for (i = 0; i < 2; i++) {
        if (!ports[i])
            continue;

        /* Set/unset unsolicited CMTI event handler */
        mm_obj_dbg (self, "%s messaging unsolicited events handlers in %s",
                    enable ? "setting" : "removing",
                    mm_port_get_device (MM_PORT (ports[i])));
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            cmti_regex,
            enable ? (MMPortSerialAtUnsolicitedMsgFn) cmti_received : NULL,
            enable ? self : NULL,
            NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            cds_regex,
            enable ? (MMPortSerialAtUnsolicitedMsgFn) cds_received : NULL,
            enable ? self : NULL,
            NULL);
    }

    task = g_task_new (self, NULL, callback, user_data);
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_messaging_setup_unsolicited_events (MMIfaceModemMessaging *self,
                                          GAsyncReadyCallback callback,
                                          gpointer user_data)
{
    set_messaging_unsolicited_events_handlers (self, TRUE, callback, user_data);
}

static void
modem_messaging_cleanup_unsolicited_events (MMIfaceModemMessaging *self,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data)
{
    set_messaging_unsolicited_events_handlers (self, FALSE, callback, user_data);
}

/*****************************************************************************/
/* Enable unsolicited events (SMS indications) (Messaging interface) */

typedef struct {
    MMPortSerialAt *primary;
    MMPortSerialAt *secondary;
} MessagingEnableUnsolicitedEventsContext;

static void
messaging_enable_unsolicited_events_context_free (MessagingEnableUnsolicitedEventsContext *ctx)
{
    g_clear_object (&ctx->primary);
    g_clear_object (&ctx->secondary);
    g_slice_free (MessagingEnableUnsolicitedEventsContext, ctx);
}

static gboolean
modem_messaging_enable_unsolicited_events_finish (MMIfaceModemMessaging *self,
                                                  GAsyncResult *res,
                                                  GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static MMBaseModemAtResponseProcessorResult
cnmi_response_processor (MMBaseModem   *self,
                         gpointer       none,
                         const gchar   *command,
                         const gchar   *response,
                         gboolean       last_command,
                         const GError  *error,
                         GVariant     **result,
                         GError       **result_error)
{
    *result = NULL;
    *result_error = NULL;

    if (error) {
        /* If we get a not-supported error and we're not in the last command, we
         * won't set 'result_error', so we'll keep on the sequence */
        if (!g_error_matches (error, MM_MESSAGE_ERROR, MM_MESSAGE_ERROR_NOT_SUPPORTED) || last_command) {
            *result_error = g_error_copy (error);
            return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_FAILURE;
        }

        return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_CONTINUE;
    }

    return MM_BASE_MODEM_AT_RESPONSE_PROCESSOR_RESULT_SUCCESS;
}

static const MMBaseModemAtCommand cnmi_sequence[] = {
    { "+CNMI=2,1,2,1,0", 3, FALSE, cnmi_response_processor },

    /* Many Qualcomm-based devices don't support <ds> of '1', despite
     * reporting they support it in the +CNMI=? response.  But they do
     * accept '2'.
     */
    { "+CNMI=2,1,2,2,0", 3, FALSE, cnmi_response_processor },

    /* Last resort: turn off delivery status reports altogether */
    { "+CNMI=2,1,2,0,0", 3, FALSE, cnmi_response_processor },
    { NULL }
};

static void
modem_messaging_enable_unsolicited_events_secondary_ready (MMBaseModem  *self,
                                                           GAsyncResult *res,
                                                           GTask        *task)
{
    MessagingEnableUnsolicitedEventsContext *ctx;
    g_autoptr(GError)                        error = NULL;

    ctx = g_task_get_task_data (task);

    /* Since the secondary is not required, we don't propagate the error anywhere */
    mm_base_modem_at_sequence_full_finish (MM_BASE_MODEM (self), res, NULL, &error);
    if (error) {
        mm_obj_dbg (self, "failed to enable messaging unsolicited events on secondary port %s: %s",
                    mm_port_get_device (MM_PORT (ctx->secondary)),
                    error->message);
    } else {
        mm_obj_dbg (self, "messaging unsolicited events enabled on secondary port %s",
                    mm_port_get_device (MM_PORT (ctx->secondary)));
    }

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_messaging_enable_unsolicited_events_primary_ready (MMBaseModem  *self,
                                                         GAsyncResult *res,
                                                         GTask        *task)
{
    MessagingEnableUnsolicitedEventsContext *ctx;
    GError                                  *error = NULL;

    mm_base_modem_at_sequence_full_finish (MM_BASE_MODEM (self), res, NULL, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    ctx = g_task_get_task_data (task);
    mm_obj_dbg (self, "messaging unsolicited events enabled on primary port %s",
                mm_port_get_device (MM_PORT (ctx->primary)));

    /* Try to enable unsolicited events for secondary port */
    if (ctx->secondary) {
        mm_obj_dbg (self, "enabling messaging unsolicited events on secondary port %s",
                    mm_port_get_device (MM_PORT (ctx->secondary)));
        mm_base_modem_at_sequence_full (
            MM_BASE_MODEM (self),
            MM_IFACE_PORT_AT (ctx->secondary),
            cnmi_sequence,
            NULL, /* response_processor_context */
            NULL, /* response_processor_context_free */
            NULL,
            (GAsyncReadyCallback)modem_messaging_enable_unsolicited_events_secondary_ready,
            task);
        return;
    }

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_messaging_enable_unsolicited_events (MMIfaceModemMessaging *self,
                                           GAsyncReadyCallback    callback,
                                           gpointer               user_data)
{
    MessagingEnableUnsolicitedEventsContext *ctx;
    GTask                                   *task;

    task = g_task_new (self, NULL, callback, user_data);

    ctx = g_slice_new0 (MessagingEnableUnsolicitedEventsContext);
    ctx->primary = mm_base_modem_get_port_primary (MM_BASE_MODEM (self));
    ctx->secondary = mm_base_modem_get_port_secondary (MM_BASE_MODEM (self));
    g_task_set_task_data (task, ctx, (GDestroyNotify) messaging_enable_unsolicited_events_context_free);

    /* Do nothing if the modem doesn't have any AT port (e.g. it could be
     * a QMI modem trying to enable the parent unsolicited messages) */
    if (!ctx->primary) {
        g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                                 "No AT port to enable messaging unsolicited events");
        g_object_unref (task);
        return;
    }

    /* Enable unsolicited events for primary port */
    mm_obj_dbg (self, "enabling messaging unsolicited events on primary port %s",
                mm_port_get_device (MM_PORT (ctx->primary)));
    mm_base_modem_at_sequence_full (
        MM_BASE_MODEM (self),
        MM_IFACE_PORT_AT (ctx->primary),
        cnmi_sequence,
        NULL, /* response_processor_context */
        NULL, /* response_processor_context_free */
        NULL,
        (GAsyncReadyCallback)modem_messaging_enable_unsolicited_events_primary_ready,
        task);
}

/*****************************************************************************/
/* Load initial list of SMS parts (Messaging interface) */

typedef struct {
    MMSmsStorage list_storage;
} ListPartsContext;

static gboolean
modem_messaging_load_initial_sms_parts_finish (MMIfaceModemMessaging *self,
                                               GAsyncResult *res,
                                               GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static MMSmsState
sms_state_from_str (const gchar *str)
{
    /* We merge unread and read messages in the same state */
    if (strstr (str, "REC"))
        return MM_SMS_STATE_RECEIVED;

    /* look for 'unsent' BEFORE looking for 'sent' */
    if (strstr (str, "UNSENT"))
        return MM_SMS_STATE_STORED;

    if (strstr (str, "SENT"))
        return MM_SMS_STATE_SENT;

    return MM_SMS_STATE_UNKNOWN;
}

static MMSmsPduType
sms_pdu_type_from_str (const gchar *str)
{
    /* We merge unread and read messages in the same state */
    if (strstr (str, "REC"))
        return MM_SMS_PDU_TYPE_DELIVER;

    if (strstr (str, "STO"))
        return MM_SMS_PDU_TYPE_SUBMIT;

    return MM_SMS_PDU_TYPE_UNKNOWN;
}

static void
sms_text_part_list_ready (MMBroadbandModem *self,
                          GAsyncResult *res,
                          GTask *task)
{
    ListPartsContext      *ctx;
    g_autoptr(GRegex)      r = NULL;
    g_autoptr(GMatchInfo)  match_info = NULL;
    const gchar           *response;
    GError                *error = NULL;

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* +CMGL: <index>,<stat>,<oa/da>,[alpha],<scts><CR><LF><data><CR><LF> */
    r = g_regex_new ("\\+CMGL:\\s*(\\d+)\\s*,\\s*([^,]*),\\s*([^,]*),\\s*([^,]*),\\s*([^\\r\\n]*)\\r\\n([^\\r\\n]*)",
                     0, 0, NULL);
    g_assert (r);

    if (!g_regex_match (r, response, 0, &match_info)) {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_INVALID_ARGS,
                                 "Couldn't parse SMS list response");
        g_object_unref (task);
        return;
    }

    ctx = g_task_get_task_data (task);

    while (g_match_info_matches (match_info)) {
        MMSmsPart            *part;
        guint                 matches;
        guint                 idx;
        g_autofree gchar      *number_enc = NULL;
        g_autofree gchar      *number = NULL;
        g_autofree gchar      *timestamp = NULL;
        g_autofree gchar      *text_enc = NULL;
        g_autofree gchar      *text = NULL;
        g_autofree gchar      *stat = NULL;
        g_autoptr(GByteArray)  raw = NULL;
        g_autoptr(GError)      inner_error = NULL;

        matches = g_match_info_get_match_count (match_info);
        if (matches != 7) {
            mm_obj_dbg (self, "failed to match entire CMGL response (count %d)", matches);
            goto next;
        }

        if (!mm_get_uint_from_match_info (match_info, 1, &idx)) {
            mm_obj_dbg (self, "failed to convert message index");
            goto next;
        }

        /* Get part state */
        stat = mm_get_string_unquoted_from_match_info (match_info, 2);
        if (!stat) {
            mm_obj_dbg (self, "failed to get part status");
            goto next;
        }

        /* Get and parse number */
        number_enc = mm_get_string_unquoted_from_match_info (match_info, 3);
        if (!number_enc) {
            mm_obj_dbg (self, "failed to get message sender number");
            goto next;
        }
        number = mm_modem_charset_str_to_utf8 (number_enc, -1, self->priv->modem_current_charset, FALSE, &inner_error);
        if (!number) {
            mm_obj_dbg (self, "failed to convert message sender number to UTF-8: %s", inner_error->message);
            goto next;
        }

        /* Get and parse timestamp (always expected in ASCII) */
        timestamp = mm_get_string_unquoted_from_match_info (match_info, 5);
        if (timestamp && !g_str_is_ascii (timestamp)) {
            mm_obj_dbg (self, "failed to parse input timestamp as ASCII");
            goto next;
        }

        /* Get and parse text */
        text_enc = g_match_info_fetch (match_info, 6);
        text = mm_modem_charset_str_to_utf8 (text_enc, -1, self->priv->modem_current_charset, FALSE, &inner_error);
        if (!text) {
            mm_obj_dbg (self, "failed to convert message text to UTF-8: %s", inner_error->message);
            goto next;
        }

        /* The raw SMS data can only be GSM, UCS2, or unknown (8-bit), so we
         * need to convert to UCS2 here.
         */
        raw = mm_modem_charset_bytearray_from_utf8 (text, MM_MODEM_CHARSET_UCS2, FALSE, NULL);
        g_assert (raw);

        /* all take() methods pass ownership of the value as well */
        part = mm_sms_part_new (idx, sms_pdu_type_from_str (stat));
        mm_sms_part_take_number (part, g_steal_pointer (&number));
        mm_sms_part_take_timestamp (part, g_steal_pointer (&timestamp));
        mm_sms_part_take_text (part, g_steal_pointer (&text));
        mm_sms_part_take_data (part, g_steal_pointer (&raw));
        mm_sms_part_set_class (part, -1);

        mm_obj_dbg (self, "correctly parsed SMS list entry (%d)", idx);
        if (!mm_iface_modem_messaging_take_part (MM_IFACE_MODEM_MESSAGING (self),
                                                 mm_broadband_modem_create_sms (MM_BROADBAND_MODEM (self)),
                                                 part,
                                                 sms_state_from_str (stat),
                                                 ctx->list_storage,
                                                 &error)) {
            mm_obj_dbg (self, "failed to add SMS: %s", inner_error->message);
            goto next;
        }

next:
        g_match_info_next (match_info, NULL);
    }

    /* We consider all done */
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static MMSmsState
sms_state_from_index (guint index)
{
    /* We merge unread and read messages in the same state */
    switch (index) {
    case 0: /* received, unread */
    case 1: /* received, read */
        return MM_SMS_STATE_RECEIVED;
    case 2:
        return MM_SMS_STATE_STORED;
    case 3:
        return MM_SMS_STATE_SENT;
    default:
        return MM_SMS_STATE_UNKNOWN;
    }
}

static void
sms_pdu_part_list_ready (MMBroadbandModem *self,
                         GAsyncResult *res,
                         GTask *task)
{
    ListPartsContext *ctx;
    const gchar *response;
    GError *error = NULL;
    GList *info_list;
    GList *l;

    /* Always always always unlock mem1 storage. Warned you've been. */
    mm_iface_modem_messaging_unlock_storages (MM_IFACE_MODEM_MESSAGING (self), TRUE, FALSE);

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    info_list = mm_3gpp_parse_pdu_cmgl_response (response, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    ctx = g_task_get_task_data (task);

    for (l = info_list; l; l = g_list_next (l)) {
        MM3gppPduInfo *info = l->data;
        MMSmsPart *part;

        part = mm_sms_part_3gpp_new_from_pdu (info->index, info->pdu, self, &error);
        if (!part) {
            /* Don't treat the error as critical */
            mm_obj_dbg (self, "error parsing PDU (%d): %s", info->index, error->message);
            g_clear_error (&error);
        } else {
            mm_obj_dbg (self, "correctly parsed PDU (%d)", info->index);
            if (!mm_iface_modem_messaging_take_part (MM_IFACE_MODEM_MESSAGING (self),
                                                     mm_broadband_modem_create_sms (MM_BROADBAND_MODEM (self)),
                                                     part,
                                                     sms_state_from_index (info->status),
                                                     ctx->list_storage,
                                                     &error)) {
                /* Don't treat the error as critical */
                mm_obj_dbg (self, "error adding SMS (%d): %s", info->index, error->message);
                g_clear_error (&error);
            }
        }
    }

    mm_3gpp_pdu_info_list_free (info_list);

    /* We consider all done */
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
list_parts_lock_storages_ready (MMIfaceModemMessaging *self,
                                GAsyncResult          *res,
                                GTask                 *task)
{
    GError *error = NULL;

    if (!mm_iface_modem_messaging_lock_storages_finish (self, res, &error)) {
        /* TODO: we should either make this lock() never fail, by automatically
         * retrying after some time, or otherwise retry here. */
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Storage now set and locked */

    /* Get SMS parts from ALL types.
     * Different command to be used if we are on Text or PDU mode */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              (MM_BROADBAND_MODEM (self)->priv->modem_messaging_sms_pdu_mode ?
                               "+CMGL=4" :
                               "+CMGL=\"ALL\""),
                              120,
                              FALSE,
                              (GAsyncReadyCallback) (MM_BROADBAND_MODEM (self)->priv->modem_messaging_sms_pdu_mode ?
                                                     sms_pdu_part_list_ready :
                                                     sms_text_part_list_ready),
                              task);
}

static void
modem_messaging_load_initial_sms_parts (MMIfaceModemMessaging *self,
                                        MMSmsStorage storage,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data)
{
    ListPartsContext *ctx;
    GTask *task;

    ctx = g_new (ListPartsContext, 1);
    ctx->list_storage = storage;

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, g_free);

    mm_obj_dbg (self, "listing SMS parts in storage '%s'", mm_sms_storage_get_string (storage));

    /* First, request to set the proper storage to read from */
    mm_iface_modem_messaging_lock_storages (self,
                                            storage,
                                            MM_SMS_STORAGE_UNKNOWN,
                                            (GAsyncReadyCallback)list_parts_lock_storages_ready,
                                            task);
}

/*****************************************************************************/
/* Create SMS */

MMBaseSms *
mm_broadband_modem_create_sms (MMBroadbandModem *self)
{
    g_assert (MM_BROADBAND_MODEM_GET_CLASS (self)->create_sms != NULL);

    return MM_BROADBAND_MODEM_GET_CLASS (self)->create_sms (self);
}

static MMBaseSms *
modem_messaging_create_sms (MMBroadbandModem *self)
{
    MMSmsStorage default_storage;

    default_storage = mm_iface_modem_messaging_get_default_storage (MM_IFACE_MODEM_MESSAGING (self));
    return mm_sms_at_new (MM_BASE_MODEM (self),
                          mm_iface_modem_is_3gpp (MM_IFACE_MODEM (self)),
                          default_storage);
}

/*****************************************************************************/
/* Check if Voice supported (Voice interface) */

static gboolean
modem_voice_check_support_finish (MMIfaceModemVoice *self,
                                  GAsyncResult *res,
                                  GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
ignore_sim_related_errors (GError **error)
{
    g_assert (error && *error);
    if (g_error_matches (*error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_NOT_INSERTED) ||
        g_error_matches (*error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_PIN)          ||
        g_error_matches (*error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_PUK)          ||
        g_error_matches (*error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_FAILURE)      ||
        g_error_matches (*error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_BUSY)         ||
        g_error_matches (*error, MM_MOBILE_EQUIPMENT_ERROR, MM_MOBILE_EQUIPMENT_ERROR_SIM_WRONG)) {
        g_clear_error (error);
    }
}

static void
clcc_format_check_ready (MMBroadbandModem *self,
                         GAsyncResult     *res,
                         GTask            *task)
{
    /* +CLCC supported unless we got any error response */
    self->priv->clcc_supported = !!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, NULL);

    /* If +CLCC unsupported we disable polling in the parent directly */
    g_object_set (self,
                  MM_IFACE_MODEM_VOICE_PERIODIC_CALL_LIST_CHECK_DISABLED, !self->priv->clcc_supported,
                  NULL);

    /* ATH command is supported; assume we have full voice capabilities */
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
ath_format_check_ready (MMBroadbandModem *self,
                        GAsyncResult *res,
                        GTask *task)
{
    GError *error = NULL;

    mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        /* Ignore some errors that the module may return when there is no SIM inserted or
         * if the SIM is PIN-locked. We do need the voice interface exposed even in those
         * cases, in order to support emergency calls */
        ignore_sim_related_errors (&error);
        if (error) {
            g_task_return_error (task, error);
            g_object_unref (task);
            return;
        }
    }

    /* Also check if +CLCC is supported */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CLCC=?",
                              3,
                              /* Do NOT cache as the reply may be different if PIN locked
                               * or unlocked. E.g. we may not support +CLCC for emergency
                               * voice calls. */
                              FALSE,
                              (GAsyncReadyCallback)clcc_format_check_ready,
                              task);
}

static void
modem_voice_check_support (MMIfaceModemVoice *self,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* We assume that all modems have voice capabilities, but ... */

    /* Check ATH support */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "H",
                              3,
                              FALSE,
                              (GAsyncReadyCallback)ath_format_check_ready,
                              task);
}

/*****************************************************************************/
/* Load full list of calls (Voice interface) */

static gboolean
modem_voice_load_call_list_finish (MMIfaceModemVoice  *self,
                                   GAsyncResult       *res,
                                   GList             **out_call_info_list,
                                   GError            **error)
{
    GList  *call_info_list;
    GError *inner_error = NULL;

    call_info_list = g_task_propagate_pointer (G_TASK (res), &inner_error);
    if (inner_error) {
        g_assert (!call_info_list);
        g_propagate_error (error, inner_error);
        return FALSE;
    }

    *out_call_info_list = call_info_list;
    return TRUE;
}

static void
clcc_ready (MMBaseModem  *self,
            GAsyncResult *res,
            GTask        *task)
{
    const gchar *response;
    GError      *error = NULL;
    GList       *call_info_list = NULL;

    response = mm_base_modem_at_command_finish (self, res, &error);
    if (!response || !mm_3gpp_parse_clcc_response (response, self, &call_info_list, &error))
        g_task_return_error (task, error);
    else
        g_task_return_pointer (task, call_info_list, (GDestroyNotify)mm_3gpp_call_info_list_free);
    g_object_unref (task);
}

static void
modem_voice_load_call_list (MMIfaceModemVoice   *self,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CLCC",
                              5,
                              FALSE,
                              (GAsyncReadyCallback)clcc_ready,
                              task);
}

/*****************************************************************************/
/* Setup/cleanup voice related in-call unsolicited events (Voice interface) */

static gboolean
modem_voice_setup_cleanup_in_call_unsolicited_events_finish (MMIfaceModemVoice  *self,
                                                             GAsyncResult       *res,
                                                             GError            **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
in_call_event_received (MMPortSerialAt   *port,
                        GMatchInfo       *info,
                        MMBroadbandModem *self)
{
    MMCallInfo  call_info;
    gchar      *str;

    call_info.index     = 0;
    call_info.direction = MM_CALL_DIRECTION_UNKNOWN;
    call_info.state     = MM_CALL_STATE_TERMINATED;
    call_info.number    = NULL;

    str = g_match_info_fetch (info, 1);
    mm_obj_dbg (self, "call terminated: %s", str);
    g_free (str);

    mm_iface_modem_voice_report_call (MM_IFACE_MODEM_VOICE (self), &call_info);
}

static void
set_voice_in_call_unsolicited_events_handlers (MMBroadbandModem *self,
                                               PortsContext     *ports_ctx,
                                               gboolean          enable)
{
    MMPortSerialAt    *ports[2];
    g_autoptr(GRegex) in_call_event_regex = NULL;
    guint             i;

    in_call_event_regex = mm_call_end_regex_get ();

    ports[0] = MM_PORT_SERIAL_AT (ports_ctx->primary);
    ports[1] = MM_PORT_SERIAL_AT (ports_ctx->secondary);

    /* Enable unsolicited events in given port */
    for (i = 0; i < 2; i++) {
        if (!ports[i])
            continue;

        mm_obj_dbg (self, "%s voice in-call unsolicited events handlers in %s",
                    enable ? "setting" : "removing",
                    mm_port_get_device (MM_PORT (ports[i])));
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            in_call_event_regex,
            enable ? (MMPortSerialAtUnsolicitedMsgFn) in_call_event_received : NULL,
            enable ? self : NULL,
            NULL);
    }
}

static void
modem_voice_setup_in_call_unsolicited_events (MMIfaceModemVoice   *_self,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
    MMBroadbandModem *self;
    GTask            *task;
    GError           *error = NULL;

    self = MM_BROADBAND_MODEM (_self);
    if (!self->priv->in_call_ports_ctx)  {
        PortsContext *ctx;

        mm_obj_dbg (self, "setting up in-call ports context");
        ctx = ports_context_new ();
        if (!ports_context_open (self, ctx, FALSE, TRUE, FALSE, &error)) {
            ports_context_unref (ctx);
            g_prefix_error (&error, "Couldn't open ports in-call: ");
        } else {
            set_voice_in_call_unsolicited_events_handlers (self, ctx, TRUE);
            self->priv->in_call_ports_ctx = ctx;
        }
    } else
        mm_obj_dbg (self, "in-call ports context already set up");

    task = g_task_new (self, NULL, callback, user_data);
    if (error)
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_voice_cleanup_in_call_unsolicited_events (MMIfaceModemVoice   *_self,
                                                GAsyncReadyCallback  callback,
                                                gpointer             user_data)
{
    MMBroadbandModem *self;
    GTask            *task;

    self = MM_BROADBAND_MODEM (_self);
    if (self->priv->in_call_ports_ctx)  {
        mm_obj_dbg (self, "cleaning up in-call ports context");
        set_voice_in_call_unsolicited_events_handlers (self, self->priv->in_call_ports_ctx, FALSE);
        g_clear_pointer (&self->priv->in_call_ports_ctx, (GDestroyNotify) ports_context_unref);
    } else
        mm_obj_dbg (self, "in-call ports context already cleaned up");

    task = g_task_new (self, NULL, callback, user_data);
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

/*****************************************************************************/
/* Setup/cleanup voice related unsolicited events (Voice interface) */

static gboolean
modem_voice_setup_cleanup_unsolicited_events_finish (MMIfaceModemVoice *self,
                                                     GAsyncResult *res,
                                                     GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
ccwa_received (MMPortSerialAt   *port,
               GMatchInfo       *info,
               MMBroadbandModem *self)
{
    MMCallInfo call_info;
    gboolean   indication_call_list_reload_enabled = FALSE;

    g_object_get (self,
                  MM_IFACE_MODEM_VOICE_INDICATION_CALL_LIST_RELOAD_ENABLED, &indication_call_list_reload_enabled,
                  NULL);

    if (indication_call_list_reload_enabled) {
        mm_obj_dbg (self, "call waiting, refreshing call list");
        mm_iface_modem_voice_reload_all_calls (MM_IFACE_MODEM_VOICE (self), NULL, NULL);
        return;
    }

    call_info.index     = 0;
    call_info.direction = MM_CALL_DIRECTION_INCOMING;
    call_info.state     = MM_CALL_STATE_WAITING;
    call_info.number    = mm_get_string_unquoted_from_match_info (info, 1);

    mm_obj_dbg (self, "call waiting (%s)", call_info.number);
    mm_iface_modem_voice_report_call (MM_IFACE_MODEM_VOICE (self), &call_info);

    g_free (call_info.number);
}

static void
ring_received (MMPortSerialAt   *port,
               GMatchInfo       *info,
               MMBroadbandModem *self)
{
    MMCallInfo call_info;
    gboolean   indication_call_list_reload_enabled = FALSE;

    g_object_get (self,
                  MM_IFACE_MODEM_VOICE_INDICATION_CALL_LIST_RELOAD_ENABLED, &indication_call_list_reload_enabled,
                  NULL);

    if (indication_call_list_reload_enabled) {
        mm_obj_dbg (self, "ringing, refreshing call list");
        mm_iface_modem_voice_reload_all_calls (MM_IFACE_MODEM_VOICE (self), NULL, NULL);
        return;
    }

    call_info.index     = 0;
    call_info.direction = MM_CALL_DIRECTION_INCOMING;
    call_info.state     = MM_CALL_STATE_RINGING_IN;
    call_info.number    = NULL;

    mm_obj_dbg (self, "ringing");
    mm_iface_modem_voice_report_call (MM_IFACE_MODEM_VOICE (self), &call_info);
}

static void
cring_received (MMPortSerialAt   *port,
                GMatchInfo       *info,
                MMBroadbandModem *self)
{
    MMCallInfo  call_info;
    gchar      *str;
    gboolean    indication_call_list_reload_enabled = FALSE;

    g_object_get (self,
                  MM_IFACE_MODEM_VOICE_INDICATION_CALL_LIST_RELOAD_ENABLED, &indication_call_list_reload_enabled,
                  NULL);

    if (indication_call_list_reload_enabled) {
        mm_obj_dbg (self, "ringing, refreshing call list");
        mm_iface_modem_voice_reload_all_calls (MM_IFACE_MODEM_VOICE (self), NULL, NULL);
        return;
    }

    /* We could have "VOICE" or "DATA". Now consider only "VOICE" */
    str = mm_get_string_unquoted_from_match_info (info, 1);
    mm_obj_dbg (self, "ringing (%s)", str);
    g_free (str);

    call_info.index     = 0;
    call_info.direction = MM_CALL_DIRECTION_INCOMING;
    call_info.state     = MM_CALL_STATE_RINGING_IN;
    call_info.number    = NULL;

    mm_iface_modem_voice_report_call (MM_IFACE_MODEM_VOICE (self), &call_info);
}

static void
clip_received (MMPortSerialAt   *port,
               GMatchInfo       *info,
               MMBroadbandModem *self)
{
    MMCallInfo call_info;
    gboolean   indication_call_list_reload_enabled = FALSE;

    g_object_get (self,
                  MM_IFACE_MODEM_VOICE_INDICATION_CALL_LIST_RELOAD_ENABLED, &indication_call_list_reload_enabled,
                  NULL);

    if (indication_call_list_reload_enabled) {
        mm_obj_dbg (self, "ringing, refreshing call list");
        mm_iface_modem_voice_reload_all_calls (MM_IFACE_MODEM_VOICE (self), NULL, NULL);
        return;
    }

    call_info.index     = 0;
    call_info.direction = MM_CALL_DIRECTION_INCOMING;
    call_info.state     = MM_CALL_STATE_RINGING_IN;
    call_info.number    = mm_get_string_unquoted_from_match_info (info, 1);

    mm_obj_dbg (self, "ringing (%s)", call_info.number);
    mm_iface_modem_voice_report_call (MM_IFACE_MODEM_VOICE (self), &call_info);

    g_free (call_info.number);
}

static void
set_voice_unsolicited_events_handlers (MMIfaceModemVoice *self,
                                       gboolean enable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
    MMPortSerialAt    *ports[2];
    g_autoptr(GRegex)  cring_regex = NULL;
    g_autoptr(GRegex)  ring_regex = NULL;
    g_autoptr(GRegex)  clip_regex = NULL;
    g_autoptr(GRegex)  ccwa_regex = NULL;
    guint              i;
    GTask             *task;

    cring_regex = mm_voice_cring_regex_get ();
    ring_regex  = mm_voice_ring_regex_get ();
    clip_regex  = mm_voice_clip_regex_get ();
    ccwa_regex  = mm_voice_ccwa_regex_get ();
    ports[0] = mm_base_modem_peek_port_primary (MM_BASE_MODEM (self));
    ports[1] = mm_base_modem_peek_port_secondary (MM_BASE_MODEM (self));

    /* Enable unsolicited events in given port */
    for (i = 0; i < 2; i++) {
        if (!ports[i])
            continue;

        /* Set/unset unsolicited CMTI event handler */
        mm_obj_dbg (self, "%s voice unsolicited events handlers in %s",
                    enable ? "setting" : "removing",
                    mm_port_get_device (MM_PORT (ports[i])));
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            cring_regex,
            enable ? (MMPortSerialAtUnsolicitedMsgFn) cring_received : NULL,
            enable ? self : NULL,
            NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            ring_regex,
            enable ? (MMPortSerialAtUnsolicitedMsgFn) ring_received : NULL,
            enable ? self : NULL,
            NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            clip_regex,
            enable ? (MMPortSerialAtUnsolicitedMsgFn) clip_received : NULL,
            enable ? self : NULL,
            NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            ccwa_regex,
            enable ? (MMPortSerialAtUnsolicitedMsgFn) ccwa_received : NULL,
            enable ? self : NULL,
            NULL);
    }

    task = g_task_new (self, NULL, callback, user_data);
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_voice_setup_unsolicited_events (MMIfaceModemVoice *self,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
    set_voice_unsolicited_events_handlers (self, TRUE, callback, user_data);
}

static void
modem_voice_cleanup_unsolicited_events (MMIfaceModemVoice *self,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data)
{
    set_voice_unsolicited_events_handlers (self, FALSE, callback, user_data);
}

/*****************************************************************************/
/* Enable unsolicited events (CALL indications) (Voice interface) */

typedef struct {
    gboolean        enable;
    MMPortSerialAt *primary;
    MMPortSerialAt *secondary;
    gchar          *clip_command;
    gboolean        clip_primary_done;
    gboolean        clip_secondary_done;
    gchar          *crc_command;
    gboolean        crc_primary_done;
    gboolean        crc_secondary_done;
    gchar          *ccwa_command;
    gboolean        ccwa_primary_done;
    gboolean        ccwa_secondary_done;
} VoiceUnsolicitedEventsContext;

static void
voice_unsolicited_events_context_free (VoiceUnsolicitedEventsContext *ctx)
{
    g_clear_object (&ctx->secondary);
    g_clear_object (&ctx->primary);
    g_free (ctx->clip_command);
    g_free (ctx->crc_command);
    g_free (ctx->ccwa_command);
    g_slice_free (VoiceUnsolicitedEventsContext, ctx);
}

static gboolean
modem_voice_enable_disable_unsolicited_events_finish (MMIfaceModemVoice  *self,
                                                      GAsyncResult       *res,
                                                      GError            **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void run_voice_unsolicited_events_setup (GTask *task);

static void
voice_unsolicited_events_setup_ready (MMBroadbandModem *self,
                                      GAsyncResult     *res,
                                      GTask            *task)
{
    VoiceUnsolicitedEventsContext *ctx;
    GError                        *error = NULL;

    ctx = g_task_get_task_data (task);

    if (!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error)) {
        mm_obj_dbg (self, "couldn't %s voice event reporting: '%s'",
                    ctx->enable ? "enable" : "disable",
                    error->message);
        g_error_free (error);
    }

    /* Continue on next port/command */
    run_voice_unsolicited_events_setup (task);
}

static void
run_voice_unsolicited_events_setup (GTask *task)
{
    MMBroadbandModem              *self;
    VoiceUnsolicitedEventsContext *ctx;
    MMPortSerialAt                *port = NULL;
    const gchar                   *command = NULL;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    /* CLIP on primary port */
    if (!ctx->clip_primary_done && ctx->clip_command && ctx->primary) {
        mm_obj_dbg (self, "%s +CLIP calling line reporting in primary port...", ctx->enable ? "enabling" : "disabling");
        ctx->clip_primary_done = TRUE;
        command = ctx->clip_command;
        port = ctx->primary;
    }
    /* CLIP on secondary port */
    else if (!ctx->clip_secondary_done && ctx->clip_command && ctx->secondary) {
        mm_obj_dbg (self, "%s +CLIP calling line reporting in secondary port...", ctx->enable ? "enabling" : "disabling");
        ctx->clip_secondary_done = TRUE;
        command = ctx->clip_command;
        port = ctx->secondary;
    }
    /* CRC on primary port */
    else if (!ctx->crc_primary_done && ctx->crc_command && ctx->primary) {
        mm_obj_dbg (self, "%s +CRC extended format of incoming call indications in primary port...", ctx->enable ? "enabling" : "disabling");
        ctx->crc_primary_done = TRUE;
        command = ctx->crc_command;
        port = ctx->primary;
    }
    /* CRC on secondary port */
    else if (!ctx->crc_secondary_done && ctx->crc_command && ctx->secondary) {
        mm_obj_dbg (self, "%s +CRC extended format of incoming call indications in secondary port...", ctx->enable ? "enabling" : "disabling");
        ctx->crc_secondary_done = TRUE;
        command = ctx->crc_command;
        port = ctx->secondary;
    }
    /* CCWA on primary port */
    else if (!ctx->ccwa_primary_done && ctx->ccwa_command && ctx->primary) {
        mm_obj_dbg (self, "%s +CCWA call waiting indications in primary port...", ctx->enable ? "enabling" : "disabling");
        ctx->ccwa_primary_done = TRUE;
        command = ctx->ccwa_command;
        port = ctx->primary;
    }
    /* CCWA on secondary port */
    else if (!ctx->ccwa_secondary_done && ctx->ccwa_command && ctx->secondary) {
        mm_obj_dbg (self, "%s +CCWA call waiting indications in secondary port...", ctx->enable ? "enabling" : "disabling");
        ctx->ccwa_secondary_done = TRUE;
        command = ctx->ccwa_command;
        port = ctx->secondary;
    }

    /* Enable/Disable unsolicited events in given port */
    if (port && command) {
        mm_base_modem_at_command_full (MM_BASE_MODEM (self),
                                       MM_IFACE_PORT_AT (port),
                                       command,
                                       3,
                                       FALSE,
                                       FALSE, /* raw */
                                       NULL, /* cancellable */
                                       (GAsyncReadyCallback)voice_unsolicited_events_setup_ready,
                                       task);
        return;
    }

    /* Fully done now */
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);

}

static void
modem_voice_enable_unsolicited_events (MMIfaceModemVoice   *self,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
    VoiceUnsolicitedEventsContext *ctx;
    GTask                         *task;

    task = g_task_new (self, NULL, callback, user_data);

    ctx = g_slice_new0 (VoiceUnsolicitedEventsContext);
    ctx->enable = TRUE;
    ctx->primary = mm_base_modem_get_port_primary (MM_BASE_MODEM (self));
    ctx->secondary = mm_base_modem_get_port_secondary (MM_BASE_MODEM (self));

    /* enable +CLIP URCs with calling line identity */
    ctx->clip_command = g_strdup ("+CLIP=1");
    /* enable +CRING URCs instead of plain RING */
    ctx->crc_command = g_strdup ("+CRC=1");
    /* enable +CCWA call waiting indications */
    ctx->ccwa_command = g_strdup ("+CCWA=1");

    g_task_set_task_data (task, ctx, (GDestroyNotify) voice_unsolicited_events_context_free);

    run_voice_unsolicited_events_setup (task);
}

static void
modem_voice_disable_unsolicited_events (MMIfaceModemVoice   *self,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
    VoiceUnsolicitedEventsContext *ctx;
    GTask                         *task;

    task = g_task_new (self, NULL, callback, user_data);

    ctx = g_slice_new0 (VoiceUnsolicitedEventsContext);
    ctx->primary = mm_base_modem_get_port_primary (MM_BASE_MODEM (self));
    ctx->secondary = mm_base_modem_get_port_secondary (MM_BASE_MODEM (self));

    /* disable +CLIP URCs with calling line identity */
    ctx->clip_command = g_strdup ("+CLIP=0");
    /* disable +CRING URCs instead of plain RING */
    ctx->crc_command = g_strdup ("+CRC=0");
    /* disable +CCWA call waiting indications */
    ctx->ccwa_command = g_strdup ("+CCWA=0");

    g_task_set_task_data (task, ctx, (GDestroyNotify) voice_unsolicited_events_context_free);

    run_voice_unsolicited_events_setup (task);
}

/*****************************************************************************/
/* Create CALL (Voice interface) */

static MMBaseCall *
modem_voice_create_call (MMIfaceModemVoice *_self,
                         MMCallDirection    direction,
                         const gchar       *number)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);

    return mm_base_call_new (MM_BASE_MODEM (self),
                             G_OBJECT (self),
                             direction,
                             number,
                             /* If +CLCC is supported, we want no incoming timeout.
                              * Also, we're able to support detailed call state updates without
                              * additional vendor-specific commands. */
                             self->priv->clcc_supported,   /* skip incoming timeout */
                             self->priv->clcc_supported,   /* dialing->ringing supported */
                             self->priv->clcc_supported);  /* ringing->active supported */
}

/*****************************************************************************/
/* Hold and accept (Voice interface) */

static gboolean
modem_voice_hold_and_accept_finish (MMIfaceModemVoice  *self,
                                    GAsyncResult       *res,
                                    GError            **error)
{
    return !!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
}

static void
modem_voice_hold_and_accept (MMIfaceModemVoice   *self,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CHLD=2",
                              20,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Hangup and accept (Voice interface) */

static gboolean
modem_voice_hangup_and_accept_finish (MMIfaceModemVoice  *self,
                                      GAsyncResult       *res,
                                      GError            **error)
{
    return !!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
}

static void
modem_voice_hangup_and_accept (MMIfaceModemVoice   *self,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CHLD=1",
                              20,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Hangup all (Voice interface) */

static gboolean
modem_voice_hangup_all_finish (MMIfaceModemVoice  *self,
                               GAsyncResult       *res,
                               GError            **error)
{
    return !!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
}

static void
modem_voice_hangup_all (MMIfaceModemVoice   *self,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CHUP",
                              3,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Join multiparty (Voice interface) */

static gboolean
modem_voice_join_multiparty_finish (MMIfaceModemVoice  *self,
                                    GAsyncResult       *res,
                                    GError            **error)
{
    return !!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
}

static void
modem_voice_join_multiparty (MMIfaceModemVoice   *self,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CHLD=3",
                              20,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Leave multiparty (Voice interface) */

static gboolean
modem_voice_leave_multiparty_finish (MMIfaceModemVoice  *self,
                                     GAsyncResult       *res,
                                     GError            **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
chld_leave_multiparty_ready (MMBaseModem  *self,
                             GAsyncResult *res,
                             GTask        *task)
{
    GError *error = NULL;

    if (!mm_base_modem_at_command_finish (self, res, &error))
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_voice_leave_multiparty (MMIfaceModemVoice   *self,
                              MMBaseCall          *call,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
    GTask *task;
    guint  idx;
    gchar *cmd;

    task = g_task_new (self, NULL, callback, user_data);

    idx = mm_base_call_get_index (call);
    if (!idx) {
        g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_WRONG_STATE,
                                 "unknown call index");
        g_object_unref (task);
        return;
    }

    cmd = g_strdup_printf ("+CHLD=2%u", idx);
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              cmd,
                              20,
                              FALSE,
                              (GAsyncReadyCallback) chld_leave_multiparty_ready,
                              task);
    g_free (cmd);
}

/*****************************************************************************/
/* Transfer (Voice interface) */

static gboolean
modem_voice_transfer_finish (MMIfaceModemVoice  *self,
                             GAsyncResult       *res,
                             GError            **error)
{
    return !!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
}

static void
modem_voice_transfer (MMIfaceModemVoice   *self,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CHLD=4",
                              20,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Call waiting setup (Voice interface) */

static gboolean
modem_voice_call_waiting_setup_finish (MMIfaceModemVoice  *self,
                                       GAsyncResult       *res,
                                       GError            **error)
{
    return !!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
}

static void
modem_voice_call_waiting_setup (MMIfaceModemVoice   *self,
                                gboolean             enable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
    gchar *cmd;

    /* Enabling or disabling the call waiting service will only be allowed when
     * the modem is registered in the network, and so, CCWA URC handling will
     * always be setup at this point (as it's part of the modem enabling phase).
     * So, just enable or disable the service (second field) but leaving URCs
     * (first field) always enabled. */
    cmd = g_strdup_printf ("+CCWA=1,%u", enable);
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              cmd,
                              60,
                              FALSE,
                              callback,
                              user_data);
    g_free (cmd);
}

/*****************************************************************************/
/* Call waiting query (Voice interface) */

static gboolean
modem_voice_call_waiting_query_finish (MMIfaceModemVoice  *self,
                                       GAsyncResult       *res,
                                       gboolean           *status,
                                       GError            **error)
{
    const gchar *response;

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
    if (!response)
        return FALSE;

    return mm_3gpp_parse_ccwa_service_query_response (response, self, status, error);
}

static void
modem_voice_call_waiting_query (MMIfaceModemVoice   *self,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
    /* This operation will only be allowed while enabled, and so, CCWA URC
     * handling would always be enabled at this point. So, just perform the
     * query, but leaving URCs enabled either way. */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CCWA=1,2",
                              60,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* ESN loading (CDMA interface) */

static gchar *
modem_cdma_load_esn_finish (MMIfaceModemCdma *self,
                            GAsyncResult *res,
                            GError **error)
{
    const gchar *result;
    gchar *esn = NULL;

    result = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
    if (!result)
        return NULL;

    result = mm_strip_tag (result, "+GSN:");
    mm_parse_gsn (result, NULL, NULL, &esn);
    mm_obj_dbg (self, "loaded ESN: %s", esn);
    return esn;
}

static void
modem_cdma_load_esn (MMIfaceModemCdma *self,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    mm_obj_dbg (self, "loading ESN...");
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+GSN",
                              3,
                              TRUE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* MEID loading (CDMA interface) */

static gchar *
modem_cdma_load_meid_finish (MMIfaceModemCdma *self,
                             GAsyncResult *res,
                             GError **error)
{
    const gchar *result;
    gchar *meid = NULL;

    result = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
    if (!result)
        return NULL;

    result = mm_strip_tag (result, "+GSN:");
    mm_parse_gsn (result, NULL, &meid, NULL);
    mm_obj_dbg (self, "loaded MEID: %s", meid);
    return meid;
}

static void
modem_cdma_load_meid (MMIfaceModemCdma *self,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
    /* Some devices return both the MEID and the ESN in the +GSN response */
    mm_obj_dbg (self, "loading MEID...");
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+GSN",
                              3,
                              TRUE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Setup/Cleanup unsolicited events (CDMA interface) */

typedef struct {
    gboolean setup;
    MMPortSerialQcdm *qcdm;
    gboolean close_port;
} CdmaUnsolicitedEventsContext;

static void
cdma_unsolicited_events_context_free (CdmaUnsolicitedEventsContext *ctx)
{
    if (ctx->qcdm && ctx->close_port)
        mm_port_serial_close (MM_PORT_SERIAL (ctx->qcdm));
    g_clear_object (&ctx->qcdm);

    g_free (ctx);
}

static void
logcmd_qcdm_ready (MMPortSerialQcdm *port,
                   GAsyncResult *res,
                   GTask *task)
{
    MMBroadbandModem *self;
    CdmaUnsolicitedEventsContext *ctx;
    QcdmResult *result;
    gint err = QCDM_SUCCESS;
    GByteArray *response;
    GError *error = NULL;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    response = mm_port_serial_qcdm_command_finish (port, res, &error);
    if (error) {
        ctx->close_port = TRUE;
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Parse the response */
    result = qcdm_cmd_log_config_set_mask_result ((const gchar *) response->data,
                                                  response->len,
                                                  &err);
    g_byte_array_unref (response);
    if (!result) {
        ctx->close_port = TRUE;
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Failed to parse Log Config Set Mask command result: %d",
                                 err);
        g_object_unref (task);
        return;
    }

    mm_port_serial_qcdm_add_unsolicited_msg_handler (port,
                                                     DM_LOG_ITEM_EVDO_PILOT_SETS_V2,
                                                     ctx->setup ? qcdm_evdo_pilot_sets_log_handle : NULL,
                                                     self,
                                                     NULL);

    qcdm_result_unref (result);

    /* Balance the mm_port_seral_open() from modem_cdma_setup_cleanup_unsolicited_events().
     * We want to close it in either case:
     *  (a) we're cleaning up and setup opened the port
     *  (b) if it was unexpectedly closed before cleanup and thus cleanup opened it
     *
     * Setup should leave the port open to allow log messages to be received
     * and sent to handlers.
     */
    ctx->close_port = ctx->setup ? FALSE : TRUE;
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_cdma_setup_cleanup_unsolicited_events (MMBroadbandModem *self,
                                             gboolean setup,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data)
{
    CdmaUnsolicitedEventsContext *ctx;
    GTask *task;
    GByteArray *logcmd;
    uint16_t log_items[] = { DM_LOG_ITEM_EVDO_PILOT_SETS_V2, 0 };
    GError *error = NULL;

    ctx = g_new0 (CdmaUnsolicitedEventsContext, 1);
    ctx->setup = TRUE;

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, (GDestroyNotify)cdma_unsolicited_events_context_free);

    ctx->qcdm = mm_base_modem_get_port_qcdm (MM_BASE_MODEM (self));
    if (!ctx->qcdm) {
        g_task_return_boolean (task, TRUE);
        g_object_unref (task);
        return;
    }

    /* Setup must open the QCDM port and keep it open to receive unsolicited
     * events.  Cleanup expects the port to already be opened from setup, but
     * if not we still want to open it and try to disable log messages.
     */
    if (setup || !mm_port_serial_is_open (MM_PORT_SERIAL (ctx->qcdm))) {
        if (!mm_port_serial_open (MM_PORT_SERIAL (ctx->qcdm), &error)) {
            g_task_return_boolean (task, TRUE);
            g_object_unref (task);
            return;
        }
    }

    logcmd = g_byte_array_sized_new (512);
    logcmd->len = qcdm_cmd_log_config_set_mask_new ((char *) logcmd->data,
                                                    512,
                                                    0x01,  /* Equipment ID */
                                                    setup ? log_items : NULL);
    assert (logcmd->len);

    mm_port_serial_qcdm_command (ctx->qcdm,
                                 logcmd,
                                 5,
                                 NULL,
                                 (GAsyncReadyCallback)logcmd_qcdm_ready,
                                 task);
    g_byte_array_unref (logcmd);
}

static gboolean
modem_cdma_setup_cleanup_unsolicited_events_finish (MMIfaceModemCdma *self,
                                                    GAsyncResult *res,
                                                    GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
modem_cdma_setup_unsolicited_events (MMIfaceModemCdma *self,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
    modem_cdma_setup_cleanup_unsolicited_events (MM_BROADBAND_MODEM (self),
                                                 TRUE,
                                                 callback,
                                                 user_data);
}

static void
modem_cdma_cleanup_unsolicited_events (MMIfaceModemCdma *self,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
    modem_cdma_setup_cleanup_unsolicited_events (MM_BROADBAND_MODEM (self),
                                                 FALSE,
                                                 callback,
                                                 user_data);
}

/*****************************************************************************/
/* HDR state check (CDMA interface) */

typedef struct {
    guint8 hybrid_mode;
    guint8 session_state;
    guint8 almp_state;
} HdrStateResults;

static void
hdr_state_cleanup_port (MMPortSerial *port)
{
    mm_port_serial_close (port);
    g_object_unref (port);
}

static gboolean
modem_cdma_get_hdr_state_finish (MMIfaceModemCdma *self,
                                 GAsyncResult *res,
                                 guint8 *hybrid_mode,
                                 guint8 *session_state,
                                 guint8 *almp_state,
                                 GError **error)
{
    HdrStateResults *results;

    results = g_task_propagate_pointer (G_TASK (res), error);
    if (!results)
        return FALSE;

    *hybrid_mode = results->hybrid_mode;
    *session_state = results->session_state;
    *almp_state = results->almp_state;
    g_free (results);

    return TRUE;
}

static void
hdr_subsys_state_info_ready (MMPortSerialQcdm *port,
                             GAsyncResult *res,
                             GTask *task)
{
    QcdmResult *result;
    HdrStateResults *results;
    gint err = QCDM_SUCCESS;
    GError *error = NULL;
    GByteArray *response;

    response = mm_port_serial_qcdm_command_finish (port, res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Parse the response */
    result = qcdm_cmd_hdr_subsys_state_info_result ((const gchar *) response->data,
                                                    response->len,
                                                    &err);
    g_byte_array_unref (response);
    if (!result) {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Failed to parse HDR subsys state info command result: %d",
                                 err);
        g_object_unref (task);
        return;
    }

    /* Build results */
    results = g_new0 (HdrStateResults, 1);
    qcdm_result_get_u8 (result, QCDM_CMD_HDR_SUBSYS_STATE_INFO_ITEM_HDR_HYBRID_MODE, &results->hybrid_mode);
    results->session_state = QCDM_CMD_HDR_SUBSYS_STATE_INFO_SESSION_STATE_CLOSED;
    qcdm_result_get_u8 (result, QCDM_CMD_HDR_SUBSYS_STATE_INFO_ITEM_SESSION_STATE, &results->session_state);
    results->almp_state = QCDM_CMD_HDR_SUBSYS_STATE_INFO_ALMP_STATE_INACTIVE;
    qcdm_result_get_u8 (result, QCDM_CMD_HDR_SUBSYS_STATE_INFO_ITEM_ALMP_STATE, &results->almp_state);
    qcdm_result_unref (result);

    g_task_return_pointer (task, results, g_free);
    g_object_unref (task);
}

static void
modem_cdma_get_hdr_state (MMIfaceModemCdma *self,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
    MMPortSerialQcdm *qcdm;
    GTask *task;
    GByteArray *hdrstate;
    GError *error = NULL;

    task = g_task_new (self, NULL, callback, user_data);

    qcdm = mm_base_modem_peek_port_qcdm (MM_BASE_MODEM (self));
    if (!qcdm) {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_UNSUPPORTED,
                                 "Cannot get HDR state without a QCDM port");
        g_object_unref (task);
        return;
    }

    if (!mm_port_serial_open (MM_PORT_SERIAL (qcdm), &error)) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    g_task_set_task_data (task,
                          g_object_ref (qcdm),
                          (GDestroyNotify) hdr_state_cleanup_port);

    /* Setup command */
    hdrstate = g_byte_array_sized_new (25);
    hdrstate->len = qcdm_cmd_hdr_subsys_state_info_new ((gchar *) hdrstate->data, 25);
    g_assert (hdrstate->len);

    mm_port_serial_qcdm_command (qcdm,
                                 hdrstate,
                                 3,
                                 NULL,
                                 (GAsyncReadyCallback)hdr_subsys_state_info_ready,
                                 task);
    g_byte_array_unref (hdrstate);
}

/*****************************************************************************/
/* Call Manager state check (CDMA interface) */

typedef struct {
    guint system_mode;
    guint operating_mode;
} CallManagerStateResults;

static void
cm_state_cleanup_port (MMPortSerial *port)
{
    mm_port_serial_close (port);
    g_object_unref (port);
}

static gboolean
modem_cdma_get_call_manager_state_finish (MMIfaceModemCdma *self,
                                          GAsyncResult *res,
                                          guint *system_mode,
                                          guint *operating_mode,
                                          GError **error)
{
    CallManagerStateResults *result;

    result = g_task_propagate_pointer (G_TASK (res), error);
    if (!result)
        return FALSE;

    *system_mode = result->system_mode;
    *operating_mode = result->operating_mode;
    g_free (result);

    return TRUE;
}

static void
cm_subsys_state_info_ready (MMPortSerialQcdm *port,
                            GAsyncResult *res,
                            GTask *task)
{
    QcdmResult *result;
    CallManagerStateResults *results;
    gint err = QCDM_SUCCESS;
    GError *error = NULL;
    GByteArray *response;

    response = mm_port_serial_qcdm_command_finish (port, res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Parse the response */
    result = qcdm_cmd_cm_subsys_state_info_result ((const gchar *) response->data,
                                                   response->len,
                                                   &err);
    g_byte_array_unref (response);
    if (!result) {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Failed to parse CM subsys state info command result: %d",
                                 err);
        g_object_unref (task);
        return;
    }

    /* Build results */
    results = g_new0 (CallManagerStateResults, 1);
    qcdm_result_get_u32 (result, QCDM_CMD_CM_SUBSYS_STATE_INFO_ITEM_OPERATING_MODE, &results->operating_mode);
    qcdm_result_get_u32 (result, QCDM_CMD_CM_SUBSYS_STATE_INFO_ITEM_SYSTEM_MODE, &results->system_mode);
    qcdm_result_unref (result);

    g_task_return_pointer (task, results, g_free);
    g_object_unref (task);
}

static void
modem_cdma_get_call_manager_state (MMIfaceModemCdma *self,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
    MMPortSerialQcdm *qcdm;
    GTask *task;
    GByteArray *cmstate;
    GError *error = NULL;

    task = g_task_new (self, NULL, callback, user_data);

    qcdm = mm_base_modem_peek_port_qcdm (MM_BASE_MODEM (self));
    if (!qcdm) {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_UNSUPPORTED,
                                 "Cannot get call manager state without a QCDM port");
        g_object_unref (task);
        return;
    }

    if (!mm_port_serial_open (MM_PORT_SERIAL (qcdm), &error)) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    g_task_set_task_data (task,
                          g_object_ref (qcdm),
                          (GDestroyNotify) cm_state_cleanup_port);

    /* Setup command */
    cmstate = g_byte_array_sized_new (25);
    cmstate->len = qcdm_cmd_cm_subsys_state_info_new ((gchar *) cmstate->data, 25);
    g_assert (cmstate->len);

    mm_port_serial_qcdm_command (qcdm,
                                 cmstate,
                                 3,
                                 NULL,
                                 (GAsyncReadyCallback)cm_subsys_state_info_ready,
                                 task);
    g_byte_array_unref (cmstate);
}

/*****************************************************************************/
/* Serving System check (CDMA interface) */

typedef struct {
    guint sid;
    guint nid;
    guint class;
    guint band;
} Cdma1xServingSystemResults;

static void
cdma1x_serving_system_state_cleanup_port (MMPortSerial *port)
{
    mm_port_serial_close (port);
    g_object_unref (port);
}

static void
cdma1x_serving_system_complete_and_free (GTask *task,
                                         guint sid,
                                         guint nid,
                                         guint class,
                                         guint band)
{
    Cdma1xServingSystemResults *results;

    results = g_new0 (Cdma1xServingSystemResults, 1);
    results->sid = sid;
    results->band = band;
    results->class = class;
    results->nid = nid;

    g_task_return_pointer (task, results, g_free);
    g_object_unref (task);
}

static GError *
cdma1x_serving_system_no_service_error (void)
{
    /* NOTE: update get_cdma1x_serving_system_ready() in mm-iface-modem-cdma.c
     * if this error changes */
    return g_error_new_literal (MM_MOBILE_EQUIPMENT_ERROR,
                                MM_MOBILE_EQUIPMENT_ERROR_NO_NETWORK,
                                "No CDMA service");
}

static gboolean
modem_cdma_get_cdma1x_serving_system_finish (MMIfaceModemCdma *self,
                                             GAsyncResult *res,
                                             guint *class,
                                             guint *band,
                                             guint *sid,
                                             guint *nid,
                                             GError **error)
{
    Cdma1xServingSystemResults *results;

    results = g_task_propagate_pointer (G_TASK (res), error);
    if (!results)
        return FALSE;

    *sid = results->sid;
    *nid = results->nid;
    *class = results->class;
    *band = results->band;
    g_free (results);

    return TRUE;
}

static void
css_query_ready (MMIfaceModemCdma *self,
                 GAsyncResult *res,
                 GTask *task)
{
    GError *error = NULL;
    const gchar *result;
    gint class = 0;
    gint sid = MM_MODEM_CDMA_SID_UNKNOWN;
    gint num;
    guchar band = 'Z';
    gboolean class_ok = FALSE;
    gboolean band_ok = FALSE;
    gboolean success = FALSE;

    result = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Strip any leading command tag and spaces */
    result = mm_strip_tag (result, "+CSS:");
    num = sscanf (result, "? , %d", &sid);
    if (num == 1) {
        /* UTStarcom and Huawei modems that use IS-707-A format; note that
         * this format obviously doesn't have other indicators like band and
         * class and thus SID 0 will be reported as "no service" (see below).
         */
        class = 0;
        band = 'Z';
        success = TRUE;
    } else {
        g_autoptr(GRegex)     r = NULL;
        g_autoptr(GMatchInfo) match_info = NULL;

        /* Format is "<band_class>,<band>,<sid>" */
        r = g_regex_new ("\\s*([^,]*?)\\s*,\\s*([^,]*?)\\s*,\\s*(\\d+)", G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
        g_assert (r);

        g_regex_match (r, result, 0, &match_info);
        if (g_match_info_get_match_count (match_info) >= 3) {
            gint override_class = 0;
            gchar *str;

            /* band class */
            str = g_match_info_fetch (match_info, 1);
            class = mm_cdma_normalize_class (str);
            g_free (str);

            /* band */
            str = g_match_info_fetch (match_info, 2);
            band = mm_cdma_normalize_band (str, &override_class);
            if (override_class)
                class = override_class;
            g_free (str);

            /* sid */
            str = g_match_info_fetch (match_info, 3);
            if (!mm_get_int_from_str (str, &sid))
                sid = MM_MODEM_CDMA_SID_UNKNOWN;
            g_free (str);

            success = TRUE;
        }
    }

    if (!success) {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Could not parse Serving System results");
        g_object_unref (task);
        return;
    }

    /* Normalize the SID */
    if (sid < 0 || sid > 32767)
        sid = MM_MODEM_CDMA_SID_UNKNOWN;

    if (class == 1 || class == 2)
        class_ok = TRUE;
    if (band != 'Z')
        band_ok = TRUE;

    /* Return 'no service' if none of the elements of the +CSS response
     * indicate that the modem has service.  Note that this allows SID 0
     * when at least one of the other elements indicates service.
     * Normally we'd treat SID 0 as 'no service' but some modems
     * (Sierra 5725) sometimes return SID 0 even when registered.
     */
    if (sid == 0 && !class_ok && !band_ok)
        sid = MM_MODEM_CDMA_SID_UNKNOWN;

    /* 99999 means unknown/no service */
    if (sid == MM_MODEM_CDMA_SID_UNKNOWN) {
        g_task_return_error (task, cdma1x_serving_system_no_service_error ());
        g_object_unref (task);
        return;
    }

    /* No means to get NID with AT commands right now */
    cdma1x_serving_system_complete_and_free (task, sid, MM_MODEM_CDMA_NID_UNKNOWN, class, band);
}

static void
serving_system_query_css (GTask *task)
{
    mm_base_modem_at_command (MM_BASE_MODEM (g_task_get_source_object (task)),
                              "+CSS?",
                              3,
                              FALSE,
                              (GAsyncReadyCallback)css_query_ready,
                              task);
}

static void
qcdm_cdma_status_ready (MMPortSerialQcdm *port,
                        GAsyncResult *res,
                        GTask *task)
{
    MMBroadbandModem *self;
    QcdmResult *result = NULL;
    guint32 sid = MM_MODEM_CDMA_SID_UNKNOWN;
    guint32 nid = MM_MODEM_CDMA_NID_UNKNOWN;
    guint32 rxstate = 0;
    gint err = QCDM_SUCCESS;
    GError *error = NULL;
    GByteArray *response;

    self = g_task_get_source_object (task);

    response = mm_port_serial_qcdm_command_finish (port, res, &error);
    if (error) {
        mm_obj_dbg (self, "failed to get cdma status: %s", error->message);
        g_clear_error (&error);

        /* Fall back to AT+CSS */
        serving_system_query_css (task);
        return;
    }

    result = qcdm_cmd_cdma_status_result ((const gchar *) response->data,
                                          response->len,
                                          &err);
    if (!result) {
        if (err != QCDM_SUCCESS)
            mm_obj_dbg (self, "failed to parse cdma status command result: %d", err);
        g_byte_array_unref (response);

        /* Fall back to AT+CSS */
        serving_system_query_css (task);
        return;
    }

    g_byte_array_unref (response);

    qcdm_result_get_u32 (result, QCDM_CMD_CDMA_STATUS_ITEM_RX_STATE, &rxstate);
    qcdm_result_get_u32 (result, QCDM_CMD_CDMA_STATUS_ITEM_SID, &sid);
    qcdm_result_get_u32 (result, QCDM_CMD_CDMA_STATUS_ITEM_NID, &nid);
    qcdm_result_unref (result);

    /* 99999 means unknown/no service */
    if (rxstate == QCDM_CMD_CDMA_STATUS_RX_STATE_ENTERING_CDMA) {
        sid = MM_MODEM_CDMA_SID_UNKNOWN;
        nid = MM_MODEM_CDMA_NID_UNKNOWN;
    }

    mm_obj_dbg (self, "CDMA 1x Status RX state: %d", rxstate);
    mm_obj_dbg (self, "CDMA 1x Status SID: %d", sid);
    mm_obj_dbg (self, "CDMA 1x Status NID: %d", nid);

    cdma1x_serving_system_complete_and_free (task,
                                             sid,
                                             nid,
                                             0,
                                             (sid == MM_MODEM_CDMA_SID_UNKNOWN) ? 0 : 'Z');
}

static void
modem_cdma_get_cdma1x_serving_system (MMIfaceModemCdma *self,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
    GError *error = NULL;
    GByteArray *cdma_status;
    GTask *task;
    MMPortSerialQcdm *qcdm;

    task = g_task_new (self, NULL, callback, user_data);

    qcdm = mm_base_modem_peek_port_qcdm (MM_BASE_MODEM (self));
    if (!qcdm) {
        /* Fall back to AT+CSS */
        serving_system_query_css (task);
        return;
    }

    if (!mm_port_serial_open (MM_PORT_SERIAL (qcdm), &error)) {
        mm_obj_dbg (self, "failed to open QCDM port for serving-system request: %s", error->message);
        g_error_free (error);

        /* Fall back to AT+CSS */
        serving_system_query_css (task);
        return;
    }

    g_task_set_task_data (task,
                          g_object_ref (qcdm),
                          (GDestroyNotify) cdma1x_serving_system_state_cleanup_port);

    /* Setup command */
    cdma_status = g_byte_array_sized_new (25);
    cdma_status->len = qcdm_cmd_cdma_status_new ((char *) cdma_status->data, 25);
    g_assert (cdma_status->len);
    mm_port_serial_qcdm_command (qcdm,
                                 cdma_status,
                                 3,
                                 NULL,
                                 (GAsyncReadyCallback) qcdm_cdma_status_ready,
                                 task);
    g_byte_array_unref (cdma_status);
}

/*****************************************************************************/
/* Service status, analog/digital check (CDMA interface) */

static gboolean
modem_cdma_get_service_status_finish (MMIfaceModemCdma *self,
                                      GAsyncResult *res,
                                      gboolean *has_cdma_service,
                                      GError **error)
{
    GError *inner_error = NULL;
    gboolean value;

    value = g_task_propagate_boolean (G_TASK (res), &inner_error);
    if (inner_error) {
        g_propagate_error (error, inner_error);
        return FALSE;
    }

    *has_cdma_service = value;
    return TRUE;
}

static void
cad_query_ready (MMIfaceModemCdma *self,
                 GAsyncResult *res,
                 GTask *task)
{
    GError *error = NULL;
    const gchar *result;

    result = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error)
        g_task_return_error (task, error);
    else {
        guint cad;

        /* Strip any leading command tag and spaces */
        result = mm_strip_tag (result, "+CAD:");
        if (!mm_get_uint_from_str (result, &cad))
            g_task_return_new_error (task,
                                     MM_CORE_ERROR,
                                     MM_CORE_ERROR_FAILED,
                                     "Failed to parse +CAD response '%s'",
                                     result);
        else
            /* 1 == CDMA service */
            g_task_return_boolean (task, (cad == 1));
    }
    g_object_unref (task);
}

static void
modem_cdma_get_service_status (MMIfaceModemCdma *self,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CAD?",
                              3,
                              FALSE,
                              (GAsyncReadyCallback)cad_query_ready,
                              task);
}

/*****************************************************************************/
/* Detailed registration state (CDMA interface) */

typedef struct {
    MMModemCdmaRegistrationState detailed_cdma1x_state;
    MMModemCdmaRegistrationState detailed_evdo_state;
} DetailedRegistrationStateResults;

typedef struct {
    MMModemCdmaRegistrationState cdma1x_state;
    MMModemCdmaRegistrationState evdo_state;
} DetailedRegistrationStateContext;

static DetailedRegistrationStateResults *
detailed_registration_state_result_new (DetailedRegistrationStateContext *ctx)
{
    DetailedRegistrationStateResults *results;

    results = g_new (DetailedRegistrationStateResults, 1);
    results->detailed_cdma1x_state = ctx->cdma1x_state;
    results->detailed_evdo_state = ctx->evdo_state;

    return results;
}

static gboolean
modem_cdma_get_detailed_registration_state_finish (MMIfaceModemCdma *self,
                                                   GAsyncResult *res,
                                                   MMModemCdmaRegistrationState *detailed_cdma1x_state,
                                                   MMModemCdmaRegistrationState *detailed_evdo_state,
                                                   GError **error)
{
    DetailedRegistrationStateResults *results;

    results = g_task_propagate_pointer (G_TASK (res), error);
    if (!results)
        return FALSE;

    *detailed_cdma1x_state = results->detailed_cdma1x_state;
    *detailed_evdo_state = results->detailed_evdo_state;
    g_free (results);
    return TRUE;
}

static void
speri_ready (MMIfaceModemCdma *self,
             GAsyncResult *res,
             GTask *task)
{
    DetailedRegistrationStateContext *ctx;
    gboolean roaming = FALSE;
    const gchar *response;
    GError *error = NULL;

    ctx = g_task_get_task_data (task);

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        /* silently discard SPERI errors */
        g_error_free (error);
        g_task_return_pointer (task,
                               detailed_registration_state_result_new (ctx),
                               g_free);
        g_object_unref (task);
        return;
    }

    /* Try to parse the results */
    response = mm_strip_tag (response, "$SPERI:");
    if (!response ||
        !mm_cdma_parse_eri (response, &roaming, NULL, NULL)) {
        mm_obj_warn (self, "couldn't parse SPERI response '%s'", response);
        g_task_return_pointer (task,
                               detailed_registration_state_result_new (ctx),
                               g_free);
        g_object_unref (task);
        return;
    }

    if (roaming) {
        /* Change the 1x and EVDO registration states to roaming if they were
         * anything other than UNKNOWN.
         */
        if (ctx->cdma1x_state > MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN)
            ctx->cdma1x_state = MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING;
        if (ctx->evdo_state > MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN)
            ctx->evdo_state = MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING;
    } else {
        /* Change 1x and/or EVDO registration state to home if home/roaming wasn't previously known */
        if (ctx->cdma1x_state == MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED)
            ctx->cdma1x_state = MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
        if (ctx->evdo_state == MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED)
            ctx->evdo_state = MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
    }

    g_task_return_pointer (task,
                           detailed_registration_state_result_new (ctx),
                           g_free);
    g_object_unref (task);
}

static void
spservice_ready (MMIfaceModemCdma *self,
                 GAsyncResult *res,
                 GTask *task)
{
    DetailedRegistrationStateContext *ctx;
    GError *error = NULL;
    const gchar *response;
    MMModemCdmaRegistrationState cdma1x_state;
    MMModemCdmaRegistrationState evdo_state;

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Try to parse the results */
    cdma1x_state = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
    evdo_state = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
    if (!mm_cdma_parse_spservice_read_response (response,
                                                &cdma1x_state,
                                                &evdo_state)) {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Couldn't parse SPSERVICE response '%s'",
                                 response);
        g_object_unref (task);
        return;
    }

    ctx = g_task_get_task_data (task);

    /* Store new intermediate results */
    ctx->cdma1x_state = cdma1x_state;
    ctx->evdo_state = evdo_state;

    /* If SPERI not supported, we're done */
    if (!MM_BROADBAND_MODEM (self)->priv->has_speri) {
        g_task_return_pointer (task,
                               detailed_registration_state_result_new (ctx),
                               g_free);
        g_object_unref (task);
        return;
    }

    /* Get roaming status to override generic registration state */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "$SPERI?",
                              3,
                              FALSE,
                              (GAsyncReadyCallback)speri_ready,
                              task);
}

static void
modem_cdma_get_detailed_registration_state (MMIfaceModemCdma *self,
                                            MMModemCdmaRegistrationState cdma1x_state,
                                            MMModemCdmaRegistrationState evdo_state,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data)
{
    DetailedRegistrationStateContext *ctx;
    GTask *task;

    /* Setup context */
    ctx = g_new0 (DetailedRegistrationStateContext, 1);
    ctx->cdma1x_state = cdma1x_state;
    ctx->evdo_state = evdo_state;

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, g_free);

    /* NOTE: If we get this generic implementation of getting detailed
     * registration state called, we DO know that we have Sprint commands
     * supported, we checked it in setup_registration_checks() */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+SPSERVICE?",
                              3,
                              FALSE,
                              (GAsyncReadyCallback)spservice_ready,
                              task);
}

/*****************************************************************************/
/* Setup registration checks (CDMA interface) */

typedef struct {
    gboolean skip_qcdm_call_manager_step;
    gboolean skip_qcdm_hdr_step;
    gboolean skip_at_cdma_service_status_step;
    gboolean skip_at_cdma1x_serving_system_step;
    gboolean skip_detailed_registration_state;
} SetupRegistrationChecksResults;

typedef struct {
    gboolean has_qcdm_port;
    gboolean has_sprint_commands;
} SetupRegistrationChecksContext;

static SetupRegistrationChecksResults *
setup_registration_checks_results_new (MMBroadbandModem *self,
                                       SetupRegistrationChecksContext *ctx)
{
    SetupRegistrationChecksResults *results;

    results = g_new0 (SetupRegistrationChecksResults, 1);

    /* Skip QCDM steps if no QCDM port */
    if (!ctx->has_qcdm_port) {
        mm_obj_dbg (self, "will skip all QCDM-based registration checks");
        results->skip_qcdm_call_manager_step = TRUE;
        results->skip_qcdm_hdr_step = TRUE;
    }

    if (MM_IFACE_MODEM_CDMA_GET_IFACE (self)->get_detailed_registration_state ==
        modem_cdma_get_detailed_registration_state) {
        /* Skip CDMA1x Serving System check if we have Sprint specific
         * commands AND if the default detailed registration checker
         * is the generic one. Implementations knowing that their
         * CSS response is undesired, should either setup NULL callbacks
         * for the specific step, or subclass this setup and return
         * FALSE themselves. */
        if (ctx->has_sprint_commands) {
            mm_obj_dbg (self, "will skip CDMA1x Serving System check, we do have Sprint commands");
            results->skip_at_cdma1x_serving_system_step = TRUE;
        } else {
            /* If there aren't Sprint specific commands, and the detailed
             * registration state getter wasn't subclassed, skip the step */
            mm_obj_dbg (self, "will skip generic detailed registration check, we don't have Sprint commands");
            results->skip_detailed_registration_state = TRUE;
        }
    }

    return results;
}

static gboolean
modem_cdma_setup_registration_checks_finish (MMIfaceModemCdma *self,
                                             GAsyncResult *res,
                                             gboolean *skip_qcdm_call_manager_step,
                                             gboolean *skip_qcdm_hdr_step,
                                             gboolean *skip_at_cdma_service_status_step,
                                             gboolean *skip_at_cdma1x_serving_system_step,
                                             gboolean *skip_detailed_registration_state,
                                             GError **error)
{
    SetupRegistrationChecksResults *results;

    results = g_task_propagate_pointer (G_TASK (res), error);
    if (!results)
        return FALSE;

    *skip_qcdm_call_manager_step = results->skip_qcdm_call_manager_step;
    *skip_qcdm_hdr_step = results->skip_qcdm_hdr_step;
    *skip_at_cdma_service_status_step = results->skip_at_cdma_service_status_step;
    *skip_at_cdma1x_serving_system_step = results->skip_at_cdma1x_serving_system_step;
    *skip_detailed_registration_state = results->skip_detailed_registration_state;
    g_free (results);
    return TRUE;
}

static void
speri_check_ready (MMIfaceModemCdma *_self,
                   GAsyncResult *res,
                   GTask *task)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    SetupRegistrationChecksContext *ctx;
    GError *error = NULL;

    ctx = g_task_get_task_data (task);

    mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error)
        g_error_free (error);
    else
        /* We DO have SPERI */
        self->priv->has_speri = TRUE;

    /* All done */
    self->priv->checked_sprint_support = TRUE;
    g_task_return_pointer (task,
                           setup_registration_checks_results_new (self, ctx),
                           g_free);
    g_object_unref (task);
}

static void
spservice_check_ready (MMIfaceModemCdma *_self,
                       GAsyncResult *res,
                       GTask *task)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    SetupRegistrationChecksContext *ctx;
    GError *error = NULL;

    ctx = g_task_get_task_data (task);

    mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        g_error_free (error);
        self->priv->checked_sprint_support = TRUE;
        g_task_return_pointer (task,
                               setup_registration_checks_results_new (self, ctx),
                               g_free);
        g_object_unref (task);
        return;
    }

    /* We DO have SPSERVICE, look for SPERI */
    ctx->has_sprint_commands = TRUE;
    self->priv->has_spservice = TRUE;
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "$SPERI?",
                              3,
                              FALSE,
                              (GAsyncReadyCallback)speri_check_ready,
                              task);
}

static void
modem_cdma_setup_registration_checks (MMIfaceModemCdma *_self,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    SetupRegistrationChecksContext *ctx;
    GTask *task;

    ctx = g_new0 (SetupRegistrationChecksContext, 1);

    /* Check if we have a QCDM port */
    ctx->has_qcdm_port = !!mm_base_modem_peek_port_qcdm (MM_BASE_MODEM (self));

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, g_free);

    /* If we have cached results of Sprint command checking, use them */
    if (self->priv->checked_sprint_support) {
        ctx->has_sprint_commands = self->priv->has_spservice;

        /* Completes in idle */
        g_task_return_pointer (task,
                               setup_registration_checks_results_new (self, ctx),
                               g_free);
        g_object_unref (task);
        return;
    }

    /* Otherwise, launch Sprint command checks. */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+SPSERVICE?",
                              3,
                              FALSE,
                              (GAsyncReadyCallback)spservice_check_ready,
                              task);
}

/*****************************************************************************/
/* Register in network (CDMA interface) */

typedef struct {
    MMBroadbandModem *self;
    GCancellable *cancellable;
    GTimer *timer;
    guint max_registration_time;
} RegisterInCdmaNetworkContext;

static void
register_in_cdma_network_context_free (RegisterInCdmaNetworkContext *ctx)
{
    /* If our cancellable reference is still around, clear it */
    if (ctx->self->priv->modem_cdma_pending_registration_cancellable ==
        ctx->cancellable) {
        g_clear_object (&ctx->self->priv->modem_cdma_pending_registration_cancellable);
    }

    if (ctx->timer)
        g_timer_destroy (ctx->timer);

    g_object_unref (ctx->cancellable);
    g_object_unref (ctx->self);
    g_free (ctx);
}

static gboolean
modem_cdma_register_in_network_finish (MMIfaceModemCdma *self,
                                       GAsyncResult *res,
                                       GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

#undef REG_IS_IDLE
#define REG_IS_IDLE(state)                              \
    (state == MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN)

#undef REG_IS_DONE
#define REG_IS_DONE(state)                                  \
    (state == MM_MODEM_CDMA_REGISTRATION_STATE_HOME ||      \
     state == MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING ||   \
     state == MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED)

static void run_cdma_registration_checks_ready (MMBroadbandModem *self,
                                                GAsyncResult *res,
                                                GTask *task);

static gboolean
run_cdma_registration_checks_again (GTask *task)
{
    /* Get fresh registration state */
    mm_iface_modem_cdma_run_registration_checks (
        MM_IFACE_MODEM_CDMA (g_task_get_source_object (task)),
        (GAsyncReadyCallback)run_cdma_registration_checks_ready,
        task);
    return G_SOURCE_REMOVE;
}

static void
run_cdma_registration_checks_ready (MMBroadbandModem *self,
                                    GAsyncResult *res,
                                    GTask *task)
{
    RegisterInCdmaNetworkContext *ctx;
    GError *error = NULL;

    ctx = g_task_get_task_data (task);

    mm_iface_modem_cdma_run_registration_checks_finish (MM_IFACE_MODEM_CDMA (self), res, &error);

    if (error) {
        mm_obj_dbg (self, "CDMA registration check failed: %s", error->message);
        mm_iface_modem_cdma_update_cdma1x_registration_state (
            MM_IFACE_MODEM_CDMA (self),
            MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
            MM_MODEM_CDMA_SID_UNKNOWN,
            MM_MODEM_CDMA_NID_UNKNOWN);
        mm_iface_modem_cdma_update_evdo_registration_state (
            MM_IFACE_MODEM_CDMA (self),
            MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN);
        mm_iface_modem_cdma_update_access_technologies (
            MM_IFACE_MODEM_CDMA (self),
            MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN);

        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* If we got registered in at least one CDMA network, end registration checks */
    if (REG_IS_DONE (self->priv->modem_cdma_cdma1x_registration_state) ||
        REG_IS_DONE (self->priv->modem_cdma_evdo_registration_state)) {
        mm_obj_dbg (self, "registered in a CDMA network (CDMA1x: '%s', EV-DO: '%s')",
                    REG_IS_DONE (self->priv->modem_cdma_cdma1x_registration_state) ? "yes" : "no",
                    REG_IS_DONE (self->priv->modem_cdma_evdo_registration_state) ? "yes" : "no");
        g_task_return_boolean (task, TRUE);
        g_object_unref (task);
        return;
    }

    /* Don't spend too much time waiting to get registered */
    if (g_timer_elapsed (ctx->timer, NULL) > ctx->max_registration_time) {
        mm_obj_dbg (self, "CDMA registration check timed out");
        mm_iface_modem_cdma_update_cdma1x_registration_state (
            MM_IFACE_MODEM_CDMA (self),
            MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
            MM_MODEM_CDMA_SID_UNKNOWN,
            MM_MODEM_CDMA_NID_UNKNOWN);
        mm_iface_modem_cdma_update_evdo_registration_state (
            MM_IFACE_MODEM_CDMA (self),
            MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN);
        mm_iface_modem_cdma_update_access_technologies (
            MM_IFACE_MODEM_CDMA (self),
            MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN);
        error = mm_mobile_equipment_error_for_code (MM_MOBILE_EQUIPMENT_ERROR_NETWORK_TIMEOUT, self);
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Check again in a few seconds. */
    mm_obj_dbg (self, "not yet registered in a CDMA network... will recheck soon");
    g_timeout_add_seconds (3,
                           (GSourceFunc)run_cdma_registration_checks_again,
                           task);
}

static void
modem_cdma_register_in_network (MMIfaceModemCdma *_self,
                                guint max_registration_time,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    RegisterInCdmaNetworkContext *ctx;
    GTask *task;

    /* (Try to) cancel previous registration request */
    if (self->priv->modem_cdma_pending_registration_cancellable) {
        g_cancellable_cancel (self->priv->modem_cdma_pending_registration_cancellable);
        g_clear_object (&self->priv->modem_cdma_pending_registration_cancellable);
    }

    ctx = g_new0 (RegisterInCdmaNetworkContext, 1);
    ctx->self = g_object_ref (self);
    ctx->max_registration_time = max_registration_time;
    ctx->cancellable = g_cancellable_new ();

    /* Keep an accessible reference to the cancellable, so that we can cancel
     * previous request when needed */
    self->priv->modem_cdma_pending_registration_cancellable =
        g_object_ref (ctx->cancellable);

    /* Get fresh registration state */
    ctx->timer = g_timer_new ();

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, (GDestroyNotify)register_in_cdma_network_context_free);

    mm_iface_modem_cdma_run_registration_checks (
        _self,
        (GAsyncReadyCallback)run_cdma_registration_checks_ready,
        task);
}

/*****************************************************************************/
/* Load currently active channels (CellBroadcast interface) */

static GArray *
modem_cell_broadcast_load_channels_finish (MMIfaceModemCellBroadcast *self,
                                           GAsyncResult *res,
                                           GError **error)
{
    return g_task_propagate_pointer (G_TASK (res), error);
}

static void
cscb_channels_format_check_ready (MMBroadbandModem *self,
                                  GAsyncResult *res,
                                  GTask *task)
{
    const gchar *response;
    GError *error = NULL;
    GArray *result;

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Parse reply */
    result = mm_3gpp_parse_cscb_response (response, &error);
    if (!result) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    g_task_return_pointer (task,
                           result,
                           (GDestroyNotify)g_array_unref);
    g_object_unref (task);
}

static void
modem_cell_broadcast_load_channels (MMIfaceModemCellBroadcast *self,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* Load configured channels */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CSCB?",
                              3,
                              TRUE,
                              (GAsyncReadyCallback)cscb_channels_format_check_ready,
                              task);
}

/*****************************************************************************/

static gboolean
modem_cell_broadcast_setup_cleanup_unsolicited_events_finish (MMIfaceModemCellBroadcast *self,
                                                              GAsyncResult *res,
                                                              GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cbc_cbm_received (MMPortSerialAt *port,
                  GMatchInfo *info,
                  MMBroadbandModem *self)
{
    g_autoptr(GError) error = NULL;
    MMCbmPart *part;
    guint length;
    gchar *pdu;

    mm_obj_dbg (self, "got new cell broadcast message indication");

    if (!mm_get_uint_from_match_info (info, 1, &length))
        return;

    pdu = g_match_info_fetch (info, 2);
    if (!pdu)
        return;

    part = mm_cbm_part_new_from_pdu (pdu, self, &error);
    if (!part) {
        /* Don't treat the error as critical */
        mm_obj_dbg (self, "error parsing PDU: %s", error->message);
    } else {
        mm_obj_dbg (self, "correctly parsed PDU");
        if (!mm_iface_modem_cell_broadcast_take_part (MM_IFACE_MODEM_CELL_BROADCAST (self),
                                                      G_OBJECT (self),
                                                      part,
                                                      MM_CBM_STATE_RECEIVED,
                                                      &error)) {
            /* Don't treat the error as critical */
            mm_obj_dbg (self, "error adding CBM: %s", error->message);
        }
    }
}

static void
set_cell_broadcast_unsolicited_events_handlers (MMIfaceModemCellBroadcast *self,
                                                gboolean enable,
                                                GAsyncReadyCallback callback,
                                                gpointer user_data)
{
    MMPortSerialAt    *ports[2];
    g_autoptr(GRegex)  cbm_regex = NULL;
    guint              i;
    GTask             *task;

    cbm_regex = mm_3gpp_cbm_regex_get ();
    ports[0] = mm_base_modem_peek_port_primary (MM_BASE_MODEM (self));
    ports[1] = mm_base_modem_peek_port_secondary (MM_BASE_MODEM (self));

    /* Add cell broadcast unsolicited events handler for port primary and secondary */
    for (i = 0; i < 2; i++) {
        if (!ports[i])
            continue;

        /* Set/unset unsolicited CBM event handler */
        mm_obj_dbg (self, "%s cell broadcast unsolicited events handlers in %s",
                    enable ? "setting" : "removing",
                    mm_port_get_device (MM_PORT (ports[i])));
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            cbm_regex,
            enable ? (MMPortSerialAtUnsolicitedMsgFn) cbc_cbm_received : NULL,
            enable ? self : NULL,
            NULL);
    }

    task = g_task_new (self, NULL, callback, user_data);
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_cell_broadcast_setup_unsolicited_events (MMIfaceModemCellBroadcast *self,
                                               GAsyncReadyCallback callback,
                                               gpointer user_data)
{
    set_cell_broadcast_unsolicited_events_handlers (self, TRUE, callback, user_data);
}

static void
modem_cell_broadcast_cleanup_unsolicited_events (MMIfaceModemCellBroadcast *self,
                                                 GAsyncReadyCallback callback,
                                                 gpointer user_data)
{
    set_cell_broadcast_unsolicited_events_handlers (self, FALSE, callback, user_data);
}

/***********************************************************************************/
/* Get channels  (CellBroadcast interface) */

static gboolean
modem_cell_broadcast_set_channels_finish (MMIfaceModemCellBroadcast *self,
                                          GAsyncResult *res,
                                          GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
modem_cell_broadcast_set_channels_ready (MMBaseModem *self,
                                         GAsyncResult *res,
                                         GTask *task)
{
    GError *error = NULL;

    if (!mm_base_modem_at_command_finish (self, res, &error))
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_cell_broadcast_set_channels (MMIfaceModemCellBroadcast *self,
                                   GArray *channels,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
    GTask *task;
    g_autoptr (GString) cmd = g_string_new ("+CSCB=0,\"");
    guint i;

    task = g_task_new (self, NULL, callback, user_data);

    for (i = 0; i < channels->len; i++) {
        MMCellBroadcastChannels ch = g_array_index (channels, MMCellBroadcastChannels, i);

        if (i > 0)
            g_string_append_c (cmd, ',');

        if (ch.start == ch.end)
            g_string_append_printf (cmd, "%u", ch.start);
        else
            g_string_append_printf (cmd, "%u-%u", ch.start, ch.end);
    }
    g_string_append (cmd, "\",\"\"");

    mm_obj_dbg (self, "Setting channels...");
    mm_base_modem_at_command (
        MM_BASE_MODEM (self),
        cmd->str,
        3,
        FALSE,
        (GAsyncReadyCallback)modem_cell_broadcast_set_channels_ready,
        task);
}

/*********************************************************/
/* Check CellBroadcast support (CellBroadcast interface) */

static gboolean
modem_cell_broadcast_check_support_finish (MMIfaceModemCellBroadcast *self,
                                           GAsyncResult *res,
                                           GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
cscb_format_check_ready (MMBroadbandModem *self,
                         GAsyncResult *res,
                         GTask *task)
{
    GError *error = NULL;

    mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_cell_broadcast_check_support (MMIfaceModemCellBroadcast *self,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* Check cell broadcast support */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CSCB=?",
                              3,
                              TRUE,
                              (GAsyncReadyCallback)cscb_format_check_ready,
                              task);
}

/*****************************************************************************/
/* Load location capabilities (Location interface) */

static MMModemLocationSource
modem_location_load_capabilities_finish (MMIfaceModemLocation *self,
                                         GAsyncResult *res,
                                         GError **error)
{
    GError *inner_error = NULL;
    gssize value;

    value = g_task_propagate_int (G_TASK (res), &inner_error);
    if (inner_error) {
        g_propagate_error (error, inner_error);
        return MM_MODEM_LOCATION_SOURCE_NONE;
    }
    return (MMModemLocationSource)value;
}

static void
modem_location_load_capabilities (MMIfaceModemLocation *self,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* Default location capabilities supported by the generic broadband
     * implementation are only LAC-CI in 3GPP-enabled modems. And even this,
     * will only be true if the modem supports CREG/CGREG=2 */
    if (!mm_iface_modem_is_3gpp (MM_IFACE_MODEM (self)))
        g_task_return_int (task, MM_MODEM_LOCATION_SOURCE_NONE);
    else
        g_task_return_int (task, MM_MODEM_LOCATION_SOURCE_3GPP_LAC_CI);

    g_object_unref (task);
}

/*****************************************************************************/
/* Enable location gathering (Location interface) */

static gboolean
enable_location_gathering_finish (MMIfaceModemLocation *self,
                                  GAsyncResult *res,
                                  GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
enable_location_gathering (MMIfaceModemLocation *self,
                           MMModemLocationSource source,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
    GTask *task;

    /* 3GPP modems need to re-run registration checks when enabling the 3GPP
     * location source, so that we get up to date LAC/CI location information.
     * Note that we don't care for when the registration checks get finished.
     */
    if (source == MM_MODEM_LOCATION_SOURCE_3GPP_LAC_CI &&
        mm_iface_modem_is_3gpp (MM_IFACE_MODEM (self))) {
        /* Reload registration to get LAC/CI */
        mm_iface_modem_3gpp_run_registration_checks (MM_IFACE_MODEM_3GPP (self), NULL, NULL);
        /* Reload registration information to get MCC/MNC */
        if (MM_BROADBAND_MODEM (self)->priv->modem_3gpp_registration_state == MM_MODEM_3GPP_REGISTRATION_STATE_HOME ||
            MM_BROADBAND_MODEM (self)->priv->modem_3gpp_registration_state == MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING)
            mm_iface_modem_3gpp_reload_current_registration_info (MM_IFACE_MODEM_3GPP (self), NULL, NULL);
    }

    /* Done we are */
    task = g_task_new (self, NULL, callback, user_data);
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

/*****************************************************************************/
/* Load network time (Time interface) */

static gchar *
modem_time_load_network_time_finish (MMIfaceModemTime *self,
                                     GAsyncResult *res,
                                     GError **error)
{
    const gchar *response;
    gchar *result = NULL;

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
    if (!response)
        return NULL;
    if (!mm_parse_cclk_response (response, &result, NULL, error))
        return NULL;
    return result;
}

static void
modem_time_load_network_time (MMIfaceModemTime *self,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CCLK?",
                              3,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Load network timezone (Time interface) */

static MMNetworkTimezone *
modem_time_load_network_timezone_finish (MMIfaceModemTime *self,
                                         GAsyncResult *res,
                                         GError **error)
{
    const gchar *response;
    MMNetworkTimezone *tz = NULL;

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
    if (!response)
        return NULL;
    if (!mm_parse_cclk_response (response, NULL, &tz, error))
        return NULL;
    return tz;
}

static void
modem_time_load_network_timezone (MMIfaceModemTime *self,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CCLK?",
                              3,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Check support (Time interface) */

static const MMBaseModemAtCommand time_check_sequence[] = {
    { "+CTZU=1",  3, TRUE, mm_base_modem_response_processor_no_result_continue },
    { "+CCLK?",   3, TRUE, mm_base_modem_response_processor_string },
    { NULL }
};

static gboolean
modem_time_check_support_finish (MMIfaceModemTime *self,
                                 GAsyncResult *res,
                                 GError **error)
{
    return !!mm_base_modem_at_sequence_finish (MM_BASE_MODEM (self), res, NULL, error);
}

static void
modem_time_check_support (MMIfaceModemTime *self,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
    mm_base_modem_at_sequence (MM_BASE_MODEM (self),
                               time_check_sequence,
                               NULL, /* response_processor_context */
                               NULL, /* response_processor_context_free */
                               callback,
                               user_data);
}

/*****************************************************************************/
/* Check support (Signal interface) */

static gboolean
modem_signal_check_support_finish (MMIfaceModemSignal  *self,
                                   GAsyncResult        *res,
                                   GError             **error)
{
    return !!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
}

static void
modem_signal_check_support (MMIfaceModemSignal  *self,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CESQ=?",
                              3,
                              TRUE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Load extended signal information (Signal interface) */

static gboolean
modem_signal_load_values_finish (MMIfaceModemSignal  *self,
                                 GAsyncResult        *res,
                                 MMSignal           **cdma,
                                 MMSignal           **evdo,
                                 MMSignal           **gsm,
                                 MMSignal           **umts,
                                 MMSignal           **lte,
                                 MMSignal           **nr5g,
                                 GError             **error)
{
    const gchar *response;

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
    if (!response || !mm_3gpp_cesq_response_to_signal_info (response, self, gsm, umts, lte, error))
        return FALSE;

    if (cdma)
        *cdma = NULL;
    if (evdo)
        *evdo = NULL;
    if (nr5g)
        *nr5g = NULL;

    return TRUE;
}

static void
modem_signal_load_values (MMIfaceModemSignal  *self,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CESQ",
                              3,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Check support (3GPP profile management interface) */

static gboolean
modem_3gpp_profile_manager_check_support_finish (MMIfaceModem3gppProfileManager  *self,
                                                 GAsyncResult                    *res,
                                                 gchar                          **index_field,
                                                 GError                         **error)
{
    if (g_task_propagate_boolean (G_TASK (res), error)) {
        *index_field = g_strdup ("profile-id");;
        return TRUE;
    }

    return FALSE;
}

static void
profile_manager_cgdcont_test_ready (MMBaseModem  *self,
                                    GAsyncResult *res,
                                    GTask        *task)
{
    GError *error = NULL;

    mm_base_modem_at_command_finish (self, res, &error);
    if (error)
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_3gpp_profile_manager_check_support (MMIfaceModem3gppProfileManager  *self,
                                          GAsyncReadyCallback              callback,
                                          gpointer                         user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);
    if (!mm_iface_modem_is_3gpp (MM_IFACE_MODEM (self))) {
        g_task_return_boolean (task, FALSE);
        g_object_unref (task);
        return;
    }

    /* Query with CGDCONT=? */
    mm_base_modem_at_command (
        MM_BASE_MODEM (self),
        "+CGDCONT=?",
        3,
        TRUE, /* allow caching, it's a test command */
        (GAsyncReadyCallback)profile_manager_cgdcont_test_ready,
        task);
}

/*****************************************************************************/
/* List profiles (3GPP profile management interface) */

typedef struct {
    GList *profiles;
} ListProfilesContext;

static void
list_profiles_context_free (ListProfilesContext *ctx)
{
    mm_3gpp_profile_list_free (ctx->profiles);
    g_slice_free (ListProfilesContext, ctx);
}

static gboolean
modem_3gpp_profile_manager_list_profiles_finish (MMIfaceModem3gppProfileManager  *self,
                                                 GAsyncResult                    *res,
                                                 GList                          **out_profiles,
                                                 GError                         **error)
{
    ListProfilesContext *ctx;

    if (!g_task_propagate_boolean (G_TASK (res), error))
        return FALSE;

    ctx = g_task_get_task_data (G_TASK (res));
    if (out_profiles)
        *out_profiles = g_steal_pointer (&ctx->profiles);
    return TRUE;
}

static void
profile_manager_cgdcont_query_ready (MMBaseModem  *self,
                                     GAsyncResult *res,
                                     GTask        *task)
{
    ListProfilesContext *ctx;
    const gchar         *response;
    GError              *error = NULL;
    GList               *pdp_context_list;

    response = mm_base_modem_at_command_finish (self, res, &error);
    if (!response) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* may return NULL without error if response is empty */
    pdp_context_list = mm_3gpp_parse_cgdcont_read_response (response, &error);
    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    ctx = g_slice_new0 (ListProfilesContext);
    g_task_set_task_data (task, ctx, (GDestroyNotify) list_profiles_context_free);
    ctx->profiles = mm_3gpp_profile_list_new_from_pdp_context_list (pdp_context_list);
    mm_3gpp_pdp_context_list_free (pdp_context_list);

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_3gpp_profile_manager_list_profiles (MMIfaceModem3gppProfileManager  *self,
                                          GAsyncReadyCallback              callback,
                                          gpointer                         user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* Query with CGDCONT? */
    mm_base_modem_at_command (
        MM_BASE_MODEM (self),
        "+CGDCONT?",
        3,
        FALSE,
        (GAsyncReadyCallback)profile_manager_cgdcont_query_ready,
        task);
}

/*****************************************************************************/
/* Check format (3GPP profile management interface) */

typedef struct {
    MMBearerIpFamily ip_type;
    guint            min_profile_id;
    guint            max_profile_id;
} CheckFormatContext;

static void
check_format_context_free (CheckFormatContext *ctx)
{
    g_slice_free (CheckFormatContext, ctx);
}

static gboolean
modem_3gpp_profile_manager_check_format_finish (MMIfaceModem3gppProfileManager  *self,
                                                GAsyncResult                    *res,
                                                gboolean                        *new_id,
                                                gint                            *min_profile_id,
                                                gint                            *max_profile_id,
                                                GEqualFunc                      *apn_cmp,
                                                MM3gppProfileCmpFlags           *profile_cmp_flags,
                                                GError                         **error)
{
    CheckFormatContext *ctx;

    if (!g_task_propagate_boolean (G_TASK (res), error))
        return FALSE;

    ctx = g_task_get_task_data (G_TASK (res));
    if (new_id)
        *new_id = TRUE;
    if (min_profile_id)
        *min_profile_id = (gint) ctx->min_profile_id;
    if (max_profile_id)
        *max_profile_id = (gint) ctx->max_profile_id;
    if (apn_cmp)
        *apn_cmp = (GEqualFunc) mm_3gpp_cmp_apn_name;
    if (profile_cmp_flags)
        *profile_cmp_flags = (MM_3GPP_PROFILE_CMP_FLAGS_NO_AUTH |
                              MM_3GPP_PROFILE_CMP_FLAGS_NO_APN_TYPE |
                              MM_3GPP_PROFILE_CMP_FLAGS_NO_ACCESS_TYPE_PREFERENCE |
                              MM_3GPP_PROFILE_CMP_FLAGS_NO_ROAMING_ALLOWANCE |
                              MM_3GPP_PROFILE_CMP_FLAGS_NO_PROFILE_SOURCE);
    return TRUE;
}

static void
check_format_cgdcont_test_ready (MMBaseModem  *self,
                                 GAsyncResult *res,
                                 GTask        *task)
{
    CheckFormatContext *ctx;
    const gchar        *response;
    GList              *format_list = NULL;
    g_autofree gchar   *ip_family_str = NULL;
    g_autoptr(GError)   error = NULL;
    gboolean            checked = FALSE;

    ctx = g_task_get_task_data (task);

    ip_family_str = mm_bearer_ip_family_build_string_from_mask (ctx->ip_type);

    response = mm_base_modem_at_command_full_finish (self, res, &error);
    if (!response)
        mm_obj_dbg (self, "failed checking context definition format: %s", error->message);
    else {
        format_list = mm_3gpp_parse_cgdcont_test_response (response, self, &error);
        if (error)
            mm_obj_dbg (self, "error parsing +CGDCONT test response: %s", error->message);
        else if (mm_3gpp_pdp_context_format_list_find_range (format_list, ctx->ip_type,
                                                             &ctx->min_profile_id, &ctx->max_profile_id))
            checked = TRUE;
    }

    if (!checked) {
        ctx->min_profile_id = 1;
        ctx->max_profile_id = G_MAXINT-1;
        mm_obj_dbg (self, "unknown +CGDCONT format details for PDP type '%s', using defaults: minimum %d, maximum %d",
                    ip_family_str, ctx->min_profile_id, ctx->max_profile_id);
    } else
        mm_obj_dbg (self, "+CGDCONT format details for PDP type '%s': minimum %d, maximum %d",
                    ip_family_str, ctx->min_profile_id, ctx->max_profile_id);

    mm_3gpp_pdp_context_format_list_free (format_list);

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_3gpp_profile_manager_check_format (MMIfaceModem3gppProfileManager *self,
                                         MMBearerIpFamily                ip_type,
                                         GAsyncReadyCallback             callback,
                                         gpointer                        user_data)
{
    GTask              *task;
    CheckFormatContext *ctx;

    task = g_task_new (self, NULL, callback, user_data);
    ctx = g_slice_new0 (CheckFormatContext);
    ctx->ip_type = ip_type;
    g_task_set_task_data (task, ctx, (GDestroyNotify)check_format_context_free);

    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CGDCONT=?",
                              3,
                              TRUE, /* cached */
                              (GAsyncReadyCallback)check_format_cgdcont_test_ready,
                              task);
}

/*****************************************************************************/
/* Delete profile (3GPP profile management interface) */

static gboolean
modem_3gpp_profile_manager_delete_profile_finish (MMIfaceModem3gppProfileManager  *self,
                                                  GAsyncResult                    *res,
                                                  GError                         **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
profile_manager_cgdcont_reset_ready (MMBaseModem  *self,
                                     GAsyncResult *res,
                                     GTask        *task)
{
    GError *error = NULL;
    gint    profile_id;

    profile_id = GPOINTER_TO_INT (g_task_get_task_data (task));

    if (!mm_base_modem_at_command_finish (self, res, &error)) {
        mm_obj_dbg (self, "attempting to reset context with id '%d' failed: %s", profile_id, error->message);
        g_task_return_error (task, error);
    } else {
        mm_obj_dbg (self, "reseted context with profile id '%d'", profile_id);
        g_task_return_boolean (task, TRUE);
    }
    g_object_unref (task);
}

static void
profile_manager_cgdel_set_ready (MMBaseModem  *self,
                                 GAsyncResult *res,
                                 GTask        *task)
{
    g_autoptr(GError)  error = NULL;
    g_autofree gchar  *cmd = NULL;
    gint               profile_id;

    profile_id = GPOINTER_TO_INT (g_task_get_task_data (task));

    if (mm_base_modem_at_command_finish (self, res, &error)) {
        mm_obj_dbg (self, "deleted context with profile id '%d'", profile_id);
        g_task_return_boolean (task, TRUE);
        g_object_unref (task);
        return;
    }

    mm_obj_dbg (self, "attempting to delete context with id '%d' failed: %s", profile_id, error->message);

    /* From 3GPP TS 27.007 (v16.3.0):
     *  A special form of the set command, +CGDCONT=<cid> causes the values for
     * context number <cid> to become undefined.
     */
    cmd = g_strdup_printf ("+CGDCONT=%d", profile_id);
    mm_base_modem_at_command (
        MM_BASE_MODEM (self),
        cmd,
        3,
        FALSE,
        (GAsyncReadyCallback)profile_manager_cgdcont_reset_ready,
        task);
}

static void
modem_3gpp_profile_manager_delete_profile (MMIfaceModem3gppProfileManager *self,
                                           MM3gppProfile                  *profile,
                                           const gchar                    *index_field,
                                           GAsyncReadyCallback             callback,
                                           gpointer                        user_data)
{
    g_autofree gchar *cmd = NULL;
    GTask            *task;
    gint              profile_id;

    g_assert (g_strcmp0 (index_field, "profile-id") == 0);

    task = g_task_new (self, NULL, callback, user_data);

    profile_id = mm_3gpp_profile_get_profile_id (profile);
    g_assert (profile_id != MM_3GPP_PROFILE_ID_UNKNOWN);
    g_task_set_task_data (task, GINT_TO_POINTER (profile_id), NULL);

    cmd = g_strdup_printf ("+CGDEL=%d", profile_id);

    mm_base_modem_at_command (
        MM_BASE_MODEM (self),
        cmd,
        3,
        FALSE,
        (GAsyncReadyCallback)profile_manager_cgdel_set_ready,
        task);
}

/*****************************************************************************/
/* Deactivate profile (3GPP profile management interface) */

static gboolean
modem_3gpp_profile_manager_check_activated_profile_finish (MMIfaceModem3gppProfileManager  *self,
                                                           GAsyncResult                    *res,
                                                           gboolean                        *out_activated,
                                                           GError                         **error)
{
    GError   *inner_error = NULL;
    gboolean  result;

    result = g_task_propagate_boolean (G_TASK (res), &inner_error);
    if (inner_error) {
        g_propagate_error (error, inner_error);
        return FALSE;
    }

    if (out_activated)
        *out_activated = result;
    return TRUE;
}

static void
check_activated_profile_cgact_query_ready (MMBaseModem  *self,
                                           GAsyncResult *res,
                                           GTask        *task)
{
    MM3gppProfile *profile;
    const gchar   *response;
    GError        *error = NULL;
    GList         *pdp_context_active_list = NULL;
    GList         *l;
    gint           profile_id;
    gboolean       activated = FALSE;

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (response)
        pdp_context_active_list = mm_3gpp_parse_cgact_read_response (response, &error);

    if (error) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    profile = g_task_get_task_data (task);
    profile_id = mm_3gpp_profile_get_profile_id (profile);

    for (l = pdp_context_active_list; l; l = g_list_next (l)) {
        MM3gppPdpContextActive *iter = l->data;

        if ((gint)iter->cid == profile_id) {
            activated = iter->active;
            break;
        }
    }
    mm_3gpp_pdp_context_active_list_free (pdp_context_active_list);

    if (!l)
        g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_NOT_FOUND,
                                 "Profile '%d' not found in CGACT? response",
                                 profile_id);
    else
        g_task_return_boolean (task, activated);
    g_object_unref (task);
}

static void
modem_3gpp_profile_manager_check_activated_profile (MMIfaceModem3gppProfileManager *self,
                                                    MM3gppProfile                  *profile,
                                                    GAsyncReadyCallback             callback,
                                                    gpointer                        user_data)
{
    GTask *task;
    gint   profile_id;

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, g_object_ref (profile), g_object_unref);

    profile_id = mm_3gpp_profile_get_profile_id (profile);

    mm_obj_dbg (self, "checking if profile with id '%d' is already activated...", profile_id);
    mm_base_modem_at_command (
        MM_BASE_MODEM (self),
        "+CGACT?",
        3,
        FALSE,
        (GAsyncReadyCallback)check_activated_profile_cgact_query_ready,
        task);
}

/*****************************************************************************/
/* Deactivate profile (3GPP profile management interface) */

static gboolean
modem_3gpp_profile_manager_deactivate_profile_finish (MMIfaceModem3gppProfileManager  *self,
                                                      GAsyncResult                    *res,
                                                      GError                         **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
deactivate_profile_cgact_set_ready (MMBaseModem  *self,
                                    GAsyncResult *res,
                                    GTask        *task)
{
    GError *error = NULL;

    if (!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error))
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_3gpp_profile_manager_deactivate_profile (MMIfaceModem3gppProfileManager *_self,
                                               MM3gppProfile                  *profile,
                                               GAsyncReadyCallback             callback,
                                               gpointer                        user_data)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (_self);
    GTask            *task;
    gint              profile_id;
    g_autofree gchar *cmd = NULL;

    task = g_task_new (self, NULL, callback, user_data);

    profile_id = mm_3gpp_profile_get_profile_id (profile);

    /* If the profile id for the initial EPS bearer is known (only applicable when
     * the modem is LTE capable), do not deactivate it as it will likely unregister
     * from the network altogether. */
    if (self->priv->initial_eps_bearer_cid_support_checked &&
        self->priv->initial_eps_bearer_cid == profile_id) {
        mm_obj_dbg (self, "skipping profile deactivation (initial EPS bearer)");
        g_task_return_boolean (task, TRUE);
        g_object_unref (task);
        return;
    }

    mm_obj_dbg (self, "deactivating profile with id '%d'...", profile_id);

    cmd = g_strdup_printf ("+CGACT=0,%d", profile_id);
    mm_base_modem_at_command (
        MM_BASE_MODEM (self),
        cmd,
        MM_BASE_BEARER_DEFAULT_DISCONNECTION_TIMEOUT,
        FALSE,
        (GAsyncReadyCallback)deactivate_profile_cgact_set_ready,
        task);
}

/*****************************************************************************/
/* Store profile (3GPP profile management interface) */

static gboolean
modem_3gpp_profile_manager_store_profile_finish (MMIfaceModem3gppProfileManager  *self,
                                                 GAsyncResult                    *res,
                                                 gint                            *out_profile_id,
                                                 MMBearerApnType                 *out_apn_type,
                                                 GError                         **error)
{
    if (!g_task_propagate_boolean (G_TASK (res), error))
        return FALSE;

    if (out_profile_id)
        *out_profile_id = GPOINTER_TO_INT (g_task_get_task_data (G_TASK (res)));
    if (out_apn_type)
        *out_apn_type = MM_BEARER_APN_TYPE_NONE;
    return TRUE;
}

static void
store_profile_cgdcont_set_ready (MMBaseModem  *self,
                                 GAsyncResult *res,
                                 GTask        *task)
{
    GError *error = NULL;

    if (!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error))
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_3gpp_profile_manager_store_profile (MMIfaceModem3gppProfileManager *self,
                                          MM3gppProfile                  *profile,
                                          const gchar                    *index_field,
                                          GAsyncReadyCallback             callback,
                                          gpointer                        user_data)
{
    GTask             *task;
    gint               profile_id;
    MMBearerIpFamily   ip_type;
    const gchar       *apn;
    const gchar       *pdp_type;
    g_autofree gchar  *ip_type_str = NULL;
    g_autofree gchar  *quoted_apn = NULL;
    g_autofree gchar  *cmd = NULL;

    g_assert (g_strcmp0 (index_field, "profile-id") == 0);

    task = g_task_new (self, NULL, callback, user_data);

    profile_id = mm_3gpp_profile_get_profile_id (profile);
    g_assert (profile_id != MM_3GPP_PROFILE_ID_UNKNOWN);
    g_task_set_task_data (task, GINT_TO_POINTER (profile_id), NULL);

    ip_type = mm_3gpp_profile_get_ip_type (profile);
    g_assert (ip_type != MM_BEARER_IP_FAMILY_NONE);
    g_assert (ip_type != MM_BEARER_IP_FAMILY_ANY);
    ip_type_str = mm_bearer_ip_family_build_string_from_mask (ip_type);
    pdp_type = mm_3gpp_get_pdp_type_from_ip_family (ip_type);
    g_assert (pdp_type);

    apn = mm_3gpp_profile_get_apn (profile);
    quoted_apn = mm_at_quote_string (apn);

    mm_obj_dbg (self, "storing profile '%d': apn '%s', ip type '%s'",
                profile_id, apn, ip_type_str);

    cmd = g_strdup_printf ("+CGDCONT=%d,\"%s\",%s", profile_id, pdp_type, quoted_apn);
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              cmd,
                              3,
                              FALSE,
                              (GAsyncReadyCallback) store_profile_cgdcont_set_ready,
                              task);
}

/*****************************************************************************/
/* Load update settings (Firmware interface) */

static MMFirmwareUpdateSettings *
modem_firmware_load_update_settings_finish (MMIfaceModemFirmware  *self,
                                            GAsyncResult          *res,
                                            GError               **error)
{
    return mm_iface_modem_firmware_load_update_settings_in_port_finish (self, res, error);
}

static void
modem_firmware_load_update_settings (MMIfaceModemFirmware *self,
                                     GAsyncReadyCallback   callback,
                                     gpointer              user_data)
{
    mm_iface_modem_firmware_load_update_settings_in_port (
        self,
        MM_PORT (mm_base_modem_peek_port_primary (MM_BASE_MODEM (self))),
        callback,
        user_data);
}

/*****************************************************************************/

static const gchar *primary_init_sequence[] = {
    /* Ensure echo is off */
    "E0",
    /* Get word responses */
    "V1",
    /* Extended numeric codes */
    "+CMEE=1",
    /* Report all call status */
    "X4",
    /* Assert DCD when carrier detected */
    "&C1",
    NULL
};

static const gchar *secondary_init_sequence[] = {
    /* Ensure echo is off */
    "E0",
    NULL
};

static void
setup_ports (MMBroadbandModem *self)
{
    MMPortSerialAt    *ports[2];
    g_autoptr(GRegex)  ciev_regex = NULL;
    g_autoptr(GRegex)  cmti_regex = NULL;
    g_autoptr(GRegex)  cusd_regex = NULL;
    GPtrArray         *array;
    guint              i;
    guint              j;

    ports[0] = mm_base_modem_peek_port_primary (MM_BASE_MODEM (self));
    ports[1] = mm_base_modem_peek_port_secondary (MM_BASE_MODEM (self));

    if (ports[0])
        g_object_set (ports[0],
                      MM_PORT_SERIAL_AT_INIT_SEQUENCE, primary_init_sequence,
                      NULL);

    if (ports[1])
        g_object_set (ports[1],
                      MM_PORT_SERIAL_AT_INIT_SEQUENCE, secondary_init_sequence,
                      NULL);

    /* Cleanup all unsolicited message handlers in all AT ports */
    array = mm_3gpp_creg_regex_get (FALSE);
    ciev_regex = mm_3gpp_ciev_regex_get ();
    cmti_regex = mm_3gpp_cmti_regex_get ();
    cusd_regex = mm_3gpp_cusd_regex_get ();

    for (i = 0; i < 2; i++) {
        if (!ports[i])
            continue;

        for (j = 0; j < array->len; j++)
            mm_port_serial_at_add_unsolicited_msg_handler (MM_PORT_SERIAL_AT (ports[i]), (GRegex *)g_ptr_array_index (array, j), NULL, NULL, NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (MM_PORT_SERIAL_AT (ports[i]), ciev_regex, NULL, NULL, NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (MM_PORT_SERIAL_AT (ports[i]), cmti_regex, NULL, NULL, NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (MM_PORT_SERIAL_AT (ports[i]), cusd_regex, NULL, NULL, NULL);
    }

    mm_3gpp_creg_regex_destroy (array);
}

/*****************************************************************************/
/* Initialization started/stopped */

static gboolean
initialization_stopped (MMBroadbandModem *self,
                        gpointer user_data,
                        GError **error)
{
    PortsContext *ctx = (PortsContext *)user_data;

    if (ctx)
        ports_context_unref (ctx);
    return TRUE;
}

static gpointer
initialization_started_finish (MMBroadbandModem *self,
                               GAsyncResult *res,
                               GError **error)
{
    return g_task_propagate_pointer (G_TASK (res), error);
}

static void
initialization_started (MMBroadbandModem *self,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
    GError *error = NULL;
    GTask *task;
    PortsContext *ctx;

    task = g_task_new (self, NULL, callback, user_data);

    /* Open ports for initialization, just the primary AT port */
    ctx = ports_context_new ();
    if (!ports_context_open (self, ctx, FALSE, FALSE, FALSE, &error)) {
        ports_context_unref (ctx);
        g_prefix_error (&error, "Couldn't open ports during modem initialization: ");
        g_task_return_error (task, error);
    } else
        g_task_return_pointer (task, ctx, (GDestroyNotify)ports_context_unref);
    g_object_unref (task);
}

/*****************************************************************************/
/* Disabling stopped */

static gboolean
disabling_stopped (MMBroadbandModem *self,
                   GError **error)
{
    if (self->priv->enabled_ports_ctx) {
        ports_context_unref (self->priv->enabled_ports_ctx);
        self->priv->enabled_ports_ctx = NULL;
    }

    return TRUE;
}

/*****************************************************************************/
/* Initializing the modem (during first enabling) */

static void
enabling_modem_init (MMBroadbandModem    *self,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
    MMPortSerialAt *primary;

    primary = mm_base_modem_peek_port_primary (MM_BASE_MODEM (self));
    if (!primary) {
        g_task_report_new_error (self, callback, user_data, enabling_modem_init,
                                 MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                                 "Failed to run init command: primary port missing");
        return;
    }

    /* Init command. ITU rec v.250 (6.1.1) says:
     *   The DTE should not include additional commands on the same command line
     *   after the Z command because such commands may be ignored.
     * So run ATZ alone.
     */
    mm_base_modem_at_command_full (MM_BASE_MODEM (self),
                                   MM_IFACE_PORT_AT (primary),
                                   "Z",
                                   6,
                                   FALSE,
                                   FALSE,
                                   NULL, /* cancellable */
                                   callback,
                                   user_data);
}

static gboolean
enabling_modem_init_finish (MMBroadbandModem  *self,
                            GAsyncResult      *res,
                            GError           **error)
{
    if (g_async_result_is_tagged (res, enabling_modem_init))
        return g_task_propagate_boolean (G_TASK (res), error);

    return !!mm_base_modem_at_command_full_finish (MM_BASE_MODEM (self), res, error);
}

/*****************************************************************************/
/* Enabling started */

typedef struct {
    PortsContext *ports;
    gboolean modem_init_required;
} EnablingStartedContext;

static void
enabling_started_context_free (EnablingStartedContext *ctx)
{
    ports_context_unref (ctx->ports);
    g_slice_free (EnablingStartedContext, ctx);
}

static gboolean
enabling_started_finish (MMBroadbandModem *self,
                         GAsyncResult *res,
                         GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static gboolean
enabling_after_modem_init_timeout (GTask *task)
{
    MMBroadbandModem *self;
    EnablingStartedContext *ctx;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    /* Reset init sequence enabled flags and run them explicitly */
    g_assert (ctx->modem_init_required);
    g_object_set (ctx->ports->primary,
                  MM_PORT_SERIAL_AT_INIT_SEQUENCE_ENABLED, TRUE,
                  NULL);
    mm_port_serial_at_run_init_sequence (ctx->ports->primary);
    if (ctx->ports->secondary) {
        g_object_set (ctx->ports->secondary,
                      MM_PORT_SERIAL_AT_INIT_SEQUENCE_ENABLED, TRUE,
                      NULL);
        mm_port_serial_at_run_init_sequence (ctx->ports->secondary);
    }

    /* Store enabled ports context and complete */
    self->priv->enabled_ports_ctx = ports_context_ref (ctx->ports);
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
    return G_SOURCE_REMOVE;
}

static void
enabling_modem_init_ready (MMBroadbandModem *self,
                           GAsyncResult *res,
                           GTask *task)
{
    GError *error = NULL;

    if (!MM_BROADBAND_MODEM_GET_CLASS (self)->enabling_modem_init_finish (self, res, &error)) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Specify that the modem init was run once */
    self->priv->modem_init_run = TRUE;

    /* After the modem init sequence, give a 500ms period for the modem to settle */
    mm_obj_dbg (self, "giving some time to settle the modem...");
    g_timeout_add (500, (GSourceFunc)enabling_after_modem_init_timeout, task);
}

static void
enabling_flash_done (MMPortSerial *port,
                     GAsyncResult *res,
                     GTask *task)
{
    MMBroadbandModem *self;
    EnablingStartedContext *ctx;
    GError *error = NULL;

    if (!mm_port_serial_flash_finish (port, res, &error)) {
        g_prefix_error (&error, "Primary port flashing failed: ");
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    if (ctx->modem_init_required) {
        mm_obj_dbg (self, "running initialization sequence...");
        MM_BROADBAND_MODEM_GET_CLASS (self)->enabling_modem_init (self,
                                                                  (GAsyncReadyCallback)enabling_modem_init_ready,
                                                                  task);
        return;
    }

    /* Store enabled ports context and complete */
    self->priv->enabled_ports_ctx = ports_context_ref (ctx->ports);
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
enabling_started (MMBroadbandModem *self,
                  GAsyncReadyCallback callback,
                  gpointer user_data)
{
    GError *error = NULL;
    EnablingStartedContext *ctx;
    GTask *task;

    ctx = g_slice_new0 (EnablingStartedContext);
    ctx->ports = ports_context_new ();

    /* Skip modem initialization if the device was hotplugged OR if we already
     * did it (i.e. don't reinitialize if the modem got disabled and enabled
     * again) */
    if (self->priv->modem_init_run)
        mm_obj_dbg (self, "skipping initialization: not first enabling");
    else if (mm_base_modem_get_hotplugged (MM_BASE_MODEM (self))) {
        self->priv->modem_init_run = TRUE;
        mm_obj_dbg (self, "skipping initialization: device hotplugged");
    } else if (!MM_BROADBAND_MODEM_GET_CLASS (self)->enabling_modem_init ||
               !MM_BROADBAND_MODEM_GET_CLASS (self)->enabling_modem_init_finish)
        mm_obj_dbg (self, "skipping initialization: not required");
    else
        ctx->modem_init_required = TRUE;

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, (GDestroyNotify)enabling_started_context_free);

    /* Open ports for enabling, including secondary AT port and QCDM if available */
    if (!ports_context_open (self, ctx->ports, ctx->modem_init_required, TRUE, TRUE, &error)) {
        g_prefix_error (&error, "Couldn't open ports during modem enabling: ");
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Ports were correctly opened, now flash the primary port */
    mm_obj_dbg (self, "flashing primary AT port before enabling...");
    mm_port_serial_flash (MM_PORT_SERIAL (ctx->ports->primary),
                          100,
                          FALSE,
                          (GAsyncReadyCallback)enabling_flash_done,
                          task);
}

/*****************************************************************************/
/* First registration checks */

static void
modem_3gpp_run_registration_checks_ready (MMIfaceModem3gpp *self,
                                          GAsyncResult *res)
{
    GError *error = NULL;

    if (!mm_iface_modem_3gpp_run_registration_checks_finish (self, res, &error)) {
        mm_obj_warn (self, "initial 3GPP registration check failed: %s", error->message);
        g_error_free (error);
        return;
    }
    mm_obj_dbg (self, "initial 3GPP registration checks finished");
}

static void
modem_cdma_run_registration_checks_ready (MMIfaceModemCdma *self,
                                          GAsyncResult *res)
{
    GError *error = NULL;

    if (!mm_iface_modem_cdma_run_registration_checks_finish (self, res, &error)) {
        mm_obj_warn (self, "initial CDMA registration check failed: %s", error->message);
        g_error_free (error);
        return;
    }
    mm_obj_dbg (self, "initial CDMA registration checks finished");
}

static gboolean
schedule_initial_registration_checks_cb (MMBroadbandModem *self)
{
    if (mm_iface_modem_is_3gpp (MM_IFACE_MODEM (self)))
        mm_iface_modem_3gpp_run_registration_checks (MM_IFACE_MODEM_3GPP (self),
                                                     (GAsyncReadyCallback) modem_3gpp_run_registration_checks_ready,
                                                     NULL);
    if (mm_iface_modem_is_cdma (MM_IFACE_MODEM (self)))
        mm_iface_modem_cdma_run_registration_checks (MM_IFACE_MODEM_CDMA (self),
                                                     (GAsyncReadyCallback) modem_cdma_run_registration_checks_ready,
                                                     NULL);
    /* We got a full reference, so balance it out here */
    g_object_unref (self);
    return G_SOURCE_REMOVE;
}

static void
schedule_initial_registration_checks (MMBroadbandModem *self)
{
    g_idle_add ((GSourceFunc) schedule_initial_registration_checks_cb, g_object_ref (self));
}

/*****************************************************************************/

typedef enum {
    /* When user requests a disable operation, the process starts here */
    DISABLING_STEP_FIRST,
    DISABLING_STEP_IFACE_SIMPLE_ABORT_ONGOING,
    DISABLING_STEP_WAIT_FOR_FINAL_STATE,
    DISABLING_STEP_DISCONNECT_BEARERS,
    /* When the disabling is launched due to a failed enable, the process
     * starts here */
    DISABLING_STEP_FIRST_AFTER_ENABLE_FAILED,
    DISABLING_STEP_IFACE_SIMPLE,
    DISABLING_STEP_IFACE_FIRMWARE,
    DISABLING_STEP_IFACE_VOICE,
    DISABLING_STEP_IFACE_SIGNAL,
    DISABLING_STEP_IFACE_OMA,
    DISABLING_STEP_IFACE_TIME,
    DISABLING_STEP_IFACE_MESSAGING,
    DISABLING_STEP_IFACE_LOCATION,
    DISABLING_STEP_IFACE_CDMA,
    DISABLING_STEP_IFACE_3GPP_USSD,
    DISABLING_STEP_IFACE_3GPP_PROFILE_MANAGER,
    DISABLING_STEP_IFACE_3GPP,
    DISABLING_STEP_IFACE_MODEM,
    DISABLING_STEP_LAST,
} DisablingStep;

typedef struct {
    gboolean       state_updates;
    DisablingStep  step;
    MMModemState   previous_state;
    GError        *saved_error;
} DisablingContext;

static void
disabling_context_free (DisablingContext *ctx)
{
    g_assert (!ctx->saved_error);
    g_slice_free (DisablingContext, ctx);
}

static gboolean
common_disable_finish (MMBroadbandModem  *self,
                       GAsyncResult      *res,
                       GError           **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
disabling_complete (GTask *task)
{
    MMBroadbandModem  *self;
    DisablingContext  *ctx;
    g_autoptr(GError)  error = NULL;

    self = g_task_get_source_object (task);
    ctx  = g_task_get_task_data (task);

    if (MM_BROADBAND_MODEM_GET_CLASS (self)->disabling_stopped &&
        !MM_BROADBAND_MODEM_GET_CLASS (self)->disabling_stopped (self, &error)) {
        mm_obj_warn (self, "error when stopping the disabling sequence: %s", error->message);
    }

    /* Disable failed? */
    if (ctx->saved_error) {
        if (ctx->state_updates && (ctx->previous_state != MM_MODEM_STATE_DISABLED)) {
            /* Fallback to previous state */
            mm_iface_modem_update_state (MM_IFACE_MODEM (self),
                                         ctx->previous_state,
                                         MM_MODEM_STATE_CHANGE_REASON_UNKNOWN);
        }
        g_task_return_error (task, g_steal_pointer (&ctx->saved_error));
        g_object_unref (task);
        return;
    }

    /* Disable succeeded */
    if (ctx->state_updates)
        mm_iface_modem_update_state (MM_IFACE_MODEM (self),
                                     MM_MODEM_STATE_DISABLED,
                                     MM_MODEM_STATE_CHANGE_REASON_USER_REQUESTED);
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void disabling_step (GTask *task);

#undef INTERFACE_DISABLE_READY_FN
#define INTERFACE_DISABLE_READY_FN(NAME,TYPE,WARN_ERRORS)                             \
    static void                                                                       \
    NAME##_disable_ready (MMBroadbandModem *self,                                     \
                          GAsyncResult     *result,                                   \
                          GTask            *task)                                     \
    {                                                                                 \
        DisablingContext  *ctx;                                                       \
        g_autoptr(GError)  error = NULL;                                              \
                                                                                      \
        if (!mm_##NAME##_disable_finish (TYPE (self), result, &error)) {              \
            if (WARN_ERRORS)                                                          \
                mm_obj_warn (self, "couldn't disable interface: %s", error->message); \
            else                                                                      \
                mm_obj_dbg (self, "couldn't disable interface: %s", error->message);  \
        }                                                                             \
                                                                                      \
        /* Go on to next step */                                                      \
        ctx = g_task_get_task_data (task);                                            \
        ctx->step++;                                                                  \
        disabling_step (task);                                                        \
    }

INTERFACE_DISABLE_READY_FN (iface_modem,                      MM_IFACE_MODEM,                      TRUE)
INTERFACE_DISABLE_READY_FN (iface_modem_3gpp,                 MM_IFACE_MODEM_3GPP,                 TRUE)
INTERFACE_DISABLE_READY_FN (iface_modem_3gpp_ussd,            MM_IFACE_MODEM_3GPP_USSD,            FALSE)
INTERFACE_DISABLE_READY_FN (iface_modem_3gpp_profile_manager, MM_IFACE_MODEM_3GPP_PROFILE_MANAGER, FALSE)
INTERFACE_DISABLE_READY_FN (iface_modem_cdma,                 MM_IFACE_MODEM_CDMA,                 TRUE)
INTERFACE_DISABLE_READY_FN (iface_modem_location,             MM_IFACE_MODEM_LOCATION,             FALSE)
INTERFACE_DISABLE_READY_FN (iface_modem_messaging,            MM_IFACE_MODEM_MESSAGING,            FALSE)
INTERFACE_DISABLE_READY_FN (iface_modem_voice,                MM_IFACE_MODEM_VOICE,                FALSE)
INTERFACE_DISABLE_READY_FN (iface_modem_signal,               MM_IFACE_MODEM_SIGNAL,               FALSE)
INTERFACE_DISABLE_READY_FN (iface_modem_time,                 MM_IFACE_MODEM_TIME,                 FALSE)
INTERFACE_DISABLE_READY_FN (iface_modem_oma,                  MM_IFACE_MODEM_OMA,                  FALSE)

static void
bearer_list_disconnect_bearers_ready (MMBearerList *list,
                                      GAsyncResult *res,
                                      GTask        *task)
{
    DisablingContext *ctx;

    ctx = g_task_get_task_data (task);
    g_assert (!ctx->saved_error);
    if (!mm_bearer_list_disconnect_bearers_finish (list, res, &ctx->saved_error)) {
        disabling_complete (task);
        return;
    }

    /* Go on to next step */
    ctx = g_task_get_task_data (task);
    ctx->step++;
    disabling_step (task);
}

static void
disabling_wait_for_final_state_ready (MMIfaceModem *self,
                                      GAsyncResult *res,
                                      GTask *task)
{
    DisablingContext *ctx;

    ctx = g_task_get_task_data (task);
    g_assert (!ctx->saved_error);
    ctx->previous_state = mm_iface_modem_wait_for_final_state_finish (self, res, &ctx->saved_error);
    if (ctx->saved_error) {
        disabling_complete (task);
        return;
    }

    switch (ctx->previous_state) {
    case MM_MODEM_STATE_UNKNOWN:
    case MM_MODEM_STATE_FAILED:
    case MM_MODEM_STATE_LOCKED:
    case MM_MODEM_STATE_DISABLED:
        /* Just return success, don't relaunch disabling.
         * Note that we do consider here UNKNOWN and FAILED status on purpose,
         * as the MMManager will try to disable every modem before removing
         * it. */
        disabling_complete (task);
        return;
    case MM_MODEM_STATE_INITIALIZING:
    case MM_MODEM_STATE_DISABLING:
    case MM_MODEM_STATE_ENABLING:
    case MM_MODEM_STATE_ENABLED:
    case MM_MODEM_STATE_SEARCHING:
    case MM_MODEM_STATE_REGISTERED:
    case MM_MODEM_STATE_DISCONNECTING:
    case MM_MODEM_STATE_CONNECTING:
    case MM_MODEM_STATE_CONNECTED:
    default:
        break;
    }

    /* We're in a final state now, go on */

    g_assert (ctx->state_updates);
    mm_iface_modem_update_state (MM_IFACE_MODEM (self),
                                 MM_MODEM_STATE_DISABLING,
                                 MM_MODEM_STATE_CHANGE_REASON_USER_REQUESTED);

    ctx->step++;
    disabling_step (task);
}

static void
disabling_step (GTask *task)
{
    MMBroadbandModem *self;
    DisablingContext *ctx;

    self = g_task_get_source_object (task);
    ctx  = g_task_get_task_data (task);
    g_assert (!ctx->saved_error);

    switch (ctx->step) {
    case DISABLING_STEP_FIRST:
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_IFACE_SIMPLE_ABORT_ONGOING:
        /* Connection requests via the Simple interface must be aborted as soon
         * as possible, because certain steps may be explicitly waiting for new
         * state transitions and such. */
        mm_iface_modem_simple_abort_ongoing (MM_IFACE_MODEM_SIMPLE (self));
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_WAIT_FOR_FINAL_STATE:
        /* cancellability allowed at this point */
        if (g_cancellable_set_error_if_cancelled (g_task_get_cancellable (task), &ctx->saved_error)) {
            disabling_complete (task);
            return;
        }

        mm_iface_modem_wait_for_final_state (MM_IFACE_MODEM (self),
                                             MM_MODEM_STATE_UNKNOWN, /* just any */
                                             (GAsyncReadyCallback)disabling_wait_for_final_state_ready,
                                             task);
        return;

    case DISABLING_STEP_DISCONNECT_BEARERS:
        /* cancellability allowed at this point */
        if (g_cancellable_set_error_if_cancelled (g_task_get_cancellable (task), &ctx->saved_error)) {
            disabling_complete (task);
            return;
        }
        if (self->priv->modem_bearer_list) {
            mm_bearer_list_disconnect_bearers (
                self->priv->modem_bearer_list,
                NULL, /* all bearers */
                (GAsyncReadyCallback)bearer_list_disconnect_bearers_ready,
                task);
            return;
        }
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_FIRST_AFTER_ENABLE_FAILED:
        /* From this point onwards, the disabling sequence will NEVER fail, all
         * errors will be treated as non-fatal, including a possible task
         * cancellation. */
        g_task_set_check_cancellable (task, FALSE);
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_IFACE_SIMPLE:
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_IFACE_FIRMWARE:
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_IFACE_VOICE:
        if (self->priv->modem_voice_dbus_skeleton) {
            mm_obj_dbg (self, "modem has voice capabilities, disabling the Voice interface...");
            mm_iface_modem_voice_disable (MM_IFACE_MODEM_VOICE (self),
                                          (GAsyncReadyCallback)iface_modem_voice_disable_ready,
                                          task);
            return;
        }
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_IFACE_SIGNAL:
        if (self->priv->modem_signal_dbus_skeleton) {
            mm_obj_dbg (self, "modem has extended signal reporting capabilities, disabling the Signal interface...");
            mm_iface_modem_signal_disable (MM_IFACE_MODEM_SIGNAL (self),
                                           (GAsyncReadyCallback)iface_modem_signal_disable_ready,
                                           task);
            return;
        }
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_IFACE_OMA:
        if (self->priv->modem_oma_dbus_skeleton) {
            mm_obj_dbg (self, "modem has OMA capabilities, disabling the OMA interface...");
            mm_iface_modem_oma_disable (MM_IFACE_MODEM_OMA (self),
                                        (GAsyncReadyCallback)iface_modem_oma_disable_ready,
                                        task);
            return;
        }
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_IFACE_TIME:
        if (self->priv->modem_time_dbus_skeleton) {
            mm_obj_dbg (self, "modem has time capabilities, disabling the Time interface...");
            mm_iface_modem_time_disable (MM_IFACE_MODEM_TIME (self),
                                         (GAsyncReadyCallback)iface_modem_time_disable_ready,
                                         task);
            return;
        }
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_IFACE_MESSAGING:
        if (self->priv->modem_messaging_dbus_skeleton) {
            mm_obj_dbg (self, "modem has messaging capabilities, disabling the Messaging interface...");
            mm_iface_modem_messaging_disable (MM_IFACE_MODEM_MESSAGING (self),
                                              (GAsyncReadyCallback)iface_modem_messaging_disable_ready,
                                              task);
            return;
        }
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_IFACE_LOCATION:
        if (self->priv->modem_location_dbus_skeleton) {
            mm_obj_dbg (self, "modem has location capabilities, disabling the Location interface...");
            mm_iface_modem_location_disable (MM_IFACE_MODEM_LOCATION (self),
                                             (GAsyncReadyCallback)iface_modem_location_disable_ready,
                                             task);
            return;
        }
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_IFACE_CDMA:
        if (self->priv->modem_cdma_dbus_skeleton) {
            mm_obj_dbg (self, "modem has CDMA capabilities, disabling the Modem CDMA interface...");
            mm_iface_modem_cdma_disable (MM_IFACE_MODEM_CDMA (self),
                                        (GAsyncReadyCallback)iface_modem_cdma_disable_ready,
                                        task);
            return;
        }
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_IFACE_3GPP_USSD:
        if (self->priv->modem_3gpp_ussd_dbus_skeleton) {
            mm_obj_dbg (self, "modem has 3GPP/USSD capabilities, disabling the Modem 3GPP/USSD interface...");
            mm_iface_modem_3gpp_ussd_disable (MM_IFACE_MODEM_3GPP_USSD (self),
                                              (GAsyncReadyCallback)iface_modem_3gpp_ussd_disable_ready,
                                              task);
            return;
        }
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_IFACE_3GPP_PROFILE_MANAGER:
        if (self->priv->modem_3gpp_profile_manager_dbus_skeleton) {
            mm_obj_dbg (self, "modem has 3GPP profile management capabilities, disabling the Modem 3GPP Profile Manager interface...");
            mm_iface_modem_3gpp_profile_manager_disable (MM_IFACE_MODEM_3GPP_PROFILE_MANAGER (self),
                                                         (GAsyncReadyCallback)iface_modem_3gpp_profile_manager_disable_ready,
                                                         task);
            return;
        }
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_IFACE_3GPP:
        if (self->priv->modem_3gpp_dbus_skeleton) {
            mm_obj_dbg (self, "modem has 3GPP capabilities, disabling the Modem 3GPP interface...");
            mm_iface_modem_3gpp_disable (MM_IFACE_MODEM_3GPP (self),
                                        (GAsyncReadyCallback)iface_modem_3gpp_disable_ready,
                                        task);
            return;
        }
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_IFACE_MODEM:
        /* This skeleton may be NULL when mm_base_modem_disable() gets called at
         * the same time as modem object disposal. */
        if (self->priv->modem_dbus_skeleton) {
            mm_obj_dbg (self, "disabling the Modem interface...");
            mm_iface_modem_disable (MM_IFACE_MODEM (self),
                                    (GAsyncReadyCallback)iface_modem_disable_ready,
                                    task);
            return;
        }
        ctx->step++;
        /* fall through */

    case DISABLING_STEP_LAST:
        /* All disabled without errors! */
        disabling_complete (task);
        return;

    default:
        break;
    }

    g_assert_not_reached ();
}

static void
common_disable (MMBroadbandModem    *self,
                gboolean             state_updates,
                DisablingStep        first_step,
                GCancellable        *cancellable,
                GAsyncReadyCallback  callback,
                gpointer             user_data)
{
    DisablingContext *ctx;
    GTask            *task;

    ctx = g_slice_new0 (DisablingContext);
    ctx->state_updates = state_updates;
    ctx->step = first_step;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_task_data (task, ctx, (GDestroyNotify)disabling_context_free);

    disabling_step (task);
}

/* Implicit disabling after failed enable */

static gboolean
enable_failed_finish (MMBroadbandModem  *self,
                      GAsyncResult      *res,
                      GError           **error)
{
    /* The implicit disabling should never ever fail */
    common_disable_finish (self, res, NULL);
    return TRUE;
}

static void
enable_failed (MMBroadbandModem    *self,
               GAsyncReadyCallback  callback,
               gpointer             user_data)
{
    common_disable (self,
                    FALSE, /* don't perform state updates */
                    DISABLING_STEP_FIRST_AFTER_ENABLE_FAILED,
                    NULL, /* no cancellable */
                    callback,
                    user_data);
}

/* User-requested disable operation */

static gboolean
disable_finish (MMBaseModem  *self,
                GAsyncResult  *res,
                GError       **error)
{
    return common_disable_finish (MM_BROADBAND_MODEM (self), res, error);
}

static void
disable (MMBaseModem         *self,
         GCancellable        *cancellable,
         GAsyncReadyCallback  callback,
         gpointer             user_data)
{
    common_disable (MM_BROADBAND_MODEM (self),
                    TRUE, /* perform state updates */
                    DISABLING_STEP_FIRST,
                    cancellable,
                    callback,
                    user_data);
}

/*****************************************************************************/

typedef enum {
    ENABLING_STEP_FIRST,
    ENABLING_STEP_WAIT_FOR_FINAL_STATE,
    ENABLING_STEP_STARTED,
    ENABLING_STEP_IFACE_MODEM,
    ENABLING_STEP_IFACE_3GPP,
    ENABLING_STEP_IFACE_3GPP_PROFILE_MANAGER,
    ENABLING_STEP_IFACE_3GPP_USSD,
    ENABLING_STEP_IFACE_CDMA,
    ENABLING_STEP_IFACE_LOCATION,
    ENABLING_STEP_IFACE_MESSAGING,
    ENABLING_STEP_IFACE_TIME,
    ENABLING_STEP_IFACE_CELL_BROADCAST,
    ENABLING_STEP_IFACE_SIGNAL,
    ENABLING_STEP_IFACE_OMA,
    ENABLING_STEP_IFACE_VOICE,
    ENABLING_STEP_IFACE_FIRMWARE,
    ENABLING_STEP_IFACE_SIMPLE,
    ENABLING_STEP_LAST,
} EnablingStep;

typedef struct {
    EnablingStep  step;
    MMModemState  previous_state;
    GError       *saved_error;
} EnablingContext;

static void
enabling_context_free (EnablingContext *ctx)
{
    g_assert (!ctx->saved_error);
    g_slice_free (EnablingContext, ctx);
}

static gboolean
enable_finish (MMBaseModem *self,
               GAsyncResult *res,
               GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
enabling_complete (GTask *task)
{
    MMBroadbandModem *self;
    EnablingContext  *ctx;

    self = g_task_get_source_object (task);
    ctx  = g_task_get_task_data (task);

    /* Enable failed? */
    if (ctx->saved_error) {
        if (ctx->previous_state != MM_MODEM_STATE_ENABLED) {
            /* Fallback to previous state */
            mm_iface_modem_update_state (MM_IFACE_MODEM (self),
                                         ctx->previous_state,
                                         MM_MODEM_STATE_CHANGE_REASON_UNKNOWN);
        }
        g_task_return_error (task, g_steal_pointer (&ctx->saved_error));
        g_object_unref (task);
        return;
    }

    /* Enable succeeded */
    mm_iface_modem_update_state (MM_IFACE_MODEM (self),
                                 MM_MODEM_STATE_ENABLED,
                                 MM_MODEM_STATE_CHANGE_REASON_USER_REQUESTED);
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
enable_failed_ready (MMBroadbandModem *self,
                     GAsyncResult     *res,
                     GTask            *task)
{
    EnablingContext *ctx;

    /* The disabling run after a failed enable will never fail */
    enable_failed_finish (self, res, NULL);

    ctx = g_task_get_task_data (task);
    g_assert (ctx->saved_error);
    enabling_complete (task);
}

static void enabling_step (GTask *task);

#undef INTERFACE_ENABLE_READY_FN
#define INTERFACE_ENABLE_READY_FN(NAME,TYPE,FATAL_ERRORS)                               \
    static void                                                                         \
    NAME##_enable_ready (MMBroadbandModem *self,                                        \
                         GAsyncResult     *result,                                      \
                         GTask            *task)                                        \
    {                                                                                   \
        EnablingContext   *ctx;                                                         \
        g_autoptr(GError)  error = NULL;                                                \
                                                                                        \
        ctx = g_task_get_task_data (task);                                              \
                                                                                        \
        if (!mm_##NAME##_enable_finish (TYPE (self), result, &error)) {                 \
            if (FATAL_ERRORS) {                                                         \
                mm_obj_warn (self, "couldn't enable interface: '%s'", error->message);  \
                g_assert (!ctx->saved_error);                                           \
                ctx->saved_error = g_steal_pointer (&error);                            \
                mm_obj_dbg (self, "running implicit disable after failed enable...");   \
                enable_failed (self, (GAsyncReadyCallback) enable_failed_ready, task);  \
                return;                                                                 \
            }                                                                           \
                                                                                        \
            mm_obj_dbg (self, "couldn't enable interface: '%s'", error->message);       \
        }                                                                               \
                                                                                        \
        /* Go on to next step */                                                        \
        ctx->step++;                                                                    \
        enabling_step (task);                                                           \
    }

INTERFACE_ENABLE_READY_FN (iface_modem,                      MM_IFACE_MODEM,                      TRUE)
INTERFACE_ENABLE_READY_FN (iface_modem_3gpp,                 MM_IFACE_MODEM_3GPP,                 TRUE)
INTERFACE_ENABLE_READY_FN (iface_modem_3gpp_profile_manager, MM_IFACE_MODEM_3GPP_PROFILE_MANAGER, FALSE)
INTERFACE_ENABLE_READY_FN (iface_modem_3gpp_ussd,            MM_IFACE_MODEM_3GPP_USSD,            FALSE)
INTERFACE_ENABLE_READY_FN (iface_modem_cdma,                 MM_IFACE_MODEM_CDMA,                 TRUE)
INTERFACE_ENABLE_READY_FN (iface_modem_cell_broadcast,       MM_IFACE_MODEM_CELL_BROADCAST,       FALSE)
INTERFACE_ENABLE_READY_FN (iface_modem_location,             MM_IFACE_MODEM_LOCATION,             FALSE)
INTERFACE_ENABLE_READY_FN (iface_modem_messaging,            MM_IFACE_MODEM_MESSAGING,            FALSE)
INTERFACE_ENABLE_READY_FN (iface_modem_voice,                MM_IFACE_MODEM_VOICE,                FALSE)
INTERFACE_ENABLE_READY_FN (iface_modem_signal,               MM_IFACE_MODEM_SIGNAL,               FALSE)
INTERFACE_ENABLE_READY_FN (iface_modem_time,                 MM_IFACE_MODEM_TIME,                 FALSE)
INTERFACE_ENABLE_READY_FN (iface_modem_oma,                  MM_IFACE_MODEM_OMA,                  FALSE)

static void
enabling_started_ready (MMBroadbandModem *self,
                        GAsyncResult     *result,
                        GTask            *task)
{
    EnablingContext *ctx;

    ctx = g_task_get_task_data (task);
    g_assert (!ctx->saved_error);
    if (!MM_BROADBAND_MODEM_GET_CLASS (self)->enabling_started_finish (self, result, &ctx->saved_error)) {
        enabling_complete (task);
        return;
    }

    /* Go on to next step */
    ctx->step++;
    enabling_step (task);
}

static void
enabling_wait_for_final_state_ready (MMIfaceModem *self,
                                     GAsyncResult *res,
                                     GTask *task)
{
    EnablingContext *ctx;

    ctx = g_task_get_task_data (task);
    g_assert (!ctx->saved_error);
    ctx->previous_state = mm_iface_modem_wait_for_final_state_finish (self, res, &ctx->saved_error);
    if (ctx->saved_error) {
        enabling_complete (task);
        return;
    }

    if (ctx->previous_state >= MM_MODEM_STATE_ENABLED) {
        /* Just return success, don't relaunch enabling */
        enabling_complete (task);
        return;
    }

    /* We're in a final state now, go on */

    mm_iface_modem_update_state (MM_IFACE_MODEM (self),
                                 MM_MODEM_STATE_ENABLING,
                                 MM_MODEM_STATE_CHANGE_REASON_USER_REQUESTED);

    ctx->step++;
    enabling_step (task);
}

static void
enabling_step (GTask *task)
{
    MMBroadbandModem *self;
    EnablingContext  *ctx;

    self = g_task_get_source_object (task);
    ctx  = g_task_get_task_data (task);
    g_assert (!ctx->saved_error);

    /* Don't run new steps if we're cancelled */
    if (g_cancellable_set_error_if_cancelled (g_task_get_cancellable (task), &ctx->saved_error)) {
        enabling_complete (task);
        return;
    }

    switch (ctx->step) {
    case ENABLING_STEP_FIRST:
        ctx->step++;
        /* fall through */

    case ENABLING_STEP_WAIT_FOR_FINAL_STATE:
        mm_iface_modem_wait_for_final_state (MM_IFACE_MODEM (self),
                                             MM_MODEM_STATE_UNKNOWN, /* just any */
                                             (GAsyncReadyCallback)enabling_wait_for_final_state_ready,
                                             task);
        return;

    case ENABLING_STEP_STARTED:
        if (MM_BROADBAND_MODEM_GET_CLASS (self)->enabling_started &&
            MM_BROADBAND_MODEM_GET_CLASS (self)->enabling_started_finish) {
            MM_BROADBAND_MODEM_GET_CLASS (self)->enabling_started (self,
                                                                   (GAsyncReadyCallback)enabling_started_ready,
                                                                   task);
            return;
        }
        ctx->step++;
        /* fall through */

    case ENABLING_STEP_IFACE_MODEM:
        /* From now on, the failure to enable one of the mandatory interfaces
         * will trigger the implicit disabling process */

        /* Enabling the Modem interface */
        mm_iface_modem_enable (MM_IFACE_MODEM (self),
                               g_task_get_cancellable (task),
                               (GAsyncReadyCallback)iface_modem_enable_ready,
                               task);
        return;

    case ENABLING_STEP_IFACE_3GPP:
        if (self->priv->modem_3gpp_dbus_skeleton) {
            mm_obj_dbg (self, "modem has 3GPP capabilities, enabling the Modem 3GPP interface...");
            /* Enabling the Modem 3GPP interface */
            mm_iface_modem_3gpp_enable (MM_IFACE_MODEM_3GPP (self),
                                        g_task_get_cancellable (task),
                                        (GAsyncReadyCallback)iface_modem_3gpp_enable_ready,
                                        task);
            return;
        }
        ctx->step++;
        /* fall through */

    case ENABLING_STEP_IFACE_3GPP_PROFILE_MANAGER:
        if (self->priv->modem_3gpp_profile_manager_dbus_skeleton) {
            mm_obj_dbg (self, "modem has 3GPP profile management capabilities, enabling the Modem 3GPP Profile Manager interface...");
            mm_iface_modem_3gpp_profile_manager_enable (MM_IFACE_MODEM_3GPP_PROFILE_MANAGER (self),
                                                        (GAsyncReadyCallback)iface_modem_3gpp_profile_manager_enable_ready,
                                                        task);
            return;
        }
        ctx->step++;
        /* fall through */

    case ENABLING_STEP_IFACE_3GPP_USSD:
        if (self->priv->modem_3gpp_ussd_dbus_skeleton) {
            mm_obj_dbg (self, "modem has 3GPP/USSD capabilities, enabling the Modem 3GPP/USSD interface...");
            mm_iface_modem_3gpp_ussd_enable (MM_IFACE_MODEM_3GPP_USSD (self),
                                             (GAsyncReadyCallback)iface_modem_3gpp_ussd_enable_ready,
                                             task);
            return;
        }
        ctx->step++;
        /* fall through */

    case ENABLING_STEP_IFACE_CDMA:
        if (self->priv->modem_cdma_dbus_skeleton) {
            mm_obj_dbg (self, "modem has CDMA capabilities, enabling the Modem CDMA interface...");
            /* Enabling the Modem CDMA interface */
            mm_iface_modem_cdma_enable (MM_IFACE_MODEM_CDMA (self),
                                        g_task_get_cancellable (task),
                                        (GAsyncReadyCallback)iface_modem_cdma_enable_ready,
                                        task);
            return;
        }
        ctx->step++;
        /* fall through */

    case ENABLING_STEP_IFACE_LOCATION:
        if (self->priv->modem_location_dbus_skeleton) {
            mm_obj_dbg (self, "modem has location capabilities, enabling the Location interface...");
            /* Enabling the Modem Location interface */
            mm_iface_modem_location_enable (MM_IFACE_MODEM_LOCATION (self),
                                            g_task_get_cancellable (task),
                                            (GAsyncReadyCallback)iface_modem_location_enable_ready,
                                            task);
            return;
        }
        ctx->step++;
        /* fall through */

    case ENABLING_STEP_IFACE_MESSAGING:
        if (self->priv->modem_messaging_dbus_skeleton) {
            mm_obj_dbg (self, "modem has messaging capabilities, enabling the Messaging interface...");
            /* Enabling the Modem Messaging interface */
            mm_iface_modem_messaging_enable (MM_IFACE_MODEM_MESSAGING (self),
                                             g_task_get_cancellable (task),
                                             (GAsyncReadyCallback)iface_modem_messaging_enable_ready,
                                             task);
            return;
        }
        ctx->step++;
        /* fall through */

    case ENABLING_STEP_IFACE_TIME:
        if (self->priv->modem_time_dbus_skeleton) {
            mm_obj_dbg (self, "modem has time capabilities, enabling the Time interface...");
            /* Enabling the Modem Time interface */
            mm_iface_modem_time_enable (MM_IFACE_MODEM_TIME (self),
                                        g_task_get_cancellable (task),
                                        (GAsyncReadyCallback)iface_modem_time_enable_ready,
                                        task);
            return;
        }
        ctx->step++;
       /* fall through */

    case ENABLING_STEP_IFACE_CELL_BROADCAST:
        if (self->priv->modem_cell_broadcast_dbus_skeleton) {
            mm_obj_dbg (self, "modem has cell broadcast capabilities, enabling the cell broadcast interface...");
            /* Enabling the Modem CellBroadcast interface */
            mm_iface_modem_cell_broadcast_enable (MM_IFACE_MODEM_CELL_BROADCAST (self),
                                                  g_task_get_cancellable (task),
                                                  (GAsyncReadyCallback)iface_modem_cell_broadcast_enable_ready,
                                                  task);
            return;
        }
        ctx->step++;
       /* fall through */

    case ENABLING_STEP_IFACE_SIGNAL:
        if (self->priv->modem_signal_dbus_skeleton) {
            mm_obj_dbg (self, "modem has extended signal reporting capabilities, enabling the Signal interface...");
            /* Enabling the Modem Signal interface */
            mm_iface_modem_signal_enable (MM_IFACE_MODEM_SIGNAL (self),
                                          g_task_get_cancellable (task),
                                          (GAsyncReadyCallback)iface_modem_signal_enable_ready,
                                          task);
            return;
        }
        ctx->step++;
       /* fall through */

    case ENABLING_STEP_IFACE_OMA:
        if (self->priv->modem_oma_dbus_skeleton) {
            mm_obj_dbg (self, "modem has OMA capabilities, enabling the OMA interface...");
            /* Enabling the Modem Oma interface */
            mm_iface_modem_oma_enable (MM_IFACE_MODEM_OMA (self),
                                       g_task_get_cancellable (task),
                                       (GAsyncReadyCallback)iface_modem_oma_enable_ready,
                                       task);
            return;
        }
        ctx->step++;
       /* fall through */

    case ENABLING_STEP_IFACE_VOICE:
        if (self->priv->modem_voice_dbus_skeleton) {
            mm_obj_dbg (self, "modem has voice capabilities, enabling the Voice interface...");
            /* Enabling the Modem Voice interface */
            mm_iface_modem_voice_enable (MM_IFACE_MODEM_VOICE (self),
                                         g_task_get_cancellable (task),
                                         (GAsyncReadyCallback)iface_modem_voice_enable_ready,
                                         task);
            return;
        }
        ctx->step++;
       /* fall through */

    case ENABLING_STEP_IFACE_FIRMWARE:
        ctx->step++;
       /* fall through */

    case ENABLING_STEP_IFACE_SIMPLE:
        ctx->step++;
       /* fall through */

    case ENABLING_STEP_LAST:
        /* Once all interfaces have been enabled, trigger registration checks in
         * 3GPP and CDMA modems. We have to do this at this point so that e.g.
         * location interface gets proper registration related info reported.
         *
         * We do this in an idle so that we don't mess up the logs at this point
         * with the new requests being triggered.
         */
        schedule_initial_registration_checks (self);

        /* All enabled without errors! */
        enabling_complete (task);
        return;

    default:
        break;
    }

    g_assert_not_reached ();
}

static void
enabling_start (GTask *task)
{
    EnablingContext *ctx;

    ctx = g_slice_new0 (EnablingContext);
    ctx->step = ENABLING_STEP_FIRST;
    g_task_set_task_data (task, ctx, (GDestroyNotify)enabling_context_free);

    enabling_step (task);
}

static void
enable (MMBaseModem         *self,
        GCancellable        *cancellable,
        GAsyncReadyCallback  callback,
        gpointer             user_data)
{
    GTask *task;

    task = g_task_new (self, cancellable, callback, user_data);

    /* Check state before launching modem enabling */
    switch (MM_BROADBAND_MODEM (self)->priv->modem_state) {
    case MM_MODEM_STATE_UNKNOWN:
        /* We may have a UNKNOWN->ENABLED transition here if the request
         * comes after having flagged the modem as invalid. Just error out
         * gracefully. */
        g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_WRONG_STATE,
                                 "Cannot enable modem: unknown state");
        g_object_unref (task);
        return;

    case MM_MODEM_STATE_FAILED:
        g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_WRONG_STATE,
                                 "Cannot enable modem: initialization failed");
        g_object_unref (task);
        return;

    case MM_MODEM_STATE_LOCKED:
        g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_WRONG_STATE,
                                 "Cannot enable modem: device locked");
        g_object_unref (task);
        return;

    case MM_MODEM_STATE_INITIALIZING:
    case MM_MODEM_STATE_DISABLED:
    case MM_MODEM_STATE_DISABLING:
        enabling_start (task);
        return;

    case MM_MODEM_STATE_ENABLING:
        g_assert_not_reached ();
        return;

    case MM_MODEM_STATE_ENABLED:
    case MM_MODEM_STATE_SEARCHING:
    case MM_MODEM_STATE_REGISTERED:
    case MM_MODEM_STATE_DISCONNECTING:
    case MM_MODEM_STATE_CONNECTING:
    case MM_MODEM_STATE_CONNECTED:
        /* Just return success, don't relaunch enabling */
        g_task_return_boolean (task, TRUE);
        g_object_unref (task);
        return;

    default:
        g_assert_not_reached ();
    }
}
/*****************************************************************************/

#if defined WITH_SUSPEND_RESUME

typedef enum {
    SYNCING_STEP_FIRST,
    SYNCING_STEP_NOTIFY,
    SYNCING_STEP_IFACE_MODEM,
    SYNCING_STEP_IFACE_3GPP,
    SYNCING_STEP_IFACE_TIME,
    SYNCING_STEP_LAST,
} SyncingStep;

typedef struct {
    SyncingStep step;
} SyncingContext;

static void syncing_step (GTask *task);

static gboolean
synchronize_finish (MMBaseModem   *self,
                    GAsyncResult  *res,
                    GError       **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
iface_modem_time_sync_ready (MMIfaceModemTime *self,
                             GAsyncResult     *res,
                             GTask            *task)
{
    SyncingContext    *ctx;
    g_autoptr(GError)  error = NULL;

    ctx = g_task_get_task_data (task);

    if (!mm_iface_modem_time_sync_finish (self, res, &error))
        mm_obj_warn (self, "time interface synchronization failed: %s", error->message);

    /* Go on to next step */
    ctx->step++;
    syncing_step (task);
}

static void
iface_modem_3gpp_sync_ready (MMIfaceModem3gpp *self,
                             GAsyncResult     *res,
                             GTask            *task)
{
    SyncingContext    *ctx;
    g_autoptr(GError) error = NULL;

    ctx = g_task_get_task_data (task);

    if (!mm_iface_modem_3gpp_sync_finish (self, res, &error))
        mm_obj_warn (self, "3GPP interface synchronization failed: %s", error->message);

    /* Go on to next step */
    ctx->step++;
    syncing_step (task);
}

static void
iface_modem_sync_ready (MMIfaceModem *self,
                        GAsyncResult *res,
                        GTask        *task)
{
    SyncingContext    *ctx;
    MMModemLock        lock;
    g_autoptr(GError)  error = NULL;

    ctx = g_task_get_task_data (task);

    if (!mm_iface_modem_sync_finish (self, res, &error))
        mm_obj_warn (self, "modem interface synchronization failed: %s", error->message);

    /* The synchronization logic only runs on modems that were enabled before
     * the suspend/resume cycle, and therefore we should not get SIM-PIN locked
     * at this point, unless the SIM was swapped. */
    lock = mm_iface_modem_get_unlock_required (self);
    if (lock == MM_MODEM_LOCK_UNKNOWN || lock == MM_MODEM_LOCK_SIM_PIN || lock == MM_MODEM_LOCK_SIM_PUK) {
        /* Abort the sync() operation right away, and report a new SIM event that will
         * disable the modem and trigger a full reprobe */
        mm_obj_warn (self, "SIM is locked... synchronization aborted");
        g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_ABORTED,
                                 "Locked SIM found during modem interface synchronization");
        g_object_unref (task);
        return;
    }

    /* Not locked, go on to next step */
    mm_obj_dbg (self, "modem unlocked, continue synchronization");
    ctx->step++;
    syncing_step (task);
    return;
}

static void
syncing_step (GTask *task)
{
    MMBroadbandModem *self;
    SyncingContext   *ctx;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    switch (ctx->step) {
    case SYNCING_STEP_FIRST:
        ctx->step++;
        /* fall through */

    case SYNCING_STEP_NOTIFY:
        g_signal_emit (self, signals[SIGNAL_SYNC_NEEDED], 0);
        ctx->step++;
        /* fall through */

    case SYNCING_STEP_IFACE_MODEM:
        /*
         * Start interface Modem synchronization.
         * We want to make sure that the SIM is unlocked and not swapped before
         * synchronizing other interfaces.
         */
        if (!self->priv->modem_dbus_skeleton) {
            g_task_return_new_error (task, MM_CORE_ERROR, MM_CORE_ERROR_ABORTED,
                                     "Synchronization aborted: no modem exposed in DBus");
            g_object_unref (task);
            return;
        }
        mm_obj_msg (self, "resume synchronization state (%d/%d): modem interface sync",
                    ctx->step, SYNCING_STEP_LAST);
        mm_iface_modem_sync (MM_IFACE_MODEM (self),
                             (GAsyncReadyCallback)iface_modem_sync_ready,
                             task);
        return;

    case SYNCING_STEP_IFACE_3GPP:
        /* Start 3GPP interface synchronization, only if modem was enabled.
         * We hardly depend on the registration and bearer status,
         * therefore we cannot continue with the other steps until
         * this one is finished.
         */
        if (self->priv->modem_3gpp_dbus_skeleton &&
            (self->priv->modem_state >= MM_MODEM_STATE_ENABLED)) {
            mm_obj_msg (self, "resume synchronization state (%d/%d): 3GPP interface sync",
                        ctx->step, SYNCING_STEP_LAST);
            mm_iface_modem_3gpp_sync (MM_IFACE_MODEM_3GPP (self), (GAsyncReadyCallback)iface_modem_3gpp_sync_ready, task);
            return;
        }
        ctx->step++;
        /* fall through */

    case SYNCING_STEP_IFACE_TIME:
        /* Start Time interface synchronization, only if modem was enabled */
        if (self->priv->modem_time_dbus_skeleton &&
            (self->priv->modem_state >= MM_MODEM_STATE_ENABLED)) {
            mm_obj_msg (self, "resume synchronization state (%d/%d): time interface sync",
                        ctx->step, SYNCING_STEP_LAST);
            mm_iface_modem_time_sync (MM_IFACE_MODEM_TIME (self), (GAsyncReadyCallback)iface_modem_time_sync_ready, task);
            return;
        }
        ctx->step++;
        /* fall through */

    case SYNCING_STEP_LAST:
        mm_obj_msg (self, "resume synchronization state (%d/%d): all done",
                    ctx->step, SYNCING_STEP_LAST);
        /* We are done without errors! */
        g_task_return_boolean (task, TRUE);
        g_object_unref (task);
        return;

    default:
        break;
    }

    g_assert_not_reached ();
}

/* 'sync' as function name conflicts with a declared function in unistd.h */
static void
synchronize (MMBaseModem         *self,
             GCancellable        *cancellable,
             GAsyncReadyCallback  callback,
             gpointer             user_data)
{
    SyncingContext *ctx;
    GTask          *task;

    task = g_task_new (self, cancellable, callback, user_data);

    /* Create SyncingContext */
    ctx = g_new0 (SyncingContext, 1);
    ctx->step = SYNCING_STEP_FIRST;
    g_task_set_task_data (task, ctx, (GDestroyNotify)g_free);

    syncing_step (task);
}

#endif

/*****************************************************************************/

typedef enum {
    INITIALIZE_STEP_FIRST,
    INITIALIZE_STEP_SETUP_PORTS,
    INITIALIZE_STEP_STARTED,
    INITIALIZE_STEP_SETUP_SIMPLE_STATUS,
    INITIALIZE_STEP_IFACE_MODEM,
    INITIALIZE_STEP_IFACE_3GPP,
    INITIALIZE_STEP_JUMP_TO_LIMITED,
    INITIALIZE_STEP_IFACE_3GPP_PROFILE_MANAGER,
    INITIALIZE_STEP_IFACE_3GPP_USSD,
    INITIALIZE_STEP_IFACE_CDMA,
    INITIALIZE_STEP_IFACE_MESSAGING,
    INITIALIZE_STEP_IFACE_TIME,
    INITIALIZE_STEP_IFACE_SIGNAL,
    INITIALIZE_STEP_IFACE_OMA,
    INITIALIZE_STEP_IFACE_SAR,
    INITIALIZE_STEP_IFACE_CELL_BROADCAST,
    INITIALIZE_STEP_FALLBACK_LIMITED,
    INITIALIZE_STEP_IFACE_LOCATION,
    INITIALIZE_STEP_IFACE_VOICE,
    INITIALIZE_STEP_IFACE_FIRMWARE,
    INITIALIZE_STEP_IFACE_SIMPLE,
    INITIALIZE_STEP_LAST,
} InitializeStep;

typedef struct {
    MMBroadbandModem *self;
    InitializeStep step;
    gpointer ports_ctx;
} InitializeContext;

static void initialize_step (GTask *task);

static void
initialize_context_free (InitializeContext *ctx)
{
    GError *error = NULL;

    if (ctx->ports_ctx &&
        MM_BROADBAND_MODEM_GET_CLASS (ctx->self)->initialization_stopped &&
        !MM_BROADBAND_MODEM_GET_CLASS (ctx->self)->initialization_stopped (ctx->self, ctx->ports_ctx, &error)) {
        mm_obj_warn (ctx->self, "error when stopping the initialization sequence: %s", error->message);
        g_error_free (error);
    }

    g_object_unref (ctx->self);
    g_free (ctx);
}

static gboolean
initialize_finish (MMBaseModem *self,
                   GAsyncResult *res,
                   GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
initialization_started_ready (MMBroadbandModem *self,
                              GAsyncResult *result,
                              GTask *task)
{
    InitializeContext *ctx;
    GError *error = NULL;
    gpointer ports_ctx;

    ctx = g_task_get_task_data (task);

    /* May return NULL without error */
    ports_ctx = MM_BROADBAND_MODEM_GET_CLASS (self)->initialization_started_finish (self, result, &error);
    if (error) {
        mm_obj_warn (self, "couldn't start initialization: %s", error->message);
        g_error_free (error);

        /* There is no Modem interface yet, so just update the variable directly */
        ctx->self->priv->modem_state = MM_MODEM_STATE_FAILED;

        /* Just jump to the last step */
        ctx->step = INITIALIZE_STEP_LAST;
        initialize_step (task);
        return;
    }

    /* Keep the ctx for later use when stopping initialization */
    ctx->ports_ctx = ports_ctx;

    /* Go on to next step */
    ctx->step++;
    initialize_step (task);
}

static void
iface_modem_initialize_ready (MMBroadbandModem *self,
                              GAsyncResult *result,
                              GTask *task)
{
    InitializeContext *ctx;
    GError *error = NULL;

    ctx = g_task_get_task_data (task);

    /* If the modem interface fails to get initialized, we will move the modem
     * to a FAILED state. Note that in this case we still export the interface. */
    if (!mm_iface_modem_initialize_finish (MM_IFACE_MODEM (self), result, &error)) {
        MMModemStateFailedReason failed_reason = MM_MODEM_STATE_FAILED_REASON_UNKNOWN;

        /* Report the new FAILED state */
        mm_obj_warn (self, "modem couldn't be initialized: %s", error->message);

        if (g_error_matches (error,
                             MM_MOBILE_EQUIPMENT_ERROR,
                             MM_MOBILE_EQUIPMENT_ERROR_SIM_NOT_INSERTED))
            failed_reason = MM_MODEM_STATE_FAILED_REASON_SIM_MISSING;
        else if (g_error_matches (error,
                                  MM_MOBILE_EQUIPMENT_ERROR,
                                  MM_MOBILE_EQUIPMENT_ERROR_SIM_FAILURE) ||
                 g_error_matches (error,
                                  MM_MOBILE_EQUIPMENT_ERROR,
                                  MM_MOBILE_EQUIPMENT_ERROR_SIM_WRONG))
            failed_reason = MM_MODEM_STATE_FAILED_REASON_SIM_MISSING;
        else if (g_error_matches (error,
                                  MM_CORE_ERROR,
                                  MM_CORE_ERROR_WRONG_SIM_STATE))
            failed_reason = MM_MODEM_STATE_FAILED_REASON_ESIM_WITHOUT_PROFILES;
        else if (g_error_matches (error,
                                  MM_CORE_ERROR,
                                  MM_CORE_ERROR_UNSUPPORTED))
            failed_reason = MM_MODEM_STATE_FAILED_REASON_UNKNOWN_CAPABILITIES;

        g_error_free (error);

        mm_iface_modem_update_failed_state (MM_IFACE_MODEM (self), failed_reason);
    } else {
        /* bind simple properties */
        mm_iface_modem_bind_simple_status (MM_IFACE_MODEM (self),
                                           self->priv->modem_simple_status);
    }

    /* Go on to next step */
    ctx->step++;
    initialize_step (task);
}

#undef INTERFACE_INIT_READY_FN
#define INTERFACE_INIT_READY_FN(NAME,TYPE,FATAL_ERRORS)                 \
    static void                                                         \
    NAME##_initialize_ready (MMBroadbandModem *self,                    \
                             GAsyncResult *result,                      \
                             GTask *task)                               \
    {                                                                   \
        InitializeContext *ctx;                                         \
        GError *error = NULL;                                           \
                                                                        \
        ctx = g_task_get_task_data (task);                              \
                                                                        \
        if (!mm_##NAME##_initialize_finish (TYPE (self), result, &error)) { \
            if (FATAL_ERRORS) {                                         \
                mm_obj_warn (self, "couldn't initialize interface: '%s'", \
                             error->message);                           \
                g_error_free (error);                                   \
                                                                        \
                /* Report the new FAILED state */                       \
                mm_iface_modem_update_failed_state (MM_IFACE_MODEM (self), \
                                                    MM_MODEM_STATE_FAILED_REASON_UNKNOWN); \
                                                                        \
                /* Just jump to the last step */                        \
                ctx->step = INITIALIZE_STEP_LAST;                       \
                initialize_step (task);                                 \
                return;                                                 \
            }                                                           \
                                                                        \
            mm_obj_dbg (self, "couldn't initialize interface: '%s'",    \
                        error->message);                                \
            /* Just shutdown this interface */                          \
            mm_##NAME##_shutdown (TYPE (self));                         \
            g_error_free (error);                                       \
        } else {                                                        \
            /* bind simple properties */                                \
            mm_##NAME##_bind_simple_status (TYPE (self), self->priv->modem_simple_status); \
        }                                                               \
                                                                        \
        /* Go on to next step */                                        \
        ctx->step++;                                                    \
        initialize_step (task);                                         \
    }

INTERFACE_INIT_READY_FN (iface_modem_3gpp,                 MM_IFACE_MODEM_3GPP,                 TRUE)
INTERFACE_INIT_READY_FN (iface_modem_3gpp_profile_manager, MM_IFACE_MODEM_3GPP_PROFILE_MANAGER, FALSE)
INTERFACE_INIT_READY_FN (iface_modem_3gpp_ussd,            MM_IFACE_MODEM_3GPP_USSD,            FALSE)
INTERFACE_INIT_READY_FN (iface_modem_cdma,                 MM_IFACE_MODEM_CDMA,                 TRUE)
INTERFACE_INIT_READY_FN (iface_modem_location,             MM_IFACE_MODEM_LOCATION,             FALSE)
INTERFACE_INIT_READY_FN (iface_modem_messaging,            MM_IFACE_MODEM_MESSAGING,            FALSE)
INTERFACE_INIT_READY_FN (iface_modem_voice,                MM_IFACE_MODEM_VOICE,                FALSE)
INTERFACE_INIT_READY_FN (iface_modem_time,                 MM_IFACE_MODEM_TIME,                 FALSE)
INTERFACE_INIT_READY_FN (iface_modem_signal,               MM_IFACE_MODEM_SIGNAL,               FALSE)
INTERFACE_INIT_READY_FN (iface_modem_oma,                  MM_IFACE_MODEM_OMA,                  FALSE)
INTERFACE_INIT_READY_FN (iface_modem_firmware,             MM_IFACE_MODEM_FIRMWARE,             FALSE)
INTERFACE_INIT_READY_FN (iface_modem_sar,                  MM_IFACE_MODEM_SAR,                  FALSE)
INTERFACE_INIT_READY_FN (iface_modem_cell_broadcast,       MM_IFACE_MODEM_CELL_BROADCAST,       FALSE)

static void
initialize_step (GTask *task)
{
    InitializeContext *ctx;

    /* Don't run new steps if we're cancelled */
    if (g_task_return_error_if_cancelled (task)) {
        g_object_unref (task);
        return;
    }

    ctx = g_task_get_task_data (task);

    switch (ctx->step) {
    case INITIALIZE_STEP_FIRST:
        ctx->step++;
       /* fall through */

    case INITIALIZE_STEP_SETUP_PORTS:
        if (MM_BROADBAND_MODEM_GET_CLASS (ctx->self)->setup_ports)
            MM_BROADBAND_MODEM_GET_CLASS (ctx->self)->setup_ports (ctx->self);
        ctx->step++;
       /* fall through */

    case INITIALIZE_STEP_STARTED:
        if (MM_BROADBAND_MODEM_GET_CLASS (ctx->self)->initialization_started &&
            MM_BROADBAND_MODEM_GET_CLASS (ctx->self)->initialization_started_finish) {
            MM_BROADBAND_MODEM_GET_CLASS (ctx->self)->initialization_started (ctx->self,
                                                                              (GAsyncReadyCallback)initialization_started_ready,
                                                                              task);
            return;
        }
        ctx->step++;
       /* fall through */

    case INITIALIZE_STEP_SETUP_SIMPLE_STATUS:
        /* Simple status must be created before any interface initialization,
         * so that interfaces add and bind the properties they want to export.
         */
        if (!ctx->self->priv->modem_simple_status)
            ctx->self->priv->modem_simple_status = mm_simple_status_new ();
        ctx->step++;
       /* fall through */

    case INITIALIZE_STEP_IFACE_MODEM:
        /* Initialize the Modem interface */
        mm_iface_modem_initialize (MM_IFACE_MODEM (ctx->self),
                                   g_task_get_cancellable (task),
                                   (GAsyncReadyCallback)iface_modem_initialize_ready,
                                   task);
        return;

    case INITIALIZE_STEP_IFACE_3GPP:
        if (mm_iface_modem_is_3gpp (MM_IFACE_MODEM (ctx->self))) {
            /* Initialize the 3GPP interface */
            mm_iface_modem_3gpp_initialize (MM_IFACE_MODEM_3GPP (ctx->self),
                                            g_task_get_cancellable (task),
                                            (GAsyncReadyCallback)iface_modem_3gpp_initialize_ready,
                                            task);
            return;
        }

        ctx->step++;
       /* fall through */

    case INITIALIZE_STEP_JUMP_TO_LIMITED:
        if (ctx->self->priv->modem_state == MM_MODEM_STATE_LOCKED ||
            ctx->self->priv->modem_state == MM_MODEM_STATE_FAILED) {
            /* Jump to the fallback step when locked or failed, we will allow some additional
             * interfaces even in locked or failed state. */
            ctx->step = INITIALIZE_STEP_FALLBACK_LIMITED;
            initialize_step (task);
            return;
        }
        ctx->step++;
       /* fall through */

    case INITIALIZE_STEP_IFACE_3GPP_PROFILE_MANAGER:
        if (mm_iface_modem_is_3gpp (MM_IFACE_MODEM (ctx->self))) {
            /* Initialize the 3GPP Profile Manager interface */
            mm_iface_modem_3gpp_profile_manager_initialize (MM_IFACE_MODEM_3GPP_PROFILE_MANAGER (ctx->self),
                                                            (GAsyncReadyCallback)iface_modem_3gpp_profile_manager_initialize_ready,
                                                            task);
            return;
        }
        ctx->step++;
       /* fall through */

    case INITIALIZE_STEP_IFACE_3GPP_USSD:
        if (mm_iface_modem_is_3gpp (MM_IFACE_MODEM (ctx->self))) {
            /* Initialize the 3GPP/USSD interface */
            mm_iface_modem_3gpp_ussd_initialize (MM_IFACE_MODEM_3GPP_USSD (ctx->self),
                                                 (GAsyncReadyCallback)iface_modem_3gpp_ussd_initialize_ready,
                                                 task);
            return;
        }
        ctx->step++;
       /* fall through */

    case INITIALIZE_STEP_IFACE_CDMA:
        if (mm_iface_modem_is_cdma (MM_IFACE_MODEM (ctx->self))) {
            /* Initialize the CDMA interface */
            mm_iface_modem_cdma_initialize (MM_IFACE_MODEM_CDMA (ctx->self),
                                            g_task_get_cancellable (task),
                                            (GAsyncReadyCallback)iface_modem_cdma_initialize_ready,
                                            task);
            return;
        }
        ctx->step++;
       /* fall through */

    case INITIALIZE_STEP_IFACE_MESSAGING:
        /* Initialize the Messaging interface */
        mm_iface_modem_messaging_initialize (MM_IFACE_MODEM_MESSAGING (ctx->self),
                                             g_task_get_cancellable (task),
                                             (GAsyncReadyCallback)iface_modem_messaging_initialize_ready,
                                             task);
        return;

    case INITIALIZE_STEP_IFACE_TIME:
        /* Initialize the Time interface */
        mm_iface_modem_time_initialize (MM_IFACE_MODEM_TIME (ctx->self),
                                        g_task_get_cancellable (task),
                                        (GAsyncReadyCallback)iface_modem_time_initialize_ready,
                                        task);
        return;

    case INITIALIZE_STEP_IFACE_SIGNAL:
        /* Initialize the Signal interface */
        mm_iface_modem_signal_initialize (MM_IFACE_MODEM_SIGNAL (ctx->self),
                                          g_task_get_cancellable (task),
                                          (GAsyncReadyCallback)iface_modem_signal_initialize_ready,
                                          task);
        return;

    case INITIALIZE_STEP_IFACE_OMA:
        /* Initialize the Oma interface */
        mm_iface_modem_oma_initialize (MM_IFACE_MODEM_OMA (ctx->self),
                                       g_task_get_cancellable (task),
                                       (GAsyncReadyCallback)iface_modem_oma_initialize_ready,
                                       task);
        return;

    case INITIALIZE_STEP_IFACE_SAR:
        /* Initialize the SAR interface */
        mm_iface_modem_sar_initialize (MM_IFACE_MODEM_SAR (ctx->self),
                                       g_task_get_cancellable (task),
                                       (GAsyncReadyCallback)iface_modem_sar_initialize_ready,
                                       task);
        return;

    case INITIALIZE_STEP_IFACE_CELL_BROADCAST:
        /* Initialize the CellBroadcast interface */
        mm_iface_modem_cell_broadcast_initialize (MM_IFACE_MODEM_CELL_BROADCAST (ctx->self),
                                                  g_task_get_cancellable (task),
                                                  (GAsyncReadyCallback)iface_modem_cell_broadcast_initialize_ready,
                                                  task);
        return;

    case INITIALIZE_STEP_FALLBACK_LIMITED:
        /* All the initialization steps after this one will be run both on
         * successful and locked/failed initializations. */
        ctx->step++;
       /* fall through */

    case INITIALIZE_STEP_IFACE_LOCATION:
        /* Initialize the Location interface */
        mm_iface_modem_location_initialize (MM_IFACE_MODEM_LOCATION (ctx->self),
                                            g_task_get_cancellable (task),
                                            (GAsyncReadyCallback)iface_modem_location_initialize_ready,
                                            task);
        return;

    case INITIALIZE_STEP_IFACE_VOICE:
        /* Initialize the Voice interface */
        mm_iface_modem_voice_initialize (MM_IFACE_MODEM_VOICE (ctx->self),
                                         g_task_get_cancellable (task),
                                         (GAsyncReadyCallback)iface_modem_voice_initialize_ready,
                                         task);
        return;

    case INITIALIZE_STEP_IFACE_FIRMWARE:
        /* Initialize the Firmware interface */
        mm_iface_modem_firmware_initialize (MM_IFACE_MODEM_FIRMWARE (ctx->self),
                                            g_task_get_cancellable (task),
                                            (GAsyncReadyCallback)iface_modem_firmware_initialize_ready,
                                            task);
        return;

    case INITIALIZE_STEP_IFACE_SIMPLE:
        if (ctx->self->priv->modem_state != MM_MODEM_STATE_FAILED)
            mm_iface_modem_simple_initialize (MM_IFACE_MODEM_SIMPLE (ctx->self));
        ctx->step++;
       /* fall through */

    case INITIALIZE_STEP_LAST:
        if (ctx->self->priv->modem_state == MM_MODEM_STATE_FAILED) {
            GError *error = NULL;

            if (!ctx->self->priv->modem_dbus_skeleton) {
                /* ABORTED here specifies an extremely fatal error that will make the modem
                 * not even exported in DBus */
                error = g_error_new (MM_CORE_ERROR, MM_CORE_ERROR_ABORTED,
                                     "Fatal error: modem is unusable");
            } else {
                /* Fatal SIM, firmware, or modem failure :-( */
                MMModemStateFailedReason reason;

                reason = mm_gdbus_modem_get_state_failed_reason (MM_GDBUS_MODEM (ctx->self->priv->modem_dbus_skeleton));
                error = g_error_new (MM_CORE_ERROR, MM_CORE_ERROR_WRONG_STATE,
                                     "Modem in failed state: %s",
                                     mm_modem_state_failed_reason_get_string (reason));

                /* Ensure we only leave the Modem, Voice, Location and Firmware interfaces
                 * around. A failure could be caused by firmware issues, which
                 * a firmware update, switch, or provisioning could fix. We also
                 * leave the Voice interface around so that we can attempt
                 * emergency voice calls, and the Location interface so that we can use
                 * GNSS without a SIM card
                 */
                mm_iface_modem_3gpp_profile_manager_shutdown (MM_IFACE_MODEM_3GPP_PROFILE_MANAGER (ctx->self));
                mm_iface_modem_3gpp_ussd_shutdown (MM_IFACE_MODEM_3GPP_USSD (ctx->self));
                mm_iface_modem_cdma_shutdown (MM_IFACE_MODEM_CDMA (ctx->self));
                mm_iface_modem_signal_shutdown (MM_IFACE_MODEM_SIGNAL (ctx->self));
                mm_iface_modem_messaging_shutdown (MM_IFACE_MODEM_MESSAGING (ctx->self));
                mm_iface_modem_time_shutdown (MM_IFACE_MODEM_TIME (ctx->self));
                mm_iface_modem_simple_shutdown (MM_IFACE_MODEM_SIMPLE (ctx->self));
            }

            g_task_return_error (task, error);
            g_object_unref (task);
            return;
        }

        if (ctx->self->priv->modem_state == MM_MODEM_STATE_LOCKED) {
            /* We're locked :-/ */
            g_task_return_new_error (task,
                                     MM_CORE_ERROR,
                                     MM_CORE_ERROR_WRONG_STATE,
                                     "Modem is currently locked, "
                                     "cannot fully initialize");
            g_object_unref (task);
            return;
        }

        /* All initialized without errors!
         * Set as disabled (a.k.a. initialized) */
        mm_iface_modem_update_state (MM_IFACE_MODEM (ctx->self),
                                     MM_MODEM_STATE_DISABLED,
                                     MM_MODEM_STATE_CHANGE_REASON_UNKNOWN);

        g_task_return_boolean (task, TRUE);
        g_object_unref (task);
        return;

    default:
        break;
    }

    g_assert_not_reached ();
}

static void
initialize (MMBaseModem *self,
            GCancellable *cancellable,
            GAsyncReadyCallback callback,
            gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, cancellable, callback, user_data);

    /* Check state before launching modem initialization */
    switch (MM_BROADBAND_MODEM (self)->priv->modem_state) {
    case MM_MODEM_STATE_FAILED:
        /* NOTE: this will only happen if we ever support hot-plugging SIMs */
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_WRONG_STATE,
                                 "Cannot initialize modem: "
                                 "device is unusable");
        break;

    case MM_MODEM_STATE_UNKNOWN:
    case MM_MODEM_STATE_LOCKED: {
        InitializeContext *ctx;

        ctx = g_new0 (InitializeContext, 1);
        ctx->self = MM_BROADBAND_MODEM (g_object_ref (self));
        ctx->step = INITIALIZE_STEP_FIRST;

        g_task_set_task_data (task, ctx, (GDestroyNotify)initialize_context_free);

        /* Set as being initialized, even if we were locked before */
        mm_iface_modem_update_state (MM_IFACE_MODEM (self),
                                     MM_MODEM_STATE_INITIALIZING,
                                     MM_MODEM_STATE_CHANGE_REASON_UNKNOWN);

        initialize_step (task);
        return;
    }

    case MM_MODEM_STATE_INITIALIZING:
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_IN_PROGRESS,
                                 "Cannot initialize modem: "
                                 "already being initialized");
        break;

    case MM_MODEM_STATE_DISABLED:
    case MM_MODEM_STATE_DISABLING:
    case MM_MODEM_STATE_ENABLING:
    case MM_MODEM_STATE_ENABLED:
    case MM_MODEM_STATE_SEARCHING:
    case MM_MODEM_STATE_REGISTERED:
    case MM_MODEM_STATE_DISCONNECTING:
    case MM_MODEM_STATE_CONNECTING:
    case MM_MODEM_STATE_CONNECTED:
        /* Just return success, don't relaunch initialization */
        g_task_return_boolean (task, TRUE);
        break;

    default:
        g_assert_not_reached ();
    }

    g_object_unref (task);
}

/*****************************************************************************/

MMModemCharset
mm_broadband_modem_get_current_charset (MMBroadbandModem *self)
{
    return self->priv->modem_current_charset;
}

/*****************************************************************************/

static void
bearer_count_multiplexed_connected (MMBaseBearer *bearer,
                                    guint        *count)
{
    /* The Multiplexed property is only set if connected, so it's enough to check
     * that one to see if we're connected and multiplexed */
    if (mm_gdbus_bearer_get_multiplexed (MM_GDBUS_BEARER (bearer)))
        *count += 1;
}

gboolean
mm_broadband_modem_get_active_multiplexed_bearers (MMBroadbandModem  *self,
                                                   guint             *out_current,
                                                   guint             *out_max,
                                                   GError           **error)
{
    g_autoptr(MMBearerList) list = NULL;
    guint                   max;
    guint                   count = 0;

    g_object_get (self,
                  MM_IFACE_MODEM_BEARER_LIST, &list,
                  NULL);

    if (!list) {
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED, "Bearer list unavailable");
        return FALSE;
    }

    max = mm_bearer_list_get_max_active_multiplexed (list);

    mm_bearer_list_foreach (list,
                            (MMBearerListForeachFunc)bearer_count_multiplexed_connected,
                            &count);
    g_assert (!(!max && count));

    if (out_max)
        *out_max = max;
    if (out_current)
        *out_current = count;

    return TRUE;
}

/*****************************************************************************/

gchar *
mm_broadband_modem_create_device_identifier (MMBroadbandModem  *self,
                                             const gchar       *ati,
                                             const gchar       *ati1,
                                             GError           **error)
{
    gchar *device_identifier;

    /* do nothing if device has gone already */
    if (!self->priv->modem_dbus_skeleton) {
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                     "Modem interface skeleton unavailable");
        return NULL;
    }

    device_identifier = mm_create_device_identifier (
                mm_base_modem_get_vendor_id (MM_BASE_MODEM (self)),
                mm_base_modem_get_product_id (MM_BASE_MODEM (self)),
                self,
                ati,
                ati1,
                mm_gdbus_modem_get_equipment_identifier (
                    MM_GDBUS_MODEM (self->priv->modem_dbus_skeleton)),
                mm_gdbus_modem_get_revision (
                    MM_GDBUS_MODEM (self->priv->modem_dbus_skeleton)),
                mm_gdbus_modem_get_model (
                    MM_GDBUS_MODEM (self->priv->modem_dbus_skeleton)),
                mm_gdbus_modem_get_manufacturer (
                    MM_GDBUS_MODEM (self->priv->modem_dbus_skeleton)));

    if (!device_identifier) {
        g_set_error (error, MM_CORE_ERROR, MM_CORE_ERROR_FAILED,
                     "Unable to generate a device identifier");
        return NULL;
    }

    return device_identifier;
}

/*****************************************************************************/

MMBroadbandModem *
mm_broadband_modem_new (const gchar *device,
                        const gchar *physdev,
                        const gchar **drivers,
                        const gchar *plugin,
                        guint16 vendor_id,
                        guint16 product_id)
{
    return g_object_new (MM_TYPE_BROADBAND_MODEM,
                         MM_BASE_MODEM_DEVICE, device,
                         MM_BASE_MODEM_PHYSDEV, physdev,
                         MM_BASE_MODEM_DRIVERS, drivers,
                         MM_BASE_MODEM_PLUGIN, plugin,
                         MM_BASE_MODEM_VENDOR_ID, vendor_id,
                         MM_BASE_MODEM_PRODUCT_ID, product_id,
                         /* Generic bearer supports TTY only */
                         MM_BASE_MODEM_DATA_NET_SUPPORTED, FALSE,
                         MM_BASE_MODEM_DATA_TTY_SUPPORTED, TRUE,
                         NULL);
}

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (object);

    switch (prop_id) {
    case PROP_MODEM_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_dbus_skeleton);
        self->priv->modem_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_3GPP_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_3gpp_dbus_skeleton);
        self->priv->modem_3gpp_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_3GPP_PROFILE_MANAGER_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_3gpp_profile_manager_dbus_skeleton);
        self->priv->modem_3gpp_profile_manager_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_3GPP_USSD_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_3gpp_ussd_dbus_skeleton);
        self->priv->modem_3gpp_ussd_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_CDMA_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_cdma_dbus_skeleton);
        self->priv->modem_cdma_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_CELL_BROADCAST_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_cell_broadcast_dbus_skeleton);
        self->priv->modem_cell_broadcast_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_SIMPLE_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_simple_dbus_skeleton);
        self->priv->modem_simple_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_LOCATION_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_location_dbus_skeleton);
        self->priv->modem_location_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_MESSAGING_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_messaging_dbus_skeleton);
        self->priv->modem_messaging_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_VOICE_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_voice_dbus_skeleton);
        self->priv->modem_voice_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_TIME_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_time_dbus_skeleton);
        self->priv->modem_time_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_SIGNAL_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_signal_dbus_skeleton);
        self->priv->modem_signal_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_OMA_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_oma_dbus_skeleton);
        self->priv->modem_oma_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_FIRMWARE_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_firmware_dbus_skeleton);
        self->priv->modem_firmware_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_SAR_DBUS_SKELETON:
        g_clear_object (&self->priv->modem_sar_dbus_skeleton);
        self->priv->modem_sar_dbus_skeleton = g_value_dup_object (value);
        break;
    case PROP_MODEM_SIM:
        g_clear_object (&self->priv->modem_sim);
        self->priv->modem_sim = g_value_dup_object (value);
        break;
    case PROP_MODEM_SIM_SLOTS:
        g_clear_pointer (&self->priv->modem_sim_slots, g_ptr_array_unref);
        self->priv->modem_sim_slots = g_value_dup_boxed (value);
        break;
    case PROP_MODEM_BEARER_LIST:
        g_clear_object (&self->priv->modem_bearer_list);
        self->priv->modem_bearer_list = g_value_dup_object (value);
        break;
    case PROP_MODEM_STATE:
        self->priv->modem_state = g_value_get_enum (value);
        break;
    case PROP_MODEM_3GPP_REGISTRATION_STATE:
        self->priv->modem_3gpp_registration_state = g_value_get_enum (value);
        break;
    case PROP_MODEM_3GPP_CS_NETWORK_SUPPORTED:
        self->priv->modem_3gpp_cs_network_supported = g_value_get_boolean (value);
        break;
    case PROP_MODEM_3GPP_PS_NETWORK_SUPPORTED:
        self->priv->modem_3gpp_ps_network_supported = g_value_get_boolean (value);
        break;
    case PROP_MODEM_3GPP_EPS_NETWORK_SUPPORTED:
        self->priv->modem_3gpp_eps_network_supported = g_value_get_boolean (value);
        break;
    case PROP_MODEM_3GPP_5GS_NETWORK_SUPPORTED:
        self->priv->modem_3gpp_5gs_network_supported = g_value_get_boolean (value);
        break;
    case PROP_MODEM_3GPP_IGNORED_FACILITY_LOCKS:
        self->priv->modem_3gpp_ignored_facility_locks = g_value_get_flags (value);
        break;
    case PROP_MODEM_3GPP_INITIAL_EPS_BEARER:
        g_clear_object (&self->priv->modem_3gpp_initial_eps_bearer);
        self->priv->modem_3gpp_initial_eps_bearer = g_value_dup_object (value);
        break;
    case PROP_MODEM_3GPP_PACKET_SERVICE_STATE:
        self->priv->modem_3gpp_packet_service_state = g_value_get_enum (value);
        break;
    case PROP_MODEM_CDMA_CDMA1X_REGISTRATION_STATE:
        self->priv->modem_cdma_cdma1x_registration_state = g_value_get_enum (value);
        break;
    case PROP_MODEM_CDMA_EVDO_REGISTRATION_STATE:
        self->priv->modem_cdma_evdo_registration_state = g_value_get_enum (value);
        break;
    case PROP_MODEM_CDMA_CDMA1X_NETWORK_SUPPORTED:
        self->priv->modem_cdma_cdma1x_network_supported = g_value_get_boolean (value);
        break;
    case PROP_MODEM_CDMA_EVDO_NETWORK_SUPPORTED:
        self->priv->modem_cdma_evdo_network_supported = g_value_get_boolean (value);
        break;
    case PROP_MODEM_MESSAGING_SMS_LIST:
        g_clear_object (&self->priv->modem_messaging_sms_list);
        self->priv->modem_messaging_sms_list = g_value_dup_object (value);
        break;
    case PROP_MODEM_MESSAGING_SMS_PDU_MODE:
        self->priv->modem_messaging_sms_pdu_mode = g_value_get_boolean (value);
        break;
    case PROP_MODEM_MESSAGING_SMS_DEFAULT_STORAGE:
        self->priv->modem_messaging_sms_default_storage = g_value_get_enum (value);
        break;
    case PROP_MODEM_CELL_BROADCAST_CBM_LIST:
        g_clear_object (&self->priv->modem_cell_broadcast_cbm_list);
        self->priv->modem_cell_broadcast_cbm_list = g_value_dup_object (value);
        break;
    case PROP_MODEM_LOCATION_ALLOW_GPS_UNMANAGED_ALWAYS:
        self->priv->modem_location_allow_gps_unmanaged_always = g_value_get_boolean (value);
        break;
    case PROP_MODEM_VOICE_CALL_LIST:
        g_clear_object (&self->priv->modem_voice_call_list);
        self->priv->modem_voice_call_list = g_value_dup_object (value);
        break;
    case PROP_MODEM_SIMPLE_STATUS:
        g_clear_object (&self->priv->modem_simple_status);
        self->priv->modem_simple_status = g_value_dup_object (value);
        break;
    case PROP_MODEM_SIM_HOT_SWAP_SUPPORTED:
        self->priv->sim_hot_swap_supported = g_value_get_boolean (value);
        break;
    case PROP_MODEM_PERIODIC_SIGNAL_CHECK_DISABLED:
        self->priv->periodic_signal_check_disabled = g_value_get_boolean (value);
        break;
    case PROP_MODEM_PERIODIC_ACCESS_TECH_CHECK_DISABLED:
        self->priv->periodic_access_tech_check_disabled = g_value_get_boolean (value);
        break;
    case PROP_MODEM_PERIODIC_CALL_LIST_CHECK_DISABLED:
        self->priv->periodic_call_list_check_disabled = g_value_get_boolean (value);
        break;
    case PROP_MODEM_INDICATION_CALL_LIST_RELOAD_ENABLED:
        self->priv->indication_call_list_reload_enabled = g_value_get_boolean (value);
        break;
    case PROP_MODEM_CARRIER_CONFIG_MAPPING:
        self->priv->carrier_config_mapping = g_value_dup_string (value);
        break;
    case PROP_MODEM_FIRMWARE_IGNORE_CARRIER:
        self->priv->modem_firmware_ignore_carrier = g_value_get_boolean (value);
        break;
    case PROP_FLOW_CONTROL:
        self->priv->flow_control = g_value_get_flags (value);
        break;
    case PROP_INDICATORS_DISABLED:
        self->priv->modem_cind_disabled = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
get_property (GObject *object,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (object);

    switch (prop_id) {
    case PROP_MODEM_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_dbus_skeleton);
        break;
    case PROP_MODEM_3GPP_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_3gpp_dbus_skeleton);
        break;
    case PROP_MODEM_3GPP_PROFILE_MANAGER_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_3gpp_profile_manager_dbus_skeleton);
        break;
    case PROP_MODEM_3GPP_USSD_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_3gpp_ussd_dbus_skeleton);
        break;
    case PROP_MODEM_CDMA_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_cdma_dbus_skeleton);
        break;
    case PROP_MODEM_CELL_BROADCAST_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_cell_broadcast_dbus_skeleton);
        break;
    case PROP_MODEM_SIMPLE_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_simple_dbus_skeleton);
        break;
    case PROP_MODEM_LOCATION_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_location_dbus_skeleton);
        break;
    case PROP_MODEM_MESSAGING_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_messaging_dbus_skeleton);
        break;
    case PROP_MODEM_VOICE_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_voice_dbus_skeleton);
        break;
    case PROP_MODEM_TIME_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_time_dbus_skeleton);
        break;
    case PROP_MODEM_SIGNAL_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_signal_dbus_skeleton);
        break;
    case PROP_MODEM_OMA_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_oma_dbus_skeleton);
        break;
    case PROP_MODEM_FIRMWARE_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_firmware_dbus_skeleton);
        break;
    case PROP_MODEM_SAR_DBUS_SKELETON:
        g_value_set_object (value, self->priv->modem_sar_dbus_skeleton);
        break;
    case PROP_MODEM_SIM:
        g_value_set_object (value, self->priv->modem_sim);
        break;
    case PROP_MODEM_SIM_SLOTS:
        g_value_set_boxed (value, self->priv->modem_sim_slots);
        break;
    case PROP_MODEM_BEARER_LIST:
        g_value_set_object (value, self->priv->modem_bearer_list);
        break;
    case PROP_MODEM_STATE:
        g_value_set_enum (value, self->priv->modem_state);
        break;
    case PROP_MODEM_3GPP_REGISTRATION_STATE:
        g_value_set_enum (value, self->priv->modem_3gpp_registration_state);
        break;
    case PROP_MODEM_3GPP_CS_NETWORK_SUPPORTED:
        g_value_set_boolean (value, self->priv->modem_3gpp_cs_network_supported);
        break;
    case PROP_MODEM_3GPP_PS_NETWORK_SUPPORTED:
        g_value_set_boolean (value, self->priv->modem_3gpp_ps_network_supported);
        break;
    case PROP_MODEM_3GPP_EPS_NETWORK_SUPPORTED:
        g_value_set_boolean (value, self->priv->modem_3gpp_eps_network_supported);
        break;
    case PROP_MODEM_3GPP_5GS_NETWORK_SUPPORTED:
        g_value_set_boolean (value, self->priv->modem_3gpp_5gs_network_supported);
        break;
    case PROP_MODEM_3GPP_IGNORED_FACILITY_LOCKS:
        g_value_set_flags (value, self->priv->modem_3gpp_ignored_facility_locks);
        break;
    case PROP_MODEM_3GPP_INITIAL_EPS_BEARER:
        g_value_set_object (value, self->priv->modem_3gpp_initial_eps_bearer);
        break;
    case PROP_MODEM_3GPP_PACKET_SERVICE_STATE:
        g_value_set_enum (value, self->priv->modem_3gpp_packet_service_state);
        break;
    case PROP_MODEM_CDMA_CDMA1X_REGISTRATION_STATE:
        g_value_set_enum (value, self->priv->modem_cdma_cdma1x_registration_state);
        break;
    case PROP_MODEM_CDMA_EVDO_REGISTRATION_STATE:
        g_value_set_enum (value, self->priv->modem_cdma_evdo_registration_state);
        break;
    case PROP_MODEM_CDMA_CDMA1X_NETWORK_SUPPORTED:
        g_value_set_boolean (value, self->priv->modem_cdma_cdma1x_network_supported);
        break;
    case PROP_MODEM_CDMA_EVDO_NETWORK_SUPPORTED:
        g_value_set_boolean (value, self->priv->modem_cdma_evdo_network_supported);
        break;
    case PROP_MODEM_MESSAGING_SMS_LIST:
        g_value_set_object (value, self->priv->modem_messaging_sms_list);
        break;
    case PROP_MODEM_MESSAGING_SMS_PDU_MODE:
        g_value_set_boolean (value, self->priv->modem_messaging_sms_pdu_mode);
        break;
    case PROP_MODEM_MESSAGING_SMS_DEFAULT_STORAGE:
        g_value_set_enum (value, self->priv->modem_messaging_sms_default_storage);
        break;
    case PROP_MODEM_CELL_BROADCAST_CBM_LIST:
        g_value_set_object (value, self->priv->modem_cell_broadcast_cbm_list);
        break;
    case PROP_MODEM_LOCATION_ALLOW_GPS_UNMANAGED_ALWAYS:
        g_value_set_boolean (value, self->priv->modem_location_allow_gps_unmanaged_always);
        break;
    case PROP_MODEM_VOICE_CALL_LIST:
        g_value_set_object (value, self->priv->modem_voice_call_list);
        break;
    case PROP_MODEM_SIMPLE_STATUS:
        g_value_set_object (value, self->priv->modem_simple_status);
        break;
    case PROP_MODEM_SIM_HOT_SWAP_SUPPORTED:
        g_value_set_boolean (value, self->priv->sim_hot_swap_supported);
        break;
    case PROP_MODEM_PERIODIC_SIGNAL_CHECK_DISABLED:
        g_value_set_boolean (value, self->priv->periodic_signal_check_disabled);
        break;
    case PROP_MODEM_PERIODIC_ACCESS_TECH_CHECK_DISABLED:
        g_value_set_boolean (value, self->priv->periodic_access_tech_check_disabled);
        break;
    case PROP_MODEM_PERIODIC_CALL_LIST_CHECK_DISABLED:
        g_value_set_boolean (value, self->priv->periodic_call_list_check_disabled);
        break;
    case PROP_MODEM_INDICATION_CALL_LIST_RELOAD_ENABLED:
        g_value_set_boolean (value, self->priv->indication_call_list_reload_enabled);
        break;
    case PROP_MODEM_CARRIER_CONFIG_MAPPING:
        g_value_set_string (value, self->priv->carrier_config_mapping);
        break;
    case PROP_MODEM_FIRMWARE_IGNORE_CARRIER:
        g_value_set_boolean (value, self->priv->modem_firmware_ignore_carrier);
        break;
    case PROP_FLOW_CONTROL:
        g_value_set_flags (value, self->priv->flow_control);
        break;
    case PROP_INDICATORS_DISABLED:
        g_value_set_boolean (value, self->priv->modem_cind_disabled);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
mm_broadband_modem_init (MMBroadbandModem *self)
{
    /* Initialize private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                              MM_TYPE_BROADBAND_MODEM,
                                              MMBroadbandModemPrivate);
    self->priv->modem_state = MM_MODEM_STATE_UNKNOWN;
    self->priv->modem_3gpp_registration_regex = mm_3gpp_creg_regex_get (TRUE);
    self->priv->modem_current_charset = MM_MODEM_CHARSET_UNKNOWN;
    self->priv->modem_3gpp_registration_state = MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN;
    self->priv->modem_3gpp_cs_network_supported = TRUE;
    self->priv->modem_3gpp_ps_network_supported = TRUE;
    self->priv->modem_3gpp_eps_network_supported = FALSE;
    self->priv->modem_3gpp_5gs_network_supported = FALSE;
    self->priv->modem_3gpp_ignored_facility_locks = MM_MODEM_3GPP_FACILITY_NONE;
    self->priv->modem_3gpp_packet_service_state = MM_MODEM_3GPP_PACKET_SERVICE_STATE_UNKNOWN;
    self->priv->modem_cdma_cdma1x_registration_state = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
    self->priv->modem_cdma_evdo_registration_state = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
    self->priv->modem_cdma_cdma1x_network_supported = TRUE;
    self->priv->modem_cdma_evdo_network_supported = TRUE;
    self->priv->modem_messaging_sms_default_storage = MM_SMS_STORAGE_UNKNOWN;
    self->priv->current_sms_mem1_storage = MM_SMS_STORAGE_UNKNOWN;
    self->priv->current_sms_mem2_storage = MM_SMS_STORAGE_UNKNOWN;
    self->priv->sim_hot_swap_supported = FALSE;
    self->priv->periodic_signal_check_disabled = FALSE;
    self->priv->periodic_access_tech_check_disabled = FALSE;
    self->priv->periodic_call_list_check_disabled = FALSE;
    self->priv->indication_call_list_reload_enabled = FALSE;
    self->priv->modem_cmer_enable_mode = MM_3GPP_CMER_MODE_NONE;
    self->priv->modem_cmer_disable_mode = MM_3GPP_CMER_MODE_NONE;
    self->priv->modem_cmer_ind = MM_3GPP_CMER_IND_NONE;
    self->priv->modem_cgerep_enable_mode = MM_3GPP_CGEREP_MODE_NONE;
    self->priv->modem_cgerep_disable_mode = MM_3GPP_CGEREP_MODE_NONE;
    self->priv->flow_control = MM_FLOW_CONTROL_NONE;
    self->priv->initial_eps_bearer_cid = -1;
}

static void
finalize (GObject *object)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (object);

    if (self->priv->enabled_ports_ctx)
        ports_context_unref (self->priv->enabled_ports_ctx);

    if (self->priv->sim_hot_swap_ports_ctx)
        ports_context_unref (self->priv->sim_hot_swap_ports_ctx);

    if (self->priv->in_call_ports_ctx)
        ports_context_unref (self->priv->in_call_ports_ctx);

    if (self->priv->modem_3gpp_registration_regex)
        mm_3gpp_creg_regex_destroy (self->priv->modem_3gpp_registration_regex);

    g_free (self->priv->carrier_config_mapping);

    G_OBJECT_CLASS (mm_broadband_modem_parent_class)->finalize (object);
}

static void
dispose (GObject *object)
{
    MMBroadbandModem *self = MM_BROADBAND_MODEM (object);

    if (self->priv->modem_dbus_skeleton) {
        mm_iface_modem_shutdown (MM_IFACE_MODEM (object));
        g_clear_object (&self->priv->modem_dbus_skeleton);
    }

    if (self->priv->modem_3gpp_dbus_skeleton) {
        mm_iface_modem_3gpp_shutdown (MM_IFACE_MODEM_3GPP (object));
        g_clear_object (&self->priv->modem_3gpp_dbus_skeleton);
    }

    if (self->priv->modem_3gpp_profile_manager_dbus_skeleton) {
        mm_iface_modem_3gpp_profile_manager_shutdown (MM_IFACE_MODEM_3GPP_PROFILE_MANAGER (object));
        g_clear_object (&self->priv->modem_3gpp_profile_manager_dbus_skeleton);
    }

    if (self->priv->modem_3gpp_ussd_dbus_skeleton) {
        mm_iface_modem_3gpp_ussd_shutdown (MM_IFACE_MODEM_3GPP_USSD (object));
        g_clear_object (&self->priv->modem_3gpp_ussd_dbus_skeleton);
    }

    if (self->priv->modem_cdma_dbus_skeleton) {
        mm_iface_modem_cdma_shutdown (MM_IFACE_MODEM_CDMA (object));
        g_clear_object (&self->priv->modem_cdma_dbus_skeleton);
    }

    if (self->priv->modem_cell_broadcast_dbus_skeleton) {
        mm_iface_modem_cell_broadcast_shutdown (MM_IFACE_MODEM_CELL_BROADCAST (object));
        g_clear_object (&self->priv->modem_cell_broadcast_dbus_skeleton);
    }

    if (self->priv->modem_location_dbus_skeleton) {
        mm_iface_modem_location_shutdown (MM_IFACE_MODEM_LOCATION (object));
        g_clear_object (&self->priv->modem_location_dbus_skeleton);
    }

    if (self->priv->modem_signal_dbus_skeleton) {
        mm_iface_modem_signal_shutdown (MM_IFACE_MODEM_SIGNAL (object));
        g_clear_object (&self->priv->modem_signal_dbus_skeleton);
    }

    if (self->priv->modem_messaging_dbus_skeleton) {
        mm_iface_modem_messaging_shutdown (MM_IFACE_MODEM_MESSAGING (object));
        g_clear_object (&self->priv->modem_messaging_dbus_skeleton);
    }

    if (self->priv->modem_voice_dbus_skeleton) {
        mm_iface_modem_voice_shutdown (MM_IFACE_MODEM_VOICE (object));
        g_clear_object (&self->priv->modem_voice_dbus_skeleton);
    }

    if (self->priv->modem_time_dbus_skeleton) {
        mm_iface_modem_time_shutdown (MM_IFACE_MODEM_TIME (object));
        g_clear_object (&self->priv->modem_time_dbus_skeleton);
    }

    if (self->priv->modem_simple_dbus_skeleton) {
        mm_iface_modem_simple_shutdown (MM_IFACE_MODEM_SIMPLE (object));
        g_clear_object (&self->priv->modem_simple_dbus_skeleton);
    }

    if (self->priv->modem_firmware_dbus_skeleton) {
        mm_iface_modem_firmware_shutdown (MM_IFACE_MODEM_FIRMWARE (object));
        g_clear_object (&self->priv->modem_firmware_dbus_skeleton);
    }

    if (self->priv->modem_sar_dbus_skeleton) {
        mm_iface_modem_sar_shutdown (MM_IFACE_MODEM_SAR (object));
        g_clear_object (&self->priv->modem_sar_dbus_skeleton);
    }

    g_clear_object (&self->priv->modem_3gpp_initial_eps_bearer);
    g_clear_object (&self->priv->modem_sim);
    g_clear_pointer (&self->priv->modem_sim_slots, g_ptr_array_unref);
    g_clear_object (&self->priv->modem_bearer_list);
    g_clear_object (&self->priv->modem_messaging_sms_list);
    g_clear_object (&self->priv->modem_voice_call_list);
    g_clear_object (&self->priv->modem_simple_status);
    g_clear_object (&self->priv->modem_cell_broadcast_cbm_list);

    if (self->priv->enabled_ports_ctx)
        ports_context_dispose (self->priv->enabled_ports_ctx);
    if (self->priv->sim_hot_swap_ports_ctx)
        ports_context_dispose (self->priv->sim_hot_swap_ports_ctx);
    if (self->priv->in_call_ports_ctx)
        ports_context_dispose (self->priv->in_call_ports_ctx);

    G_OBJECT_CLASS (mm_broadband_modem_parent_class)->dispose (object);
}

static void
iface_modem_init (MMIfaceModemInterface *iface)
{
    /* Initialization steps */
    iface->load_current_capabilities = modem_load_current_capabilities;
    iface->load_current_capabilities_finish = modem_load_current_capabilities_finish;
    iface->load_manufacturer = modem_load_manufacturer;
    iface->load_manufacturer_finish = modem_load_manufacturer_finish;
    iface->load_model = modem_load_model;
    iface->load_model_finish = modem_load_model_finish;
    iface->load_revision = modem_load_revision;
    iface->load_revision_finish = modem_load_revision_finish;
    iface->load_equipment_identifier = modem_load_equipment_identifier;
    iface->load_equipment_identifier_finish = modem_load_equipment_identifier_finish;
    iface->load_device_identifier = modem_load_device_identifier;
    iface->load_device_identifier_finish = modem_load_device_identifier_finish;
    iface->load_own_numbers = modem_load_own_numbers;
    iface->load_own_numbers_finish = modem_load_own_numbers_finish;
    iface->load_unlock_required = modem_load_unlock_required;
    iface->load_unlock_required_finish = modem_load_unlock_required_finish;
    iface->load_unlock_retries = load_unlock_retries;
    iface->load_unlock_retries_finish = load_unlock_retries_finish;
    iface->create_sim = modem_create_sim;
    iface->create_sim_finish = modem_create_sim_finish;
    iface->load_supported_modes = modem_load_supported_modes;
    iface->load_supported_modes_finish = modem_load_supported_modes_finish;
    iface->load_power_state = modem_load_power_state;
    iface->load_power_state_finish = modem_load_power_state_finish;
    iface->load_supported_ip_families = modem_load_supported_ip_families;
    iface->load_supported_ip_families_finish = modem_load_supported_ip_families_finish;
    iface->create_bearer_list = modem_create_bearer_list;

    /* Enabling steps */
    iface->modem_power_up = modem_power_up;
    iface->modem_power_up_finish = modem_power_up_finish;
    iface->check_for_sim_swap = modem_check_for_sim_swap;
    iface->check_for_sim_swap_finish = modem_check_for_sim_swap_finish;
    iface->setup_flow_control = modem_setup_flow_control;
    iface->setup_flow_control_finish = modem_setup_flow_control_finish;
    iface->load_supported_charsets = modem_load_supported_charsets;
    iface->load_supported_charsets_finish = modem_load_supported_charsets_finish;
    iface->setup_charset = modem_setup_charset;
    iface->setup_charset_finish = modem_setup_charset_finish;

    /* Additional actions */
    iface->load_signal_quality = modem_load_signal_quality;
    iface->load_signal_quality_finish = modem_load_signal_quality_finish;
    iface->create_bearer = modem_create_bearer;
    iface->create_bearer_finish = modem_create_bearer_finish;
    iface->command = modem_command;
    iface->command_finish = modem_command_finish;

    iface->load_access_technologies = modem_load_access_technologies;
    iface->load_access_technologies_finish = modem_load_access_technologies_finish;
}

static void
iface_modem_3gpp_init (MMIfaceModem3gppInterface *iface)
{
    /* Initialization steps */
    iface->load_imei = modem_3gpp_load_imei;
    iface->load_imei_finish = modem_3gpp_load_imei_finish;
    iface->load_enabled_facility_locks = modem_3gpp_load_enabled_facility_locks;
    iface->load_enabled_facility_locks_finish = modem_3gpp_load_enabled_facility_locks_finish;
    iface->load_eps_ue_mode_operation = modem_3gpp_load_eps_ue_mode_operation;
    iface->load_eps_ue_mode_operation_finish = modem_3gpp_load_eps_ue_mode_operation_finish;
    iface->load_initial_eps_bearer_settings = modem_3gpp_load_initial_eps_bearer_settings;
    iface->load_initial_eps_bearer_settings_finish = modem_3gpp_load_initial_eps_bearer_settings_finish;

    /* Enabling steps */
    iface->setup_unsolicited_events = modem_3gpp_setup_unsolicited_events;
    iface->setup_unsolicited_events_finish = modem_3gpp_setup_cleanup_unsolicited_events_finish;
    iface->enable_unsolicited_events = modem_3gpp_enable_unsolicited_events;
    iface->enable_unsolicited_events_finish = modem_3gpp_enable_disable_unsolicited_events_finish;
    iface->setup_unsolicited_registration_events = modem_3gpp_setup_unsolicited_registration_events;
    iface->setup_unsolicited_registration_events_finish = modem_3gpp_setup_unsolicited_registration_events_finish;
    iface->enable_unsolicited_registration_events = modem_3gpp_enable_unsolicited_registration_events;
    iface->enable_unsolicited_registration_events_finish = modem_3gpp_enable_disable_unsolicited_registration_events_finish;

    /* Disabling steps */
    iface->disable_unsolicited_events = modem_3gpp_disable_unsolicited_events;
    iface->disable_unsolicited_events_finish = modem_3gpp_enable_disable_unsolicited_events_finish;
    iface->cleanup_unsolicited_events = modem_3gpp_cleanup_unsolicited_events;
    iface->cleanup_unsolicited_events_finish = modem_3gpp_setup_cleanup_unsolicited_events_finish;
    iface->disable_unsolicited_registration_events = modem_3gpp_disable_unsolicited_registration_events;
    iface->disable_unsolicited_registration_events_finish = modem_3gpp_enable_disable_unsolicited_registration_events_finish;
    iface->cleanup_unsolicited_registration_events = modem_3gpp_cleanup_unsolicited_registration_events;
    iface->cleanup_unsolicited_registration_events_finish = modem_3gpp_cleanup_unsolicited_registration_events_finish;

    /* Additional actions */
    iface->load_operator_code = modem_3gpp_load_operator_code;
    iface->load_operator_code_finish = modem_3gpp_load_operator_code_finish;
    iface->load_operator_name = modem_3gpp_load_operator_name;
    iface->load_operator_name_finish = modem_3gpp_load_operator_name_finish;
    iface->load_initial_eps_bearer = modem_3gpp_load_initial_eps_bearer;
    iface->load_initial_eps_bearer_finish = modem_3gpp_load_initial_eps_bearer_finish;
    iface->run_registration_checks = modem_3gpp_run_registration_checks;
    iface->run_registration_checks_finish = modem_3gpp_run_registration_checks_finish;
    iface->register_in_network = modem_3gpp_register_in_network;
    iface->register_in_network_finish = modem_3gpp_register_in_network_finish;
    iface->scan_networks = modem_3gpp_scan_networks;
    iface->scan_networks_finish = modem_3gpp_scan_networks_finish;
    iface->set_eps_ue_mode_operation = modem_3gpp_set_eps_ue_mode_operation;
    iface->set_eps_ue_mode_operation_finish = modem_3gpp_set_eps_ue_mode_operation_finish;
    iface->create_initial_eps_bearer = modem_3gpp_create_initial_eps_bearer;
    iface->set_packet_service_state = modem_3gpp_set_packet_service_state;
    iface->set_packet_service_state_finish = modem_3gpp_set_packet_service_state_finish;
    iface->set_initial_eps_bearer_settings = modem_3gpp_set_initial_eps_bearer_settings;
    iface->set_initial_eps_bearer_settings_finish = modem_3gpp_set_initial_eps_bearer_settings_finish;
}

static void
iface_modem_3gpp_profile_manager_init (MMIfaceModem3gppProfileManagerInterface *iface)
{
    /* Initialization steps */
    iface->check_support = modem_3gpp_profile_manager_check_support;
    iface->check_support_finish = modem_3gpp_profile_manager_check_support_finish;

    /* User actions */
    iface->list_profiles = modem_3gpp_profile_manager_list_profiles;
    iface->list_profiles_finish = modem_3gpp_profile_manager_list_profiles_finish;
    iface->delete_profile = modem_3gpp_profile_manager_delete_profile;
    iface->delete_profile_finish = modem_3gpp_profile_manager_delete_profile_finish;
    iface->check_format = modem_3gpp_profile_manager_check_format;
    iface->check_format_finish = modem_3gpp_profile_manager_check_format_finish;
    iface->check_activated_profile = modem_3gpp_profile_manager_check_activated_profile;
    iface->check_activated_profile_finish = modem_3gpp_profile_manager_check_activated_profile_finish;
    iface->deactivate_profile = modem_3gpp_profile_manager_deactivate_profile;
    iface->deactivate_profile_finish = modem_3gpp_profile_manager_deactivate_profile_finish;
    iface->store_profile = modem_3gpp_profile_manager_store_profile;
    iface->store_profile_finish = modem_3gpp_profile_manager_store_profile_finish;
}

static void
iface_modem_3gpp_ussd_init (MMIfaceModem3gppUssdInterface *iface)
{
    /* Initialization steps */
    iface->check_support = modem_3gpp_ussd_check_support;
    iface->check_support_finish = modem_3gpp_ussd_check_support_finish;

    /* Enabling steps */
    iface->setup_unsolicited_events = modem_3gpp_ussd_setup_unsolicited_events;
    iface->setup_unsolicited_events_finish = modem_3gpp_ussd_setup_cleanup_unsolicited_events_finish;
    iface->enable_unsolicited_events = modem_3gpp_ussd_enable_unsolicited_events;
    iface->enable_unsolicited_events_finish = modem_3gpp_ussd_enable_disable_unsolicited_events_finish;

    /* Disabling steps */
    iface->cleanup_unsolicited_events_finish = modem_3gpp_ussd_setup_cleanup_unsolicited_events_finish;
    iface->cleanup_unsolicited_events = modem_3gpp_ussd_cleanup_unsolicited_events;
    iface->disable_unsolicited_events = modem_3gpp_ussd_disable_unsolicited_events;
    iface->disable_unsolicited_events_finish = modem_3gpp_ussd_enable_disable_unsolicited_events_finish;

    /* Additional actions */
    iface->encode = modem_3gpp_ussd_encode;
    iface->decode = modem_3gpp_ussd_decode;
    iface->send = modem_3gpp_ussd_send;
    iface->send_finish = modem_3gpp_ussd_send_finish;
    iface->cancel = modem_3gpp_ussd_cancel;
    iface->cancel_finish = modem_3gpp_ussd_cancel_finish;
}

static void
iface_modem_cdma_init (MMIfaceModemCdmaInterface *iface)
{
    /* Initialization steps */
    iface->load_esn = modem_cdma_load_esn;
    iface->load_esn_finish = modem_cdma_load_esn_finish;
    iface->load_meid = modem_cdma_load_meid;
    iface->load_meid_finish = modem_cdma_load_meid_finish;

    /* Registration check steps */
    iface->setup_unsolicited_events = modem_cdma_setup_unsolicited_events;
    iface->setup_unsolicited_events_finish = modem_cdma_setup_cleanup_unsolicited_events_finish;
    iface->cleanup_unsolicited_events = modem_cdma_cleanup_unsolicited_events;
    iface->cleanup_unsolicited_events_finish = modem_cdma_setup_cleanup_unsolicited_events_finish;
    iface->setup_registration_checks = modem_cdma_setup_registration_checks;
    iface->setup_registration_checks_finish = modem_cdma_setup_registration_checks_finish;
    iface->get_call_manager_state = modem_cdma_get_call_manager_state;
    iface->get_call_manager_state_finish = modem_cdma_get_call_manager_state_finish;
    iface->get_hdr_state = modem_cdma_get_hdr_state;
    iface->get_hdr_state_finish = modem_cdma_get_hdr_state_finish;
    iface->get_service_status = modem_cdma_get_service_status;
    iface->get_service_status_finish = modem_cdma_get_service_status_finish;
    iface->get_cdma1x_serving_system = modem_cdma_get_cdma1x_serving_system;
    iface->get_cdma1x_serving_system_finish = modem_cdma_get_cdma1x_serving_system_finish;
    iface->get_detailed_registration_state = modem_cdma_get_detailed_registration_state;
    iface->get_detailed_registration_state_finish = modem_cdma_get_detailed_registration_state_finish;

    /* Additional actions */
    iface->register_in_network = modem_cdma_register_in_network;
    iface->register_in_network_finish = modem_cdma_register_in_network_finish;
}

static void
iface_modem_cell_broadcast_init (MMIfaceModemCellBroadcastInterface *iface)
{
    iface->check_support = modem_cell_broadcast_check_support;
    iface->check_support_finish = modem_cell_broadcast_check_support_finish;
    iface->setup_unsolicited_events = modem_cell_broadcast_setup_unsolicited_events;
    iface->setup_unsolicited_events_finish = modem_cell_broadcast_setup_cleanup_unsolicited_events_finish;
    iface->load_channels = modem_cell_broadcast_load_channels;
    iface->load_channels_finish = modem_cell_broadcast_load_channels_finish;
    iface->cleanup_unsolicited_events = modem_cell_broadcast_cleanup_unsolicited_events;
    iface->cleanup_unsolicited_events_finish = modem_cell_broadcast_setup_cleanup_unsolicited_events_finish;
    iface->set_channels = modem_cell_broadcast_set_channels;
    iface->set_channels_finish = modem_cell_broadcast_set_channels_finish;
}

static void
iface_modem_simple_init (MMIfaceModemSimpleInterface *iface)
{
}

static void
iface_modem_location_init (MMIfaceModemLocationInterface *iface)
{
    iface->load_capabilities = modem_location_load_capabilities;
    iface->load_capabilities_finish = modem_location_load_capabilities_finish;
    iface->enable_location_gathering = enable_location_gathering;
    iface->enable_location_gathering_finish = enable_location_gathering_finish;
}

static void
iface_modem_messaging_init (MMIfaceModemMessagingInterface *iface)
{
    iface->check_support = modem_messaging_check_support;
    iface->check_support_finish = modem_messaging_check_support_finish;
    iface->load_supported_storages = modem_messaging_load_supported_storages;
    iface->load_supported_storages_finish = modem_messaging_load_supported_storages_finish;
    iface->set_default_storage = modem_messaging_set_default_storage;
    iface->set_default_storage_finish = modem_messaging_set_default_storage_finish;
    iface->setup_sms_format = modem_messaging_setup_sms_format;
    iface->setup_sms_format_finish = modem_messaging_setup_sms_format_finish;
    iface->load_initial_sms_parts = modem_messaging_load_initial_sms_parts;
    iface->load_initial_sms_parts_finish = modem_messaging_load_initial_sms_parts_finish;
    iface->setup_unsolicited_events = modem_messaging_setup_unsolicited_events;
    iface->setup_unsolicited_events_finish = modem_messaging_setup_cleanup_unsolicited_events_finish;
    iface->enable_unsolicited_events = modem_messaging_enable_unsolicited_events;
    iface->enable_unsolicited_events_finish = modem_messaging_enable_unsolicited_events_finish;
    iface->cleanup_unsolicited_events = modem_messaging_cleanup_unsolicited_events;
    iface->cleanup_unsolicited_events_finish = modem_messaging_setup_cleanup_unsolicited_events_finish;
    iface->init_current_storages = modem_messaging_init_current_storages;
    iface->init_current_storages_finish = modem_messaging_init_current_storages_finish;
    iface->lock_storages = modem_messaging_lock_storages;
    iface->lock_storages_finish = modem_messaging_lock_storages_finish;
    iface->unlock_storages = modem_messaging_unlock_storages;
}

static void
iface_modem_voice_init (MMIfaceModemVoiceInterface *iface)
{
    iface->check_support = modem_voice_check_support;
    iface->check_support_finish = modem_voice_check_support_finish;
    iface->setup_unsolicited_events = modem_voice_setup_unsolicited_events;
    iface->setup_unsolicited_events_finish = modem_voice_setup_cleanup_unsolicited_events_finish;
    iface->enable_unsolicited_events = modem_voice_enable_unsolicited_events;
    iface->enable_unsolicited_events_finish = modem_voice_enable_disable_unsolicited_events_finish;
    iface->disable_unsolicited_events = modem_voice_disable_unsolicited_events;
    iface->disable_unsolicited_events_finish = modem_voice_enable_disable_unsolicited_events_finish;
    iface->cleanup_unsolicited_events = modem_voice_cleanup_unsolicited_events;
    iface->cleanup_unsolicited_events_finish = modem_voice_setup_cleanup_unsolicited_events_finish;

    iface->setup_in_call_unsolicited_events = modem_voice_setup_in_call_unsolicited_events;
    iface->setup_in_call_unsolicited_events_finish = modem_voice_setup_cleanup_in_call_unsolicited_events_finish;
    iface->cleanup_in_call_unsolicited_events = modem_voice_cleanup_in_call_unsolicited_events;
    iface->cleanup_in_call_unsolicited_events_finish = modem_voice_setup_cleanup_in_call_unsolicited_events_finish;

    iface->create_call = modem_voice_create_call;
    iface->load_call_list = modem_voice_load_call_list;
    iface->load_call_list_finish = modem_voice_load_call_list_finish;
    iface->hold_and_accept = modem_voice_hold_and_accept;
    iface->hold_and_accept_finish = modem_voice_hold_and_accept_finish;
    iface->hangup_and_accept = modem_voice_hangup_and_accept;
    iface->hangup_and_accept_finish = modem_voice_hangup_and_accept_finish;
    iface->hangup_all = modem_voice_hangup_all;
    iface->hangup_all_finish = modem_voice_hangup_all_finish;
    iface->join_multiparty = modem_voice_join_multiparty;
    iface->join_multiparty_finish = modem_voice_join_multiparty_finish;
    iface->leave_multiparty = modem_voice_leave_multiparty;
    iface->leave_multiparty_finish = modem_voice_leave_multiparty_finish;
    iface->transfer = modem_voice_transfer;
    iface->transfer_finish = modem_voice_transfer_finish;
    iface->call_waiting_setup = modem_voice_call_waiting_setup;
    iface->call_waiting_setup_finish = modem_voice_call_waiting_setup_finish;
    iface->call_waiting_query = modem_voice_call_waiting_query;
    iface->call_waiting_query_finish = modem_voice_call_waiting_query_finish;
}

static void
iface_modem_time_init (MMIfaceModemTimeInterface *iface)
{
    iface->check_support = modem_time_check_support;
    iface->check_support_finish = modem_time_check_support_finish;
    iface->load_network_time = modem_time_load_network_time;
    iface->load_network_time_finish = modem_time_load_network_time_finish;
    iface->load_network_timezone = modem_time_load_network_timezone;
    iface->load_network_timezone_finish = modem_time_load_network_timezone_finish;
}

static void
iface_modem_signal_init (MMIfaceModemSignalInterface *iface)
{
    iface->check_support        = modem_signal_check_support;
    iface->check_support_finish = modem_signal_check_support_finish;
    iface->load_values          = modem_signal_load_values;
    iface->load_values_finish   = modem_signal_load_values_finish;
}

static void
iface_modem_oma_init (MMIfaceModemOmaInterface *iface)
{
}

static void
iface_modem_firmware_init (MMIfaceModemFirmwareInterface *iface)
{
    iface->load_update_settings = modem_firmware_load_update_settings;
    iface->load_update_settings_finish = modem_firmware_load_update_settings_finish;
}

static void
iface_modem_sar_init (MMIfaceModemSarInterface *iface)
{
}

static void
mm_broadband_modem_class_init (MMBroadbandModemClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    MMBaseModemClass *base_modem_class = MM_BASE_MODEM_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMBroadbandModemPrivate));

    /* Virtual methods */
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->dispose = dispose;
    object_class->finalize = finalize;

    base_modem_class->initialize = initialize;
    base_modem_class->initialize_finish = initialize_finish;
    base_modem_class->enable = enable;
    base_modem_class->enable_finish = enable_finish;
    base_modem_class->disable = disable;
    base_modem_class->disable_finish = disable_finish;

#if defined WITH_SUSPEND_RESUME
    base_modem_class->sync = synchronize;
    base_modem_class->sync_finish = synchronize_finish;
#endif

    klass->setup_ports = setup_ports;
    klass->initialization_started = initialization_started;
    klass->initialization_started_finish = initialization_started_finish;
    klass->initialization_stopped = initialization_stopped;
    klass->enabling_started = enabling_started;
    klass->enabling_started_finish = enabling_started_finish;
    klass->enabling_modem_init = enabling_modem_init;
    klass->enabling_modem_init_finish = enabling_modem_init_finish;
    klass->disabling_stopped = disabling_stopped;
    klass->load_initial_eps_bearer_cid = load_initial_eps_bearer_cid;
    klass->load_initial_eps_bearer_cid_finish = load_initial_eps_bearer_cid_finish;
    klass->create_sms = modem_messaging_create_sms;

    g_object_class_override_property (object_class,
                                      PROP_MODEM_DBUS_SKELETON,
                                      MM_IFACE_MODEM_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_3GPP_DBUS_SKELETON,
                                      MM_IFACE_MODEM_3GPP_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_3GPP_PROFILE_MANAGER_DBUS_SKELETON,
                                      MM_IFACE_MODEM_3GPP_PROFILE_MANAGER_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_3GPP_USSD_DBUS_SKELETON,
                                      MM_IFACE_MODEM_3GPP_USSD_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_CDMA_DBUS_SKELETON,
                                      MM_IFACE_MODEM_CDMA_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_CELL_BROADCAST_DBUS_SKELETON,
                                      MM_IFACE_MODEM_CELL_BROADCAST_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_SIMPLE_DBUS_SKELETON,
                                      MM_IFACE_MODEM_SIMPLE_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_LOCATION_DBUS_SKELETON,
                                      MM_IFACE_MODEM_LOCATION_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_MESSAGING_DBUS_SKELETON,
                                      MM_IFACE_MODEM_MESSAGING_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_VOICE_DBUS_SKELETON,
                                      MM_IFACE_MODEM_VOICE_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_TIME_DBUS_SKELETON,
                                      MM_IFACE_MODEM_TIME_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_SIGNAL_DBUS_SKELETON,
                                      MM_IFACE_MODEM_SIGNAL_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_OMA_DBUS_SKELETON,
                                      MM_IFACE_MODEM_OMA_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_SAR_DBUS_SKELETON,
                                      MM_IFACE_MODEM_SAR_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_FIRMWARE_DBUS_SKELETON,
                                      MM_IFACE_MODEM_FIRMWARE_DBUS_SKELETON);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_SIM,
                                      MM_IFACE_MODEM_SIM);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_SIM_SLOTS,
                                      MM_IFACE_MODEM_SIM_SLOTS);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_BEARER_LIST,
                                      MM_IFACE_MODEM_BEARER_LIST);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_STATE,
                                      MM_IFACE_MODEM_STATE);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_3GPP_REGISTRATION_STATE,
                                      MM_IFACE_MODEM_3GPP_REGISTRATION_STATE);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_3GPP_CS_NETWORK_SUPPORTED,
                                      MM_IFACE_MODEM_3GPP_CS_NETWORK_SUPPORTED);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_3GPP_PS_NETWORK_SUPPORTED,
                                      MM_IFACE_MODEM_3GPP_PS_NETWORK_SUPPORTED);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_3GPP_EPS_NETWORK_SUPPORTED,
                                      MM_IFACE_MODEM_3GPP_EPS_NETWORK_SUPPORTED);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_3GPP_5GS_NETWORK_SUPPORTED,
                                      MM_IFACE_MODEM_3GPP_5GS_NETWORK_SUPPORTED);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_3GPP_IGNORED_FACILITY_LOCKS,
                                      MM_IFACE_MODEM_3GPP_IGNORED_FACILITY_LOCKS);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_3GPP_INITIAL_EPS_BEARER,
                                      MM_IFACE_MODEM_3GPP_INITIAL_EPS_BEARER);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_3GPP_PACKET_SERVICE_STATE,
                                      MM_IFACE_MODEM_3GPP_PACKET_SERVICE_STATE);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_CDMA_CDMA1X_REGISTRATION_STATE,
                                      MM_IFACE_MODEM_CDMA_CDMA1X_REGISTRATION_STATE);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_CDMA_EVDO_REGISTRATION_STATE,
                                      MM_IFACE_MODEM_CDMA_EVDO_REGISTRATION_STATE);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_CDMA_CDMA1X_NETWORK_SUPPORTED,
                                      MM_IFACE_MODEM_CDMA_CDMA1X_NETWORK_SUPPORTED);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_CDMA_EVDO_NETWORK_SUPPORTED,
                                      MM_IFACE_MODEM_CDMA_EVDO_NETWORK_SUPPORTED);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_MESSAGING_SMS_LIST,
                                      MM_IFACE_MODEM_MESSAGING_SMS_LIST);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_MESSAGING_SMS_PDU_MODE,
                                      MM_IFACE_MODEM_MESSAGING_SMS_PDU_MODE);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_MESSAGING_SMS_DEFAULT_STORAGE,
                                      MM_IFACE_MODEM_MESSAGING_SMS_DEFAULT_STORAGE);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_CELL_BROADCAST_CBM_LIST,
                                      MM_IFACE_MODEM_CELL_BROADCAST_CBM_LIST);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_LOCATION_ALLOW_GPS_UNMANAGED_ALWAYS,
                                      MM_IFACE_MODEM_LOCATION_ALLOW_GPS_UNMANAGED_ALWAYS);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_VOICE_CALL_LIST,
                                      MM_IFACE_MODEM_VOICE_CALL_LIST);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_SIMPLE_STATUS,
                                      MM_IFACE_MODEM_SIMPLE_STATUS);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_SIM_HOT_SWAP_SUPPORTED,
                                      MM_IFACE_MODEM_SIM_HOT_SWAP_SUPPORTED);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_PERIODIC_SIGNAL_CHECK_DISABLED,
                                      MM_IFACE_MODEM_PERIODIC_SIGNAL_CHECK_DISABLED);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_PERIODIC_ACCESS_TECH_CHECK_DISABLED,
                                      MM_IFACE_MODEM_PERIODIC_ACCESS_TECH_CHECK_DISABLED);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_PERIODIC_CALL_LIST_CHECK_DISABLED,
                                      MM_IFACE_MODEM_VOICE_PERIODIC_CALL_LIST_CHECK_DISABLED);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_INDICATION_CALL_LIST_RELOAD_ENABLED,
                                      MM_IFACE_MODEM_VOICE_INDICATION_CALL_LIST_RELOAD_ENABLED);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_CARRIER_CONFIG_MAPPING,
                                      MM_IFACE_MODEM_CARRIER_CONFIG_MAPPING);

    g_object_class_override_property (object_class,
                                      PROP_MODEM_FIRMWARE_IGNORE_CARRIER,
                                      MM_IFACE_MODEM_FIRMWARE_IGNORE_CARRIER);

    properties[PROP_FLOW_CONTROL] =
        g_param_spec_flags (MM_BROADBAND_MODEM_FLOW_CONTROL,
                            "Flow control",
                            "Flow control settings to use in the connected TTY",
                            MM_TYPE_FLOW_CONTROL,
                            MM_FLOW_CONTROL_NONE,
                            G_PARAM_READWRITE);
    g_object_class_install_property (object_class, PROP_FLOW_CONTROL, properties[PROP_FLOW_CONTROL]);

    properties[PROP_INDICATORS_DISABLED] =
        g_param_spec_boolean (MM_BROADBAND_MODEM_INDICATORS_DISABLED,
                              "Disable indicators",
                              "Avoid explicitly setting up +CIND URCs",
                              FALSE,
                              G_PARAM_READWRITE);
    g_object_class_install_property (object_class, PROP_INDICATORS_DISABLED, properties[PROP_INDICATORS_DISABLED]);

#if defined WITH_SUSPEND_RESUME
    signals[SIGNAL_SYNC_NEEDED] =
        g_signal_new (MM_BROADBAND_MODEM_SIGNAL_SYNC_NEEDED,
                      G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MMBroadbandModemClass, sync_needed),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 0);
#endif
}
