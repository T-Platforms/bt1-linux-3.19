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

#ifndef __BAIKAL_COMMON_H
#define __BAIKAL_COMMON_H

#define CPU_FREQ		600000000
#define CPU_CLK_DIV 	2

extern __iomem void *plat_of_remap_node(const char *node);
extern int device_tree_early_init(void);

#endif /* __BAIKAL_COMMON_H */