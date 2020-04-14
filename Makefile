lsyslog: lsyslog.o lsyslog_parser.o

vpath %.c src
vpath %.c.rl src

%.c: %.c.rl
	ragel -G2 -o $@ $^
