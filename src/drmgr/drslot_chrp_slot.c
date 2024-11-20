/**
 * @file drslot_chrp_slot.c
 *
 * Copyright (C) IBM Corporation 2006
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <librtas.h>

#include "dr.h"
#include "drpci.h"
#include "ofdt.h"

static char *usagestr = "-c slot {-a | -r | -Q} -s <drc_name | drc_index>";

void
slot_usage(char **pusage)
{
	*pusage = usagestr;
}

static int query_slot(struct dr_node *node)
{
	if (node == NULL)
		return RC_NONEXISTENT;

	if (! node->is_owned) {
		say(ERROR, "%s not owned by partition\n", usr_drc_name);
		return RC_DONT_OWN;
	}

	if (is_display_adapter(node)) {
		say(ERROR, "Display adapters adre not supported by drmgr.\n");
		return RC_IN_USE;
	}

	/* Special case for HMC */
	return RC_LINUX_SLOT;
}

/**
 * release_slot
 *
 */
static int
release_slot(struct dr_node *slot)
{
	int rc;

	rc = release_drc(slot->drc_index, slot->dev_type);
	if (rc)
		return rc;

	if (kernel_dlpar_exists())
		rc = do_dt_kernel_dlpar(slot->drc_index, REMOVE);
	else
		rc = remove_device_tree_nodes(slot->ofdt_path);
	if (rc) {
		acquire_drc(slot->drc_index);
		return rc;
	}

	return rc;
}
/**
 * remove_slot
 *
 */
static int
remove_slot(struct dr_node *node)
{
	int rc,rc2;

	if (is_display_adapter(node)) {
		say(ERROR, "DLPAR of display adapters is not supported.\n");
		return -1;
	}

	/* For PCI nodes */
	if (node->dev_type == PCI_DLPAR_DEV) {
		rc = disable_hp_children(node->drc_name);
		if (rc)
			say(ERROR, "failed to disable hotplug children\n");

		rc = release_hp_children(node->drc_name);
		if (rc && rc != -EINVAL) {
			say(ERROR, "failed to release hotplug children\n");
			return rc;
		}
	}

	say(DEBUG, "The sensor-state of drc_index 0x%x is %d\n",
	    node->drc_index, dr_entity_sense(node->drc_index));

	/* need to remove slot from sysfs which will
	 * "Hot unplug" the slot from pci world. unmap_bus_range will be
	 * done here also.
	 */
	rc = dlpar_remove_slot(node->drc_name);
	if (rc) {
		say(DEBUG, "remove %s from hotplug subsystem failed\n",
		    node->drc_name);
		say(ERROR, "Unknown failure. Data may be out of sync and \n"
			"the system may require a reboot.\n");
		return rc;
	}

	rc = release_slot(node);
	if (rc) {
		int num_acquired = 0;
		// try to restore to previous state
		rc2 = acquire_hp_children(node->ofdt_path, &num_acquired);
		if (rc2 && rc2 != -EINVAL) {
			say(ERROR, "Unknown failure %d. Data may be out of "
			    "sync and\nthe system may require a reboot.\n", rc2);
			return rc;
		}

		rc2 = dlpar_add_slot(node->drc_name);
		if (rc2) {
			say(ERROR, "Unknown failure %d. Data may be out of "
			    "sync and\nthe system may require a reboot.\n", rc2);
			return rc;
		}

		if (num_acquired) {
			rc2 = enable_hp_children(node->drc_name);
			if (rc2) {
				say(ERROR, "failed to re-enable hotplug "
				    "children. %d\n", rc2);
				return rc;
			}
		}
	}
	return rc;
}

/**
 * acquire_slot
 *
 */
static int
acquire_slot(char *drc_name, struct dr_node **slot)
{
	struct dr_connector drc;
	char path[DR_PATH_MAX];
	int rc;

	rc = get_drc_by_name(drc_name, &drc, path, OFDT_BASE);
	if (rc) {
		say(ERROR, "Could not find drc index for %s, unable to add the"
		    "slot.\n", drc_name);
		return rc;
	}

	if (!strncmp(drc.type, "display", 7)) {
		say(ERROR, "DLPAR of display adapters is not supported.\n");
		return -1;
	}

	rc = acquire_drc(drc.index);
	if (rc)
		return rc;

	if (kernel_dlpar_exists()) {
		rc = do_dt_kernel_dlpar(drc.index, ADD);
	} else {
		struct of_node *of_nodes;

		of_nodes = configure_connector(drc.index);
		if (of_nodes == NULL) {
			release_drc(drc.index, PCI_DLPAR_DEV);
			return -1;
		}

		rc = add_device_tree_nodes(path, of_nodes);
		free_of_node(of_nodes);
	}

	if (rc) {
		say(ERROR, "add_device_tree_nodes failed at %s\n", path);
		release_drc(drc.index, PCI_DLPAR_DEV);
		return -1;
	}

	/* Now that the node has been added to the device-tree, retrieve it.
	 * This also acts as a sanity check that everything up to this
	 * point has succeeded.
	 */
	*slot = get_node_by_name(drc_name, PCI_NODES | VIO_NODES);
	if (*slot == NULL) {
		say(ERROR, "Could not get find \"%s\"\n", drc_name);
		/* or should we call release_drc? but need device type */
		release_drc(drc.index, PHB_DEV);
		return -1;
	}

	return 0;
}

