/*
 * This software is licensed under the terms of the MIT License.
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2024, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2024, Andrei Alexeyev <akari@taisei-project.org>.
 */

#include "texture.h"
#include "sdlgpu.h"

#include "../api.h"

#include "util.h"

static int8_t texture_type_uncompressed_remap_table[] = {
	#define INIT_ENTRY(type_suffix, ...) \
		[TEX_TYPE_##type_suffix] = TEX_TYPE_INVALID,
	TEX_TYPES_UNCOMPRESSED(INIT_ENTRY)
};

static SDL_GpuTextureFormat sdlgpu_texfmt_ts2sdl(TextureType t) {
	switch(t) {
		case TEX_TYPE_RGBA_8:						return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		case TEX_TYPE_RGB_8:						return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_RG_8:							return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_R_8:							return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
		case TEX_TYPE_RGBA_16:						return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM;
		case TEX_TYPE_RGB_16:						return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_RG_16:						return SDL_GPU_TEXTUREFORMAT_R16G16_UNORM;
		case TEX_TYPE_R_16:							return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_RGBA_16_FLOAT:				return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
		case TEX_TYPE_RGB_16_FLOAT:					return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_RG_16_FLOAT:					return SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;
		case TEX_TYPE_R_16_FLOAT:					return SDL_GPU_TEXTUREFORMAT_R16_FLOAT;
		case TEX_TYPE_RGBA_32_FLOAT:				return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
		case TEX_TYPE_RGB_32_FLOAT:					return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_RG_32_FLOAT:					return SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT;
		case TEX_TYPE_R_32_FLOAT:					return SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
		case TEX_TYPE_DEPTH_8:						return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_DEPTH_16: 					return SDL_GPU_TEXTUREFORMAT_D16_UNORM;
		case TEX_TYPE_DEPTH_24:						return SDL_GPU_TEXTUREFORMAT_D24_UNORM;
		case TEX_TYPE_DEPTH_32:						return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_DEPTH_16_FLOAT:				return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
		case TEX_TYPE_DEPTH_32_FLOAT:				return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
		case TEX_TYPE_COMPRESSED_ETC1_RGB:			return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_COMPRESSED_ETC2_RGBA:			return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_COMPRESSED_BC1_RGB:			return SDL_GPU_TEXTUREFORMAT_BC1_UNORM;
		case TEX_TYPE_COMPRESSED_BC3_RGBA:			return SDL_GPU_TEXTUREFORMAT_BC3_UNORM;
		case TEX_TYPE_COMPRESSED_BC4_R:				return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_COMPRESSED_BC5_RG:			return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_COMPRESSED_BC7_RGBA:			return SDL_GPU_TEXTUREFORMAT_BC7_UNORM;
		case TEX_TYPE_COMPRESSED_PVRTC1_4_RGB:		return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_COMPRESSED_PVRTC1_4_RGBA:		return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_COMPRESSED_ASTC_4x4_RGBA:		return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_COMPRESSED_ATC_RGB:			return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_COMPRESSED_ATC_RGBA:			return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_COMPRESSED_PVRTC2_4_RGB:		return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_COMPRESSED_PVRTC2_4_RGBA:		return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_COMPRESSED_ETC2_EAC_R11:		return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_COMPRESSED_ETC2_EAC_RG11:		return SDL_GPU_TEXTUREFORMAT_INVALID;
		case TEX_TYPE_COMPRESSED_FXT1_RGB:			return SDL_GPU_TEXTUREFORMAT_INVALID;

		default:
			UNREACHABLE;
	}
}

static SDL_GpuTextureFormat sdlgpu_texfmt_ts2sdl_checksupport(TextureType t) {
	SDL_GpuTextureFormat fmt = sdlgpu_texfmt_ts2sdl(t);

	if(fmt == SDL_GPU_TEXTUREFORMAT_INVALID) {
		return fmt;
	}

	SDL_GpuTextureUsageFlags usage = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT;

	if(TEX_TYPE_IS_DEPTH(t)) {
		usage |= SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT;
	} else {
		usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT;
	}

	// FIXME query support for cubemaps separately?

	if(SDL_GpuSupportsTextureFormat(sdlgpu.device, fmt, SDL_GPU_TEXTURETYPE_2D, usage)) {
		return fmt;
	}

	return SDL_GPU_TEXTUREFORMAT_INVALID;
}

static SDL_GpuTextureFormat sdlgpu_texfmt_to_srgb(SDL_GpuTextureFormat fmt) {
	switch(fmt) {
		case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
		case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM: return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
		case SDL_GPU_TEXTUREFORMAT_BC3_UNORM:      return SDL_GPU_TEXTUREFORMAT_BC3_UNORM_SRGB;
		case SDL_GPU_TEXTUREFORMAT_BC7_UNORM:      return SDL_GPU_TEXTUREFORMAT_BC7_UNORM_SRGB;
		default: return SDL_GPU_TEXTUREFORMAT_INVALID;
	}
}

static TextureType sdlgpu_remap_texture_type(TextureType tt) {
	if(!TEX_TYPE_IS_COMPRESSED(tt)) {
		return texture_type_uncompressed_remap_table[tt];
	}

	return tt;
}

static PixmapFormat sdlgpu_texfmt_to_pixfmt(SDL_GpuTextureFormat fmt) {
	switch(fmt) {
		case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
			return PIXMAP_FORMAT_RGBA8;
		case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
			return PIXMAP_FORMAT_RGBA8;  // FIXME can't represent swizzled formats yet!
		case SDL_GPU_TEXTUREFORMAT_R16G16_UNORM:
			return PIXMAP_FORMAT_RG16;
		case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM:
			return PIXMAP_FORMAT_RGBA16;
		case SDL_GPU_TEXTUREFORMAT_R8_UNORM:
			return PIXMAP_FORMAT_R8;
		case SDL_GPU_TEXTUREFORMAT_A8_UNORM:
			return PIXMAP_FORMAT_R8;
		case SDL_GPU_TEXTUREFORMAT_BC1_UNORM:
			return PIXMAP_FORMAT_BC1_RGB;
		case SDL_GPU_TEXTUREFORMAT_BC3_UNORM:
			return PIXMAP_FORMAT_BC3_RGBA;
		case SDL_GPU_TEXTUREFORMAT_BC7_UNORM:
			return PIXMAP_FORMAT_BC7_RGBA;
		case SDL_GPU_TEXTUREFORMAT_R8G8_SNORM:
			return PIXMAP_FORMAT_RG8;
		case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM:
			return PIXMAP_FORMAT_RGBA8;
		case SDL_GPU_TEXTUREFORMAT_R16_FLOAT:
			return PIXMAP_FORMAT_R16F;
		case SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT:
			return PIXMAP_FORMAT_RG16F;
		case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT:
			return PIXMAP_FORMAT_RGBA16F;
		case SDL_GPU_TEXTUREFORMAT_R32_FLOAT:
			return PIXMAP_FORMAT_R32F;
		case SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT:
			return PIXMAP_FORMAT_RG32F;
		case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT:
			return PIXMAP_FORMAT_RGB32F;
		case SDL_GPU_TEXTUREFORMAT_R8_UINT:
			return PIXMAP_FORMAT_R8;
		case SDL_GPU_TEXTUREFORMAT_R8G8_UINT:
			return PIXMAP_FORMAT_RG8;
		case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT:
			return PIXMAP_FORMAT_RGBA8;
		case SDL_GPU_TEXTUREFORMAT_R16_UINT:
			return PIXMAP_FORMAT_R16;
		case SDL_GPU_TEXTUREFORMAT_R16G16_UINT:
			return PIXMAP_FORMAT_RG16;
		case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT:
			return PIXMAP_FORMAT_RGBA16;
		case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB:
			return PIXMAP_FORMAT_RGBA8;
		case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB:
			return PIXMAP_FORMAT_RGBA8;  // FIXME can't represent swizzled formats yet!
		case SDL_GPU_TEXTUREFORMAT_BC3_UNORM_SRGB:
			return PIXMAP_FORMAT_BC3_RGBA;
		case SDL_GPU_TEXTUREFORMAT_BC7_UNORM_SRGB:
			return PIXMAP_FORMAT_BC7_RGBA;
		case SDL_GPU_TEXTUREFORMAT_D16_UNORM:
			return PIXMAP_FORMAT_R16;
		case SDL_GPU_TEXTUREFORMAT_D32_FLOAT:
			return PIXMAP_FORMAT_R32F;

		case SDL_GPU_TEXTUREFORMAT_INVALID:
		case SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM:
		case SDL_GPU_TEXTUREFORMAT_B5G5R5A1_UNORM:
		case SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM:
		case SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM:
		case SDL_GPU_TEXTUREFORMAT_BC2_UNORM:
		case SDL_GPU_TEXTUREFORMAT_D24_UNORM:
		case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
		case SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT:
			return 0;
	}

	UNREACHABLE;
}

void sdlgpu_texture_init_type_remap_table(void) {
	// TODO: This logic could be generalized to all backends
	static const int8_t fallback_table[][9] = {
		[TEX_TYPE_RGB_8]			= { TEX_TYPE_RGBA_8, TEX_TYPE_INVALID },
		[TEX_TYPE_RGBA_8]			= { TEX_TYPE_INVALID },
		[TEX_TYPE_RG_8]				= { TEX_TYPE_RGB_8, TEX_TYPE_RGBA_8, TEX_TYPE_RG_16, TEX_TYPE_RGB_16, TEX_TYPE_RGBA_16, TEX_TYPE_INVALID },
		[TEX_TYPE_R_8]				= { TEX_TYPE_R_8, TEX_TYPE_RG_8, TEX_TYPE_R_16, TEX_TYPE_RGB_8, TEX_TYPE_RG_16, TEX_TYPE_RGBA_8, TEX_TYPE_RGB_16, TEX_TYPE_RGBA_16, TEX_TYPE_INVALID },
		[TEX_TYPE_RGBA_16]			= { TEX_TYPE_RGBA_8, TEX_TYPE_INVALID },
		[TEX_TYPE_RGB_16]			= { TEX_TYPE_RGBA_16, TEX_TYPE_RGB_8, TEX_TYPE_RGBA_8, TEX_TYPE_INVALID },
		[TEX_TYPE_RG_16]			= { TEX_TYPE_RGB_16, TEX_TYPE_RGBA_16, TEX_TYPE_RG_8, TEX_TYPE_RGB_8, TEX_TYPE_RGBA_8, TEX_TYPE_INVALID },
		[TEX_TYPE_R_16]				= { TEX_TYPE_RG_16, TEX_TYPE_RGB_16, TEX_TYPE_RGBA_16, TEX_TYPE_R_8, TEX_TYPE_RG_8, TEX_TYPE_RGB_8, TEX_TYPE_RGBA_8, TEX_TYPE_INVALID },
		[TEX_TYPE_RGBA_16_FLOAT]	= { TEX_TYPE_RGBA_32_FLOAT, TEX_TYPE_INVALID },
		[TEX_TYPE_RGB_16_FLOAT]		= { TEX_TYPE_RGBA_16_FLOAT, TEX_TYPE_RGB_32_FLOAT, TEX_TYPE_RGBA_32_FLOAT, TEX_TYPE_INVALID },
		[TEX_TYPE_RG_16_FLOAT]		= { TEX_TYPE_RGB_16_FLOAT, TEX_TYPE_RG_32_FLOAT, TEX_TYPE_RGBA_16_FLOAT, TEX_TYPE_RGB_32_FLOAT, TEX_TYPE_RGBA_32_FLOAT, TEX_TYPE_INVALID },
		[TEX_TYPE_R_16_FLOAT]		= { TEX_TYPE_RG_16_FLOAT, TEX_TYPE_R_32_FLOAT, TEX_TYPE_RGB_16_FLOAT, TEX_TYPE_RG_32_FLOAT, TEX_TYPE_RGBA_16_FLOAT, TEX_TYPE_RGB_32_FLOAT, TEX_TYPE_RGBA_32_FLOAT, TEX_TYPE_INVALID },
		[TEX_TYPE_RGBA_32_FLOAT]	= { TEX_TYPE_RGBA_16_FLOAT, TEX_TYPE_INVALID },
		[TEX_TYPE_RGB_32_FLOAT]		= { TEX_TYPE_RGBA_32_FLOAT, TEX_TYPE_RGB_16_FLOAT, TEX_TYPE_RGBA_16_FLOAT, TEX_TYPE_INVALID },
		[TEX_TYPE_RG_32_FLOAT]		= { TEX_TYPE_RGB_32_FLOAT, TEX_TYPE_RGBA_32_FLOAT, TEX_TYPE_RG_16_FLOAT, TEX_TYPE_RGB_16_FLOAT, TEX_TYPE_RGBA_16_FLOAT, TEX_TYPE_INVALID },
		[TEX_TYPE_R_32_FLOAT]		= { TEX_TYPE_RG_32_FLOAT, TEX_TYPE_RGB_32_FLOAT, TEX_TYPE_RGBA_32_FLOAT, TEX_TYPE_RG_16_FLOAT, TEX_TYPE_RGB_16_FLOAT, TEX_TYPE_RGBA_16_FLOAT, TEX_TYPE_INVALID },
		[TEX_TYPE_DEPTH_8]			= { TEX_TYPE_DEPTH_16, TEX_TYPE_DEPTH_24, TEX_TYPE_DEPTH_32, TEX_TYPE_DEPTH_16_FLOAT, TEX_TYPE_DEPTH_32_FLOAT, TEX_TYPE_INVALID },
		[TEX_TYPE_DEPTH_16]			= { TEX_TYPE_DEPTH_24, TEX_TYPE_DEPTH_32, TEX_TYPE_DEPTH_16_FLOAT, TEX_TYPE_DEPTH_32_FLOAT, TEX_TYPE_DEPTH_8, TEX_TYPE_INVALID },
		[TEX_TYPE_DEPTH_24]			= { TEX_TYPE_DEPTH_32, TEX_TYPE_DEPTH_24, TEX_TYPE_DEPTH_32_FLOAT, TEX_TYPE_DEPTH_16, TEX_TYPE_DEPTH_16_FLOAT, TEX_TYPE_DEPTH_8, TEX_TYPE_INVALID },
		[TEX_TYPE_DEPTH_32]			= { TEX_TYPE_DEPTH_24, TEX_TYPE_DEPTH_32_FLOAT, TEX_TYPE_DEPTH_16, TEX_TYPE_DEPTH_16_FLOAT, TEX_TYPE_DEPTH_8, TEX_TYPE_INVALID },
		[TEX_TYPE_DEPTH_16_FLOAT]	= { TEX_TYPE_DEPTH_32_FLOAT, TEX_TYPE_DEPTH_16, TEX_TYPE_DEPTH_24, TEX_TYPE_DEPTH_32, TEX_TYPE_DEPTH_8, TEX_TYPE_INVALID },
		[TEX_TYPE_DEPTH_32_FLOAT]	= { TEX_TYPE_DEPTH_16_FLOAT, TEX_TYPE_DEPTH_32, TEX_TYPE_DEPTH_24, TEX_TYPE_DEPTH_16, TEX_TYPE_DEPTH_8, TEX_TYPE_INVALID },
	};

	for(TextureType t = 0; t < ARRAY_SIZE(texture_type_uncompressed_remap_table); ++t) {
		SDL_GpuTextureFormat gpu_format = sdlgpu_texfmt_ts2sdl_checksupport(t);

		if(gpu_format != SDL_GPU_TEXTUREFORMAT_INVALID) {
			texture_type_uncompressed_remap_table[t] = t;
			continue;
		}

		texture_type_uncompressed_remap_table[t] = TEX_TYPE_INVALID;

		for(int i = 0; i < ARRAY_SIZE(fallback_table[t]); ++i) {
			TextureType fallback_type = fallback_table[t][i];

			if(fallback_type == TEX_TYPE_INVALID) {
				break;
			}

			gpu_format = sdlgpu_texfmt_ts2sdl_checksupport(fallback_type);

			if(gpu_format != SDL_GPU_TEXTUREFORMAT_INVALID) {
				texture_type_uncompressed_remap_table[t] = fallback_type;
				break;
			}
		}

		attr_unused TextureType remapped_type = texture_type_uncompressed_remap_table[t];
		gpu_format = sdlgpu_texfmt_ts2sdl(remapped_type);
		assert(gpu_format != SDL_GPU_TEXTUREFORMAT_INVALID);
		assert(gpu_format == sdlgpu_texfmt_ts2sdl_checksupport(remapped_type));

		log_debug("%s --> %s (GPU format: %i)",
			r_texture_type_name(t), r_texture_type_name(remapped_type), gpu_format);
	}
}

static bool sdlgpu_texture_check_swizzle_component(char val, char defval) {
	return !val || val == defval;
}

static bool sdlgpu_texture_check_swizzle(SDL_GpuTextureFormat fmt, const SwizzleMask *swizzle) {
	return
		sdlgpu_texture_check_swizzle_component(swizzle->r, 'r') &&
		sdlgpu_texture_check_swizzle_component(swizzle->g, 'g') &&
		sdlgpu_texture_check_swizzle_component(swizzle->b, 'b') &&
		sdlgpu_texture_check_swizzle_component(swizzle->a, 'a');
}

static SDL_GpuFilter sdlgpu_filter_ts2sdl(TextureFilterMode fm) {
	switch(fm) {
		case TEX_FILTER_LINEAR:
		case TEX_FILTER_LINEAR_MIPMAP_NEAREST:
		case TEX_FILTER_LINEAR_MIPMAP_LINEAR:
			return SDL_GPU_FILTER_LINEAR;
		case TEX_FILTER_NEAREST:
		case TEX_FILTER_NEAREST_MIPMAP_NEAREST:
		case TEX_FILTER_NEAREST_MIPMAP_LINEAR:
			return SDL_GPU_FILTER_NEAREST;
	}

	UNREACHABLE;
}

static SDL_GpuSamplerMipmapMode sdlgpu_mipmode_ts2sdl(TextureFilterMode fm) {
	switch(fm) {
		case TEX_FILTER_LINEAR_MIPMAP_LINEAR:
		case TEX_FILTER_NEAREST_MIPMAP_LINEAR:
			return SDL_GPU_FILTER_LINEAR;
		case TEX_FILTER_LINEAR_MIPMAP_NEAREST:
		case TEX_FILTER_NEAREST_MIPMAP_NEAREST:
		case TEX_FILTER_LINEAR:
		case TEX_FILTER_NEAREST:
			return SDL_GPU_FILTER_NEAREST;
	}

	UNREACHABLE;
}

static SDL_GpuTextureType sdlgpu_type_ts2sdl(TextureClass cls) {
	switch(cls) {
		case TEXTURE_CLASS_2D:
			return SDL_GPU_TEXTURETYPE_2D;
		case TEXTURE_CLASS_CUBEMAP:
			return SDL_GPU_TEXTURETYPE_CUBE;
		default: UNREACHABLE;
	}
}

static SDL_GpuSamplerMipmapMode sdlgpu_addrmode_ts2sdl(TextureWrapMode wm) {
	switch(wm) {
		case TEX_WRAP_REPEAT:	return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		case TEX_WRAP_MIRROR:	return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
		case TEX_WRAP_CLAMP:	return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	}

	UNREACHABLE;
}

void sdlgpu_texture_update_sampler(Texture *tex) {
	if(tex->sampler && !tex->sampler_is_outdated) {
		return;
	}

	if(tex->sampler) {
		SDL_GpuReleaseSampler(sdlgpu.device, tex->sampler);
	}

	auto p = &tex->params;

	tex->sampler = SDL_GpuCreateSampler(sdlgpu.device, &(SDL_GpuSamplerCreateInfo) {
		.minFilter = sdlgpu_filter_ts2sdl(p->filter.min),
		.magFilter = sdlgpu_filter_ts2sdl(p->filter.mag),
		.mipmapMode = sdlgpu_mipmode_ts2sdl(p->filter.min),
		.addressModeU = sdlgpu_addrmode_ts2sdl(p->wrap.s),
		.addressModeV = sdlgpu_addrmode_ts2sdl(p->wrap.t),
		.addressModeW = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.mipLodBias = 0,
		.anisotropyEnable = p->anisotropy > 1,
		.maxAnisotropy = p->anisotropy,
		.compareEnable = 0,
		.compareOp = 0,
		.minLod = 0,
		.maxLod = p->mipmaps,
	});

	tex->sampler_is_outdated = false;
}

static bool is_depth_format(SDL_GpuTextureFormat fmt) {
	switch(fmt) {
		case SDL_GPU_TEXTUREFORMAT_D16_UNORM:
		case SDL_GPU_TEXTUREFORMAT_D24_UNORM:
		case SDL_GPU_TEXTUREFORMAT_D32_FLOAT:
		case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
		case SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT:
			return true;
		default: return false;
	}
}

Texture *sdlgpu_texture_create(const TextureParams *params) {
	/*
	 * FIXME: a lot of the param validation code is more-or-less copypasted from gl33,
	 * we should organize it into a common utility function.
	 */

	auto tex = ALLOC(Texture, {
		.params = *params,
		.refs = 1,
	});

	tex->params.type = sdlgpu_remap_texture_type(tex->params.type);
	SDL_GpuTextureFormat tex_fmt = sdlgpu_texfmt_ts2sdl(tex->params.type);

	if(UNLIKELY(tex->params.type == TEX_TYPE_INVALID)) {
		log_error("Requested unsupported texture type %s", r_texture_type_name(tex->params.type));
		goto fail;
	}

	assert(tex_fmt != SDL_GPU_TEXTUREFORMAT_INVALID);

	if(tex->params.flags & TEX_FLAG_SRGB) {
		SDL_GpuTextureFormat tex_fmt_srgb = sdlgpu_texfmt_to_srgb(tex_fmt);

		if(tex_fmt_srgb == SDL_GPU_TEXTUREFORMAT_INVALID) {
			log_error("No sRGB support for texture type %s (internal format %u)",
				r_texture_type_name(tex->params.type), tex_fmt);
			goto fail;
		}

		tex_fmt = tex_fmt_srgb;
	}

	if(!sdlgpu_texture_check_swizzle(tex_fmt, &tex->params.swizzle)) {
		auto swizzle = &tex->params.swizzle;
		log_warn("Can't apply swizzle %c%c%c%c to texture: swizzle masks are not supported",
			swizzle->r?:'r', swizzle->g?:'g', swizzle->b?:'b', swizzle->a?:'a');
	}

	assert(tex->params.width > 0);
	assert(tex->params.height > 0);

	uint required_layers;

	switch(tex->params.class) {
		case TEXTURE_CLASS_2D:      required_layers = 1; break;
		case TEXTURE_CLASS_CUBEMAP: required_layers = 6; break;
		default: UNREACHABLE;
	}

	if(!tex->params.layers) {
		tex->params.layers = required_layers;
	}

	assert(tex->params.layers == required_layers);

	if(tex->params.class == TEXTURE_CLASS_CUBEMAP) {
		assert(tex->params.width == tex->params.height);
	}

	uint max_mipmaps = r_texture_util_max_num_miplevels(tex->params.width, tex->params.height);
	assert(max_mipmaps > 0);

	if(tex->params.mipmaps == 0) {
		if(tex->params.mipmap_mode == TEX_MIPMAP_AUTO) {
			tex->params.mipmaps = TEX_MIPMAPS_MAX;
		} else {
			tex->params.mipmaps = 1;
		}
	}

	if(tex->params.mipmaps == TEX_MIPMAPS_MAX || tex->params.mipmaps > max_mipmaps) {
		tex->params.mipmaps = max_mipmaps;
	}

	if(tex->params.anisotropy == 0) {
		tex->params.anisotropy = TEX_ANISOTROPY_DEFAULT;
	}

	SDL_GpuTextureType type = sdlgpu_type_ts2sdl(tex->params.class);
	SDL_GpuTextureUsageFlagBits usage = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT;

	if(is_depth_format(tex_fmt)) {
		usage |= SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT;
	} else if(SDL_GpuSupportsTextureFormat(sdlgpu.device, tex_fmt, type, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT)) {
		// FIXME: we don't know whether we're going to need to render to this texture or not
		usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT;
	}


	tex->gpu_texture = SDL_GpuCreateTexture(sdlgpu.device, &(SDL_GpuTextureCreateInfo) {
		.type = type,
		.format = tex_fmt,
		.width = tex->params.width,
		.height = tex->params.height,
		.layerCountOrDepth = tex->params.layers,
		.levelCount = tex->params.mipmaps,
		.usageFlags = usage,
	});

	if(UNLIKELY(!tex->gpu_texture)) {
		log_sdl_error(LOG_ERROR, "SDL_GpuCreateTexture");
		goto fail;
	}

	tex->gpu_format = tex_fmt;

	static uint counter;
	tex->number = ++counter;
	tex->is_virgin = true;
	tex->load.op = SDL_GPU_LOADOP_DONT_CARE;
	sdlgpu_texture_set_debug_label(tex, "Unnamed");
	return tex;

fail:
	mem_free(tex);
	return NULL;
}

void sdlgpu_texture_get_size(Texture *tex, uint mipmap, uint *width, uint *height) {
	// TODO: copypasted from gl33; move to common code?

	if(mipmap >= tex->params.mipmaps) {
		mipmap = tex->params.mipmaps - 1;
	}

	assert(mipmap < 32);

	if(mipmap == 0) {
		if(width != NULL) {
			*width = tex->params.width;
		}

		if(height != NULL) {
			*height = tex->params.height;
		}
	} else {
		if(width != NULL) {
			*width = max(1, tex->params.width / (1u << mipmap));
		}

		if(height != NULL) {
			*height = max(1, tex->params.height / (1u << mipmap));
		}
	}
}

void sdlgpu_texture_get_params(Texture *tex, TextureParams *params) {
	*params = tex->params;
}

const char *sdlgpu_texture_get_debug_label(Texture *tex) {
	return tex->debug_label;
}

void sdlgpu_texture_set_debug_label(Texture *tex, const char *label) {
	// strlcpy(tex->debug_label, label, sizeof(tex->debug_label));
	snprintf(tex->debug_label, sizeof(tex->debug_label), "#%u: %s", tex->number, label);
	SDL_GpuSetTextureName(sdlgpu.device, tex->gpu_texture, tex->debug_label);
}

void sdlgpu_texture_set_filter(Texture *tex, TextureFilterMode fmin, TextureFilterMode fmag) {
	UNREACHABLE;
}

void sdlgpu_texture_set_wrap(Texture *tex, TextureWrapMode ws, TextureWrapMode wt) {
	UNREACHABLE;
}

void sdlgpu_texture_invalidate(Texture *tex) {
	tex->load.op = SDL_GPU_LOADOP_DONT_CARE;
}

void sdlgpu_texture_fill(Texture *tex, uint mipmap, uint layer, const Pixmap *image) {
	sdlgpu_texture_fill_region(tex, mipmap, layer, 0, 0, image);
}

void sdlgpu_texture_fill_region(Texture *tex, uint mipmap, uint layer, uint x, uint y, const Pixmap *image) {
	bool covers_whole =
		x == 0 && y == 0 && image->width >= tex->params.width && image->height >= tex->params.height;

	log_debug("Target: %p [%s] x:%d y:%d w:%d h:%d%s", tex, tex->debug_label, x, y, image->width, image->height, covers_whole ? " (whole coverage)" : "");

	// TODO persistent transfer buffer for streamed textures
	SDL_GpuTransferBuffer *tbuf = SDL_GpuCreateTransferBuffer(sdlgpu.device, &(SDL_GpuTransferBufferCreateInfo) {
		.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		.sizeInBytes = image->data_size,
	});

	uint8_t *mapped = SDL_GpuMapTransferBuffer(sdlgpu.device, tbuf, SDL_FALSE);
	memcpy(mapped, image->data.untyped, image->data_size);
	SDL_GpuUnmapTransferBuffer(sdlgpu.device, tbuf);

	bool cycle = (bool)(tex->params.flags & TEX_FLAG_STREAM);

	if(!covers_whole && tex->load.op == SDL_GPU_LOADOP_CLEAR) {
		sdlgpu_stop_current_pass(CBUF_UPLOAD);
		SDL_GpuEndRenderPass(SDL_GpuBeginRenderPass(sdlgpu.frame.upload_cbuf,
			&(SDL_GpuColorAttachmentInfo) {
				.clearColor = tex->load.clear.color.sdl_fcolor,
				.loadOp = SDL_GPU_LOADOP_CLEAR,
				.storeOp = SDL_GPU_STOREOP_STORE,
				.texture = tex->gpu_texture,
				.mipLevel = mipmap,
				.layerOrDepthPlane = layer,
				.cycle = cycle,
			}, 1, NULL));
	}

	if(tex->load.op == SDL_GPU_LOADOP_DONT_CARE) {
		covers_whole = true;
	}

	SDL_GpuCopyPass *copy_pass = sdlgpu_begin_or_resume_copy_pass(CBUF_UPLOAD);

	uint tex_h;
	sdlgpu_texture_get_size(tex, mipmap, NULL, &tex_h);

	SDL_GpuUploadToTexture(copy_pass,
		&(SDL_GpuTextureTransferInfo) {
			.transferBuffer = tbuf,
		},
		&(SDL_GpuTextureRegion) {
			.texture = tex->gpu_texture,
			.x = x,
			.y = tex_h - y - image->height,
			.w = image->width,
			.h = image->height,
			.d = 1,
			.layer = layer,
			.mipLevel = mipmap,
		},
		covers_whole && cycle);

	sdlgpu_texture_taint(tex);
	tex->load.op = SDL_GPU_LOADOP_LOAD;

	SDL_GpuReleaseTransferBuffer(sdlgpu.device, tbuf);
}

void sdlgpu_texture_prepare(Texture *tex) {
	if(tex->params.mipmap_mode == TEX_MIPMAP_AUTO && tex->mipmaps_outdated) {
		log_debug("Generating mipmaps for %p (%s)", tex, tex->debug_label);
		sdlgpu_stop_current_pass(CBUF_DRAW);
		SDL_GpuGenerateMipmaps(sdlgpu.frame.cbuf, tex->gpu_texture);
		tex->mipmaps_outdated = false;
	}
}

void sdlgpu_texture_taint(Texture *tex) {
	tex->mipmaps_outdated = true;
	tex->is_virgin = false;
}

void sdlgpu_texture_clear(Texture *tex, const Color *clr) {
	// FIXME add depth parameter
	tex->load.op = SDL_GPU_LOADOP_CLEAR;
	tex->load.clear.color = *clr;

#if 0
	Framebuffer *temp_fb = sdlgpu_framebuffer_create();
	sdlgpu_framebuffer_attach(temp_fb, tex, 0, FRAMEBUFFER_ATTACH_COLOR0);
	sdlgpu_framebuffer_clear(temp_fb, BUFFER_COLOR, clr, 1);
	sdlgpu_framebuffer_destroy(temp_fb);
	tex->mipmaps_outdated = true;
#endif
}

void sdlgpu_texture_destroy(Texture *tex) {
	if(SDL_AtomicDecRef(&tex->refs)) {
		log_debug("Texture %p (%s) is being destroyed!", tex, tex->debug_label);

		assert(!strstartswith(tex->debug_label, "<NULL TEXTURE"));

		SDL_GpuReleaseTexture(sdlgpu.device, tex->gpu_texture);
		SDL_GpuReleaseSampler(sdlgpu.device, tex->sampler);
		mem_free(tex);
	}
}

bool sdlgpu_texture_type_query(
	TextureType type, TextureFlags flags, PixmapFormat pxfmt, PixmapOrigin pxorigin,
	TextureTypeQueryResult *result
) {
	type = sdlgpu_remap_texture_type(type);
	SDL_GpuTextureFormat format = sdlgpu_texfmt_ts2sdl(type);

	if(flags & TEX_FLAG_SRGB) {
		format = sdlgpu_texfmt_to_srgb(format);
	}

	if(format == SDL_GPU_TEXTUREFORMAT_INVALID) {
		return false;
	}

	PixmapFormat pixfmt = sdlgpu_texfmt_to_pixfmt(format);

	if(pixfmt == 0) {
		// FIXME: supported but can't upload or download, what to do here?
		return false;
	}

	if(!result) {
		return true;
	}

	result->optimal_pixmap_format = pixfmt;
	result->optimal_pixmap_origin = PIXMAP_ORIGIN_BOTTOMLEFT;

	result->supplied_pixmap_origin_supported = (pxorigin == result->optimal_pixmap_origin);
	result->supplied_pixmap_format_supported = (pxfmt == result->optimal_pixmap_format);

	return true;
}

bool sdlgpu_texture_sampler_compatible(Texture *tex, UniformType sampler_type) {
	switch(tex->params.class) {
		case TEXTURE_CLASS_CUBEMAP:	return sampler_type == UNIFORM_SAMPLER_CUBE;
		case TEXTURE_CLASS_2D:		return sampler_type == UNIFORM_SAMPLER_2D;
		default: UNREACHABLE;
	}
}

bool sdlgpu_texture_dump(Texture *tex, uint mipmap, uint layer, Pixmap *dst) {
	UNREACHABLE;
}

bool sdlgpu_texture_transfer(Texture *dst, Texture *src) {
	UNREACHABLE;
}

static SDL_GpuCopyPass *sdlgpu_texture_copy_blit(
	SDL_GpuCopyPass *copy_pass,
	TextureSlice *dst,
	TextureSlice *src,
	bool cycle
) {
	UNREACHABLE;  // TODO
}

SDL_GpuCopyPass *sdlgpu_texture_copy(
	SDL_GpuCopyPass *copy_pass,
	TextureSlice *dst,
	TextureSlice *src,
	bool cycle
) {
	if(src->texture->gpu_format != dst->texture->gpu_format) {
		return sdlgpu_texture_copy_blit(copy_pass, dst, src, cycle);
	}

	uint dw, dh, sw, sh;
	r_texture_get_size(dst->texture, dst->mip_level, &dw, &dh);
	r_texture_get_size(src->texture, src->mip_level, &sw, &sh);

	if(dw != sw || dh != sh) {
		return sdlgpu_texture_copy_blit(copy_pass, dst, src, cycle);
	}

	if(!copy_pass) {
		copy_pass = sdlgpu_begin_or_resume_copy_pass(CBUF_UPLOAD);
	}

	assert(src->texture->params.layers == dst->texture->params.layers);

	SDL_GpuCopyTextureToTexture(copy_pass,
		&(SDL_GpuTextureLocation) {
			.texture = src->texture->gpu_texture,
			.mipLevel = src->mip_level,
			.layer = 0,
		}, &(SDL_GpuTextureLocation) {
			.texture = dst->texture->gpu_texture,
			.mipLevel = dst->mip_level,
			.layer = 0,
		}, dw, dh, dst->texture->params.layers, cycle);

	return copy_pass;
}