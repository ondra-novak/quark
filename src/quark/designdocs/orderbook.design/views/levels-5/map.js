function(doc) {
	if (doc._id.substr(0,2) == "o."
		&& !doc.finished
		&& doc.status == "active"
		&& (doc.type == "limit" || doc.type == "postlimit")) {
		
			var price = doc.limitPrice;
			var a = [doc.dir=="buy"?0:1,Math.floor(price)]
			for (var i = 10; i <= 100000; i = i * 10) {
				var p = Math.floor(price * i) % 10;
				a.push(p);
			}
		
			emit(a, doc.size);
		}	
}