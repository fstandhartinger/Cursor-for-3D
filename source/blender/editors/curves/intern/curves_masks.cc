/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include "BLI_offset_indices.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"

#include "ED_curves.hh"

namespace blender::ed::curves {

IndexMask curve_mask_from_points(const bke::CurvesGeometry &curves,
                                 const IndexMask &point_mask,
                                 const GrainSize grain_size,
                                 IndexMaskMemory &memory)
{
  Array<bool> selected(curves.points_num());
  point_mask.to_bools(selected);

  const OffsetIndices points_by_curve = curves.points_by_curve();
  return IndexMask::from_predicate(
      curves.curves_range(), grain_size, memory, [&](const int curve_i) {
        const IndexRange points = points_by_curve[curve_i];
        for (const int point_i : points) {
          if (selected[point_i]) {
            return true;
          }
        }
        return false;
      });
}

IndexMask end_points(const bke::CurvesGeometry &curves,
                     const IndexMask &curves_mask,
                     const int amount_start,
                     const int amount_end,
                     const bool inverted,
                     IndexMaskMemory &memory)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();

  Array<bool> end_points(curves.points_num(), inverted ? false : true);
  curves_mask.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
    end_points.as_mutable_span()
        .slice(points_by_curve[curve_i].drop_front(amount_start).drop_back(amount_end))
        .fill(inverted ? true : false);
  });

  return IndexMask::from_bools(end_points, memory);
}

IndexMask end_points(const bke::CurvesGeometry &curves,
                     const int amount_start,
                     const int amount_end,
                     const bool inverted,
                     IndexMaskMemory &memory)
{
  return end_points(curves, curves.curves_range(), amount_start, amount_end, inverted, memory);
}

}  // namespace blender::ed::curves
