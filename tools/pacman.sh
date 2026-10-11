#!/usr/bin/env bash

# True color -- use these variables to easily have different color
#    and format output.  Colours are 24-bit RGB, sent as SGR
#    38;2;r;g;b for the foreground and 48;2;r;g;b for the background
#    (ITU-T T.416), which Terminology stores per cell and renders
#    exactly: no palette rounding, and no theme shifts.  Make sure to
#    output the reset sequence after colors, and use the 'off' feature
#    for anything you turn on.

# Foreground colour from R, G, B components (0-255 each).
fg()
{
  printf '%s[38;2;%d;%d;%dm' "$esc" "$1" "$2" "$3"
}

# The same, for the background.
bg()
{
  printf '%s[48;2;%d;%d;%dm' "$esc" "$1" "$2" "$3"
}

initializeANSI()
{
  esc=$(echo -en "\033")
  redf=$(fg 233 60 62);      # #e93c3e
  greenf=$(fg 98 197 84)     # #62c554
  yellowf=$(fg 250 179 20)   # #fab314
  bluef=$(fg 56 142 245);    # #388ef5
  purplef=$(fg 171 98 241)   # #ab62f1
  cyanf=$(fg 38 198 218)     # #26c6da
  whitef=$(fg 252 252 252)   # #fcfcfc, the eyes

  # The second row paints the same picture with the hues as SGR 48;2
  # backgrounds: solid areas become blank tiles on the hue.  A cell's
  # background fills the whole cell, though, so on a half block the
  # exposed half can only come from the foreground; there the hue goes
  # back into a foreground glyph, over an invisible background.
  redb=$(bg 233 60 62)
  greenb=$(bg 98 197 84)
  yellowb=$(bg 250 179 20)
  blueb=$(bg 56 142 245)
  purpleb=$(bg 171 98 241)
  cyanb=$(bg 38 198 218)

  # Invisible background (SGR 48;1): Terminology maps it to COL_INVIS,
  # a fully transparent colour, so the theme background shows through
  # whatever the theme is.  The half-block edges of the second row draw
  # their exposed half with it instead of leaving the default
  # foreground to paint there.
  bginv="${esc}[48;1m"

  reset="${esc}[0m"
}

# note in this first use that switching colors doesn't require a reset
# first - the new color overrides the old one.

clear

initializeANSI

