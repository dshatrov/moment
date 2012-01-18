TARGET = BasicPlayer.swf

.PHONY: all clean

all: $(TARGET)

$(TARGET): BasicPlayer.as
	/opt/flex/bin/mxmlc -headless-server=true -static-link-runtime-shared-libraries=true $^
#	/opt/flex/bin/mxmlc $^
#	/opt/flex/bin/mxmlc -debug=true $^

clean:
	rm -f $(TARGET)

