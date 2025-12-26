.PHONY: all clean install deb test

CC = gcc
CFLAGS = -Wall -Wextra -g
KUBSH_LDFLAGS = -lreadline
VFS_LDFLAGS = -lfuse3 -D_FILE_OFFSET_BITS=64

all: kubsh vfs

kubsh: kubsh.c
	$(CC) $(CFLAGS) -o kubsh kubsh.c $(KUBSH_LDFLAGS)
vfs: vfs.c
	$(CC) $(CFLAGS) -o vfs vfs.c $(VFS_LDFLAGS)
clean:
	rm -f kubsh vfs
	rm -rf pkg
test:
	@echo "=== Testing kubsh ==="
	@rm -f ~/.kubsh_history
	@echo "=== Test 1: Basic commands ==="
	@echo "echo test" | ./kubsh 2>/dev/null | grep -q "test" && echo "+ echo works" || echo "- echo failed"
	@echo "Test 2: History"
	@echo "echo cmd1" | ./kubsh 2>/dev/null
	@echo "echo cmd2" | ./kubsh 2>/dev/null
	@echo "history" | ./kubsh 2>/dev/null | grep -q "cmd2" && echo "+ history works" || echo "- history failed"
	@echo "Test 3: Quit command"
	@echo "\q" | ./kubsh 2>/dev/null && echo "+ quit works" || echo "quit failed"
	@echo "=== Tests completed ==="
deb: kubsh vfs
	mkdir -p pkg/DEBIAN pkg/usr/bin/
	cp kubsh vfs pkg/usr/bin/
	cat > pkg/DEBIAN/control << 'CONTROL_EOF'
Package: kubsh
Version: 1.0-1
Architecture: amd64
Maintainer: Student <student@example.com>
Depends: libc6 (>= 2.31), libreadline8 (>= 8.0), fuse3
Description: Custom shell with VFS support for user management
CONTROL_EOF
	dpkg-deb --build pkg kubsh_1.0_amd64.deb

install: kubsh vfs
	install -D -m 755 kubsh $(DESTDIR)/usr/bin/kubsh
	install -D -m 755 vfs $(DESTDIR)/usr/bin/vfs
run: kubsh
	./kubsh
