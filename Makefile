
all: read-randr-json

clean:
	rm -f read-randr-json

read-randr-json: read-randr-json.cc
	g++ -o $@ $< -lxcb -lxcb-randr
