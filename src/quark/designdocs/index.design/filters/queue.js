function(doc) {
	if (doc._deleted) return false;
	if (doc._id == "error") return true;
	if (doc._id == "settings") return true;
	if (doc._id == "control") return true;
	if (doc._id.substr(0,2) == "o."
		&& !doc.finished
		&& (doc.status == "validating"
			|| (doc.updateStatus && doc.updateStatus == "validating")
			|| doc.cancelReq)) return true;
	return false;
}