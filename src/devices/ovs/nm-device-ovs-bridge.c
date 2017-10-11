/* NetworkManager -- Network link manager
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright 2017 Red Hat, Inc.
 */

#include "nm-default.h"

#include "nm-device-ovs-bridge.h"
#include "nm-device-ovs-port.h"
#include "nm-ovsdb.h"

#include "devices/nm-device-private.h"
#include "nm-active-connection.h"
#include "nm-setting-connection.h"
#include "nm-setting-ovs-bridge.h"

#include "introspection/org.freedesktop.NetworkManager.Device.OvsBridge.h"

#include "devices/nm-device-logging.h"
_LOG_DECLARE_SELF(NMDeviceOvsBridge);

/*****************************************************************************/

struct _NMDeviceOvsBridge {
	NMDevice parent;
};

struct _NMDeviceOvsBridgeClass {
	NMDeviceClass parent;
};

G_DEFINE_TYPE (NMDeviceOvsBridge, nm_device_ovs_bridge, NM_TYPE_DEVICE)

/*****************************************************************************/

static gboolean
create_and_realize (NMDevice *device,
                    NMConnection *connection,
                    NMDevice *parent,
                    const NMPlatformLink **out_plink,
                    GError **error)
{
        /* The actual backing resources will be created on enslavement by the port
         * when it can identify the port and the bridge. */

	return TRUE;
}

static gboolean
unrealize (NMDevice *device, GError **error)
{
	return TRUE;
}

static NMDeviceCapabilities
get_generic_capabilities (NMDevice *device)
{
	return NM_DEVICE_CAP_IS_SOFTWARE;
}

static gboolean
check_connection_compatible (NMDevice *device, NMConnection *connection)
{
	NMSettingConnection *s_con;
	const char *connection_type;

	if (!NM_DEVICE_CLASS (nm_device_ovs_bridge_parent_class)->check_connection_compatible (device, connection))
		return FALSE;

	s_con = nm_connection_get_setting_connection (connection);
	g_assert (s_con);
	connection_type = nm_setting_connection_get_connection_type (s_con);
	if (!connection_type)
		return FALSE;

	if (strcmp (connection_type, NM_SETTING_OVS_BRIDGE_SETTING_NAME) == 0)
		return TRUE;

	return FALSE;
}

static gboolean
check_slave_connection_compatible (NMDevice *device, NMConnection *slave)
{
	NMSettingConnection *s_con;
	const char *slave_type;

	s_con = nm_connection_get_setting_connection (slave);
	g_assert (s_con);
	slave_type = nm_setting_connection_get_slave_type (s_con);
	if (!slave_type)
		return FALSE;

	if (strcmp (slave_type, NM_SETTING_OVS_BRIDGE_SETTING_NAME) == 0)
		return TRUE;

	return FALSE;
}

static NMActStageReturn
act_stage3_ip4_config_start (NMDevice *device,
                             NMIP4Config **out_config,
                             NMDeviceStateReason *out_failure_reason)
{
	return NM_ACT_STAGE_RETURN_IP_FAIL;
}

static NMActStageReturn
act_stage3_ip6_config_start (NMDevice *device,
                             NMIP6Config **out_config,
                             NMDeviceStateReason *out_failure_reason)
{
	return NM_ACT_STAGE_RETURN_IP_FAIL;
}

static void
add_iface_cb (GError *error, gpointer user_data)
{
	NMDevice *slave = user_data;

	if (error) {
	        nm_log_warn (LOGD_DEVICE, "device %s could not be added to a ovs bridge: %s",
		             nm_device_get_iface (slave), error->message);
	        nm_device_state_changed (slave,
	                                 NM_DEVICE_STATE_FAILED,
	                                 NM_DEVICE_STATE_REASON_UNKNOWN);
	}

	g_object_unref (slave);
}

static gboolean
enslave_slave (NMDevice *device, NMDevice *slave, NMConnection *connection, gboolean configure)
{
	if (!configure)
		return TRUE;

	if (!NM_IS_DEVICE_OVS_PORT (slave)) {
		/* As a convenience measure, we allow enslaving the interfaces
		 * directly to the bridge without an intermediate ovs-port.
		 * In that case, create a port just for the slave. */
		nm_ovsdb_add_interface (nm_ovsdb_get (),
		                        nm_device_get_applied_connection (device),
		                        nm_device_get_applied_connection (slave),
		                        nm_device_get_applied_connection (slave),
		                        add_iface_cb, g_object_ref (device));
	}

	return TRUE;
}

static void
del_iface_cb (GError *error, gpointer user_data)
{
	NMDevice *slave = user_data;

	if (error) {
	        nm_log_warn (LOGD_DEVICE, "device %s could not be removed from a ovs bridge: %s",
		             nm_device_get_iface (slave), error->message);
	        nm_device_state_changed (slave,
	                                 NM_DEVICE_STATE_FAILED,
	                                 NM_DEVICE_STATE_REASON_UNKNOWN);
	}

	g_object_unref (slave);
}

static void
release_slave (NMDevice *device, NMDevice *slave, gboolean configure)
{
	if (!configure)
		return;

	if (NM_IS_DEVICE_OVS_PORT (slave))
		return;

	nm_ovsdb_del_interface (nm_ovsdb_get (), nm_device_get_iface (slave),
				del_iface_cb, g_object_ref (slave));
}

/*****************************************************************************/

static void
nm_device_ovs_bridge_init (NMDeviceOvsBridge *self)
{
}

static void
nm_device_ovs_bridge_class_init (NMDeviceOvsBridgeClass *klass)
{
	NMDeviceClass *device_class = NM_DEVICE_CLASS (klass);

	device_class->is_master = TRUE;
	device_class->create_and_realize = create_and_realize;
	device_class->unrealize = unrealize;
	device_class->get_generic_capabilities = get_generic_capabilities;
	device_class->check_connection_compatible = check_connection_compatible;
	device_class->check_slave_connection_compatible = check_slave_connection_compatible;
	device_class->act_stage3_ip4_config_start = act_stage3_ip4_config_start;
	device_class->act_stage3_ip6_config_start = act_stage3_ip6_config_start;
	device_class->enslave_slave = enslave_slave;
	device_class->release_slave = release_slave;

	nm_exported_object_class_add_interface (NM_EXPORTED_OBJECT_CLASS (klass),
	                                        NMDBUS_TYPE_DEVICE_OVS_BRIDGE_SKELETON,
	                                        NULL);
}