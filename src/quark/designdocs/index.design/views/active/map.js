function(doc) {
	
	if (doc._id.substr(0,2) == "o."  && !doc.finished && !doc.cancelReq) {
		emit(doc.user,null);  
	}
	
}