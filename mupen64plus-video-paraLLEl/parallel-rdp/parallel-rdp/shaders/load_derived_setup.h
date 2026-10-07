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

#ifndef LOAD_DERIVED_SETUP_H_
#define LOAD_DERIVED_SETUP_H_

DerivedSetup load_derived_setup(uint index)
{
#if WIDE_STORAGE
	uint base = index * 14u;
	uint dz_word = derived_setup.words[base + 11u];
	uint f0 = derived_setup.words[base + 12u];
	uint f1 = derived_setup.words[base + 13u];
	return DerivedSetup(
		u8x4(packed_u8x4(derived_setup.words[base + 0u])),
		u8x4(packed_u8x4(derived_setup.words[base + 1u])),
		u8x4(packed_u8x4(derived_setup.words[base + 2u])),
		u8x4(packed_u8x4(derived_setup.words[base + 3u])),
		u8x4(packed_u8x4(derived_setup.words[base + 4u])),
		u8x4(packed_u8x4(derived_setup.words[base + 5u])),
		u8x4(packed_u8x4(derived_setup.words[base + 6u])),
		u8x4(packed_u8x4(derived_setup.words[base + 7u])),
		u8x4(packed_u8x4(derived_setup.words[base + 8u])),
		u8x4(packed_u8x4(derived_setup.words[base + 9u])),
		derived_setup.words[base + 10u], u16(dz_word & 0xffffu),
		u8((dz_word >> 16u) & 0xffu), u8(dz_word >> 24u),
		i16x4(bitfieldExtract(int(f0), 0, 16), bitfieldExtract(int(f0), 16, 16),
		        bitfieldExtract(int(f1), 0, 16), bitfieldExtract(int(f1), 16, 16)));
#elif SMALL_TYPES
	return derived_setup.elems[index];
#else
	return DerivedSetup(
			u8x4(derived_setup.elems[index].constant_muladd0),
			u8x4(derived_setup.elems[index].constant_mulsub0),
			u8x4(derived_setup.elems[index].constant_mul0),
			u8x4(derived_setup.elems[index].constant_add0),
			u8x4(derived_setup.elems[index].constant_muladd1),
			u8x4(derived_setup.elems[index].constant_mulsub1),
			u8x4(derived_setup.elems[index].constant_mul1),
			u8x4(derived_setup.elems[index].constant_add1),
			u8x4(derived_setup.elems[index].fog_color),
			u8x4(derived_setup.elems[index].blend_color),
			uint(derived_setup.elems[index].fill_color),
			u16(derived_setup.elems[index].dz),
			u8(derived_setup.elems[index].dz_compressed),
			u8(derived_setup.elems[index].min_lod),
			i16x4(derived_setup.elems[index].factors));
#endif
}

#endif
