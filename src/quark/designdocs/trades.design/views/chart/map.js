function(doc) {
  if (doc._id.substr(0,2) == "t.") {
	emit([doc.time/86400|0,(doc.time/3600|0)%24, (doc.time/300|0)%12, (doc.time/60|0)%5], [doc.price,doc.size,doc.index]);
}}
