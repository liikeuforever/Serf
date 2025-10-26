#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <ctime>
#include <iomanip>

uint64_t ParseTimestamp(const std::string& time_str) {
    struct tm tm = {};
    std::istringstream ss(time_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return static_cast<uint64_t>(mktime(&tm));
}

int main(int argc, char* argv[]) {
    std::string csv_file = argv[1];
    int max_points = 1200;
    
    std::ifstream file(csv_file);
    std::string line;
    std::getline(file, line); // header
    
    std::vector<uint64_t> timestamps;
    int count = 0;
    
    while (std::getline(file, line) && count < max_points) {
        std::istringstream ss(line);
        std::string lon_str, lat_str, time_str;
        
        if (std::getline(ss, lon_str, ',') && 
            std::getline(ss, lat_str, ',') && 
            std::getline(ss, time_str)) {
            uint64_t timestamp = ParseTimestamp(time_str);
            timestamps.push_back(timestamp);
            
            if (count >= 1155 && count <= 1165) {
                std::cout << "Point " << count << ": timestamp=" << timestamp << " (" << time_str << ")\n";
            }
            count++;
        }
    }
    
    std::cout << "\n检查timestamp连续性:\n";
    for (size_t i = 1155; i < 1165 && i < timestamps.size(); i++) {
        uint64_t delta = (timestamps[i] > timestamps[i-1]) ? 
                        (timestamps[i] - timestamps[i-1]) : 
                        (timestamps[i-1] - timestamps[i]);
        std::cout << "Point " << i << " delta=" << delta << "\n";
    }
    
    return 0;
}
