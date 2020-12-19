# Compiler Info ('g++-4.8 --version')
# sh: g++-4.8: command not found
# End Compiler Info Output
CXX = /usr/bin/g++-4.8
LINK = /usr/bin/g++-4.8
NDKDIR ?= /${SOFTWARE_PATH}/Nuke11.2v2
#You will need to set the path to boost for psdReader only
BOOSTDIR ?= REPLACE_ME_WITH_BOOST_DIR
#You will need to set the path to openEXR for exr plugins only
OPENEXRDIR ?= REPLACE_ME_WITH_OPENEXR_DIR
CXXFLAGS ?= -c \
            -DUSE_GLEW \
            -I$(NDKDIR)/include \
            -fPIC -msse -fpermissive
LINKFLAGS ?= -L$(NDKDIR) \
             -L./ \
             -L/usr/lib
LIBS ?= -lDDImage
LINKFLAGS += -shared
all: Scroll.so Cone.so TaperedCylinder.so FisheyeDistort.so

scroll: Scroll.so

cone: Cone.so

taperedcylinder: TaperedCylinder.so

stunmap: STUnmap.so

dr: DeepRemove.so

do: DeepOpacity.so

dc: DeepCurveTool.so

dp: DeepPlus.so

.PRECIOUS : %.os
%.os: %.cpp
	$(CXX) $(CXXFLAGS) -o tmp/$(@) $<
%.so: %.os
	$(LINK) $(LINKFLAGS) $(LIBS) -o lib/$(@) tmp/$<
%.a: %.cpp
	$(CXX) $(CXXFLAGS) -o lib$(@) $<

ndkexists:
	if test -d $(NDKDIR); \
	then echo "using NDKDIR from ${NDKDIR}"; \
	else echo "NDKDIR dir not found! Please set NDKDIR"; exit 2; \
	fi
clean:
	rm -rf *.os \
	       *.o \
	       *.a \
	       *.so
