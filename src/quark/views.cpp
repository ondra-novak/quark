#include "views.h"

namespace quark {


View queueView("_design/index/_view/queue", View::includeDocs | View::update);
Filter waitfordoc("index/waitfordoc");
Filter queueFilter("index/queue",View::includeDocs);
View userTrades("_design/trades/_view/byUser", View::update);
View userActiveOrders("_design/index/_view/active", View::includeDocs|View::update);

}
