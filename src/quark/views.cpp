#include "views.h"

namespace quark {


View tradesByCounter("_design/trades/_view/counter", View::update);
View queueView("_design/index/_view/queue", View::includeDocs | View::update);
Filter waitfordoc("index/waitfordoc");
Filter queueFilter("index/queue",View::includeDocs);


}
