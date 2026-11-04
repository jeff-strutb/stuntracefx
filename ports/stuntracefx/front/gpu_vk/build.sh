#!/bin/sh
# Compile the Vulkan fill's kernels (rows.comp, fill.comp, water.comp, common.glsl) to
# SPIR-V and write them into ../gpu_vk_spv.h, which is checked in: building
# the game needs no shader compiler. Run after editing them
# (glslangValidator, from glslang or the Vulkan SDK).
set -e
cd "$(dirname "$0")"
glslangValidator -V --target-env vulkan1.1 --vn k_rows_comp -o rows.h rows.comp >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn k_fill_comp -o fill.h fill.comp >/dev/null
glslangValidator -V --target-env vulkan1.1 --vn k_water_comp -o water.h water.comp >/dev/null
{
  echo "/* gpu_vk_spv.h: SPIR-V of gpu_vk/rows.comp, fill.comp and water.comp, written by gpu_vk/build.sh: do not edit. */"
  for f in rows.h fill.h water.h; do sed -n '/^const uint32_t/,$p' $f | sed 's/^const/static const/'; done
} > ../gpu_vk_spv.h
rm -f rows.h fill.h water.h
