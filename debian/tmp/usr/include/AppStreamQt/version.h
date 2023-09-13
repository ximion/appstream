/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2021-2022 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "appstreamqt_export.h"

/* compile time version
 */
#define ASQ_MAJOR_VERSION				1
#define ASQ_MINOR_VERSION				0
#define ASQ_MICRO_VERSION				0

/* check whether a AppStreamQt version equal to or greater than
 * major.minor.micro.
 */
#define ASQ_CHECK_VERSION(major,minor,micro)    \
    (ASQ_MAJOR_VERSION > (major) || \
     (ASQ_MAJOR_VERSION == (major) && ASQ_MINOR_VERSION > (minor)) || \
     (ASQ_MAJOR_VERSION == (major) && ASQ_MINOR_VERSION == (minor) && \
      ASQ_MICRO_VERSION >= (micro)))
