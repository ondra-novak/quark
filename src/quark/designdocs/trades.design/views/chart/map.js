function(doc) {
  if (doc.time) {
	emit([(doc.time/604800|0),(doc.time/86400|0)%7,(doc.time/14400|0)%6,(doc.time/3600|0)%4, (doc.time/900|0)%4,(doc.time/300|0)%3, (doc.time/60|0)%5], [doc.price,doc.size]);
}}
