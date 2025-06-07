#pragma once


namespace RFlowey {

  using page_id_t = long;
  using frame_id_t = long;

  using hash_t = unsigned long long;

  constexpr float SPLIT_RATE = 3.0 / 4;
  constexpr float MERGE_RATE = 1.0 / 4;
  using index_type = unsigned long;
  constexpr int PAGESIZE = 4096;
  constexpr page_id_t INVALID_PAGE_ID=-1;
  constexpr size_t LRU_K = 5;
  constexpr size_t DISK_PAGE_CONFIG_ID=1;
  //Global manager for Disk(unused)
  //inline IOManager* manager;

}