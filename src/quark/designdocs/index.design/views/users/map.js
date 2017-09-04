function(doc) {
	
	if (doc._id.substr(0,2) == "o." ) {
		emit([doc.user,doc.timeModified],null);  
	}
	
}