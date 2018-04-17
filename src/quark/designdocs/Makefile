DESIGNDOCS = $(patsubst %/,%,$(wildcard *.design/))


DESIGN_DEPS =  $(addsuffix .deps,$(DESIGNDOCS))
DESIGN_JSON =  $(addsuffix .json,$(DESIGNDOCS))
DESIGN_CPP =  $(addsuffix .cpp,$(DESIGNDOCS))
DESIGN_H =  $(addsuffix .h,$(DESIGNDOCS))

COUCHCPP = ../../../bin/couchcpp -f couchcpp.cfg
COUCHCPP_INCLUDES = ../../../
COUCHCPP_BASEDIR = tmp
 
all : designs

tmp :
	mkdir tmp


%.design.deps:
	echo $(@:.deps=.json) : `find  $(@:.deps=)/*`  > $@  	

%.design.cpp : %.design.json %.design.h tmp/bin2c
	tmp/bin2c $< designdoc_$(@F:.design.cpp=) > $@

%.design.h :
	echo extern const char *designdoc_$(@F:.design.h=)\; > $@
	echo extern unsigned int designdoc_$(@F:.design.h=)_length\; >> $@

%.design.json: | tmp 
#	FILES=`find $(@:.json=) -name "*.cpp"`; $(COUCHCPP) -p $$FILES -o $(COUCHCPP_BASEDIR) 
	cd $(@:.json=) && couchapp push --export > ../$(@F)
	echo $@ : `find  $(@:.json=)/*`  > $(@:.json=.deps)  	

-include $(DESIGN_DEPS)

tmp/bin2c: bin2c.c | tmp
	@echo Building resources
	$(CC) -o $@ bin2c.c

designs: $(DESIGN_CPP) $(DESIGN_H)

clean-designs: 
	rm -f $(DESIGN_DEPS) $(DESIGN_JSON) $(DESIGN_CPP)
	rm -rf tmp

		