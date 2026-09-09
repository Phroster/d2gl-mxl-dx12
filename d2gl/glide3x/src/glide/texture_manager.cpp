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

#include "texture_manager.h"
#include <algorithm>
#include <array>

namespace d2gl {

GlideTexture g_glide_texture;

TextureManager::TextureManager(const SubTextureCounts& size_counts, Upload upload)
	: m_size_counts(size_counts), m_upload(std::move(upload))
{
	uint16_t tex_start = 0;

	for (auto& size_count : size_counts) {
		auto size = size_count.first;
		auto count = size_count.second;

		uint16_t div = 512 / size;
		uint16_t tex_count = count * div * div;

		uint8_t shift = 0;
		switch (size) {
			case 8: shift = 5; break;
			case 16: shift = 4; break;
			case 32: shift = 3; break;
			case 64: shift = 2; break;
			case 128: shift = 1; break;
		}

		m_data.insert({ size, {} });
		m_data[size].sub_texure_info.assign(tex_count + 1, { 0 });
		m_data[size].tex_count = tex_count;
		m_data[size].available.reserve(tex_count);
		m_data[size].cache.reserve(tex_count);

		for (uint16_t i = 0; i < count; i++) {
			uint16_t n = i * div * div;
			uint16_t tex_num = tex_start + i;

			for (uint16_t y = 0; y < div; y++) {
				uint16_t ny = y * div;

				for (uint16_t x = 0; x < div; x++) {
					uint16_t ix = n + ny + x + 1;

					m_data[size].sub_texure_info[ix].tex_num = tex_num;
					m_data[size].sub_texure_info[ix].offset = { x * size, y * size };
					m_data[size].sub_texure_info[ix].shift = shift;

					m_data[size].available[ix] = true;
				}
			}
		}

		tex_start += count;
	}
}

const SubTextureInfo* TextureManager::getSubTextureInfo(uint32_t address, uint16_t size, uint16_t width, uint16_t height, uint32_t frame_count)
{
	if (g_glide_texture.hash.find(address) == g_glide_texture.hash.end()) {
		++m_stats.missing_source;
		return nullptr;
	}

	// Native cells reuse addresses across aspect ratios. Equal bytes do not
	// imply equal textures: a 256x128 upload cannot back a 128x256 draw. In
	// particular transparent animation frames otherwise reveal old pixels in
	// the part of the atlas slot the previous rectangle never initialized.
	const uint64_t hash = uint64_t(g_glide_texture.hash[address])
		| (uint64_t(width) << 32) | (uint64_t(height) << 48);
	return acquire(address, hash, size, frame_count, [&](const SubTextureInfo& slot) {
		m_upload(g_glide_texture.memory + address, slot, width, height);
		return true;
	});
}

const SubTextureInfo* TextureManager::getImmutableSubTextureInfo(uint32_t identity, uint16_t width, uint16_t height,
	uint32_t frame_count, const std::function<bool(uint8_t*)>& decode)
{
	const auto before = m_stats.immutable_uploads;
	// A separate identity namespace keeps native animation frames independent
	// of every virtual address/content the game's texture driver reuses.
	const auto slot = acquire((uint64_t(1) << 32) | identity,
		(uint64_t(width) << 32) | (uint64_t(height) << 48), std::max(width, height), frame_count,
		[&](const SubTextureInfo& target) {
			std::array<uint8_t, 256 * 256> pixels;
			if (!decode(pixels.data())) { ++m_stats.invalid_immutable; return false; }
			m_upload(pixels.data(), target, width, height);
			++m_stats.immutable_uploads;
			return true;
		});
	if (slot && before == m_stats.immutable_uploads) ++m_stats.immutable_hits;
	return slot;
}

const SubTextureInfo* TextureManager::acquire(uint64_t address, uint64_t hash, uint16_t size, uint32_t frame_count,
	const std::function<bool(const SubTextureInfo&)>& upload)
{
	auto& data = m_data[size];

	auto [entry, inserted] = data.cache.try_emplace(address);
	auto& cache = entry->second;
	if (inserted) {
		data.recency.push_front(address);
		cache.recency = data.recency.begin();
		cache.last_used_frame = frame_count;
	}

	if (cache.last_used_frame != frame_count) {
		for (auto it = cache.items.begin(); it != cache.items.end();) {
			if ((*it).first != hash) {
				data.available[(*it).second] = true;
				it = cache.items.erase(it);
			} else
				it++;
		}
		cache.last_used_frame = frame_count;
		data.recency.splice(data.recency.begin(), data.recency, cache.recency);
	}
	// Failed decodes and overfull frames must not accumulate empty identities.
	const auto discardEmpty = [&] {
		if (cache.items.empty()) {
			data.recency.erase(cache.recency);
			data.cache.erase(address);
		}
	};

	if (cache.items.find(hash) == cache.items.end()) {
		while (data.available.empty() && !data.recency.empty()) {
			// Reclaim only the oldest retired source, not the whole previous
			// frame. Loot draws before scenery; bulk eviction otherwise forces
			// hot scenery to upload again on each animation-cell change.
			const auto oldest = data.cache.find(data.recency.back());
			// First use moves a source to the front once per frame. If the
			// oldest is live, every source is pinned by this frame's queued draws.
			if (oldest->second.last_used_frame == frame_count) break;
			for (const auto& item : oldest->second.items) {
				data.available[item.second] = true;
				++m_stats.reclaimed_slots;
			}
			data.recency.pop_back();
			data.cache.erase(oldest);
		}
		if (data.available.empty()) {
			++m_stats.exhausted;
			discardEmpty();
			return nullptr;
		}

		const auto id = data.available.begin()->first;
		const SubTextureInfo* texture_info = &data.sub_texure_info[id];
		if (!upload(*texture_info)) { discardEmpty(); return nullptr; }

		cache.items.insert({ hash, id });
		data.available.erase(id);

		return texture_info;
	}

	return &data.sub_texure_info[cache.items[hash]];
}

void TextureManager::clearCache()
{
	for (auto& size_count : m_size_counts) {
		auto size = size_count.first;
		auto& data = m_data[size];

		for (auto& cache : data.cache) {
			for (auto& item : cache.second.items) {
				data.available[item.second] = true;
			}
		}
		data.cache.clear();
		data.recency.clear();
	}
}

}
