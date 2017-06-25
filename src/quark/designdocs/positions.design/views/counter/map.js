function(doc) {
	if (doc._id.substr(0,2) == "t.") {
		emit(doc.index, null);		
	}
}