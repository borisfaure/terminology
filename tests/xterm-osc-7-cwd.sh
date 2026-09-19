#!/bin/sh

# fill space with E
printf '\033#8'

# The checksum is of the final state, so every rejected form goes first and
# proves itself by not disturbing the known-good value set at the end.

# not a file:// URI
printf '\033]7;http://localhost/tmp\033\\'

# no path at all
printf '\033]7;file://localhost\007'

# an empty payload clears the directory rather than being an error: that is
# what tmux sends for a pane that has none. Invisible to the checksum, which
# cannot tell "cleared" from "never set", so it is verified by --dump.
printf '\033]7;\007'

# a host that is not ours: ignored, and deliberately not an error
printf '\033]7;file://not-this-host.invalid/tmp/elsewhere\033\\'

# a control character smuggled in through the percent-encoding
printf '\033]7;file:///tmp/a%%07b\033\\'

# Longer than PATH_MAX once decoded. Percent-decoding only ever shortens and
# the OSC payload is capped at 4096 Eina_Unicode, so multibyte expansion is the
# only way to reach the cap: 1120 characters of U+1D11E, 4 bytes each.
pad=''
i=0
while [ $i -lt 40 ]; do
    pad="$pad𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞"
    i=$((i + 1))
done
# On stderr, which the checksum never sees: if PATH_MAX ever exceeds the byte
# count the sequence stops being rejected and this case quietly stops biting.
printf 'PATH_MAX=%s, oversized path is %s bytes\n' \
       "$(getconf PATH_MAX /)" \
       "$(printf '%s' "$pad" | wc -c | tr -d ' ')" >&2
printf '\033]7;file:///%s\033\\' "$pad"

# accepted: empty host, localhost, and percent-decoding. The last one wins.
printf '\033]7;file:///tmp/valid\033\\'
printf '\033]7;file://localhost/tmp/valid-2\007'
printf '\033]7;file:///tmp/a%%20b\033\\'
