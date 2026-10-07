/* 32-bit packed storage compatibility helpers for Vulkan drivers without
 * VK_KHR_8bit_storage. Buffers retain their native CPU byte layout. */
#ifndef PACKED_STORAGE_H_
#define PACKED_STORAGE_H_

uint packed_u8_load(uint word, uint byte_index)
{
	return (word >> ((byte_index & 3u) * 8u)) & 0xffu;
}

uint packed_u16_load(uint word, uint half_index)
{
	return (word >> ((half_index & 1u) * 16u)) & 0xffffu;
}

int packed_i16_load(uint word, uint half_index)
{
	return bitfieldExtract(int(word), int((half_index & 1u) * 16u), 16);
}

uvec4 packed_u8x4(uint word)
{
	return uvec4(word & 0xffu, (word >> 8u) & 0xffu,
	             (word >> 16u) & 0xffu, (word >> 24u) & 0xffu);
}

// Atomic RMW prevents adjacent byte writers from destroying one another.
#define PACKED_STORE_U8(words, byte_index, value) \
	{ uint _pi = (byte_index) >> 2u; uint _ps = ((byte_index) & 3u) * 8u; \
	  uint _pm = 0xffu << _ps; uint _po; uint _pn; \
	  do { _po = (words)[_pi]; _pn = (_po & ~_pm) | (((uint(value)) & 0xffu) << _ps); } \
	  while (atomicCompSwap((words)[_pi], _po, _pn) != _po); }

#define PACKED_LOAD_U8(words, byte_index) packed_u8_load((words)[(byte_index) >> 2u], (byte_index))
#define PACKED_LOAD_U16(words, half_index) packed_u16_load((words)[(half_index) >> 1u], (half_index))

#endif
