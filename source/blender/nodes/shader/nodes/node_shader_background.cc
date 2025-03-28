/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_background_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .description("Color of the emitted light");
  b.add_input<decl::Float>("Strength")
      .default_value(1.0f)
      .min(0.0f)
      .max(1000000.0f)
      .description("Strength of the emitted light")
      .translation_context(BLT_I18NCONTEXT_AMOUNT);
  b.add_input<decl::Float>("Weight").available(false);
  b.add_output<decl::Shader>("Background");
}

static int node_shader_gpu_background(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_background", in, out);
}

}  // namespace blender::nodes::node_shader_background_cc

/* node type definition */
void register_node_type_sh_background()
{
  namespace file_ns = blender::nodes::node_shader_background_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeBackground", SH_NODE_BACKGROUND);
  ntype.ui_name = "Background";
  ntype.ui_description =
      "Add background light emission.\nNote: This node should only be used for the world surface "
      "output";
  ntype.enum_name_legacy = "BACKGROUND";
  ntype.nclass = NODE_CLASS_SHADER;
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = world_shader_nodes_poll;
  ntype.gpu_fn = file_ns::node_shader_gpu_background;

  blender::bke::node_register_type(ntype);
}
