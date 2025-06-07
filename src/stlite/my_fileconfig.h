#pragma once

#include <fstream>
#include <string>
#include <filesystem>
#include <stdexcept>
#include <iostream>
#include <utility>

namespace RFlowey {

  /**
   * @class FiledConfig
   * @brief An RAII helper class for storing unstructured config data using only std library components.
   */
  class FiledConfig {
  private:
    // Using std::streampos for file positions, which is typical for fstream.
    using pos_t_ = std::streampos; 
    template <typename val_t_> struct RAII_Tracker;

    static FiledConfig &get_instance() {
      // Meyer's Singleton
      static FiledConfig instance;
      return instance;
    }

    // Static file path, initialized inline (C++17)
    inline static std::string file_path = "./persistent_std.config";
    // Static flag to control whether to ignore persisted values (newly added)
    inline static bool ignore_persisted_values_s = false; 
    
    std::fstream fconfig;
    bool write_only; // True if the file was empty before any writes by this instance
    pos_t_ global_cur = 0; // Current global byte offset in the file

    FiledConfig() {
      std::filesystem::path p(file_path);
      if (p.has_parent_path()) {
          std::filesystem::create_directories(p.parent_path());
      }

      bool file_existed = std::filesystem::exists(file_path);
      
      fconfig.open(file_path, std::ios::binary | std::ios::in | std::ios::out | std::ios::ate);

      if (!fconfig.is_open()) {
        fconfig.open(file_path, std::ios::binary | std::ios::out); 
        fconfig.close();
        fconfig.open(file_path, std::ios::binary | std::ios::in | std::ios::out | std::ios::ate);
      }

      if (!fconfig.is_open()) {
        throw std::runtime_error("FiledConfig: Failed to open or create config file: " + file_path);
      }

      pos_t_ initial_size = 0;
      if (file_existed) {
          initial_size = fconfig.tellg(); 
      }
      
      write_only = (!file_existed || initial_size == 0);

      try {
        // If the file is smaller than 4096, resize_file expands it (usually zero-filled).
        // If it's larger, it truncates.
        // We only want to expand, not truncate if it's already larger and potentially valid.
        // However, the original code implies a fixed-size or minimum-size region.
        // For simplicity and matching original intent of resize, we keep it.
        // If the file was smaller than 4096, it will be expanded.
        // If it was larger, it will be truncated to 4096.
        // If this truncation is undesirable, the logic here would need to change,
        // e.g., only resize if current size < 4096.
        if (std::filesystem::exists(file_path)) { // resize_file needs the file to exist
            pos_t_ current_size_before_resize = std::filesystem::file_size(file_path);
            if (current_size_before_resize < 4096) {
                 std::filesystem::resize_file(file_path, 4096);
            }
            // If current_size_before_resize >= 4096, we do nothing, preserving existing larger content.
            // The original code would truncate here. This is a slight deviation to prevent data loss
            // if the file is intentionally larger. If strict 4096 is needed, revert to unconditional resize.
        } else {
            // File didn't exist, resize_file will create it with the specified size.
            // This path might not be hit if open already created it, but good for robustness.
            std::filesystem::resize_file(file_path, 4096);
        }

      } catch (const std::filesystem::filesystem_error& e) {
        fconfig.close();
        throw std::runtime_error(std::string("FiledConfig: Failed to resize file: ") + file_path + " (" + e.what() + ")");
      }
      
      fconfig.seekg(0, std::ios::beg);
      fconfig.seekp(0, std::ios::beg);

      if (!fconfig.good()) {
        fconfig.close();
        throw std::runtime_error("FiledConfig: Stream in bad state after opening/resizing file: " + file_path);
      }
    }

    ~FiledConfig() {
      if (fconfig.is_open()) {
        fconfig.flush(); 
        fconfig.close();
      }
    }

    FiledConfig(const FiledConfig &) = delete;
    FiledConfig &operator=(const FiledConfig &) = delete;
    FiledConfig(FiledConfig &&) = delete;
    FiledConfig &operator=(FiledConfig &&) = delete;

