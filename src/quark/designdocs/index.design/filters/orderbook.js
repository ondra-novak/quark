function(doc) {
	return doc._id.substr(0,2) == "o." 
		&& doc.status != "validating" 
			&& doc.type == "limit";
}