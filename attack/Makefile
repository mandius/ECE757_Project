all: hello-static



hello-static: hello.c dockcross-x86
	./dockcross-x86 bash -c 'sudo $$CC hello.c -o hello -static'


dockcross-x86:
	docker run --rm dockcross/linux-x86 > ./dockcross-x86
	chmod +x ./dockcross-x86

clean:
	rm -f dockcross-x86 hello
