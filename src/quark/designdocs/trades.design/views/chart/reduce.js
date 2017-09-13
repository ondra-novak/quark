function(keys, values, rereduce) {
  var agr ; //open=0,high=1, low=2, close=3, volume=4, n=5 , sum=6, sum2=7, volume2=8, index=9  - use indexes for performance reason

if (rereduce) {
	agr = values[0];
	var ci = agr[9];
	var oi = agr[9];
	values.forEach(function(x) {
		if (ci < x[9]) {ci = x[9]; agr[3] = x[3];}
		if (oi > x[9]) {oi = x[9]; agr[0] = x[0];agr[9] = x[9];}
		if (agr[1] < x[1]) agr[1] = x[1];
		if (agr[2] > x[2]) agr[2] = x[2];
		agr[5] += x[5];
		agr[6] += x[6];
		agr[4] += x[4];
		agr[7] += x[7];
		agr[8] += x[8];
	});
	return agr;
} else {
    var p = values[0][0];
	var i = keys[0][0];
	var v = values[0][1];
	agr = [p,p,p,p,0,0,0,0,0,i];
	values.forEach(function(x,idx) {
		var p = x[0];
		var v = x[1];
		var i = keys[idx][0];
		if (ci < i) {ci = i; agr[3] = p;}
		if (oi > i) {oi = i; agr[0] = p;agr[9] = i;}
		if (agr.h < p) agr[1] = p;
		if (agr.l > p) agr[2] = p;
		agr[5] += 1;
		agr[6] += p;
		agr[4] += v;
		agr[7] += p*p;
		agr[8] += v*v;
	});
	return agr;


}

}
