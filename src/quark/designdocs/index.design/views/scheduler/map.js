function(doc) {	
	if (doc.expireTime && !doc.finished &&  !doc.expired) {
		emit(doc.expireTime,null);  
	}
}