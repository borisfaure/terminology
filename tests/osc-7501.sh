#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# root record, replaced completely: msg must be gone after the second one
printf '\033]7501;state=working:progress=40:msg=Y2FyZ28gdGVzdA==\033\\'
printf '\033]7501;state=working\007'
# blocked with kind, app and unpadded base64
printf '\033]7501;id=tf:state=blocked:kind=permission:app=terraform:msg=QXBwbHkgMyBjaGFuZ2VzPw\033\\'
# out-of-range or misplaced values become absent
printf '\033]7501;id=a:state=working:progress=101:kind=auth\033\\'
printf '\033]7501;id=b:state=done:progress=50:app=bad!app\033\\'
# whitespace, repeated and unknown keys
printf '\033]7501; id = c : state=idle : state=error : colour=red \033\\'
# a malformed pair is skipped, the rest applies
printf '\033]7501;id=d:nonsense:STATE=x:state=done\033\\'
# title
printf '\033]7501;id=e:state=done:title=QnVpbGQgT0s=\033\\'
# U+202E is stripped from the text, the report still applies
printf '\033]7501;id=f:state=done:msg=4oCuZXZpbA==\033\\'
