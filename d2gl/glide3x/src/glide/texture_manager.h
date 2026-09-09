/*
	D2GL: Diablo 2 LoD Glide/DDraw to OpenGL Wrapper.
	Copyright (C) 2023  Bayaraa

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>
#include <glm/vec2.hpp>

namespace d2gl {

struct SubTextureInfo {
	uint8_t shift;
	uint16_t tex_num;
	glm::vec<2, uint16_t> offset;
};

struct TextureCache {
	uint32_t last_used_frame = 0;
	std::unordered_map<uint64_t, uint16_t> items;
};

struct TextureManagerData {
	uint16_t tex_count = 0;
	std::vector<SubTextureInfo> sub_texure_info;
	std::unordered_map<uint16_t, bool> available;
	std::unordered_map<uint64_t, TextureCache> cache;
};

typedef std::vector<std::pair<uint16_t, uint16_t>> SubTextureCounts;

struct GlideTexture {
	uint8_t* memory = nullptr;
	std::map<uint32_t, uint32_t> hash;
};

extern GlideTexture g_glide_texture;

struct TextureCacheStats {
	uint64_t reclaimed_slots = 0;
	uint64_t exhausted = 0;
	uint64_t missing_source = 0;
	uint64_t immutable_uploads = 0;
	uint64_t immutable_hits = 0;
	uint64_t invalid_immutable = 0;
};

class TextureManager {
	using Upload = std::function<void(uint8_t*, const SubTextureInfo&, uint16_t, uint16_t)>;
	std::map<uint16_t, TextureManagerData> m_data;
	SubTextureCounts m_size_counts;
	Upload m_upload;
	TextureCacheStats m_stats;
	const SubTextureInfo* acquire(uint64_t source, uint64_t content, uint16_t size, uint32_t frame_count,
		const std::function<bool(const SubTextureInfo&)>& upload);

public:
	TextureManager(const SubTextureCounts& size_counts, Upload upload);
	~TextureManager() = default;

	inline size_t getUsage(uint16_t size) { return m_data[size].tex_count - m_data[size].available.size(); }
	const TextureCacheStats& stats() const { return m_stats; }

	const SubTextureInfo* getSubTextureInfo(uint32_t address, uint16_t size, uint16_t width, uint16_t height, uint32_t frame_count);
	const SubTextureInfo* getImmutableSubTextureInfo(uint32_t identity, uint16_t width, uint16_t height,
		uint32_t frame_count, const std::function<bool(uint8_t*)>& decode);
	void clearCache();
};

}
