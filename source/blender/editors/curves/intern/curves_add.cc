/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include "BLI_listbase.h"
#include "BLI_math_base_safe.h"
#include "BLI_rand.hh"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"

#include "BLT_translation.hh"

#include "ED_curves.hh"
#include "ED_node.hh"
#include "ED_object.hh"

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"

namespace blender::ed::curves {

static bool has_surface_deformation_node(const bNodeTree &ntree)
{
  if (!ntree.nodes_by_type("GeometryNodeDeformCurvesOnSurface").is_empty()) {
    return true;
  }
  for (const bNode *node : ntree.group_nodes()) {
    if (const bNodeTree *sub_tree = reinterpret_cast<const bNodeTree *>(node->id)) {
      if (has_surface_deformation_node(*sub_tree)) {
        return true;
      }
    }
  }
  return false;
}

static bool has_surface_deformation_node(const Object &curves_ob)
{
  LISTBASE_FOREACH (const ModifierData *, md, &curves_ob.modifiers) {
    if (md->type != eModifierType_Nodes) {
      continue;
    }
    const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
    if (nmd->node_group == nullptr) {
      continue;
    }
    if (has_surface_deformation_node(*nmd->node_group)) {
      return true;
    }
  }
  return false;
}

void ensure_surface_deformation_node_exists(bContext &C, Object &curves_ob)
{
  if (has_surface_deformation_node(curves_ob)) {
    return;
  }

  Main *bmain = CTX_data_main(&C);
  Scene *scene = CTX_data_scene(&C);

  ModifierData *md = object::modifier_add(
      nullptr, bmain, scene, &curves_ob, DATA_("Surface Deform"), eModifierType_Nodes);
  NodesModifierData &nmd = *reinterpret_cast<NodesModifierData *>(md);
  nmd.node_group = bke::node_tree_add_tree(bmain, DATA_("Surface Deform"), "GeometryNodeTree");

  if (!nmd.node_group->geometry_node_asset_traits) {
    nmd.node_group->geometry_node_asset_traits = MEM_callocN<GeometryNodeAssetTraits>(__func__);
  }

  nmd.node_group->geometry_node_asset_traits->flag |= GEO_NODE_ASSET_MODIFIER;

  bNodeTree *ntree = nmd.node_group;
  ntree->tree_interface.add_socket(
      "Geometry", "", "NodeSocketGeometry", NODE_INTERFACE_SOCKET_OUTPUT, nullptr);
  ntree->tree_interface.add_socket(
      "Geometry", "", "NodeSocketGeometry", NODE_INTERFACE_SOCKET_INPUT, nullptr);
  bNode *group_input = bke::node_add_static_node(&C, *ntree, NODE_GROUP_INPUT);
  bNode *group_output = bke::node_add_static_node(&C, *ntree, NODE_GROUP_OUTPUT);
  bNode *deform_node = bke::node_add_static_node(&C, *ntree, GEO_NODE_DEFORM_CURVES_ON_SURFACE);

  BKE_main_ensure_invariants(*bmain, nmd.node_group->id);

  bke::node_add_link(*ntree,
                     *group_input,
                     *static_cast<bNodeSocket *>(group_input->outputs.first),
                     *deform_node,
                     *bke::node_find_socket(*deform_node, SOCK_IN, "Curves"));
  bke::node_add_link(*ntree,
                     *deform_node,
                     *bke::node_find_socket(*deform_node, SOCK_OUT, "Curves"),
                     *group_output,
                     *static_cast<bNodeSocket *>(group_output->inputs.first));

  group_input->location[0] = -200;
  group_output->location[0] = 200;
  deform_node->location[0] = 0;

  BKE_main_ensure_invariants(*bmain, nmd.node_group->id);
}

bke::CurvesGeometry primitive_random_sphere(const int curves_size, const int points_per_curve)
{
  bke::CurvesGeometry curves(points_per_curve * curves_size, curves_size);

  MutableSpan<int> offsets = curves.offsets_for_write();
  MutableSpan<float3> positions = curves.positions_for_write();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter<float> radius = attributes.lookup_or_add_for_write_only_span<float>(
      "radius", bke::AttrDomain::Point);

  for (const int i : offsets.index_range()) {
    offsets[i] = points_per_curve * i;
  }

  RandomNumberGenerator rng;

  const OffsetIndices points_by_curve = curves.points_by_curve();
  for (const int i : curves.curves_range()) {
    const IndexRange points = points_by_curve[i];
    MutableSpan<float3> curve_positions = positions.slice(points);
    MutableSpan<float> curve_radii = radius.span.slice(points);

    const float theta = 2.0f * M_PI * rng.get_float();
    const float phi = safe_acosf(2.0f * rng.get_float() - 1.0f);

    float3 no = {std::sin(theta) * std::sin(phi), std::cos(theta) * std::sin(phi), std::cos(phi)};
    no = math::normalize(no);

    float3 co = no;
    for (int key = 0; key < points_per_curve; key++) {
      float t = key / float(points_per_curve - 1);
      curve_positions[key] = co;
      curve_radii[key] = 0.02f * (1.0f - t);

      float3 offset = float3(rng.get_float(), rng.get_float(), rng.get_float()) * 2.0f - 1.0f;
      co += (offset + no) / points_per_curve;
    }
  }

  radius.finish();

  return curves;
}

}  // namespace blender::ed::curves
