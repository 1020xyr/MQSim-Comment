#include "Data_Cache_Flash.h"
#include <assert.h>


namespace SSD_Components
{
	Data_Cache_Flash::Data_Cache_Flash(unsigned int capacity_in_pages) : capacity_in_pages(capacity_in_pages) {}
	bool Data_Cache_Flash::Exists(const stream_id_type stream_id, const LPA_type lpn)
	{
		// 地址需进行转换
		LPA_type key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
		auto it = slots.find(key);
		// 未找到
		if (it == slots.end()) {
			return false;
		}

		return true;
	}

	Data_Cache_Flash::~Data_Cache_Flash()
	{
		for (auto &slot : slots) {
			delete slot.second;
		}
	}

	Data_Cache_Slot_Type Data_Cache_Flash::Get_slot(const stream_id_type stream_id, const LPA_type lpn)
	{
		LPA_type key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
		auto it = slots.find(key);
		assert(it != slots.end());
		// 访问过后将该页移至LRU队列MRU端
		if (lru_list.begin()->first != key) {
			lru_list.splice(lru_list.begin(), lru_list, it->second->lru_list_ptr);
		}

		return *(it->second);
	}

	bool Data_Cache_Flash::Check_free_slot_availability()
	{
		return slots.size() < capacity_in_pages;
	}

	bool Data_Cache_Flash::Check_free_slot_availability(unsigned int no_of_slots)
	{
		return slots.size() + no_of_slots <= capacity_in_pages;
	}

	bool Data_Cache_Flash::Empty()
	{
		return slots.size() == 0;
	}

	bool Data_Cache_Flash::Full()
	{
		return slots.size() == capacity_in_pages;
	}
	// 实际上这个方法从来没用过
	Data_Cache_Slot_Type Data_Cache_Flash::Evict_one_dirty_slot()
	{
		assert(slots.size() > 0);
		// auto自动判断变量类型
		auto itr = lru_list.rbegin();
		while (itr != lru_list.rend()) {
			if ((*itr).second->Status == Cache_Slot_Status::DIRTY_NO_FLASH_WRITEBACK) {
				break;
			}
			itr++;
		}
		// 实际上我并没有看懂为啥直接淘汰队列末尾元素，方法应该需要淘汰脏页
		Data_Cache_Slot_Type evicted_item = *lru_list.back().second;
		// 未找到脏页
		if (itr == lru_list.rend()) {
			evicted_item.Status = Cache_Slot_Status::EMPTY;
			return evicted_item;
		}

		slots.erase(lru_list.back().first);
		delete lru_list.back().second;
		lru_list.pop_back();
		
		return evicted_item;
	}

	Data_Cache_Slot_Type Data_Cache_Flash::Evict_one_slot_lru()
	{
		assert(slots.size() > 0);
		slots.erase(lru_list.back().first);
		Data_Cache_Slot_Type evicted_item = *lru_list.back().second;
		delete lru_list.back().second;
		lru_list.pop_back();

		return evicted_item;
	}

	void Data_Cache_Flash::Change_slot_status_to_writeback(const stream_id_type stream_id, const LPA_type lpn)
	{
		LPA_type key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
		auto it = slots.find(key);
		assert(it != slots.end());
		it->second->Status = Cache_Slot_Status::DIRTY_FLASH_WRITEBACK;
	}

	void Data_Cache_Flash::Insert_read_data(const stream_id_type stream_id, const LPA_type lpn, const data_cache_content_type content,
		const data_timestamp_type timestamp, const page_status_type state_bitmap_of_read_sectors)
	{
		LPA_type key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
		
		if (slots.find(key) != slots.end()) {
			throw std::logic_error("Duplicate lpn insertion into data cache!");
		}
		if (slots.size() >= capacity_in_pages) {
			throw std::logic_error("Data cache overfull!");
		}

		Data_Cache_Slot_Type* cache_slot = new Data_Cache_Slot_Type();
		cache_slot->LPA = lpn;
		cache_slot->State_bitmap_of_existing_sectors = state_bitmap_of_read_sectors;
		cache_slot->Content = content;
		cache_slot->Timestamp = timestamp;
		cache_slot->Status = Cache_Slot_Status::CLEAN;
		lru_list.push_front(std::pair<LPA_type, Data_Cache_Slot_Type*>(key, cache_slot));
		// 给lru_list_ptr赋值
		cache_slot->lru_list_ptr = lru_list.begin();
		slots[key] = cache_slot;
	}

	void Data_Cache_Flash::Insert_write_data(const stream_id_type stream_id, const LPA_type lpn, const data_cache_content_type content,
		const data_timestamp_type timestamp, const page_status_type state_bitmap_of_write_sectors)
	{
		LPA_type key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
		
		if (slots.find(key) != slots.end()) {
			throw std::logic_error("Duplicate lpn insertion into data cache!!");
		}
		
		if (slots.size() >= capacity_in_pages) {
			throw std::logic_error("Data cache overfull!");
		}

		Data_Cache_Slot_Type* cache_slot = new Data_Cache_Slot_Type();
		cache_slot->LPA = lpn;
		cache_slot->State_bitmap_of_existing_sectors = state_bitmap_of_write_sectors;
		cache_slot->Content = content;
		cache_slot->Timestamp = timestamp;
		// 和read方法唯一的不同，缓存页的状态为脏页未写回
		cache_slot->Status = Cache_Slot_Status::DIRTY_NO_FLASH_WRITEBACK;
		lru_list.push_front(std::pair<LPA_type, Data_Cache_Slot_Type*>(key, cache_slot));
		cache_slot->lru_list_ptr = lru_list.begin();
		slots[key] = cache_slot;
	}

	void Data_Cache_Flash::Update_data(const stream_id_type stream_id, const LPA_type lpn, const data_cache_content_type content,
		const data_timestamp_type timestamp, const page_status_type state_bitmap_of_write_sectors)
	{
		LPA_type key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
		auto it = slots.find(key);
		assert(it != slots.end());

		it->second->LPA = lpn;
		it->second->State_bitmap_of_existing_sectors = state_bitmap_of_write_sectors;
		it->second->Content = content;
		it->second->Timestamp = timestamp;
		it->second->Status = Cache_Slot_Status::DIRTY_NO_FLASH_WRITEBACK;
		// 移至MRU端
		if (lru_list.begin()->first != key) {
			lru_list.splice(lru_list.begin(), lru_list, it->second->lru_list_ptr);
		}
	}

	void Data_Cache_Flash::Remove_slot(const stream_id_type stream_id, const LPA_type lpn)
	{
		LPA_type key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
		auto it = slots.find(key);
		assert(it != slots.end());
		lru_list.erase(it->second->lru_list_ptr);
		delete it->second;
		slots.erase(it);
	}
}
