PCM_DIR = ./pcm
#CXXFLAGS += --stack-check -Wall -g -I$(PCM_DIR)
CXXFLAGS += -Wall -O3 -I$(PCM_DIR) -std=c++11
LDFLAGS += -L$(PCM_DIR)
LIB= -lrt -lPCM -lpthread

OBJS = main.o monitor.o process.o timer.o

all: cmr

-include $(OBJS:.o=.d)
cmr: $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIB)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $*.cpp -o $*.o
	@$(CXX) -MM $(CXXFLAGS) $*.cpp > $*.d
	@# http://scottmcpeak.com/autodepend/autodepend.html
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
	 sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -f $*.d.tmp

.PHONY: clean
clean:
	rm -rf *.x *.o *~ *.d cmr
