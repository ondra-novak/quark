function(doc) {
	return (doc._id.substr(0,2) == "o." && doc.finished && !doc._deleted);
}