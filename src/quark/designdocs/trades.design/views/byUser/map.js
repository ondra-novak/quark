function(doc) {
	if (doc.buy) {
		emit([doc.buy[1],doc.time], null);		
		emit([doc.sell[1],doc.time], null);		
	} else {
		emit([doc.buyUser,doc.time], null);		
		emit([doc.sellUser,doc.time], null);
	}
}