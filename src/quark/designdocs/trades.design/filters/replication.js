function(doc) {
	return (doc.buyOrder || doc.buy) && (doc.sellOrder || doc.sell) && !doc._deleted;
}