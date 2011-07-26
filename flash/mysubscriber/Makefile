TARGET = MySubscriber.swf

.PHONY: all clean

all: $(TARGET)

$(TARGET): MySubscriber.as
	/opt/flex/bin/mxmlc -debug=true $^

clean:
	rm -f $(TARGET)

