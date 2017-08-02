function(doc) {
	if (doc._id.substr(0,2) == "o."
		&& !doc.finished
		&& doc.status == "active"
		&& doc.type == "limit" ) {
			emit([doc.dir,doc.limitPrice], doc.size);
		}	
}