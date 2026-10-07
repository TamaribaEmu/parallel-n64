/* Copyright (c) 2020 Themaister
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef DATA_STRUCTURES_BUFFERS_H_
#define DATA_STRUCTURES_BUFFERS_H_

#include "data_structures.h"
#include "packed_storage.h"

layout(set = 0, binding = 0, std430) buffer VRAM32
{
	uint data[];
} vram32;

layout(set = 0, binding = 0, std430) buffer VRAM16
{
	mem_u16 data[];
} vram16;

layout(set = 0, binding = 0, std430) buffer VRAM8
{
#if WIDE_STORAGE
	uint data[];
#else
	mem_u8 data[];
#endif
} vram8;

layout(set = 0, binding = 1, std430) buffer HiddenVRAM
{
#if WIDE_STORAGE
	uint data[];
#else
	mem_u8 data[];
#endif
} hidden_vram;

layout(set = 0, binding = 2, std430) readonly buffer TMEM16
{
	TMEMInstance16Mem instances[];
} tmem16;

layout(set = 0, binding = 2, std430) readonly buffer TMEM8
{
#if WIDE_STORAGE
	uint data[];
#else
	TMEMInstance8Mem instances[];
#endif
} tmem8;

#if WIDE_STORAGE
uint vram_load8(uint index) { return PACKED_LOAD_U8(vram8.data, index); }
uint hidden_vram_load8(uint index) { return PACKED_LOAD_U8(hidden_vram.data, index); }
uint tmem_load8(uint instance, uint index)
{
	return PACKED_LOAD_U8(tmem8.data, instance * 4096u + index);
}
#else
uint vram_load8(uint index) { return uint(vram8.data[index]); }
uint hidden_vram_load8(uint index) { return uint(hidden_vram.data[index]); }
uint tmem_load8(uint instance, uint index) { return uint(tmem8.instances[instance].elems[index]); }
#endif

layout(set = 1, binding = 0, std430) readonly buffer TriangleSetupBuffer
{
#if WIDE_STORAGE
	uint words[];
#else
	TriangleSetupMem elems[];
#endif
} triangle_setup;
#include "load_triangle_setup.h"

layout(set = 1, binding = 1, std430) readonly buffer AttributeSetupBuffer
{
	AttributeSetupMem elems[];
} attribute_setup;
#include "load_attribute_setup.h"

layout(set = 1, binding = 2, std430) readonly buffer DerivedSetupBuffer
{
#if WIDE_STORAGE
	uint words[];
#else
	DerivedSetupMem elems[];
#endif
} derived_setup;
#include "load_derived_setup.h"

layout(set = 1, binding = 3, std430) readonly buffer ScissorStateBuffer
{
	ScissorStateMem elems[];
} scissor_state;
#include "load_scissor_state.h"

layout(set = 1, binding = 4, std430) readonly buffer StaticRasterStateBuffer
{
#if WIDE_STORAGE
	uint words[];
#else
	StaticRasterizationStateMem elems[];
#endif
} static_raster_state;
#include "load_static_raster_state.h"

layout(set = 1, binding = 5, std430) readonly buffer DepthBlendStateBuffer
{
#if WIDE_STORAGE
	uint words[];
#else
	DepthBlendStateMem elems[];
#endif
} depth_blend_state;
#include "load_depth_blend_state.h"

layout(set = 1, binding = 6, std430) readonly buffer StateIndicesBuffer
{
#if WIDE_STORAGE
	uint words[];
#else
	InstanceIndicesMem elems[];
#endif
} state_indices;

#if WIDE_STORAGE
uvec4 load_state_static_depth_tmem(uint index)
{
	return packed_u8x4(state_indices.words[index * 4u]);
}

uint load_state_tile_info(uint index, uint tile)
{
	uint word = state_indices.words[index * 4u + 2u + (tile >> 2u)];
	return packed_u8_load(word, tile);
}
#else
uvec4 load_state_static_depth_tmem(uint index)
{
	return uvec4(state_indices.elems[index].static_depth_tmem);
}

uint load_state_tile_info(uint index, uint tile)
{
	return uint(state_indices.elems[index].tile_infos[tile]);
}
#endif

layout(set = 1, binding = 7, std430) readonly buffer TileInfoBuffer
{
#if WIDE_STORAGE
	uint words[];
#else
	TileInfoMem elems[];
#endif
} tile_infos;
#include "load_tile_info.h"

layout(set = 1, binding = 8, std430) readonly buffer SpanSetups
{
#if WIDE_STORAGE
	uint words[];
#else
	SpanSetupMem elems[];
#endif
} span_setups;
#include "load_span_setup.h"

layout(set = 1, binding = 9, std430) readonly buffer SpanInfoOffsetBuffer
{
	SpanInfoOffsetsMem elems[];
} span_offsets;
#include "load_span_offsets.h"

layout(set = 1, binding = 10) uniform utextureBuffer uBlenderDividerLUT;

layout(set = 1, binding = 11, std430) readonly buffer TileBinning
{
	uint elems[];
} tile_binning;

layout(set = 1, binding = 12, std430) readonly buffer TileBinningCoarse
{
	uint elems[];
} tile_binning_coarse;

layout(set = 2, binding = 0, std140) uniform GlobalConstants
{
	GlobalFBInfo fb_info;
} global_constants;

#endif
