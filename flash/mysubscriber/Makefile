TARGET = MySubscriber.swf

.PHONY: all clean

all: $(TARGET)

$(TARGET): MySubscriber.as
	mxmlc -debug=true $^

clean:
	rm -f $(TARGET)