cat << EOF

 ${yellowf}  ▄███████▄${reset}   ${redf}  ▄██████▄${reset}    ${greenf}  ▄██████▄${reset}    ${bluef}  ▄██████▄${reset}    ${purplef}  ▄██████▄${reset}    ${cyanf}  ▄██████▄${reset}
 ${yellowf}▄█████████▀▀${reset}  ${redf}▄${whitef}█▀█${redf}██${whitef}█▀█${redf}██▄${reset}  ${greenf}▄${whitef}█▀█${greenf}██${whitef}█▀█${greenf}██▄${reset}  ${bluef}▄${whitef}█▀█${bluef}██${whitef}█▀█${bluef}██▄${reset}  ${purplef}▄${whitef}█▀█${purplef}██${whitef}█▀█${purplef}██▄${reset}  ${cyanf}▄${whitef}█▀█${cyanf}██${whitef}█▀█${cyanf}██▄${reset}
 ${yellowf}███████▀${reset}      ${redf}█${whitef}▄▄█${redf}██${whitef}▄▄█${redf}███${reset}  ${greenf}█${whitef}▄▄█${greenf}██${whitef}▄▄█${greenf}███${reset}  ${bluef}█${whitef}▄▄█${bluef}██${whitef}▄▄█${bluef}███${reset}  ${purplef}█${whitef}▄▄█${purplef}██${whitef}▄▄█${purplef}███${reset}  ${cyanf}█${whitef}▄▄█${cyanf}██${whitef}▄▄█${cyanf}███${reset}
 ${yellowf}███████▄${reset}      ${redf}████████████${reset}  ${greenf}████████████${reset}  ${bluef}████████████${reset}  ${purplef}████████████${reset}  ${cyanf}████████████${reset}
 ${yellowf}▀█████████▄▄${reset}  ${redf}██▀██▀▀██▀██${reset}  ${greenf}██▀██▀▀██▀██${reset}  ${bluef}██▀██▀▀██▀██${reset}  ${purplef}██▀██▀▀██▀██${reset}  ${cyanf}██▀██▀▀██▀██${reset}
 ${yellowf}  ▀███████▀${reset}   ${redf}▀   ▀  ▀   ▀${reset}  ${greenf}▀   ▀  ▀   ▀${reset}  ${bluef}▀   ▀  ▀   ▀${reset}  ${purplef}▀   ▀  ▀   ▀${reset}  ${cyanf}▀   ▀  ▀   ▀${reset}

   ${yellowf}${bginv}▄${yellowb}       ${bginv}${yellowf}▄${reset}     ${redf}${bginv}▄${redb}      ${bginv}${redf}▄${reset}      ${greenf}${bginv}▄${greenb}      ${bginv}${greenf}▄${reset}      ${bluef}${bginv}▄${blueb}      ${bginv}${bluef}▄${reset}      ${purplef}${bginv}▄${purpleb}      ${bginv}${purplef}▄${reset}      ${cyanf}${bginv}▄${cyanb}      ${bginv}${cyanf}▄${reset}
 ${yellowf}${bginv}▄${yellowb}         ${bginv}${yellowf}▀▀${reset}  ${redf}${bginv}▄${whitef}█▀█${redb}  ${bginv}${whitef}█▀█${redb}  ${bginv}${redf}▄${reset}  ${greenf}${bginv}▄${whitef}█▀█${greenb}  ${bginv}${whitef}█▀█${greenb}  ${bginv}${greenf}▄${reset}  ${bluef}${bginv}▄${whitef}█▀█${blueb}  ${bginv}${whitef}█▀█${blueb}  ${bginv}${bluef}▄${reset}  ${purplef}${bginv}▄${whitef}█▀█${purpleb}  ${bginv}${whitef}█▀█${purpleb}  ${bginv}${purplef}▄${reset}  ${cyanf}${bginv}▄${whitef}█▀█${cyanb}  ${bginv}${whitef}█▀█${cyanb}  ${bginv}${cyanf}▄${reset}
 ${yellowb}       ${bginv}${yellowf}▀${reset}      ${redb} ${bginv}${whitef}▄▄█${redb}  ${bginv}${whitef}▄▄█${redb}   ${reset}  ${greenb} ${bginv}${whitef}▄▄█${greenb}  ${bginv}${whitef}▄▄█${greenb}   ${reset}  ${blueb} ${bginv}${whitef}▄▄█${blueb}  ${bginv}${whitef}▄▄█${blueb}   ${reset}  ${purpleb} ${bginv}${whitef}▄▄█${purpleb}  ${bginv}${whitef}▄▄█${purpleb}   ${reset}  ${cyanb} ${bginv}${whitef}▄▄█${cyanb}  ${bginv}${whitef}▄▄█${cyanb}   ${reset}
 ${yellowb}       ${bginv}${yellowf}▄${reset}      ${redb}            ${reset}  ${greenb}            ${reset}  ${blueb}            ${reset}  ${purpleb}            ${reset}  ${cyanb}            ${reset}
 ${bginv}${yellowf}▀${yellowb}         ${bginv}${yellowf}▄▄${reset}  ${redb}  ${bginv}${redf}▀${redb}  ${bginv}${redf}▀▀${redb}  ${bginv}${redf}▀${redb}  ${reset}  ${greenb}  ${bginv}${greenf}▀${greenb}  ${bginv}${greenf}▀▀${greenb}  ${bginv}${greenf}▀${greenb}  ${reset}  ${blueb}  ${bginv}${bluef}▀${blueb}  ${bginv}${bluef}▀▀${blueb}  ${bginv}${bluef}▀${blueb}  ${reset}  ${purpleb}  ${bginv}${purplef}▀${purpleb}  ${bginv}${purplef}▀▀${purpleb}  ${bginv}${purplef}▀${purpleb}  ${reset}  ${cyanb}  ${bginv}${cyanf}▀${cyanb}  ${bginv}${cyanf}▀▀${cyanb}  ${bginv}${cyanf}▀${cyanb}  ${reset}
   ${yellowf}${bginv}▀${yellowb}       ${bginv}${yellowf}▀${reset}   ${redf}${bginv}▀${reset}   ${redf}${bginv}▀${reset}  ${redf}${bginv}▀${reset}   ${redf}${bginv}▀${reset}  ${greenf}${bginv}▀${reset}   ${greenf}${bginv}▀${reset}  ${greenf}${bginv}▀${reset}   ${greenf}${bginv}▀${reset}  ${bluef}${bginv}▀${reset}   ${bluef}${bginv}▀${reset}  ${bluef}${bginv}▀${reset}   ${bluef}${bginv}▀${reset}  ${purplef}${bginv}▀${reset}   ${purplef}${bginv}▀${reset}  ${purplef}${bginv}▀${reset}   ${purplef}${bginv}▀${reset}  ${cyanf}${bginv}▀${reset}   ${cyanf}${bginv}▀${reset}  ${cyanf}${bginv}▀${reset}   ${cyanf}${bginv}▀${reset}

EOF
