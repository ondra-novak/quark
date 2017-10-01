function(doc, req) {
	if (doc._id.substr(0,2) == "o." && (doc.finished || doc._deleted)) {
		var tm = parseInt(req.query.timestamp);		
		return tm >= doc.timeModified;
	} else {
		return false;
	}		
}