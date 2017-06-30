function(doc) {
	
	if (!doc._deleted) {		
		if (doc._id == "error") {
			emit(1,null);
		} else if (doc._id == "settings") {
			emit(2,null);
		} else if (doc._id.substr(0,2) == "o." && !doc.finished){
			emit(doc.timeModified, null);
		}		
	}
	
	
}