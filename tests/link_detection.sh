#!/bin/sh

# char width: 7
# char height: 15

# clear screen
printf '\033[2J'

# set color
printf '\033[46;31;3m'

# move to 2; 0
printf '\033[2;0H'
printf 'Avast to go on account crack Jennys tea cup code of conduct grog https://terminolo.gy/ blossom hands scourge of the seven seas gangway pillage starboard. Admiral of the Black case shot barque red ensign Privateer cutlass Arr measured fer yer chains Gold Road league. Crack Jennys tea cup cog spirits keelhaul handsomely broadside carouser ho gabion barque. www.Enlightenment.org\r\n\r\n'

printf 'Black jack six pounders flogging splice the main brace starboard yo-ho-ho Corsair plunder gally keelhaul. Landlubber or just lubber sloop jib lugsail foo.bar@qux.com clipper jury mast hornswaggle Blimey yard Pirate Round. Ye grapple parley brig brigantine spanker fluke warp league man-of-war.\r\n\r\n'

printf 'Jolly Roger fluke me brig grapple furl tack rutters gally brigantine.  Shiver me timbers yo-ho-ho black spot barque fore doubloon plunder run a shot ~/bin/terminology across the bow tack league. Brig lad keel keelhaul skysail cutlass rutters handsomely snow splice the main brace. \r\n\r\n'

printf "Reef sails Gold Road dead men tell no tales aft gangway lad quarter draught case shot grapple. Stern lass jury mast yo-ho-ho maroon shrouds mizzen /usr/bin/terminology wench jolly boat Admiral of the Black. Maroon Chain Shot pirate wench pillage strike colors bowsprit bucko lee Davy Jones\047 Locker.  \r\n"

##
# URL (https://terminolo.gy/)
##

# mouse move
printf '\033}tm;450;25\0'
# no url detection just before url
printf '\033}tln\0'

# mouse move
printf '\033}tm;458;25\0'
# url detection on first character 'h'
printf '\033}tlu;65;1;5;2;https://terminolo.gy/\0'

# mouse move
printf '\033}tm;495;25\0'
# url detection on first character ':'
printf '\033}tlu;65;1;5;2;https://terminolo.gy/\0'

# mouse move
printf '\033}tm;499;25\0'
# url detection on first character '/'
printf '\033}tlu;65;1;5;2;https://terminolo.gy/\0'

# mouse move
printf '\033}tm;510;25\0'
# url detection on second character '/'
printf '\033}tlu;65;1;5;2;https://terminolo.gy/\0'

# mouse move
printf '\033}tm;550;25\0'
# url detection on 'n'
printf '\033}tlu;65;1;5;2;https://terminolo.gy/\0'

# mouse move
printf '\033}tm;5;45\0'
# url detection on start of second line
printf '\033}tlu;65;1;5;2;https://terminolo.gy/\0'

# mouse move
printf '\033}tm;40;45\0'
# url detection on last /
printf '\033}tlu;65;1;5;2;https://terminolo.gy/\0'

# mouse move
printf '\033}tm;45;45\0'
# no url detection on space after url
printf '\033}tln\0'

##
# Not a link (league.)
##

# mouse move
printf '\033}tm;140;65\0'
# no url detection on 'l'
printf '\033}tln\0'

# mouse move
printf '\033}tm;180;65\0'
# no url detection on '.'
printf '\033}tln\0'



##
# URL (www.Enlightenment.org)
##

# mouse move
printf '\033}tm;255;85\0'
# no url detection on ' ' before url
printf '\033}tln\0'

# mouse move
printf '\033}tm;265;85\0'
# url detection on 'w'
printf '\033}tlu;37;5;57;5;www.Enlightenment.org\0'

# mouse move
printf '\033}tm;395;85\0'
# url detection on 'r'
printf '\033}tlu;37;5;57;5;www.Enlightenment.org\0'

# mouse move
printf '\033}tm;410;85\0'
# no url detection on empty space after url
printf '\033}tln\0'


##
# Email (foo.bar@qux.com)
##

# mouse move
printf '\033}tm;485;130\0'
# no email detection on ' ' before email
printf '\033}tln\0'

# mouse move
printf '\033}tm;495;133\0'
# email detection on 'f'
printf '\033}tle;70;8;4;9;foo.bar@qux.com\0'

# mouse move
printf '\033}tm;540;130\0'
# email detection on 'f'
printf '\033}tle;70;8;4;9;foo.bar@qux.com\0'

# mouse move
printf '\033}tm;540;130\0'
# email detection on '@'
printf '\033}tle;70;8;4;9;foo.bar@qux.com\0'

# mouse move
printf '\033}tm;10;140\0'
# email detection on '.'
printf '\033}tle;70;8;4;9;foo.bar@qux.com\0'

# mouse move
printf '\033}tm;45;140\0'
# no email detection on ' ' after email
printf '\033}tln\0'


##
# File (~/bin/terminology)
##

# mouse move
printf '\033}tm;480;200\0'
# no file detection on ' ' before path
printf '\033}tln\0'

# mouse move
printf '\033}tm;485;200\0'
# file detection on '~'
printf '\033}tlp;69;13;5;14;%s/bin/terminology\0' "$HOME"

# mouse move
printf '\033}tm;550;200\0'
# file detection on 'm'
printf '\033}tlp;69;13;5;14;%s/bin/terminology\0' "$HOME"

# mouse move
printf '\033}tm;5;215\0'
# file detection on 'n'
printf '\033}tlp;69;13;5;14;%s/bin/terminology\0' "$HOME"

# mouse move
printf '\033}tm;40;215\0'
# file detection on 'n'
printf '\033}tlp;69;13;5;14;%s/bin/terminology\0' "$HOME"

# mouse move
printf '\033}tm;48;215\0'
# no file detection on ' ' after path
printf '\033}tln\0'



##
# File (/usr/bin/terminology)
##

# mouse move
printf '\033}tm;466;280\0'
# no file detection on ' ' before path
printf '\033}tln\0'

# mouse move
printf '\033}tm;470;280\0'
# file detection on '/'
printf '\033}tlp;67;18;6;19;/usr/bin/terminology\0'

# mouse move
printf '\033}tm;550;280\0'
# file detection on 'r'
printf '\033}tlp;67;18;6;19;/usr/bin/terminology\0'

# mouse move
printf '\033}tm;5;295\0'
# file detection on 'i'
printf '\033}tlp;67;18;6;19;/usr/bin/terminology\0'

# mouse move
printf '\033}tm;40;295\0'
# file detection on 'g'
printf '\033}tlp;67;18;6;19;/usr/bin/terminology\0'

# mouse move
printf '\033}tm;55;295\0'
# no file detection on ' ' after path
printf '\033}tln\0'


