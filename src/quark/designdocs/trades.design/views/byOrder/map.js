function(doc) {
	if (doc._id.substr(0,2) == "t.") {
		emit(doc.buyOrder, null);		
		emit(doc.sellOrder, null);
	}
}