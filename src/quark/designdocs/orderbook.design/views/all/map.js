function(doc) {
	if (doc._id.substr(0,2) == "o."
		&& !doc.finished
		&& doc.status == "active"
		&& (doc.type == "limit" || doc.type == "postlimit")) {
			emit([doc.dir=="buy"?0:1,doc.limitPrice], doc.size);
		}	
}