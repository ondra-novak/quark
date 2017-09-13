function(doc) {
	emit(doc.buyOrder, null);		
	emit(doc.sellOrder, null);
}