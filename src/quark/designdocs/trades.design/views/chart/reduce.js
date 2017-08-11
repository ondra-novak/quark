function(keys, values, rereduce) {
  var agr = {};

if (rereduce) {
	agr = values[0];
	var ci = agr.i;
	var oi = agr.i;
	values.forEach(function(x) {
		if (ci < x.i) {ci = x.i; agr.c = x.c;}
		if (oi > x.i) {oi = x.i; agr.o = x.o;agr.i = x.i;}
		if (agr.h < x.h) agr.h = x.h;
		if (agr.l > x.l) agr.l = x.l;
		agr.n += x.n;
		agr.s += x.s;
		agr.v += x.v;
		agr.s2 += x.s2;
		agr.v2 += x.v2;
	});
	return agr;
} else {
	agr.h = agr.l = agr.o = agr.c = values[0][0];
	agr.i = values[0][2];
	agr.n = agr.v = agr.s2 = agr.v2 = agr.s = 0;
	values.forEach(function(x) {
		var p = x[0];
		var v = x[1];
		var i = x[2];
		if (ci < i) {ci = i; agr.c = p;}
		if (oi > i) {oi = i; agr.o = p;agr.i = i;}
		if (agr.h < p) agr.h = p;
		if (agr.l > p) agr.l = p;
		agr.n += 1;
		agr.s += p;
		agr.v += v;
		agr.s2 += p*p;
		agr.v2 += v*v;
	});
	return agr;


}

}
