#pragma once

#include <string>

using Time_t = int;
namespace TimeUtil {

    // --- Type Alias ---


    // --- Constants ---
    const int REF_YEAR = 2025;
    const int REF_MONTH = 5; // 1-indexed (June)
    const int REF_DAY = 1;   // 1-indexed

    const int MINUTES_IN_HOUR = 60;
    const int HOURS_IN_DAY = 24;
    const int MINUTES_IN_DAY = MINUTES_IN_HOUR * HOURS_IN_DAY; // 1440

    constexpr int Minute = 1;
    constexpr int Hour = 60 * Minute;
    constexpr int Day = 24 * Hour;

    // Days in month for 2025 (not a leap year). Index 0 unused, 1=Jan, ..., 12=Dec.
    constexpr int DAYS_IN_MONTH_2025[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // Reference Epoch: 2025-06-01 00:00:00.
    // All "total_minutes_since_ref" are relative to this epoch.
    // All "day_index" are days since this epoch (2025-06-01 is day_index 0).
    // Valid minutes are 0 to END_OF_2025_EXCLUSIVE_MINUTES - 1.
    constexpr Time_t calculate_end_of_2025_exclusive_minutes() {
        Time_t days_in_scope = 0;
        // Days in June (from REF_DAY)
        days_in_scope += (DAYS_IN_MONTH_2025[REF_MONTH] - REF_DAY + 1);
        // Full months July to December
        for (int m = REF_MONTH + 1; m <= 12; ++m) {
            days_in_scope += DAYS_IN_MONTH_2025[m];
        }
        return days_in_scope * MINUTES_IN_DAY;
    }
    constexpr Time_t END_OF_2025_EXCLUSIVE_MINUTES = calculate_end_of_2025_exclusive_minutes(); // e.g., 214 * 1440 = 308160

    // --- Low-Level Parsing and Formatting Helpers (Declarations) ---
    bool parseMMDD(const std::string& date_str, int& month_out, int& day_out);
    bool parseHHMM(const std::string& time_str, int& hour_out, int& minute_out);
    void formatMMDD(int month, int day, std::string& out_str);
    void formatHHMM(int hour, int minute, std::string& out_str);

    // --- Core Conversion Functions (Declarations) ---
    // These are free functions in the TimeUtil namespace.

    /**
     * @brief Converts total minutes since ref epoch to "MM-DD" and "hh:mm" strings.
     * @return True if successful, false if total_minutes is out of 2025 scope.
     */
    bool stringsFromMinutes(Time_t total_minutes_since_ref,
                            std::string& date_str_out,
                            std::string& time_str_out);

    /**
     * @brief Converts "MM-DD" and "hh:mm" strings (for 2025) to total minutes since ref epoch.
     * @return Minutes since epoch, or -1 if parsing fails or date is invalid/before epoch.
     */
    Time_t minutesFromStrings(const std::string& date_str, const std::string& time_str);

    /**
     * @brief Converts a day index (days since 2025-06-01) to an "MM-DD" string.
     * @return True if successful, false if day_index is negative or out of 2025 scope.
     */
    bool dateStringFromDayIndex(int day_index, std::string& date_str_out);

    /**
     * @brief Converts an "MM-DD" string (for 2025) to a day index.
     * @return Day index, or -1 if parsing fails or date is before epoch/invalid for 2025.
     */
    int dayIndexFromDateString(const std::string& date_str);

    /**
     * @brief Converts an "hh:mm" string to minutes since midnight.
     * @return Minutes since midnight (0-1439), or -1 if parsing fails or time is invalid.
     */
    int minutesInDayFromTimeString(const std::string& time_str);

    /**
     * @brief Converts minutes since midnight to an "hh:mm" string.
     * @return True if successful, false if minutes_in_day is invalid.
     */
    bool timeStringFromMinutesInDay(int minutes_in_day, std::string& time_str_out);


    // --- DateTime Struct ---
    struct DateTime {
    private:
        Time_t total_minutes_since_ref_;

        // Helper to extract components. Assumes total_minutes_since_ref_ is valid.
        void get_components_unsafe(int& year_out, int& month_out, int& day_out, int& hour_out, int& minute_out) const;

    public:
        // Constructors
        DateTime(); // Default: Invalid DateTime
        explicit DateTime(Time_t minutes_since_epoch);
        DateTime(const std::string& date_str, const std::string& time_str = "00:00");

        // Validity check
        bool isValid() const;

        // Accessors
        int year() const;
        int month() const; // 1-indexed
        int day() const;   // 1-indexed
        int hour() const;  // 0-23
        int minute() const;// 0-59

        // Formatting
        std::string getDateString() const; // "MM-DD"
        std::string getTimeString() const; // "hh:mm"
        std::string getFullString() const;

        // Arithmetic
        DateTime operator+(int minutes_to_add) const;

        DateTime operator-(int minutes_to_subtract) const;
        Time_t operator-(const DateTime& other) const; // Difference in minutes

        // Comparison
        bool operator<(const DateTime& other) const;
        bool operator==(const DateTime& other) const;
        bool operator!=(const DateTime& other) const;
        bool operator<=(const DateTime& other) const;
        bool operator>(const DateTime& other) const;
        bool operator>=(const DateTime& other) const;

        // Raw Access
        Time_t getRawMinutes() const;

        DateTime roundDownToDate() const;

        DateTime roundUpToDate() const;
    };

} // namespace TimeUtil