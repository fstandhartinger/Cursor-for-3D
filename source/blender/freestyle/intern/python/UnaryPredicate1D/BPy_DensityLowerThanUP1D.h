/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#pragma once

#include "../BPy_UnaryPredicate1D.h"

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject DensityLowerThanUP1D_Type;

#define BPy_DensityLowerThanUP1D_Check(v) \
  (PyObject_IsInstance((PyObject *)v, (PyObject *)&DensityLowerThanUP1D_Type))

/*---------------------------Python BPy_DensityLowerThanUP1D structure definition----------*/
typedef struct {
  BPy_UnaryPredicate1D py_up1D;
} BPy_DensityLowerThanUP1D;

///////////////////////////////////////////////////////////////////////////////////////////
