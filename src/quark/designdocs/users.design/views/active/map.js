function(doc) {
	if (doc._id.substr(0,2) == "o." && !doc.finished) {
		emit(doc.user, doc._rev);
	}
}