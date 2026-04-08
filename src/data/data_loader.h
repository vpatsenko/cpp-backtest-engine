#pragma once

#include "../core/types.h"
#include <string>
#include <vector>
#include <memory>

namespace backtest {

// Data loader for LOB and trade data
class DataLoader {
public:
    static std::vector<std::shared_ptr<Event>> load_events(
        const std::string& lob_path,
        const std::string& trades_path
    );
};

} // namespace backtest
