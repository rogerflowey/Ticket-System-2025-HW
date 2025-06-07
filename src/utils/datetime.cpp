#include "datetime.h" // Ensure this matches your header file name
#include <cstdio> // For snprintf

namespace TimeUtil {

// --- Low-Level Parsing and Formatting Helper Implementations ---

bool parseMMDD(const std::string& date_str, int& month_out, int& day_out) {
    if (date_str.length() != 5 || date_str[2] != '-') {
        return false;
    }
    std::string month_s = date_str.substr(0, 2);
    std::string day_s = date_str.substr(3, 2);

    for (char c : month_s) if (c < '0' || c > '9') return false;
    for (char c : day_s) if (c < '0' || c > '9') return false;

    try {
        month_out = std::stoi(month_s);
        day_out = std::stoi(day_s);
    } catch (const std::exception&) { // Catches std::invalid_argument, std::out_of_range
        return false;
    }

    if (month_out < 1 || month_out > 12) return false;
    if (day_out < 1 || day_out > DAYS_IN_MONTH_2025[month_out]) { // Relies on DAYS_IN_MONTH_2025
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

    try {
        hour_out = std::stoi(hour_s);
        minute_out = std::stoi(minute_s);
    } catch (const std::exception&) {
        return false;
    }

    if (hour_out < 0 || hour_out > 23 || minute_out < 0 || minute_out > 59) {
        return false;
    }
    return true;
}

void formatMMDD(int month, int day, std::string& out_str) {
    char buf[6]; // MM-DD + null
    snprintf(buf, sizeof(buf), "%02d-%02d", month, day);
    out_str = buf;
}

void formatHHMM(int hour, int minute, std::string& out_str) {
    char buf[6]; // hh:mm + null
    snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
    out_str = buf;
}

// --- Core Conversion Function Implementations ---

bool dateStringFromDayIndex(int day_index, std::string& date_str_out) {
    if (day_index < 0 || (day_index * MINUTES_IN_DAY) >= END_OF_2025_EXCLUSIVE_MINUTES) {
        // Check if day_index itself would lead to minutes outside the scope
        // Max valid day_index is (END_OF_2025_EXCLUSIVE_MINUTES / MINUTES_IN_DAY) - 1
        if (END_OF_2025_EXCLUSIVE_MINUTES == 0 && day_index == 0) { /* allow day 0 if scope is 0 days */ }
        else if (day_index >= (END_OF_2025_EXCLUSIVE_MINUTES / MINUTES_IN_DAY) ) return false;
    }


    int current_month = REF_MONTH;
    int current_day = REF_DAY;
    int day_offset_remaining = day_index;

    while (day_offset_remaining > 0) {
        // This check should ideally not be hit if day_index is pre-validated against END_OF_2025_EXCLUSIVE_MINUTES
        if (current_month > 12) return false; // Should be caught by day_index check

        int days_in_current_m_val = DAYS_IN_MONTH_2025[current_month];
        int days_left_in_current_m = days_in_current_m_val - current_day;

        if (day_offset_remaining <= days_left_in_current_m) {
            current_day += day_offset_remaining;
            day_offset_remaining = 0;
        } else {
            day_offset_remaining -= (days_left_in_current_m + 1);
            current_day = 1;
            current_month++;
        }
    }
     if (current_month > 12) return false; // Final check

    formatMMDD(current_month, current_day, date_str_out);
    return true;
}

int dayIndexFromDateString(const std::string& date_str) {
    int month, day;
    if (!parseMMDD(date_str, month, day)) {
        return -1;
    }

    if (month < REF_MONTH || (month == REF_MONTH && day < REF_DAY)) {
        return -1; // Date is before reference epoch
    }
    // parseMMDD already ensures month <= 12 and day is valid for that month in 2025.

    int day_idx = 0;
    int temp_m = REF_MONTH;
    int temp_d = REF_DAY;

    while(temp_m < month || (temp_m == month && temp_d < day)) {
        day_idx++;
        temp_d++;
        if (temp_d > DAYS_IN_MONTH_2025[temp_m]) {
            temp_d = 1;
            temp_m++;
            // If temp_m > 12, it means the original date_str was for a date beyond 2025,
            // which parseMMDD should catch if it strictly uses DAYS_IN_MONTH_2025.
            // This loop assumes valid month/day for 2025 from parseMMDD.
            if (temp_m > 12) return -1; // Should not be reached if parseMMDD is correct
        }
    }
    // Ensure the calculated day_index is within the 2025 scope
    if ((day_idx * MINUTES_IN_DAY) >= END_OF_2025_EXCLUSIVE_MINUTES) {
        return -1;
    }
    return day_idx;
}

int minutesInDayFromTimeString(const std::string& time_str) {
    int hour, minute;
    if (!parseHHMM(time_str, hour, minute)) {
        return -1;
    }
    return hour * MINUTES_IN_HOUR + minute;
}

bool timeStringFromMinutesInDay(int minutes_in_day, std::string& time_str_out) {
    if (minutes_in_day < 0 || minutes_in_day >= MINUTES_IN_DAY) {
        return false;
    }
    int hour = minutes_in_day / MINUTES_IN_HOUR;
    int minute = minutes_in_day % MINUTES_IN_HOUR;
    formatHHMM(hour, minute, time_str_out);
    return true;
}

bool stringsFromMinutes(Time_t total_minutes_since_ref,
                        std::string& date_str_out,
                        std::string& time_str_out) {
    if (total_minutes_since_ref < 0 || total_minutes_since_ref >= END_OF_2025_EXCLUSIVE_MINUTES) {
        return false;
    }
    int day_idx = total_minutes_since_ref / MINUTES_IN_DAY;
    int minutes_in_day = total_minutes_since_ref % MINUTES_IN_DAY;

    if (!dateStringFromDayIndex(day_idx, date_str_out)) {
        return false; // Should be caught by initial total_minutes check
    }
    if (!timeStringFromMinutesInDay(minutes_in_day, time_str_out)) {
        return false; // Should not happen if logic is correct
    }
    return true;
}

Time_t minutesFromStrings(const std::string& date_str, const std::string& time_str) {
    int day_idx = dayIndexFromDateString(date_str);
    if (day_idx == -1) {
        return -1;
    }
    int minutes_in_day = minutesInDayFromTimeString(time_str);
    if (minutes_in_day == -1) {
        return -1;
    }
    Time_t total_minutes = day_idx * MINUTES_IN_DAY + minutes_in_day;

    // The dayIndexFromDateString should ensure day_idx is within scope.
    // So, total_minutes should also be within scope if components are valid.
    // A final check here is redundant if dayIndexFromDateString is correct.
    // if (total_minutes >= END_OF_2025_EXCLUSIVE_MINUTES) return -1; // Redundant if day_idx is capped

    return total_minutes;
}


// --- DateTime Struct Member Implementations ---

void DateTime::get_components_unsafe(int& year_out, int& month_out, int& day_out, int& hour_out, int& minute_out) const {
    // This function assumes total_minutes_since_ref_ is already validated by isValid()
    // and is within the 0 to END_OF_2025_EXCLUSIVE_MINUTES-1 range.
    year_out = REF_YEAR;

    int day_idx = total_minutes_since_ref_ / MINUTES_IN_DAY;
    int minutes_in_day_val = total_minutes_since_ref_ % MINUTES_IN_DAY;

    hour_out = minutes_in_day_val / MINUTES_IN_HOUR;
    minute_out = minutes_in_day_val % MINUTES_IN_HOUR;

    month_out = REF_MONTH;
    day_out = REF_DAY;

    int current_day_offset = day_idx;

    while (current_day_offset > 0) {
        int days_in_current_month_val = DAYS_IN_MONTH_2025[month_out];
        int days_left_in_month = days_in_current_month_val - day_out;

        if (current_day_offset <= days_left_in_month) {
            day_out += current_day_offset;
            current_day_offset = 0;
        } else {
            current_day_offset -= (days_left_in_month + 1);
            day_out = 1;
            month_out++;
            // No need to check month_out > 12 here, as isValid() ensures
            // total_minutes_since_ref_ doesn't push us beyond Dec 31, 2025.
        }
    }
}

DateTime::DateTime() : total_minutes_since_ref_(-1) {}

DateTime::DateTime(Time_t minutes_since_epoch) : total_minutes_since_ref_(minutes_since_epoch) {}

DateTime::DateTime(const std::string& date_str, const std::string& time_str) {
    total_minutes_since_ref_ = TimeUtil::minutesFromStrings(date_str, time_str);
}

bool DateTime::isValid() const {
    if (total_minutes_since_ref_ < 0) {
        return false;
    }
    if (total_minutes_since_ref_ >= TimeUtil::END_OF_2025_EXCLUSIVE_MINUTES) {
        return false;
    }
    return true;
}

int DateTime::year() const {
    if (!isValid()) return -1;
    return REF_YEAR;
}

int DateTime::month() const {
    if (!isValid()) return -1;
    int y, m, d, h, mn;
    get_components_unsafe(y, m, d, h, mn);
    return m;
}

int DateTime::day() const {
    if (!isValid()) return -1;
    int y, m, d, h, mn;
    get_components_unsafe(y, m, d, h, mn);
    return d;
}

int DateTime::hour() const {
    if (!isValid()) return -1;
    int y, m, d, h, mn;
    get_components_unsafe(y, m, d, h, mn);
    return h;
}

int DateTime::minute() const {
    if (!isValid()) return -1;
    int y, m, d, h, mn;
    get_components_unsafe(y, m, d, h, mn);
    return mn;
}

std::string DateTime::getDateString() const {
    if (!isValid()) return "INVALID_DATE";
    int y, m, d, h, mn;
    get_components_unsafe(y, m, d, h, mn);
    std::string date_str_out;
    TimeUtil::formatMMDD(m, d, date_str_out);
    return date_str_out;
}

std::string DateTime::getTimeString() const {
    if (!isValid()) return "INVALID_TIME";
    int y, m, d, h, mn;
    get_components_unsafe(y, m, d, h, mn);
    std::string time_str_out;
    TimeUtil::formatHHMM(h, mn, time_str_out);
    return time_str_out;
}
std::string DateTime::getFullString() const {
    if (!isValid()) return "INVALID_TIME";
    int y, m, d, h, mn;
    get_components_unsafe(y, m, d, h, mn);
    std::string date_str_out;
    formatMMDD(m, d, date_str_out);
    std::string time_str_out;
    formatHHMM(h, mn, time_str_out);
    return date_str_out+" "+time_str_out;
}

DateTime DateTime::operator+(int minutes_to_add) const {
    if (!isValid() && minutes_to_add != 0) return DateTime(); // Adding to invalid (unless 0) gives invalid
    if (!isValid() && minutes_to_add == 0) return DateTime(total_minutes_since_ref_); // Adding 0 to invalid keeps its specific invalid value
    return DateTime(total_minutes_since_ref_ + minutes_to_add);
}

DateTime DateTime::operator-(int minutes_to_subtract) const {
     if (!isValid() && minutes_to_subtract != 0) return DateTime();
     if (!isValid() && minutes_to_subtract == 0) return DateTime(total_minutes_since_ref_);
    return DateTime(total_minutes_since_ref_ - minutes_to_subtract);
}

Time_t DateTime::operator-(const DateTime& other) const {
    return total_minutes_since_ref_ - other.total_minutes_since_ref_;
}

bool DateTime::operator<(const DateTime& other) const {
    return total_minutes_since_ref_ < other.total_minutes_since_ref_;
}
bool DateTime::operator==(const DateTime& other) const {
    return total_minutes_since_ref_ == other.total_minutes_since_ref_;
}
bool DateTime::operator!=(const DateTime& other) const {
    return !(*this == other);
}
bool DateTime::operator<=(const DateTime& other) const {
    return total_minutes_since_ref_ <= other.total_minutes_since_ref_;
}
bool DateTime::operator>(const DateTime& other) const {
    return total_minutes_since_ref_ > other.total_minutes_since_ref_;
}
bool DateTime::operator>=(const DateTime& other) const {
    return total_minutes_since_ref_ >= other.total_minutes_since_ref_;
}

Time_t DateTime::getRawMinutes() const {
    return total_minutes_since_ref_;
}

DateTime DateTime::roundDownToDate() const {
  if (!isValid()) {
      return DateTime(); // Return invalid DateTime if current is invalid
  }
  Time_t minutes_at_start_of_day = (total_minutes_since_ref_ / MINUTES_IN_DAY) * MINUTES_IN_DAY;
  return DateTime(minutes_at_start_of_day);
}

DateTime DateTime::roundUpToDate() const {
    if (!isValid()) {
        return DateTime(); // Return invalid DateTime if current is invalid
    }
    Time_t minutes_in_day_component = total_minutes_since_ref_ % MINUTES_IN_DAY;

    if (minutes_in_day_component == 0) {
        // Already at 00:00 of a day, so it's already "rounded up"
        return DateTime(total_minutes_since_ref_);
    } else {
        // Not at 00:00, so round to the start of the next day
        Time_t current_day_idx = total_minutes_since_ref_ / MINUTES_IN_DAY;
        Time_t start_of_next_day_minutes = (current_day_idx + 1) * MINUTES_IN_DAY;
        return DateTime(start_of_next_day_minutes);
    }
}

} // namespace TimeUtil