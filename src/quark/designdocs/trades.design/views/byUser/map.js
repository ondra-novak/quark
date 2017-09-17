function(doc) {
	emit([doc.buyUser,doc.time], null);		
	emit([doc.sellUser,doc.time], null);
}