/**
 * add_slot
 *
 */
static int add_slot()
{
	struct dr_node *node = NULL;
	int rc, n_children = 0;
	
	rc = acquire_slot(usr_drc_name, &node);
	if (rc)
		return rc;
	
	/* For PCI nodes */
	if (node->dev_type == PCI_DLPAR_DEV) {
		rc = acquire_hp_children(node->ofdt_path, &n_children);
		if (rc) {
			if (release_slot(node)) {
				say(ERROR, "Unknown failure. Data may be out "
				    "of sync and\nthe system may require "
				    "a reboot.\n");
			}
			goto slot_add_exit;
		}
	}

	/* Need to add node into sysfs which will "Hot plug" the node into
	 * pci world.
	 */
	rc = dlpar_add_slot(node->drc_name);
	if (rc) {
		if (n_children) {
			if (release_hp_children(node->drc_name)) {
				say(ERROR, "Unknown failure. Data may be out "
				    "of sync and\nthe system may require "
				    "a reboot.\n");
			}
		}
		
		if (release_slot(node)) {
			say(ERROR, "Unknown failure. Data may be out of sync "
			    "and\nthe system may require a reboot.\n");
		}
	}

	if (n_children) {
		rc = enable_hp_children(node->drc_name);
		if (rc) {
			say(ERROR, "Configure adapter failed.\n");
			if (release_hp_children(node->drc_name)) {
				say(ERROR, "Unknown failure. Data may be out "
				    "of sync and\nthe system may require "
				    "a reboot.\n");
			}
			
			if (dlpar_remove_slot(node->drc_name)) {
				say(DEBUG, "remove %s from hotplug subsystem "
				    "failed\n", node->drc_name);
				say(ERROR, "Unknown failure. Data may be out "
				    "of sync and\nthe system may require "
				    "a reboot.\n");
			}

			if (release_slot(node)) {
				say(ERROR, "Unknown failure. Data may be out "
				    "of sync and\nthe system may require "
					"a reboot.\n");
			}
			goto slot_add_exit;
		}

		say(DEBUG, "adapter in node[%s] has been configured.\n",
		    node->drc_name);
	}

slot_add_exit:
	if (node)
		free_node(node);

	return rc;
}

int valid_slot_options(void)
{
	if (!usr_drc_name) {
		say(ERROR, "A drc name must be specified\n");
		return -1;
	}

        if ((usr_action != ADD) && (usr_action != REMOVE)
            && (usr_action != QUERY)) {
                say(ERROR, "The '-r', '-a', or '-Q' option must be specified "
                    "for slot operations.\n");
                return -1;
        }

	return 0;
}

int drslot_chrp_slot(void)
{
	struct dr_node *node;
	int rc;

	if (! slot_dlpar_capable()) {
		say(ERROR, "DLPAR slot operations are not supported on"
		    "this kernel.");
		return -1;
	}

	node = get_node_by_name(usr_drc_name, PCI_NODES | VIO_NODES);

	switch (usr_action) {
	    case ADD:
		if (node && node->is_owned) {
			say(ERROR, "partition already owns %s\n", usr_drc_name);
			rc = RC_ALREADY_OWN;
		} else {
			free_node(node);
			node = NULL;

			rc = add_slot();
		}
		break;

	    case REMOVE:
		if (node == NULL) {
			say(ERROR, "%s does not exist\n", usr_drc_name);
			rc = RC_NONEXISTENT;
		} else {	
			if (! node->is_owned) {
				say(ERROR, "%s not owned by partition\n",
				    usr_drc_name);
				rc = RC_DONT_OWN;
			} else
				rc = remove_slot(node);
		}
		break;

	    case QUERY:
		rc = query_slot(node);
		break;

	    default:
		rc = -1;
	}

	free_node(node);
	return rc;
}
