function(keys, values, rereduce) {
	
	var size = 0;
	var count = 0;
	if (rereduce) {
		
		values.forEach(function(x) {
			size = size + x.size;
			count = count + x.count;
		});
		
	} else {
		
		values.forEach(function(x) {
			size = size + x;
			count++;
		});
		
	}
	return {
		"size":size,
		"count":count
	}
}