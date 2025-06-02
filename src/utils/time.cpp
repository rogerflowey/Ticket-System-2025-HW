#include "time.h"
#include <stdexcept>
namespace TimeUtil {

// --- Parsing and Formatting Helper Implementations ---

bool parseMMDD(const std::string& date_str, int& month_out, int& day_out) {
    if (date_str.length() != 5 || date_str[2] != '-') {
        return false;
    }
    std::string month_s = date_str.substr(0, 2);
    std::string day_s = date_str.substr(3, 2);

    // Check for non-digit characters manually if std::stoi is too lenient or to avoid exceptions
    for (char c : month_s) if (c < '0' || c > '9') return false;
    for (char c : day_s) if (c < '0' || c > '9') return false;

    month_out = std::stoi(month_s);
    day_out = std::stoi(day_s);

    if (month_out < 1 || month_out > 12 || day_out < 1 || day_out > DAYS_IN_MONTH_2025[month_out]) {
        return false;
    }
    return true;
}

bool parseHHMM(const std::string& time_str, int& hour_out, int& minute_out) {
    if (time_str.length() != 5 || time_str[2] != ':') {
        return false;
    }
    std::string hour_s = time_str.substr(0, 2);
    std::string minute_s = time_str.substr(3, 2);

    for (char c : hour_s) if (c < '0' || c > '9') return false;
    for (char c : minute_s) if (c < '0' || c > '9') return false;

    hour_out = std::stoi(hour_s);
    minute_out = std::stoi(minute_s);

    if (hour_out < 0 || hour_out > 23 || minute_out < 0 || minute_out > 59) {
        return false; // Invalid hour or minute
    }

    return true;
}

void formatMMDD(int month, int day, std::string& out_str) {
    out_str.clear();
    if (month < 10) out_str += '0';
    out_str += std::to_string(month);
    out_str += '-';
    if (day < 10) out_str += '0';
    out_str += std::to_string(day);
}

void formatHHMM(int hour, int minute, std::string& out_str) {
    out_str.clear();
    if (hour < 10) out_str += '0';
    out_str += std::to_string(hour);
    out_str += ':';
    if (minute < 10) out_str += '0';
    out_str += std::to_string(minute);
}


// --- Day Index and Date String Conversion Implementations ---

bool dayIndexToDateString(int day_index, std::string& date_str_out) {
    if (day_index < 0) return false;

    int current_month = REF_MONTH;
    int day_in_year_offset = day_index + (REF_DAY - 1); // 0-indexed day within REF_MONTH

    while (current_month <= 12 && day_in_year_offset >= DAYS_IN_MONTH_2025[current_month]) {
        day_in_year_offset -= DAYS_IN_MONTH_2025[current_month];
        current_month++;
    }

    if (current_month > 12) { // Spanned into next year, not handled by simple DAYS_IN_MONTH_2025
        return false; // Or extend logic for multi-year if needed
    }

    formatMMDD(current_month, day_in_year_offset + 1, date_str_out);
    return true;
}

int dateStringToDayIndex(const std::string& date_str) {
    int month, day;
    if (!parseMMDD(date_str, month, day)) {
        return -1;
    }

    // Check if date is before reference date (simplified for 2025)
    if (month < REF_MONTH || (month == REF_MONTH && day < REF_DAY)) {
        return -1;
    }

    int days_from_year_start_to_ref = 0;
    for (int m = 1; m < REF_MONTH; ++m) {
        days_from_year_start_to_ref += DAYS_IN_MONTH_2025[m];
    }
    days_from_year_start_to_ref += (REF_DAY - 1);

    int days_from_year_start_to_target = 0;
    for (int m = 1; m < month; ++m) {
        days_from_year_start_to_target += DAYS_IN_MONTH_2025[m];
    }
    days_from_year_start_to_target += (day - 1);

    int day_index = days_from_year_start_to_target - days_from_year_start_to_ref;
    return (day_index < 0) ? -1 : day_index; // Should not be < 0 due to earlier check
}

// --- Time of Day Conversion Implementations ---

int timeStringToMinutesInDay(const std::string& time_str) {
    int hour, minute;
    if (!parseHHMM(time_str, hour, minute)) {
        return -1;
    }
    return hour * MINUTES_IN_HOUR + minute;
}

bool minutesInDayToTimeString(int minutes_in_day, std::string& time_str_out) {
    if (minutes_in_day < 0 || minutes_in_day >= MINUTES_IN_DAY) {
        return false;
    }
    int hour = minutes_in_day / MINUTES_IN_HOUR;
    int minute = minutes_in_day % MINUTES_IN_HOUR;
    formatHHMM(hour, minute, time_str_out);
    return true;
}

// --- Core Conversion Function Implementations ---

bool minutesToDateTimeStrings(int total_minutes_since_ref,
                              std::string& date_str_out,
                              std::string& time_str_out) {
    if (total_minutes_since_ref < 0) {
        return false;
    }
    int day_idx = total_minutes_since_ref / MINUTES_IN_DAY;
    int minutes_in_day = total_minutes_since_ref % MINUTES_IN_DAY;

    if (!dayIndexToDateString(day_idx, date_str_out)) {
        return false; // Should not happen if day_idx is non-negative
    }
    if (!minutesInDayToTimeString(minutes_in_day, time_str_out)) {
        return false; // Should not happen
    }
    return true;
}

int dateTimeStringsToMinutes(const std::string& date_str, const std::string& time_str) {
    int day_idx = dateStringToDayIndex(date_str);
    if (day_idx == -1) {
        return -1;
    }
    int minutes_in_day = timeStringToMinutesInDay(time_str);
    if (minutes_in_day == -1) {
        return -1;
    }
    return day_idx * MINUTES_IN_DAY + minutes_in_day;
}

} // namespace TimeUtil