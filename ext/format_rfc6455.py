import sys
MASK = 'abcd'

msg = " ".join(sys.argv[1:])

header = "\x81"
if len(msg) < 126:
	header += chr(0x80 | len(msg))
elif len(msg) <= 0xFFFF:
	header += chr(0x80 | 126)
	header += chr(0x0000 | len(msg))
else:
	header += chr(0x80 | 127)
	header += chr(0x0000000000000000 | len(msg))

out = repr('""'.join([chr(ord(c) ^ ord(MASK[i % 4])) for i, c in enumerate(msg)]))

# Remove the single quotes that are printed with it
print '"%s""%s""%s"' % (repr(header)[1:-1], MASK, out[1:-1])
