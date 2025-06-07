#include <string>
#include "map.hpp"
#include "vector.hpp"

class CommandParser {
public:
    long long timestamp;
    std::string commandName;
    sjtu::map<std::string, std::string> arguments;

    CommandParser() : timestamp(0) {}

    bool parse(const std::string& line) {
        timestamp = 0;
        commandName = "";
        arguments.clear();

        if (line.empty()) {
            return false;
        }

        sjtu::vector<std::string> words;
        size_t current_pos = 0;

        while (current_pos < line.length()) {
            size_t token_start = line.find_first_not_of(" \t\n\r\f\v", current_pos);
            if (token_start == std::string::npos) {
                break;
            }
            size_t token_end = line.find_first_of(" \t\n\r\f\v", token_start);
            std::string token;
            if (token_end == std::string::npos) {
                token = line.substr(token_start);
                words.push_back(token);
                break;
            }
            token = line.substr(token_start, token_end - token_start);
            words.push_back(token);
            current_pos = token_end;
        }

        if (words.size() == 0) {
            return false;
        }

        const std::string& ts_token = words[0];
        if (ts_token.length() < 3 || ts_token.front() != '[' || ts_token.back() != ']') {
            return false;
        }
        std::string ts_numeric_part = ts_token.substr(1, ts_token.length() - 2);
        if (sscanf(ts_numeric_part.c_str(), "%lld", &timestamp) != 1) {
            return false;
        }

        // 2. Parse Command Name
        if (words.size() < 2) {
            return false;
        }
        commandName = words[1];

        size_t arg_tokens_start_index = 2;
        if (words.size() <= arg_tokens_start_index) {
            return true;
        }

        size_t num_arg_tokens = words.size() - arg_tokens_start_index;
        if (num_arg_tokens % 2 != 0) {
            return false;
        }

        for (size_t i = 0; i < num_arg_tokens; i += 2) {
            const std::string& key_str = words[arg_tokens_start_index + i];
            if (key_str.length() < 2 || key_str[0] != '-') {
               return false;
            }
            
            std::string actual_key = key_str.substr(1);
            const std::string& value_str = words[arg_tokens_start_index + i + 1];
            arguments.insert({actual_key,value_str});
        }

        return true;
    }

    std::string getArg(const std::string& key, const std::string& defaultValue = "") const {
        auto it = arguments.find(key);
        if (it != arguments.cend()) {
            return it->second;
        }
        return defaultValue;
    }
    bool hasArg(const std::string& key) const {
        return arguments.count(key) > 0;
    }
};
