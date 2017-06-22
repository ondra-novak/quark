#include "views.h"

namespace quark {


View tradesByCounter("_design/trades/_view/counter", View::update);
}
