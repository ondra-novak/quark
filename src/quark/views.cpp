#include "views.h"

namespace quark {


View tradesByCounter("_design/trades/_view/counter", View::update);
View queueView("_design/orders/_view/queue", View::includeDocs | View::update);
Filter errorWait("orders/error");

}
