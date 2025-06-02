#ifndef TIME_UTIL_HPP
#define TIME_UTIL_HPP

#include <string> // Allowed STL container
// <cstdio> for sprintf/sscanf if allowed, otherwise manual parsing/formatting.
// We'll assume manual parsing/formatting using std::string methods for stricter adherence.

namespace TimeUtil {

    // --- Constants ---
    // Reference Epoch: 2025-06-01 00:00:00
    // All "total_minutes_since_ref" are relative to this epoch.
    // All "day_index" are days since this epoch (2025-06-01 is day_index 0).

    const int REF_YEAR = 2025;
    const int REF_MONTH = 6; // 1-indexed (June)
    const int REF_DAY = 1;   // 1-indexed

    const int MINUTES_IN_HOUR = 60;
    const int HOURS_IN_DAY = 24;
    const int MINUTES_IN_DAY = MINUTES_IN_HOUR * HOURS_IN_DAY; // 1440

    // Days in month for 2025 (not a leap year). Index 0 unused, 1=Jan, ..., 12=Dec.
    // This array is crucial for date calculations.
    const int DAYS_IN_MONTH_2025[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // --- Core Conversion Functions ---

    /**
     * @brief Converts total minutes since the reference epoch (2025-06-01 00:00)
     *        into separate "MM-DD" date and "hh:mm" time strings.
     * @param total_minutes_since_ref Integer minutes since the epoch.
     * @param date_str_out Output string for the date part (e.g., "06-15").
     * @param time_str_out Output string for the time part (e.g., "14:30").
     * @return True if conversion is successful, false otherwise (e.g., negative minutes).
     */
    bool minutesToDateTimeStrings(int total_minutes_since_ref,
                                  std::string& date_str_out,
                                  std::string& time_str_out);

    /**
     * @brief Converts "MM-DD" date and "hh:mm" time strings (assumed to be for the year 2025)
     *        into total minutes since the reference epoch (2025-06-01 00:00).
     * @param date_str Input "MM-DD" string (e.g., "06-15").
     * @param time_str Input "hh:mm" string (e.g., "14:30").
     * @return Integer minutes since the epoch. Returns -1 if parsing fails or date is invalid/before epoch.
     */
    int dateTimeStringsToMinutes(const std::string& date_str, const std::string& time_str);

    // --- Day Index and Date String Conversions (Relative to Epoch) ---

    /**
     * @brief Converts a day index (number of days since 2025-06-01) to an "MM-DD" string.
     *        Day 0 corresponds to 2025-06-01.
     * @param day_index Non-negative integer representing days since the epoch.
     * @param date_str_out Output "MM-DD" string.
     * @return True if conversion is successful, false otherwise (e.g., negative day_index).
     */
    bool dayIndexToDateString(int day_index, std::string& date_str_out);

    /**
     * @brief Converts an "MM-DD" string (assumed for year 2025) into a day index
     *        (number of days since 2025-06-01).
     * @param date_str Input "MM-DD" string.
     * @return Non-negative integer day index. Returns -1 if parsing fails or date is before epoch.
     */
    int dateStringToDayIndex(const std::string& date_str);

    // --- Time of Day Conversions (Within a Single Day) ---

    /**
     * @brief Converts an "hh:mm" time string to minutes since midnight (0 to 1439).
     * @param time_str Input "hh:mm" string (e.g., "08:30").
     * @return Integer minutes since midnight. Returns -1 if parsing fails or time is invalid.
     */
    int timeStringToMinutesInDay(const std::string& time_str);

    /**
     * @brief Converts minutes since midnight (0 to 1439) to an "hh:mm" time string.
     * @param minutes_in_day Integer minutes since midnight.
     * @param time_str_out Output "hh:mm" string.
     * @return True if conversion is successful, false otherwise (e.g., invalid minutes_in_day).
     */
    bool minutesInDayToTimeString(int minutes_in_day, std::string& time_str_out);

    // --- Parsing and Formatting Helpers (Internal or for specific needs) ---
    // These are used by the above functions. They could be in an anonymous namespace in the .cpp.

    /**
     * @brief Parses an "MM-DD" string into month and day integers.
     * @param date_str Input "MM-DD" string.
     * @param month_out Output integer for month (1-12).
     * @param day_out Output integer for day (1-31).
     * @return True if parsing is successful and basic validation passes, false otherwise.
     */
    bool parseMMDD(const std::string& date_str, int& month_out, int& day_out);

    /**
     * @brief Parses an "hh:mm" string into hour and minute integers.
     * @param time_str Input "hh:mm" string.
     * @param hour_out Output integer for hour (0-23).
     * @param minute_out Output integer for minute (0-59).
     * @return True if parsing is successful and basic validation passes, false otherwise.
     */
    bool parseHHMM(const std::string& time_str, int& hour_out, int& minute_out);

    /**
     * @brief Formats month and day integers into an "MM-DD" string.
     * @param month Month (1-12).
     * @param day Day (1-31).
     * @param out_str Output "MM-DD" string.
     */
    void formatMMDD(int month, int day, std::string& out_str);

    /**
     * @brief Formats hour and minute integers into an "hh:mm" string.
     * @param hour Hour (0-23).
     * @param minute Minute (0-59).
     * @param out_str Output "hh:mm" string.
     */
    void formatHHMM(int hour, int minute, std::string& out_str);

} // namespace TimeUtil

#endif // TIME_UTIL_HPP