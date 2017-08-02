function(doc, req) {
	var reqdoc = req.query.doc;
	return doc._id == reqdoc;
}