    template <typename val_t_> struct RAII_Tracker {
    private:
      pos_t_ cur_offset;
    public:
      val_t_ val;

      explicit RAII_Tracker(const val_t_ &default_value) : val(default_value) {
        auto &singleton = FiledConfig::get_instance();
        
        if (!singleton.fconfig.is_open() || !singleton.fconfig.good()) {
            throw std::runtime_error("RAII_Tracker: Config file not open or in bad state.");
        }

        cur_offset = singleton.global_cur;
        singleton.global_cur += static_cast<pos_t_>(sizeof(val_t_)); 

        // Check if we are trying to track beyond a reasonable initial size (e.g., 4096).
        // This is more of a sanity check/warning. The file can grow if written past the end.
        if (cur_offset + static_cast<pos_t_>(sizeof(val_t_)) > static_cast<pos_t_>(4096)) {
            // std::cerr << "Warning: Tracking data potentially beyond initial 4096 bytes at offset " << cur_offset << std::endl;
        }
        
        bool attempt_load = !singleton.write_only && !FiledConfig::ignore_persisted_values_s;

        if (attempt_load) {
          singleton.fconfig.seekg(cur_offset);
          if (singleton.fconfig.good()) { 
            singleton.fconfig.read(reinterpret_cast<char *>(&val), sizeof(val_t_));
            if (singleton.fconfig.gcount() != sizeof(val_t_)) {
              val = default_value; 
              singleton.fconfig.clear(); 
            }
          } else {
            singleton.fconfig.clear();
            // val remains default_value
          }
        }
        // If !attempt_load, 'val' remains 'default_value' and will be written on destruction.
      }

      ~RAII_Tracker() {
        try {
          auto &singleton = FiledConfig::get_instance();
          if (singleton.fconfig.is_open()) {
            singleton.fconfig.clear(); 
            singleton.fconfig.seekp(cur_offset);
            if (singleton.fconfig.good()) { 
                singleton.fconfig.write(reinterpret_cast<const char *>(&val), sizeof(val_t_));
                singleton.fconfig.flush(); 
                if(!singleton.fconfig.good()){
                    std::cerr << "FiledConfig: Error writing to config file at offset " << cur_offset << std::endl;
                }
            } else {
                 std::cerr << "FiledConfig: Error seeking to offset " << cur_offset << " for writing." << std::endl;
            }
          }
        } catch (const std::exception& e) {
          std::cerr << "FiledConfig: Exception in RAII_Tracker destructor: " << e.what() << std::endl;
        } catch (...) {
          std::cerr << "FiledConfig: Unknown exception in RAII_Tracker destructor." << std::endl;
        }
      }

      RAII_Tracker(const RAII_Tracker &) = delete;
      RAII_Tracker &operator=(const RAII_Tracker &) = delete;
      RAII_Tracker(RAII_Tracker &&) = delete; 
      RAII_Tracker &operator=(RAII_Tracker &&) = delete;
    };

  public:
    template <typename val_t_> 
    using tracker_t_ = RAII_Tracker<val_t_>;

    /**
     * @brief Sets the file path for the configuration.
     * @warning This should be called BEFORE the first call to track() or any operation
     *          that would instantiate the singleton. Otherwise, the singleton will be
     *          initialized with the default path.
     * @param path The new path to the configuration file.
     */
    static void set_file_path(const std::string &path) { 
        file_path = path; 
    }

    /**
     * @brief Sets a global flag to ignore any persisted values in the config file.
     * If set to true, all subsequently created trackers will initialize with their
     * default values, regardless of what's in the config file. They will still
     * write their values upon destruction. This is useful for testing or forcing
     * a reset to defaults.
     * @param ignore True to ignore persisted values and use defaults; false (default) to attempt loading.
     */
    static void set_ignore_persisted_values(bool ignore) {
        ignore_persisted_values_s = ignore;
    }

    /**
     * @brief Creates a tracker for a value of type val_t.
     * The value is initialized using the provided arguments. 
     * If the config file exists, contains data at the tracker's position, and 
     * `set_ignore_persisted_values` is false (or not called), that data is loaded.
     * Otherwise, the initialized value (default) is used. The current value
     * will be saved on the tracker's destruction.
     * @tparam val_t The type of the value to track. Must be trivially copyable.
     * @tparam Args Argument types for constructing val_t.
     * @param args Arguments to construct the default value of val_t.
     * @return An RAII_Tracker for the value.
     */
    template <typename val_t, typename... Args>
    static RAII_Tracker<val_t> track(Args &&...args) {
      return RAII_Tracker<val_t>(val_t(std::forward<Args>(args)...));
    }
  };

}