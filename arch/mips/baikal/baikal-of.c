/*
 * Baikal-T SOC platform support code.
 *
 * Copyright (C) 2014  Baikal Electronics OJSC
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/sys_soc.h>
#include <linux/slab.h>			/* kzalloc */
#include <linux/memblock.h>

#include <asm/fw/fw.h>
#include <asm/prom.h>

#include "common.h"

static char mips_revision[16] = "Unknown";
static char mips_soc_id[16]   = "Unknown";

__iomem void *plat_of_remap_node(const char *node)
{
	struct resource res;
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, node);
	if (!np)
		panic("Failed to find %s node", node);

	if (of_address_to_resource(np, 0, &res))
		panic("Failed to get resource for %s", node);

	if ((request_mem_region(res.start,
				resource_size(&res),
				res.name) < 0))
		panic("Failed to request resources for %s", node);

	return ioremap_nocache(res.start, resource_size(&res));
}

void __init device_tree_init(void)
{
	/* Set machine name */
	mips_set_machine_name(of_flat_dt_get_machine_name());

	/* Restore tree model and copy into kernel memory */
	unflatten_and_copy_device_tree();
}

static int __init __get_reserved_region(unsigned long node,
					     const char *uname)
{
	int t_len = (dt_root_addr_cells + dt_root_size_cells) * sizeof(__be32);
	phys_addr_t base, size;
	int len;
	const __be32 *prop;

	prop = of_get_flat_dt_prop(node, "reg", &len);
	if (!prop)
		return -ENOENT;

	if (len && len % t_len != 0) {
		pr_err("Reserved memory: invalid reg property in '%s', skipping node.\n",
		       uname);
		return -EINVAL;
	}

	while (len >= t_len) {
		base = dt_mem_next_cell(dt_root_addr_cells, &prop);
		size = dt_mem_next_cell(dt_root_size_cells, &prop);

		if (size) {
			add_memory_region(base, size, BOOT_MEM_RESERVED_PREF);
			pr_info("Reserved memory: reserved region for node '%s': base 0x%08x, size %ld MiB\n",
				uname, (uint32_t)base, (unsigned long)size / SZ_1M);
		} else {
			pr_info("Reserved memory: failed to reserve memory for node '%s': base %pa, size %ld MiB\n",
				uname, &base, (unsigned long)size / SZ_1M);
		}

		len -= t_len;
	}
	return 0;
}

static int __init __reserved_mem_check_root(unsigned long node)
{
	const __be32 *prop;

	prop = of_get_flat_dt_prop(node, "#size-cells", NULL);
	if (!prop || be32_to_cpup(prop) != dt_root_size_cells)
		return -EINVAL;

	prop = of_get_flat_dt_prop(node, "#address-cells", NULL);
	if (!prop || be32_to_cpup(prop) != dt_root_addr_cells)
		return -EINVAL;

	prop = of_get_flat_dt_prop(node, "ranges", NULL);
	if (!prop)
		return -EINVAL;
	return 0;
}

static int __init __find_reserved_mem(unsigned long node, const char *uname,
					  int depth, void *data)
{
	static int found;
	const char *status;
	int err;

	if (!found && depth == 1 && strcmp(uname, "reserved-memory") == 0) {
		if (__reserved_mem_check_root(node) != 0) {
			pr_err("Reserved memory: unsupported node format, ignoring\n");
			/* break scan */
			return 1;
		}
		found = 1;
		/* scan next node */
		return 0;
	} else if (!found) {
		/* scan next node */
		return 0;
	} else if (found && depth < 2) {
		/* scanning of /reserved-memory has been finished */
		return 1;
	}

	status = of_get_flat_dt_prop(node, "status", NULL);
	if (status && strcmp(status, "okay") != 0 && strcmp(status, "ok") != 0)
		return 0;

	err = __get_reserved_region(node, uname);

	/* scan next node */
	return 1;
}

int __init device_tree_early_init(void)
{
	/* Assume that device tree blob ptr in fw_arg3 */
	void *fdt = config_enabled(CONFIG_BUILTIN_DTB) ?
						__dtb_start : phys_to_virt(fw_arg3);

	if ((unsigned long)fdt < PAGE_OFFSET) {
		pr_err("Device tree blob address < PAGE_OFFSET\n");
		goto no_dtb;
	}

	if (!early_init_dt_scan(fdt))
		goto no_dtb;

	of_scan_flat_dt(__find_reserved_mem, NULL);

	/* Inform about initial device tree location */
	pr_info("Machine device tree at: 0x%p\n", fdt);

	/* Copy device tree command line to arcitecture command line */
	strlcpy(arcs_cmdline, boot_command_line, COMMAND_LINE_SIZE);
	return 0;

no_dtb:
		pr_warn("No valid device tree found, continuing without\n");
#ifndef CONFIG_CMDLINE_OVERRIDE
		/* Init command line from bootloader */
		fw_init_cmdline();
#endif
	return -1;
}

static int __init plat_of_setup(void)
{
	struct soc_device_attribute *soc_dev_attr;
	struct soc_device *soc_dev;
	struct device *parent = NULL;
	unsigned int cpuid = current_cpu_data.processor_id;

	if (unlikely(!of_have_populated_dt()))
		return 0;

	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		goto populate;
	/* SoC attributes */
	soc_dev_attr->machine	= mips_get_machine_name();
	soc_dev_attr->family	= get_system_type();
	soc_dev_attr->revision	= mips_revision;
	soc_dev_attr->soc_id	= mips_soc_id;
	/* Populate SoC-specific attributes */
	snprintf(mips_revision, 15, "%u.%u", (cpuid >> 5) & 0x07,
		cpuid & 0x07);
	snprintf(mips_soc_id, 15, "0x%08X",
		readl(phys_to_virt(BAIKAL_BOOT_CTRL_DRID)));
	/* Register SoC device */
	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev)) {
		kfree(soc_dev_attr);
		goto populate;
	}
	/* SoC platform device is parent for all */
	parent = soc_device_to_device(soc_dev);
populate:
	if (of_platform_populate(NULL, of_default_bus_match_table, NULL, parent))
		panic("Failed to populate device tree");

	return 0;
}
arch_initcall(plat_of_setup);
