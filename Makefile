CC?=gcc
CFLAGS?=-O2
MODULE_FLAGS=-fPIC -DPIC -shared

all:
	$(CC) $(CFLAGS) $(MODULE_FLAGS) -DDYNAMIC_LINKING -o m_wol.so m_wol.c -I../Unreal3.2/include -I../Unreal3.2/extras/regexp/include

clean:
	rm -f m_wol.so
