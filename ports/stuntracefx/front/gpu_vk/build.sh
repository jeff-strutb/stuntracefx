#!/bin/sh
# Compile the Vulkan fill's kernels (rows.comp, fill.comp, water.comp, prims.comp, pack.comp, haze.comp, noise.comp, common.glsl) and
# the models' and screen quads' shaders (model.vert, model.frag, quad.vert,
# quad.frag) to
# SPIR-V and write them into ../gpu_vk_spv.h, which is checked in: building
# the game needs no shader compiler. Run after editing them
# (glslangValidator, from glslang or the Vulkan SDK).
set -e
cd "$(dirname "$0")"
glslangValidator -V --target-env vulkan1.1 --vn k_rows_comp -o rows.h rows.comp >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn k_fill_comp -o fill.h fill.comp >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn k_water_comp -o water.h water.comp >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn k_prims_comp -o prims.h prims.comp >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn k_pack_comp -o pack.h pack.comp >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn k_haze_comp -o haze.h haze.comp >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn k_noise_comp -o noise.h noise.comp >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn k_model_vert -o mvert.h model.vert >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn k_model_frag -o mfrag.h model.frag >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn k_quad_vert -o qvert.h quad.vert >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn k_quad_frag -o qfrag.h quad.frag >/dev/null
{
  echo "/* gpu_vk_spv.h: SPIR-V of gpu_vk/rows.comp, fill.comp, water.comp, prims.comp, pack.comp, haze.comp, noise.comp, model.vert, model.frag, quad.vert and quad.frag, written by gpu_vk/build.sh: do not edit. */"
  for f in rows.h fill.h water.h prims.h pack.h haze.h noise.h mvert.h mfrag.h qvert.h qfrag.h; do sed -n '/^const uint32_t/,$p' $f | sed 's/^const/static const/'; done
} > ../gpu_vk_spv.h
rm -f rows.h fill.h water.h prims.h pack.h haze.h noise.h mvert.h mfrag.h qvert.h qfrag.h
