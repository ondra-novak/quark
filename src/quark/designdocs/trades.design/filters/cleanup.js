function(doc, req) {
	if (doc.time || doc._deleted) {
		var tm = parseInt(req.query.timestamp);		
		return tm >= doc.time;
	} else {
		return false;
	}		
}