all: attacker victim



attacker: attacker.c dockcross-x86
	./dockcross-x86 bash -c 'sudo $$CC attacker.c -O0  -m64 -Xlinker -Map=output.map -o attacker -static'

victim: victim.c dockcross-x86
	./dockcross-x86 bash -c 'sudo $$CC victim.c  -O0 -m64 -o victim -static'


dockcross-x86:
	docker run --rm dockcross/linux-x86 > ./dockcross-x86
	chmod +x ./dockcross-x86

clean:
	rm -f dockcross-x86 attacker victim
