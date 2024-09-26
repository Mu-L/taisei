/*
 * This software is licensed under the terms of the MIT License.
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2024, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2024, Andrei Alexeyev <akari@taisei-project.org>.
 */

#pragma once
#include "taisei.h"

#include "public.h"

bool vfs_mount_resindex_layered(const char *mountpoint, const char *backend_vfspath)
	attr_nonnull(1, 2) attr_nodiscard;
