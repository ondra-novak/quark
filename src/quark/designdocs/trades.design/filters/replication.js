function(doc) {
	return doc.buyOrder && doc.sellOrder && !doc._deleted;
}